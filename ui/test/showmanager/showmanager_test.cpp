/*
  Q Light Controller Plus - Unit test
  showmanager_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "showmanager.h"
#include "multitrackview.h"
#include "sequenceitem.h"
#include "trackitem.h"
#include "showtimelineeditor.h"
#include "showitem.h"
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
#include "showtreecontroller.h"
#include "chaser.h"
#include "genericfader.h"
#include "chaserstep.h"
#include "doc.h"
#include "outputrulekeeper.h"
#include "mastertimer.h"
#include "fixture.h"
#include "audio.h"
#include "efx.h"
#include <QBuffer>
#include <QDropEvent>
#include <QMimeData>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QLabel>
#include <QGraphicsProxyWidget>
#include <QMenu>
#include <QUrl>
#include "video.h"
#include "audioplugincache.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

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

void ShowManager_Test::aCueSnapsToAMarker()
{
    MultiTrackView *v = m_manager->m_showview;
    m_show->setMarker(10000, 20000, "Chorus", QColor("#4a90d9"));
    v->setMarkers(m_show->markers());
    const double msPerPx = double(v->getTimeFromPosition(TRACK_WIDTH + 200)) / 200.0;
    QVERIFY(msPerPx > 0);

    bool snapped = false;
    QCOMPARE(v->snapToMarker(10000 + quint32(3 * msPerPx), &snapped), quint32(10000));
    QVERIFY(snapped);
    QCOMPARE(v->snapToMarker(20000 - quint32(2 * msPerPx), &snapped), quint32(20000));   // its end too
    QVERIFY(snapped);
    const quint32 farAway = 10000 + quint32(60 * msPerPx);
    QCOMPARE(v->snapToMarker(farAway, &snapped), farAway);
    QVERIFY(!snapped);
}

void ShowManager_Test::drawsWithoutTheAudioFile()
{
    Audio *a = new Audio(m_doc);
    a->setName("Song");
    m_doc->addFunction(a);
    a->setSourceFileName("/no/such/dir/song.wav");
    Track *t = m_show->tracks().first();
    t->createShowFunction(a->id())->setStartTime(0);
    m_manager->m_showview->setMarkers(QMap<quint32, ShowMarker>{ { 0, ShowMarker(5000, "Intro", QColor("#4a90d9")) } });
    m_manager->updateMultiTrackView();
    QVERIFY(m_manager->m_showview->firstAudioItem() != nullptr);
    m_manager->resize(900, 400);
    const QImage img = m_manager->grab().toImage();
    QVERIFY(!img.isNull());
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

void ShowManager_Test::aGeneratedCueListKeepsItsStepsOnATrack()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring Show");
    const quint32 list = st->addCueList(prod, "Act 1");
    st->addCue(list, -1, "Preset");
    st->addCue(list, 0, "Lights up");
    st->addCue(list, 1, "Blackout");
    const quint32 cid = st->cueList(list)->chaser;
    QVERIFY(st->isGenerated(cid));
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(cid));
    QVERIFY(ch != nullptr);
    QVERIFY(ch->stepsCount() >= 2);

    Track *t = new Track(m_sceneB);
    t->setName("Chase");
    m_show->addTrack(t);
    ShowFunction *sf = t->createShowFunction(cid);
    sf->setStartTime(1000);
    sf->setDuration(4000);
    m_manager->updateMultiTrackView();

    SequenceItem *item = nullptr;
    foreach (ShowItem *i, m_manager->m_showview->m_items)
        if (SequenceItem *s = qobject_cast<SequenceItem *>(i))
            if (s->getChaser() == ch)
                item = s;
    QVERIFY(item != nullptr);
    QVERIFY(item->stepsLocked());

    // A snapshot of every step and the modes.
    const auto snap = [ch]() {
        QString out;
        for (int i = 0; i < ch->stepsCount(); i++)
        {
            const ChaserStep s = ch->steps().at(i);
            out += QString("%1:%2:%3:%4;").arg(s.fid).arg(s.duration).arg(s.fadeIn).arg(s.fadeOut);
        }
        return out + QString::number(ch->durationMode()) + QString::number(ch->fadeInMode());
    };
    const QString before = snap();
    const int undoDepth = m_manager->m_undoStack.size();

    const auto press = [item](const QPointF &at) {
        QGraphicsSceneMouseEvent e(QEvent::GraphicsSceneMousePress);
        e.setButton(Qt::LeftButton);
        e.setButtons(Qt::LeftButton);
        e.setPos(at);
        e.setScenePos(item->mapToScene(at));
        item->mousePressEvent(&e);
        QGraphicsSceneMouseEvent r(QEvent::GraphicsSceneMouseRelease);
        r.setButton(Qt::LeftButton);
        r.setPos(at);
        r.setScenePos(item->mapToScene(at));
        item->mouseReleaseEvent(&r);
    };
    const qreal w = item->boundingRect().width();

    // The fade band, a cue body, the right-edge stretch: each is refused,
    // names the cue list, and changes nothing.
    const QList<QPointF> hits = {QPointF(w / 3, TRACK_HEIGHT * 0.8), QPointF(w / 2, 30),
                                 QPointF(w - 1, 30)};
    for (const QPointF &at : hits)
    {
        press(at);
        QVERIFY2(item->lastRefusal().contains("Act 1"), qPrintable(item->lastRefusal()));
        QVERIFY(item->lastRefusal().contains("Show Tree"));
        QCOMPARE(snap(), before);
    }
    item->setDuration(9000, false);
    item->deleteSelectedCues();
    item->scaleSelectedCues(2.0);
    QCOMPARE(snap(), before);
    QCOMPARE(m_manager->m_undoStack.size(), undoDepth);

    // Moving the clip is still the user's: the title strip is the plain
    // block-move grab, and the clip's start follows.
    press(QPointF(w / 2, 5));
    item->setPos(item->pos() + QPointF(50, 0));
    sf->setStartTime(2000);
    QCOMPARE(sf->startTime(), quint32(2000));
    QCOMPARE(snap(), before);
}

void ShowManager_Test::theItemEditorGoesWithItsFunction()
{
    EFX *efx = new EFX(m_doc);
    efx->setName("Sweep");
    QVERIFY(m_doc->addFunction(efx));

    m_manager->resize(1000, 700);
    m_manager->show();
    m_manager->showRightEditor(efx);
    QPointer<QWidget> ed = m_manager->m_currentEditor;
    QVERIFY(!ed.isNull());
    QCOMPARE(m_manager->m_editorFunctionID, efx->id());

    m_doc->deleteFunction(efx->id());
    QVERIFY(m_manager->m_currentEditor == nullptr);
    QCOMPARE(m_manager->m_editorFunctionID, Function::invalidId());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(ed.isNull(), "the item editor outlived its function");
    m_doc->setMode(Doc::Operate);
    m_doc->setMode(Doc::Design);

    // Another show: a function of the same id gets its editor (the id of
    // the last one was kept through the clear).
    EFX *again = new EFX(m_doc);
    QVERIFY(m_doc->addFunction(again));
    m_manager->showRightEditor(again);
    QVERIFY(m_manager->m_currentEditor != nullptr);
    m_doc->clearContents();
    QCOMPARE(m_manager->m_editorFunctionID, Function::invalidId());
    QVERIFY(m_manager->m_currentEditor == nullptr);
}


void ShowManager_Test::playInOperateClaimsAShowRunningFromDesign()
{
    Fixture *f = new Fixture(m_doc);
    f->setChannels(1);
    f->setUniverse(0);
    f->setAddress(0);
    QVERIFY(m_doc->addFixture(f));
    f->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    MasterTimer *timer = m_doc->masterTimer();
    m_manager->slotStartPlayback();         // Play in Design
    timer->timerTick();
    QVERIFY(m_show->isRunning());
    m_doc->setMode(Doc::Operate);
    timer->timerTick();
    QVERIFY(m_show->previewGate()->load()); // left running from Design: held
    m_manager->slotStartPlayback();         // Play in Operate
    timer->timerTick();
    QVERIFY(m_show->isRunning());
    QVERIFY(m_show->isPaused() == false);   // not a pause
    QVERIFY(m_show->previewGate()->load() == false);
    m_show->stop(FunctionParent::master());
    timer->timerTick();
    timer->timerTick();
    m_doc->setMode(Doc::Design);
}

/* Show only (R6-8): a Show that external timecode starts -- and the
   section's linked manual cue list its freeze starts -- is the show only
   when the operator armed the timecode link in Operate in this session.
   Saved on with the show (or switched on in Design): held. All Off
   disarms it. */
