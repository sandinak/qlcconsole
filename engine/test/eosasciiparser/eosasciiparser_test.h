/*
  Q Light Controller Plus - qlcconsole
  eosasciiparser_test.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef EOSASCIIPARSER_TEST_H
#define EOSASCIIPARSER_TEST_H

#include <QObject>

class EosAsciiParser_Test : public QObject
{
    Q_OBJECT

private slots:
    void levels();
    void times();
    void channelLists();
    void header();
    void personalityAndPatch();
    void conventionalPatch();
    void groups();
    void cues();
    void cueParts();
    void palettesAndPresets();
    void subs();
    void patchOptionsAndSubModes();
    void unknownRecordsSkipped();
    void notAscii();
    void realExport();
};

#endif
