#include "cvatnative/ui/AnnotationCanvas.h"

#include "cvatnative/annotation/Geometry.h"
#include "cvatnative/ui/AnnotationItems.h"

#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace cvat {

AnnotationCanvas::AnnotationCanvas(QWidget* parent)
    : QGraphicsView(parent),
      scene_(new QGraphicsScene(this))
{
    setScene(scene_);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                   QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setBackgroundBrush(QColor(38, 40, 43));
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    background_ = new QGraphicsPixmapItem;
    background_->setZValue(-1000);
    background_->setTransformationMode(Qt::SmoothTransformation);
    scene_->addItem(background_);

    // Guides are scene items so they stay 1 px wide at any zoom. They are not
    // selectable or movable and do not accept mouse input, so they never
    // intercept the pointer.
    auto makeGuide = [this]() {
        auto* line = new QGraphicsLineItem;
        QPen pen(QColor(230, 30, 30));
        pen.setWidth(1);
        pen.setCosmetic(true);
        line->setPen(pen);
        line->setZValue(900);
        line->setVisible(false);
        scene_->addItem(line);
        return line;
    };
    crosshairH_ = makeGuide();
    crosshairV_ = makeGuide();

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() { updateCrosshair(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { updateCrosshair(); });
    connect(this, &AnnotationCanvas::zoomChanged, this, [this]() { updateCrosshair(); });
}

void AnnotationCanvas::setModel(AnnotationModel* model)
{
    if (model_) {
        disconnect(model_, nullptr, this, nullptr);
    }
    model_ = model;
    if (!model_) {
        refreshItems();
        return;
    }
    connect(model_, &AnnotationModel::changed, this, &AnnotationCanvas::refreshItems);
    connect(model_, &AnnotationModel::selectionChanged, this,
            &AnnotationCanvas::updateSelectionVisuals);
    refreshItems();
}

void AnnotationCanvas::setFrameImage(const QImage& image)
{
    frame_ = image;
    if (image.isNull()) {
        background_->setPixmap(QPixmap());
        return;
    }
    background_->setPixmap(QPixmap::fromImage(image));
    scene_->setSceneRect(QRectF(0, 0, image.width(), image.height()));
    if (firstImage_) {
        firstImage_ = false;
        fitToView();
    }
    emit statusMessage(QStringLiteral("%1 × %2").arg(image.width()).arg(image.height()));
}

void AnnotationCanvas::setTool(CanvasTool tool)
{
    clearPendingCreation();
    clearTemplatePlacement();
    tool_ = tool;
    switch (tool_) {
    case CanvasTool::Select:
        setCursor(Qt::ArrowCursor);
        break;
    case CanvasTool::Rectangle:
        setCursor(Qt::CrossCursor);
        emit statusMessage(QStringLiteral("Click to place first corner of rectangle (Esc to cancel)"));
        break;
    default:
        setCursor(Qt::CrossCursor);
        break;
    }
    updateCrosshair();
}

void AnnotationCanvas::setActiveLabel(int labelId)
{
    activeLabel_ = labelId;
}

bool AnnotationCanvas::isPlacingTemplate() const
{
    return interaction_ == Interaction::PlaceTemplate;
}

void AnnotationCanvas::startTemplatePlacement(const CopiedTemplate& tmpl)
{
    startGroupPlacement({tmpl});
}

void AnnotationCanvas::startGroupPlacement(const QVector<CopiedTemplate>& items)
{
    clearPendingCreation();
    clearTemplatePlacement();
    for (const auto& item : items) {
        if (item.valid) {
            templates_.push_back(item);
        }
    }
    if (templates_.isEmpty()) {
        return;
    }
    activeLabel_ = templates_.first().labelId;
    interaction_ = Interaction::PlaceTemplate;
    setCursor(Qt::CrossCursor);

    const QPoint mousePos = mapFromGlobal(QCursor::pos());
    const QPointF scenePos = (viewport()->rect().contains(mousePos))
        ? mapToScene(mousePos)
        : mapToScene(viewport()->rect().center());
    updateTemplatePreview(scenePos);
}

void AnnotationCanvas::cancelTemplatePlacement()
{
    clearTemplatePlacement();
    interaction_ = Interaction::None;
    setCursor(tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    emit statusMessage(QStringLiteral("Placement cancelled"));
}

void AnnotationCanvas::clearTemplatePlacement()
{
    clearTemplatePreview();
    templates_.clear();
    if (interaction_ == Interaction::PlaceTemplate) {
        interaction_ = Interaction::None;
    }
}

void AnnotationCanvas::clearTemplatePreview()
{
    for (auto* item : templatePreviewItems_) {
        scene_->removeItem(item);
        delete item;
    }
    templatePreviewItems_.clear();
    if (templateBadgeBg_) {
        scene_->removeItem(templateBadgeBg_);
        delete templateBadgeBg_;
        templateBadgeBg_ = nullptr;
    }
    if (templateBadgeText_) {
        scene_->removeItem(templateBadgeText_);
        delete templateBadgeText_;
        templateBadgeText_ = nullptr;
    }
}

void AnnotationCanvas::updateTemplatePreview(const QPointF& center)
{
    if (interaction_ != Interaction::PlaceTemplate || templates_.isEmpty()) {
        return;
    }

    // Create one preview item per shape the first time, then only move them.
    if (templatePreviewItems_.size() != templates_.size()) {
        for (auto* item : templatePreviewItems_) {
            scene_->removeItem(item);
            delete item;
        }
        templatePreviewItems_.clear();
        for (const auto& t : templates_) {
            QPen pen(t.color, 2, Qt::DashLine);
            pen.setCosmetic(true);
            const QColor fill(t.color.red(), t.color.green(), t.color.blue(), 55);
            QGraphicsItem* item = nullptr;
            if (t.shapeType == QStringLiteral("rectangle") || t.shapeType == QStringLiteral("ellipse")) {
                item = scene_->addRect(QRectF(), pen, fill);
            } else {
                item = scene_->addPath(QPainterPath(), pen, fill);
            }
            item->setZValue(20000);
            templatePreviewItems_.push_back(item);
        }
    }

    QRectF bounds;
    for (int i = 0; i < templates_.size(); ++i) {
        const CopiedTemplate& t = templates_[i];
        const QPointF c = center + t.offset;
        const QRectF rect(c.x() - t.width / 2.0, c.y() - t.height / 2.0, t.width, t.height);
        bounds |= rect;

        QGraphicsItem* item = templatePreviewItems_[i];
        if (auto* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            rectItem->setRect(rect);
        } else if (auto* pathItem = qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QPainterPath path;
            if (!t.relativePoints.isEmpty()) {
                path.moveTo(c.x() + t.relativePoints[0], c.y() + t.relativePoints[1]);
                for (int p = 2; p + 1 < t.relativePoints.size(); p += 2) {
                    path.lineTo(c.x() + t.relativePoints[p], c.y() + t.relativePoints[p + 1]);
                }
                path.closeSubpath();
            }
            pathItem->setPath(path);
        }
    }

    const CopiedTemplate& first = templates_.first();
    const QString badgeStr = templates_.size() == 1
        ? QStringLiteral("%1 · %2 × %3 px")
              .arg(first.labelName)
              .arg(static_cast<int>(std::round(first.width)))
              .arg(static_cast<int>(std::round(first.height)))
        : QStringLiteral("%1 annotations").arg(templates_.size());

    if (!templateBadgeText_) {
        templateBadgeText_ = scene_->addSimpleText(badgeStr);
        templateBadgeText_->setZValue(20002);
        QFont f = templateBadgeText_->font();
        f.setPixelSize(11);
        f.setBold(true);
        templateBadgeText_->setFont(f);
        templateBadgeText_->setBrush(Qt::white);
    } else {
        templateBadgeText_->setText(badgeStr);
    }

    if (!templateBadgeBg_) {
        templateBadgeBg_ = scene_->addRect(QRectF());
        templateBadgeBg_->setZValue(20001);
        templateBadgeBg_->setBrush(QColor(15, 23, 42, 225));
        templateBadgeBg_->setPen(QPen(first.color, 1));
    }

    const QRectF tRect = templateBadgeText_->boundingRect();
    const double badgeW = tRect.width() + 12;
    const double badgeH = tRect.height() + 4;
    const double badgeX = bounds.center().x() - badgeW / 2.0;
    const double badgeY = bounds.bottom() + 6;
    templateBadgeBg_->setRect(badgeX, badgeY, badgeW, badgeH);
    templateBadgeText_->setPos(badgeX + 6, badgeY + 2);
}

void AnnotationCanvas::placeTemplateAt(const QPointF& pos)
{
    if (!model_ || templates_.isEmpty()) {
        clearTemplatePlacement();
        return;
    }

    // Build every shape before clearing the placement state.
    const QVector<CopiedTemplate> templates = templates_;
    QVector<cvat::Shape> shapes;
    for (const auto& t : templates) {
        const QPointF c = pos + t.offset;
        cvat::Shape shape;
        shape.labelId = t.labelId;
        shape.type = t.shapeType;
        shape.frame = model_->currentFrame();
        shape.attributes = t.attributes;
        shape.occluded = t.occluded;
        shape.rotation = t.rotation;
        shape.zOrder = model_->nextZOrder() + static_cast<int>(shapes.size());

        if (shape.type == QStringLiteral("rectangle") || shape.type == QStringLiteral("ellipse") ||
            shape.type == QStringLiteral("mask")) {
            const double left = c.x() - t.width / 2.0;
            const double top = c.y() - t.height / 2.0;
            shape.points = {left, top, left + t.width, top + t.height};
        } else {
            for (int i = 0; i + 1 < t.relativePoints.size(); i += 2) {
                shape.points.push_back(c.x() + t.relativePoints[i]);
                shape.points.push_back(c.y() + t.relativePoints[i + 1]);
            }
        }
        shapes.push_back(shape);
    }

    clearTemplatePlacement();
    // One call, so the whole placement is a single undo step.
    model_->addShapes(shapes);

    setTool(CanvasTool::Select);
    emit toolChanged(CanvasTool::Select);
    if (templates.size() == 1) {
        emit statusMessage(QStringLiteral("Placed %1 [%2 × %3 px]")
                               .arg(templates.first().labelName)
                               .arg(static_cast<int>(std::round(templates.first().width)))
                               .arg(static_cast<int>(std::round(templates.first().height))));
    } else {
        emit statusMessage(QStringLiteral("Placed %1 annotations").arg(templates.size()));
    }
    emit shapeCreated(model_->selection().isEmpty() ? QString() : *model_->selection().begin());
}

void AnnotationCanvas::refreshItems()
{
    for (auto* item : items_) {
        scene_->removeItem(item);
        delete item;
    }
    items_.clear();
    if (!model_) {
        return;
    }
    for (const auto& visible : model_->visibleShapes()) {
        QColor color(QStringLiteral("#4c9aff"));
        QString name;
        if (const Label* label = model_->label(visible.shape.labelId)) {
            color = QColor(label->color);
            name = label->name;
        }
        auto* item = new AnnotationItem(visible.key, visible.shape, color, name);
        item->setSelected(model_->selection().contains(visible.key));
        scene_->addItem(item);
        items_.push_back(item);
    }
}

void AnnotationCanvas::updateSelectionVisuals()
{
    if (!model_) {
        return;
    }
    const auto selected = model_->selection();
    for (auto* item : items_) {
        item->setSelected(selected.contains(item->key()));
        item->update();
    }
}

AnnotationItem* AnnotationCanvas::annotationAt(const QPointF& scenePosition) const
{
    const auto hits = scene_->items(scenePosition);
    for (auto* item : hits) {
        if (auto* annotation = dynamic_cast<AnnotationItem*>(item)) {
            return annotation;
        }
    }
    return nullptr;
}

cvat::Shape AnnotationCanvas::shapeAtScenePoint(const QPointF& point) const
{
    if (!model_) {
        return {};
    }
    for (const auto& visible : model_->visibleShapes()) {
        if (Geometry::hitTest(visible.shape, point.x(), point.y(), 2.0)) {
            return visible.shape;
        }
    }
    return {};
}

void AnnotationCanvas::updateRectanglePreview(const QPointF& start, const QPointF& end)
{
    const QRectF rect = QRectF(start, end).normalized();
    QColor color(255, 196, 0);
    if (model_) {
        if (const Label* lbl = model_->label(activeLabel_)) {
            color = QColor(lbl->color);
        }
    }
    if (!rectanglePreview_) {
        QPen pen(color, 2, Qt::DashLine);
        pen.setCosmetic(true);
        rectanglePreview_ = scene_->addRect(rect, pen,
                                            QColor(color.red(), color.green(), color.blue(), 35));
        rectanglePreview_->setZValue(10000);
    } else {
        rectanglePreview_->setRect(rect);
    }

    const QString dimText = QStringLiteral("%1 × %2 px")
        .arg(static_cast<int>(std::round(rect.width())))
        .arg(static_cast<int>(std::round(rect.height())));

    if (!rectangleBadgeText_) {
        rectangleBadgeText_ = scene_->addSimpleText(dimText);
        rectangleBadgeText_->setZValue(10002);
        QFont f = rectangleBadgeText_->font();
        f.setPixelSize(11);
        f.setBold(true);
        rectangleBadgeText_->setFont(f);
        rectangleBadgeText_->setBrush(Qt::white);
    } else {
        rectangleBadgeText_->setText(dimText);
    }

    if (!rectangleBadgeBg_) {
        rectangleBadgeBg_ = scene_->addRect(QRectF());
        rectangleBadgeBg_->setZValue(10001);
        rectangleBadgeBg_->setBrush(QColor(15, 23, 42, 220));
        rectangleBadgeBg_->setPen(QPen(color, 1));
    }

    const QRectF tRect = rectangleBadgeText_->boundingRect();
    const double badgeW = tRect.width() + 10;
    const double badgeH = tRect.height() + 4;
    const double badgeX = rect.center().x() - badgeW / 2.0;
    const double badgeY = rect.bottom() + 4;
    rectangleBadgeBg_->setRect(badgeX, badgeY, badgeW, badgeH);
    rectangleBadgeText_->setPos(badgeX + 5, badgeY + 2);

    emit statusMessage(QStringLiteral("Rectangle: %1 — click opposite corner to finish (Esc to cancel)").arg(dimText));
}

void AnnotationCanvas::createRectangle(const QPointF& start, const QPointF& end)
{
    const QRectF rect = QRectF(start, end).normalized();
    if (rect.width() < 3.0 || rect.height() < 3.0) {
        emit statusMessage(QStringLiteral("Rectangle is too small"));
        return;
    }
    if (!model_ || activeLabel_ <= 0) {
        emit statusMessage(QStringLiteral("Select a label before drawing"));
        return;
    }
    cvat::Shape shape;
    shape.labelId = activeLabel_;
    shape.type = QStringLiteral("rectangle");
    shape.points = {rect.left(), rect.top(), rect.right(), rect.bottom()};
    shape.frame = model_->currentFrame();
    shape.zOrder = model_->nextZOrder();
    model_->addShape(shape);
    emit statusMessage(QStringLiteral("Created rectangle: %1 × %2 px")
        .arg(static_cast<int>(std::round(rect.width())))
        .arg(static_cast<int>(std::round(rect.height()))));
    emit shapeCreated(model_->selection().isEmpty() ? QString() : *model_->selection().begin());
}

void AnnotationCanvas::updatePolygonPreview(const QPointF& cursor)
{
    if (pendingPoints_.isEmpty()) {
        return;
    }
    QPainterPath path;
    path.moveTo(pendingPoints_.first());
    for (qsizetype i = 1; i < pendingPoints_.size(); ++i) {
        path.lineTo(pendingPoints_[i]);
    }
    path.lineTo(cursor);
    if (!polygonPreview_) {
        QPen pen(QColor(255, 196, 0));
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        polygonPreview_ = scene_->addPath(path, pen);
        polygonPreview_->setZValue(10000);
    } else {
        polygonPreview_->setPath(path);
    }
}

void AnnotationCanvas::clearPendingCreation()
{
    pendingPoints_.clear();
    if (rectanglePreview_) {
        scene_->removeItem(rectanglePreview_);
        delete rectanglePreview_;
        rectanglePreview_ = nullptr;
    }
    if (rectangleBadgeBg_) {
        scene_->removeItem(rectangleBadgeBg_);
        delete rectangleBadgeBg_;
        rectangleBadgeBg_ = nullptr;
    }
    if (rectangleBadgeText_) {
        scene_->removeItem(rectangleBadgeText_);
        delete rectangleBadgeText_;
        rectangleBadgeText_ = nullptr;
    }
    if (polygonPreview_) {
        scene_->removeItem(polygonPreview_);
        delete polygonPreview_;
        polygonPreview_ = nullptr;
    }
}

void AnnotationCanvas::finishPolygon()
{
    if (pendingPoints_.size() < 2) {
        clearPendingCreation();
        return;
    }
    if (!model_ || activeLabel_ <= 0) {
        emit statusMessage(QStringLiteral("Select a label before drawing"));
        clearPendingCreation();
        return;
    }
    const bool polygon = tool_ == CanvasTool::Polygon;
    if (polygon && pendingPoints_.size() < 3) {
        emit statusMessage(QStringLiteral("A polygon needs at least three points"));
        clearPendingCreation();
        return;
    }
    cvat::Shape shape;
    shape.labelId = activeLabel_;
    shape.type = polygon ? QStringLiteral("polygon") : QStringLiteral("polyline");
    shape.frame = model_->currentFrame();
    for (const auto& point : pendingPoints_) {
        shape.points.push_back(point.x());
        shape.points.push_back(point.y());
    }
    clearPendingCreation();
    model_->addShape(shape);
}

void AnnotationCanvas::beginMoveOrResize(AnnotationItem* item, const QPointF& scenePosition,
                                          bool additive)
{
    const QString key = item->key();
    const bool alreadySelected = model_->selection().contains(key);
    if (additive) {
        model_->toggleSelection(key);
        if (!model_->selection().contains(key)) {
            return;
        }
    } else if (!alreadySelected) {
        model_->selectOnly(key);
    }
    updateSelectionVisuals();

    dragItem_ = item;
    dragShape_ = item->annotationShape();
    dragOriginal_ = dragShape_;
    dragStart_ = scenePosition;
    dragHandle_ = item->handleAt(item->mapFromScene(scenePosition),
                                 10.0 / std::max(0.05, zoom()));
    interaction_ = dragHandle_ >= 0 ? Interaction::Resize : Interaction::Move;
    setCursor(dragHandle_ >= 0 ? Qt::SizeFDiagCursor : Qt::ClosedHandCursor);
}

void AnnotationCanvas::applyMove(const QPointF& scenePosition)
{
    if (!dragItem_) {
        return;
    }
    const QPointF delta = scenePosition - dragStart_;
    dragShape_ = dragOriginal_;
    Geometry::translate(dragShape_.points, delta.x(), delta.y());
    dragItem_->setShape(dragShape_);
}

void AnnotationCanvas::applyResize(const QPointF& scenePosition)
{
    if (!dragItem_) {
        return;
    }
    dragShape_ = dragOriginal_;
    const QString type = dragShape_.type;
    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") ||
        type == QStringLiteral("mask")) {
        if (dragShape_.points.size() < 4) {
            return;
        }
        QRectF rect = Geometry::rectangle(dragShape_.points).toRectF();
        switch (dragHandle_) {
        case 0: rect.setTopLeft(scenePosition); break;
        case 1: rect.setTopRight(scenePosition); break;
        case 2: rect.setBottomRight(scenePosition); break;
        case 3: rect.setBottomLeft(scenePosition); break;
        default: break;
        }
        rect = rect.normalized();
        if (rect.width() < 1.0 || rect.height() < 1.0) {
            return;
        }
        dragShape_.points = {rect.left(), rect.top(), rect.right(), rect.bottom()};
    } else {
        const int index = dragHandle_ * 2;
        if (index + 1 < dragShape_.points.size()) {
            dragShape_.points[index] = scenePosition.x();
            dragShape_.points[index + 1] = scenePosition.y();
        }
    }
    dragItem_->setShape(dragShape_);
}

void AnnotationCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || (spacePanning_ && event->button() == Qt::LeftButton)) {
        interactionBeforePan_ = interaction_;
        interaction_ = Interaction::Pan;
        panLast_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    const QPointF scenePosition = mapToScene(event->position().toPoint());

    if (event->button() == Qt::RightButton) {
        if (interaction_ == Interaction::PlaceTemplate) {
            cancelTemplatePlacement();
            return;
        }
        if (interaction_ == Interaction::CreateRectangle) {
            clearPendingCreation();
            interaction_ = Interaction::None;
            setTool(CanvasTool::Select);
            emit toolChanged(CanvasTool::Select);
            setCursor(Qt::ArrowCursor);
            emit statusMessage(QStringLiteral("Rectangle creation cancelled"));
            return;
        }
        if (!pendingPoints_.isEmpty()) {
            finishPolygon();
            return;
        }

        AnnotationItem* item = annotationAt(scenePosition);
        if (item) {
            if (model_ && !model_->selection().contains(item->key())) {
                model_->selectOnly(item->key());
            }
            emit annotationContextMenuRequested(item->key(), event->globalPosition().toPoint());
            event->accept();
            return;
        }
        return;
    }

    if (event->button() != Qt::LeftButton || !model_) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (interaction_ == Interaction::PlaceTemplate) {
        placeTemplateAt(scenePosition);
        return;
    }

    if (tool_ == CanvasTool::Select) {
        AnnotationItem* item = annotationAt(scenePosition);
        if (item) {
            beginMoveOrResize(item, scenePosition, event->modifiers() & Qt::ControlModifier);
        } else {
            if (!(event->modifiers() & Qt::ControlModifier)) {
                model_->clearSelection();
            }
            interactionBeforePan_ = Interaction::None; // background-click pan: no prior interaction to restore
            interaction_ = Interaction::Pan;
            panLast_ = event->position();
            setCursor(Qt::ClosedHandCursor);
        }
        return;
    }

    if (activeLabel_ <= 0) {
        emit statusMessage(QStringLiteral("Select a label before drawing"));
        return;
    }

    if (tool_ == CanvasTool::Rectangle) {
        if (interaction_ == Interaction::CreateRectangle) {
            const QPointF start = creationStart_;
            interaction_ = Interaction::None;
            clearPendingCreation();
            createRectangle(start, scenePosition);
            setTool(CanvasTool::Select);
            emit toolChanged(CanvasTool::Select);
            return;
        }
        interaction_ = Interaction::CreateRectangle;
        creationStart_ = scenePosition;
        updateRectanglePreview(scenePosition, scenePosition);
        emit statusMessage(QStringLiteral("Click opposite corner to complete rectangle (Esc to cancel)"));
        return;
    }

    if (tool_ == CanvasTool::Points) {
        cvat::Shape shape;
        shape.labelId = activeLabel_;
        shape.type = QStringLiteral("points");
        shape.points = {scenePosition.x(), scenePosition.y()};
        shape.frame = model_->currentFrame();
        shape.zOrder = model_->nextZOrder();
        model_->addShape(shape);
        return;
    }

    // Polygon and polyline: collect vertices until double-click/Enter/right-click.
    pendingPoints_.push_back(scenePosition);
    updatePolygonPreview(scenePosition);
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent* event)
{
    lastCursor_ = event->position().toPoint();
    cursorInside_ = true;
    updateCrosshair();

    const QPointF scenePosition = mapToScene(event->position().toPoint());

    switch (interaction_) {
    case Interaction::Pan: {
        const QPointF delta = event->position() - panLast_;
        panLast_ = event->position();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                        static_cast<int>(delta.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                      static_cast<int>(delta.y()));
        return;
    }
    case Interaction::Move:
        applyMove(scenePosition);
        return;
    case Interaction::Resize:
        applyResize(scenePosition);
        return;
    case Interaction::CreateRectangle:
        updateRectanglePreview(creationStart_, scenePosition);
        return;
    case Interaction::PlaceTemplate:
        updateTemplatePreview(scenePosition);
        return;
    case Interaction::None:
        break;
    }

    if (tool_ == CanvasTool::Polygon || tool_ == CanvasTool::Polyline) {
        if (!pendingPoints_.isEmpty()) {
            updatePolygonPreview(scenePosition);
        }
    }

    if (tool_ == CanvasTool::Select) {
        AnnotationItem* item = annotationAt(scenePosition);
        // Hover only changes the cursor. Selection is changed by clicks, so
        // moving the pointer never drops a multi-selection.
        if (!item) {
            setCursor(spacePanning_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
            return;
        }
        const bool selected = model_ && model_->selection().contains(item->key());
        const int handle = item->handleAt(item->mapFromScene(scenePosition),
                                          10.0 / std::max(0.05, zoom()));
        if (selected && handle >= 0) {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (interaction_ == Interaction::Pan) {
        if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
            // Restore the interaction that was active before panning started
            interaction_ = interactionBeforePan_;
            interactionBeforePan_ = Interaction::None;
            if (interaction_ == Interaction::PlaceTemplate || interaction_ == Interaction::CreateRectangle) {
                setCursor(Qt::CrossCursor);
            } else {
                setCursor(spacePanning_ ? Qt::OpenHandCursor
                                        : (tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor));
            }
            return;
        }
    }

    if (interaction_ == Interaction::CreateRectangle) {
        // Dragging is removed: mouse release NEVER creates a rectangle.
        // It strictly requires a second click.
        return;
    }

    if (interaction_ == Interaction::Move || interaction_ == Interaction::Resize) {
        AnnotationItem* item = dragItem_;
        const bool changed = dragShape_.points != dragOriginal_.points ||
            dragShape_.rotation != dragOriginal_.rotation;
        interaction_ = Interaction::None;
        dragItem_ = nullptr;
        dragHandle_ = -1;
        setCursor(tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        if (item && changed) {
            const QString key = item->key();
            model_->updateVisibleShape(key, dragShape_);
        }
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void AnnotationCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!pendingPoints_.isEmpty()) {
        finishPolygon();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void AnnotationCanvas::wheelEvent(QWheelEvent* event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.18 : 1.0 / 1.18;
    zoomAt(event->position().toPoint(), factor);
    event->accept();
}

void AnnotationCanvas::zoomAt(const QPoint& viewportPosition, double factor)
{
    const double current = zoom();
    const double next = Geometry::clamp(current * factor, 0.03, 40.0);
    if (std::abs(next - current) < 1e-9) {
        return;
    }

    // Keep the image point beneath the pointer stationary while changing the
    // view transform. This makes zooming useful on large technical drawings.
    const QPointF sceneBefore = mapToScene(viewportPosition);
    scale(next / current, next / current);
    const QPointF sceneAfter = mapToScene(viewportPosition);
    const QPointF correction = sceneAfter - sceneBefore;
    translate(correction.x(), correction.y());
    emit zoomChanged(zoom());
}

void AnnotationCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacePanning_ = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        if (interaction_ == Interaction::PlaceTemplate) {
            cancelTemplatePlacement();
            return;
        }
        clearPendingCreation();
        interaction_ = Interaction::None;
        setTool(CanvasTool::Select);
        emit toolChanged(CanvasTool::Select);
        setCursor(Qt::ArrowCursor);
        emit statusMessage(QStringLiteral("Ready"));
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!pendingPoints_.isEmpty()) {
            finishPolygon();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void AnnotationCanvas::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacePanning_ = false;
        if (interaction_ == Interaction::Pan) {
            // Restore the interaction active before Spacebar pan
            interaction_ = interactionBeforePan_;
            interactionBeforePan_ = Interaction::None;
        }
        if (interaction_ == Interaction::PlaceTemplate || interaction_ == Interaction::CreateRectangle) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        }
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void AnnotationCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const QPointF scenePosition = mapToScene(event->pos());
    AnnotationItem* item = annotationAt(scenePosition);
    if (item) {
        if (model_ && !model_->selection().contains(item->key())) {
            model_->selectOnly(item->key());
        }
        emit annotationContextMenuRequested(item->key(), event->globalPos());
        event->accept();
        return;
    }
    QGraphicsView::contextMenuEvent(event);
}

void AnnotationCanvas::fitToView()
{
    const QRectF rect = background_ ? background_->boundingRect() : scene_->sceneRect();
    if (!rect.isEmpty()) {
        fitInView(rect, Qt::KeepAspectRatio);
        emit zoomChanged(zoom());
    }
}

void AnnotationCanvas::zoomIn()
{
    scale(1.2, 1.2);
    emit zoomChanged(zoom());
}

void AnnotationCanvas::zoomOut()
{
    scale(1.0 / 1.2, 1.0 / 1.2);
    emit zoomChanged(zoom());
}

void AnnotationCanvas::resetZoom()
{
    resetTransform();
    emit zoomChanged(zoom());
}

double AnnotationCanvas::zoom() const
{
    return transform().m11();
}

void AnnotationCanvas::setCrosshairEnabled(bool enabled)
{
    crosshairEnabled_ = enabled;
    updateCrosshair();
}

void AnnotationCanvas::setGuideColor(const QColor& color)
{
    QPen pen(color);
    pen.setWidth(1);
    pen.setCosmetic(true);
    crosshairH_->setPen(pen);
    crosshairV_->setPen(pen);
}

void AnnotationCanvas::enterEvent(QEnterEvent* event)
{
    cursorInside_ = true;
    QGraphicsView::enterEvent(event);
}

void AnnotationCanvas::leaveEvent(QEvent* event)
{
    cursorInside_ = false;
    updateCrosshair();
    QGraphicsView::leaveEvent(event);
}

void AnnotationCanvas::updateCrosshair()
{
    // Guides only help while placing geometry, so hide them in Select mode.
    const bool drawing = tool_ != CanvasTool::Select || interaction_ == Interaction::PlaceTemplate;
    const bool visible = crosshairEnabled_ && cursorInside_ && drawing;
    crosshairH_->setVisible(visible);
    crosshairV_->setVisible(visible);
    if (!visible) {
        return;
    }

    // Span the visible part of the scene so the guides reach the window edges
    // at any zoom or scroll position.
    const QPointF cursor = mapToScene(lastCursor_);
    const QRectF area = mapToScene(viewport()->rect()).boundingRect();
    crosshairH_->setLine(area.left(), cursor.y(), area.right(), cursor.y());
    crosshairV_->setLine(cursor.x(), area.top(), cursor.x(), area.bottom());
}

} // namespace cvat