void ShowManager_Test::timecodeDrivesAShowOnlyFixtureOnlyWhenArmedInOperate()
{
    Fixture *f = new Fixture(m_doc);
    f->setChannels(1);
    f->setUniverse(0);
    f->setAddress(0);
    QVERIFY(m_doc->addFixture(f));
    f->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    Chaser *talk = new Chaser(m_doc);
    ChaserStep st(m_sceneB);
    st.hold = 100000;
    talk->addStep(st);
    m_doc->addFunction(talk);
    m_show->setMarker(0, 100000, QStringLiteral("Talk"));
    m_show->setMarkerCueList(0, talk->id());
    MasterTimer *timer = m_doc->masterTimer();
    auto stopAll = [this, timer, talk]() {
        m_show->stop(FunctionParent::master());
        talk->stop(FunctionParent::master());
        timer->timerTick();
        timer->timerTick();
    };

    m_show->setTimecodeFollow(true);                // saved on / set in Design
    m_doc->setMode(Doc::Operate);
    QCOMPARE(m_show->timecodeArmEpoch(), quint32(0));
    m_manager->slotTimecodeRunningChanged(true);    // timecode rolls
    timer->timerTick();
    QVERIFY(m_show->isRunning());
    QVERIFY(m_show->previewGate()->load());         // not armed: held
    m_manager->slotTimecodeRunningChanged(false);   // the freeze: its talk list
    timer->timerTick();
    QVERIFY(talk->isRunning());
    QVERIFY(talk->previewGate()->load());
    stopAll();

    m_manager->setFollowTimecode(false);            // the operator arms it, in Operate
    m_manager->setFollowTimecode(true);
    QVERIFY(m_show->timecodeArmEpoch() >= GenericFader::liveEpoch());     // a stamp of this session
    m_manager->slotTimecodeRunningChanged(true);
    timer->timerTick();
    QVERIFY(m_show->isRunning());
    QVERIFY(m_show->previewGate()->load() == false);
    m_manager->slotTimecodeRunningChanged(false);
    timer->timerTick();
    QVERIFY(talk->isRunning());
    QVERIFY(talk->previewGate()->load() == false);

    m_doc->outputRuleKeeper()->emergencyStop();     // All Off disarms it
    timer->timerTick();
    timer->timerTick();
    QVERIFY(!m_show->isRunning());
    m_manager->slotTimecodeRunningChanged(true);
    timer->timerTick();
    QVERIFY(m_show->isRunning());
    QVERIFY(m_show->previewGate()->load());
    m_manager->slotTimecodeRunningChanged(false);
    timer->timerTick();
    QVERIFY(talk->isRunning());
    QVERIFY(talk->previewGate()->load());
    stopAll();
    m_doc->setMode(Doc::Design);
}

