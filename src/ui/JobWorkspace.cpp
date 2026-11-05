#include "cvatnative/ui/JobWorkspace.h"

#include "cvatnative/api/AnnotationsApi.h"
#include "cvatnative/api/JobsApi.h"
#include "cvatnative/api/LabelsApi.h"
#include "cvatnative/annotation/Geometry.h"
#include "cvatnative/annotation/AnnotationModel.h"
#include "cvatnative/core/Settings.h"
#include "cvatnative/media/MediaService.h"
#include "cvatnative/storage/LocalStore.h"
#include "cvatnative/storage/RecoveryStore.h"
#include "cvatnative/ui/AnnotationCanvas.h"
#include "cvatnative/ui/LabelPanel.h"

#include <QAction>
#include <QActionGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QFormLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <utility>

namespace cvat {

namespace {
QString shapeTypeName(const QString& type)
{
    if (type == QStringLiteral("rectangle")) return QStringLiteral("rectangle");
    if (type == QStringLiteral("polygon")) return QStringLiteral("polygon");
    if (type == QStringLiteral("polyline")) return QStringLiteral("polyline");
    if (type == QStringLiteral("points")) return QStringLiteral("points");
    if (type == QStringLiteral("ellipse")) return QStringLiteral("ellipse");
    if (type == QStringLiteral("mask")) return QStringLiteral("mask");
    if (type == QStringLiteral("cuboid")) return QStringLiteral("cuboid");
    if (type == QStringLiteral("skeleton")) return QStringLiteral("skeleton");
    return type;
}
} // namespace

JobWorkspace::JobWorkspace(const Job& job, AnnotationsApi* annotations, JobsApi* jobs,
                           LabelsApi* labels, MediaService* media,
                           RecoveryStore* recovery, LocalStore* store,
                           QWidget* parent)
    : QWidget(parent),
      job_(job),
      annotations_(annotations),
      jobs_(jobs),
      labelsApi_(labels),
      media_(media),
      recovery_(recovery),
      store_(store),
      model_(new AnnotationModel(this))
{
    setupUi();
    connectModel();
    load();

    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setInterval(std::max(5, Settings::instance().autosaveSeconds()) * 1000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this]() {
        if (!model_->isDirty()) {
            return;
        }
        saveLocalSnapshot();
        setStatus(QStringLiteral("Local draft saved"));
        if (Settings::instance().remoteAutosave()) {
            saveRemote({}, true);
        }
    });
    autosaveTimer_->start();
}

JobWorkspace::~JobWorkspace()
{
    if (model_ && model_->isDirty()) {
        saveLocalSnapshot();
    }
    if (frameRequest_) {
        frameRequest_->cancel();
        frameRequest_ = nullptr;
    }
}

