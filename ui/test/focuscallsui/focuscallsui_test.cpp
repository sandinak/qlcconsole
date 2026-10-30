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
#include <QCheckBox>
#include <QLineEdit>
#include <QSettings>
#include <QPointer>
#include <QInputDialog>
#include <QLabel>
#include <QtTest>

#include "focuscallsui_test.h"
#include "../common/layoutstability.h"
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
#include "focusreport.h"
#include "programmingoutline.h"
#include "workbench.h"
#include "monitorgraphicsview.h"
#include "focuscallcue.h"
#include "fixturevisualtraits.h"
#include "cuerunner.h"
#include "controllerscheme.h"
#include "showtreeview.h"
#include "focusmenu.h"
#include "venuemenu.h"
#include "venuerigdialog.h"
#include "venuerig.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "teachingsettings.h"

#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>

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

void FocusCallsUi_Test::theNewVenueNameHasRoom()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));   // "Start from" shows too
    QVERIFY(venues->setActiveVenue(QString()));
    FocusCallsDialog d(m_doc);
    QComboBox *venue = d.findChild<QComboBox *>("fcVenue");
    QLineEdit *name = d.findChild<QLineEdit *>("fcNewVenue");
    QVERIFY(venue != nullptr && name != nullptr);
    venue->setCurrentIndex(venue->count() - 1);                      // New venue...
    d.show();
    QVERIFY(QTest::qWaitForWindowExposed(&d));
    QVERIFY(name->isVisible());
    const int room = name->fontMetrics().horizontalAdvance(QStringLiteral("Lincoln High School"));
    QVERIFY2(name->width() >= room, qPrintable(QString("%1 px for the name, %2 wanted, dialog %3 px")
                                               .arg(name->width()).arg(room).arg(d.width())));
    for (const char *id : { "fcApplyVenue", "fcRenameVenue", "fcDeleteVenue", "fcCorrections", "fcVenueRig" })
    {
        QPushButton *b = d.findChild<QPushButton *>(id);
        QVERIFY(b != nullptr);
        QVERIFY2(b->width() >= b->minimumSizeHint().width(),
                 qPrintable(QString("%1: %2 < %3").arg(id).arg(b->width()).arg(b->minimumSizeHint().width())));
        QVERIFY2(QRect(QPoint(0, 0), d.size()).contains(QRect(b->mapTo(&d, QPoint(0, 0)), b->size())), id);
    }
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
    QToolButton *btn = wb.findChild<QToolButton *>("workbenchFocusMenu");
    QVERIFY(btn != nullptr);
    wb.load(s->id());
    QCOMPARE(wb.refineScenes(), QList<quint32>({ s->id() }));
    QVERIFY(!btn->isHidden());
    QCOMPARE(btn->text(), QString("Focus"));

    // The header's Focus > Focus this look: the scene, as a layer, on the same controller.
    emit btn->menu()->aboutToShow();
    QAction *self = btn->menu()->findChild<QAction *>("workbenchRefine");
    QVERIFY(self != nullptr && self->isEnabled());
    QCOMPARE(self->text(), QString("Focus this look\342\200\246"));
    // A Build scene is not a cue: no venue focus cue to make.
    QVERIFY(btn->menu()->findChild<QAction *>("workbenchFocusCue") == nullptr);
    self->trigger();
    RefineSession *r = m_doc->programmer()->refineSession();
    QVERIFY(r->isSceneMode());
    QPointer<RefinePanel> panel = RefinePanel::instance();
    QVERIFY(panel != nullptr);
    QVERIFY2(panel->headline().contains("scene Ballad look"), qPrintable(panel->headline()));
    QCOMPARE(panel->controlMap()->actionText(GamepadRole::Y),
             QString("Scope: layer \342\206\222 group \342\206\222 fixture"));
    r->stop();

    /* Q116: the Centre palette is the scene's own here, so no warning; give
       it a second user and Refine asks first. */
    Scene *other = new Scene(m_doc);
    other->setName("Chorus look");
    other->addPalette(centre->id());
    QVERIFY(m_doc->addFunction(other));
    QVERIFY(wb.openRefine());
    r->nudgePosition(1.0f, 0.0f);
    QVERIFY(!r->keep());
    QVERIFY2(panel->sharedLine().contains("Chorus look"), qPrintable(panel->sharedLine()));
    QPushButton *all = panel->findChild<QPushButton *>("refineSharedConfirm");
    QVERIFY(all != nullptr);
    all->click();
    QCOMPARE(centre->values(), QVariantList({ 271, 135 }));
    QVERIFY(panel->sharedLine().isEmpty());
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
    fcc->setRunningCueProbe([cue](quint32) { return cue; });   // the list plays it
    m_doc->setMode(Doc::Operate);
    fcc->cueCurrent(list, cue);
    QCOMPARE(runner.focusArmedText(), QString("Venue focus: waiting for the controller \u2014 press Start"));
    QToolButton *start = runner.findChild<QToolButton *>("crFocusStart");
    QVERIFY(start != nullptr && !start->isHidden());

    // The Run tab's button does what Start does; the panel comes up on it.
    start->click();
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QVERIFY(runner.focusArmedText().startsWith("Venue focus running"));
    QPointer<FocusModePanel> panel = FocusModePanel::attach(m_doc, nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(panel->findChild<QLabel *>("focusCall")->text().startsWith("Call 1 of 1"));
    // Menu (Esc): leave, and on with the list.
    QSignalSpy goOn(fcc, &FocusCallCue::continueRequested);
    QTest::keyClick(panel, Qt::Key_Escape);
    QTRY_COMPARE(fcc->state(), FocusCallCue::Idle);
    QCOMPARE(goOn.count(), 1);
    QVERIFY(runner.focusArmedText().isEmpty());
    fcc->setRunningCueProbe({});
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
    QCOMPARE(said.last().at(0).toString(), QString("T1. Fixture 1."));
    OperatorFeedbackCombo::saveFeedback(int(ControllerScheme::LightsFeedback));
    QCOMPARE(OperatorFeedbackCombo::savedFeedback(), int(ControllerScheme::LightsFeedback));
    QSettings().remove("controllerTools/feedback");
}

void FocusCallsUi_Test::theLaptopAsksForTheVenue()
{
    FocusCall c;
    c.groups << m_group;
    c.target.kind = FocusSession::Reference::Target;
    c.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ c });
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Pre-show");
    const quint32 cue = st->addCue(list, -1, "Focus");
    QVERIFY(st->setCuesFocusCall({ cue }, true));
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    QPointer<QDialog> ask;
    connect(fcc, &FocusCallCue::venueNeeded, this, [&](const QString &fallback, int ms, const QString &current) {
        ask = FocusCallsDialog::askVenue(m_doc, nullptr, fallback, ms, current);
    });
    m_doc->setMode(Doc::Operate);
    fcc->cueCurrent(list, cue);
    QVERIFY(fcc->start());
    QVERIFY(ask != nullptr);
    QLabel *left = ask->findChild<QLabel *>("fcAskVenueLeft");
    QVERIFY2(left->text().startsWith("A new venue in 8 s"), qPrintable(left->text()));
    QVERIFY(ask->findChild<QPushButton *>("fcAskVenueStill") == nullptr);   // none applied yet
    QLineEdit *name = ask->findChild<QLineEdit *>("fcAskVenueName");
    QTest::keyClicks(name, "Kennedy");          // typing: the clock waits
    QVERIFY(fcc->state() == FocusCallCue::NamingVenue);
    QTest::keyClick(name, Qt::Key_Return);
    QCOMPARE(venues->activeVenue(), QString("Kennedy"));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    QTRY_VERIFY(ask.isNull());                   // it closes itself
    m_doc->programmer()->focusSession()->stop();

    // Q129: with one applied, still asked -- "Still at Kennedy" keeps it.
    fcc->cueCurrent(list, st->addCue(list, 0, "House"));
    fcc->cueCurrent(list, cue);
    QVERIFY(fcc->start());
    QVERIFY(ask != nullptr);
    QVERIFY(ask->findChild<QLabel *>("fcAskVenueQuestion")->text().startsWith("Still at Kennedy?"));
    QPushButton *still = ask->findChild<QPushButton *>("fcAskVenueStill");
    QVERIFY(still != nullptr);
    still->click();
    QCOMPARE(venues->activeVenue(), QString("Kennedy"));
    QCOMPARE(fcc->state(), FocusCallCue::Running);
    m_doc->programmer()->focusSession()->stop();
    m_doc->setMode(Doc::Design);
}

