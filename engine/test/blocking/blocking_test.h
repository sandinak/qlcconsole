/*
  Q Light Controller Plus
  blocking_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef BLOCKING_TEST_H
#define BLOCKING_TEST_H

#include <QObject>

class Doc;

class Blocking_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aTargetStandsWhereItsBlockingSays();
    void aBlockingIsAnOffsetSoAVenueMovesIt();
    void aCueTracksItsBlockingForward();
    void aimDmxFollowsTheCuesBlocking();
    void areaAndLineShapesChangePerBlocking();
    void theFollowSpotFollowsTheBlockedPosition();
    void blockingSurvivesSaveAndLoad();
    void anOldShowLoadsWithDefaultAndPlaysTheSame();
    void showReviewSaysWhenABlockingIsOutOfReach();

private:
    Doc *m_doc = nullptr;
};

#endif
