/*
  Q Light Controller Plus
  structurestudioview.h

  Lighting Studio — structure canvas. An orthographic Top/Front/Side view of a
  single rigging STRUCTURE (a Stand + its booms/bars, a Tower + its shelves, or a
  Truss + its base plate) with the fixtures mounted on it drawn in place. Unlike
  StudioPlaneView (which works in a studio group's LOCAL frame), this projects
  WORLD coordinates, so it can show any placeable structure and everything on it.

  Projection (metres → in-plane a,b):
    - Top   : a = X (stage right +),  b = Y (upstage +, screen-down)
    - Front : a = X,                  b = Z (height, screen-up)
    - Side  : a = Y,                  b = Z (height, screen-up)

  Slice 1 is a read-only reference drawing; dragging/adding land in later slices.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef STRUCTURESTUDIOVIEW_H
#define STRUCTURESTUDIOVIEW_H

#include <QWidget>
#include <QVector3D>
#include <QList>
#include <QSet>

class Doc;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class StructureStudioView : public QWidget
{
    Q_OBJECT

public:
    enum Kind  { StandKind = 0, TowerKind = 1, TrussKind = 2,
                 PlatformKind = 3, PipeKind = 4 };
    enum Plane { Top = 0, Front = 1, Side = 2 };

    StructureStudioView(Doc *doc, Kind kind, quint32 id, QWidget *parent = nullptr);

    void setPlane(Plane p);
    Plane plane() const { return m_plane; }

    /** Re-gather the structure + its fixtures and repaint (after external edits). */
    void reload();

    /** Fixtures currently mounted on this structure (for the side tree). */
    QList<quint32> mountedFixtures() const;

    /** Ring-highlight a set of fixtures (driven by the tree selection). */
    void setHighlight(const QList<quint32> &ids);

    /** Assign @p ids to the CURRENT view's face (Top/Front/Side): set their
     *  studioMount and pin them to that face surface. Empty = all mounted. */
    void putOnFace(const QList<quint32> &ids);

    /** Lay @p ids out evenly on the current face (name order). Auto-orients: if
     *  they don't fit side-by-side across the width they STACK vertically,
     *  centred, top→bottom. Empty = all mounted. */
    void distributeOnFace(const QList<quint32> &ids);

    /** Inspector edits for one fixture (frame-group only). */
    void setFixtureFace(quint32 fid, int face);     ///< studioMount + re-pin
    void setFixtureAngle(quint32 fid, float deg);   ///< studioAngle (bar rotation)

signals:
    /** A fixture on the structure was double-clicked (id passed through). */
    void fixtureActivated(quint32 fid);
    /** A fixture on the structure was single-clicked (selection). */
    void fixtureSelected(quint32 fid);
    /** A drag of a fixture is about to change the doc (for undo snapshotting). */
    void editAboutToStart();
    /** A fixture was dragged to a new spot on the structure (rig prop changed). */
    void fixtureMoved(quint32 fid);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    QPointF project(const QVector3D &w) const;      ///< world → in-plane (a,b) metres
    QPointF w2s(const QVector3D &w) const;           ///< world → screen pixels
    QPointF screenToPlane(const QPointF &px) const;  ///< screen pixels → in-plane (a,b) metres
    /** Remap a dragged fixture's mount param from a screen point (edits pipe
     *  offset / tower U-V-shelf / truss offset in place). Returns true if moved. */
    bool dragFixtureTo(quint32 fid, const QPointF &px);
    void refit();                                    ///< scale/centre to fit everything
    void collectPoints(QList<QVector3D> &pts) const; ///< every point the fit should frame

    void drawGrid(QPainter &p) const;
    void drawStructure(QPainter &p) const;
    void drawPipe(QPainter &p, const class Pipe *pipe) const;
    void drawFixtures(QPainter &p) const;
    quint32 hitTestFixture(const QPointF &px) const;

    double fixtureLenM(quint32 fid) const;             ///< physical length (metres)
    QVector3D fixtureAxisLocal(const struct FixtureRigProps &rp) const; ///< unit long axis in the frame
    QVector3D fixtureEndA(quint32 fid) const;          ///< world end A of the bar
    QVector3D fixtureEndB(quint32 fid) const;          ///< world end B of the bar
    /** For the anchor platform: which local component the given face pins, and to
     *  what value. mount 0=Top(pin Z),1=Front(pin Y),2=Side(pin X). */
    void facePin(int mount, int &pinComp, double &pinVal) const;

    QList<const class Pipe *> standPipes() const;    ///< pipes on this stand (Kind==Stand)

private:
    Doc     *m_doc;
    Kind     m_kind;
    quint32  m_id;
    Plane    m_plane = Front;

    double   m_scale = 60.0;    ///< pixels per metre
    QPointF  m_originPx;        ///< where world (a=0,b=0) lands on screen
    bool     m_panning = false;
    QPointF  m_panLast;
    quint32  m_dragFid = 0;    ///< fixture being dragged (0 = none)
    bool     m_dragged = false;
    QSet<quint32> m_highlight;  ///< ring-highlighted fixtures (tree selection)
};

/** @} */

#endif // STRUCTURESTUDIOVIEW_H