void FocusCallsUi_Test::anOldVenueIsAskedAbout()
{
    // A show whose applied venue was last changed three days ago.
    const QString show = m_dir->filePath("Old.qlcc");
    QFile f(VenueCorrections::sidecarPath(show));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QString("<QlcVenues Version=\"1\" Show=\"Old.qlcc\" Active=\"Lincoln HS\">"
                    "<Venue Name=\"Lincoln HS\" Modified=\"%1\"/></QlcVenues>")
            .arg(QDateTime::currentDateTime().addDays(-3).toString(Qt::ISODate)).toUtf8());
    f.close();
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(show, true);
    QVERIFY(venues->needsConfirm());

    // The Focus calls dialog does not default to it: New venue does.
    {
        FocusCallsDialog d(m_doc);
        QCOMPARE(d.findChild<QComboBox *>("fcVenue")->currentData().toInt(), -2);
        QVERIFY(d.findChild<QLabel *>("fcVenueNote")->text().contains("Lincoln HS"));
    }

    // On open: "Still at Lincoln HS?", New venue the default.
    QPointer<QDialog> d = FocusCallsDialog::confirmVenue(m_doc, nullptr);
    QVERIFY(d != nullptr);
    QVERIFY(d->findChild<QLabel *>("venueConfirmQuestion")->text().contains("Still at Lincoln HS?"));
    QPushButton *fresh = d->findChild<QPushButton *>("venueConfirmNew");
    QVERIFY(fresh->isDefault());
    d->findChild<QPushButton *>("venueConfirmStill")->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(venues->activeVenue(), QString("Lincoln HS"));
    QVERIFY(!venues->needsConfirm());
    QVERIFY(FocusCallsDialog::confirmVenue(m_doc, nullptr) == nullptr);   // nothing to ask
    {
        FocusCallsDialog dlg(m_doc);
        QCOMPARE(dlg.venueChoice(), QString("Lincoln HS"));                 // confirmed: the default
    }

    // Opened again, New venue: a new one, Lincoln HS kept.
    venues->setShowFile(show, true);
    d = FocusCallsDialog::confirmVenue(m_doc, nullptr);
    QVERIFY(d != nullptr);
    QTest::keyClicks(d->findChild<QLineEdit *>("venueConfirmName"), "Kennedy");
    d->findChild<QPushButton *>("venueConfirmNew")->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(venues->activeVenue(), QString("Kennedy"));
    QVERIFY(venues->hasVenue("Lincoln HS"));
}

void FocusCallsUi_Test::theChangeSinceTheSaveShows()
{
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(m_doc, nullptr, o, &why,
                                                               int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    QVERIFY(panel->deltaLine().isEmpty());
    QTest::keyClick(panel, Qt::Key_Right, Qt::ShiftModifier);    // 1 degree of pan
    QCOMPARE(panel->deltaLine(), QString("Since the save: +1.0\u00b0 pan"));
    QTest::keyClick(panel, Qt::Key_Return);
    QVERIFY(panel->deltaLine().isEmpty());
    // U: the next light not saved (none: "All saved").
    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    QTest::keyClick(panel, Qt::Key_U);
    QCOMPARE(said.last().at(0).toString(), QString("All saved."));

    // The plot's ghost, as Monitor draws it.
    MonitorGraphicsView view(m_doc);
    view.resize(800, 600);
    QVERIFY(!view.hasFocusGhost());
    view.setFocusGhost(true, QVector3D(3.0f, 4.0f, 1.6f), QString("+2.5\u00b0 pan"));
    QVERIFY(view.hasFocusGhost());
    QCOMPARE(view.focusGhostLabel(), QString("saved (+2.5\u00b0 pan)"));
    view.setFocusGhost(false, QVector3D(), QString());
    QVERIFY(!view.hasFocusGhost());
}

void FocusCallsUi_Test::theVoiceHearsTheAnnouncements()
{
    OperatorVoice *voice = OperatorVoice::create(this);
    QCOMPARE(voice->backend(), QString("none"));   // never heard from a test
    SilentVoice *silent = qobject_cast<SilentVoice *>(voice);
    QVERIFY(silent != nullptr);
    voice->setSettleMs(0);      // as before Q171: theVoiceWaitsForTheSelectionToSettle

    const bool was = OperatorVoice::isEnabled();
    OperatorVoice::setEnabled(true);
    OperatorAnnouncer announcer(m_doc->programmer(), voice);
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QVERIFY(m_doc->programmer()->focusSession()->start(o));
    QCOMPARE(silent->said().last(), QString("Fixture 1, T1."));
    m_doc->programmer()->focusSession()->next();
    QCOMPARE(silent->said().last(), QString("Fixture 2, T2."));
    QCOMPARE(announcer.lastSaid(), QString("Fixture 2, T2."));

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
    QCOMPARE(announcer.lastSaid(), QString("Fixture 3, T3."));
    OperatorVoice::setEnabled(was);
    m_doc->programmer()->focusSession()->stop();
    delete voice;
}

void FocusCallsUi_Test::theVoiceNeverDropsAConfirmation()
{
    SilentVoice voice;
    voice.setSettleMs(0);       // as before Q171 (no settle, nothing cut)
    voice.setSlow(true);
    voice.say("Fixture 1 of 4.");                                        // being said
    voice.say("Fixture 2 of 4.");                                        // where: stale soon
    voice.say("Spot 2 kept.", ControllerScheme::ConfirmPriority);
    voice.say("Fixture 3 of 4.");                                        // replaces 2 of 4
    voice.say("Controller lost. Lights held.", ControllerScheme::ImportantPriority);
    voice.say("Fixture 4 of 4.");                                        // replaces 3 of 4
    QCOMPARE(voice.queued(), QStringList({ "Spot 2 kept.", "Controller lost. Lights held.",
                                           "Fixture 4 of 4." }));
    for (int i = 0; i < 4; ++i)
        voice.finishLine();
    QCOMPARE(voice.said(), QStringList({ "Fixture 1 of 4.", "Spot 2 kept.",
                                         "Controller lost. Lights held.", "Fixture 4 of 4." }));
    QVERIFY(!voice.isSpeaking());

    // Through the announcer, with the priority the engine gave it.
    const bool was = OperatorVoice::isEnabled();
    OperatorVoice::setEnabled(true);
    SilentVoice heard;
    heard.setSettleMs(0);
    heard.setSlow(true);
    OperatorAnnouncer announcer(m_doc->programmer(), &heard);
    emit m_doc->programmer()->operatorAnnounce("Call 1 of 2.", ControllerScheme::WherePriority);
    emit m_doc->programmer()->operatorAnnounce("Saved.", ControllerScheme::ConfirmPriority);
    emit m_doc->programmer()->operatorAnnounce("Call 2 of 2.", ControllerScheme::WherePriority);
    QCOMPARE(heard.queued(), QStringList({ "Saved.", "Call 2 of 2." }));
    heard.finishLine();
    heard.finishLine();
    heard.finishLine();
    OperatorVoice::setEnabled(was);
}

void FocusCallsUi_Test::theVoiceWaitsForTheSelectionToSettle()
{
    namespace CS = ControllerScheme;
    // 1. Ten steps in a row: nothing until it settles, then only the last.
    {
        SilentVoice v;
        v.setSettleMs(400);
        for (int i = 1; i <= 10; ++i)
            v.say(QString("Fixture %1 of 10.").arg(i));
        QVERIFY(v.said().isEmpty());
        QVERIFY(!v.isSpeaking());
        QCOMPARE(v.settling(), QString("Fixture 10 of 10."));
        v.settleNow();
        QCOMPARE(v.said(), QStringList({ "Fixture 10 of 10." }));
        QVERIFY(v.settling().isEmpty());
        QCOMPARE(v.cuts(), 0);
    }
    // 2. A step while a where line is being said: nothing cut until it
    //    settles, then the stale one is cut once and the newest said.
    {
        SilentVoice v;
        v.setSettleMs(400);
        v.setSlow(true);
        v.say("Fixture 2 of 8.");
        v.settleNow();
        QCOMPARE(v.said(), QStringList({ "Fixture 2 of 8." }));
        v.say("Fixture 3 of 8.");
        v.say("Fixture 6 of 8.");
        QCOMPARE(v.cuts(), 0);
        QVERIFY(v.cutLines().isEmpty());
        v.settleNow();
        QCOMPARE(v.cuts(), 1);
        QCOMPARE(v.cutLines(), QStringList({ "Fixture 2 of 8." }));
        QCOMPARE(v.said(), QStringList({ "Fixture 2 of 8.", "Fixture 6 of 8." }));
        v.finishLine();
        QVERIFY(!v.isSpeaking());
    }
    // 3. A save while a step is settling: the save at once, the step after
    //    it -- neither dropped, nothing cut.
    {
        SilentVoice v;
        v.setSettleMs(400);
        v.setSlow(true);
        v.say("Fixture 7 of 8.");
        v.say("Saved fixture 7.", CS::ConfirmPriority);
        QCOMPARE(v.said(), QStringList({ "Saved fixture 7." }));
        v.settleNow();
        QCOMPARE(v.cuts(), 0);                      // a save is never cut
        QCOMPARE(v.queued(), QStringList({ "Fixture 7 of 8." }));
        v.finishLine();
        QCOMPARE(v.said(), QStringList({ "Saved fixture 7.", "Fixture 7 of 8." }));
        v.finishLine();
    }
    // 4. Important over a where line being said: cut at once. Over a
    //    confirmation: it waits, nothing cut.
    {
        SilentVoice v;
        v.setSettleMs(400);
        v.setSlow(true);
        v.say("Fixture 3 of 8, Spot 3, needs focus.");
        v.settleNow();
        v.say("Controller lost. Lights held.", CS::ImportantPriority);
        QCOMPARE(v.cuts(), 1);
        QCOMPARE(v.said().last(), QString("Controller lost. Lights held."));
        v.finishLine();
        QVERIFY(!v.isSpeaking());
        v.say("Saved fixture 3.", CS::ConfirmPriority);
        v.say("Holding.", CS::ImportantPriority);
        QCOMPARE(v.cuts(), 1);
        QCOMPARE(v.queued(), QStringList({ "Holding." }));
        // 5. A where line never stops a confirmation or an important line.
        v.say("Fixture 4 of 8.");
        v.settleNow();
        QCOMPARE(v.cuts(), 1);
        QCOMPARE(v.queued(), QStringList({ "Holding.", "Fixture 4 of 8." }));
        v.finishLine();
        QCOMPARE(v.said().last(), QString("Holding."));
        v.say("Fixture 5 of 8.");
        v.settleNow();
        QCOMPARE(v.cuts(), 1);                      // not over "Holding." either
        QCOMPARE(v.queued(), QStringList({ "Fixture 5 of 8." }));
        v.finishLine();
        v.finishLine();
        QCOMPARE(v.said().last(), QString("Fixture 5 of 8."));
    }
    // 7. Say the light (a confirmation) while a step settles: at once.
    //    A where-now line (a tool's first, entering a call): at once, and
    //    the older step waiting is gone.
    {
        SilentVoice v;
        v.setSettleMs(400);
        v.say("Fixture 2 of 8.");
        v.say("Spot 2, fixture 2.", CS::ConfirmPriority);
        QCOMPARE(v.said(), QStringList({ "Spot 2, fixture 2." }));
        v.say("Call 2 of 5, AT spots to Soloist DS.", CS::WhereNowPriority);
        QCOMPARE(v.said().last(), QString("Call 2 of 5, AT spots to Soloist DS."));
        QVERIFY(v.settling().isEmpty());
        v.settleNow();
        QCOMPARE(v.said().size(), 2);
    }
    // Off (0): every line at once, nothing cut -- as before.
    {
        SilentVoice v;
        v.setSettleMs(0);
        v.setSlow(true);
        v.say("Fixture 1 of 4.");
        v.say("Fixture 2 of 4.");
        v.say("Controller lost.", CS::ImportantPriority);
        QCOMPARE(v.said(), QStringList({ "Fixture 1 of 4." }));
        QCOMPARE(v.cuts(), 0);
        QCOMPARE(v.queued(), QStringList({ "Fixture 2 of 4.", "Controller lost." }));
    }
    // The real timer.
    {
        SilentVoice v;
        v.setSettleMs(30);
        v.say("Fixture 1 of 2.");
        v.say("Fixture 2 of 2.");
        QVERIFY(v.said().isEmpty());
        QTRY_COMPARE(v.said(), QStringList({ "Fixture 2 of 2." }));
    }
    // The setting: Off / 250 / 400 / 700, 400 by default, beside Voice detail.
    {
        QSettings().remove("voice/settleMs");
        QCOMPARE(OperatorVoice::savedSettleMs(), 400);
        SilentVoice v;
        QCOMPARE(v.settleMs(), 400);
        VoiceSettleCombo combo;
        QCOMPARE(combo.count(), 4);
        QCOMPARE(combo.currentData().toInt(), 400);
        combo.setCurrentIndex(combo.findData(250));
        QCOMPARE(OperatorVoice::savedSettleMs(), 250);
        QCOMPARE(v.settleMs(), 250);                // a voice follows the setting
        combo.setCurrentIndex(combo.findData(0));
        QCOMPARE(v.settleMs(), 0);
        QSettings().remove("voice/settleMs");
    }

    // 8. End to end: the first line at once, five fast steps, then only
    //    where it stopped.
    const bool was = OperatorVoice::isEnabled();
    OperatorVoice::setEnabled(true);
    SilentVoice heard;
    heard.setSettleMs(400);
    OperatorAnnouncer announcer(m_doc->programmer(), &heard);
    FocusSession *s = m_doc->programmer()->focusSession();
    QSignalSpy lines(s, &FocusSession::announce);
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QVERIFY(s->start(o));
    QCOMPARE(lines.last().at(1).toInt(), int(CS::WhereNowPriority));
    QCOMPARE(heard.said().last(), QString("Fixture 1, T1."));   // at once
    const int first = heard.said().size();
    for (int i = 0; i < 5; ++i)
        s->next();
    QCOMPARE(lines.last().at(1).toInt(), int(CS::WherePriority));
    QCOMPARE(heard.said().size(), first);
    heard.settleNow();
    QCOMPARE(heard.said().size(), first + 1);
    QCOMPARE(heard.said().last(), QString("Fixture 3, T3."));      // 1 -> 2, 3, 1, 2, 3
    QVERIFY(!heard.said().contains("Fixture 2, T2."));
    // 9. Lights only: a step never reaches the voice.
    m_doc->programmer()->setOperatorFeedback(CS::LightsFeedback);
    s->next();
    QVERIFY(heard.settling().isEmpty());
    heard.settleNow();
    QCOMPARE(heard.said().size(), first + 1);
    m_doc->programmer()->setOperatorFeedback(CS::BothFeedback);
    s->stop();
    OperatorVoice::setEnabled(was);
}

void FocusCallsUi_Test::aSiblingsVenuesAreOffered()
{
    const QString v5 = m_dir->filePath("Dance-0.5.qlcc");
    QFile f(VenueCorrections::sidecarPath(v5));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QString("<QlcVenues Version=\"1\" Show=\"Dance-0.5.qlcc\" Active=\"Lincoln HS\">"
                    "<Venue Name=\"Lincoln HS\" Modified=\"%1\"/></QlcVenues>")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate)).toUtf8());
    f.close();
    const QString v6 = m_dir->filePath("Dance-0.6.qlcc");
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(v6, true);
    QVERIFY(FocusCallsDialog::offerVenueImport(m_doc, nullptr, v5) == nullptr);   // has its own
    QPointer<QDialog> d = FocusCallsDialog::offerVenueImport(m_doc, nullptr, v6);
    QVERIFY(d != nullptr);
    QVERIFY(d->findChild<QLabel *>("venueImportText")->text().contains("Dance-0.5.venues.xml"));
    d->findChild<QPushButton *>("venueImportUse")->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(venues->activeVenue(), QString("Lincoln HS"));
    QVERIFY(QFile::exists(VenueCorrections::sidecarPath(v6)));
}

