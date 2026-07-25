/*
  Q Light Controller Plus
  riserfaceeditor.h

  A dialog for laying fixtures out on a riser (stage platform) face. Shows the
  chosen face (front or top) to scale, with a draggable marker per mounted
  fixture. Add fixtures from the rig, drag them into place (or distribute them
  evenly across the width), and on OK each fixture's FixtureRigProps riser mount
  (platform id + face + U/V position in metres) is written — its world position
  is then derived from the riser, so it follows the riser's moves/height.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef RISERFACEEDITOR_H
#define RISERFACEEDITOR_H

#include <QDialog>
#include <QHash>

class QGraphicsScene;
class QGraphicsView;
class QComboBox;
class QListWidget;
class Doc;
class StagePlatform;
class MonitorFixtureItem;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class RiserFaceEditor final : public QDialog
{
    Q_OBJECT

public:
    RiserFaceEditor(Doc *doc, StagePlatform *platform, QWidget *parent = nullptr);

    /** Apply the layout to the fixtures' rig props (called on OK). */
    void applyToRig();

protected:
    /** Re-fit the face to fill the view whenever the dialog resizes/shows. */
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;

private slots:
    void slotFaceChanged(int index);
    void slotAddFixture();
    void slotRemoveSelected();
    void slotDistribute();

private:
    /** Face dimensions in metres for the current face (width x, height/depth y). */
    void faceSize(qreal &fw, qreal &fh) const;

    /** Rebuild the scene: face rectangle + a marker for each mounted fixture. */
    void rebuildScene();

    /** Add a draggable marker for fixture @p fid at metres (u,v) on the face. */
    void addMarker(quint32 fid, qreal u, qreal v);

    /** Physical size of a fixture in METRES (from its mode; long/thin fallback
     *  for linear fixtures with no declared physical size). */
    QSizeF fixtureSizeM(quint32 fid) const;

    /** Fit the face rectangle to fill the view, preserving aspect. */
    void fitFace();

    /** Clamp every marker inside the face and read its (u,v) back. */
    void readMarkers(QHash<quint32, QPointF> &out) const;

    Doc           *m_doc;
    StagePlatform *m_platform;
    int            m_face;         ///< FixtureRigProps::RiserFront / RiserTop

    QGraphicsScene *m_scene;
    QGraphicsView  *m_view;
    QComboBox      *m_faceCombo;
    QComboBox      *m_addCombo;

    /** Scene units per metre (fixed and fine, so even a 25 mm strip is many
     *  scene units; the view fit-scales for display, keeping face and fixtures
     *  at their TRUE relative size — no per-item minimum needed). */
    qreal m_ppm = 1000.0;

    /** The real 2D-monitor fixture rendering, reused as a draggable marker so it
     *  matches the canvas (heads, proportions), keyed by fixture id. */
    QHash<quint32, MonitorFixtureItem *> m_markers;
    /** Each marker's scene size (px) so we can read its centre back to (u,v). */
    QHash<quint32, QSizeF> m_markerSize;
};

/** @} */

#endif // RISERFACEEDITOR_H
