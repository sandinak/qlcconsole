/*
  Q Light Controller Plus
  mountview_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef MOUNTVIEW_TEST_H
#define MOUNTVIEW_TEST_H

#include <QObject>

/* MountInSituView: the structure a fixture hangs on, front-on, the fixture
   ringed, with its height offset on a dimension line. */
class MountView_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void aFixtureOnATrussShowsTheTruss();
    void aFixtureOnAPipeShowsThePipe();
    void aFixtureOnATowerShowsTheTower();
    void anUnmountedFixtureSaysSo();
    void refreshFollowsTheHeightOffsetInFeetAndMetres();
    void refreshMovesBetweenMountedAndFree();
    void opensQuickly();
    void picturesForReview();
};

#endif
