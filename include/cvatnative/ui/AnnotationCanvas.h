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
class QGraphicsSimpleTextItem;

namespace cvat {

class AnnotationItem;

enum class CanvasTool { Select, Rectangle, Polygon, Polyline, Points };

struct CopiedTemplate {
    bool valid = false;
    int labelId = 0;
    QString labelName;
    QColor color;
    QString shapeType{QStringLiteral("rectangle")};
    double width = 0.0;
    double height = 0.0;
    QVector<double> relativePoints;
    QVector<AttributeValue> attributes;
    bool occluded = false;
    double rotation = 0.0;
    int zOrder = 0;
};

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

    void startTemplatePlacement(const CopiedTemplate& tmpl);
    void cancelTemplatePlacement();
    [[nodiscard]] bool isPlacingTemplate() const;

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
    void toolChanged(CanvasTool tool);
    void templatePlaced(const QString& key);
    void annotationContextMenuRequested(const QString& key, const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    enum class Interaction { None, Move, Resize, CreateRectangle, Pan, PlaceTemplate };

    void updateSelectionVisuals();
    void clearPendingCreation();
    void finishPolygon();
    void updatePolygonPreview(const QPointF& cursor);
    void createRectangle(const QPointF& start, const QPointF& end);
    void updateRectanglePreview(const QPointF& start, const QPointF& end);
    void updateTemplatePreview(const QPointF& center);
    void clearTemplatePreview();
    void clearTemplatePlacement();
    void placeTemplateAt(const QPointF& pos);
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
    QGraphicsRectItem* rectangleBadgeBg_ = nullptr;
    QGraphicsSimpleTextItem* rectangleBadgeText_ = nullptr;
    QGraphicsPathItem* polygonPreview_ = nullptr;
    CopiedTemplate templateData_;
    QGraphicsRectItem* templateRectPreview_ = nullptr;
    QGraphicsPathItem* templatePathPreview_ = nullptr;
    QGraphicsRectItem* templateBadgeBg_ = nullptr;
    QGraphicsSimpleTextItem* templateBadgeText_ = nullptr;
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
    bool spacePanning_ = false;
    Interaction interactionBeforePan_ = Interaction::None;
};

} // namespace cvat