void FocusCallsUi_Test::theRefinePanelHasTheLamp()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Main");
    const quint32 cue = st->addCue(list, -1, "Ballad");
    ShowTree::Target t;
    t.groups << m_group;
    const quint32 layer = st->addLayer(cue, t);
    ShowTree::Look look;
    look.key = ShowTree::KeyPosition;
    look.palettes << st->createInlinePalette(QLCPalette::PanTilt, { 270, 135 }, "Centre");
    QVERIFY(st->setLayerLook(cue, layer, look));
    QString why;
    QPointer<RefinePanel> panel = RefinePanel::open(m_doc, nullptr, cue, 0xFFFFFFFF, &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusStatusLamp *lamp = panel->statusLamp();
    QVERIFY(lamp != nullptr);
    QCOMPARE(lamp->state(), FocusStatusLamp::AllSaved);
    RefineSession *r = m_doc->programmer()->refineSession();
    r->setScope(RefineSession::FixtureScope);
    QVERIFY(r->nudgePosition(1.0f, 0.0f));
    QCOMPARE(lamp->state(), FocusStatusLamp::ThisUnsaved);
    QCOMPARE(lamp->count(), 1);
    r->nextItem();
    QCOMPARE(lamp->state(), FocusStatusLamp::OthersUnsaved);
    QCOMPARE(r->pendingCountPhrase(), QString("1 not kept."));
    r->revert();
    QCOMPARE(lamp->state(), FocusStatusLamp::AllSaved);
    r->stop();
    QTRY_VERIFY(panel.isNull());
}

