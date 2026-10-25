/*
  Q Light Controller Plus
  focusmode_test.cpp

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

#include <QtTest>
#include <QSignalSpy>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QStandardItemModel>
#include <QLabel>
#include <QPointer>
#include <QSettings>
#include <QClipboard>
#include <QTreeWidget>
#include <QMessageBox>

#include "focusmode_test.h"
#include "iopluginstub.h"
#include "qlcfile.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "controllertestdialog.h"
#include "controllerkeepalive.h"
#include "focuscontrolmap.h"
#include "focusmodepanel.h"
#include "focussession.h"
#include "gamepadrole.h"
#include "qlcphysical.h"
#include "stagetarget.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "app.h"
#include "doc.h"

using R = GamepadRole::Role;

namespace
{
/* A 540/270 spot: pan, tilt, dimmer, free at (x, y) metres, hung at 5 m. */
Fixture *makeSpot(Doc *doc, const QString &name, float x, float y)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus Map Spot " + name);
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("3ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity };
    const char *names[] = { "Pan", "Tilt", "Dimmer" };
    for (int k = 0; k < 3; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    mode->setPhysical(ph);
    QLCFixtureHead head;
    head.addChannel(0); head.addChannel(1); head.addChannel(2);
    mode->insertHead(-1, head);
    def->addMode(mode);

    Fixture *fxi = new Fixture(doc);
    fxi->setName(name);
    fxi->setFixtureDefinition(def, mode);
    fxi->setUniverse(0);
    fxi->setAddress(quint32(doc->fixtures().size() * 3));
    if (doc->addFixture(fxi) == false)
        return nullptr;
    doc->monitorProperties()->setFixturePosition(fxi->id(), 0, 0,
                                                 QVector3D(x * 1000.0f, y * 1000.0f, 5.0f));
    return fxi;
}

/* The gamepad's channels: the four stick axes, then the buttons. */
enum Ch : quint32
{
    LX = 0, LY = 1, RX = 2, RY = 3,
    BtnA = 10, BtnB = 11, BtnX = 12, BtnY = 13, BtnLB = 14, BtnRB = 15,
    BtnView = 16, BtnMenu = 17, Up = 18, Down = 19, Left = 20, Right = 21
};
}

void FocusMode_Test::initTestCase()
{
    m_doc = new Doc(this);
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    m_stub = static_cast<IOPluginStub *>(
        m_doc->ioPluginCache()->plugin(QStringLiteral("I/O Plugin Stub")));
    QVERIFY2(m_stub != nullptr, "the I/O plugin stub did not load");
    QVERIFY(m_doc->inputOutputMap()->setInputPatch(0, m_stub->name(), QString(), 0));

    Fixture *a = makeSpot(m_doc, "A", 2.0f, 2.0f);
    Fixture *b = makeSpot(m_doc, "B", 4.0f, 2.0f);
    QVERIFY(a != nullptr && b != nullptr);
    m_heads = { a->id(), b->id() };
    StageTarget *t = m_doc->monitorProperties()->addStageTarget();
    t->setName(QStringLiteral("Centre"));
    t->setPosition(QVector3D(3.0f, 5.0f, 0.0f));
    m_target = t->id();
}

void FocusMode_Test::init()
{
    QSettings().remove(QStringLiteral("focusmode/input"));
    QSettings().remove(QStringLiteral("focusmode/inputDevice"));
    m_doc->programmer()->focusSession()->setStepDegrees(0.1f);   // + in a test before
    unplugPad();
}

void FocusMode_Test::cleanup()
{
    if (FocusModePanel *panel = FocusModePanel::instance())
    {
        QPointer<FocusModePanel> guard(panel);
        m_doc->programmer()->focusSession()->discardAll();
        panel->requestExit();
        QTRY_VERIFY(guard.isNull());
    }
    QVERIFY(!m_doc->programmer()->focusSession()->isActive());
}

void FocusMode_Test::cleanupTestCase()
{
    unplugPad();
    delete m_doc;
    m_doc = nullptr;
}

void FocusMode_Test::plugInPad()
{
    m_stub->m_channelRoles = { { LX, "lx" }, { LY, "ly" }, { RX, "rx" }, { RY, "ry" },
                               { BtnA, "a" }, { BtnB, "b" }, { BtnX, "x" }, { BtnY, "y" },
                               { BtnLB, "lb" }, { BtnRB, "rb" }, { BtnView, "back" },
                               { BtnMenu, "start" }, { Up, "dpad_up" }, { Down, "dpad_down" },
                               { Left, "dpad_left" }, { Right, "dpad_right" } };
    m_stub->m_channelNames = { { LX, "Left Stick X" }, { LY, "Left Stick Y" },
                               { RX, "Right Stick X" }, { RY, "Right Stick Y" },
                               { BtnA, "A" }, { BtnB, "B" }, { BtnX, "X" }, { BtnY, "Y" },
                               { BtnLB, "LB" }, { BtnRB, "RB" }, { BtnView, "View" },
                               { BtnMenu, "Menu" }, { Up, "D-pad Up" }, { Down, "D-pad Down" },
                               { Left, "D-pad Left" }, { Right, "D-pad Right" } };
    m_doc->programmer()->autoBindFromProfile();
    emit m_doc->inputOutputMap()->pluginConfigurationChanged(m_stub->name(), true);
}

void FocusMode_Test::unplugPad()
{
    if (m_stub == nullptr)
        return;
    m_stub->m_channelRoles.clear();
    m_stub->m_channelNames.clear();
    emit m_doc->inputOutputMap()->pluginConfigurationChanged(m_stub->name(), true);
}

static FocusSession::Options optionsFor(const QList<quint32> &heads, quint32 target)
{
    FocusSession::Options o;
    o.fixtures = heads;
    o.first.kind = FocusSession::Reference::Target;
    o.first.id = target;
    return o;
}

