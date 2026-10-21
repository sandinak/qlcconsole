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

#include "focusmode_test.h"
#include "iopluginstub.h"
#include "qlcfile.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
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
    QCOMPARE(map->actionText(R::RB), QString("Next head"));
    QCOMPARE(map->controlName(R::RB), QString("R1"));
    QCOMPARE(map->actionText(R::LB), QString("Previous head"));
    QCOMPARE(map->actionText(R::B), QString("Revert"));
    QCOMPARE(map->actionText(R::X), QString("Zero"));
    QCOMPARE(map->actionText(R::Back), QString("Undo save"));
    QCOMPARE(map->controlName(R::Back), QString("Share"));
    QCOMPARE(map->actionText(R::Start), QString("Exit"));
    QCOMPARE(map->actionText(R::LeftX), QString("Nudge"));
    QCOMPARE(map->actionText(R::RightX), QString("Fine nudge"));
    QVERIFY2(map->actionText(R::DpadUp).contains("0.1"), qPrintable(map->actionText(R::DpadUp)));
    // Named by role only: the usual name.
    QCOMPARE(map->controlName(R::DpadLeft), QString("D-pad left"));

    // What the profile does not have is greyed; what Focus mode does not
    // use says nothing.
    QVERIFY(map->isMapped(R::A));
    QVERIFY(!map->isMapped(R::Y));
    QCOMPARE(map->actionText(R::Y), QString("Second reference"));
    QVERIFY(!map->isMapped(R::LeftTrigger));
    QVERIFY(map->actionText(R::LeftTrigger).isEmpty());

    // The step the D-pad takes follows + and -.
    QTest::keyClick(panel, Qt::Key_Plus);
    QVERIFY2(map->actionText(R::DpadUp).startsWith("1"), qPrintable(map->actionText(R::DpadUp)));
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
    QCOMPARE(QSettings().value("focusmode/input").toString(), QString("keyboard"));
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

    // On the controller, the same press moves it.
    emit iom->inputValueChanged(0, LX, 128, QString());
    emit iom->inputValueChanged(0, RY, 128, QString());
    panel->setInputDevice(FocusSession::ControllerInput);
    press(Right);
    QCOMPARE(s->liveOffset(first).pan, 0.2f);
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
    QCOMPARE(map->keyAction(Qt::Key_BracketRight), QString("Next head"));
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
    QVERIFY(QSettings().value("focusmode/input").toString().isEmpty());

    // Back: offered, not taken.
    plugInPad();
    QCOMPARE(s->inputDevice(), FocusSession::KeyboardInput);
    QVERIFY(back->isVisible());
    QVERIFY2(note->text().contains("back"), qPrintable(note->text()));
    back->click();
    QCOMPARE(s->inputDevice(), FocusSession::ControllerInput);
    QCOMPARE(panel->controlMap()->mode(), FocusControlMap::ControllerMap);
    QVERIFY(!note->isVisible());
    QVERIFY(!back->isVisible());
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
    for (const auto &t : themes)
    {
        qApp->setPalette(App::themePalette(t.theme, native));
        QCoreApplication::processEvents();

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