void FocusCallsUi_Test::theCallPanelNeverMovesTheLayout()
{
    // A second group: a light with a long name and a zoom, and a plain one.
    Fixture *longName = makeSpot(m_doc, QString("Upstage centre profile spot on the second electric, "
                                                "far end by the fly rail"), 3.0f);
    QLCFixtureMode *mode = const_cast<QLCFixtureMode *>(longName->fixtureMode());
    {
        QLCFixtureDef *def = mode->fixtureDef();
        QLCChannel *z = new QLCChannel();
        z->setName("Zoom"); z->setGroup(QLCChannel::Beam);
        z->setPreset(QLCChannel::BeamZoomSmallBig);
        def->addChannel(z); mode->insertChannel(z, 3);
        QLCChannel *f = new QLCChannel();
        f->setName("Focus"); f->setGroup(QLCChannel::Beam);
        f->setPreset(QLCChannel::BeamFocusNearFar);
        def->addChannel(f); mode->insertChannel(f, 4);
        QLCPhysical ph = mode->physical();
        ph.setLensDegreesMin(10.0);
        ph.setLensDegreesMax(40.0);
        mode->setPhysical(ph);
        longName->setFixtureDefinition(def, mode);
    }
    FixtureGroup *g2 = new FixtureGroup(m_doc);
    g2->setName("Electric 2 by the fly rail");
    g2->setSize(QSize(2, 1));
    g2->assignFixture(longName->id(), QLCPoint(0, 0));
    g2->assignFixture(m_spots.first(), QLCPoint(1, 0));
    m_doc->addFixtureGroup(g2);

    // Saves to a venue with a long name.
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(venues->setActiveVenue(venues->addVenue(
        "Lincoln High School Performing Arts Center, main stage (spring tour)")));

    FocusCall c1;
    c1.groups << m_group;
    c1.target.kind = FocusSession::Reference::Target;
    c1.target.id = m_target;
    c1.note = "Cut off the pros\nKeep the shutters off the legs";
    FocusCall c2;
    c2.groups << g2->id();
    c2.target.kind = FocusSession::Reference::Target;
    c2.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ c1, c2 });
    FocusSession::Options o;
    o.calls = m_doc->programmer()->focusCalls()->sessionCalls();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(m_doc, nullptr, o, &why,
                                                               int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusSession *s = m_doc->programmer()->focusSession();
    QVERIFY(s->isRunningCalls() && s->savesToVenue());
    panel->resize(720, 900);
    panel->show();
    QVERIFY(QTest::qWaitForWindowExposed(panel));

    const QStringList names = { "focusTitle", "focusCall", "focusFixture", "focusFixtureNote",
                                "focusSaveStatus", "focusReferenceName", "focusOffsets", "focusDelta",
                                "focusConfirmed", "focusBeam", "focusSaved", "focusScope", "focusStep",
                                "focusPausedNote", "focusInputNote", "focusExit" };
    const QHash<QString, QWidget *> extra = { { "map", panel->controlMap() } };
    const LayoutSnapshot base = LayoutSnapshot::take(panel, names, extra);
    QStringList moved;
    auto check = [&](const QString &step) {
        const QString what = LayoutSnapshot::take(panel, names, extra).movedSince(base, step);
        if (!what.isEmpty())
            moved << what;
        if (panel->minimumSizeHint().width() > 720)
            moved << QStringLiteral("after %1: wants %2 px wide").arg(step)
                     .arg(panel->minimumSizeHint().width());
    };
    auto key = [&](int k, Qt::KeyboardModifiers m = Qt::NoModifier) {
        QTest::keyClick(panel, Qt::Key(k), m);
    };

    check("opening");
    QVERIFY(panel->findChild<QLabel *>("focusCall")->text().contains("Keep the shutters"));
    for (int i = 0; i < 3; ++i)
    {
        key(Qt::Key_BracketRight);
        check(QStringLiteral("next fixture %1").arg(i + 1));
    }
    key(Qt::Key_D);
    QVERIFY(venues->isDead(s->currentFixture()));
    QVERIFY(!panel->findChild<QLabel *>("focusFixtureNote")->text().isEmpty());
    check("marked dead");
    key(Qt::Key_D);
    check("not dead");
    key(Qt::Key_Right, Qt::ShiftModifier);
    check("a nudge");
    key(Qt::Key_Return);
    check("save");
    key(Qt::Key_Left, Qt::ShiftModifier);
    key(Qt::Key_Backspace);
    check("revert");
    key(Qt::Key_G);
    check("the whole call");
    key(Qt::Key_G);
    check("one fixture");
    key(Qt::Key_PageDown);
    QVERIFY(panel->findChild<QLabel *>("focusCall")->text().startsWith("Call 2 of 2"));
    check("the next call (no note)");
    for (int i = 0; i < 2; ++i)
    {
        key(Qt::Key_BracketRight);
        check(QStringLiteral("call 2, next fixture %1").arg(i + 1));
    }
    s->setIndex(0);
    check("the long name");
    key(Qt::Key_Period, Qt::ShiftModifier);
    key(Qt::Key_Apostrophe, Qt::ShiftModifier);
    check("zoom and focus");
    key(Qt::Key_R);
    check("second reference");
    key(Qt::Key_R);
    key(Qt::Key_H);
    check("holding");
    key(Qt::Key_H);
    key(Qt::Key_PageUp);
    check("back to call 1");

    QVERIFY2(moved.isEmpty(), qPrintable(moved.join(QStringLiteral("\n"))));
    s->discardAll();
}

void FocusCallsUi_Test::theRefinePanelNeverMovesTheLayout()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Spring");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 cue = st->addCue(list, -1, "Ballad with a long name for the cue in act two");
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
    const quint32 verse = st->addCue(list, 0, "Verse");

    for (const quint32 c : { cue, verse })
    {
        QString why;
        QPointer<RefinePanel> panel = RefinePanel::open(m_doc, nullptr, c,
                                                        c == cue ? layer : 0xFFFFFFFF, &why);
        QVERIFY2(panel != nullptr, qPrintable(why));
        panel->resize(720, 820);
        panel->show();
        QVERIFY(QTest::qWaitForWindowExposed(panel));
        const QStringList names = { "refineTitle", "refineLayer", "refineScope", "refinePending",
                                    "refineToSource", "refineShared", "refineExit" };
        const QHash<QString, QWidget *> extra = { { "map", panel->controlMap() } };
        const LayoutSnapshot base = LayoutSnapshot::take(panel, names, extra);
        QStringList moved;
        auto check = [&](const QString &step) {
            const QString what = LayoutSnapshot::take(panel, names, extra).movedSince(base, step);
            if (!what.isEmpty())
                moved << what;
        };
        auto key = [&](int k) { QTest::keyClick(panel, Qt::Key(k)); };
        check("opening");
        key(Qt::Key_Right);
        check("pending");
        key(Qt::Key_G);
        check("scope");
        key(Qt::Key_BracketRight);
        check("next fixture");
        key(Qt::Key_G);
        key(Qt::Key_G);
        check("scope back");
        key(Qt::Key_Return);
        check("kept");
        key(Qt::Key_S);
        check("keep to source");
        key(Qt::Key_S);
        key(Qt::Key_H);
        check("holding");
        key(Qt::Key_H);
        QVERIFY2(moved.isEmpty(), qPrintable(QStringLiteral("cue %1: ").arg(c)
                                             + moved.join(QStringLiteral("\n"))));
        m_doc->programmer()->refineSession()->stop();
        delete panel;
    }
}

