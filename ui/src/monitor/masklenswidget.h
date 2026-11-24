/*
  Q Light Controller Plus
  masklenswidget.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef MASKLENSWIDGET_H
#define MASKLENSWIDGET_H

#include <QWidget>
#include <QPointF>

#include "nolightzone.h"

/**
 * The beam seen from behind the light: a circle sized for the lens angle (a
 * laser: its scan rectangle) with the taped bands -- LightMask's top, bottom,
 * left and right, in degrees -- shaded, and how much of the beam still gets
 * through. Drag a band's edge in from any side to set that side's mask.
 * The setters do not emit; maskChanged() is the person's drag only.
 */
class MaskLensWidget : public QWidget
{
    Q_OBJECT

public:
    enum Side { Top, Bottom, Left, Right };
    Q_ENUM(Side)

    explicit MaskLensWidget(QWidget *parent = nullptr);

    /** The lens' full beam angle, degrees; the beam is a circle of half that. */
    void setLensAngle(double degrees);
    /** A laser's scan window instead (full angles, degrees): a rectangle. */
    void setScanWindow(double hDeg, double vDeg);
    void setMask(float top, float bottom, float left, float right);
    void setMask(const LightMask &mask);

    LightMask mask() const { return m_mask; }
    /** Half the beam (or scan window) across / up-down, degrees. */
    double halfWidth() const { return m_halfW; }
    double halfHeight() const { return m_halfH; }
    /** Percent (0..100) of the beam that clears @a mask. */
    double percentThrough() const;
    static double percentThrough(double halfW, double halfH, bool rectangle, const LightMask &mask);

    /** Where @a side's edge sits now (widget pixels, on its midpoint). */
    QPointF edgePoint(Side side) const;

signals:
    void maskChanged(float top, float bottom, float left, float right);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    QPointF centre() const;
    double scale() const;       // pixels per degree
    double limit(Side side) const;
    int hitEdge(const QPointF &p) const;
    void dragTo(const QPointF &p);
    void refreshText();

    LightMask m_mask;
    double m_halfW = 15.0, m_halfH = 15.0;
    bool m_rect = false;
    int m_drag = -1;
};

#endif
