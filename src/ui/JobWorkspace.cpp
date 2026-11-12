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
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QFormLayout>
#include <QFrame>
#include <QResizeEvent>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace cvat {

class ToastWidget : public QFrame {
public:
    explicit ToastWidget(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(18, 10, 18, 10);
        label_ = new QLabel(this);
        label_->setAlignment(Qt::AlignCenter);
        layout->addWidget(label_);

        hideTimer_ = new QTimer(this);
        hideTimer_->setSingleShot(true);
        connect(hideTimer_, &QTimer::timeout, this, [this]() {
            hide();
        });
        hide();
    }

    void showToast(const QString& text, bool isError = false, int durationMs = 3000)
    {
        label_->setText(text);
        if (isError) {
            setStyleSheet(QStringLiteral(
                "QFrame { background-color: rgba(30, 20, 24, 0.96); "
                "border: 1px solid #ef4444; border-radius: 8px; }"));
            label_->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #fca5a5;"));
        } else {
            setStyleSheet(QStringLiteral(
                "QFrame { background-color: rgba(15, 28, 22, 0.96); "
                "border: 1px solid #22c55e; border-radius: 8px; }"));
            label_->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600; color: #86efac;"));
        }
        adjustSize();
        if (parentWidget()) {
            const int x = (parentWidget()->width() - width()) / 2;
            const int y = 46;
            move(x, y);
        }
        show();
        raise();
        hideTimer_->start(durationMs);
    }

private:
    QLabel* label_ = nullptr;
    QTimer* hideTimer_ = nullptr;
};

class LabelRowWidget : public QWidget {
public:
    LabelRowWidget(int id, const QString& name, const QString& color, int count,
                   const QString& countSuffix, QWidget* parent = nullptr)
        : QWidget(parent), id_(id), name_(name), count_(count)
    {
        setAttribute(Qt::WA_Hover, true);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 4, 6, 4);
        layout->setSpacing(8);

        cb_ = new QCheckBox(this);
        cb_->setChecked(count > 0);

        auto* colorLabel = new QLabel(this);
        colorLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QPixmap pixmap(12, 12);
        pixmap.fill(QColor(color.isEmpty() ? QStringLiteral("#888888") : color));
        colorLabel->setPixmap(pixmap);

        auto* nameLabel = new QLabel(name, this);
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        nameLabel->setStyleSheet(QStringLiteral("font-weight: 500;"));

        auto* countLabel = new QLabel(this);
        countLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QString countStr = QStringLiteral("(%1)").arg(count);
        if (!countSuffix.isEmpty()) {
            countStr = QStringLiteral("(%1 %2)").arg(count).arg(countSuffix);
        }
        countLabel->setText(countStr);
        if (count > 0) {
            countLabel->setStyleSheet(QStringLiteral(
                "color: #0284c7; font-weight: 600; padding: 2px 7px; "
                "background: rgba(56, 189, 248, 0.15); border-radius: 4px; font-size: 11px;"));
        } else {
            countLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px; padding: 2px 4px;"));
        }

        layout->addWidget(cb_);
        layout->addWidget(colorLabel);
        layout->addWidget(nameLabel);
        layout->addStretch(1);
        layout->addWidget(countLabel);

        setStyleSheet(QStringLiteral(
            "LabelRowWidget { border-radius: 4px; } "
            "LabelRowWidget:hover { background-color: rgba(255, 255, 255, 0.06); }"));
    }

    [[nodiscard]] int id() const { return id_; }
    [[nodiscard]] QString name() const { return name_; }
    [[nodiscard]] int count() const { return count_; }
    [[nodiscard]] bool isChecked() const { return cb_->isChecked(); }
    void setChecked(bool checked) { cb_->setChecked(checked); }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        QWidget::mousePressEvent(event);
        cb_->toggle();
    }

private:
    int id_;
    QString name_;
    int count_;
    QCheckBox* cb_ = nullptr;
};

class LabelFilterWidget : public QWidget {
public:
    LabelFilterWidget(const QVector<Label>& labels,
                      const QHash<int, int>& counts,
                      const QString& countSuffix = QString(),
                      QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(6);

        auto* topLayout = new QHBoxLayout;
        topLayout->setContentsMargins(0, 0, 0, 0);
        topLayout->setSpacing(6);

        searchEdit_ = new QLineEdit(this);
        searchEdit_->setPlaceholderText(QStringLiteral("Search labels…"));
        searchEdit_->setClearButtonEnabled(true);
        topLayout->addWidget(searchEdit_, 1);

        selectAllBtn_ = new QPushButton(QStringLiteral("Select All"), this);
        selectNoneBtn_ = new QPushButton(QStringLiteral("Select None"), this);
        topLayout->addWidget(selectAllBtn_);
        topLayout->addWidget(selectNoneBtn_);
        mainLayout->addLayout(topLayout);

        scrollArea_ = new QScrollArea(this);
        scrollArea_->setWidgetResizable(true);
        scrollArea_->setFrameShape(QFrame::StyledPanel);
        scrollArea_->setMinimumHeight(180);
        scrollArea_->setMaximumHeight(260);

        container_ = new QWidget(scrollArea_);
        containerLayout_ = new QVBoxLayout(container_);
        containerLayout_->setContentsMargins(4, 4, 4, 4);
        containerLayout_->setSpacing(2);

        populateRows(labels, counts, countSuffix);

        scrollArea_->setWidget(container_);
        mainLayout->addWidget(scrollArea_, 1);

        connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& query) {
            const QString trimmed = query.trimmed();
            for (auto* row : rows_) {
                const bool match = trimmed.isEmpty() ||
                                   row->name().contains(trimmed, Qt::CaseInsensitive);
                row->setVisible(match);
            }
        });

        connect(selectAllBtn_, &QPushButton::clicked, this, [this]() {
            for (auto* row : rows_) {
                if (row->isVisible()) {
                    row->setChecked(true);
                }
            }
        });

        connect(selectNoneBtn_, &QPushButton::clicked, this, [this]() {
            for (auto* row : rows_) {
                row->setChecked(false);
            }
        });
    }

    void reload(const QVector<Label>& labels,
                const QHash<int, int>& counts,
                const QString& countSuffix = QString())
    {
        populateRows(labels, counts, countSuffix);
        if (!searchEdit_->text().isEmpty()) {
            const QString trimmed = searchEdit_->text().trimmed();
            for (auto* row : rows_) {
                row->setVisible(trimmed.isEmpty() || row->name().contains(trimmed, Qt::CaseInsensitive));
            }
        }
    }

    [[nodiscard]] bool hasMatchingLabels() const { return !rows_.isEmpty(); }

    [[nodiscard]] QSet<int> selectedLabelIds() const
    {
        QSet<int> result;
        for (auto* row : rows_) {
            if (row->isChecked()) {
                result.insert(row->id());
            }
        }
        return result;
    }