void FocusCallsUi_Test::aMissingVoiceOutputFallsBackToTheSystem()
{
#ifdef Q_OS_WIN
    QSKIP("the fake say is a shell script");
#else
    // A `say` of its own: lists the outputs in "devices", fails like say -a
    // on one not listed, and writes "<output>|<line>" to "log" for the rest.
    const QString dir = m_dir->path();
    const QString say = dir + "/say";
    {
        QFile f(say);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("#!/bin/sh\n"
                "dir=$(dirname \"$0\")\n"
                "dev=\"\"; prev=\"\"; last=\"\"\n"
                "for a in \"$@\"; do\n"
                "  if [ \"$prev\" = \"-a\" ]; then dev=\"$a\"; fi\n"
                "  prev=\"$a\"; last=\"$a\"\n"
                "done\n"
                "if [ \"$dev\" = \"?\" ]; then cat \"$dir/devices\"; exit 0; fi\n"
                "if [ -n \"$dev\" ] && ! grep -q \"^ *$dev \" \"$dir/devices\"; then\n"
                "  echo \"Invalid device\" >&2; exit 1\n"
                "fi\n"
                "printf '%s|%s\\n' \"$dev\" \"$last\" >> \"$dir/log\"\n");
        f.close();
        QFile::setPermissions(say, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    auto setDevices = [dir](const QString &listing) {
        QFile f(dir + "/devices");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(listing.toUtf8());
    };
    auto log = [dir]() {
        QFile f(dir + "/log");
        if (!f.open(QIODevice::ReadOnly))
            return QStringList();
        return QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    };
    const QString all = "   74 MacBook Pro Speakers\n   91 Wireless Controller\n   92 Wireless Controller\n";
    setDevices(all);

    // Read as say -a ? lists them; one name twice, told apart by ID.
    const QList<OperatorVoice::OutputDevice> listed = OperatorVoice::outputDeviceList(say);
    QCOMPARE(listed.size(), 3);
    QCOMPARE(listed.at(2).id, QString("92"));
    QCOMPARE(listed.at(2).name, QString("Wireless Controller"));
    QCOMPARE(OperatorVoice::findOutputDevice(listed, "92", "Wireless Controller"), 2);
    QCOMPARE(OperatorVoice::findOutputDevice(listed, "17", "Wireless Controller"), 1);   // a new ID: by name
    QCOMPARE(OperatorVoice::findOutputDevice(listed, "92", "AirPods"), -1);
    QCOMPARE(OperatorVoice::findOutputDevice(listed, "", "MacBook Pro Speakers"), 0);

    OperatorVoice::setOutputDevice("Wireless Controller", "92");
    QCOMPARE(OperatorVoice::outputDeviceId(), QString("92"));
    CommandVoice voice(say, { "-r", "210" });
    voice.setSettleMs(0);       // each line at once (the settle has its own test)
    voice.setDeviceCheckInterval(0);
    auto sayAndWait = [&voice](const QString &line, int priority) {
        voice.say(line, priority);
        QTRY_VERIFY_WITH_TIMEOUT(!voice.isSpeaking(), 5000);
    };

    // The chosen pad, by its ID.
    sayAndWait("Fixture 1, T1.", ControllerScheme::WherePriority);
    QCOMPARE(log(), QStringList({ "92|Fixture 1, T1." }));

    /* Its ID gone but the name there: a pad plugged in again gets a new
       ID, so the name finds it. */
    setDevices("   74 MacBook Pro Speakers\n   93 Wireless Controller\n");
    sayAndWait("Fixture 2, T2.", ControllerScheme::WherePriority);
    QStringList heard = log();
    QCOMPARE(heard.size(), 2);
    QCOMPARE(heard.at(1), QString("93|Fixture 2, T2."));

    // Not there at all: the system's output, never lost, and said why once.
    OperatorVoice::setOutputDevice("Headset feed", "50");
    sayAndWait("Controller lost. Lights held.", ControllerScheme::ImportantPriority);
    heard = log();
    QCOMPARE(heard.size(), 3);
    QCOMPARE(heard.at(2), "|" + CommandVoice::missingDeviceNotice("Headset feed")
                          + " Controller lost. Lights held.");
    sayAndWait("Saved.", ControllerScheme::ConfirmPriority);
    sayAndWait("Fixture 3, T3.", ControllerScheme::WherePriority);
    heard = log();
    QCOMPARE(heard.size(), 5);
    QCOMPARE(heard.at(3), QString("|Saved."));
    QCOMPARE(heard.at(4), QString("|Fixture 3, T3."));

    // Gone between the look and the line (say -a fails): the line again, on the system's.
    setDevices("   74 MacBook Pro Speakers\n   50 Headset feed\n");
    sayAndWait("Fixture 4, T4.", ControllerScheme::WherePriority);
    QCOMPARE(log().last(), QString("50|Fixture 4, T4."));
    voice.setDeviceCheckInterval(60000);        // trusts that look...
    setDevices("   74 MacBook Pro Speakers\n");  // ...and the output goes
    sayAndWait("Controller lost. Lights held.", ControllerScheme::ImportantPriority);
    QCOMPARE(log().last(), "|" + CommandVoice::missingDeviceNotice("Headset feed")
                           + " Controller lost. Lights held.");
    voice.setDeviceCheckInterval(0);

    // Back: to it again.
    setDevices("   74 MacBook Pro Speakers\n   50 Headset feed\n");
    sayAndWait("Fixture 5, T5.", ControllerScheme::WherePriority);
    QCOMPARE(log().last(), QString("50|Fixture 5, T5."));

    // The system's output chosen: no -a at all.
    OperatorVoice::setOutputDevice(QString());
    QVERIFY(OperatorVoice::outputDeviceId().isEmpty());
    sayAndWait("Fixture 6, T6.", ControllerScheme::WherePriority);
    QCOMPARE(log().last(), QString("|Fixture 6, T6."));
#endif
}

void FocusCallsUi_Test::oneCorrectionIsSeenAndCleared()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(FocusCallsDialog::showCorrections(m_doc, nullptr) == nullptr);   // none applied
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    MonitorProperties *props = m_doc->monitorProperties();
    for (quint32 fid : { m_spots.at(0), m_spots.at(1) })
    {
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.panOffsetDeg = 1.5f;
        props->setFixtureRigProps(fid, rp);
    }
    QCOMPARE(venues->correctionCount(), 2);
    {
        FocusCallsDialog dlg(m_doc);
        QVERIFY(dlg.findChild<QPushButton *>("fcCorrections")->isEnabled());
    }
    QPointer<QDialog> d = FocusCallsDialog::showCorrections(m_doc, nullptr);
    QVERIFY(d != nullptr);
    QListWidget *list = d->findChild<QListWidget *>("venueCorrectionList");
    QCOMPARE(list->count(), 2);
    QVERIFY(list->item(0)->text().contains("pan 1.50"));
    list->setCurrentRow(0);
    d->findChild<QPushButton *>("venueCorrectionClear")->click();
    QCOMPARE(venues->correctionCount(), 1);
    QCOMPARE(list->count(), 1);
    QCOMPARE(props->fixtureRigProps(m_spots.at(0)).panOffsetDeg, 0.0f);       // the show's own
    QCOMPARE(props->fixtureRigProps(m_spots.at(1)).panOffsetDeg, 1.5f);
    d->close();
}

QTEST_MAIN(FocusCallsUi_Test)

void FocusCallsUi_Test::theFocusReportIsExported()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    venues->setShowFile(m_dir->filePath("Spring.qlcc"), false);
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    FocusCall call;
    call.groups << m_group;
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ call });

    FocusCallsDialog d(m_doc);
    QLineEdit *by = d.findChild<QLineEdit *>("fcCheckedBy");
    QPushButton *report = d.findChild<QPushButton *>("fcReport");
    QVERIFY(by != nullptr && report != nullptr);
    QVERIFY(report->isEnabled());
    by->clear();
    QTest::keyClicks(by, "Sam");
    QCOMPARE(venues->checkedBy(), QString("Sam"));
    QCOMPARE(QSettings().value("focusCalls/checkedBy").toString(), QString("Sam"));
    QVERIFY(venues->markChecked({ m_spots.first() }));

    // None picked: no venue, no report.
    QComboBox *combo = d.findChild<QComboBox *>("fcVenue");
    combo->setCurrentIndex(0);
    QVERIFY(!report->isEnabled());

    const QString pdf = m_dir->filePath("report.pdf");
    const QString html = m_dir->filePath("report.html");
    const QString csv = m_dir->filePath("report.csv");
    QString why;
    QVERIFY2(FocusCallsDialog::writeReport(m_doc, "Lincoln HS", pdf, &why), qPrintable(why));
    QVERIFY(FocusCallsDialog::writeReport(m_doc, "Lincoln HS", html, &why));
    QVERIFY(FocusCallsDialog::writeReport(m_doc, "Lincoln HS", csv, &why));
    QVERIFY(!FocusCallsDialog::writeReport(m_doc, "Nowhere", csv, &why));
    QVERIFY(why.contains("Nowhere"));
    auto read = [](const QString &path) {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
    QVERIFY(read(pdf).startsWith("%PDF"));
    QVERIFY(read(pdf).size() > 1000);
    const QByteArray page = read(html);
    QVERIFY(page.contains("Focus report: Lincoln HS"));
    QVERIFY(page.contains("Show: Spring"));
    QVERIFY(page.contains("Focused by: Sam"));
    const QByteArray table = read(csv);
    QVERIFY(table.startsWith(FocusReport::csvColumns().join(',').toUtf8()));
    QCOMPARE(table.count("\r\n"), 1 + 3);    // the header and the three spots
}

void FocusCallsUi_Test::aNewVenueStartsFromTheLast()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    MonitorProperties *props = m_doc->monitorProperties();
    venues->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    {
        FocusCallsDialog d(m_doc);
        QCheckBox *from = d.findChild<QCheckBox *>("fcFromLast");
        QVERIFY(from != nullptr);
        QVERIFY(from->isHidden());                  // no venue yet to start from
    }
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(m_spots.first());
    rp.panOffsetDeg = 4.0f;
    props->setFixtureRigProps(m_spots.first(), rp);

    FocusCallsDialog d(m_doc);
    QCheckBox *from = d.findChild<QCheckBox *>("fcFromLast");
    d.setVenueChoice("Jefferson MS");
    QVERIFY(!from->isHidden());
    QVERIFY(from->isChecked());
    QCOMPARE(from->text(), QString("Start from Lincoln HS"));
    QVERIFY(d.switchVenue("Jefferson MS"));
    QCOMPARE(venues->activeVenue(), QString("Jefferson MS"));
    QCOMPARE(props->fixtureRigProps(m_spots.first()).panOffsetDeg, 4.0f);
    // Unticked: the show's own.
    d.setVenueChoice("Kennedy");
    from->setChecked(false);
    QVERIFY(d.switchVenue("Kennedy"));
    QCOMPARE(props->fixtureRigProps(m_spots.first()).panOffsetDeg, 0.0f);
}

