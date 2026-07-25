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
#include <QSet>

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

    /** The face index of the current view (0=Top, 1=Front, 2=Side). */
    int currentFace() const { return int(m_plane); }

    /** Auto-distribute @p ids on the CURRENT face: assign them to it, pin them
     *  to its surface, and lay them out (horizontal vs vertical decided from
     *  whether they fit side-by-side across the feature width; name order). If
     *  @p ids is empty, distributes the fixtures already on the current face
     *  (or all members if none are). */
    void distributeEvenly(const QList<quint32> &ids = QList<quint32>());

    /** Put @p ids on the current face WITHOUT relaying them out: set their mount
     *  to this face and pin them to its surface, keeping their in-plane spot. */
    void assignToFace(const QList<quint32> &ids);

    /** Highlight a set of fixtures (e.g. the left-list selection). */
    void setHighlight(const QList<quint32> &ids);

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
    void wheelEvent(QWheelEvent *) override;

private:
    QList<quint32> members() const;
    void refit();                              ///< recompute scale/offset to fit
    void drawStructure(QPainter &p) const;     ///< the anchor feature as reference
    void drawFixture(QPainter &p, quint32 fid) const;   ///< a fixture as an LED bar
    double fixtureLenM(quint32 fid) const;     ///< real physical length (metres)
    QVector3D fixtureAxis(quint32 fid) const;  ///< unit long-axis direction in the local frame
    void facePin(const QList<quint32> &ids, int &mount, int &pinComp, double &pinVal) const;
    class StagePlatform *anchorPlatform() const;   ///< platform this group is slaved to, or null
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
    bool     m_panning = false;
    QPointF  m_panLast;
    QSet<quint32> m_highlight;
};

#endif // STUDIOPLANEVIEW_H
