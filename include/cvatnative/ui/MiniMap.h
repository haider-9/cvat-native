#pragma once

#include <QPixmap>
#include <QRectF>
#include <QSizeF>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace cvat {

// Small overview of the whole frame with a rectangle for the part the canvas
// is showing. Clicking or dragging on it asks the canvas to pan there.
class MiniMap : public QWidget {
    Q_OBJECT
public:
    explicit MiniMap(QWidget* parent = nullptr);

    void setFrame(const QImage& frame);
    // visible is the visible area in image pixels.
    void setView(const QRectF& visible, const QSizeF& imageSize);

signals:
    void panRequested(const QPointF& imagePoint);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] QRectF fittedRect() const;
    [[nodiscard]] QPointF toImage(const QPointF& widgetPoint) const;

    QPixmap thumbnail_;
    QSizeF imageSize_;
    QRectF view_;
    bool dragging_ = false;
};

} // namespace cvat