void FocusMode_Test::theMapShowsTheProfilesBindings()
{
    /* Another maker's pad: its own names, roles saying what they are, and
       no Y (triangle) in this profile. */
    m_stub->m_channelRoles = { { LX, "lx" }, { LY, "ly" }, { RX, "rx" }, { RY, "ry" },
                               { BtnA, "a" }, { BtnB, "b" }, { BtnX, "x" },
                               { BtnLB, "lb" }, { BtnRB, "rb" }, { BtnView, "back" },
                               { BtnMenu, "start" }, { Up, "dpad_up" }, { Down, "dpad_down" },
                               { Left, "dpad_left" }, { Right, "dpad_right" } };
    m_stub->m_channelNames = { { BtnA, "Cross" }, { BtnB, "Circle" }, { BtnX, "Square" },
                               { BtnLB, "L1" }, { BtnRB, "R1" }, { BtnView, "Share" },
                               { BtnMenu, "Options" } };
    m_doc->programmer()->autoBindFromProfile();

    const ProgrammerController::FocusController pad = m_doc->programmer()->focusController();
    QVERIFY(pad.present);
    QCOMPARE(pad.universe, 0u);
    QCOMPARE(pad.controls.value(R::A), QString("Cross"));
    QVERIFY(!pad.controls.contains(R::Y));

    QString why;
    QPointer<FocusModePanel> panel =
        FocusModePanel::startWith(m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusControlMap *map = panel->controlMap();
    QVERIFY(map != nullptr);
    QCOMPARE(map->mode(), FocusControlMap::ControllerMap);

    // Each control: what it does here, by its role, under its own name.
    QCOMPARE(map->actionText(R::A), QString("Save"));
    QCOMPARE(map->controlName(R::A), QString("Cross"));
    QCOMPARE(map->actionText(R::RB), QString("Next fixture"));
    QCOMPARE(map->controlName(R::RB), QString("R1"));
    QCOMPARE(map->actionText(R::LB), QString("Previous fixture"));
    QCOMPARE(map->actionText(R::B), QString("Revert"));
    QCOMPARE(map->actionText(R::X), QString("Zero"));
    QCOMPARE(map->actionText(R::Back), QString("Undo save"));
    QCOMPARE(map->controlName(R::Back), QString("Share"));
    QCOMPARE(map->actionText(R::Start), QString("Exit"));
    QCOMPARE(map->actionText(R::LeftX), QString("Nudge"));
    QCOMPARE(map->actionText(R::RightX), QString("Fine nudge"));
    QVERIFY2(map->actionText(R::DpadUp).contains("Spot down"), qPrintable(map->actionText(R::DpadUp)));   // Q103
    // Named by role only: the usual name.
    QCOMPARE(map->controlName(R::DpadLeft), QString("D-pad left"));

    // What the profile does not have is greyed; what Focus mode does not
    // use says nothing.
    QVERIFY(map->isMapped(R::A));
    QVERIFY(!map->isMapped(R::Y));
    QCOMPARE(map->actionText(R::Y), QString("Scope: group \342\206\224 fixture"));
    QCOMPARE(map->actionText(R::LS), QString("Second reference"));
    QVERIFY(!map->isMapped(R::LeftTrigger));
    QVERIFY(map->actionText(R::LeftTrigger).isEmpty());

    // The step the arrow keys take follows + and -.
    QTest::keyClick(panel, Qt::Key_Plus);
    QVERIFY2(map->keyAction(Qt::Key_Up).contains("1\302\260"), qPrintable(map->keyAction(Qt::Key_Up)));
    QVERIFY(map->grab().width() > 0);   // and it paints
}

void FocusMode_Test::theDefaultFollowsTheDevice()
{
    // Nothing connected: the keyboard, and the controller not choosable.
    QCOMPARE(FocusModePanel::defaultInput(m_doc), FocusSession::KeyboardInput);
    {
        FocusModeSetupDialog setup(m_doc, Function::invalidId(), m_heads);
        QComboBox *use = setup.findChild<QComboBox *>("focusInput");
        QVERIFY(use != nullptr);
        QCOMPARE(setup.inputDevice(), FocusSession::KeyboardInput);
        QStandardItemModel *m = qobject_cast<QStandardItemModel *>(use->model());
        QVERIFY(m != nullptr);
        QVERIFY(!m->item(use->findData(int(FocusSession::ControllerInput)))->isEnabled());
    }
    QString why;
    QPointer<FocusModePanel> panel =
        FocusModePanel::startWith(m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusSession *s = m_doc->programmer()->focusSession();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::KeyboardMap);
    QToolButton *useController = panel->findChild<QToolButton *>("focusUseController");
    QVERIFY(useController != nullptr);
    QVERIFY(!useController->isEnabled());
    // Asked for the controller with none there: still the keyboard.
    panel->setInputDevice(FocusSession::ControllerInput);
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    panel->requestExit();
    QTRY_VERIFY(panel.isNull());

    // A controller connected: the controller.
    plugInPad();
    QCOMPARE(FocusModePanel::defaultInput(m_doc), FocusSession::ControllerInput);
    {
        FocusModeSetupDialog setup(m_doc, Function::invalidId(), m_heads);
        QCOMPARE(setup.inputDevice(), FocusSession::ControllerInput);
    }
    panel = FocusModePanel::startWith(m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::ControllerMap);
    QVERIFY(panel->findChild<QToolButton *>("focusUseController")->isChecked());

    // Choosing the keyboard is remembered: next time it is the keyboard,
    // controller or not.
    panel->findChild<QToolButton *>("focusUseKeyboard")->click();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::KeyboardMap);
    QCOMPARE(QSettings().value("focusmode/inputDevice").toString(), QString("keyboard"));
    QCOMPARE(FocusModePanel::defaultInput(m_doc), FocusSession::KeyboardInput);
    panel->findChild<QToolButton *>("focusUseController")->click();
    QCOMPARE(FocusModePanel::defaultInput(m_doc), FocusSession::ControllerInput);
}

