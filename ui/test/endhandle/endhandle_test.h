/*
  Q Light Controller Plus - Unit test
  endhandle_test.h

  Exercises the show END-handle interaction on MultiTrackView: when the end is
  scrolled off-screen, the pinned edge chip must be grabbable — dragging it
  changes the show length, a plain click jumps the view to the handle.
*/

#ifndef ENDHANDLE_TEST_H
#define ENDHANDLE_TEST_H

#include <QObject>

class EndHandle_Test : public QObject
{
    Q_OBJECT

private slots:
    void offscreenEdgeDragChangesLength();
    void offscreenEdgeClickJumpsNoLengthChange();
    void onscreenHandleStillDraggable();
    void onscreenHandleWinsOverMarkerBehind();
};

#endif
