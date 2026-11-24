/*
  Q Light Controller Plus
  planmapwidget.h

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

#ifndef PLANMAPWIDGET_H
#define PLANMAPWIDGET_H

#include <QWidget>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "../qlcui_export.h"

class MonitorProperties;

/** A small top-down stage map for Fixture Properties: the stage outline,
 *  the centre line (CL), the plaster line, the plot origin and the fixture
 *  as a dot, with dimension lines "10' 1/2" from CL" and "12' 1/4" from
 *  plaster line". It shows which numbers the X / Y fields are: the dot's
 *  distance from the plot origin, while the dimension lines read from the
 *  stage's own references (what "On stage" says).
 *
 *  All lengths are millimetres and every one that is shown goes through
 *  MeasurementUnits::formatLength. The stage rectangle, centre line and
 *  plaster line are WORLD positions (the plot's, +Y downstage); position()
 *  is what the X / Y fields hold, from the plot origin. Setters never emit;
 *  only a drag does. */
class QLC_UI_EXPORT PlanMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanMapWidget(QWidget *parent = nullptr);

    /** Stage rectangle, centre line x and plaster line y, world mm. */
    void setStage(const QRectF &stageMm, double centreLineXmm, double plasterLineYmm);
    /** Everything above, the plot origin and the grid step, from the show. */
    void setFromProperties(const MonitorProperties *props);
    /** The plot origin in world mm (X / Y are counted from here). */
    void setOrigin(const QPointF &worldMm);
    /** The fixture's X / Y from the plot origin, mm. */
    void setPosition(double xMm, double yMm);
    /** The snap step in mm for a drag (Shift drags free). 0 = no snapping. */
    void setGridStep(double mm);
    /** Dragging sets X / Y; off, @a reason is shown (and is the tooltip). */
    void setEditable(bool editable, const QString &reason = QString());

    QPointF position() const { return m_pos; }
    bool isEditable() const { return m_editable; }
    QString reason() const { return m_reason; }
    /** The dimension labels as drawn. */
    QString centreLineText() const;
    QString plasterLineText() const;
    /** The line under the map: the reason when locked, else what X / Y are. */
    QString captionText() const;
    /** The dot in widget pixels. */
    QPointF dotPoint() const;
    QSize sizeHint() const override { return QSize(260, 160); }

signals:
    /** Dragged: the new X / Y from the plot origin, mm. */
    void positionChanged(double xMm, double yMm);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    QRectF mapRect() const;
    QRectF viewWorld() const;           // world mm shown: the stage plus the dot
    QPointF toPixels(const QPointF &worldMm) const;
    QPointF toWorld(const QPointF &px) const;
    void dragTo(const QPointF &px, bool free);
    QPointF worldPos() const { return m_origin + m_pos; }

    QRectF m_stage;
    double m_clX = 0, m_plasterY = 0;
    QPointF m_origin;
    QPointF m_pos;
    double m_grid = 0;
    bool m_editable = true;
    QString m_reason;
    bool m_dragging = false;
};

#endif
