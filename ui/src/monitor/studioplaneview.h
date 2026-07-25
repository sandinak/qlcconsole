/*
  Q Light Controller Plus
  studioplaneview.h

  Copyright (c) Massimo Callegari

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

#ifndef STUDIOPLANEVIEW_H
#define STUDIOPLANEVIEW_H

#include <QWidget>
#include <QVector3D>
#include <QHash>

class Doc;

/** Fixture Studio — orthographic plane editor (FIXTURESTUDIO_DESIGN Phase 1,
 *  graphical half). Draws a studio group's members as draggable dots in one of
 *  three orthographic projections of the group's LOCAL frame:
 *   - Top   (X→right, Y→down)   : plan view
 *   - Front (X→right, Z→up)     : downstage elevation (a step's front face)
 *   - Side  (Y→right, Z→up)     : side elevation
 *  Dragging a dot edits the two in-plane components of that member's
 *  group-local offset (FixtureRigProps::groupLocal); the third is preserved.
 *  Purely a view over the document — it mutates via Doc and emits changed(). */
class StudioPlaneView : public QWidget
{
    Q_OBJECT

public:
    enum Plane { Top = 0, Front = 1, Side = 2 };

    StudioPlaneView(Doc *doc, quint32 groupId, QWidget *parent = nullptr);

    void setPlane(Plane p);
    Plane plane() const { return m_plane; }

    /** Re-read members + refit the view (call after external edits). */
    void reload();

signals:
    /** A member's local offset changed by a drag (live). */
    void memberMoved(quint32 fid);
    /** Any change worth repainting the map for. */
    void changed();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    QList<quint32> members() const;
    void refit();                              ///< recompute scale/offset to fit
    QPointF project(const QVector3D &local) const;   ///< local → in-plane (a,b) metres
    QVector3D unproject(const QPointF &ab, const QVector3D &prev) const; ///< (a,b) → local
    QPointF worldToScreen(const QPointF &ab) const;  ///< in-plane metres → pixels
    QPointF screenToWorld(const QPointF &px) const;  ///< pixels → in-plane metres
    quint32 hitTest(const QPointF &px) const;

private:
    Doc     *m_doc;
    quint32  m_groupId;
    Plane    m_plane = Top;

    double   m_scale = 60.0;   ///< pixels per metre
    QPointF  m_originPx;       ///< where local (0,0) lands on screen
    quint32  m_dragFid = 0;
};

#endif // STUDIOPLANEVIEW_H