private:
    void populateRows(const QVector<Label>& labels,
                      const QHash<int, int>& counts,
                      const QString& countSuffix)
    {
        for (auto* row : rows_) {
            containerLayout_->removeWidget(row);
            delete row;
        }
        rows_.clear();
        QLayoutItem* item;
        while ((item = containerLayout_->takeAt(0)) != nullptr) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }

        for (const auto& label : labels) {
            const int count = counts.value(label.id, 0);
            if (count <= 0) {
                continue; // Only show labels present on current frame!
            }
            auto* row = new LabelRowWidget(label.id, label.name, label.color, count,
                                           countSuffix, container_);
            containerLayout_->addWidget(row);
            rows_.push_back(row);
        }

        if (rows_.isEmpty()) {
            auto* emptyLabel = new QLabel(QStringLiteral("No matching annotations on this frame"), container_);
            emptyLabel->setStyleSheet(QStringLiteral("color: #888; padding: 16px; font-style: italic;"));
            emptyLabel->setAlignment(Qt::AlignCenter);
            containerLayout_->addWidget(emptyLabel);
        } else {
            for (auto* row : rows_) {
                row->setChecked(true);
            }
        }

        containerLayout_->addStretch(1);
    }

    QLineEdit* searchEdit_ = nullptr;
    QPushButton* selectAllBtn_ = nullptr;
    QPushButton* selectNoneBtn_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* container_ = nullptr;
    QVBoxLayout* containerLayout_ = nullptr;
    QVector<LabelRowWidget*> rows_;
};