void FocusMode_Test::theKeyboardIgnoresTheController()
{
    plugInPad();
    InputOutputMap *iom = m_doc->inputOutputMap();
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();

    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    const quint32 first = s->currentFixture();

    QSignalSpy exitAsked(s, &FocusSession::exitRequested);
    QSignalSpy actions(pc, &ProgrammerController::buttonActionTriggered);
    auto press = [iom](quint32 ch) {
        emit iom->inputValueChanged(0, ch, 255, QString());
        emit iom->inputValueChanged(0, ch, 0, QString());
    };
    // A resting controller, knocked: nothing moves, nothing steps or saves.
    press(Right);
    press(BtnA);
    press(BtnRB);
    press(BtnMenu);
    emit iom->inputValueChanged(0, LX, 255, QString());
    emit iom->inputValueChanged(0, RY, 0, QString());
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    QCOMPARE(s->currentFixture(), first);
    QCOMPARE(s->liveOffset(first), FocusSession::Offset());
    QVERIFY(!s->isChecked(first));
    QCOMPARE(exitAsked.count(), 0);
    QCOMPARE(actions.count(), 0);      // and nothing else heard it either
    QVERIFY(!panel->controlMap()->isLit(R::A));

    // The keys still work.
    QTest::keyClick(panel, Qt::Key_Right);
    QCOMPARE(s->liveOffset(first).pan, 0.1f);

    // On the controller, the stick moves it (the D-pad is zoom and focus).
    emit iom->inputValueChanged(0, LX, 128, QString());
    emit iom->inputValueChanged(0, RY, 128, QString());
    panel->setInputDevice(FocusSession::ControllerInput);
    emit iom->inputValueChanged(0, LX, 255, QString());
    for (int i = 0; i < 5; ++i)
        pc->applyDesignJoystick();
    emit iom->inputValueChanged(0, LX, 128, QString());
    QVERIFY(s->liveOffset(first).pan > 0.1f);
}

/* The changelog (Q140, Q103): H holds every light (and again lets go),
   and on the keyboard , . spot down / flood out the zoom and ; ' make the
   focus softer / sharper -- keys on the Focus mode panel. */
void FocusMode_Test::theKeysHoldZoomAndFocus()
{
    // A wash with a 10-40 degree zoom channel and a focus channel.
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test"); def->setModel("Focus Map Zoom Wash");
    def->setType(QLCFixtureDef::MovingHead);
    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("5ch");
    const QLCChannel::Group groups[] = { QLCChannel::Pan, QLCChannel::Tilt, QLCChannel::Intensity,
                                         QLCChannel::Beam, QLCChannel::Beam };
    const QLCChannel::Preset presets[] = { QLCChannel::Custom, QLCChannel::Custom, QLCChannel::Custom,
                                           QLCChannel::BeamZoomSmallBig, QLCChannel::BeamFocusNearFar };
    const char *names[] = { "Pan", "Tilt", "Dimmer", "Zoom", "Focus" };
    QLCFixtureHead head;
    for (int k = 0; k < 5; ++k)
    {
        QLCChannel *ch = new QLCChannel();
        ch->setName(names[k]);
        ch->setGroup(groups[k]);
        ch->setControlByte(QLCChannel::MSB);
        if (presets[k] != QLCChannel::Custom)
            ch->setPreset(presets[k]);
        def->addChannel(ch);
        mode->insertChannel(ch, k);
        head.addChannel(k);
    }
    QLCPhysical ph;
    ph.setFocusPanMax(540);
    ph.setFocusTiltMax(270);
    ph.setLensDegreesMin(10.0);
    ph.setLensDegreesMax(40.0);
    mode->setPhysical(ph);
    mode->insertHead(-1, head);
    def->addMode(mode);
    Fixture *wash = new Fixture(m_doc);
    wash->setName("Wash 1");
    wash->setFixtureDefinition(def, mode);
    wash->setUniverse(0);
    wash->setAddress(200);
    QVERIFY(m_doc->addFixture(wash));
    const quint32 washId = wash->id();
    m_doc->monitorProperties()->setFixturePosition(washId, 0, 0, QVector3D(3000.0f, 2000.0f, 5.0f));

    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor({ washId }, m_target), &why, int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    QCOMPARE(s->beamKind(washId), FocusSession::ZoomChannel);
    QVERIFY(s->hasFocusControl(washId));

    // The beam: , narrower (spot down), . wider (flood out).
    double deg = 0.0, lo = 0.0, hi = 0.0;
    QVERIFY(s->beamAngle(washId, deg, lo, hi));
    const double startDeg = deg;
    QTest::keyClick(panel, Qt::Key_Comma);
    QVERIFY(s->beamAngle(washId, deg, lo, hi));
    QCOMPARE(deg, startDeg - 1.0);
    QTest::keyClick(panel, Qt::Key_Period);
    QTest::keyClick(panel, Qt::Key_Period);
    QVERIFY(s->beamAngle(washId, deg, lo, hi));
    QCOMPARE(deg, startDeg + 1.0 > hi ? hi : startDeg + 1.0);
    QTest::keyClick(panel, Qt::Key_Comma, Qt::ShiftModifier);      // 5 at a time
    QVERIFY(s->beamAngle(washId, deg, lo, hi));
    QVERIFY(deg < startDeg - 3.0);
    const uchar zoomSent = s->pendingWrites().value(qMakePair(0, 203));
    QCOMPARE(int(zoomSent), qRound((deg - lo) / (hi - lo) * 255.0));

    // The focus: ' sharper, ; softer, from where it is.
    double focus = 0.0;
    QTest::keyClick(panel, Qt::Key_Apostrophe);
    QVERIFY(s->focusLevel(washId, focus));
    const double sharper = focus;
    QTest::keyClick(panel, Qt::Key_Semicolon);
    QTest::keyClick(panel, Qt::Key_Semicolon);
    QVERIFY(s->focusLevel(washId, focus));
    QVERIFY(qAbs(focus - (sharper - 0.04)) < 1e-6);
    QCOMPARE(int(s->pendingWrites().value(qMakePair(0, 204))), qRound(focus * 255.0));

    // H: every light holds -- the keys move nothing -- and H again lets go.
    QSignalSpy holding(pc, &ProgrammerController::holdingChanged);
    QTest::keyClick(panel, Qt::Key_H);
    QVERIFY(pc->isHolding());
    QVERIFY(s->isHolding());
    QCOMPARE(holding.count(), 1);
    // Said in the title, with a real dash (it was written twice-encoded:
    // "HOLDING \u00e2\u0080\u0094 click...").
    QString banner;
    foreach (QLabel *l, panel->findChildren<QLabel *>())
        if (l->text().startsWith(QLatin1String("HOLDING")))
            banner = l->text();
    QVERIFY2(banner.startsWith(QString::fromUtf8("HOLDING \u2014 click both sticks")), qPrintable(banner));
    const FocusSession::Offset before = s->liveOffset(washId);
    double heldDeg = 0.0, heldFocus = 0.0;
    QVERIFY(s->beamAngle(washId, heldDeg, lo, hi));
    QVERIFY(s->focusLevel(washId, heldFocus));
    QTest::keyClick(panel, Qt::Key_Right);
    QTest::keyClick(panel, Qt::Key_Period);
    QTest::keyClick(panel, Qt::Key_Apostrophe);
    QCOMPARE(s->liveOffset(washId), before);
    QVERIFY(s->beamAngle(washId, deg, lo, hi));
    QCOMPARE(deg, heldDeg);
    QVERIFY(s->focusLevel(washId, focus));
    QCOMPARE(focus, heldFocus);
    QTest::keyClick(panel, Qt::Key_H);
    QVERIFY(!pc->isHolding());
    QVERIFY(!s->isHolding());
    QTest::keyClick(panel, Qt::Key_Right);
    QVERIFY(s->liveOffset(washId) != before);

    s->discardAll();
    panel->requestExit();
    QTRY_VERIFY(panel.isNull());
    QVERIFY(m_doc->deleteFixture(washId));
}

