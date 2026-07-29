/*
  Q Light Controller Plus - Unit test
  endhandle_test.cpp
*/

#include <QtTest>
#include <QScrollBar>
#include <QSignalSpy>
#include <QMouseEvent>

#include "endhandle_test.h"
#include "multitrackview.h"
#include "headeritems.h"   // HEADER_HEIGHT, MARKER_LANE_HEIGHT, TRACKS_TOP
#include "trackitem.h"     // TRACK_WIDTH
#include "show.h"          // ShowMarker

// Deliver a synthetic mouse event straight to the view's viewport, mirroring
// what the windowing system would post (offscreen has no real cursor). The
// view maps event->position() through mapToScene(), so the local position is
// what matters.
static void sendMouse(QWidget *vp, QEvent::Type type, const QPoint &local,
                      Qt::MouseButton button)
{
    QMouseEvent ev(type, QPointF(local), QPointF(vp->mapToGlobal(local)),
                   button, button, Qt::NoModifier);
    QApplication::sendEvent(vp, &ev);
}

// Build a view with an explicit length, sized and scrolled so the end handle
// is off-screen to the LEFT and only its edge chip is visible.
static void setupOffscreenEnd(MultiTrackView &v, quint32 lenMs)
{
    v.setEditable(true);
    v.resize(1000, 400);
    v.setConfiguredLength(lenMs);
    v.show();
    QApplication::processEvents();
    // Scroll right so the handle's time is well left of the viewport. The
    // horizontal scrollbar is in scene pixels (identity transform).
    const int target = int(v.getPositionFromTime(lenMs + 40000));
    v.horizontalScrollBar()->setValue(target);
    v.verticalScrollBar()->setValue(0);  // keep the ruler + marker lane in view
    QApplication::processEvents();
}

// The edge chip is pinned just right of the frozen header column, i.e. at
// viewport x in [TRACK_WIDTH, TRACK_WIDTH+96], vertically in the marker lane.
static QPoint edgeChipPoint()
{
    return QPoint(TRACK_WIDTH + 30, HEADER_HEIGHT + MARKER_LANE_HEIGHT / 2);
}

void EndHandle_Test::offscreenEdgeDragChangesLength()
{
    MultiTrackView v;
    setupOffscreenEnd(v, 20000);

    QSignalSpy spy(&v, SIGNAL(showLengthChangeRequested(quint32)));
    QWidget *vp = v.viewport();
    const QPoint p = edgeChipPoint();

    sendMouse(vp, QEvent::MouseButtonPress, p, Qt::LeftButton);
    // Drag the pinned chip to the right in a few steps.
    for (int dx = 40; dx <= 240; dx += 40)
        sendMouse(vp, QEvent::MouseMove, p + QPoint(dx, 0), Qt::LeftButton);
    sendMouse(vp, QEvent::MouseButtonRelease, p + QPoint(240, 0), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    const quint32 newLen = spy.takeFirst().at(0).toUInt();
    // Dragging the chip RIGHT must lengthen the show (relative to 20s), and the
    // pinned chip must NOT snap the length to the viewport edge (~60s).
    QVERIFY2(newLen > 20000, qPrintable(QString("len=%1").arg(newLen)));
    QVERIFY2(newLen < 45000, qPrintable(QString("len=%1 (snapped to edge?)").arg(newLen)));
}

void EndHandle_Test::offscreenEdgeClickJumpsNoLengthChange()
{
    MultiTrackView v;
    setupOffscreenEnd(v, 20000);

    QSignalSpy spy(&v, SIGNAL(showLengthChangeRequested(quint32)));
    QWidget *vp = v.viewport();
    const QPoint p = edgeChipPoint();
    const int scrollBefore = v.horizontalScrollBar()->value();

    // Press + release WITHOUT moving = a click: it should jump the view to the
    // (off-screen, left) handle and NOT commit a length change.
    sendMouse(vp, QEvent::MouseButtonPress, p, Qt::LeftButton);
    sendMouse(vp, QEvent::MouseButtonRelease, p, Qt::LeftButton);

    QCOMPARE(spy.count(), 0);
    QVERIFY2(v.horizontalScrollBar()->value() < scrollBefore,
             "click should have scrolled left toward the handle");
}

void EndHandle_Test::onscreenHandleStillDraggable()
{
    // Regression: with the handle ON-screen, grabbing its line and dragging
    // still sets the length (absolute position map), unaffected by the edge path.
    MultiTrackView v;
    v.setEditable(true);
    v.resize(1000, 400);
    v.setConfiguredLength(20000);
    v.show();
    QApplication::processEvents();
    v.horizontalScrollBar()->setValue(0);
    v.verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    QSignalSpy spy(&v, SIGNAL(showLengthChangeRequested(quint32)));
    QWidget *vp = v.viewport();
    // Viewport x of the 20s handle (identity transform, scrolled home).
    const int hx = int(v.getPositionFromTime(20000));
    const QPoint p(hx, HEADER_HEIGHT + MARKER_LANE_HEIGHT / 2);

    sendMouse(vp, QEvent::MouseButtonPress, p, Qt::LeftButton);
    sendMouse(vp, QEvent::MouseMove, p + QPoint(120, 0), Qt::LeftButton);
    sendMouse(vp, QEvent::MouseButtonRelease, p + QPoint(120, 0), Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QVERIFY2(spy.takeFirst().at(0).toUInt() > 20000, "drag right should lengthen");
}

void EndHandle_Test::onscreenHandleWinsOverMarkerBehind()
{
    // A full-width section marker sits behind the on-screen end handle. Pressing
    // the handle flag must grab the HANDLE (set length), not move the marker.
    MultiTrackView v;
    v.setEditable(true);
    v.resize(1000, 400);
    v.setConfiguredLength(20000);
    QMap<quint32, ShowMarker> markers;
    markers.insert(0, ShowMarker(40000, "Full Show"));  // spans across the handle
    v.setMarkers(markers);
    v.show();
    QApplication::processEvents();
    v.horizontalScrollBar()->setValue(0);
    v.verticalScrollBar()->setValue(0);
    QApplication::processEvents();

    QSignalSpy lenSpy(&v, SIGNAL(showLengthChangeRequested(quint32)));
    QSignalSpy markSpy(&v, SIGNAL(markerMovedRequested(quint32,quint32,quint32,QString,QColor)));
    QWidget *vp = v.viewport();
    // Press right on the handle flag, in the marker lane (where the marker is).
    const int hx = int(v.getPositionFromTime(20000));
    const QPoint p(hx, HEADER_HEIGHT + MARKER_LANE_HEIGHT / 2);

    sendMouse(vp, QEvent::MouseButtonPress, p, Qt::LeftButton);
    sendMouse(vp, QEvent::MouseMove, p + QPoint(100, 0), Qt::LeftButton);
    sendMouse(vp, QEvent::MouseButtonRelease, p + QPoint(100, 0), Qt::LeftButton);

    QCOMPARE(lenSpy.count(), 1);          // handle grabbed → length set
    QCOMPARE(markSpy.count(), 0);         // marker NOT moved
    QVERIFY2(lenSpy.takeFirst().at(0).toUInt() > 20000, "drag right should lengthen");
}

QTEST_MAIN(EndHandle_Test)