/* Drop from the desktop onto the timeline (Branson: drag/drop for audio, and
   video). */

static QTemporaryDir *dropDir()
{
    static QTemporaryDir dir;
    return &dir;
}

QString ShowManager_Test::makeWav(const QString &name)
{
    // One second of 8 kHz mono 8-bit silence.
    const int rate = 8000;
    QByteArray d;
    auto le32 = [&d](quint32 v) { for (int i = 0; i < 4; i++) d.append(char((v >> (8 * i)) & 0xff)); };
    auto le16 = [&d](quint16 v) { d.append(char(v & 0xff)); d.append(char(v >> 8)); };
    d.append("RIFF"); le32(36 + rate); d.append("WAVEfmt "); le32(16);
    le16(1); le16(1); le32(rate); le32(rate); le16(1); le16(8);
    d.append("data"); le32(rate); d.append(QByteArray(rate, char(0x80)));
    const QString path = dropDir()->filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write(d);
    return path;
}

bool ShowManager_Test::loadAudioDecoders()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8; i++, dir.cdUp())
        if (dir.exists("plugins/Audio"))
        {
            m_doc->audioPluginCache()->load(QDir(dir.filePath("plugins/Audio")));
            break;
        }
    return m_doc->audioPluginCache()->getSupportedFormats().contains("*.wav");
}