void FocusMode_Test::pressingLightsTheControl()
{
    plugInPad();
    InputOutputMap *iom = m_doc->inputOutputMap();
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::ControllerInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusControlMap *map = panel->controlMap();

    // A button lights while it is held.
    emit iom->inputValueChanged(0, BtnB, 255, QString());
    QVERIFY(map->isLit(R::B));
    QVERIFY(!map->isLit(R::A));
    emit iom->inputValueChanged(0, BtnB, 0, QString());
    QVERIFY(!map->isLit(R::B));
    emit iom->inputValueChanged(0, Up, 255, QString());
    QVERIFY(map->isLit(R::DpadUp));
    emit iom->inputValueChanged(0, Up, 0, QString());

    // The sticks show where they are: left pushed right, right pushed up.
    emit iom->inputValueChanged(0, LX, 255, QString());
    emit iom->inputValueChanged(0, RY, 0, QString());
    QVERIFY(map->stickDeflection(false).x() > 0.9);
    QVERIFY(map->stickDeflection(true).y() < -0.9);
    emit iom->inputValueChanged(0, LX, 128, QString());
    emit iom->inputValueChanged(0, RY, 128, QString());
    QVERIFY(qAbs(map->stickDeflection(false).x()) < 0.05);

    // The keys light their own map.
    panel->setInputDevice(FocusSession::KeyboardInput);
    QCOMPARE(map->mode(), FocusControlMap::KeyboardMap);
    QTest::keyPress(panel, Qt::Key_Return);
    QVERIFY(map->isKeyLit(Qt::Key_Return));
    QVERIFY(map->isKeyLit(Qt::Key_Enter));      // one key on the map
    QVERIFY(s->isChecked(s->currentFixture()));
    QTest::keyRelease(panel, Qt::Key_Return);
    QVERIFY(!map->isKeyLit(Qt::Key_Return));
    QTest::keyPress(panel, Qt::Key_Up, Qt::ShiftModifier);
    QVERIFY(map->isKeyLit(Qt::Key_Up));
    QVERIFY(map->isKeyLit(Qt::Key_Shift));
    QTest::keyRelease(panel, Qt::Key_Up, Qt::ShiftModifier);
    QVERIFY(!map->isKeyLit(Qt::Key_Up));
    QCOMPARE(map->keyAction(Qt::Key_Escape), QString("Exit"));
    // The keyboard says what the controller says: one table, the tool's words.
    using CS = ControllerScheme::Verb;
    const QList<QPair<int, CS> > verbKeys = {
        { Qt::Key_BracketLeft, CS::PreviousItem }, { Qt::Key_BracketRight, CS::NextItem },
        { Qt::Key_PageUp, CS::PreviousGroup }, { Qt::Key_PageDown, CS::NextGroup },
        { Qt::Key_G, CS::CycleScope }, { Qt::Key_Return, CS::Keep }, { Qt::Key_Backspace, CS::Revert },
        { Qt::Key_0, CS::Reset }, { Qt::Key_Z, CS::Undo }, { Qt::Key_Escape, CS::Exit },
        { Qt::Key_R, CS::Alternate }, { Qt::Key_T, CS::Alternate }, { Qt::Key_W, CS::WhereAmI },
        { Qt::Key_Comma, CS::ZoomNarrower }, { Qt::Key_Period, CS::ZoomWider },
        { Qt::Key_Semicolon, CS::FocusNearer }, { Qt::Key_Apostrophe, CS::FocusFarther } };
    for (ControllerScheme::Tool tool : { ControllerScheme::FocusTool, ControllerScheme::FocusCallsTool })
    {
        map->setTool(tool);
        for (const auto &kv : verbKeys)
            QCOMPARE(map->keyAction(kv.first), ControllerScheme::label(tool, kv.second));
        for (int k : { Qt::Key_U, Qt::Key_N, Qt::Key_D, Qt::Key_H, Qt::Key_Minus, Qt::Key_Plus })
            QVERIFY2(!map->keyAction(k).isEmpty(), qPrintable(QKeySequence(k).toString()));
        // Every key the panel takes is on the map, as a key to click.
        QImage img(map->sizeHint(), QImage::Format_ARGB32);
        map->resize(map->sizeHint());
        map->render(&img);
        const QList<int> drawn = map->clickable();
        for (int k : { int(Qt::Key_PageUp), int(Qt::Key_PageDown), int(Qt::Key_G), int(Qt::Key_R),
                       int(Qt::Key_W), int(Qt::Key_U), int(Qt::Key_N), int(Qt::Key_D), int(Qt::Key_H),
                       int(Qt::Key_Comma), int(Qt::Key_Period), int(Qt::Key_Semicolon),
                       int(Qt::Key_Apostrophe), int(Qt::Key_Return) })
            if (!map->keyAction(k).isEmpty())
                QVERIFY2(drawn.contains(k), qPrintable(QKeySequence(k).toString()));
    }
    map->setTool(ControllerScheme::FocusCallsTool);
    QCOMPARE(map->keyAction(Qt::Key_R), QString("Target page: move the call's target"));
    QCOMPARE(map->keyAction(Qt::Key_Return), QString("Save for this venue"));
    map->setTool(ControllerScheme::FocusTool);
    QCOMPARE(map->keyAction(Qt::Key_R), QString("Second reference"));
    QCOMPARE(map->keyAction(Qt::Key_BracketRight), QString("Next fixture"));
    QVERIFY(map->keyAction(Qt::Key_F).isEmpty());
    QVERIFY(map->grab().width() > 0);
}

