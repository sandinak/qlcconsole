/*
  Q Light Controller Plus
  fixtureeditorcheck_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREEDITORCHECK_TEST_H
#define FIXTUREEDITORCHECK_TEST_H

#include <QObject>

class FixtureEditorCheck_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aBrokenFileIsRepairedStepByStep();
    void anEditByHandIsCheckedAgain();
    void aFileThatLosesPartsCanBeOpenedAnyway();
    void theProblemsPanelCountsAndGoesToAnIssue();
};

#endif
