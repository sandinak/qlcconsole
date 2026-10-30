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
#include "multitrackview.h"
#include "sequenceitem.h"
#undef protected
#undef private

#include "showmanager_test.h"
#include "showfunction.h"
#include "track.h"
#include "scene.h"
#include "sequence.h"
#include <QComboBox>
#include <QGraphicsSceneMouseEvent>
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

/* CHANGELOG 0.2.2 Fixed, "Shows timeline: Undo keeps each track's
   settings": any Undo used to reset Solo on every track. */
void ShowManager_Test::undoKeepsATracksSolo()
{
    Track *other = new Track();
    other->setName("Sound");
    QVERIFY(m_show->addTrack(other));
    other->createShowFunction(m_sceneB)->setDuration(1000);
    Track *t = m_show->tracks().first();
    t->setSolo(true);
    QVERIFY(!other->isSolo());

    m_manager->slotFunctionDropped(m_sceneB, 5000, t);
    m_manager->slotUndo();

    QCOMPARE(m_show->tracks().count(), 2);
    t = m_show->tracks().first();
    QCOMPARE(t->name(), QStringLiteral("Lights"));
    QCOMPARE(t->showFunctions().count(), 1);
    QVERIFY(t->isSolo());
    QVERIFY(!m_show->tracks().last()->isSolo());
}

void ShowManager_Test::undoStartsFreshForAnotherShowOrDocument()
{
    Track *t = m_show->tracks().first();
    m_manager->slotFunctionDropped(m_sceneB, 5000, t);
    QVERIFY(m_manager->m_undoAction->isEnabled());

    Show *other = new Show(m_doc);
    other->setName("Zed");               // after "Timeline" in the combo
    m_doc->addFunction(other);
    other->addTrack(new Track());
    // As New Show does: pick its place in the combo, then refill it
    m_manager->m_selectedShowIndex = 1;
    m_manager->updateShowsCombo();
    QCOMPARE(m_manager->m_show, other);
    QVERIFY(m_manager->m_undoStack.isEmpty());
    QVERIFY(!m_manager->m_undoAction->isEnabled());

    m_manager->slotFunctionDropped(m_sceneA, 0, other->tracks().first());
    QVERIFY(m_manager->m_undoAction->isEnabled());
    m_manager->slotDocClearing();
    QVERIFY(m_manager->m_undoStack.isEmpty());
    QVERIFY(!m_manager->m_undoAction->isEnabled());
}

void ShowManager_Test::aDeletedFunctionLeavesNoItemBehind()
{
    QCOMPARE(m_manager->m_showview->m_items.count(), 1);
    QVERIFY(m_doc->deleteFunction(m_sceneA));
    QVERIFY(m_manager->m_showview->m_items.isEmpty());
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 0);

    // The next drop on that track (it pushes neighbours aside) is safe
    m_manager->slotFunctionDropped(m_sceneB, 0, m_show->tracks().first());
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 1);
}

void ShowManager_Test::renamingATrackRenamesItsAttribute()
{
    Track *t = m_show->tracks().first();
    const QString was = QString("Lights-%1").arg(t->id());
    QVERIFY(m_show->getAttributeIndex(was) >= 0);
    const int attrs = m_show->attributes().count();

    t->setName("Front");                         // as the in-place edit does
    emit m_manager->m_showview->trackRenamed(t);
    QCOMPARE(m_show->getAttributeIndex(was), -1);
    QVERIFY(m_show->getAttributeIndex(QString("Front-%1").arg(t->id())) >= 0);

    QVERIFY(m_show->removeTrack(t->id()));       // unregisters it by name
    QCOMPARE(m_show->attributes().count(), attrs - 1);
}

void ShowManager_Test::aClickOnAChaseBlockDoesntMoveIt()
{
    Sequence *seq = new Sequence(m_doc);
    seq->setBoundSceneID(m_sceneB);
    m_doc->addFunction(seq);
    seq->addStep(ChaserStep(m_sceneB, 0, 2000, 0));
    Track *t = new Track(m_sceneB);
    t->setName("Chase");
    m_show->addTrack(t);
    ShowFunction *sf = t->createShowFunction(seq->id());
    sf->setStartTime(1000);
    sf->setDuration(2000);
    m_manager->updateMultiTrackView();

    SequenceItem *item = nullptr;
    foreach (ShowItem *i, m_manager->m_showview->m_items)
        if (SequenceItem *s = qobject_cast<SequenceItem *>(i))
            item = s;
    QVERIFY(item != nullptr);
    m_doc->resetModified();

    const QPointF body(item->boundingRect().width() / 2, 30);   // the cue, below the title strip
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    press.setPos(body);
    press.setScenePos(item->mapToScene(body));
    item->mousePressEvent(&press);
    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    release.setButton(Qt::LeftButton);
    release.setPos(body);
    release.setScenePos(item->mapToScene(body));
    item->mouseReleaseEvent(&release);

    QCOMPARE(sf->startTime(), quint32(1000));
    QVERIFY(!m_doc->isModified());
}

QTEST_MAIN(ShowManager_Test)