void JobWorkspace::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    toolbar_ = new QToolBar(this);
    toolbar_->setMovable(false);
    toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    layout->addWidget(toolbar_);

    auto* back = toolbar_->addAction(QStringLiteral("Back"));
    back->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));
    connect(back, &QAction::triggered, this, &JobWorkspace::requestBack);

    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet(QStringLiteral("font-weight: 600; padding: 0 12px;"));
    toolbar_->addWidget(titleLabel_);

    toolbar_->addSeparator();
    saveAction_ = toolbar_->addAction(QStringLiteral("Save"));
    saveAction_->setShortcut(QKeySequence(QStringLiteral("S")));
    saveAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(saveAction_, &QAction::triggered, this, [this]() { saveRemote(); });

    auto* newLabelAction = toolbar_->addAction(QStringLiteral("New label"));
    newLabelAction->setShortcut(QKeySequence(QStringLiteral("N")));
    newLabelAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    newLabelAction->setToolTip(QStringLiteral("New label (N)"));
    newLabelAction->setStatusTip(QStringLiteral("Start a rectangle with the active class (N)"));
    connect(newLabelAction, &QAction::triggered, this, [this]() {
        if (labelPanel_->activeLabel() <= 0) {
            setStatus(QStringLiteral("Select a label class first"), true);
            return;
        }
        canvas_->setTool(CanvasTool::Rectangle);
        setStatus(QStringLiteral("Draw a new label with the selected class"));
    });

    auto* copyLabelAction = toolbar_->addAction(QStringLiteral("Copy label"));
    copyLabelAction->setShortcut(QKeySequence(QStringLiteral("C")));
    copyLabelAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    copyLabelAction->setToolTip(QStringLiteral("Copy label (C)"));
    copyLabelAction->setStatusTip(QStringLiteral("Copy the selected object's class (C)"));
    connect(copyLabelAction, &QAction::triggered, this, &JobWorkspace::copyLabel);

    auto* pasteLabelAction = toolbar_->addAction(QStringLiteral("Paste label"));
    pasteLabelAction->setShortcut(QKeySequence(QStringLiteral("V")));
    pasteLabelAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    pasteLabelAction->setToolTip(QStringLiteral("Paste label (V)"));
    pasteLabelAction->setStatusTip(QStringLiteral("Apply the copied class to selected objects (V)"));
    connect(pasteLabelAction, &QAction::triggered, this, &JobWorkspace::pasteLabel);

    auto* bridgeButton = new QToolButton(this);
    bridgeButton->setText(QStringLiteral("Bridge tools"));
    bridgeButton->setPopupMode(QToolButton::InstantPopup);
    bridgeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* bridgeMenu = new QMenu(bridgeButton);
    bridgeMenu->addAction(QStringLiteral("Copy annotations"), this,
                          &JobWorkspace::copyAnnotations);
    bridgeMenu->addAction(QStringLiteral("Paste annotations"), this,
                          &JobWorkspace::pasteAnnotations);
    bridgeMenu->addSeparator();
    bridgeMenu->addAction(QStringLiteral("Delete annotations"), this,
                          &JobWorkspace::showDeleteDialog);
    bridgeMenu->addAction(QStringLiteral("Grow / shrink rectangles"), this,
                          &JobWorkspace::showGrowDialog);
    bridgeMenu->addAction(QStringLiteral("Move annotations"), this,
                          &JobWorkspace::showMoveDialog);
    bridgeMenu->addAction(QStringLiteral("Relabel annotations"), this,
                          &JobWorkspace::showRelabelDialog);
    bridgeButton->setMenu(bridgeMenu);
    toolbar_->addWidget(bridgeButton);

    undoAction_ = toolbar_->addAction(QStringLiteral("Undo"));
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this]() { model_->undo(); });

    redoAction_ = toolbar_->addAction(QStringLiteral("Redo"));
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, [this]() { model_->redo(); });

    deleteAction_ = toolbar_->addAction(QStringLiteral("Delete"));
    deleteAction_->setShortcut(QKeySequence::Delete);
    connect(deleteAction_, &QAction::triggered, this, &JobWorkspace::deleteSelection);

    toolbar_->addSeparator();
    auto* previous = toolbar_->addAction(QStringLiteral("◀ Frame"));
    previous->setShortcut(QKeySequence(QStringLiteral("D")));
    previous->setToolTip(QStringLiteral("Previous frame (D)"));
    connect(previous, &QAction::triggered, this, [this]() {
        setFrame(model_->currentFrame() - 1);
    });
    auto* next = toolbar_->addAction(QStringLiteral("Frame ▶"));
    next->setShortcut(QKeySequence(QStringLiteral("F")));
    next->setToolTip(QStringLiteral("Next frame (F)"));
    connect(next, &QAction::triggered, this, [this]() {
        setFrame(model_->currentFrame() + 1);
    });

    frameSpin_ = new QSpinBox(this);
    frameSpin_->setMinimumWidth(90);
    frameSpin_->setRange(0, 0);
    toolbar_->addWidget(frameSpin_);
    connect(frameSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (value != model_->currentFrame()) {
            setFrame(value);
        }
    });

    frameSlider_ = new QSlider(Qt::Horizontal, this);
    frameSlider_->setMinimumWidth(140);
    frameSlider_->setRange(0, 0);
    toolbar_->addWidget(frameSlider_);
    connect(frameSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (value != model_->currentFrame()) {
            setFrame(value);
        }
    });

    toolbar_->addSeparator();
    toolGroup_ = new QActionGroup(this);
    toolGroup_->setExclusive(true);
    auto addTool = [this](const QString& text, CanvasTool tool, const QString& shortcut) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setData(static_cast<int>(tool));
        action->setShortcut(QKeySequence(shortcut));
        toolGroup_->addAction(action);
        toolbar_->addAction(action);
        return action;
    };
    addTool(QStringLiteral("Select"), CanvasTool::Select, QStringLiteral("Q"))->setChecked(true);
    addTool(QStringLiteral("Box"), CanvasTool::Rectangle, QStringLiteral("R"));
    connect(toolGroup_, &QActionGroup::triggered, this, [this](QAction* action) {
        setTool(action->data().toInt());
    });

    toolbar_->addSeparator();
    auto* zoomIn = toolbar_->addAction(QStringLiteral("Zoom +"));
    connect(zoomIn, &QAction::triggered, this, [this]() { canvas_->zoomIn(); });
    auto* zoomOut = toolbar_->addAction(QStringLiteral("Zoom −"));
    connect(zoomOut, &QAction::triggered, this, [this]() { canvas_->zoomOut(); });
    auto* fit = toolbar_->addAction(QStringLiteral("Fit"));
    connect(fit, &QAction::triggered, this, [this]() { canvas_->fitToView(); });

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    canvas_ = new AnnotationCanvas(splitter);
    canvas_->setModel(model_);
    splitter->addWidget(canvas_);

    auto* side = new QWidget(splitter);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(4, 4, 4, 4);
    labelPanel_ = new LabelPanel(side);
    sideLayout->addWidget(labelPanel_, 1);

    auto* objectGroup = new QGroupBox(QStringLiteral("Objects on this frame"), side);
    auto* objectLayout = new QVBoxLayout(objectGroup);
    objectList_ = new QListWidget(objectGroup);
    objectList_->setAlternatingRowColors(true);
    objectLayout->addWidget(objectList_);
    sideLayout->addWidget(objectGroup);
    splitter->addWidget(side);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({1000, 320});
    layout->addWidget(splitter, 1);

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(8, 2, 8, 4);
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: #888;"));
    dirtyLabel_ = new QLabel(this);
    dirtyLabel_->setStyleSheet(QStringLiteral("color: #d59b00; font-weight: 600;"));
    footer->addWidget(statusLabel_, 1);
    footer->addWidget(dirtyLabel_);
    layout->addLayout(footer);

    connect(canvas_, &AnnotationCanvas::statusMessage, this, [this](const QString& message) {
        setStatus(message);
    });
    connect(canvas_, &AnnotationCanvas::zoomChanged, this, [this](double zoom) {
        setStatus(QStringLiteral("Zoom %1%").arg(static_cast<int>(zoom * 100)));
    });
    connect(labelPanel_, &LabelPanel::labelActivated, this,
            [this](int labelId) { canvas_->setActiveLabel(labelId); });
    connect(labelPanel_, &LabelPanel::attributesChanged, this,
            &JobWorkspace::changeAttributes);
    connect(labelPanel_, &LabelPanel::occludedChanged, this,
            &JobWorkspace::changeOccluded);
    connect(labelPanel_, &LabelPanel::deleteRequested, this,
            &JobWorkspace::deleteSelection);
    connect(objectList_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
        if (!current) {
            model_->clearSelection();
            return;
        }
        model_->selectOnly(current->data(Qt::UserRole).toString());
    });

    updateTitle();
    updateActions();
}

