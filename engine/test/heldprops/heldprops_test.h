/*
  Q Light Controller Plus
  heldprops_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HELDPROPS_TEST_H
#define HELDPROPS_TEST_H

#include <QObject>

class Doc;

class HeldProps_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void onlyStaffAndCandleAreHeld();
    void aHeldPropFollowsItsHolderInEveryBlocking();
    void cuesGiveTakeAndHandOnWithTracking();
    void defaultIsUnchanged();
    void holdsSurviveSaveAndLoad();
    void anOlderBuildSeesThePropAtItsPlace();
    void giveEachMakesOneEach();
    void anAimAtThePersonIgnoresTheProp();
    void candleStateFollowsSelect();
    void candleRemoteDefinitionMatchesTheChart();

private:
    Doc *m_doc = nullptr;
};

#endif
