#pragma once

#include "cvatnative/annotation/AnnotationModel.h"

#include <QGraphicsView>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVector>

class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;

namespace cvat {

class AnnotationItem;

enum class CanvasTool { Select, Rectangle, Polygon, Polyline, Points };

// High-performance canvas: the frame is a QPixmap, annotations are lightweight
// custom items in image-pixel coordinates. No per-frame widget construction and
// no copies of the full annotation set are made while dragging.
class AnnotationCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit AnnotationCanvas(QWidget* parent = nullptr);

    void setModel(AnnotationModel* model);
    void setFrameImage(const QImage& image);
    void setTool(CanvasTool tool);
    [[nodiscard]] CanvasTool tool() const { return tool_; }
    void setActiveLabel(int labelId);
    [[nodiscard]] int activeLabel() const { return activeLabel_; }

    void refreshItems();
    void fitToView();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    [[nodiscard]] double zoom() const;

signals:
    void statusMessage(const QString& message);
    void zoomChanged(double factor);
    void shapeCreated(const QString& key);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Interaction { None, Move, Resize, CreateRectangle, Pan };

    void updateSelectionVisuals();
    void clearPendingCreation();
    void finishPolygon();
    void updatePolygonPreview(const QPointF& cursor);
    void createRectangle(const QPointF& start, const QPointF& end);
    void updateRectanglePreview(const QPointF& start, const QPointF& end);
    void beginMoveOrResize(AnnotationItem* item, const QPointF& scenePosition,
                           bool additive);
    void applyMove(const QPointF& scenePosition);
    void applyResize(const QPointF& scenePosition);
    void zoomAt(const QPoint& viewportPosition, double factor);
    [[nodiscard]] AnnotationItem* annotationAt(const QPointF& scenePosition) const;
    [[nodiscard]] cvat::Shape shapeAtScenePoint(const QPointF& point) const;

    AnnotationModel* model_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsPixmapItem* background_ = nullptr;
    QList<AnnotationItem*> items_;
    QGraphicsRectItem* rectanglePreview_ = nullptr;
    QGraphicsPathItem* polygonPreview_ = nullptr;
    QImage frame_;
    CanvasTool tool_ = CanvasTool::Select;
    Interaction interaction_ = Interaction::None;
    int activeLabel_ = 0;

    AnnotationItem* dragItem_ = nullptr;
    cvat::Shape dragShape_;
    cvat::Shape dragOriginal_;
    QPointF dragStart_;
    int dragHandle_ = -1;
    QPointF panLast_;
    QPointF creationStart_;
    QVector<QPointF> pendingPoints_;
    bool firstImage_ = true;
    bool adjusting_ = false;
};

} // namespace cvat