void JobWorkspace::connectModel()
{
    connect(model_, &AnnotationModel::changed, this, [this]() {
        refreshObjects();
        updateActions();
        updateTitle();
    });
    connect(model_, &AnnotationModel::selectionChanged, this, [this]() {
        updateSelectionUi();
        updateActions();
    });
    connect(model_, &AnnotationModel::dirtyChanged, this, [this](bool) {
        updateTitle();
        updateActions();
    });
    connect(model_, &AnnotationModel::undoStackChanged, this, [this]() { updateActions(); });
    connect(model_, &AnnotationModel::frameChanged, this, [this](int frame) {
        QSignalBlocker spinBlocker(frameSpin_);
        QSignalBlocker sliderBlocker(frameSlider_);
        frameSpin_->setValue(frame);
        frameSlider_->setValue(frame);
    });
}

void JobWorkspace::load()
{
    setStatus(QStringLiteral("Loading job…"));
    loadLabels();
    loadMeta();
    loadAnnotations();
    setFrame(firstFrame());
}

void JobWorkspace::loadLabels()
{
    const int projectId = job_.projectId;
    const int taskId = job_.taskId;
    auto apply = [this](const QVector<Label>& labels) {
        labels_ = labels;
        model_->setLabels(labels_);
        labelPanel_->setLabels(labels_);
        if (!labels_.isEmpty()) {
            labelPanel_->setActiveLabel(labels_.first().id);
            canvas_->setActiveLabel(labels_.first().id);
        } else {
            setStatus(QStringLiteral("This job has no labels"), true);
        }
    };

    labelsApi_->list(projectId, projectId > 0 ? 0 : taskId, {}, [this, apply, projectId](
                                                              Result<QVector<Label>> result) {
        if (!result) {
            setStatus(QStringLiteral("Could not load labels: ") + result.error().toString(), true);
            return;
        }
        if (result->isEmpty() && projectId > 0) {
            labelsApi_->list(0, job_.taskId, {}, [apply](Result<QVector<Label>> fallback) {
                apply(fallback ? *fallback : QVector<Label>{});
            });
            return;
        }
        apply(*result);
    });
}

void JobWorkspace::loadMeta()
{
    media_->requestMeta(job_.id, [this](Result<DataMeta> result) {
        if (!result) {
            return;
        }
        const DataMeta meta = *result;
        if (job_.frameCount <= 0 && meta.size > 0) {
            job_.frameCount = meta.size;
        }
        const int start = firstFrame();
        const int stop = lastFrame();
        if (stop >= start) {
            QSignalBlocker spinBlocker(frameSpin_);
            QSignalBlocker sliderBlocker(frameSlider_);
            frameSpin_->setRange(start, stop);
            frameSlider_->setRange(start, stop);
            frameSpin_->setValue(model_->currentFrame());
            frameSlider_->setValue(model_->currentFrame());
        }
    });
}