void FocusMode_Test::aDisconnectFallsBackToTheKeyboard()
{
    plugInPad();
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QLabel *note = panel->findChild<QLabel *>("focusInputNote");
    QPushButton *back = panel->findChild<QPushButton *>("focusControllerBack");
    QVERIFY(note != nullptr && back != nullptr);
    QVERIFY(!note->isVisible());

    // Unplugged: the keyboard, said so -- and not remembered as a choice.
    unplugPad();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::KeyboardMap);
    QVERIFY(note->isVisible());
    QVERIFY2(note->text().contains("disconnected"), qPrintable(note->text()));
    QVERIFY(!back->isVisible());
    QVERIFY(!panel->findChild<QToolButton *>("focusUseController")->isEnabled());
    QVERIFY(QSettings().value("focusmode/inputDevice").toString().isEmpty());

    /* Back: it takes over by itself (RC2: "Use the controller" was the only
       way back, and nobody saw it), and says so. */
    plugInPad();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::ControllerMap);
    QVERIFY(note->isVisible());
    QVERIFY2(note->text().startsWith("Controller back"), qPrintable(note->text()));
    QVERIFY(!back->isVisible());
    QVERIFY(QSettings().value("focusmode/inputDevice").toString().isEmpty());
}

void FocusMode_Test::aPadThatSleepsIsPickedUpAgain()
{
    /* The real path: the plugin's device goes (a Bluetooth pad asleep) and
       comes back. The input patch is kept meanwhile and put back by name,
       and Focus mode reads the pad again without a restart. */
    plugInPad();
    m_stub->m_linesAreHardware = true;
    auto restore = qScopeGuard([this]() {
        m_stub->m_lineCount = 4;
        m_stub->m_linesAreHardware = false;
        if (m_doc->inputOutputMap()->inputPatch(0) == nullptr)
            m_stub->configure();
    });
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QLabel *note = panel->findChild<QLabel *>("focusInputNote");
    QPushButton *back = panel->findChild<QPushButton *>("focusControllerBack");

    m_stub->m_lineCount = 0;            // asleep: no line for it
    m_stub->configure();                // the plugin's rescan
    QVERIFY(m_doc->inputOutputMap()->inputPatch(0) == nullptr);
    QVERIFY(m_doc->programmer()->focusController().present == false);
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QVERIFY2(note->text().contains("disconnected"), qPrintable(note->text()));

    QSignalSpy said(m_doc->programmer(), &ProgrammerController::operatorAnnounce);
    m_stub->m_lineCount = 4;            // awake
    m_stub->configure();
    QVERIFY(m_doc->inputOutputMap()->inputPatch(0) != nullptr);
    QVERIFY(m_doc->programmer()->focusController().present);
    // Taken up at once, and said -- no button to find.
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QVERIFY2(note->text().startsWith("Controller back"), qPrintable(note->text()));
    QVERIFY(!back->isVisible());
    QVERIFY(!said.isEmpty());
    QCOMPARE(said.last().at(0).toString(), QString("Controller back."));
    QCOMPARE(said.last().at(1).toInt(), int(ControllerScheme::ImportantPriority));

    // And the stick moves the light: the DMX the session sends changes.
    ProgrammerController *pc = m_doc->programmer();
    const quint32 cur = s->currentFixture();
    const QHash<QPair<int, int>, uchar> before = s->pendingWrites();
    emit m_doc->inputOutputMap()->inputValueChanged(0, LX, 255, QString());
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    emit m_doc->inputOutputMap()->inputValueChanged(0, LX, 128, QString());
    QVERIFY(s->liveOffset(cur).pan > 0.0f);
    QVERIFY(s->pendingWrites() != before);
    s->discardAll();
    panel->close();
}

void FocusMode_Test::aPadAsleepAtTheStartIsSaidAndTakesOverWhenWoken()
{
    /* RC2: the pad asleep (Xbox auto-off), Focus mode started: it ran on the
       keyboard with only a grey "No controller connected", said nothing,
       and the pad, woken, was ignored. */
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    // Said in every feedback setting (Q131): lights only too.
    const ControllerScheme::Feedback feedback = pc->operatorFeedback();
    pc->setOperatorFeedback(ControllerScheme::LightsFeedback);
    auto restore = qScopeGuard([pc, feedback]() { pc->setOperatorFeedback(feedback); });
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QLabel *note = panel->findChild<QLabel *>("focusInputNote");
    QPushButton *back = panel->findChild<QPushButton *>("focusControllerBack");
    QVERIFY(note->isVisible());
    QVERIFY2(note->text().startsWith("No controller connected: using the keyboard"),
             qPrintable(note->text()));
    QVERIFY(!back->isVisible());
    bool spoken = false;
    for (const QList<QVariant> &l : said)
        if (l.at(0).toString() == QLatin1String("No controller connected. Using the keyboard."))
            spoken = l.at(1).toInt() == int(ControllerScheme::ImportantPriority);
    QVERIFY2(spoken, "the start with no controller was not said, with Feedback on lights only");

    // Woken: it takes over, said, and moves the light.
    said.clear();
    plugInPad();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::ControllerMap);
    QVERIFY2(note->text().startsWith("Controller back"), qPrintable(note->text()));
    QVERIFY(!said.isEmpty());
    QCOMPARE(said.last().at(0).toString(), QString("Controller back."));
    // Not remembered: it was nobody's choice.
    QVERIFY(QSettings().value("focusmode/inputDevice").toString().isEmpty());

    const quint32 cur = s->currentFixture();
    const QHash<QPair<int, int>, uchar> before = s->pendingWrites();
    emit m_doc->inputOutputMap()->inputValueChanged(0, LX, 255, QString());
    for (int i = 0; i < 10; ++i)
        pc->applyDesignJoystick();
    emit m_doc->inputOutputMap()->inputValueChanged(0, LX, 128, QString());
    QVERIFY(s->liveOffset(cur).pan > 0.0f);
    QVERIFY(s->pendingWrites() != before);
    s->discardAll();
}

