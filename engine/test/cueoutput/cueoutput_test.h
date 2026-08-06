/*
  Q Light Controller Plus - Unit test
  cueoutput_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CUEOUTPUT_TEST_H
#define CUEOUTPUT_TEST_H

#include <QObject>

class Doc;

class CueOutput_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** A Scene's baked values split into intensity vs non-intensity per fixture. */
    void sceneSplitsIntensityFromRest();
    /** A dark fixture (no intensity set) reports lit=false. */
    void unlitFixtureReportsDark();
    /** A Collection unions its member scenes, later member winning a shared channel. */
    void collectionUnionsMembersLastWins();

private:
    Doc *m_doc = nullptr;
    quint32 m_moverId = 0;   //!< a fixture with Pan/Tilt + master dimmer + RGB
};

#endif // CUEOUTPUT_TEST_H