namespace {
class ArrowNavigationFilter : public QObject {
public:
    explicit ArrowNavigationFilter(QListWidget* list, QObject* parent = nullptr)
        : QObject(parent), list_(list) {}
protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Down) {
                int next = list_->currentRow() + 1;
                while (next < list_->count() && list_->item(next)->isHidden()) ++next;
                if (next < list_->count()) list_->setCurrentRow(next);
                return true;
            }
            if (keyEvent->key() == Qt::Key_Up) {
                int prev = list_->currentRow() - 1;
                while (prev >= 0 && list_->item(prev)->isHidden()) --prev;
                if (prev >= 0) list_->setCurrentRow(prev);
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
private:
    QListWidget* list_ = nullptr;
};

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
    frameDebounce_ = new QTimer(this);
    frameDebounce_->setSingleShot(true);
    frameDebounce_->setInterval(120);
    connect(frameDebounce_, &QTimer::timeout, this, [this]() {
        requestFrame(requestedFrame_);
    });

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
    toast_ = new ToastWidget(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto shouldIgnoreSingleKey = [this]() -> bool {
        QWidget* focused = focusWidget();
        return focused && (qobject_cast<QLineEdit*>(focused) || qobject_cast<QTextEdit*>(focused));
    };

    toolbar_ = new QToolBar(this);
    toolbar_->setMovable(false);
    toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    layout->addWidget(toolbar_);

    auto* back = toolbar_->addAction(QStringLiteral("Back"));
    back->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));
    back->setShortcutContext(Qt::WindowShortcut);
    this->addAction(back);
    connect(back, &QAction::triggered, this, &JobWorkspace::requestBack);

    titleLabel_ = new QLabel(this);
    titleLabel_->setStyleSheet(QStringLiteral("font-weight: 600; padding: 0 12px;"));
    toolbar_->addWidget(titleLabel_);

    toolbar_->addSeparator();
    saveAction_ = new QAction(QStringLiteral("Save"), this);
    saveAction_->setShortcuts({QKeySequence(QStringLiteral("S")), QKeySequence::Save});
    saveAction_->setShortcutContext(Qt::WindowShortcut);
    saveAction_->setToolTip(QStringLiteral("Save to server (S or Ctrl+S)"));
    this->addAction(saveAction_);
    toolbar_->addAction(saveAction_);
    connect(saveAction_, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        saveRemote();
    });

    auto* changeClassAction = new QAction(QStringLiteral("Change class"), this);
    changeClassAction->setShortcut(QKeySequence(QStringLiteral("L")));
    changeClassAction->setShortcutContext(Qt::WindowShortcut);
    changeClassAction->setToolTip(QStringLiteral("Choose the class to draw (L)"));
    changeClassAction->setStatusTip(QStringLiteral("Choose the active class for drawing (L)"));
    this->addAction(changeClassAction);
    toolbar_->addAction(changeClassAction);
    connect(changeClassAction, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        showNewLabelDialog();
    });

    auto* newLabelAction = new QAction(QStringLiteral("Draw rectangle"), this);
    newLabelAction->setShortcut(QKeySequence(QStringLiteral("N")));
    newLabelAction->setShortcutContext(Qt::WindowShortcut);
    newLabelAction->setToolTip(QStringLiteral("Draw a rectangle with the active class (N)"));
    newLabelAction->setStatusTip(QStringLiteral("Start drawing a rectangle with the active class (N)"));
    this->addAction(newLabelAction);
    toolbar_->addAction(newLabelAction);
    connect(newLabelAction, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        const Label* activeLabel = model_->label(labelPanel_->activeLabel());
        if (!activeLabel) {
            setStatus(QStringLiteral("Choose a class first (L)"), true);
            return;
        }
        for (auto* action : toolGroup_->actions()) {
            if (action->data().toInt() == static_cast<int>(CanvasTool::Rectangle)) {
                action->setChecked(true);
                break;
            }
        }
        canvas_->setTool(CanvasTool::Rectangle);
        setStatus(QStringLiteral("Ready to draw '%1' (N)").arg(activeLabel->name));
    });

    auto* copyLabelAction = new QAction(QStringLiteral("Copy label"), this);
    copyLabelAction->setShortcut(QKeySequence(QStringLiteral("C")));
    copyLabelAction->setShortcutContext(Qt::WindowShortcut);
    copyLabelAction->setToolTip(QStringLiteral("Copy label (C) - class and dimensions"));
    copyLabelAction->setStatusTip(QStringLiteral("Copy the selected object's class and dimensions (C)"));
    this->addAction(copyLabelAction);
    toolbar_->addAction(copyLabelAction);
    connect(copyLabelAction, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        copyLabel();
    });

    auto* pasteLabelAction = new QAction(QStringLiteral("Place label"), this);
    pasteLabelAction->setShortcut(QKeySequence(QStringLiteral("V")));
    pasteLabelAction->setShortcutContext(Qt::WindowShortcut);
    pasteLabelAction->setToolTip(QStringLiteral("Place copied label (V)"));
    pasteLabelAction->setStatusTip(QStringLiteral("Place the copied class and dimensions onto the image (V)"));
    this->addAction(pasteLabelAction);
    toolbar_->addAction(pasteLabelAction);
    connect(pasteLabelAction, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        pasteLabel();
    });

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

    undoAction_ = new QAction(QStringLiteral("Undo"), this);
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setShortcutContext(Qt::WindowShortcut);
    this->addAction(undoAction_);
    toolbar_->addAction(undoAction_);
    connect(undoAction_, &QAction::triggered, this, [this]() { model_->undo(); });

    redoAction_ = new QAction(QStringLiteral("Redo"), this);
    redoAction_->setShortcut(QKeySequence::Redo);
    redoAction_->setShortcutContext(Qt::WindowShortcut);
    this->addAction(redoAction_);
    toolbar_->addAction(redoAction_);
    connect(redoAction_, &QAction::triggered, this, [this]() { model_->redo(); });

    deleteAction_ = new QAction(QStringLiteral("Delete"), this);
    deleteAction_->setShortcut(QKeySequence::Delete);
    deleteAction_->setShortcutContext(Qt::WindowShortcut);
    this->addAction(deleteAction_);
    toolbar_->addAction(deleteAction_);
    connect(deleteAction_, &QAction::triggered, this, &JobWorkspace::deleteSelection);

    toolbar_->addSeparator();
    auto* previous = new QAction(QStringLiteral("◀ Frame"), this);
    previous->setShortcut(QKeySequence(QStringLiteral("D")));
    previous->setShortcutContext(Qt::WindowShortcut);
    previous->setToolTip(QStringLiteral("Previous frame (D)"));
    this->addAction(previous);
    toolbar_->addAction(previous);
    connect(previous, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
        setFrame(model_->currentFrame() - 1);
    });
    auto* next = new QAction(QStringLiteral("Frame ▶"), this);
    next->setShortcut(QKeySequence(QStringLiteral("F")));
    next->setShortcutContext(Qt::WindowShortcut);
    next->setToolTip(QStringLiteral("Next frame (F)"));
    this->addAction(next);
    toolbar_->addAction(next);
    connect(next, &QAction::triggered, this, [this, shouldIgnoreSingleKey]() {
        if (shouldIgnoreSingleKey()) return;
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
    auto addTool = [this, shouldIgnoreSingleKey](const QString& text, CanvasTool tool, const QString& shortcut) {
        auto* action = new QAction(text, this);
        action->setCheckable(true);
        action->setData(static_cast<int>(tool));
        action->setShortcut(QKeySequence(shortcut));
        action->setShortcutContext(Qt::WindowShortcut);
        this->addAction(action);
        toolGroup_->addAction(action);
        toolbar_->addAction(action);
        return action;
    };
    addTool(QStringLiteral("Select"), CanvasTool::Select, QStringLiteral("Q"))->setChecked(true);
    addTool(QStringLiteral("Box"), CanvasTool::Rectangle, QStringLiteral("R"));
    connect(toolGroup_, &QActionGroup::triggered, this, [this, shouldIgnoreSingleKey](QAction* action) {
        if (shouldIgnoreSingleKey()) return;
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
    splitter->setSizes({1000, 420});
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
    connect(canvas_, &AnnotationCanvas::annotationContextMenuRequested,
            this, &JobWorkspace::showLabelContextMenu);
    connect(canvas_, &AnnotationCanvas::toolChanged, this, [this](CanvasTool tool) {
        for (auto* action : toolGroup_->actions()) {
            if (action->data().toInt() == static_cast<int>(tool)) {
                action->setChecked(true);
                break;
            }
        }
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
        saveLocalSnapshot();
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
        labels_ = flattenLabelHierarchy(labels);
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
    requestedFrame_ = clamped;
    model_->setCurrentFrame(clamped);
    {
        QSignalBlocker spinBlocker(frameSpin_);
        QSignalBlocker sliderBlocker(frameSlider_);
        frameSpin_->setValue(clamped);
        frameSlider_->setValue(clamped);
    }
    frameDebounce_->start();
    refreshObjects();
    updateTitle();
}

void JobWorkspace::requestFrame(int frame)
{
    const quint64 generation = ++frameGeneration_;
    setStatus(QStringLiteral("Loading frame %1…").arg(frame));
    frameRequest_ = media_->requestFrame(
        job_.id, frame, QStringLiteral("compressed"),
        [this, frame, generation](Result<QImage> result) {
            if (generation != frameGeneration_) {
                return;
            }
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
                    QStringLiteral("%1  ·  %2").arg(name, shapeTypeName(visible.shape.type)),
                    objectList_);
                item->setToolTip(QStringLiteral("%1 · %2 · %3 #%4")
                    .arg(name, shapeTypeName(visible.shape.type), kind)
                    .arg(visible.objectId));
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
        setStatus(QStringLiteral("Select a label first to copy its class and dimensions"), true);
        showToast(QStringLiteral("✕ Select a label first to copy"), true);
        return;
    }

    const auto visible = model_->visibleShape(key);
    if (!visible) {
        setStatus(QStringLiteral("Select a visible shape to copy its class and dimensions"), true);
        showToast(QStringLiteral("✕ Select a visible shape to copy"), true);
        return;
    }

    const Shape& shape = visible->shape;
    const Label* label = model_->label(shape.labelId);

    copiedTemplate_.valid = true;
    copiedTemplate_.labelId = shape.labelId;
    copiedTemplate_.labelName = label ? label->name : QStringLiteral("label %1").arg(shape.labelId);
    copiedTemplate_.color = label ? QColor(label->color) : QColor(76, 154, 255);
    copiedTemplate_.shapeType = shape.type;
    copiedTemplate_.attributes = shape.attributes;
    copiedTemplate_.occluded = shape.occluded;
    copiedTemplate_.rotation = shape.rotation;
    copiedTemplate_.zOrder = shape.zOrder;
    copiedLabelId_ = shape.labelId;

    if (shape.points.size() >= 4 && (shape.type == QStringLiteral("rectangle") || shape.type == QStringLiteral("ellipse"))) {
        const double x1 = std::min(shape.points[0], shape.points[2]);
        const double x2 = std::max(shape.points[0], shape.points[2]);
        const double y1 = std::min(shape.points[1], shape.points[3]);
        const double y2 = std::max(shape.points[1], shape.points[3]);
        copiedTemplate_.width = std::max(2.0, x2 - x1);
        copiedTemplate_.height = std::max(2.0, y2 - y1);
        copiedTemplate_.relativePoints = {
            -copiedTemplate_.width / 2.0, -copiedTemplate_.height / 2.0,
             copiedTemplate_.width / 2.0,  copiedTemplate_.height / 2.0
        };
    } else {
        const RectD box = Geometry::boundingRect(shape.points);
        copiedTemplate_.width = std::max(2.0, box.width);
        copiedTemplate_.height = std::max(2.0, box.height);
        const double cx = box.center().x();
        const double cy = box.center().y();
        copiedTemplate_.relativePoints.clear();
        for (int i = 0; i + 1 < shape.points.size(); i += 2) {
            copiedTemplate_.relativePoints.push_back(shape.points[i] - cx);
            copiedTemplate_.relativePoints.push_back(shape.points[i + 1] - cy);
        }
    }

    const QString msg = QStringLiteral("Copied %1 [%2 × %3 px] — press V to place")
        .arg(copiedTemplate_.labelName)
        .arg(static_cast<int>(std::round(copiedTemplate_.width)))
        .arg(static_cast<int>(std::round(copiedTemplate_.height)));
    setStatus(msg);
}

void JobWorkspace::pasteLabel()
{
    if (!model_ || !copiedTemplate_.valid) {
        setStatus(QStringLiteral("No copied label. Select a label and press C first"), true);
        showToast(QStringLiteral("✕ No copied label. Press C on a label first"), true);
        return;
    }

    labelPanel_->setActiveLabel(copiedTemplate_.labelId);
    canvas_->setActiveLabel(copiedTemplate_.labelId);
    canvas_->startTemplatePlacement(copiedTemplate_);

    const QString msg = QStringLiteral("Placing %1 [%2 × %3 px] — click to place onto image (Esc to cancel)")
        .arg(copiedTemplate_.labelName)
        .arg(static_cast<int>(std::round(copiedTemplate_.width)))
        .arg(static_cast<int>(std::round(copiedTemplate_.height)));
    setStatus(msg);
    showToast(msg);
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

void JobWorkspace::showNewLabelDialog()
{
    if (labels_.isEmpty()) {
        setStatus(QStringLiteral("This job has no labels defined"), true);
        showToast(QStringLiteral("✕ No labels defined for this job"), true);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Draw New Label · Select Class (L)"));
    dialog.resize(380, 420);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(8);

    auto* header = new QLabel(QStringLiteral("<b>Choose a class to draw:</b>"), &dialog);
    layout->addWidget(header);

    auto* searchEdit = new QLineEdit(&dialog);
    searchEdit->setPlaceholderText(QStringLiteral("Search classes (type to filter)…"));
    searchEdit->setClearButtonEnabled(true);
    layout->addWidget(searchEdit);

    auto* listWidget = new QListWidget(&dialog);
    listWidget->setAlternatingRowColors(true);
    listWidget->setSpacing(2);

    const int activeId = labelPanel_->activeLabel();
    QListWidgetItem* activeItem = nullptr;

    for (int i = 0; i < labels_.size(); ++i) {
        const auto& label = labels_[i];
        QPixmap pixmap(14, 14);
        pixmap.fill(QColor(label.color.isEmpty() ? QStringLiteral("#888888") : label.color));

        QString itemText = label.name;
        if (i < 9) {
            itemText += QStringLiteral("  [%1]").arg(i + 1);
        }
        auto* item = new QListWidgetItem(QIcon(pixmap), itemText, listWidget);
        item->setData(Qt::UserRole, label.id);
        item->setData(Qt::UserRole + 1, label.name);
        item->setSizeHint(QSize(0, 28));

        if (label.id == activeId) {
            activeItem = item;
        }
    }

    if (activeItem) {
        listWidget->setCurrentItem(activeItem);
    } else if (listWidget->count() > 0) {
        listWidget->setCurrentRow(0);
    }

    layout->addWidget(listWidget, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (auto* okBtn = buttonBox->button(QDialogButtonBox::Ok)) {
        okBtn->setText(QStringLiteral("Draw Rectangle"));
    }
    layout->addWidget(buttonBox);

    auto selectFirstVisible = [listWidget]() {
        for (int i = 0; i < listWidget->count(); ++i) {
            auto* it = listWidget->item(i);
            if (!it->isHidden()) {
                listWidget->setCurrentItem(it);
                break;
            }
        }
    };

    connect(searchEdit, &QLineEdit::textChanged, &dialog, [listWidget, selectFirstVisible](const QString& text) {
        const QString trimmed = text.trimmed();
        for (int i = 0; i < listWidget->count(); ++i) {
            auto* item = listWidget->item(i);
            const QString name = item->data(Qt::UserRole + 1).toString();
            const bool match = trimmed.isEmpty() || name.contains(trimmed, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
        selectFirstVisible();
    });

    auto* navFilter = new ArrowNavigationFilter(listWidget, &dialog);
    searchEdit->installEventFilter(navFilter);

    connect(searchEdit, &QLineEdit::returnPressed, &dialog, [&dialog]() {
        dialog.accept();
    });

    connect(listWidget, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    searchEdit->setFocus();

    if (dialog.exec() == QDialog::Accepted) {
        auto* current = listWidget->currentItem();
        if (!current || current->isHidden()) {
            selectFirstVisible();
            current = listWidget->currentItem();
        }
        if (current) {
            const int labelId = current->data(Qt::UserRole).toInt();
            const QString labelName = current->data(Qt::UserRole + 1).toString();
            labelPanel_->setActiveLabel(labelId);
            canvas_->setActiveLabel(labelId);

            for (auto* action : toolGroup_->actions()) {
                if (action->data().toInt() == static_cast<int>(CanvasTool::Rectangle)) {
                    action->setChecked(true);
                    break;
                }
            }
            canvas_->setTool(CanvasTool::Rectangle);

            const QString msg = QStringLiteral("Ready to draw '%1' — click opposite corners to create rectangle (Esc to cancel)")
                                    .arg(labelName);
            setStatus(msg);
            showToast(QStringLiteral("Drawing '%1'").arg(labelName), false, 2500);
        }
    }
}

void JobWorkspace::showLabelContextMenu(const QString& key, const QPoint& globalPos)
{
    if (!model_) {
        return;
    }
    const auto optVisible = model_->visibleShape(key);
    if (!optVisible) {
        return;
    }
    const auto visible = *optVisible;
    const int currentLabelId = visible.shape.labelId;
    const Label* currentLabel = model_->label(currentLabelId);
    const QString currentName = currentLabel ? currentLabel->name : QStringLiteral("id %1").arg(currentLabelId);

    QMenu menu(this);
    auto* headerAction = menu.addAction(QStringLiteral("Change Class (Current: %1)").arg(currentName));
    headerAction->setEnabled(false);
    QFont hFont = headerAction->font();
    hFont.setBold(true);
    headerAction->setFont(hFont);
    menu.addSeparator();

    for (const auto& label : labels_) {
        QPixmap pixmap(14, 14);
        pixmap.fill(QColor(label.color.isEmpty() ? QStringLiteral("#888888") : label.color));
        auto* action = menu.addAction(QIcon(pixmap), label.name);
        action->setCheckable(true);
        if (label.id == currentLabelId) {
            action->setChecked(true);
        }
        connect(action, &QAction::triggered, this, [this, key, label]() {
            if (model_->setObjectLabel(key, label.id)) {
                canvas_->scene()->update();
                canvas_->viewport()->update();
                QCoreApplication::processEvents();
                const QString msg = QStringLiteral("Changed label to '%1'; press S to save").arg(label.name);
                setStatus(msg);
                showToast(QStringLiteral("✓ ") + msg, false, 2500);
            }
        });
    }

    menu.exec(globalPos);
}

void JobWorkspace::showGrowDialog()
{
    if (!model_) {
        return;
    }

    if (growDialog_) {
        growDialog_->raise();
        growDialog_->activateWindow();
        return;
    }

    auto* dialog = new QDialog(this);
    growDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlags(Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
    dialog->resize(460, 460);

    auto* layout = new QVBoxLayout(dialog);
    layout->setSpacing(10);

    auto* header = new QLabel(dialog);
    layout->addWidget(header);

    auto countRectsForFrame = [this]() -> QHash<int, int> {
        QHash<int, int> counts;
        for (const auto& visible : model_->visibleShapes()) {
            if (visible.shape.type == QStringLiteral("rectangle") &&
                visible.shape.points.size() == 4) {
                counts[visible.shape.labelId]++;
            }
        }
        return counts;
    };

    auto* filter = new LabelFilterWidget(labels_, countRectsForFrame(), QStringLiteral("rectangles"), dialog);
    layout->addWidget(filter, 1);

    auto updateHeader = [this, header, filter, dialog]() {
        const int frame = model_->currentFrame();
        dialog->setWindowTitle(QStringLiteral("Grow / shrink rectangles · Frame %1").arg(frame));
        if (filter->hasMatchingLabels()) {
            header->setText(QStringLiteral("<b>Frame %1</b>: Select rectangle classes to grow or shrink in real time.")
                                .arg(frame));
        } else {
            header->setText(QStringLiteral("<b>Frame %1</b>: <span style=\"color:#e53e3e;\">No rectangle annotations on this frame.</span>")
                                .arg(frame));
        }
    };
    updateHeader();

    auto* form = new QFormLayout;
    auto* amount = new QDoubleSpinBox(dialog);
    amount->setRange(0.1, 2000.0);
    amount->setValue(5.0);
    amount->setSingleStep(1.0);
    amount->setSuffix(QStringLiteral(" px per side"));
    amount->setToolTip(QStringLiteral("Amount in pixels to expand (Grow) or contract (Shrink) rectangle boundaries"));
    form->addRow(QStringLiteral("Amount:"), amount);
    layout->addLayout(form);

    auto* actionLayout = new QHBoxLayout;
    actionLayout->setSpacing(8);

    auto* growBtn = new QPushButton(QStringLiteral("Grow (+)"), dialog);
    growBtn->setStyleSheet(QStringLiteral("font-weight: 600; padding: 6px 14px;"));
    auto* shrinkBtn = new QPushButton(QStringLiteral("Shrink (−)"), dialog);
    shrinkBtn->setStyleSheet(QStringLiteral("font-weight: 600; padding: 6px 14px;"));
    auto* undoBtn = new QPushButton(QStringLiteral("Undo"), dialog);
    undoBtn->setEnabled(model_->canUndo());
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dialog);

    const bool hasRects = filter->hasMatchingLabels();
    growBtn->setEnabled(hasRects);
    shrinkBtn->setEnabled(hasRects);

    actionLayout->addWidget(growBtn);
    actionLayout->addWidget(shrinkBtn);
    actionLayout->addWidget(undoBtn);
    actionLayout->addStretch(1);
    actionLayout->addWidget(closeBtn);
    layout->addLayout(actionLayout);

    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::close);

    auto apply = [this, filter, amount, undoBtn](double direction) -> bool {
        const QSet<int> selectedLabels = filter->selectedLabelIds();
        if (selectedLabels.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("No labels selected"),
                                 QStringLiteral("Check at least one label to grow or shrink."));
            return false;
        }

        const double step = amount->value();
        const double delta = direction * step;
        QVector<QPair<QString, Shape>> updates;

        for (const auto& visible : model_->visibleShapes()) {
            if (visible.shape.type != QStringLiteral("rectangle") ||
                visible.shape.points.size() != 4 ||
                !selectedLabels.contains(visible.shape.labelId)) {
                continue;
            }
            Shape shape = visible.shape;
            const double x1 = std::min(shape.points[0], shape.points[2]);
            const double x2 = std::max(shape.points[0], shape.points[2]);
            const double y1 = std::min(shape.points[1], shape.points[3]);
            const double y2 = std::max(shape.points[1], shape.points[3]);
            if (x2 - x1 + 2 * delta <= 0 || y2 - y1 + 2 * delta <= 0) {
                continue;
            }
            shape.points = {x1 - delta, y1 - delta, x2 + delta, y2 + delta};
            updates.push_back({visible.key, shape});
        }

        if (updates.isEmpty()) {
            showToast(QStringLiteral("No rectangles could be resized"), true);
            return false;
        }

        const int changed = model_->updateVisibleShapes(updates);
        if (changed > 0) {
            canvas_->scene()->update();
            canvas_->viewport()->update();
            QCoreApplication::processEvents();

            const QString actionWord = direction > 0 ? QStringLiteral("Grown") : QStringLiteral("Shrunk");
            const QString msg = QStringLiteral("%1 %2 rectangle(s) by %3 px on frame %4; press S to save")
                                    .arg(actionWord).arg(changed).arg(step, 0, 'f', 1).arg(model_->currentFrame());
            setStatus(msg);
            showToast(QStringLiteral("✓ ") + msg, false, 2500);
            undoBtn->setEnabled(model_->canUndo());
            return true;
        }
        return false;
    };

    connect(growBtn, &QPushButton::clicked, dialog, [apply]() { apply(1.0); });
    connect(shrinkBtn, &QPushButton::clicked, dialog, [apply]() { apply(-1.0); });

    connect(undoBtn, &QPushButton::clicked, dialog, [this, undoBtn]() {
        if (model_->undo()) {
            canvas_->scene()->update();
            canvas_->viewport()->update();
            QCoreApplication::processEvents();
            undoBtn->setEnabled(model_->canUndo());
            showToast(QStringLiteral("Undone last action"), false, 2000);
        }
    });

    connect(model_, &AnnotationModel::undoStackChanged, dialog, [this, undoBtn]() {
        undoBtn->setEnabled(model_->canUndo());
    });

    connect(model_, &AnnotationModel::frameChanged, dialog,
            [this, filter, updateHeader, countRectsForFrame, growBtn, shrinkBtn](int) {
        filter->reload(labels_, countRectsForFrame(), QStringLiteral("rectangles"));
        updateHeader();
        const bool hasRects = filter->hasMatchingLabels();
        growBtn->setEnabled(hasRects);
        shrinkBtn->setEnabled(hasRects);
    });

    dialog->show();
}

void JobWorkspace::showMoveDialog()
{
    if (!model_) {
        return;
    }
    const int frame = model_->currentFrame();

    // Count all visible annotations per label strictly on the current frame
    QHash<int, int> shapeCounts;
    for (const auto& visible : model_->visibleShapes()) {
        shapeCounts[visible.shape.labelId]++;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Move annotations · Frame %1").arg(frame));
    dialog.resize(460, 440);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    auto* header = new QLabel(
        QStringLiteral("<b>Frame %1</b>: Select classes to shift in position.").arg(frame), &dialog);
    layout->addWidget(header);

    auto* filter = new LabelFilterWidget(labels_, shapeCounts, QStringLiteral("items"), &dialog);
    layout->addWidget(filter, 1);

    auto* form = new QFormLayout;
    auto* dx = new QDoubleSpinBox(&dialog);
    auto* dy = new QDoubleSpinBox(&dialog);
    dx->setRange(-100000, 100000);
    dy->setRange(-100000, 100000);
    dx->setValue(0.0);
    dy->setValue(0.0);
    dx->setSuffix(QStringLiteral(" px"));
    dy->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("X offset"), dx);
    form->addRow(QStringLiteral("Y offset"), dy);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (!filter->hasMatchingLabels()) {
        if (auto* okBtn = buttons->button(QDialogButtonBox::Ok)) {
            okBtn->setEnabled(false);
        }
    }
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        const QSet<int> selectedLabels = filter->selectedLabelIds();
        if (selectedLabels.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("No labels selected"),
                                 QStringLiteral("Check at least one label to move."));
            return;
        }
        if (dx->value() == 0.0 && dy->value() == 0.0) {
            dialog.accept();
            return;
        }
        QVector<QPair<QString, Shape>> updates;
        for (const auto& visible : model_->visibleShapes()) {
            if (!selectedLabels.contains(visible.shape.labelId)) {
                continue;
            }
            Shape shape = visible.shape;
            Geometry::translate(shape.points, dx->value(), dy->value());
            updates.push_back({visible.key, shape});
        }
        const int changed = model_->updateVisibleShapes(updates);
        canvas_->scene()->update();
        canvas_->viewport()->update();
        QCoreApplication::processEvents();

        const QString msg = QStringLiteral("Moved %1 annotation(s) on frame %2; press S to save")
                                .arg(changed).arg(frame);
        setStatus(msg);
        showToast(QStringLiteral("✓ ") + msg, false, 3000);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void JobWorkspace::showRelabelDialog()
{
    if (!model_) {
        return;
    }
    const int frame = model_->currentFrame();

    QHash<int, int> shapeCounts;
    for (const auto& visible : model_->visibleShapes()) {
        shapeCounts[visible.shape.labelId]++;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Relabel annotations · Frame %1").arg(frame));
    dialog.resize(440, 240);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    auto* header = new QLabel(
        QStringLiteral("<b>Frame %1</b>: Replace a class with another on this frame.").arg(frame), &dialog);
    layout->addWidget(header);

    auto* form = new QFormLayout;
    auto* from = new QComboBox(&dialog);
    auto* to = new QComboBox(&dialog);
    for (const auto& label : labels_) {
        const int count = shapeCounts.value(label.id, 0);
        if (count > 0) {
            from->addItem(QStringLiteral("%1 (%2 on frame)").arg(label.name).arg(count), label.id);
        }
        to->addItem(label.name, label.id);
    }
    form->addRow(QStringLiteral("Replace"), from);
    form->addRow(QStringLiteral("With"), to);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (from->count() == 0) {
        if (auto* okBtn = buttons->button(QDialogButtonBox::Ok)) {
            okBtn->setEnabled(false);
        }
        header->setText(QStringLiteral("<b>Frame %1</b>: <span style=\"color:#e53e3e;\">No annotations on this frame to relabel.</span>").arg(frame));
    }
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (from->count() == 0) {
            dialog.reject();
            return;
        }
        const int source = from->currentData().toInt();
        const int target = to->currentData().toInt();
        if (source == target) {
            dialog.accept();
            return;
        }
        int changed = 0;
        for (const auto& visible : model_->visibleShapes()) {
            if (visible.shape.labelId == source) {
                changed += model_->setObjectLabel(visible.key, target) ? 1 : 0;
            }
        }
        const QString msg = QStringLiteral("Relabeled %1 annotation(s) on frame %2; press S to save")
                                .arg(changed).arg(frame);
        setStatus(msg);
        showToast(QStringLiteral("✓ ") + msg, false, 3000);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void JobWorkspace::showDeleteDialog()
{
    if (!model_) {
        return;
    }
    const int frame = model_->currentFrame();

    QHash<int, int> shapeCounts;
    for (const auto& visible : model_->visibleShapes()) {
        shapeCounts[visible.shape.labelId]++;
    }
    for (const auto& tag : model_->visibleTags()) {
        shapeCounts[tag.labelId]++;
    }

    const bool hasSelection = !model_->selection().isEmpty();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Delete annotations · Frame %1").arg(frame));
    dialog.resize(460, 440);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);

    auto* header = new QLabel(
        QStringLiteral("<b>Frame %1</b>: Select classes to delete from this frame.").arg(frame), &dialog);
    layout->addWidget(header);

    auto* filter = new LabelFilterWidget(labels_, shapeCounts, QStringLiteral("items"), &dialog);
    layout->addWidget(filter, 1);

    QCheckBox* onlySelectionBox = nullptr;
    if (hasSelection) {
        onlySelectionBox = new QCheckBox(
            QStringLiteral("Only delete currently selected annotation(s) of the checked classes"), &dialog);
        layout->addWidget(onlySelectionBox);
    }

    auto* buttons = new QDialogButtonBox(&dialog);
    auto* deleteBtn = buttons->addButton(QStringLiteral("Delete"), QDialogButtonBox::DestructiveRole);
    if (!filter->hasMatchingLabels()) {
        deleteBtn->setEnabled(false);
    }
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(deleteBtn, &QPushButton::clicked, &dialog, [&]() {
        const QSet<int> selectedLabels = filter->selectedLabelIds();
        if (selectedLabels.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("No labels selected"),
                                 QStringLiteral("Check at least one label to delete."));
            return;
        }
        const bool onlySelected = onlySelectionBox && onlySelectionBox->isChecked();
        const auto activeSelection = model_->selection();

        QSet<QString> keysToDelete;
        for (const auto& visible : model_->visibleShapes()) {
            if (!selectedLabels.contains(visible.shape.labelId)) {
                continue;
            }
            if (onlySelected && !activeSelection.contains(visible.key)) {
                continue;
            }
            keysToDelete.insert(visible.key);
        }
        for (const auto& tag : model_->visibleTags()) {
            if (!selectedLabels.contains(tag.labelId)) {
                continue;
            }
            const QString tagKey = model_->keyForTag(tag);
            if (onlySelected && !activeSelection.contains(tagKey)) {
                continue;
            }
            keysToDelete.insert(tagKey);
        }

        if (keysToDelete.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Nothing to delete"),
                                     QStringLiteral("No matching annotations found to delete on frame %1.").arg(frame));
            return;
        }

        const int count = keysToDelete.size();
        model_->removeObjects(keysToDelete);
        canvas_->scene()->update();
        canvas_->viewport()->update();
        QCoreApplication::processEvents();

        const QString msg = QStringLiteral("Deleted %1 annotation(s) on frame %2; press S to save")
                                .arg(count).arg(frame);
        setStatus(msg);
        showToast(msg, false, 3000);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
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
    titleLabel_->setToolTip(title);
    const bool dirty = model_ && model_->isDirty();
    if (dirty) {
        dirtyLabel_->setText(QStringLiteral("● Saved locally · press S to save to server"));
        dirtyLabel_->setStyleSheet(QStringLiteral(
            "color: #eab308; font-weight: 600; padding: 2px 8px; "
            "border-radius: 4px; background: rgba(234, 179, 8, 0.15);"));
    } else if (annotationsLoaded_) {
        dirtyLabel_->setText(QStringLiteral("✓ Saved to server"));
        dirtyLabel_->setStyleSheet(QStringLiteral(
            "color: #22c55e; font-weight: 600; padding: 2px 8px; "
            "border-radius: 4px; background: rgba(34, 197, 94, 0.15);"));
    } else {
        dirtyLabel_->setText(QString());
    }
}

void JobWorkspace::setStatus(const QString& message, bool error)
{
    statusLabel_->setStyleSheet(error ? QStringLiteral("color: #c44;")
                                      : QStringLiteral("color: #888;"));
    statusLabel_->setText(message);
}

void JobWorkspace::showToast(const QString& message, bool isError, int durationMs)
{
    if (!toast_) {
        toast_ = new ToastWidget(this);
    }
    toast_->showToast(message, isError, durationMs);
}

void JobWorkspace::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (toast_ && toast_->isVisible()) {
        const int x = (width() - toast_->width()) / 2;
        toast_->move(x, 46);
    }
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
        setStatus(QStringLiteral("Cannot save: annotations still loading from server"), true);
        if (!silent) {
            showToast(QStringLiteral("✕ Still loading annotations — try again in a moment"), true, 4000);
        }
        if (done) {
            done(false);
        }
        return;
    }

    const auto patch = model_->buildPatch();
    if (!patch.any()) {
        model_->markClean();
        dirtyLabel_->setText(QStringLiteral("✓ Saved to server"));
        dirtyLabel_->setStyleSheet(QStringLiteral(
            "color: #22c55e; font-weight: 600; padding: 2px 8px; "
            "border-radius: 4px; background: rgba(34, 197, 94, 0.15);"));
        if (!silent) {
            setStatus(QStringLiteral("Server up to date · No unsaved changes"));
            showToast(QStringLiteral("✓ Server up to date · No unsaved changes"), false, 3000);
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
    if (!silent) {
        showToast(QStringLiteral("⏳ Saving changes to server…"), false, 10000);
        setStatus(QStringLiteral("Saving changes to server…"));
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

    const QPointer<JobWorkspace> self(this);
    auto finish = [self, operations, done, silent](bool ok, const QString& error) {
        if (!self) {
            if (done) {
                done(ok);
            }
            return;
        }
        if (!ok) {
            self->saving_ = false;
            self->setStatus(QStringLiteral("Save failed: ") + error, true);
            if (!silent) {
                self->showToast(QStringLiteral("✕ Save failed: ") + error, true, 5000);
            }
            self->updateActions();
            if (done) {
                done(false);
            }
            return;
        }

        // All PATCH operations succeeded — mark clean immediately so the UI
        // reflects the saved state without waiting for a second round-trip.
        for (const auto& operation : *operations) {
            if (self->recovery_ && operation.pendingId > 0) {
                self->recovery_->removePatch(operation.pendingId);
            }
        }
        self->model_->markClean();
        self->annotationsLoaded_ = true;
        self->saving_ = false;
        if (self->recovery_) {
            self->recovery_->clearJob(self->job_.id);
        }
        if (self->store_) {
            self->store_->removeSnapshot(self->job_.id, -1);
        }
        if (!silent) {
            self->setStatus(QStringLiteral("Saved changes to server"));
            self->showToast(QStringLiteral("✓ Saved changes to server"), false, 3000);
        }
        self->dirtyLabel_->setText(QStringLiteral("✓ Saved to server"));
        self->dirtyLabel_->setStyleSheet(QStringLiteral(
            "color: #22c55e; font-weight: 600; padding: 2px 8px; "
            "border-radius: 4px; background: rgba(34, 197, 94, 0.15);"));
        self->updateActions();
        self->updateTitle();
        if (done) {
            done(true);
        }

        // Reload annotations silently in the background to sync server-assigned
        // IDs (e.g. newly created shapes get their real id from the server).
        // If this fails we stay in the already-clean state — no error shown.
        self->annotations_->get(self->job_.id, [self](Result<LabeledData> reloaded) {
            if (!self || !reloaded || !self->model_) {
                return;
            }
            if (self->model_->isDirty()) {
                return;
            }
            const int frame = self->model_->currentFrame();
            self->model_->load(*reloaded, frame);
            self->model_->markClean();
            self->refreshObjects();
        });
    };

    struct SaveRunner {
        std::function<void(int)> run;
    };
    auto runner = std::make_shared<SaveRunner>();
    runner->run = [self, operations, runner, finish](int index) {
        if (!self) {
            runner->run = nullptr;
            return;
        }
        if (index >= operations->size()) {
            auto finishCb = finish;
            runner->run = nullptr;
            finishCb(true, {});
            return;
        }
        const Operation operation = (*operations)[index];
        self->annotations_->patch(self->job_.id, operation.action, operation.data,
                                  [self, runner, index, finish](Result<ApiResponse> result) {
            if (!result) {
                auto finishCb = finish;
                if (runner) {
                    runner->run = nullptr;
                }
                finishCb(false, result.error().toString());
                return;
            }
            if (self && runner && runner->run) {
                runner->run(index + 1);
            }
        });
    };
    runner->run(0);
}

void JobWorkspace::saveLocalSnapshot()
{
    if (!store_ || !store_->isOpen() || !model_) {
        return;
    }
    if (snapshotBusy_) {
        snapshotPending_ = true;
        return;
    }
    const quint64 revision = model_->revision();
    if (revision == lastSavedRevision_) {
        return;
    }

    const QString databasePath = store_->databasePath();
    const int jobId = job_.id;
    const LabeledData snapshot = model_->data();
    snapshotBusy_ = true;

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, revision]() {
        if (watcher->result()) {
            lastSavedRevision_ = revision;
        }
        snapshotBusy_ = false;
        watcher->deleteLater();
        if (snapshotPending_ || (model_ && model_->revision() != lastSavedRevision_)) {
            snapshotPending_ = false;
            saveLocalSnapshot();
        }
    });
    watcher->setFuture(QtConcurrent::run([databasePath, jobId, snapshot]() {
        LocalStore workerStore;
        if (!workerStore.open(databasePath)) {
            return false;
        }
        RecoveryStore workerRecovery(&workerStore);
        const bool saved = workerRecovery.saveSnapshot(jobId, -1, snapshot);
        workerStore.close();
        return saved;
    }));
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