void JobWorkspace::loadAnnotations()
{
    annotations_->get(job_.id, [this](Result<LabeledData> result) {
        if (!result) {
            setStatus(QStringLiteral("Could not load annotations: ") +
                      result.error().toString(), true);
            return;
        }
        model_->load(*result, model_->currentFrame());
        annotationsLoaded_ = true;
        if (!recoveryChecked_) {
            recoveryChecked_ = true;
            const auto snapshot = recovery_->loadSnapshot(job_.id, -1);
            if (snapshot && snapshot->toJson() != result->toJson()) {
                const auto saved = recovery_->snapshotTime(job_.id, -1);
                const QString when = saved.isValid()
                    ? saved.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                    : QStringLiteral("earlier");
                const auto answer = QMessageBox::question(
                    this, QStringLiteral("Recover local draft"),
                    QStringLiteral("A local draft from %1 differs from the server. "
                                   "Load the local draft?").arg(when),
                    QMessageBox::Yes | QMessageBox::No);
                if (answer == QMessageBox::Yes) {
                    model_->load(*snapshot, model_->currentFrame());
                    setStatus(QStringLiteral("Recovered local draft"));
                } else if (store_) {
                    store_->removeSnapshot(job_.id, -1);
                }
            }
        }
        setFrame(model_->currentFrame());
        refreshObjects();
        updateActions();
        updateTitle();
        setStatus(QStringLiteral("Ready"));
    });
}

int JobWorkspace::firstFrame() const
{
    return std::max(0, job_.startFrame);
}

int JobWorkspace::lastFrame() const
{
    if (job_.stopFrame >= job_.startFrame) {
        return job_.stopFrame;
    }
    if (job_.frameCount > 0) {
        return firstFrame() + job_.frameCount - 1;
    }
    return firstFrame();
}

void JobWorkspace::setFrame(int frame)
{
    const int clamped = std::max(firstFrame(), std::min(lastFrame(), frame));
    model_->setCurrentFrame(clamped);
    {
        QSignalBlocker spinBlocker(frameSpin_);
        QSignalBlocker sliderBlocker(frameSlider_);
        frameSpin_->setValue(clamped);
        frameSlider_->setValue(clamped);
    }
    requestFrame(clamped);
    refreshObjects();
    updateTitle();
}

void JobWorkspace::requestFrame(int frame)
{
    requestedFrame_ = frame;
    if (frameRequest_) {
        RequestHandle* previous = frameRequest_;
        frameRequest_ = nullptr;
        previous->cancel();
    }
    setStatus(QStringLiteral("Loading frame %1…").arg(frame));
    frameRequest_ = media_->requestFrame(
        job_.id, frame, QStringLiteral("compressed"),
        [this, frame](Result<QImage> result) {
            frameRequest_ = nullptr;
            if (frame != requestedFrame_) {
                return;
            }
            if (!result) {
                setStatus(QStringLiteral("Frame %1 failed: %2")
                              .arg(frame)
                              .arg(result.error().toString()),
                          true);
                return;
            }
            canvas_->setFrameImage(*result);
            setStatus(QStringLiteral("Frame %1 of %2")
                          .arg(frame - firstFrame() + 1)
                          .arg(lastFrame() - firstFrame() + 1));
            if (frame + 1 <= lastFrame()) {
                media_->requestFrame(job_.id, frame + 1, QStringLiteral("compressed"),
                                     [](Result<QImage>) {});
            }
        });
}

void JobWorkspace::refreshObjects()
{
    {
        QSignalBlocker blocker(objectList_);
        objectList_->clear();
        if (model_) {
            for (const auto& visible : model_->visibleShapes()) {
                const Label* label = model_->label(visible.shape.labelId);
                const QString name = label ? label->name : QStringLiteral("label %1")
                                                      .arg(visible.shape.labelId);
                const QString kind = visible.fromTrack ? QStringLiteral("track")
                                                       : QStringLiteral("shape");
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1 · %2 · %3 #%4")
                        .arg(name, shapeTypeName(visible.shape.type), kind)
                        .arg(visible.objectId),
                    objectList_);
                item->setData(Qt::UserRole, visible.key);
            }
            for (const auto& tag : model_->visibleTags()) {
                const Label* label = model_->label(tag.labelId);
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1 · tag").arg(label ? label->name
                                                         : QStringLiteral("label")),
                    objectList_);
                item->setData(Qt::UserRole, model_->keyForTag(tag));
            }
        }
    }
    updateSelectionUi();
}

