/*
  Q Light Controller Plus
  fixturedefcheck_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREDEFCHECK_TEST_H
#define FIXTUREDEFCHECK_TEST_H

#include <QObject>

class FixtureDefCheck_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aBackslashTypedForASlashIsFoundAndRepaired();
    void aMismatchedClosingTagIsClosedRight();
    void aBareAmpersandIsEscaped();
    void aMissingDoctypeIsAdded();
    void aGoodFileHasNothingToDiagnose();
    void clearlyWrongDataIsAnError();
    void educatedGuessesAtTextMistakes_data();
    void educatedGuessesAtTextMistakes();
    void aModeNamingAMissingChannelIsCaughtAndGuessed();
    void unnamedAndDuplicateNamesAreFixed();
    void theBriefSaysItInAFewWords();
    void anEmptyStubBecomesAPlaceholder();
    void slashMistakesAreFixedWithoutAsking();
    void theWledLanternLoadsAsALantern();
    void missingDataIsAWarning();
    void onlyALightIsAskedForLightOutput();
};

#endif