void FocusMode_Test::theKeysNudgingHoldTheTakeoverABeat()
{
    // Woken while the keys are nudging: not snatched -- once they stop.
    FocusSession *s = m_doc->programmer()->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    QLabel *note = panel->findChild<QLabel *>("focusInputNote");
    QPushButton *back = panel->findChild<QPushButton *>("focusControllerBack");
    QTest::keyClick(panel, Qt::Key_Right);
    plugInPad();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QVERIFY2(note->text().contains("takes over when the keys stop"), qPrintable(note->text()));
    QVERIFY2(note->text().contains("Use the controller"), qPrintable(note->text()));
    QVERIFY(back->isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(s->inputDevice(), FocusSession::ControllerInput, 3000);
    QVERIFY2(note->text().startsWith("Controller back"), qPrintable(note->text()));
    QVERIFY(!back->isVisible());
    s->discardAll();
}

void FocusMode_Test::aChosenKeyboardIsKeptAndSaysWhatToPress()
{
    // The keyboard chosen: a pad that connects is not forced on anyone.
    FocusModePanel::rememberInput(FocusSession::KeyboardInput);
    ProgrammerController *pc = m_doc->programmer();
    FocusSession *s = pc->focusSession();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why);
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    QSignalSpy said(pc, &ProgrammerController::operatorAnnounce);
    plugInPad();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QLabel *note = panel->findChild<QLabel *>("focusInputNote");
    QPushButton *back = panel->findChild<QPushButton *>("focusControllerBack");
    QVERIFY(note->isVisible());
    QVERIFY2(note->text().contains("click Use the controller"), qPrintable(note->text()));
    QVERIFY(back->isVisible());
    QStringList lines;
    for (const QList<QVariant> &l : said)
        lines << l.at(0).toString();
    QVERIFY2(lines.contains("Keyboard in use. Click Use the controller to switch."),
             qPrintable(lines.join(" | ")));
    back->click();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
}

void FocusMode_Test::theSetupWithNoPadIsNotAChoice()
{
    /* With no controller the setup offers only the keyboard: that is not
       remembered (it was: a pad asleep once made every later session start
       on the keyboard, the pad ignored), and says the pad takes over. */
    {
        FocusModeSetupDialog setup(m_doc, Function::invalidId(), m_heads);
        QCOMPARE(setup.inputDevice(), FocusSession::KeyboardInput);
        QVERIFY(!setup.inputChosen());
        QComboBox *use = setup.findChild<QComboBox *>("focusInput");
        QVERIFY2(use->currentText().contains("takes over when it connects"),
                 qPrintable(use->currentText()));
    }
    plugInPad();
    {
        FocusModeSetupDialog setup(m_doc, Function::invalidId(), m_heads);
        QVERIFY(setup.inputChosen());
    }
    // An old setting from before is not read: the controller, when there.
    QSettings().setValue("focusmode/input", "keyboard");
    QCOMPARE(FocusModePanel::defaultInput(m_doc), FocusSession::ControllerInput);
}

namespace
{
/** The keep-alive's platform side, inert: no real activity in a test. */
class QuietBackend final : public ControllerKeepAlive::Backend
{
public:
    void beginActivity(const QString &) override {}
    void endActivity() override {}
    ControllerKeepAlive::Permission inputMonitoring() const override
    { return ControllerKeepAlive::PermissionGranted; }
};
}

void FocusMode_Test::aPausedControllerSaysWhy()
{
    ControllerKeepAlive *keep = m_doc->programmer()->controllerKeepAlive();
    keep->setBackend(new QuietBackend);
    plugInPad();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::ControllerInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->show();
    // A tool window that a Mac would otherwise hide whenever the app is not in front.
    QVERIFY(panel->testAttribute(Qt::WA_MacAlwaysShowToolWindow));
    QVERIFY(keep->isHeld());

    QLabel *paused = panel->findChild<QLabel *>("focusPausedNote");
    QVERIFY(paused != nullptr);
    QVERIFY(!paused->isVisible());

    // The plugin knows the controller cannot be read: said, with its reason.
    m_stub->m_inputProblems = { { 0, "macOS is not letting qlcconsole read the controller." } };
    emit m_doc->programmer()->focusControllerChanged();
    QVERIFY(paused->isVisible());
    QCOMPARE(paused->text(), QString("Controller paused: macOS is not letting qlcconsole read "
                                     "the controller."));
    m_stub->m_inputProblems.clear();
    emit m_doc->programmer()->focusControllerChanged();
    QVERIFY(!paused->isVisible());

    // Asleep, then found again by a press.
    keep->setSystemState(ControllerKeepAlive::Asleep);
    QVERIFY(paused->isVisible());
    QVERIFY2(paused->text().contains("asleep"), qPrintable(paused->text()));
    keep->setSystemState(ControllerKeepAlive::Waking);
    QVERIFY2(paused->text().contains("after the Mac slept"), qPrintable(paused->text()));
    emit m_doc->inputOutputMap()->inputValueChanged(0, BtnLB, 0, QString());
    QVERIFY(!paused->isVisible());

    // On the keyboard the controller is not in use: nothing to say.
    keep->setSystemState(ControllerKeepAlive::Asleep);
    panel->setInputDevice(FocusSession::KeyboardInput, false);
    QVERIFY(!paused->isVisible());
    keep->setSystemState(ControllerKeepAlive::Awake);

    m_doc->programmer()->focusSession()->discardAll();
    panel->requestExit();
    QTRY_VERIFY(panel.isNull());
    QVERIFY(!keep->isHeld());
}