void JobWorkspace::updateSelectionUi()
{
    if (!model_) {
        return;
    }
    const QString key = selectedKey();
    if (key.isEmpty()) {
        labelPanel_->clearObject();
        return;
    }

    const auto visible = model_->visibleShape(key);
    if (visible) {
        const Label* label = model_->label(visible->shape.labelId);
        const QString name = label ? label->name : QStringLiteral("label %1")
                                                .arg(visible->shape.labelId);
        QString summary = QStringLiteral("%1 · %2").arg(name, shapeTypeName(visible->shape.type));
        if (visible->fromTrack) {
            summary += QStringLiteral(" · track keyframe");
        }
        labelPanel_->showObject(key, visible->shape.labelId, visible->shape.attributes,
                                visible->shape.occluded, summary);
    } else if (key.startsWith(QLatin1Char('g'))) {
        for (const auto& tag : model_->visibleTags()) {
            if (model_->keyForTag(tag) == key) {
                const Label* label = model_->label(tag.labelId);
                labelPanel_->showObject(key, tag.labelId, tag.attributes, false,
                                        QStringLiteral("%1 · tag")
                                            .arg(label ? label->name
                                                       : QStringLiteral("label")));
                break;
            }
        }
    }

    const auto selected = model_->selection();
    QSignalBlocker blocker(objectList_);
    for (int row = 0; row < objectList_->count(); ++row) {
        auto* item = objectList_->item(row);
        item->setSelected(selected.contains(item->data(Qt::UserRole).toString()));
        if (item->data(Qt::UserRole).toString() == key) {
            objectList_->setCurrentItem(item);
        }
    }
}

QString JobWorkspace::selectedKey() const
{
    if (!model_ || model_->selection().isEmpty()) {
        return {};
    }
    return *model_->selection().constBegin();
}

void JobWorkspace::copyLabel()
{
    if (!model_) {
        return;
    }
    const QString key = selectedKey();
    if (key.isEmpty()) {
        setStatus(QStringLiteral("Select an object to copy its label"), true);
        return;
    }

    if (const auto visible = model_->visibleShape(key)) {
        copiedLabelId_ = visible->shape.labelId;
    } else if (key.startsWith(QLatin1Char('g'))) {
        copiedLabelId_ = 0;
        for (const auto& tag : model_->visibleTags()) {
            if (model_->keyForTag(tag) == key) {
                copiedLabelId_ = tag.labelId;
                break;
            }
        }
    }

    if (copiedLabelId_ > 0) {
        const Label* label = model_->label(copiedLabelId_);
        setStatus(QStringLiteral("Copied label: %1")
                      .arg(label ? label->name : QString::number(copiedLabelId_)));
    } else {
        setStatus(QStringLiteral("Selected object has no label"), true);
    }
}

void JobWorkspace::pasteLabel()
{
    if (!model_ || copiedLabelId_ <= 0) {
        setStatus(QStringLiteral("Copy a label before pasting"), true);
        return;
    }
    if (model_->selection().isEmpty()) {
        setStatus(QStringLiteral("Select one or more objects to paste the label"), true);
        return;
    }

    int changed = 0;
    const auto selected = model_->selection();
    for (const QString& key : selected) {
        changed += model_->setObjectLabel(key, copiedLabelId_) ? 1 : 0;
    }
    const Label* label = model_->label(copiedLabelId_);
    setStatus(changed > 0
                  ? QStringLiteral("Pasted label %1 object(s): %2")
                        .arg(changed)
                        .arg(label ? label->name : QString::number(copiedLabelId_))
                  : QStringLiteral("Selected objects already use that label"));
}

