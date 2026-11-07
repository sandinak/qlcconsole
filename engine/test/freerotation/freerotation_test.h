/*
  Q Light Controller Plus - Unit test
  freerotation_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FREEROTATION_TEST_H
#define FREEROTATION_TEST_H

#include <QObject>
#include <QList>

class QLCFixtureDef;
class Doc;

class FreeRotation_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void aBandOnThePanChannelSpinsAndStopsByPointing();
    void aChannelOfItsOwnFastToSlowOneWaySlowToFastTheOther();
    void rangesSaidOnlyInWordsAreReadFromTheWords();
    void presetsThatGiveOneWayAreReadFromTheWords();
    void aHeadWithItsSpinOutsideItsHeadsUsesTheLooseChannel();
    void aSpinChannelIsNeverTheHeadsPointingChannel();
    void gobosAndPrismsThatRotateAreNotTheHeadTurning();
    void rpmInTheDefinitionIsRead();

    void freeSpinWritesEachFixturesOwnValues();
    void freeSpinSkipsHeadsThatCannotSpin();
    void freeSpinStopsAndReturnsToTheLook();
    void freeSpinBouncesAndAlternates();
    void freeSpinSpinsAreNeverMixedPartWay();

private:
    QLCFixtureDef *def(const QString &file);
    quint32 addFixture(const QString &file, const QString &mode, quint32 address);

    Doc *m_doc = nullptr;
    QList<QLCFixtureDef *> m_defs;
};

#endif
