/*
  Q Light Controller Plus - Unit test
  effectinstance_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTINSTANCE_TEST_H
#define EFFECTINSTANCE_TEST_H

#include <QObject>
#include <QList>

class Doc;
class FixtureGroup;
class QLCPalette;

class EffectInstance_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** An Effect look with no fixture/group targets must drive ZERO fixtures
     *  (safety: no run-on-the-whole-rig fallback). */
    void emptyLookDrivesNothing();

    /** Assigning a group targets exactly that group's fixtures. */
    void groupTargetDrivesItsFixtures();

    /** Assigning a single fixture targets only that fixture. */
    void singleFixtureTargetOnly();

private:
    Doc          *m_doc = nullptr;
    FixtureGroup *m_group = nullptr;
    QLCPalette   *m_palette = nullptr;
    QList<quint32> m_fixtureIds;
};

#endif