void ShowManager_Test::dropFiles(const QStringList &files, quint32 timeMs, int row, bool drop,
                                 MultiTrackView *view)
{
    MultiTrackView *v = view ? view : m_manager->m_showview;
    QMimeData md;
    QList<QUrl> urls;
    for (const QString &f : files)
        urls << QUrl::fromLocalFile(f);
    md.setUrls(urls);
    const QPointF sp(v->getPositionFromTime(timeMs), TRACKS_TOP + row * TRACK_HEIGHT + 5);
    const QPoint vp = v->mapFromScene(sp);
    QDragEnterEvent enter(QPointF(vp), Qt::CopyAction, &md, Qt::LeftButton, Qt::NoModifier);
    v->dragEnterEvent(&enter);
    QVERIFY(enter.isAccepted());
    QDragMoveEvent move(QPointF(vp), Qt::CopyAction, &md, Qt::LeftButton, Qt::NoModifier);
    v->dragMoveEvent(&move);
    if (!drop)
        return;
    QDropEvent dropEv(QPointF(vp), Qt::CopyAction, &md, Qt::LeftButton, Qt::NoModifier);
    v->dropEvent(&dropEv);
}

void ShowManager_Test::droppedAudioLandsOnTheTrack()
{
    if (!loadAudioDecoders())
        QSKIP("no wav decoder (libsndfile) in this build");
    // Name order, not the order given.
    const QString b = makeWav("b-second.wav");
    const QString a = makeWav("a-first.wav");
    MultiTrackView *v = m_manager->m_showview;
    quint32 want = 0;
    Track *trk = nullptr;
    QVERIFY(v->dropTarget(v->mapFromScene(QPointF(v->getPositionFromTime(20000), TRACKS_TOP + 5)),
                          &want, &trk));
    dropFiles(QStringList() << b << a, 20000, 0);

    Track *t = m_show->tracks().first();
    QCOMPARE(t->showFunctions().count(), 3);       // the scene + two files
    QCOMPARE(m_doc->functionsByType(Function::AudioType).count(), 2);
    ShowFunction *first = nullptr, *second = nullptr;
    for (ShowFunction *sf : t->showFunctions())
    {
        Function *f = m_doc->function(sf->functionID());
        if (f->name() == "a-first.wav") first = sf;
        if (f->name() == "b-second.wav") second = sf;
    }
    QVERIFY(first && second);
    QCOMPARE(first->startTime(), want);
    QVERIFY(first->duration() > 0);
    QCOMPARE(second->startTime(), first->startTime() + first->duration());
    QVERIFY(m_manager->m_statusLabel->text().contains("Added 2"));

    // One Undo takes both back.
    m_manager->slotUndo();
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 1);
}

void ShowManager_Test::droppedAudioBelowTheTracksMakesATrack()
{
    if (!loadAudioDecoders())
        QSKIP("no wav decoder (libsndfile) in this build");
    dropFiles(QStringList() << makeWav("song.wav"), 5000, 3);   // row 3: no such track
    QCOMPARE(m_show->tracks().count(), 2);
    Track *t = m_show->tracks().last();
    QCOMPARE(t->showFunctions().count(), 1);
    QCOMPARE(m_doc->function(t->showFunctions().first()->functionID())->name(),
             QStringLiteral("song.wav"));
    m_manager->slotUndo();
    QCOMPARE(m_show->tracks().count(), 1);
}

