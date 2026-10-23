/*
  Q Light Controller Plus - Unit test
  showmanager_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>

#define private public
#define protected public
#include "showmanager.h"
#undef protected
#undef private

#include "showmanager_test.h"
#include "showfunction.h"
#include "track.h"
#include "scene.h"
#include "show.h"
#include "doc.h"

void ShowManager_Test::init()
{
    m_doc = new Doc(this);
    Scene *a = new Scene(m_doc);
    a->setName("A");
    m_doc->addFunction(a);
    m_sceneA = a->id();
    Scene *b = new Scene(m_doc);
    b->setName("B");
    m_doc->addFunction(b);
    m_sceneB = b->id();

    m_show = new Show(m_doc);
    m_show->setName("Timeline");
    m_doc->addFunction(m_show);
    Track *t = new Track();
    t->setName("Lights");
    m_show->addTrack(t);
    ShowFunction *sf = t->createShowFunction(m_sceneA);
    sf->setStartTime(1000);
    sf->setDuration(2000);

    m_manager = new ShowManager(nullptr, m_doc);
    m_manager->updateShowsCombo();
    QCOMPARE(m_manager->m_show, m_show);
    QVERIFY(m_manager->m_undoStack.isEmpty());
    QVERIFY(!m_manager->m_undoAction->isEnabled());
}

void ShowManager_Test::cleanup()
{
    delete m_manager;
    m_manager = nullptr;
    delete m_doc;
    m_doc = nullptr;
}

void ShowManager_Test::undoTakesBackADrop()
{
    Track *t = m_show->tracks().first();
    m_manager->slotFunctionDropped(m_sceneB, 5000, t);
    t = m_show->tracks().first();
    QCOMPARE(t->showFunctions().count(), 2);
    QVERIFY(m_manager->m_undoAction->isEnabled());

    m_manager->m_undoAction->trigger();
    QCOMPARE(m_show->tracks().count(), 1);
    t = m_show->tracks().first();
    QCOMPARE(t->name(), QStringLiteral("Lights"));
    QCOMPARE(t->showFunctions().count(), 1);
    ShowFunction *sf = t->showFunctions().first();
    QCOMPARE(sf->functionID(), m_sceneA);
    QCOMPARE(sf->startTime(), quint32(1000));
    QCOMPARE(sf->duration(), quint32(2000));
    QVERIFY(!m_manager->m_undoAction->isEnabled());      // nothing more to undo
    QVERIFY(m_doc->isModified());
}

void ShowManager_Test::undoPutsAMarkerBack()
{
    m_show->setMarker(1000, 3000, "Verse", QColor("#4a90d9"));
    m_show->setMarkerCueList(1000, m_sceneB);             // any function id is a link
    QCOMPARE(m_show->markerCueList(1000), m_sceneB);

    m_manager->slotMarkerRelabel(1000, "Chorus");
    QCOMPARE(m_show->markers().value(1000).label, QStringLiteral("Chorus"));

    m_manager->slotUndo();
    QVERIFY(m_show->markers().contains(1000));
    QCOMPARE(m_show->markers().value(1000).label, QStringLiteral("Verse"));
    QCOMPARE(m_show->markers().value(1000).end, quint32(3000));
    QCOMPARE(m_show->markerCueList(1000), m_sceneB);
}

void ShowManager_Test::undoKeepsEveryTracksSettings()
{
    Track *t = m_show->tracks().first();
    t->setHoldLast(true);
    t->setSoloSafe(true);
    t->setPriority(Track::Override);
    t->setIntensity(0.5);
    t->showFunctions().first()->setLocked(true);

    // Any edit, then Undo.
    m_manager->slotFunctionDropped(m_sceneB, 5000, t);
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 2);
    // A snapshot of a locked track comes back locked (and still holding
    // both items: the lock goes on after they are put back).
    m_show->tracks().first()->setLocked(true);
    m_manager->pushUndoSnapshot();
    m_manager->slotUndo();
    QVERIFY(m_show->tracks().first()->isLocked());
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 2);
    m_show->tracks().first()->setLocked(false);
    m_manager->slotUndo();

    t = m_show->tracks().first();
    QCOMPARE(t->showFunctions().count(), 1);
    QVERIFY(t->isHoldLast());
    QVERIFY(t->isSoloSafe());
    QCOMPARE(int(t->priority()), int(Track::Override));
    QCOMPARE(t->intensity(), 0.5);
    QVERIFY(t->showFunctions().first()->isLocked());
}

QTEST_MAIN(ShowManager_Test)
