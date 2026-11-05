#include "cvatnative/ui/AnnotationCanvas.h"

#include "cvatnative/annotation/Geometry.h"
#include "cvatnative/ui/AnnotationItems.h"

#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
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
    tool_ = tool;
    switch (tool_) {
    case CanvasTool::Select: setCursor(Qt::ArrowCursor); break;
    case CanvasTool::Rectangle: setCursor(Qt::CrossCursor); break;
    default: setCursor(Qt::CrossCursor); break;
    }
}

void AnnotationCanvas::setActiveLabel(int labelId)
{
    activeLabel_ = labelId;
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
    if (!rectanglePreview_) {
        QPen pen(QColor(255, 196, 0));
        pen.setCosmetic(true);
        pen.setStyle(Qt::DashLine);
        rectanglePreview_ = scene_->addRect(QRectF(start, end), pen,
                                            QColor(255, 196, 0, 30));
        rectanglePreview_->setZValue(10000);
    } else {
        rectanglePreview_->setRect(QRectF(start, end).normalized());
    }
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
    model_->addShape(shape);
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
    if (event->button() == Qt::MiddleButton) {
        interaction_ = Interaction::Pan;
        panLast_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    const QPointF scenePosition = mapToScene(event->position().toPoint());

    if (event->button() == Qt::RightButton) {
        if (!pendingPoints_.isEmpty()) {
            finishPolygon();
        }
        return;
    }

    if (event->button() != Qt::LeftButton || !model_) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (tool_ == CanvasTool::Select) {
        AnnotationItem* item = annotationAt(scenePosition);
        if (item) {
            beginMoveOrResize(item, scenePosition, event->modifiers() & Qt::ControlModifier);
        } else if (!(event->modifiers() & Qt::ControlModifier)) {
            model_->clearSelection();
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
            return;
        }
        interaction_ = Interaction::CreateRectangle;
        creationStart_ = scenePosition;
        updateRectanglePreview(scenePosition, scenePosition);
        return;
    }

    if (tool_ == CanvasTool::Points) {
        cvat::Shape shape;
        shape.labelId = activeLabel_;
        shape.type = QStringLiteral("points");
        shape.points = {scenePosition.x(), scenePosition.y()};
        shape.frame = model_->currentFrame();
        model_->addShape(shape);
        return;
    }

    // Polygon and polyline: collect vertices until double-click/Enter/right-click.
    pendingPoints_.push_back(scenePosition);
    updatePolygonPreview(scenePosition);
}

void AnnotationCanvas::mouseMoveEvent(QMouseEvent* event)
{
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
        if (!item) {
            setCursor(Qt::ArrowCursor);
            return;
        }
        const int handle = item->handleAt(item->mapFromScene(scenePosition),
                                          10.0 / std::max(0.05, zoom()));
        if (handle >= 0 && model_->selection().contains(item->key())) {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void AnnotationCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && interaction_ == Interaction::Pan) {
        interaction_ = Interaction::None;
        setCursor(tool_ == CanvasTool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        return;
    }

    if (interaction_ == Interaction::Move || interaction_ == Interaction::Resize) {
        AnnotationItem* item = dragItem_;
        const bool changed = dragShape_.points != dragOriginal_.points ||
            dragShape_.rotation != dragOriginal_.rotation;
        interaction_ = Interaction::None;
        dragItem_ = nullptr;
        dragHandle_ = -1;
        setCursor(Qt::ArrowCursor);
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
    if (event->key() == Qt::Key_Escape) {
        clearPendingCreation();
        interaction_ = Interaction::None;
        setCursor(Qt::ArrowCursor);
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

} // namespace cvat