void ShowManager_Test::droppedVideoLandsOnTheTrack()
{
    const QString path = dropDir()->filePath("clip.mp4");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not really a movie");
    f.close();
    dropFiles(QStringList() << path, 30000, 0);
    QCOMPARE(m_doc->functionsByType(Function::VideoType).count(), 1);
    Track *t = m_show->tracks().first();
    QCOMPARE(t->showFunctions().count(), 2);
    m_manager->slotUndo();
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 1);
}

void ShowManager_Test::anUnsupportedDropSaysWhy()
{
    const QString path = dropDir()->filePath("notes.txt");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("hello");
    f.close();
    dropFiles(QStringList() << path, 20000, 0);
    QVERIFY(m_manager->m_statusLabel->text().startsWith("Can't use notes.txt:"));
    QVERIFY(!m_manager->m_statusLabel->isHidden());
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 1);
    QVERIFY(!m_manager->m_undoAction->isEnabled());
}

static QAction *named(const QMenu &m, const char *name)
{
    for (QAction *a : m.actions())
        if (a->objectName() == name)
            return a;
    return nullptr;
}

void ShowManager_Test::sidebarMenusOfferWhatCanAct()
{
    MultiTrackView *v = m_manager->m_showview;
    m_manager->updateMultiTrackView();

    QMenu blank;
    v->buildBlankSidebarMenu(&blank);
    for (const char *n : { "addTrack", "addAudio", "addVideo", "addFunction" })
        QVERIFY2(named(blank, n) && named(blank, n)->isEnabled(), n);
    QVERIFY(!named(blank, "rename"));
    QVERIFY(!named(blank, "delete"));

    QMenu track;
    v->trackItemFor(m_show->tracks().first())->buildContextMenu(&track);
    for (const char *n : { "addTrack", "addAudio", "addVideo", "addFunction", "duplicate",
                           "mute", "solo", "lock", "rename", "colour", "moveDown", "delete" })
        QVERIFY2(named(track, n), n);
    QVERIFY(!named(track, "moveUp"));              // already the top row

    QVERIFY(!v->m_items.isEmpty());
    QMenu item;
    v->m_items.first()->buildContextMenu(&item);
    for (const char *n : { "duplicate", "rename", "moveToTrack", "snapToGrid",
                           "showInFunctions", "delete" })
        QVERIFY2(named(item, n), n);
    QVERIFY(!named(item, "addTrack"));
    // The shared toolbar actions, not copies.
    QCOMPARE(named(item, "snapToGrid"), m_manager->m_snapGridAction);
    QCOMPARE(named(item, "delete"), m_manager->m_deleteAction);
    QCOMPARE(named(blank, "addAudio"), m_manager->m_addAudioAction);
}

void ShowManager_Test::theEmbeddedTimelineTakesTheSameDrops()
{
    ShowTimelineEditor ed(nullptr, m_show, m_doc);
    QVERIFY(ed.m_showview->acceptsFileDrops());

    const QString clip = dropDir()->filePath("embedded.mp4");
    QFile f(clip);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not really a movie");
    f.close();
    const QString txt = dropDir()->filePath("embedded.txt");
    QFile g(txt);
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("hello");
    g.close();

    dropFiles(QStringList() << clip, 30000, 0, true, ed.m_showview);
    QCOMPARE(m_doc->functionsByType(Function::VideoType).count(), 1);
    QCOMPARE(m_show->tracks().first()->showFunctions().count(), 2);
    QVERIFY(ed.m_statusLabel->text().startsWith("Added embedded.mp4"));

    // Below the tracks: a track of its own.
    dropFiles(QStringList() << clip, 5000, 4, true, ed.m_showview);
    QCOMPARE(m_show->tracks().count(), 2);

    dropFiles(QStringList() << txt, 20000, 0, true, ed.m_showview);
    QVERIFY(ed.m_statusLabel->text().startsWith("Can't use embedded.txt:"));
    QCOMPARE(m_doc->functionsByType(Function::VideoType).count(), 2);

    if (loadAudioDecoders())
    {
        dropFiles(QStringList() << makeWav("emb-b.wav") << makeWav("emb-a.wav"), 60000, 0,
                  true, ed.m_showview);
        QCOMPARE(m_doc->functionsByType(Function::AudioType).count(), 2);
    }
}

