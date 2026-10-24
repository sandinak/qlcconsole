/*
  Q Light Controller Plus
  focuscallsui_test.cpp

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QSignalSpy>
#include <QToolButton>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QPointer>
#include <QLabel>
#include <QtTest>

#include "focuscallsui_test.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include "focuscallsdialog.h"
#include "focuscontrolmap.h"
#include "focusmodepanel.h"
#include "operatorvoice.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "refinesession.h"
#include "fixturegroup.h"
#include "refinepanel.h"
#include "stagetarget.h"
#include "qlcphysical.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "focuscalls.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "focuscallcue.h"
#include "cuerunner.h"
#include "controllerscheme.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

namespace
{
Fixture *makeSpot(Doc *doc, const QString &name, float x)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("UI Calls Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    QLCFixtureHead head;
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    doc->addFixture(fxi);
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0, QVector3D(x * 1000.0f, 2000.0f, 5.0f));
    return fxi;
}
}

void FocusCallsUi_Test::initTestCase()
{
    qputenv("QLC_NO_VOICE", "1");
}

void FocusCallsUi_Test::init()
{
    m_doc = new Doc(this);
    m_dir = new QTemporaryDir();
    m_doc->programmer()->focusSession()->setFlashEnabled(false);
    m_doc->programmer()->refineSession()->setFlashEnabled(false);
    m_spots.clear();
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("DH Tspots");
    g->setSize(QSize(3, 1));
    for (int i = 0; i < 3; ++i)
    {
        Fixture *fx = makeSpot(m_doc, QString("T%1").arg(i + 1), 2.0f + 2.0f * i);
        m_spots << fx->id();
        g->assignFixture(fx->id(), QLCPoint(i, 0));
    }
    m_doc->addFixtureGroup(g);
    m_group = g->id();
    StageTarget *t = m_doc->monitorProperties()->addStageTarget();
    t->setName("DSC");
    t->setPosition(QVector3D(4.0f, 6.0f, 0.0f));
    m_target = t->id();
}

void FocusCallsUi_Test::cleanup()
{
    if (FocusModePanel *p = FocusModePanel::instance())
    {
        m_doc->programmer()->focusSession()->stop();
        delete p;
    }
    if (RefinePanel *p = RefinePanel::instance())
    {
        m_doc->programmer()->refineSession()->stop();
        delete p;
    }
    delete m_doc;
    m_doc = nullptr;
    delete m_dir;
    m_dir = nullptr;
}

void FocusCallsUi_Test::theDialogEditsCallsAndPicksTheVenue()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);

    FocusCallsDialog d(m_doc);
    QCOMPARE(d.callList()->count(), 0);
    QPushButton *add = d.findChild<QPushButton *>("fcAdd");
    QVERIFY(add != nullptr);
    add->click();
    QCOMPARE(d.callList()->count(), 1);
    QListWidget *groups = d.findChild<QListWidget *>("fcGroups");
    QComboBox *target = d.findChild<QComboBox *>("fcTarget");
    QVERIFY(groups != nullptr && target != nullptr);
    groups->item(0)->setCheckState(Qt::Checked);
    target->setCurrentIndex(target->findText("DSC"));
    QCOMPARE(d.callList()->item(0)->text(), QString("1. DH Tspots \342\206\222 DSC"));
    add->click();   // the next call starts at the same point
    QCOMPARE(d.calls().at(1).target.id, m_target);

    // None: the show's own calibration, and nothing beside the show.
    d.setVenueChoice(QString());
    QVERIFY(d.apply());
    QCOMPARE(m_doc->programmer()->focusCalls()->count(), 2);
    QVERIFY(!venues->isActive());
    QVERIFY(!QFile::exists(venues->filePath()));

    // A new venue: made, applied, and its file written.
    d.setVenueChoice("Lincoln HS");
    QCOMPARE(d.venueChoice(), QString("Lincoln HS"));
    QVERIFY(d.apply());
    QCOMPARE(venues->activeVenue(), QString("Lincoln HS"));
    QVERIFY(QFile::exists(venues->filePath()));
    // The new-venue field offers the date when nothing is typed.
    FocusCallsDialog d2(m_doc);
    QLineEdit *nv = d2.findChild<QLineEdit *>("fcNewVenue");
    QVERIFY(nv->placeholderText().startsWith("Venue "));
    QCOMPARE(d2.venueChoice(), QString("Lincoln HS"));   // the one applied is offered
}

void FocusCallsUi_Test::theVenueManager()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    MonitorProperties *props = m_doc->monitorProperties();
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(m_spots.first());
    rp.tiltOffsetDeg = 2.0f;
    props->setFixtureRigProps(m_spots.first(), rp);     // recorded into Lincoln HS (Q111)
    venues->addVenue("Kennedy");

    FocusCallsDialog d(m_doc);
    QComboBox *combo = d.findChild<QComboBox *>("fcVenue");
    QVERIFY(combo->currentText().startsWith("Lincoln HS"));
    QVERIFY(combo->currentText().contains("applied"));
    QCOMPARE(d.venueChoice(), QString("Lincoln HS"));

    // Switch: Kennedy's corrections (none) in place of Lincoln's, at once.
    QVERIFY(d.switchVenue("Kennedy"));
    QCOMPARE(venues->activeVenue(), QString("Kennedy"));
    QCOMPARE(props->fixtureRigProps(m_spots.first()).tiltOffsetDeg, 0.0f);
    QVERIFY(combo->currentText().startsWith("Kennedy"));
    // None: the show's own.
    QVERIFY(d.switchVenue(QString()));
    QVERIFY(!venues->isActive());

    // Rename: kept, with its corrections.
    QVERIFY(d.renameVenue("Lincoln HS", "Lincoln High"));
    QVERIFY(!d.renameVenue("Lincoln High", "Kennedy"));     // taken
    QCOMPARE(venues->venueNames(), QStringList({ "Lincoln High", "Kennedy" }));
    QCOMPARE(venues->venue("Lincoln High").fixtures.value(m_spots.first()).tiltOffsetDeg, 2.0f);
    QCOMPARE(d.venueChoice(), QString("Lincoln High"));
    QVERIFY(d.switchVenue("Lincoln High"));
    QCOMPARE(props->fixtureRigProps(m_spots.first()).tiltOffsetDeg, 2.0f);

    // Delete the applied one: its corrections come off, the show's back.
    QVERIFY(d.deleteVenue("Lincoln High"));
    QVERIFY(!venues->isActive());
    QCOMPARE(props->fixtureRigProps(m_spots.first()).tiltOffsetDeg, 0.0f);
    QCOMPARE(venues->venueNames(), QStringList({ "Kennedy" }));
    // The file says so.
    QFile f(venues->filePath());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray xml = f.readAll();
    QVERIFY(!xml.contains("Lincoln"));
    QVERIFY(xml.contains("Kennedy"));
}

void FocusCallsUi_Test::aVenueNeedsASavedShow()
{
    FocusCallsDialog d(m_doc);
    d.setVenueChoice("Kennedy");
    QString why;
    QVERIFY(!d.apply(&why));
    QVERIFY(why.contains("Save the show"));
    QLabel *note = d.findChild<QLabel *>("fcVenueNote");
    QVERIFY(note->text().contains("Save the show"));
}

void FocusCallsUi_Test::thePanelShowsTheCall()
{
    FocusCall c;
    c.groups << m_group;
    c.target.kind = FocusSession::Reference::Target;
    c.target.id = m_target;
    c.note = "Cut off the pros";
    m_doc->programmer()->focusCalls()->setCalls({ c });
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(m_doc, nullptr, o, &why,
                                                               int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    QLabel *call = panel->findChild<QLabel *>("focusCall");
    QVERIFY(call != nullptr && !call->isHidden());
    QVERIFY2(call->text().startsWith("Call 1 of 1: DH Tspots"), qPrintable(call->text()));
    QVERIFY(call->text().contains("Cut off the pros"));
    QLabel *scope = panel->findChild<QLabel *>("focusScope");
    QVERIFY(scope->text().startsWith("One fixture"));
    QVERIFY(scope->text().contains("show's own calibration"));
    QCOMPARE(panel->controlMap()->tool(), ControllerScheme::FocusCallsTool);
    QCOMPARE(panel->controlMap()->actionText(GamepadRole::RightTrigger), QString("Next focus call"));
    // G is Y on the keyboard: the whole call.
    QTest::keyClick(panel, Qt::Key_G);
    QCOMPARE(m_doc->programmer()->focusSession()->scope(), FocusSession::GroupScope);
    QVERIFY(scope->text().startsWith("The whole call (3)"));
}

void FocusCallsUi_Test::theRefinePanel()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 cue = st->addCue(list, -1, "Ballad");
    ShowTree::Target t;
    t.groups << m_group;
    const quint32 layer = st->addLayer(cue, t);
    QLCPalette *centre = new QLCPalette(QLCPalette::PanTilt);
    centre->setName("Centre");
    centre->setValues({ 270, 135 });
    m_doc->addPalette(centre);
    ShowTree::Look pos;
    pos.key = ShowTree::KeyPosition;
    pos.palettes << centre->id();
    QVERIFY(st->setLayerLook(cue, layer, pos));

    QString why;
    QPointer<RefinePanel> panel = RefinePanel::open(m_doc, nullptr, cue, layer, &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    QVERIFY2(panel->headline().contains("DH Tspots, layer."), qPrintable(panel->headline()));
    QCOMPARE(panel->controlMap()->tool(), ControllerScheme::RefineTool);
    QCOMPARE(panel->controlMap()->actionText(GamepadRole::RightTrigger), QString("Next layer"));
    QCOMPARE(panel->controlMap()->actionText(GamepadRole::A), QString("Keep in the cue"));
    QVERIFY(panel->pendingLine().startsWith("Nothing pending"));

    QTest::keyClick(panel, Qt::Key_Right);
    QVERIFY2(panel->pendingLine().contains("pan 271"), qPrintable(panel->pendingLine()));
    QTest::keyClick(panel, Qt::Key_Return);
    QVERIFY(panel->pendingLine().startsWith("Nothing pending"));
    QCOMPARE(m_doc->palette(st->cue(cue)->layers.first().look(ShowTree::KeyPosition)->palettes.first())
             ->values(), QVariantList({ 271, 135 }));
    QVERIFY(panel->keepToLine().isEmpty());     // its own layer
    // G: to one fixture.
    QTest::keyClick(panel, Qt::Key_G);
    QVERIFY(panel->scopeLine().startsWith("Fixture: an exception for T1"));
    // Escape with nothing pending leaves at once.
    QTest::keyClick(panel, Qt::Key_Escape);
    QVERIFY(!m_doc->programmer()->refineSession()->isActive());
    QTRY_VERIFY(panel.isNull());

    // A cue that only tracks it: the panel offers where A keeps (S switches).
    const quint32 verse = st->addCue(list, 0, "Verse");
    panel = RefinePanel::open(m_doc, nullptr, verse, 0xFFFFFFFF, &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    QVERIFY2(panel->keepToLine().startsWith("Keep in the cue it tracks from (cue 1)"),
             qPrintable(panel->keepToLine()));
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(!r->keepsToSource());
    QTest::keyClick(panel, Qt::Key_S);
    QVERIFY(r->keepsToSource());
    QTest::keyClick(panel, Qt::Key_Escape);
    QTRY_VERIFY(panel.isNull());
}

void FocusCallsUi_Test::refineFromTheWorkbench()
{
    Scene *s = new Scene(m_doc);
    s->setName("Ballad look");
    s->addFixtureGroup(m_group);
    QLCPalette *centre = new QLCPalette(QLCPalette::PanTilt);
    centre->setName("Centre");
    centre->setValues({ 270, 135 });
    m_doc->addPalette(centre);
    s->addPalette(centre->id());
    QVERIFY(m_doc->addFunction(s));

    Workbench wb(m_doc);
    QToolButton *btn = wb.findChild<QToolButton *>("workbenchRefine");
    QVERIFY(btn != nullptr);
    wb.load(s->id());
    QCOMPARE(wb.refineScenes(), QList<quint32>({ s->id() }));
    QVERIFY(!btn->isHidden());

    // The header's Refine: the scene, as a layer, on the same controller.
    btn->click();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->isSceneMode());
    QPointer<RefinePanel> panel = RefinePanel::instance();
    QVERIFY(panel != nullptr);
    QVERIFY2(panel->headline().contains("scene Ballad look"), qPrintable(panel->headline()));
    QCOMPARE(panel->controlMap()->actionText(GamepadRole::Y),
             QString("Scope: layer \342\206\222 group \342\206\222 fixture"));
    r->stop();

    // A fixture's row: Refine starts on that fixture, at Fixture scope.
    QVERIFY(wb.openRefine(s->id(), 0xFFFFFFFF, m_spots.at(1)));
    QCOMPARE(r->scope(), RefineSession::FixtureScope);
    QCOMPARE(r->currentFixture(), m_spots.at(1));
    QVERIFY(panel->scopeLine().startsWith("Fixture: an exception for T2"));
    r->stop();
    QTRY_VERIFY(panel.isNull());
}

void FocusCallsUi_Test::aFocusCallCueWaitsForTheController()
{
    FocusCall c;
    c.groups << m_group;
    c.target.kind = FocusSession::Reference::Target;
    c.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ c });
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    const quint32 list = st->addCueList(prod, "Pre-show");
    const quint32 cue = st->addCue(list, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ cue }, true));

    CueRunner runner(m_doc, CueRunner::Full);
    QVERIFY(runner.focusArmedText().isEmpty());
    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    m_doc->setMode(Doc::Operate);
    fcc->cueCurrent(list, cue);
    QCOMPARE(runner.focusArmedText(), QString("Focus calls: waiting for the controller \u2014 press Start"));
    QToolButton *start = runner.findChild<QToolButton *>("crFocusStart");
    QVERIFY(start != nullptr && !start->isHidden());

    // The Run tab's button does what Start does; the panel comes up on it.
    start->click();
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(runner.focusArmedText().startsWith("Focus calls running"));
    QPointer<FocusModePanel> panel = FocusModePanel::attach(m_doc, nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(panel->findChild<QLabel *>("focusCall")->text().startsWith("Call 1 of 1"));
    // Menu (Esc): leave, and on with the list.
    QSignalSpy goOn(fcc, &FocusCallCue::continueRequested);
    QTest::keyClick(panel, Qt::Key_Escape);
    QTRY_COMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(goOn.count(), 1);
    QVERIFY(runner.focusArmedText().isEmpty());
    m_doc->setMode(Doc::Design);
}

void FocusCallsUi_Test::feedbackIsOneSettingOnEveryPanel()
{
    QSettings().remove("controllerTools/feedback");
    QCOMPARE(OperatorFeedbackCombo::savedFeedback(), int(ControllerScheme::BothFeedback));
    ProgrammerController *pc = m_doc->programmer();
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(m_doc, nullptr, o, &why,
                                                               int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    QComboBox *combo = panel->findChild<QComboBox *>("operatorFeedback");
    QVERIFY(combo != nullptr);
    QCOMPARE(combo->currentData().toInt(), int(ControllerScheme::BothFeedback));
    combo->setCurrentIndex(combo->findData(int(ControllerScheme::VoiceFeedback)));
    QCOMPARE(pc->operatorFeedback(), ControllerScheme::VoiceFeedback);
    // Shift+W: the next one, said; the panel follows.
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QTest::keyClick(panel, Qt::Key_W, Qt::ShiftModifier);
    QCOMPARE(pc->operatorFeedback(), ControllerScheme::BothFeedback);
    QCOMPARE(said.last().at(0).toString(), QString("Feedback: lights and voice."));
    QCOMPARE(combo->currentData().toInt(), int(ControllerScheme::BothFeedback));
    // W: Say the light.
    QTest::keyClick(panel, Qt::Key_W);
    QCOMPARE(said.last().at(0).toString(), QString("T1. Fixture 1 of 3."));
    OperatorFeedbackCombo::saveFeedback(int(ControllerScheme::LightsFeedback));
    QCOMPARE(OperatorFeedbackCombo::savedFeedback(), int(ControllerScheme::LightsFeedback));
    QSettings().remove("controllerTools/feedback");
}

void FocusCallsUi_Test::theVoiceHearsTheAnnouncements()
{
    OperatorVoice *voice = OperatorVoice::create(this);
    QCOMPARE(voice->backend(), QString("none"));   // never heard from a test
    SilentVoice *silent = qobject_cast<SilentVoice *>(voice);
    QVERIFY(silent != nullptr);

    const bool was = OperatorVoice::isEnabled();
    OperatorVoice::setEnabled(true);
    OperatorAnnouncer announcer(m_doc->programmer(), voice);
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QVERIFY(m_doc->programmer()->focusSession()->start(o));
    QCOMPARE(silent->said().last(), QString("Fixture 1 of 3."));
    m_doc->programmer()->focusSession()->next();
    QCOMPARE(silent->said().last(), QString("Fixture 2 of 3."));
    QCOMPARE(announcer.lastSaid(), QString("Fixture 2 of 3."));

    // Never over a line being said: the newest waits, stale ones go.
    silent->setSlow(true);
    const int before = silent->said().size();
    voice->say("Call 1 of 3.");
    voice->say("Fixture 2 of 4.");
    voice->say("Fixture 3 of 4.");
    QCOMPARE(silent->said().size(), before + 1);
    QVERIFY(voice->isSpeaking());
    QCOMPARE(voice->waiting(), QString("Fixture 3 of 4."));
    silent->finishLine();
    QCOMPARE(silent->said().last(), QString("Fixture 3 of 4."));
    silent->finishLine();
    QVERIFY(!voice->isSpeaking());
    QCOMPARE(silent->said().size(), before + 2);
    silent->setSlow(false);

    // Off: nothing is said (but the last phrase is still known).
    OperatorVoice::setEnabled(false);
    const int count = silent->said().size();
    m_doc->programmer()->focusSession()->next();
    QCOMPARE(silent->said().size(), count);
    QCOMPARE(announcer.lastSaid(), QString("Fixture 3 of 3."));
    OperatorVoice::setEnabled(was);
    m_doc->programmer()->focusSession()->stop();
    delete voice;
}

QTEST_MAIN(FocusCallsUi_Test)