void FocusCallsUi_Test::practiceScramblesAndScores()
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    MonitorProperties *props = m_doc->monitorProperties();
    venues->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(venues->setActiveVenue(venues->addVenue("Lincoln HS")));
    FixtureRigProps rp = props->fixtureRigProps(m_spots.first());
    rp.panOffsetDeg = 1.5f;
    props->setFixtureRigProps(m_spots.first(), rp);
    const VenueCorrections::Venue real = venues->venue("Lincoln HS");
    FocusCall call;
    call.groups << m_group;
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ call });

    FocusCallsDialog d(m_doc);
    QPushButton *practice = d.findChild<QPushButton *>("fcPractice");
    QVERIFY(practice != nullptr && practice->isEnabled());
    QString why;
    m_doc->teachingSettings()->setEnabled(true);
    QVERIFY2(d.startPractice(2.0f, 6.0f, 7, &why), qPrintable(why));
    QVERIFY(d.isPractice());
    QVERIFY(venues->isPracticeActive());
    QVERIFY(venues->activeVenue().startsWith("Practice "));
    QCOMPARE(venues->venue(venues->activeVenue()).practice.truth.size(), 3);

    // The views: the light's right offsets, so its beam lands off its mark.
    FixtureRigProps view = props->fixtureRigProps(m_spots.first());
    QVERIFY(qAbs(view.panOffsetDeg - 1.5f) >= 1.9f);
    QVERIFY(practiceViewProps(m_doc, m_spots.first(), view));
    QCOMPARE(view.panOffsetDeg, 1.5f);
    QVector3D dir;
    QVERIFY(!focusIntentDirection(m_doc, m_spots.first(), dir));

    // The end: the score, and the real venue back as it was.
    QPointer<QDialog> result = FocusCallsDialog::showPracticeResult(m_doc, nullptr);
    QVERIFY(result != nullptr);
    QLabel *text = result->findChild<QLabel *>("practiceResultText");
    QVERIFY(text->text().contains("0 of 3 lights within 1"));
    QVERIFY(text->text().contains("Lincoln HS is back on the rig"));
    QCOMPARE(venues->activeVenue(), QString("Lincoln HS"));
    QCOMPARE(props->fixtureRigProps(m_spots.first()).panOffsetDeg, 1.5f);
    QCOMPARE(venues->venue("Lincoln HS").fixtures.size(), real.fixtures.size());
    QVERIFY(!practiceViewProps(m_doc, m_spots.first(), view));
    delete result.data();
    // Nothing to show with no practice applied.
    QVERIFY(FocusCallsDialog::showPracticeResult(m_doc, nullptr) == nullptr);
}

namespace
{
QAction *menuAction(QMenu *menu, const char *name)
{
    QAction *a = menu->findChild<QAction *>(QString::fromLatin1(name));
    if (a == nullptr)
        for (QAction *any : menu->actions())
            if (any->objectName() == QLatin1String(name))
                return any;
    return a;
}
}

void FocusCallsUi_Test::quickFocusSaysWhyStartIsGreyed()
{
    // No lights at all, no target: why, and never a blank box.
    Doc empty(this);
    {
        FocusModeSetupDialog d(&empty, 0xFFFFFFFF, {});
        QPushButton *start = d.findChild<QPushButton *>("focusStart");
        QLabel *reason = d.findChild<QLabel *>("focusStartReason");
        QComboBox *aim = d.findChild<QComboBox *>("focusReference");
        QVERIFY(start != nullptr && reason != nullptr && aim != nullptr);
        QVERIFY(!start->isEnabled());
        QCOMPARE(d.startBlocked(), QString("No moving heads are placed on the plot. Place them in the "
                                           "Lighting Studio, or choose Fixed lights."));
        QVERIFY(!reason->isHidden());
        QCOMPARE(reason->text(), d.startBlocked());
        QCOMPARE(start->toolTip(), d.startBlocked());
        QCOMPARE(aim->count(), 1);
        QCOMPARE(aim->currentText(), QString("No stage target yet"));
        // No way to place one from here (not opened from the Lighting Studio).
        QVERIFY(d.findChild<QPushButton *>("focusAddTarget")->isHidden());
    }

    // Moving heads on the plot but no target: that is the reason, and
    // Add a target (the Lighting Studio's) makes one: Start comes on.
    m_doc->monitorProperties()->removeStageTarget(m_target);
    FocusModeSetupDialog d(m_doc, 0xFFFFFFFF, {});
    QPushButton *start = d.findChild<QPushButton *>("focusStart");
    QPushButton *add = d.findChild<QPushButton *>("focusAddTarget");
    QComboBox *aim = d.findChild<QComboBox *>("focusReference");
    QVERIFY(!start->isEnabled());
    QCOMPARE(d.startBlocked(), QString("No stage target to aim at. Add one with Add \342\226\270 Add Target Position."));
    QCOMPARE(aim->currentText(), QString("No stage target yet"));
    int asked = 0;
    d.setAddTarget([this, &asked]() {
        ++asked;
        StageTarget *t = m_doc->monitorProperties()->addStageTarget();
        t->setName("Centre");
        return true;
    });
    QVERIFY(!add->isHidden());
    add->click();
    QCOMPARE(asked, 1);
    QCOMPARE(aim->currentText(), QString("Centre"));
    QVERIFY(d.startBlocked().isEmpty());
    QVERIFY(start->isEnabled());
    QVERIFY(d.findChild<QLabel *>("focusStartReason")->isHidden());
    QVERIFY(add->isHidden());
}

void FocusCallsUi_Test::theFocusMenuGreysWhatCannotBeUsed()
{
    QWidget parent;
    QMenu menu;
    int quick = 0;
    Doc empty(this);
    FocusMenu::fill(&menu, &empty, &parent, [&quick]() { ++quick; });
    QStringList texts;
    for (QAction *a : menu.actions())
        if (!a->isSeparator())
            texts << a->text();
    // Not a Teaching show: no scramble in the menu
    QCOMPARE(texts, QStringList({ "Quick focus\342\200\246", "Venue focus\342\200\246", "Edit focus calls\342\200\246",
                                  "Venues", "Focus report\342\200\246" }));
    empty.teachingSettings()->setEnabled(true);
    FocusMenu::fill(&menu, &empty, &parent, [&quick]() { ++quick; });
    texts.clear();
    for (QAction *a : menu.actions())
        if (!a->isSeparator())
            texts << a->text();
    QCOMPARE(texts, QStringList({ "Quick focus\342\200\246", "Venue focus\342\200\246", "Edit focus calls\342\200\246",
                                  "Venues", "Practice (scramble)\342\200\246", "Focus report\342\200\246" }));
    QVERIFY(menu.toolTipsVisible());

    // Nothing to focus: greyed, each with its reason as the tooltip.
    QAction *q = menuAction(&menu, "focusQuick");
    QVERIFY(!q->isEnabled());
    QCOMPARE(q->toolTip(), QString("No lights are placed on the plot. Place them in the Lighting Studio first."));
    QAction *v = menuAction(&menu, "focusVenue");
    QVERIFY(!v->isEnabled());
    QCOMPARE(v->toolTip(), QString("No focus calls yet. Add them with Focus \342\226\270 Edit focus calls\342\200\246"));
    QVERIFY(menuAction(&menu, "focusEditCalls")->isEnabled());   // always: that is where calls are made
    QVERIFY(!menuAction(&menu, "focusPractice")->isEnabled());
    QVERIFY(menuAction(&menu, "focusPractice")->toolTip().startsWith("No focus calls yet"));
    QVERIFY(!menuAction(&menu, "focusReport")->isEnabled());
    QVERIFY(menuAction(&menu, "focusReport")->toolTip().startsWith("No venue yet"));
    // Venues: the venue menu (Q150), the one the footer's chip opens.
    QMenu *venues = menuAction(&menu, "focusVenues")->menu();
    QVERIFY(venues != nullptr);
    QStringList vt;
    for (QAction *a : venues->actions())
        if (!a->isSeparator())
            vt << a->text();
    QCOMPARE(vt, QStringList({ "Switch venue", "New venue\342\200\246", "Rename\342\200\246",
                               "Rig and universes\342\200\246", "Venue focus\342\200\246",
                               "Focus report\342\200\246" }));
    QAction *newVenue = menuAction(venues, "venueNew");
    QVERIFY(!newVenue->isEnabled());
    QCOMPARE(newVenue->toolTip(), QString("Save the show first: a venue's corrections are kept in a file beside it."));
    QVERIFY(!menuAction(venues, "venueSwitch")->isEnabled());
    QVERIFY(!menuAction(venues, "venueRig")->isEnabled());
    QVERIFY(!menuAction(venues, "venueRename")->isEnabled());

    // The rig placed, calls made, the show saved, a venue applied: all on.
    VenueCorrections *vc = m_doc->programmer()->venueCorrections();
    vc->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    FocusCall call;
    call.groups << m_group;
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ call });
    QVERIFY(vc->setActiveVenue(vc->addVenue("Lincoln HS")));
    m_doc->teachingSettings()->setEnabled(true);
    FocusMenu::fill(&menu, m_doc, &parent, [&quick]() { ++quick; });
    for (const char *name : { "focusQuick", "focusVenue", "focusEditCalls", "focusPractice", "focusReport" })
        QVERIFY2(menuAction(&menu, name)->isEnabled(), name);
    QVERIFY(menuAction(&menu, "focusVenue")->toolTip().contains("Lincoln HS"));
    QVERIFY(menuAction(&menu, "focusPractice")->toolTip().contains("never touched"));
    venues = menuAction(&menu, "focusVenues")->menu();
    QVERIFY(menuAction(venues, "venueNew")->isEnabled());
    QVERIFY(menuAction(venues, "venueRig")->isEnabled());
    QVERIFY(menuAction(venues, "venueRename")->isEnabled());
    QAction *apply = menuAction(venues, "venueSwitch");
    QVERIFY(apply->isEnabled());
    QStringList at;
    for (QAction *a : apply->menu()->actions())
        at << QString("%1%2").arg(a->text(), a->isChecked() ? "*" : "");
    QCOMPARE(at, QStringList({ "None (the show's own calibration)", "Lincoln HS*" }));
    // Switch venue > None: the show's own, at once.
    apply->menu()->actions().first()->trigger();
    QVERIFY(!vc->isActive());
    // Filled again (each time it opens): one Switch submenu, not two.
    FocusMenu::fill(&menu, m_doc, &parent, [&quick]() { ++quick; });
    venues = menuAction(&menu, "focusVenues")->menu();
    QCOMPARE(venues->findChildren<QMenu *>(Qt::FindDirectChildrenOnly).size(), 1);

    // Quick focus is the Lighting Studio's own (its selection and scene).
    menuAction(&menu, "focusQuick")->trigger();
    QCOMPARE(quick, 1);
}