void ShowManager_Test::shootsTheTimelineDrop()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        return;
    // Audio where a decoder is built; a clip's name otherwise (same drop).
    const bool audio = loadAudioDecoders();
    QStringList files;
    const QString ext = audio ? "wav" : "mp4";
    for (const char *n : { "1-Intro", "2-Verse" })
    {
        if (audio)
            files << makeWav(QString("%1.%2").arg(n, ext));
        else
        {
            const QString p = dropDir()->filePath(QString("%1.%2").arg(n, ext));
            QFile f(p);
            if (f.open(QIODevice::WriteOnly))
                f.write("x");
            files << p;
        }
    }
    const QList<QSize> sizes = { QSize(900, 500), QSize(1280, 720), QSize(1600, 900) };
    for (const QSize &sz : sizes)
    {
        const QString tag = QString("%1x%2").arg(sz.width()).arg(sz.height());
        m_manager->resize(sz);
        m_manager->show();
        QCoreApplication::processEvents();
        MultiTrackView *v = m_manager->m_showview;
        m_manager->slotFilesDragEntered(files);
        dropFiles(files, 8000, 0, false);
        QCoreApplication::processEvents();
        m_manager->grab().save(QDir(dir).filePath("ghost-" + tag + ".png"));
        dropFiles(files, 8000, 0, true);
        QCoreApplication::processEvents();
        m_manager->grab().save(QDir(dir).filePath("dropped-" + tag + ".png"));

        QMenu blank;
        v->buildBlankSidebarMenu(&blank);
        blank.ensurePolished();
        blank.resize(blank.sizeHint());
        blank.grab().save(QDir(dir).filePath("menu-sidebar-" + tag + ".png"));
        QMenu track;
        v->trackItemFor(m_show->tracks().first())->buildContextMenu(&track);
        track.ensurePolished();
        track.resize(track.sizeHint());
        track.grab().save(QDir(dir).filePath("menu-track-" + tag + ".png"));

        m_manager->slotUndo();
    }
}

void ShowManager_Test::addTrackFromTheMenuIsOneUndo()
{
    QMenu blank;
    m_manager->m_showview->buildBlankSidebarMenu(&blank);
    named(blank, "addTrack")->trigger();
    QCOMPARE(m_show->tracks().count(), 2);
    QVERIFY(m_manager->m_undoAction->isEnabled());
    m_manager->slotUndo();
    QCOMPARE(m_show->tracks().count(), 1);
}

void ShowManager_Test::renameInPlaceAndMoveUpDown()
{
    Track *other = new Track();
    other->setName("Sound");
    QVERIFY(m_show->addTrack(other));
    m_manager->updateMultiTrackView();
    MultiTrackView *v = m_manager->m_showview;

    TrackItem *ti = v->trackItemFor(m_show->tracks().first());
    ti->m_changeName->trigger();                    // the menu's Rename: inline
    QVERIFY(ti->m_nameProxy != nullptr);
    QLineEdit *e = qobject_cast<QLineEdit *>(ti->m_nameProxy->widget());
    QVERIFY(e);
    e->setText("Front");
    ti->slotRenameCommitted();
    QCOMPARE(m_show->tracks().first()->name(), QStringLiteral("Front"));

    ti = v->trackItemFor(m_show->tracks().first());
    ti->m_moveDown->trigger();
    QCOMPARE(m_show->tracks().first()->name(), QStringLiteral("Sound"));
    QCOMPARE(m_show->tracks().last()->name(), QStringLiteral("Front"));
    ti = v->trackItemFor(m_show->tracks().last());
    ti->m_moveUp->trigger();
    QCOMPARE(m_show->tracks().first()->name(), QStringLiteral("Front"));
}

QTEST_MAIN(ShowManager_Test)
