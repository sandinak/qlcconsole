/*
  Q Light Controller Plus
  mountinsituview.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef MOUNTINSITUVIEW_H
#define MOUNTINSITUVIEW_H

#include <QWidget>
#include "../qlcui_export.h"

class Doc;
class QLabel;
class StructureStudioView;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

/** What a fixture hangs on, seen front-on with the fixture ringed: the
 *  structure (truss, pipe, tower, platform) drawn by a StructureStudioView
 *  locked to its Front elevation, a caption saying face, hung or upright,
 *  placement and angle/tilt, and a height-offset dimension line. Look only.
 *  A fixture on no structure says so in one line. Fixture Properties calls
 *  refresh() on every live change. */
class QLC_UI_EXPORT MountInSituView : public QWidget
{
    Q_OBJECT

public:
    MountInSituView(Doc *doc, quint32 fixtureId, QWidget *parent = nullptr);

    quint32 fixtureId() const { return m_fid; }
    /** Follow another fixture (the dialog's selection changed). */
    void setFixtureId(quint32 fixtureId);

    /** Re-read the fixture's rig props and redraw. */
    void refresh();

    /** True when the fixture is on a structure (else the "free on the plot" line). */
    bool isMounted() const { return m_view != nullptr; }
    /** The embedded view (null when the fixture is not on a structure). */
    StructureStudioView *view() const { return m_view; }
    /** Caption under the picture (or the unmounted line). */
    QString caption() const;
    /** Height-offset label, e.g. "2' 3\" above the truss"; empty when unmounted. */
    QString heightText() const;

    QSize sizeHint() const override { return QSize(260, 160); }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    class DimensionOverlay;

    void clearView();

    Doc *m_doc;
    quint32 m_fid;
    int m_kind = -1;
    quint32 m_structId = 0;
    StructureStudioView *m_view = nullptr;
    DimensionOverlay *m_dim = nullptr;
    QLabel *m_caption;
    QString m_heightText;
};

/** @} */

#endif