void FocusCallsUi_Test::focusEntriesElsewhereSayWhy()
{
    // The Run tab: Venue focus, greyed with why until there are calls.
    CueRunner runner(m_doc, CueRunner::Full);
    QToolButton *venue = runner.findChild<QToolButton *>("crFocusCalls");
    QVERIFY(venue != nullptr);
    QCOMPARE(venue->text(), QString("Venue focus\342\200\246"));
    QVERIFY(!venue->isEnabled());
    QVERIFY(venue->toolTip().startsWith("No focus calls yet"));
    FocusCall call;
    call.groups << m_group;
    call.target.kind = FocusSession::Reference::Target;
    call.target.id = m_target;
    m_doc->programmer()->focusCalls()->setCalls({ call });
    QVERIFY(venue->isEnabled());
    QVERIFY(venue->toolTip().startsWith("Pre-show"));
    m_doc->programmer()->focusCalls()->setCalls({});
    QVERIFY(!venue->isEnabled());

    // The Show Tree cue page: Focus in its header, the tick box renamed.
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("Spring"), "Main");
    const quint32 cue = st->addCue(list, -1, "Top");
    ShowTreeView view(nullptr, m_doc);
    QVERIFY(view.selectNode(cue));
    QToolButton *focus = view.findChild<QToolButton *>("stCueFocusMenu");
    QVERIFY(focus != nullptr);
    QCOMPARE(focus->text(), QString("Focus"));
    QCheckBox *tick = view.findChild<QCheckBox *>("stCueFocusCall");
    QCOMPARE(tick->text(), QString("Make this a venue focus cue"));
    QVERIFY(!tick->isEnabled());     // no calls for it to run
    QVERIFY(tick->toolTip().startsWith("No focus calls yet"));
    QMenu *m = focus->menu();
    emit m->aboutToShow();
    QStringList texts;
    for (QAction *a : m->actions())
        if (!a->isSeparator())
            texts << a->text();
    QCOMPARE(texts, QStringList({ "Focus this cue\342\200\246", "Make this a venue focus cue", "Venue focus\342\200\246" }));
    QVERIFY(menuAction(m, "stCueFocusThis")->isEnabled());
    QVERIFY(!menuAction(m, "stCueFocusVenueCue")->isEnabled());
    QVERIFY(!menuAction(m, "stCueVenueFocus")->isEnabled());
    // With calls: it can be made the venue focus cue from the header.
    m_doc->programmer()->focusCalls()->setCalls({ call });
    emit m->aboutToShow();
    QAction *make = menuAction(m, "stCueFocusVenueCue");
    QVERIFY(make->isEnabled() && !make->isChecked());
    make->trigger();
    QVERIFY(st->cue(cue)->focusCall);
    QCOMPARE(st->undoText(), QString("Make cue a venue focus cue"));

    // No lights in the show: Focus this cue says why.
    Doc empty(this);
    ShowTreeController *est = empty.showTree();
    const quint32 eList = est->addCueList(est->addProduction("Spring"), "Main");
    const quint32 eCue = est->addCue(eList, -1, "Top");
    ShowTreeView eview(nullptr, &empty);
    QVERIFY(eview.selectNode(eCue));
    QMenu *em = eview.findChild<QToolButton *>("stCueFocusMenu")->menu();
    emit em->aboutToShow();
    QAction *self = menuAction(em, "stCueFocusThis");
    QVERIFY(!self->isEnabled());
    QCOMPARE(self->toolTip(), QString("No lights in the show yet to focus. Add fixtures first."));
}

void FocusCallsUi_Test::practiceExplainsTheScramble()
{
    const QString text = FocusCallsDialog::practiceExplanation();
    for (const char *says : { "knocks every moving head", "off its target by a random amount",
                              "focus them back", "degrees off", "how long it takes", "saves",
                              "real venues are never touched" })
        QVERIFY2(text.contains(QLatin1String(says)), says);
    // Only a Teaching show lists the scramble (File > Show Settings)
    {
        m_doc->teachingSettings()->setEnabled(false);
        FocusCallsDialog off(m_doc);
        QVERIFY(off.findChild<QPushButton *>("fcPractice")->isHidden());
    }
    m_doc->teachingSettings()->setEnabled(true);
    FocusCallsDialog d(m_doc);
    QVERIFY(!d.findChild<QPushButton *>("fcPractice")->isHidden());
    QCOMPARE(d.windowTitle(), QString("Venue Focus"));
    QCOMPARE(d.findChild<QPushButton *>("fcPractice")->toolTip(), text);
    QCOMPARE(d.findChild<QPushButton *>("fcStart")->text(), QString("Start venue focus"));
}

void FocusCallsUi_Test::voiceDetailIsOneSettingOnEveryPanel()
{
    ProgrammerController *pc = m_doc->programmer();
    FocusSession::Options o;
    o.fixtures = m_spots;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = m_target;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(m_doc, nullptr, o);
    QVERIFY(panel != nullptr);
    QComboBox *detail = panel->findChild<QComboBox *>("voiceDetail");
    QVERIFY(detail != nullptr);
    QCOMPARE(detail->count(), 3);
    QCOMPARE(detail->currentData().toInt(), int(ControllerScheme::StandardVoice));
    detail->setCurrentIndex(detail->findData(int(ControllerScheme::TerseVoice)));
    QCOMPARE(pc->voiceDetail(), ControllerScheme::TerseVoice);
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    pc->focusSession()->next();
    QCOMPARE(said.last().at(0).toString(), QString("F2 T2."));
    // The panel's line shows what was confirmed, as it is said.
    pc->focusSession()->nudge(0.5f, 0.0f);
    pc->focusSession()->saveAllHeld();
    QCOMPARE(panel->findChild<QLabel *>("focusConfirmed")->text(), QString("Saved all."));
    pc->setVoiceDetail(ControllerScheme::VerboseVoice);
    QCOMPARE(detail->currentData().toInt(), int(ControllerScheme::VerboseVoice));
    // Kept with Feedback, this computer's.
    VoiceDetailCombo::saveDetail(int(ControllerScheme::VerboseVoice));
    QCOMPARE(VoiceDetailCombo::savedDetail(), int(ControllerScheme::VerboseVoice));
    VoiceDetailCombo::saveDetail(int(ControllerScheme::StandardVoice));
    pc->setVoiceDetail(ControllerScheme::StandardVoice);
}

void FocusCallsUi_Test::lightsThatCannotBeAimedAreListed()
{
    // A plain dimmer selected with the spots: listed at Start, left out by default.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("UI Calls Dimmer");
    def->setType(QLCFixtureDef::Dimmer);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("1ch");
    QLCChannel *ch = new QLCChannel();
    ch->setName("Dimmer");
    ch->setGroup(QLCChannel::Intensity);
    def->addChannel(ch);
    mode->insertChannel(ch, 0);
    def->addMode(mode);
    Fixture *dim = new Fixture(m_doc);
    dim->setName("Worklight");
    dim->setFixtureDefinition(def, mode);
    dim->setUniverse(0);
    dim->setAddress(100);
    m_doc->addFixture(dim);

    FocusModeSetupDialog d(m_doc, 0xFFFFFFFF, { m_spots.first(), dim->id() });
    QCOMPARE(d.unfocusable(), QList<quint32>({ dim->id() }));
    QVERIFY(d.exclusionText().contains("Worklight"));
    QVERIFY(d.exclusionText().contains("cannot be aimed"));
    QCOMPARE(d.options().fixtures, QList<quint32>({ m_spots.first() }));
    // Start: the warning, Exclude them the default.
    QTimer::singleShot(0, &d, []() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(box != nullptr);
        QCOMPARE(box->objectName(), QString("focusExcludeBox"));
        QCOMPARE(box->defaultButton()->objectName(), QString("focusExclude"));
        box->defaultButton()->click();
    });
    QVERIFY(d.confirmExclusions());
    QTimer::singleShot(0, &d, []() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            box->button(QMessageBox::Cancel)->click();
    });
    QVERIFY(!d.confirmExclusions());
}