void FocusMode_Test::theControllerTestListsEachInput()
{
    plugInPad();
    m_stub->m_connectionInfo = { { 0, "Bluetooth LE \302\267 Xbox Series X|S Controller (045E:0B13)" } };
    const int before = m_stub->m_diagnostics;
    FocusModeSetupDialog setup(m_doc, 0, m_heads);
    QPushButton *test = setup.findChild<QPushButton *>("focusControllerTest");
    QVERIFY(test != nullptr);
    test->click();
    QPointer<ControllerTestDialog> dlg = ControllerTestDialog::instance();
    QVERIFY(dlg != nullptr);
    QCOMPARE(m_stub->m_diagnostics, before + 1);
    QVERIFY(dlg->pluginCount() >= 1);
    QLabel *devices = dlg->findChild<QLabel *>("controllerTestDevices");
    QVERIFY(devices != nullptr);
    QVERIFY2(devices->text().contains("045E:0B13"), qPrintable(devices->text()));

    const QString dev = "Xbox Wireless Controller";
    const QString conn = "Bluetooth LE";
    m_stub->emitDiagnostic(0, dev, conn, "Button 1", 1, BtnA, 255);
    m_stub->emitDiagnostic(0, dev, conn, "Button 1", 0, BtnA, 0);
    m_stub->emitDiagnostic(0, dev, conn, "X", 65535, LX, 255);
    m_stub->emitDiagnostic(0, dev, conn, "Consumer 0xb2 (Record)", 1, -1, -1);
    QCOMPARE(dlg->rowCount(), 3);   // one row per element, however often

    QTreeWidget *tree = dlg->findChild<QTreeWidget *>("controllerTestInputs");
    QVERIFY(tree != nullptr);
    const QTreeWidgetItem *a = tree->topLevelItem(0);
    QCOMPARE(a->text(2), QString("Button 1"));
    QCOMPARE(a->text(8), QString("2"));           // seen twice
    QCOMPARE(a->text(4), QString("0 \342\200\246 1"));     // the raw values seen
    QVERIFY2(a->text(7).contains("role a"), qPrintable(a->text(7)));
    QVERIFY2(a->text(7).contains("Focus: Save"), qPrintable(a->text(7)));
    QVERIFY2(tree->topLevelItem(1)->text(7).contains("Focus: Nudge"),
             qPrintable(tree->topLevelItem(1)->text(7)));
    QVERIFY2(tree->topLevelItem(2)->text(7).startsWith("nothing"),
             qPrintable(tree->topLevelItem(2)->text(7)));

    // The log: the devices, a row per element, every event in order.
    const QString log = dlg->logText();
    QVERIFY(log.contains("045E:0B13"));
    QVERIFY(log.contains("Button 1\t"));
    QVERIFY(log.contains("Consumer 0xb2 (Record)"));
    QCOMPARE(log.count("Xbox Wireless Controller  Button 1 = "), 2);
    QPushButton *copy = dlg->findChild<QPushButton *>("controllerTestCopy");
    QVERIFY(copy != nullptr);
    copy->click();
    QCOMPARE(QGuiApplication::clipboard()->text(), log);

    // Closed: the plugins stop reporting.
    dlg->close();
    QTRY_VERIFY(dlg.isNull());
    QCOMPARE(m_stub->m_diagnostics, before);
    m_stub->m_connectionInfo.clear();
}

void FocusMode_Test::theLampSaysWhatIsUnsaved()
{
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusSession *s = m_doc->programmer()->focusSession();
    FocusStatusLamp *lamp = panel->findChild<FocusStatusLamp *>(QStringLiteral("focusStatusLamp"));
    QLabel *words = panel->findChild<QLabel *>(QStringLiteral("focusSaveStatus"));
    QVERIFY(lamp != nullptr && lamp == panel->statusLamp() && words != nullptr);
    const QString first = m_doc->fixture(s->currentFixture())->name();

    // Blue, with a tick: all saved (gap 10: colour-blind safe, a shape, a count).
    QCOMPARE(lamp->state(), FocusStatusLamp::AllSaved);
    QCOMPARE(lamp->symbol(), QString::fromUtf8("\u2713"));
    QCOMPARE(lamp->count(), 0);
    QCOMPARE(FocusStatusLamp::savedColor(), QColor(0x00, 0x72, 0xb2));
    QCOMPARE(lamp->centreColor(), FocusStatusLamp::savedColor());
    QCOMPARE(lamp->ringColor(), FocusStatusLamp::savedColor());
    QCOMPARE(words->text(), QStringLiteral("All saved"));

    // Yellow: the light being focused.
    QTest::keyClick(panel, Qt::Key_Right);
    QCOMPARE(lamp->state(), FocusStatusLamp::ThisUnsaved);
    QCOMPARE(lamp->centreColor(), FocusStatusLamp::thisColor());
    QCOMPARE(lamp->ringColor(), FocusStatusLamp::thisColor());
    QCOMPARE(words->text(), QStringLiteral("Unsaved: this light"));

    // Red: another light, by name.
    QTest::keyClick(panel, Qt::Key_BracketRight);
    QCOMPARE(lamp->state(), FocusStatusLamp::OthersUnsaved);
    QCOMPARE(lamp->centreColor(), FocusStatusLamp::othersColor());
    QCOMPARE(lamp->ringColor(), FocusStatusLamp::othersColor());
    QCOMPARE(words->text(), QStringLiteral("Unsaved: 1 other light (%1)").arg(first));

    // Both: red outside, yellow in the middle.
    QTest::keyClick(panel, Qt::Key_Up);
    QCOMPARE(lamp->state(), FocusStatusLamp::BothUnsaved);
    QCOMPARE(lamp->ringColor(), FocusStatusLamp::othersColor());
    QCOMPARE(lamp->centreColor(), FocusStatusLamp::thisColor());
    QCOMPARE(words->text(), QStringLiteral("Unsaved: this light and 1 other (%1)").arg(first));
    QCOMPARE(lamp->symbol(), QStringLiteral("!"));
    QCOMPARE(lamp->count(), 2);

    // Save this one: a green flash, then red for the other.
    QTest::keyClick(panel, Qt::Key_Return);
    QVERIFY(lamp->isFlashing());
    QCOMPARE(lamp->centreColor(), FocusStatusLamp::savedColor());
    QCOMPARE(lamp->state(), FocusStatusLamp::OthersUnsaved);
    QTRY_VERIFY(!lamp->isFlashing());
    QCOMPARE(lamp->ringColor(), FocusStatusLamp::othersColor());

    // Back to it, revert: all saved, and a revert does not flash.
    QTest::keyClick(panel, Qt::Key_BracketLeft);
    QCOMPARE(lamp->state(), FocusStatusLamp::ThisUnsaved);
    QTest::keyClick(panel, Qt::Key_Backspace);
    QVERIFY(!lamp->isFlashing());
    QCOMPARE(lamp->state(), FocusStatusLamp::AllSaved);
    QCOMPARE(words->text(), QStringLiteral("All saved"));
}

