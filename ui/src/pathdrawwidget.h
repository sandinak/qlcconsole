/*
  Q Light Controller Plus
  pathdrawwidget.h

  XY canvas widget for drawing a movement path used by EffectScript "path"
  parameters. Users click to add control points, drag to move them, and
  right-click to delete them. The closed Catmull-Rom spline through the
  points is rendered in real time.

  Path data is serialized as a compact JSON array: [[x,y], [x,y], ...]
  with x and y in [0, 1] (0=left/top, 1=right/bottom).

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PATHDRAWWIDGET_H
#define PATHDRAWWIDGET_H

#include <QWidget>
#include <QList>
#include <QPointF>

class QPushButton;

/** @addtogroup ui_functions
 * @{
 */

class PathDrawWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PathDrawWidget(QWidget *parent = nullptr);

    void    setPath(const QString &jsonPath);
    QString pathJson() const;

    QSize sizeHint() const override { return QSize(220, 250); }
    QSize minimumSizeHint() const override { return QSize(160, 185); }

signals:
    void pathChanged(const QString &jsonPath);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    // The canvas fills everything above the button row
    QRect    canvasRect() const;
    QPointF  toWidget(const QPointF &norm) const;
    QPointF  toNorm(const QPointF &w) const;
    int      hitTest(const QPointF &widgetPos) const;
    QPainterPath buildSpline() const;
    void     emitChanged();
    void     reposition();

    static QList<QPointF> makeCircle();
    static QList<QPointF> makeFig8();

    static const int kHit     = 10;  // hit-test radius in pixels
    static const int kDotR    = 5;   // drawn control-point radius
    static const int kBtnH    = 26;  // toolbar strip height

    QList<QPointF> m_pts;
    int            m_drag  = -1;
    int            m_hover = -1;

    QPushButton *m_clearBtn;
    QPushButton *m_circleBtn;
    QPushButton *m_fig8Btn;
};

/** @} */

#endif // PATHDRAWWIDGET_H