namespace
{
QStringList menuTexts(QMenu *menu)
{
    QStringList out;
    for (QAction *a : menu->actions())
    {
        if (!a->isSeparator())
        {
            out << QString("%1%2").arg(a->text(), a->isEnabled() ? "" : " (off)");
        }
    }
    return out;
}

/** The dialog named @p name open now (not modal: the venue menu's). */
template <class T> T *openDialog(const char *name)
{
    for (QWidget *w : QApplication::topLevelWidgets())
    {
        if (w->objectName() == QLatin1String(name) && w->isVisible())
        {
            if (T *t = qobject_cast<T *>(w))
            {
                return t;
            }
        }
    }
    return nullptr;
}
}

void FocusCallsUi_Test::theVenueChipSaysTheVenueAndOpensItsMenu()
{
    QWidget window;
    VenueChip chip(m_doc, &window);
    VenueCorrections *vc = m_doc->programmer()->venueCorrections();

    // No venue: "none", and (in the qlcconsole views) shown all the same.
    QCOMPARE(chip.text(), QString("Venue: none"));
    QVERIFY(chip.wanted());
    QVERIFY(!chip.isAmber());
    // Classic: only once the show has venues.
    chip.setAlwaysShown(false);
    QVERIFY(!chip.wanted());

    // Its menu, as things are: nothing saved, so nothing to switch to.
    QPointer<QMenu> menu = chip.openMenu();
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->isVisible());
    QCOMPARE(menuTexts(menu), QStringList({ "Switch venue (off)", "New venue\342\200\246 (off)",
                                            "Rename\342\200\246 (off)", "Rig and universes\342\200\246 (off)",
                                            "Venue focus\342\200\246 (off)", "Focus report\342\200\246 (off)" }));
    menu->close();

    // Saved, two venues, Lincoln HS applied: the chip says so at once.
    vc->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    vc->addVenue("Kennedy");
    QVERIFY(chip.wanted());                     // Classic too, now there are venues
    QVERIFY(vc->setActiveVenue(vc->addVenue("Lincoln HS")));
    QCOMPARE(chip.text(), QString("Venue: Lincoln HS"));
    QVERIFY(chip.toolTip().contains("Lincoln HS is applied"));
    menu = chip.openMenu();
    QStringList texts = menuTexts(menu);
    QCOMPARE(texts, QStringList({ "Switch venue", "New venue\342\200\246", "Rename\342\200\246",
                                  "Rig and universes\342\200\246", "Venue focus\342\200\246 (off)",
                                  "Focus report\342\200\246" }));
    QMenu *sw = menuAction(menu, "venueSwitch")->menu();
    QStringList names;
    for (QAction *a : sw->actions())
        names << QString("%1%2").arg(a->text(), a->isChecked() ? "*" : "");
    QCOMPARE(names, QStringList({ "None (the show's own calibration)", "Kennedy", "Lincoln HS*" }));

    // Switched from the chip: applied, and the chip follows.
    sw->actions().at(1)->trigger();
    QCOMPARE(vc->activeVenue(), QString("Kennedy"));
    QCOMPARE(chip.text(), QString("Venue: Kennedy"));
    menu->close();

    // Renamed from the chip (its own small dialog, not modal).
    chip.openMenu();
    menuAction(chip.menu(), "venueRename")->trigger();
    QPointer<QInputDialog> ask = openDialog<QInputDialog>("venueRenameName");
    QVERIFY(ask != nullptr);
    QCOMPARE(ask->textValue(), QString("Kennedy"));
    ask->setTextValue("Kennedy Center");
    ask->accept();
    QTRY_VERIFY(ask.isNull());
    QCOMPARE(vc->activeVenue(), QString("Kennedy Center"));
    QCOMPARE(chip.text(), QString("Venue: Kennedy Center"));
    chip.menu()->close();

    // A new venue from the chip: made, and applied.
    chip.openMenu();
    menuAction(chip.menu(), "venueNew")->trigger();
    ask = openDialog<QInputDialog>("venueNewName");
    QVERIFY(ask != nullptr);
    ask->setTextValue("Roosevelt");
    ask->accept();
    QTRY_VERIFY(ask.isNull());
    QCOMPARE(vc->activeVenue(), QString("Roosevelt"));
    QCOMPARE(chip.text(), QString("Venue: Roosevelt"));
    chip.menu()->close();

    // Removed (the venue manager): back to none.
    QVERIFY(vc->removeVenue("Roosevelt"));
    QCOMPARE(chip.text(), QString("Venue: none"));

    // None from the chip's menu.
    QVERIFY(vc->setActiveVenue("Lincoln HS"));
    chip.openMenu();
    menuAction(chip.menu(), "venueSwitch")->menu()->actions().first()->trigger();
    QVERIFY(!vc->isActive());
    QCOMPARE(chip.text(), QString("Venue: none"));
    chip.menu()->close();
}

void FocusCallsUi_Test::theVenueChipTurnsAmberWithItsClashes()
{
    QWidget window;
    VenueChip chip(m_doc, &window);
    VenueCorrections *vc = m_doc->programmer()->venueCorrections();
    vc->setShowFile(m_dir->filePath("Tour.qlcc"), false);
    QVERIFY(vc->setActiveVenue(vc->addVenue("Lincoln HS")));
    QVERIFY(!chip.isAmber());
    QVERIFY(menuAction(chip.openMenu(), "venueClashes") == nullptr);   // none: not offered
    chip.menu()->close();

    // The room keeps U1's first 8 channels: our spots are on them.
    VenueRig::NotOurs keep;
    keep.universe = 0;
    keep.from = 0;
    keep.to = 7;
    QVERIFY(vc->setNotOurs("Lincoln HS", { keep }));
    const int n = vc->clashes("Lincoln HS").size();
    QVERIFY(n > 0);
    QCOMPARE(chip.clashes(), n);
    QVERIFY(chip.isAmber());
    QVERIFY2(chip.text().startsWith("Venue: Lincoln HS \302\267 "), qPrintable(chip.text()));
    QVERIFY(chip.text().endsWith(n == 1 ? "1 clash" : QString("%1 clashes").arg(n)));
    QVERIFY(chip.styleSheet().contains("#f5a623"));
    QAction *list = menuAction(chip.openMenu(), "venueClashes");
    QVERIFY(list != nullptr);
    QCOMPARE(list->text(), QString("Clashes (%1)\342\200\246").arg(n));
    // It opens the venue page, its clash list in front.
    list->trigger();
    QPointer<VenueRigDialog> page = openDialog<VenueRigDialog>("VenueRigDialog");
    QVERIFY(page != nullptr);
    QCOMPARE(page->clashList()->count(), n);
    page->close();
    QTRY_VERIFY(page.isNull());
    chip.menu()->close();

    // A spot moved off those channels: one fewer, a moment later.
    m_doc->fixture(m_spots.last())->setAddress(100);
    QTRY_COMPARE(chip.clashes(), vc->clashes("Lincoln HS").size());
    QVERIFY(chip.clashes() < n);

    // The room keeps nothing: plain again.
    QVERIFY(vc->setNotOurs("Lincoln HS", {}));
    QCOMPARE(chip.clashes(), 0);
    QVERIFY(!chip.isAmber());
    QCOMPARE(chip.text(), QString("Venue: Lincoln HS"));
}

void FocusCallsUi_Test::theVenueChipAsksStillAt()
{
    // A show whose applied venue was last changed three days ago.
    const QString show = m_dir->filePath("Old.qlcc");
    QFile f(VenueCorrections::sidecarPath(show));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QString("<QlcVenues Version=\"1\" Show=\"Old.qlcc\" Active=\"Lincoln HS\">"
                    "<Venue Name=\"Lincoln HS\" Modified=\"%1\"/></QlcVenues>")
            .arg(QDateTime::currentDateTime().addDays(-3).toString(Qt::ISODate)).toUtf8());
    f.close();
    QWidget window;
    VenueChip chip(m_doc, &window);
    VenueCorrections *vc = m_doc->programmer()->venueCorrections();
    vc->setShowFile(show, true);
    chip.refresh();
    QVERIFY(vc->needsConfirm());
    QCOMPARE(chip.text(), QString("Venue: Lincoln HS?"));
    QVERIFY(chip.toolTip().contains("are you still there?"));
    QMenu *menu = chip.openMenu();
    QAction *still = menuAction(menu, "venueStillAt");
    QVERIFY(still != nullptr);
    QCOMPARE(menu->actions().first(), still);
    QCOMPARE(still->text(), QString("Still at Lincoln HS?\342\200\246"));
    menu->close();

    // Answered (the prompt on opening, or from the menu): on the chip.
    still->trigger();
    QPointer<QDialog> d = openDialog<QDialog>("venueConfirm");
    QVERIFY(d != nullptr);
    d->findChild<QPushButton *>("venueConfirmStill")->click();
    QTRY_VERIFY(d.isNull());
    QCOMPARE(chip.text(), QString("Venue: Lincoln HS"));
    QVERIFY(chip.toolTip().contains("Confirmed: you are at Lincoln HS."));
    QVERIFY(menuAction(chip.openMenu(), "venueStillAt") == nullptr);
    chip.menu()->close();
}