void FocusMode_Test::exitNamesTheUnsavedLights()
{
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::KeyboardInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    FocusSession *s = m_doc->programmer()->focusSession();
    s->nudge(1.0f, 0.0f);
    s->next();
    s->nudge(1.0f, 0.0f);

    QString asked;
    QTimer::singleShot(0, this, [&asked]() {
        QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (box == nullptr)
            return;
        asked = box->text();
        box->button(QMessageBox::Cancel)->click();
    });
    QVERIFY(!panel->requestExit());
    QCOMPARE(asked, QStringLiteral("2 lights have changes you have not saved: A, B."));
    QVERIFY(s->isActive());
}

void FocusMode_Test::shots()
{
    const QString out = qEnvironmentVariable("FOCUSUI_SHOTS");
    if (out.isEmpty())
        QSKIP("FOCUSUI_SHOTS is not set");
    QDir().mkpath(out);

    plugInPad();
    InputOutputMap *iom = m_doc->inputOutputMap();
    QString why;
    QPointer<FocusModePanel> panel = FocusModePanel::startWith(
        m_doc, nullptr, optionsFor(m_heads, m_target), &why, int(FocusSession::ControllerInput));
    QVERIFY2(panel != nullptr, qPrintable(why));
    panel->resize(660, 640);

    const QPalette native = qApp->palette();
    const struct { int theme; const char *name; } themes[] = {
        { App::ThemeDefault, "default" }, { App::ThemeTan, "tan" }, { App::ThemeBlue, "blue" },
        { App::ThemeQLCOriginal, "qlc-original" }, { App::ThemeRedShift, "redshift" },
        { App::ThemeVSCodeDark, "vscode-dark" },
    };
    auto save = [&](QWidget *w, const QString &file) {
        QImage img(w->size() * 2, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(2.0);
        img.fill(Qt::transparent);
        w->render(&img);
        QVERIFY2(img.save(file), qPrintable(file));
    };
    // Both unsaved: the other head nudged, then this one.
    FocusSession *fs = m_doc->programmer()->focusSession();
    fs->nudge(0.5f, 0.0f);
    fs->next();
    fs->nudge(0.0f, 0.3f);
    auto saveStatus = [&](const QString &file) {
        // The lamp and its words, as a strip across the panel.
        const QRect lamp = panel->statusLamp()->geometry();
        const QLabel *words = panel->findChild<QLabel *>(QStringLiteral("focusSaveStatus"));
        const int right = lamp.right() + 12 + words->fontMetrics().horizontalAdvance(words->text()) + 16;
        const QRect strip(0, lamp.top() - 6, qMin(panel->width(), right), lamp.height() + 12);
        QImage img(strip.size() * 2, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(2.0);
        img.fill(panel->palette().color(QPalette::Window));
        panel->render(&img, QPoint(), QRegion(strip));
        QVERIFY2(img.save(file), qPrintable(file));
    };
    for (const auto &t : themes)
    {
        qApp->setPalette(App::themePalette(t.theme, native));
        QCoreApplication::processEvents();
        saveStatus(QStringLiteral("%1/save-status-%2.png").arg(out, t.name));

        // The controller, the right bumper held and the left stick pushed.
        panel->setInputDevice(FocusSession::ControllerInput, false);
        emit iom->inputValueChanged(0, BtnRB, 255, QString());
        emit iom->inputValueChanged(0, LX, 220, QString());
        emit iom->inputValueChanged(0, LY, 70, QString());
        save(panel, QStringLiteral("%1/focus-controller-%2.png").arg(out, t.name));
        save(panel->controlMap(), QStringLiteral("%1/map-controller-%2.png").arg(out, t.name));
        emit iom->inputValueChanged(0, BtnRB, 0, QString());
        emit iom->inputValueChanged(0, LX, 128, QString());
        emit iom->inputValueChanged(0, LY, 128, QString());

        // The keyboard, ⇧ and ↑ held.
        panel->setInputDevice(FocusSession::KeyboardInput, false);
        panel->controlMap()->setKeyDown(Qt::Key_Shift, true);
        panel->controlMap()->setKeyDown(Qt::Key_Up, true);
        save(panel, QStringLiteral("%1/focus-keyboard-%2.png").arg(out, t.name));
        save(panel->controlMap(), QStringLiteral("%1/map-keyboard-%2.png").arg(out, t.name));
        panel->controlMap()->clearInput();
    }
    qApp->setPalette(native);

    // A profile with other names, unmapped Y: for a look.
    m_stub->m_channelNames = { { BtnA, "Cross" }, { BtnB, "Circle" }, { BtnX, "Square" },
                               { BtnLB, "L1" }, { BtnRB, "R1" }, { BtnView, "Share" },
                               { BtnMenu, "Options" } };
    m_stub->m_channelRoles.remove(BtnY);
    emit iom->pluginConfigurationChanged(m_stub->name(), true);
    qApp->setPalette(App::themePalette(App::ThemeBlue, native));
    panel->setInputDevice(FocusSession::ControllerInput, false);
    save(panel->controlMap(), QStringLiteral("%1/map-controller-other-profile-blue.png").arg(out));
    qApp->setPalette(native);
}

QTEST_MAIN(FocusMode_Test)