void JobWorkspace::copyAnnotations()
{
    if (!model_ || model_->selection().isEmpty()) {
        setStatus(QStringLiteral("Select annotations to copy"), true);
        return;
    }
    QJsonArray shapes;
    QJsonArray bridgeClip;
    for (const QString& key : model_->selection()) {
        const auto visible = model_->visibleShape(key);
        if (!visible) {
            continue;
        }
        Shape shape = visible->shape;
        shape.id.reset();
        shape.frame = model_->currentFrame();
        shapes.push_back(shape.toJson());
        QJsonObject bridgeShape;
        const Label* label = model_->label(shape.labelId);
        bridgeShape.insert(QStringLiteral("labelId"), shape.labelId);
        bridgeShape.insert(QStringLiteral("labelName"), label ? label->name : QString());
        bridgeShape.insert(QStringLiteral("shapeType"), shape.type);
        QJsonArray points;
        for (const double point : shape.points) {
            points.push_back(point);
        }
        bridgeShape.insert(QStringLiteral("points"), points);
        bridgeShape.insert(QStringLiteral("occluded"), shape.occluded);
        bridgeShape.insert(QStringLiteral("zOrder"), shape.zOrder);
        bridgeShape.insert(QStringLiteral("rotation"), shape.rotation);
        QJsonObject attributes;
        for (const auto& attribute : shape.attributes) {
            attributes.insert(QString::number(attribute.specId), attribute.value);
        }
        bridgeShape.insert(QStringLiteral("attributes"), attributes);
        bridgeShape.insert(QStringLiteral("source"), shape.source);
        bridgeClip.push_back(bridgeShape);
    }
    if (shapes.isEmpty()) {
        setStatus(QStringLiteral("Only visible shapes can be copied"), true);
        return;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("v"), 1);
    payload.insert(QStringLiteral("job_id"), job_.id);
    payload.insert(QStringLiteral("frame"), model_->currentFrame());
    payload.insert(QStringLiteral("clip"), bridgeClip);
    payload.insert(QStringLiteral("shapes"), shapes);
    QGuiApplication::clipboard()->setText(
        QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    setStatus(QStringLiteral("Copied %1 annotation(s)").arg(shapes.size()));
}

void JobWorkspace::pasteAnnotations()
{
    if (!model_) {
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(
        QGuiApplication::clipboard()->text().toUtf8());
    if (!document.isObject() || document.object().value(QStringLiteral("v")).toInt() != 1) {
        setStatus(QStringLiteral("Clipboard does not contain native annotations"), true);
        return;
    }
    QJsonArray rawShapes = document.object().value(QStringLiteral("shapes")).toArray();
    if (rawShapes.isEmpty()) {
        for (const auto& value : document.object().value(QStringLiteral("clip")).toArray()) {
            const QJsonObject source = value.toObject();
            QJsonObject converted;
            int labelId = source.value(QStringLiteral("labelId")).toInt();
            const QString labelName = source.value(QStringLiteral("labelName")).toString();
            if (!model_->label(labelId)) {
                for (const auto& label : labels_) {
                    if (label.name.compare(labelName, Qt::CaseInsensitive) == 0) {
                        labelId = label.id;
                        break;
                    }
                }
            }
            converted.insert(QStringLiteral("label_id"), labelId);
            converted.insert(QStringLiteral("type"), source.value(QStringLiteral("shapeType")));
            converted.insert(QStringLiteral("points"), source.value(QStringLiteral("points")));
            converted.insert(QStringLiteral("occluded"), source.value(QStringLiteral("occluded")));
            converted.insert(QStringLiteral("z_order"), source.value(QStringLiteral("zOrder")));
            converted.insert(QStringLiteral("rotation"), source.value(QStringLiteral("rotation")));
            converted.insert(QStringLiteral("source"), source.value(QStringLiteral("source")));
            rawShapes.push_back(converted);
        }
    }
    int pasted = 0;
    int skipped = 0;
    for (const auto& value : rawShapes) {
        Shape shape = Shape::fromJson(value.toObject());
        shape.id.reset();
        shape.frame = model_->currentFrame();
        if (model_->label(shape.labelId)) {
            model_->addShape(shape);
            ++pasted;
        } else {
            ++skipped;
        }
    }
    setStatus(pasted > 0
                  ? QStringLiteral("Pasted %1 annotation(s)%2")
                        .arg(pasted)
                        .arg(skipped ? QStringLiteral(", %1 skipped").arg(skipped) : QString())
                  : QStringLiteral("No clipboard annotations matched this job's labels"),
              pasted == 0);
}

void JobWorkspace::showGrowDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Grow / shrink rectangles"));
    auto* layout = new QFormLayout(&dialog);
    auto* label = new QComboBox(&dialog);
    label->addItem(QStringLiteral("All visible rectangles"));
    for (const auto& entry : labels_) {
        label->addItem(entry.name, entry.id);
    }
    auto* amount = new QDoubleSpinBox(&dialog);
    amount->setRange(0.1, 2000.0);
    amount->setValue(5.0);
    amount->setSuffix(QStringLiteral(" px per side"));
    layout->addRow(QStringLiteral("Class"), label);
    layout->addRow(QStringLiteral("Amount"), amount);
    auto* buttons = new QDialogButtonBox(&dialog);
    auto* grow = buttons->addButton(QStringLiteral("Grow"), QDialogButtonBox::AcceptRole);
    auto* shrink = buttons->addButton(QStringLiteral("Shrink"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    auto apply = [this, label, amount](double direction) {
        const int labelId = label->currentData().toInt();
        int changed = 0;
        for (const auto& visible : model_->visibleShapes()) {
            if (visible.shape.type != QStringLiteral("rectangle") ||
                visible.shape.points.size() != 4 ||
                (labelId > 0 && visible.shape.labelId != labelId)) {
                continue;
            }
            Shape shape = visible.shape;
            const double x1 = std::min(shape.points[0], shape.points[2]);
            const double x2 = std::max(shape.points[0], shape.points[2]);
            const double y1 = std::min(shape.points[1], shape.points[3]);
            const double y2 = std::max(shape.points[1], shape.points[3]);
            const double delta = direction * amount->value();
            if (x2 - x1 + 2 * delta <= 0 || y2 - y1 + 2 * delta <= 0) {
                continue;
            }
            shape.points = {x1 - delta, y1 - delta, x2 + delta, y2 + delta};
            changed += model_->updateVisibleShape(visible.key, shape) ? 1 : 0;
        }
        setStatus(QStringLiteral("%1 rectangle(s) updated locally; press S to save").arg(changed));
    };
    connect(grow, &QPushButton::clicked, &dialog, [&]() { apply(1.0); dialog.accept(); });
    connect(shrink, &QPushButton::clicked, &dialog, [&]() { apply(-1.0); dialog.accept(); });
    dialog.exec();
}

void JobWorkspace::showMoveDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Move annotations"));
    auto* layout = new QFormLayout(&dialog);
    auto* dx = new QDoubleSpinBox(&dialog);
    auto* dy = new QDoubleSpinBox(&dialog);
    dx->setRange(-100000, 100000);
    dy->setRange(-100000, 100000);
    dx->setSuffix(QStringLiteral(" px"));
    dy->setSuffix(QStringLiteral(" px"));
    layout->addRow(QStringLiteral("X offset"), dx);
    layout->addRow(QStringLiteral("Y offset"), dy);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        int changed = 0;
        for (const auto& visible : model_->visibleShapes()) {
            Shape shape = visible.shape;
            Geometry::translate(shape.points, dx->value(), dy->value());
            changed += model_->updateVisibleShape(visible.key, shape) ? 1 : 0;
        }
        setStatus(QStringLiteral("Moved %1 annotation(s) locally; press S to save").arg(changed));
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void JobWorkspace::showRelabelDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Relabel annotations"));
    auto* layout = new QFormLayout(&dialog);
    auto* from = new QComboBox(&dialog);
    auto* to = new QComboBox(&dialog);
    for (const auto& label : labels_) {
        from->addItem(label.name, label.id);
        to->addItem(label.name, label.id);
    }
    layout->addRow(QStringLiteral("Replace"), from);
    layout->addRow(QStringLiteral("With"), to);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const int source = from->currentData().toInt();
        const int target = to->currentData().toInt();
        int changed = 0;
        if (source != target) {
            for (const auto& visible : model_->visibleShapes()) {
                if (visible.shape.labelId == source) {
                    changed += model_->setObjectLabel(visible.key, target) ? 1 : 0;
                }
            }
        }
        setStatus(QStringLiteral("Relabeled %1 annotation(s) locally; press S to save").arg(changed));
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void JobWorkspace::showDeleteDialog()
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("Delete annotations"),
                    QStringLiteral("Delete the currently selected annotation(s) from this frame?"),
                    QMessageBox::Yes | QMessageBox::No, this);
    if (box.exec() == QMessageBox::Yes) {
        deleteSelection();
        setStatus(QStringLiteral("Deleted selected annotation(s) locally; press S to save"));
    }
}

void JobWorkspace::updateActions()
{
    if (!saveAction_) {
        return;
    }
    const bool hasSelection = model_ && !model_->selection().isEmpty();
    saveAction_->setEnabled(annotationsLoaded_ && !saving_);
    undoAction_->setEnabled(!saving_ && model_->canUndo());
    redoAction_->setEnabled(!saving_ && model_->canRedo());
    deleteAction_->setEnabled(!saving_ && hasSelection);
}

void JobWorkspace::updateTitle()
{
    if (!titleLabel_) {
        return;
    }
    QString title = job_.taskName.isEmpty()
        ? QStringLiteral("Job %1").arg(job_.id)
        : QStringLiteral("%1 · job %2").arg(job_.taskName).arg(job_.id);
    if (!job_.projectName.isEmpty()) {
        title += QStringLiteral(" · %1").arg(job_.projectName);
    }
    titleLabel_->setText(title);
    const bool dirty = model_ && model_->isDirty();
    dirtyLabel_->setText(dirty ? QStringLiteral("● unsaved changes") : QString());
}

void JobWorkspace::setStatus(const QString& message, bool error)
{
    statusLabel_->setStyleSheet(error ? QStringLiteral("color: #c44;")
                                      : QStringLiteral("color: #888;"));
    statusLabel_->setText(message);
}

void JobWorkspace::setTool(int tool)
{
    canvas_->setTool(static_cast<CanvasTool>(tool));
}

void JobWorkspace::saveRemote(std::function<void(bool)> done, bool silent)
{
    if (saving_) {
        if (done) {
            done(false);
        }
        return;
    }
    if (!annotationsLoaded_) {
        setStatus(QStringLiteral("Annotations are not loaded yet"), true);
        if (done) {
            done(false);
        }
        return;
    }

    const auto patch = model_->buildPatch();
    if (!patch.any()) {
        model_->markClean();
        if (!silent) {
            setStatus(QStringLiteral("No changes to save"));
        }
        if (done) {
            done(true);
        }
        return;
    }

    saving_ = true;
    updateActions();
    saveLocalSnapshot();
    if (recovery_) {
        recovery_->clearJob(job_.id);
    }

    struct Operation {
        PatchAction action;
        LabeledData data;
        qint64 pendingId = -1;
    };
    auto operations = std::make_shared<QVector<Operation>>();
    auto addOperation = [this, operations](PatchAction action, const LabeledData& data) {
        Operation operation;
        operation.action = action;
        operation.data = data;
        if (recovery_) {
            operation.pendingId = recovery_->queuePatch(
                job_.id, model_->currentFrame(), AnnotationsApi::actionName(action), data);
        }
        operations->push_back(operation);
    };
    if (patch.hasRemove()) {
        addOperation(PatchAction::Delete, patch.remove);
    }
    if (patch.hasUpdate()) {
        addOperation(PatchAction::Update, patch.update);
    }
    if (patch.hasCreate()) {
        addOperation(PatchAction::Create, patch.create);
    }

    auto finish = [this, operations, done, silent](bool ok, const QString& error) {
        if (!ok) {
            saving_ = false;
            setStatus(QStringLiteral("Save failed: ") + error, true);
            updateActions();
            if (done) {
                done(false);
            }
            return;
        }
        for (const auto& operation : *operations) {
            if (recovery_ && operation.pendingId > 0) {
                recovery_->removePatch(operation.pendingId);
            }
        }
        annotations_->get(job_.id, [this, done, silent](Result<LabeledData> reloaded) {
            if (!reloaded) {
                saving_ = false;
                setStatus(QStringLiteral("Saved, but reloading failed: ") +
                          reloaded.error().toString(), true);
                updateActions();
                if (done) {
                    done(false);
                }
                return;
            }
            const int frame = model_->currentFrame();
            model_->load(*reloaded, frame);
            model_->markClean();
            annotationsLoaded_ = true;
            saving_ = false;
            if (recovery_) {
                recovery_->clearJob(job_.id);
            }
            if (store_) {
                store_->removeSnapshot(job_.id, -1);
            }
            if (!silent) {
                setStatus(QStringLiteral("Saved to server"));
            }
            refreshObjects();
            updateActions();
            updateTitle();
            if (done) {
                done(true);
            }
        });
    };

    auto send = std::make_shared<std::function<void(int)>>();
    std::weak_ptr<std::function<void(int)>> weak = send;
    *send = [this, operations, weak, finish](int index) {
        if (index >= operations->size()) {
            finish(true, {});
            return;
        }
        const Operation operation = (*operations)[index];
        annotations_->patch(job_.id, operation.action, operation.data,
                            [this, operations, weak, index, finish](
                                Result<ApiResponse> result) {
            if (!result) {
                finish(false, result.error().toString());
                return;
            }
            if (auto next = weak.lock()) {
                (*next)(index + 1);
            }
        });
    };
    (*send)(0);
}

void JobWorkspace::saveLocalSnapshot()
{
    if (recovery_ && model_) {
        recovery_->saveSnapshot(job_.id, -1, model_->data());
    }
}

bool JobWorkspace::isDirty() const
{
    return model_ && model_->isDirty();
}

void JobWorkspace::requestBack()
{
    if (!model_ || !model_->isDirty()) {
        emit backRequested();
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Unsaved changes"),
        QStringLiteral("Save the changes to the server before leaving?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return;
    }
    if (answer == QMessageBox::Discard) {
        emit backRequested();
        return;
    }
    saveRemote([this](bool ok) {
        if (ok) {
            emit backRequested();
        }
    });
}

void JobWorkspace::deleteSelection()
{
    if (!model_ || model_->selection().isEmpty()) {
        return;
    }
    model_->removeObjects(model_->selection());
}

void JobWorkspace::changeSelectionLabel(int labelId)
{
    const QString key = selectedKey();
    if (!key.isEmpty()) {
        model_->setObjectLabel(key, labelId);
    }
}

void JobWorkspace::changeAttributes(const QVector<AttributeValue>& attributes)
{
    const QString key = selectedKey();
    if (!key.isEmpty()) {
        model_->setObjectAttributes(key, attributes);
    }
}

void JobWorkspace::changeOccluded(bool occluded)
{
    const QString key = selectedKey();
    if (!key.isEmpty()) {
        model_->setObjectOccluded(key, occluded);
    }
}

void JobWorkspace::applySettings()
{
    if (autosaveTimer_) {
        autosaveTimer_->setInterval(std::max(5, Settings::instance().autosaveSeconds()) * 1000);
    }
}

} // namespace cvat
