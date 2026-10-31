/*
  Q Light Controller Plus - Unit test
  vcbutton_test.cpp

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QObject>
#include <QtTest>
#include <QMenu>
#include <QPushButton>
#include <QMimeData>

#define protected public
#define private public
#include "qlcfixturedefcache.h"
#include "qlcinputsource.h"
#include "virtualconsole.h"
#include "vcbutton_test.h"
#include "mastertimer.h"
#include "qlcmacros.h"
#include "vcbutton.h"
#include "vcframe.h"
#include "scene.h"
#include "collection.h"
#include "chaser.h"
#include "chaseraction.h"
#include "functionparent.h"
#include "showtreecontroller.h"
#include "showcontrol.h"
#include "programmercontroller.h"
#include "functionselection.h"
#include "functionstreewidget.h"
#include "vcbuttonproperties.h"
#include "hitmap.h"
#include "showstatus.h"
#include "doc.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "inputoutputmap.h"
#include "universe.h"
#undef private
#undef protected

static const QKeySequence keySequenceA(Qt::Key_A);
static const QKeySequence keySequenceB(Qt::Key_B);

void VCButton_Test::initTestCase()
{
    m_doc = NULL;
}

void VCButton_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);

    Fixture* fxi = new Fixture(m_doc);
    fxi->setChannels(4);
    m_doc->addFixture(fxi);
}

void VCButton_Test::cleanup()
{
    delete VirtualConsole::instance();
    delete m_doc;
}

void VCButton_Test::initial()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    QCOMPARE(btn.objectName(), QString("VCButton"));
    QCOMPARE(btn.frameStyle(), (int) KVCFrameStyleNone);
    QCOMPARE(btn.caption(), QString());
    QCOMPARE(btn.size(), QSize(50, 50));
    QCOMPARE(btn.function(), Function::invalidId());
    QCOMPARE(btn.startupIntensity(), qreal(1.0));
    QCOMPARE(btn.isStartupIntensityEnabled(), false);
    QCOMPARE(btn.state(), VCButton::Inactive);
    QCOMPARE(btn.action(), VCButton::Toggle);
    QCOMPARE(btn.iconPath(), QString());
    QVERIFY(btn.m_chooseIconAction != NULL);
    QVERIFY(btn.m_resetIconAction != NULL);

    // Only for coverage
    btn.setBackgroundImage(QString());
}

void VCButton_Test::function()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    btn.setFunction(42);
    QCOMPARE(btn.function(), Function::invalidId());

    Scene* s = new Scene(m_doc);
    s->setName("Test1");
    m_doc->addFunction(s);
    btn.setFunction(s->id());
    QCOMPARE(btn.function(), s->id());
    QCOMPARE(btn.toolTip(), QString("Test1"));

    Scene* s2 = new Scene(m_doc);
    s2->setName("Test2");
    m_doc->addFunction(s2);
    btn.setFunction(s2->id());
    QCOMPARE(btn.function(), s2->id());
    QCOMPARE(btn.toolTip(), QString("Test2"));

    btn.setFunction(s2->id() + 1);
    QCOMPARE(btn.function(), Function::invalidId());
    QCOMPARE(btn.toolTip(), QString());

    btn.setFunction(s2->id());
    QCOMPARE(btn.function(), s2->id());
    QCOMPARE(btn.toolTip(), QString("Test2"));

    m_doc->deleteFunction(s2->id());
    QCOMPARE(btn.function(), Function::invalidId());
    QCOMPARE(btn.toolTip(), QString());

    m_doc->deleteFunction(s->id());
    QCOMPARE(btn.function(), Function::invalidId());
    QCOMPARE(btn.toolTip(), QString());
}

void VCButton_Test::action()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    btn.setAction(VCButton::Flash);
    QCOMPARE(btn.action(), VCButton::Flash);
    btn.setAction(VCButton::Toggle);
    QCOMPARE(btn.action(), VCButton::Toggle);

    QCOMPARE(VCButton::actionToString(VCButton::Toggle), QString("Toggle"));
    QCOMPARE(VCButton::actionToString(VCButton::Flash), QString("Flash"));
    // 15: no action, though a value an Action can hold (0..15).
    QCOMPARE(VCButton::actionToString(VCButton::Action(15)), QString("Toggle"));
    QCOMPARE(VCButton::stringToAction("Toggle"), VCButton::Toggle);
    QCOMPARE(VCButton::stringToAction("Flash"), VCButton::Flash);
    QCOMPARE(VCButton::stringToAction("Foobar"), VCButton::Toggle);
}

void VCButton_Test::intensity()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    btn.enableStartupIntensity(true);
    QCOMPARE(btn.isStartupIntensityEnabled(), true);
    btn.enableStartupIntensity(false);
    QCOMPARE(btn.isStartupIntensityEnabled(), false);

    for (qreal i = -0.5; i < 1.2; i += 0.01)
    {
        btn.setStartupIntensity(i);
        QCOMPARE(btn.startupIntensity(), CLAMP(i, qreal(0.0), qreal(1.0)));
    }
}

void VCButton_Test::bgcolor()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    m_doc->resetModified();
    btn.setBackgroundColor(QColor(Qt::red));
    QCOMPARE(btn.backgroundColor(), QColor(Qt::red));
    QCOMPARE(btn.palette().color(QPalette::Button), QColor(Qt::red));
    QCOMPARE(m_doc->isModified(), true);
    QVERIFY(btn.foregroundColor() != QColor(Qt::red));
}

void VCButton_Test::fgcolor()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    m_doc->resetModified();
    btn.setForegroundColor(QColor(Qt::red));
    QCOMPARE(btn.foregroundColor(), QColor(Qt::red));
    QCOMPARE(btn.palette().color(QPalette::ButtonText), QColor(Qt::red));
    QCOMPARE(btn.palette().color(QPalette::WindowText), QColor(Qt::red));
    QCOMPARE(m_doc->isModified(), true);
    QVERIFY(btn.backgroundColor() != QColor(Qt::red));
}

void VCButton_Test::resetColors()
{
    QWidget w;

    VCButton btn(&w, m_doc);

    btn.setForegroundColor(QColor(Qt::red));
    btn.setBackgroundColor(QColor(Qt::blue));
    m_doc->resetModified();
    btn.resetForegroundColor();
    QCOMPARE(btn.foregroundColor(), w.palette().color(QPalette::WindowText));
    QCOMPARE(btn.backgroundColor(), QColor(Qt::blue));
    QCOMPARE(m_doc->isModified(), true);

    btn.resetForegroundColor();
    QCOMPARE(btn.foregroundColor(), w.palette().color(QPalette::WindowText));
    QCOMPARE(btn.backgroundColor(), QColor(Qt::blue));

    btn.setForegroundColor(QColor(Qt::red));
    btn.setBackgroundColor(QColor(Qt::blue));
    m_doc->resetModified();
    btn.resetBackgroundColor();
    QCOMPARE(btn.backgroundColor(), w.palette().color(QPalette::Button));
    QCOMPARE(btn.foregroundColor(), QColor(Qt::red));
    QCOMPARE(m_doc->isModified(), true);

    btn.resetBackgroundColor();
    QCOMPARE(btn.backgroundColor(), w.palette().color(QPalette::Button));
    QCOMPARE(btn.foregroundColor(), QColor(Qt::red));
}

void VCButton_Test::iconPath()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    m_doc->resetModified();
    btn.setIconPath("../../../resources/icons/png/qlcconsole.png");
    QCOMPARE(btn.iconPath(), QString("../../../resources/icons/png/qlcconsole.png"));
    QCOMPARE(m_doc->isModified(), true);

    m_doc->resetModified();
    btn.slotResetIcon();
    QCOMPARE(btn.iconPath(), QString());
    QCOMPARE(m_doc->isModified(), true);
}

void VCButton_Test::on()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    btn.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 1)));

    QCOMPARE(btn.state(), VCButton::Inactive);

    btn.setState(VCButton::Inactive);
    QCOMPARE(btn.state(), VCButton::Inactive);

    btn.setState(VCButton::Active);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.setState(VCButton::Active);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.setState(VCButton::Inactive);
    QCOMPARE(btn.state(), VCButton::Inactive);
}

void VCButton_Test::keySequence()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    QCOMPARE(btn.keySequence(), QKeySequence());

    QKeySequence seq(keySequenceA);
    btn.setKeySequence(seq);
    QCOMPARE(btn.keySequence(), seq);

    seq = QKeySequence(keySequenceB);
    QVERIFY(btn.keySequence() != seq);
    btn.setKeySequence(seq);
    QCOMPARE(btn.keySequence(), seq);
}

void VCButton_Test::copy()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    m_doc->addFunction(sc);

    VCButton btn(&w, m_doc);
    btn.setCaption("Foobar");
    btn.setIconPath("../../../resources/icons/png/qlcconsole.png");
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Flash);
    btn.setKeySequence(QKeySequence(keySequenceB));
    btn.enableStartupIntensity(true);
    btn.setStartupIntensity(qreal(0.2));

    VCFrame parent(&w, m_doc);
    VCButton* copy = qobject_cast<VCButton*> (btn.createCopy(&parent));
    QVERIFY(copy != NULL);
    QCOMPARE(copy->caption(), QString("Foobar"));
    QCOMPARE(copy->iconPath(), QString("../../../resources/icons/png/qlcconsole.png"));
    QCOMPARE(copy->function(), sc->id());
    QCOMPARE(copy->action(), VCButton::Flash);
    QCOMPARE(copy->keySequence(), QKeySequence(keySequenceB));
    QCOMPARE(copy->isStartupIntensityEnabled(), true);
    QCOMPARE(copy->startupIntensity(), qreal(0.2));
    delete copy;
}

void VCButton_Test::load()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    m_doc->setWorkspacePath(QDir("../../../resources/icons/png").absolutePath());

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("Button");
    xmlWriter.writeAttribute("Caption", "Pertti");
    xmlWriter.writeAttribute("Icon", "qlcconsole.png");

    xmlWriter.writeStartElement("WindowState");
    xmlWriter.writeAttribute("X", "20");
    xmlWriter.writeAttribute("Y", "20");
    xmlWriter.writeAttribute("Width", "60");
    xmlWriter.writeAttribute("Height", "60");
    xmlWriter.writeAttribute("Visible", "True");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Appearance");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Function");
    xmlWriter.writeAttribute("ID", QString::number(sc->id()));
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Input");
    xmlWriter.writeEndElement();

    xmlWriter.writeTextElement("Action", "Flash");

    xmlWriter.writeTextElement("Key", QKeySequence(keySequenceA).toString());

    xmlWriter.writeStartElement("Intensity");
    xmlWriter.writeAttribute("Adjust", "True");
    xmlWriter.writeCharacters("60");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    VCButton btn(&w, m_doc);
    QCOMPARE(btn.loadXML(xmlReader), true);
    QCOMPARE(btn.caption(), QString("Pertti"));
    QCOMPARE(btn.iconPath(), QFileInfo(QString("../../../resources/icons/png/qlcconsole.png")).canonicalFilePath());
    QCOMPARE(btn.function(), sc->id());
    QCOMPARE(btn.action(), VCButton::Flash);
    QCOMPARE(btn.keySequence(), QKeySequence(keySequenceA));
    QCOMPARE(btn.isStartupIntensityEnabled(), true);
    QCOMPARE(btn.startupIntensity(), qreal(0.6));
    QCOMPARE(btn.pos(), QPoint(20, 20));
    QCOMPARE(btn.size(), QSize(60, 60));

    buffer.close();
    QByteArray bData = buffer.data();
    bData.replace("Adjust=\"True\"", "Adjust=\"False\"");
    buffer.setData(bData);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    buffer.seek(0);
    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    QCOMPARE(btn.loadXML(xmlReader), true);
    QCOMPARE(btn.caption(), QString("Pertti"));
    QCOMPARE(btn.iconPath(), QFileInfo(QString("../../../resources/icons/png/qlcconsole.png")).canonicalFilePath());
    QCOMPARE(btn.function(), sc->id());
    QCOMPARE(btn.action(), VCButton::Flash);
    QCOMPARE(btn.keySequence(), QKeySequence(keySequenceA));
    QCOMPARE(btn.isStartupIntensityEnabled(), false);
    QCOMPARE(btn.startupIntensity(), qreal(0.6));
    QCOMPARE(btn.pos(), QPoint(20, 20));
    QCOMPARE(btn.size(), QSize(60, 60));

    buffer.close();
    bData = buffer.data();
    bData.replace("<Button", "<Buton");
    buffer.setData(bData);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    buffer.seek(0);
    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    QCOMPARE(btn.loadXML(xmlReader), false);
}

void VCButton_Test::save()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    m_doc->setWorkspacePath(QDir("../../../resources/icons/png").absolutePath());

    VCButton btn(&w, m_doc);
    btn.setCaption("Foobar");
    btn.setIconPath("../../../resources/icons/png/qlcconsole.png");
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Flash);
    btn.setKeySequence(QKeySequence(keySequenceB));
    btn.enableStartupIntensity(true);
    btn.setStartupIntensity(qreal(0.2));
    btn.setFlashForceLTP(true);
    btn.setFlashOverride(true);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    int function = 0, action = 0, key = 0, intensity = 0, wstate = 0, appearance = 0, flashProperties = 0;
    QCOMPARE(btn.saveXML(&xmlWriter), true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QCOMPARE(xmlReader.name().toString(), QString("Button"));
    QCOMPARE(xmlReader.attributes().value("Icon").toString(), QString("qlcconsole.png"));
    QCOMPARE(xmlReader.attributes().value("Caption").toString(), QString("Foobar"));

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Function")
        {
            function++;
            QCOMPARE(xmlReader.attributes().value("ID").toString(), QString::number(sc->id()));
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Action")
        {
            QCOMPARE(xmlReader.attributes().value("Override").toString(), QString("1"));
            QCOMPARE(xmlReader.attributes().value("ForceLTP").toString(), QString("1"));
            flashProperties++;
            QCOMPARE(xmlReader.readElementText(), QString("Flash"));
            action++;
        }
        else if (xmlReader.name().toString() == "Key")
        {
            key++;
            QCOMPARE(xmlReader.readElementText(), QKeySequence(keySequenceB).toString());
        }
        else if (xmlReader.name().toString() == "Intensity")
        {
            intensity++;
            QCOMPARE(xmlReader.attributes().value("Adjust").toString(), QString("True"));
            QCOMPARE(xmlReader.readElementText(), QString("20"));
        }
        else if (xmlReader.name().toString() == "WindowState")
        {
            wstate++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Appearance")
        {
            appearance++;
            xmlReader.skipCurrentElement();
        }
        else
        {
            QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            xmlReader.skipCurrentElement();
        }
    }

    QCOMPARE(function, 1);
    QCOMPARE(action, 1);
    QCOMPARE(key, 1);
    QCOMPARE(intensity, 1);
    QCOMPARE(wstate, 1);
    QCOMPARE(appearance, 1);
    QCOMPARE(flashProperties, 1);
}

void VCButton_Test::customMenu()
{
    QWidget w;

    VCButton btn(&w, m_doc);
    QMenu* menu = btn.customMenu(NULL);
    QVERIFY(menu != NULL);
    QCOMPARE(menu->title(), tr("Icon"));
    QCOMPARE(menu->actions().size(), 2);
    QCOMPARE(menu->actions()[0], btn.m_chooseIconAction);
    QCOMPARE(menu->actions()[1], btn.m_resetIconAction);
    delete menu;
}

void VCButton_Test::toggle()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    sc->setValue(0, 0, 255);
    sc->setFadeInSpeed(1000);
    sc->setFadeOutSpeed(1000);
    m_doc->addFunction(sc);

    VCButton btn(&w, m_doc);
    btn.setCaption("Foobar");
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Toggle);
    btn.setKeySequence(QKeySequence(keySequenceB));
    btn.enableStartupIntensity(true);
    btn.setStartupIntensity(qreal(0.2));

    // Mouse button press in design mode doesn't toggle the function
    QCOMPARE(m_doc->mode(), Doc::Design);
    QMouseEvent ev(QEvent::MouseButtonPress, QPoint(0, 0), QPoint(0, 0), QPoint(0, 0),
                   Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    btn.mousePressEvent(&ev);
    QCOMPARE(m_doc->masterTimer()->m_functionList.size(), 0);
    QMouseEvent ev2(QEvent::MouseButtonRelease, QPoint(0, 0), QPoint(0, 0), QPoint(0, 0),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    btn.mouseReleaseEvent(&ev2);
    QCOMPARE(m_doc->masterTimer()->m_functionList.size(), 0);

    // Mouse button press in operate mode should toggle the function
    m_doc->setMode(Doc::Operate);
    btn.slotKeyPressed(QKeySequence(keySequenceB));
    // tell MasterTimer to process start queue
    m_doc->masterTimer()->timerTick();
    QCOMPARE(m_doc->masterTimer()->m_functionList.size(), 1);
    QCOMPARE(m_doc->masterTimer()->m_functionList[0], sc);
    QCOMPARE(sc->getAttributeValue(Function::Intensity), btn.startupIntensity());
    btn.slotKeyReleased(QKeySequence(keySequenceB));
    m_doc->masterTimer()->timerTick(); // Allow MasterTimer to take the function under execution
    QCOMPARE(sc->stopped(), false);
    QCOMPARE(btn.state(), VCButton::Active);

    QMouseEvent ev3(QEvent::MouseButtonPress, QPoint(0, 0), QPoint(0, 0), QPoint(0, 0),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    btn.mousePressEvent(&ev3);
    QCOMPARE(sc->m_stop, true);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.slotFunctionStopped(sc->id());
    QCOMPARE(btn.state(), VCButton::Inactive);
    VCButton another(&w, m_doc);
    QVERIFY(btn.palette().color(QPalette::Button) != another.palette().color(QPalette::Button));
    QTest::qWait(500);
    QVERIFY(btn.palette().color(QPalette::Button) == another.palette().color(QPalette::Button));

    QMouseEvent ev4(QEvent::MouseButtonRelease, QPoint(0, 0), QPoint(0, 0), QPoint(0, 0),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    btn.mouseReleaseEvent(&ev4);
}

void VCButton_Test::flash()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    m_doc->addFunction(sc);

    VCButton btn(&w, m_doc);
    btn.setCaption("Foobar");
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Flash);
    btn.setKeySequence(QKeySequence(keySequenceB));
    btn.enableStartupIntensity(false);
    btn.setStartupIntensity(qreal(0.2));

    QSignalSpy spy(sc, SIGNAL(flashing(quint32,bool)));

    m_doc->setMode(Doc::Operate);
    btn.slotKeyPressed(QKeySequence(keySequenceB));
    QCOMPARE(m_doc->masterTimer()->m_functionList.size(), 0);
    QCOMPARE(btn.state(), VCButton::Active);
    QCOMPARE(sc->getAttributeValue(Function::Intensity), qreal(1.0));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy[0][0].toUInt(), sc->id());
    QCOMPARE(spy[0][1].toBool(), true);

    btn.slotKeyReleased(QKeySequence(keySequenceB));
    QCOMPARE(btn.state(), VCButton::Inactive);
    QCOMPARE(spy.size(), 2);
    QCOMPARE(spy[1][0].toUInt(), sc->id());
    QCOMPARE(spy[1][1].toBool(), false);

    btn.slotFunctionFlashing(sc->id() + 1, true);
    QCOMPARE(btn.state(), VCButton::Inactive);

    m_doc->setMode(Doc::Design);
}

void VCButton_Test::input()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    sc->setValue(0, 0, 255);
    sc->setFadeInSpeed(1000);
    sc->setFadeOutSpeed(1000);
    m_doc->addFunction(sc);

    VCButton btn(&w, m_doc);
    btn.setCaption("Foobar");
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Flash);
    btn.enableStartupIntensity(true);
    btn.setStartupIntensity(qreal(1.0));
    btn.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 0)));

    btn.slotInputValueChanged(0, 0, 255);
    QCOMPARE(btn.state(), VCButton::Inactive);

    m_doc->setMode(Doc::Operate);

    btn.slotInputValueChanged(0, 0, 255);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.slotInputValueChanged(0, 0, 1);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(btn.state(), VCButton::Inactive);

    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(btn.state(), VCButton::Inactive);

    btn.slotInputValueChanged(0, 0, 1);
    QCOMPARE(btn.state(), VCButton::Active);

    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(btn.state(), VCButton::Inactive);

    btn.setAction(VCButton::Toggle);

    btn.slotInputValueChanged(0, 0, 255);
    m_doc->masterTimer()->timerTick();
    QCOMPARE(btn.state(), VCButton::Active);
    QCOMPARE(sc->getAttributeValue(Function::Intensity), btn.startupIntensity());

    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(sc->m_stop, false);

    btn.slotInputValueChanged(0, 0, 255);
    QCOMPARE(sc->m_stop, true);

    // Test that blackout gets toggled thru ext input
    btn.setAction(VCButton::Blackout);
    btn.slotInputValueChanged(0, 0, 1);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), true);
    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), true);
    btn.slotInputValueChanged(0, 0, 255);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), false);
    btn.slotInputValueChanged(0, 0, 0);
    QCOMPARE(m_doc->inputOutputMap()->blackout(), false);

    // Test that panic gets toggled thru ext input
    m_doc->masterTimer()->startFunction(sc);
    QCOMPARE(m_doc->masterTimer()->runningFunctions(), 1);
    m_doc->masterTimer()->start();
    btn.setAction(VCButton::StopAll);
    btn.slotInputValueChanged(0, 0, 1);
    QTest::qWait(100);
    QCOMPARE(m_doc->masterTimer()->runningFunctions(), 0);
    m_doc->masterTimer()->stop();
}

void VCButton_Test::stepButtonsOnATreeListGoThroughTheOneGoPath()
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::NonTracking);
    for (int i = 0; i < 4; ++i)
        st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    Chaser *tree = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(tree != NULL);

    QList<QPair<ShowControl::Command, ShowControl::Route>> seen;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route route) {
        seen << qMakePair(r.command, route);
        return sc->execute(r);
    });
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        tree->stop(FunctionParent::master());
        m_doc->masterTimer()->timerTick();
    });

    m_doc->setMode(Doc::Operate);
    MasterTimer *timer = m_doc->masterTimer();
    tree->start(timer, FunctionParent::master());
    timer->timerTick();
    timer->timerTick();
    QVERIFY(tree->isRunning());
    QTRY_VERIFY(m_doc->programmer()->m_runningChasers.contains(tree->id()));

    QWidget w;
    VCButton next(&w, m_doc);
    next.setAction(VCButton::ChaserStepNext);
    next.pressFunction();
    QCOMPARE(seen.size(), 1);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Go, ShowControl::RouteVcCueList));
    VCButton prev(&w, m_doc);
    prev.setAction(VCButton::ChaserStepPrev);
    prev.pressFunction();
    QCOMPARE(seen.size(), 2);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Back, ShowControl::RouteVcCueList));
}

void VCButton_Test::stepButtonsOnAPlainChaserStepDirectly()
{
    ShowControl *sc = m_doc->showControl();
    int routed = 0;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route) {
        ++routed;
        return sc->execute(r);
    });
    Chaser *c = NULL;
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        if (c != NULL)
            c->stop(FunctionParent::master());
        m_doc->masterTimer()->timerTick();
    });

    Scene *a = new Scene(m_doc);
    a->setValue(0, 0, 255);
    m_doc->addFunction(a);
    Scene *b = new Scene(m_doc);
    b->setValue(0, 0, 100);
    m_doc->addFunction(b);
    c = new Chaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    c->addStep(ChaserStep(a->id()));
    c->addStep(ChaserStep(b->id()));
    m_doc->addFunction(c);
    QVERIFY(!sc->isTreeGoList(c->id()));

    m_doc->setMode(Doc::Operate);
    MasterTimer *timer = m_doc->masterTimer();
    c->start(timer, FunctionParent::master());
    timer->timerTick();
    timer->timerTick();
    QVERIFY(c->isRunning());
    QTRY_VERIFY(m_doc->programmer()->m_runningChasers.contains(c->id()));
    QCOMPARE(c->currentStepIndex(), 0);

    QWidget w;
    VCButton next(&w, m_doc);
    next.setAction(VCButton::ChaserStepNext);
    next.pressFunction();
    timer->timerTick();
    timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 1);
    VCButton prev(&w, m_doc);
    prev.setAction(VCButton::ChaserStepPrev);
    prev.pressFunction();
    timer->timerTick();
    timer->timerTick();
    QCOMPARE(c->currentStepIndex(), 0);
    QCOMPARE(routed, 0);
}

void VCButton_Test::paint()
{
    QWidget w;

    VCButton btn(&w, m_doc);

    w.show();
    btn.show();

    QTest::qWait(1);

    btn.setState(VCButton::Active);
    btn.update();
    QTest::qWait(1);
    btn.setState(VCButton::Inactive);
    btn.update();
    QTest::qWait(1);
    btn.setIconPath("../../../resources/icons/png/qlcconsole.png");
    btn.update();
    QTest::qWait(1);
    btn.setCaption("Foobar");
    btn.update();
    QTest::qWait(1);
    btn.setAction(VCButton::Flash);
    btn.update();
    QTest::qWait(1);
    m_doc->setMode(Doc::Operate);
    btn.update();
    QTest::qWait(1);
}

/* 0.3.1 (k2): a Flash button on a Show Tree hit (a Collection the tree
   marks flashPlays) plays it while held and stops it on release. */
void VCButton_Test::flashPlaysAHit()
{
    QWidget w;

    Scene *sc = new Scene(m_doc);
    sc->setValue(0, 0, 255);
    m_doc->addFunction(sc);
    Collection *hit = new Collection(m_doc);
    hit->addFunction(sc->id());
    hit->setFlashPlays(true);
    m_doc->addFunction(hit);

    VCButton btn(&w, m_doc);
    btn.setFunction(hit->id());
    btn.setAction(VCButton::Flash);
    btn.setKeySequence(QKeySequence(keySequenceB));

    m_doc->setMode(Doc::Operate);
    btn.slotKeyPressed(QKeySequence(keySequenceB));
    m_doc->masterTimer()->timerTick();
    QCOMPARE(btn.state(), VCButton::Active);
    QVERIFY(m_doc->masterTimer()->m_functionList.contains(hit));
    QVERIFY(hit->flashing());

    btn.slotKeyReleased(QKeySequence(keySequenceB));
    QCOMPARE(btn.state(), VCButton::Inactive);
    QVERIFY(!hit->flashing());
    QCOMPARE(hit->m_stop, true);
    m_doc->masterTimer()->timerTick();
    m_doc->masterTimer()->timerTick();
    QVERIFY(!m_doc->masterTimer()->m_functionList.contains(hit));
    m_doc->setMode(Doc::Design);
}

/* A one-shot (Breathe, Once) in a Toggle hit: once its breath has ended the
   hit is still on, and the next press plays it again; a press while it still
   breathes turns the hit off. */
namespace
{
quint32 hitWith(Doc *doc, bool oneShot)
{
    FixtureGroup *grp = new FixtureGroup(doc);
    grp->setName("G");
    grp->setSize(QSize(1, 1));
    grp->assignFixture(doc->fixtures().first()->id());
    doc->addFixtureGroup(grp);
    Scene *sc = new Scene(doc);
    sc->setName("Hit scene");
    doc->addFunction(sc);
    sc->addFixtureGroup(grp->id());
    QLCPalette *level = new QLCPalette(QLCPalette::Dimmer, doc);
    level->setName("20%");
    level->setValue(51);
    doc->addPalette(level);
    sc->addPalette(level->id());
    if (oneShot)
    {
        QLCPalette *fx = new QLCPalette(QLCPalette::Effect, doc);
        fx->setName("Breathe");
        fx->setScriptPath(QStringLiteral(QLC_TEST_RESOURCE_ROOT) + "effectscripts/dimmer-phaser.js");
        doc->addPalette(fx);
        fx->setEffectParamValue("waveform", 0);
        fx->setEffectParamValue("rate", 2.0);               // a 0.5 s breath
        fx->setEffectParamValue("minDim", 0.05);
        fx->setEffectParamValue("maxDim", 1.0);
        fx->setEffectParamValue("spread", 0);
        fx->setEffectParamValue("__runs", 1);               // Once
        sc->addPalette(fx->id());
    }
    Collection *hit = new Collection(doc);
    doc->addFunction(hit);
    hit->addFunction(sc->id());
    return hit->id();
}

int level0(Doc *doc)
{
    QList<Universe *> us = doc->inputOutputMap()->claimUniverses();
    const int v = int(uchar(us.at(0)->preGMValues().at(0)));
    doc->inputOutputMap()->releaseUniverses(false);
    return v;
}
}

void VCButton_Test::toggleRestartsAFinishedOneShot()
{
    QWidget w;
    const quint32 hit = hitWith(m_doc, true);
    VCButton btn(&w, m_doc);
    btn.setFunction(hit);
    btn.setAction(VCButton::Toggle);
    btn.setKeySequence(QKeySequence(keySequenceB));
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
    m_doc->setMode(Doc::Operate);
    Function *coll = m_doc->function(hit);

    auto press = [&]() {
        btn.slotKeyPressed(QKeySequence(keySequenceB));
        btn.slotKeyReleased(QKeySequence(keySequenceB));
    };
    press();
    QCOMPARE(btn.state(), VCButton::Active);
    // Mid-breath: poll for the rise instead of sampling one wall-clock instant,
    // so a slow runner that overshoots 250 ms still sees the breath.
    QTRY_VERIFY2_WITH_TIMEOUT(level0(m_doc) > 20, qPrintable(QString::number(level0(m_doc))), 600);
    QTest::qWait(1200);                                             // the breath is over
    QVERIFY(coll->isRunning());
    QVERIFY2(level0(m_doc) < 15, qPrintable(QString::number(level0(m_doc))));

    press();                                                        // finished: again
    QCOMPARE(btn.state(), VCButton::Active);
    QVERIFY(coll->isRunning());
    QTRY_VERIFY2_WITH_TIMEOUT(level0(m_doc) > 20, qPrintable(QString::number(level0(m_doc))), 600);

    press();                                                        // breathing: off
    QTest::qWait(250);
    QVERIFY(!coll->isRunning());
    QCOMPARE(btn.state(), VCButton::Inactive);
    m_doc->setMode(Doc::Design);
    m_doc->masterTimer()->stop();
}

/* No one-shot in the hit: a press turns it on and the next one off. */
void VCButton_Test::toggleWithoutAOneShotTurnsOff()
{
    QWidget w;
    const quint32 hit = hitWith(m_doc, false);
    VCButton btn(&w, m_doc);
    btn.setFunction(hit);
    btn.setAction(VCButton::Toggle);
    btn.setKeySequence(QKeySequence(keySequenceB));
    m_doc->inputOutputMap()->startUniverses();
    m_doc->masterTimer()->start();
    m_doc->setMode(Doc::Operate);
    Function *coll = m_doc->function(hit);
    btn.slotKeyPressed(QKeySequence(keySequenceB));
    QCOMPARE(btn.state(), VCButton::Active);
    QTest::qWait(400);
    QVERIFY(coll->isRunning());
    btn.slotKeyPressed(QKeySequence(keySequenceB));
    QTest::qWait(250);
    QVERIFY(!coll->isRunning());
    QCOMPARE(btn.state(), VCButton::Inactive);
    m_doc->setMode(Doc::Design);
    m_doc->masterTimer()->stop();
}

/* A button fires a Show Tree hit itself: renamed, the hit's Collection
   deleted and made again, the button follows the hit. */
void VCButton_Test::firesAHitByIdentity()
{
    QWidget w;
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 hit = st->addLook(prod, "Stab", "Hits");
    const quint32 fid = st->lookFunction(hit);
    QVERIFY(fid != Function::invalidId());

    VCButton btn(&w, m_doc);
    btn.setHit(hit);
    QCOMPARE(btn.hit(), hit);
    QCOMPARE(btn.function(), fid);

    // Renamed: the same hit, the same Collection.
    ShowTree::Cue cue = *st->listOfCue(hit)->cue(hit);
    cue.name = "Big Stab";
    QVERIFY(st->setCue(cue));
    QCOMPARE(btn.function(), fid);
    QCOMPARE(btn.toolTip(), QString("Big Stab"));

    // Its Collection gone and made again (a new id): the button takes it.
    m_doc->deleteFunction(fid);
    QCOMPARE(btn.hit(), hit);
    Scene *taken = new Scene(m_doc);             // so the new one has a new id
    QVERIFY(m_doc->addFunction(taken, fid));
    st->bake();
    QCOMPARE(btn.function(), Function::invalidId());   // bake() alone says nothing
    QVERIFY(st->addLook(prod, "Other", "Hits") != Function::invalidId());
    const quint32 again = st->lookFunction(hit);
    QVERIFY(again != Function::invalidId());
    QVERIFY(again != fid);
    QCOMPARE(btn.function(), again);

    // Toggle fires it.
    btn.setAction(VCButton::Toggle);
    btn.setKeySequence(QKeySequence(keySequenceA));
    m_doc->setMode(Doc::Operate);
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    m_doc->masterTimer()->timerTick();
    QVERIFY(m_doc->masterTimer()->m_functionList.contains(m_doc->function(again)));
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    m_doc->masterTimer()->timerTick();
    m_doc->masterTimer()->timerTick();
    m_doc->setMode(Doc::Design);

    // Deleted: it fires nothing, and the hit back (undo) it fires it again.
    QVERIFY(st->removeCue(hit));
    QCOMPARE(btn.function(), Function::invalidId());
    QVERIFY(st->undo());
    QCOMPARE(btn.function(), st->lookFunction(hit));
    QVERIFY(btn.function() != Function::invalidId());

    // A plain function: no hit any more.
    btn.setHit(Function::invalidId());
    btn.setFunction(again);
    QCOMPARE(btn.hit(), Function::invalidId());
}

void VCButton_Test::hitSavesAndLoads()
{
    QWidget w;
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 hit = st->addLook(prod, "Stab");
    VCButton btn(&w, m_doc);
    btn.setHit(hit);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    xmlWriter.writeStartDocument();
    QVERIFY(btn.saveXML(&xmlWriter));
    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.buffer().contains(QStringLiteral("Hit=\"%1\"").arg(hit).toUtf8()));

    // The file's function id is stale (the tree made the Collection again
    // since): the hit wins.
    QByteArray xml = buffer.buffer();
    xml.replace(QStringLiteral("ID=\"%1\"").arg(st->lookFunction(hit)).toUtf8(), "ID=\"4242\"");
    QBuffer in(&xml);
    in.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&in);
    xmlReader.readNextStartElement();
    VCButton loaded(&w, m_doc);
    QVERIFY(loaded.loadXML(xmlReader));
    QCOMPARE(loaded.hit(), hit);
    QCOMPARE(loaded.function(), st->lookFunction(hit));

    // An old show (no Hit): as it was.
    Scene *sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    VCButton plain(&w, m_doc);
    plain.setFunction(sc->id());
    QBuffer b2;
    b2.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter w2(&b2);
    QVERIFY(plain.saveXML(&w2));
    w2.setDevice(NULL);
    QVERIFY(!b2.buffer().contains("Hit="));
}

/* The button's function picker: a Hits section with each hit, by
   production and folder; picked there, the button fires the hit. */
void VCButton_Test::pickerHasAHitsSection()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("DanceHysteria");
    const quint32 hit = st->addLook(prod, "Stab", "Big");
    const quint32 fid = st->lookFunction(hit);

    QWidget w;
    FunctionSelection fs(&w, m_doc);
    fs.setFilter(Function::SceneType, true);     // no Collections: hits still there
    fs.showHits(true);
    fs.m_isInitializing = false;
    fs.refillTree();
    QTreeWidgetItem *section = nullptr;
    for (int i = 0; i < fs.m_funcTree->topLevelItemCount(); ++i)
        if (fs.m_funcTree->topLevelItem(i)->text(0) == "Hits")
            section = fs.m_funcTree->topLevelItem(i);
    QVERIFY(section != nullptr);
    QCOMPARE(section->child(0)->text(0), QString("DanceHysteria"));
    QCOMPARE(section->child(0)->child(0)->text(0), QString("Big"));
    QTreeWidgetItem *item = section->child(0)->child(0)->child(0);
    QCOMPARE(item->text(0), QString("Stab"));
    item->setSelected(true);
    fs.slotItemSelectionChanged();
    QCOMPARE(fs.selection(), QList<quint32>() << fid);
    QCOMPARE(fs.selectedHit(), hit);

    // Only once: not again under Collection.
    FunctionSelection all(&w, m_doc);
    all.showHits(true);
    all.m_isInitializing = false;
    all.refillTree();
    int found = 0;
    std::function<void(QTreeWidgetItem *)> count = [&](QTreeWidgetItem *it) {
        if (it->data(0, Qt::UserRole).toUInt() == fid)
            ++found;
        for (int i = 0; i < it->childCount(); ++i)
            count(it->child(i));
    };
    for (int i = 0; i < all.m_funcTree->topLevelItemCount(); ++i)
        count(all.m_funcTree->topLevelItem(i));
    QCOMPARE(found, 1);

    // The properties dialog binds the button to the hit.
    VCButton btn(&w, m_doc);
    VCButtonProperties props(&btn, m_doc);
    props.slotSetFunction(fid);
    props.m_hits = { hit };
    props.accept();
    QCOMPARE(btn.hit(), hit);
    QCOMPARE(btn.function(), fid);
}

/* Scoped hits (Q154): a button holds several hits and fires the armed one
   with the narrowest place; none armed, it does nothing; what it held
   when its place ended is let go. */
void VCButton_Test::severalHitsFireTheArmedOne()
{
    QWidget w;
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 c2 = st->addCue(list, 0, "Two");
    const quint32 onCue2 = st->addHit(ShowTree::HitScope::Cue, c2, "Strobe");
    const quint32 everywhere = st->addHit(ShowTree::HitScope::Global, prod, "Blackout");
    Q_UNUSED(c1);

    VCButton btn(&w, m_doc);
    btn.setAction(VCButton::Toggle);
    btn.setKeySequence(QKeySequence(keySequenceA));
    btn.setHits({ everywhere, onCue2 });
    QCOMPARE(btn.hit(), everywhere);
    QCOMPARE(btn.hits(), QList<quint32>({ everywhere, onCue2 }));
    QCOMPARE(btn.armedHit(), everywhere);
    QCOMPARE(btn.function(), st->lookFunction(everywhere));

    // Only the cue's hit: nothing armed, nothing fires.
    VCButton only(&w, m_doc);
    only.setAction(VCButton::Toggle);
    only.setKeySequence(QKeySequence(keySequenceB));
    only.setHit(onCue2);
    QCOMPARE(only.armedHit(), Function::invalidId());
    QCOMPARE(only.function(), Function::invalidId());
    m_doc->setMode(Doc::Operate);
    only.slotKeyPressed(QKeySequence(keySequenceB));
    QCOMPARE(only.state(), VCButton::Inactive);

    // GO to cue 2: its hit wins on both buttons.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(chaser != nullptr);
    auto go = [&]() {
        ChaserAction action;
        action.m_action = ChaserNextStep;
        action.m_stepIndex = -1;
        action.m_masterIntensity = 1.0;
        action.m_stepIntensity = 1.0;
        action.m_fadeMode = Chaser::FromFunction;
        chaser->setAction(action);
        if (!chaser->isRunning())
            chaser->start(m_doc->masterTimer(), FunctionParent::master());
        for (int i = 0; i < 3; ++i)
            m_doc->masterTimer()->timerTick();
        QCoreApplication::processEvents();
    };
    go();
    QCOMPARE(btn.armedHit(), everywhere);
    go();
    QCOMPARE(st->runningCue(list), c2);
    QCOMPARE(btn.armedHit(), onCue2);
    QCOMPARE(btn.function(), st->lookFunction(onCue2));
    QCOMPARE(only.function(), st->lookFunction(onCue2));

    // Fired, then the list stops: the hit is let go, the button falls back.
    Function *strobe = m_doc->function(st->lookFunction(onCue2));
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    m_doc->masterTimer()->timerTick();
    QCOMPARE(btn.state(), VCButton::Active);
    QVERIFY(strobe->isRunning());
    chaser->stop(FunctionParent::master());
    m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QCOMPARE(btn.state(), VCButton::Inactive);
    QVERIFY(strobe->stopped() || strobe->m_stop);
    QCOMPARE(btn.armedHit(), everywhere);
    QCOMPARE(btn.function(), st->lookFunction(everywhere));
    QCOMPARE(only.armedHit(), Function::invalidId());
    m_doc->masterTimer()->timerTick();
    m_doc->masterTimer()->timerTick();
    m_doc->setMode(Doc::Design);

    // A plain function given: no hits any more.
    Scene *sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    btn.setFunction(sc->id());
    QVERIFY(btn.hits().isEmpty());
    QCOMPARE(btn.armedHit(), Function::invalidId());
}

// QDragEnterEvent takes a QPointF from Qt 6.12 (the QPoint one is deprecated).
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
static const QPointF kDragAt(1, 1);
#else
static const QPoint kDragAt(1, 1);
#endif

void VCButton_Test::severalHitsSaveLoadAndDrop()
{
    QWidget w;
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 a = st->addLook(prod, "Wash");
    const quint32 b = st->addHit(ShowTree::HitScope::Cue, c1, "Strobe");

    // A hit dragged from the Show Tree onto a plain button: it fires it.
    Scene *sc = new Scene(m_doc);
    m_doc->addFunction(sc);
    VCButton btn(&w, m_doc);
    btn.setFunction(sc->id());
    btn.setAction(VCButton::Blackout);
    QByteArray data;
    {
        QDataStream out(&data, QIODevice::WriteOnly);
        out << a;
    }
    QMimeData mime;
    mime.setData(VCButton::hitMimeType(), data);
    QDragEnterEvent enter(kDragAt, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    btn.dragEnterEvent(&enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(1, 1), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    btn.dropEvent(&drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(btn.hits(), QList<quint32>({ a }));
    QCOMPARE(btn.action(), VCButton::Toggle);
    QCOMPARE(btn.function(), st->lookFunction(a));
    // A second: added; again: nothing.
    QVERIFY(btn.addHits({ b }));
    QVERIFY(!btn.addHits({ b, a }));
    QCOMPARE(btn.hits(), QList<quint32>({ a, b }));
    // Not a hit: refused.
    QByteArray notHit;
    {
        QDataStream out(&notHit, QIODevice::WriteOnly);
        out << c1;
    }
    QMimeData cueMime;
    cueMime.setData(VCButton::hitMimeType(), notHit);
    QDragEnterEvent enter2(kDragAt, Qt::CopyAction, &cueMime, Qt::LeftButton, Qt::NoModifier);
    btn.dragEnterEvent(&enter2);
    QVERIFY(!enter2.isAccepted());

    // Saved: the first as Hit=, the others as <Hit>; read back the same.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    QVERIFY(btn.saveXML(&xmlWriter));
    xmlWriter.setDevice(NULL);
    buffer.close();
    QByteArray xml = buffer.buffer();
    QVERIFY(xml.contains(QStringLiteral("Hit=\"%1\"").arg(a).toUtf8()));
    QVERIFY(xml.contains(QStringLiteral("<Hit ID=\"%1\"/>").arg(b).toUtf8()));
    {
        QBuffer in(&xml);
        in.open(QIODevice::ReadOnly | QIODevice::Text);
        QXmlStreamReader r(&in);
        r.readNextStartElement();
        VCButton loaded(&w, m_doc);
        QVERIFY(loaded.loadXML(r));
        QCOMPARE(loaded.hits(), QList<quint32>({ a, b }));
        QCOMPARE(loaded.function(), st->lookFunction(a));
    }
    // The properties dialog keeps them, and Remove takes one off.
    VCButtonProperties props(&btn, m_doc);
    QCOMPARE(props.m_hits, QList<quint32>({ a, b }));
    QCOMPARE(props.m_hitsTree->topLevelItemCount(), 2);
    QVERIFY(props.m_hitsTree->topLevelItem(1)->text(1).contains("cue"));
    props.m_hitsTree->setCurrentItem(props.m_hitsTree->topLevelItem(1));
    props.findChild<QPushButton *>("vcbRemoveHit")->click();
    props.accept();
    QCOMPARE(btn.hits(), QList<quint32>({ a }));
}

/* Q157: what a hit button does across the show, in words and as a table;
   a second hit at one place asks Step through / Together / Replace /
   Cancel; a narrower one asks to override; Show checks list it all; the
   face marks an override. */
void VCButton_Test::hitButtonMapAsksAndChecks()
{
    QWidget w;
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("P");
    const quint32 list = st->addCueList(prod, "Main");
    const quint32 c1 = st->addCue(list, -1, "One");
    const quint32 blackout = st->addHit(ShowTree::HitScope::Global, prod, "Blackout");
    const quint32 strobe = st->addHit(ShowTree::HitScope::Cue, c1, "Strobe");
    const quint32 pop = st->addHit(ShowTree::HitScope::Cue, c1, "Pop");
    const quint32 red = st->addHit(ShowTree::HitScope::List, list, "Red");

    QStringList asked;
    QList<int> answers;
    HitMap::setAsker([&](const QString &title, const QString &, const QStringList &choices) {
        asked << title;
        return answers.isEmpty() ? int(choices.count()) - 1 : answers.takeFirst();
    });

    VCButton btn(&w, m_doc);
    btn.setCaption("HIT 1");
    QVERIFY(btn.addHitsAsking({ blackout }, nullptr));
    QVERIFY(asked.isEmpty());                       // the first: nothing to ask
    // Narrower: override there? Cancel -- not added; Override -- added.
    QVERIFY(!btn.addHitsAsking({ strobe }, nullptr));
    QCOMPARE(asked, QStringList({ "Override there?" }));
    answers << 0;
    QVERIFY(btn.addHitsAsking({ strobe }, nullptr));
    QCOMPARE(btn.hits(), QList<quint32>({ blackout, strobe }));
    // Same place: Step through.
    asked.clear();
    answers << 0;
    QVERIFY(btn.addHitsAsking({ pop }, nullptr));
    QCOMPARE(asked, QStringList({ "Two hits in one place" }));
    QCOMPARE(btn.samePlace(), VCButton::StepThrough);
    // Replace: the one there goes.
    VCButton other(&w, m_doc);
    other.setHits({ strobe });
    answers << 2;
    QVERIFY(other.addHitsAsking({ pop }, nullptr));
    QCOMPARE(other.hits(), QList<quint32>({ pop }));

    // The map, in words and as rows.
    answers << 0;
    QVERIFY(btn.addHitsAsking({ red }, nullptr));
    const QString map = HitMap::sentence(m_doc, "HIT 1", btn.hits(), btn.samePlace());
    QVERIFY2(map.startsWith("HIT 1 fires Blackout everywhere, except Strobe in cue"), qPrintable(map));
    QVERIFY2(map.contains("(overrides Blackout)"), qPrintable(map));
    QVERIFY2(map.contains("Red in cue list Main (overrides Blackout)"), qPrintable(map));
    QVERIFY2(map.contains("2 hits live in cue"), qPrintable(map));
    QVERIFY2(map.contains("step through"), qPrintable(map));
    const QList<HitMap::Row> rows = HitMap::rows(m_doc, btn.hits(), btn.samePlace());
    QCOMPARE(rows.count(), 4);
    QCOMPARE(rows.at(0).when, QString("Always"));
    QVERIFY(rows.at(1).note.contains("overrides Blackout, Red"));
    QVERIFY(rows.at(1).note.contains("shares its place with Pop"));
    QVERIFY(st->hitOverrides(strobe, red));
    QVERIFY(!st->hitOverrides(red, strobe));
    QVERIFY(st->sameHitPlace(strobe, pop));

    // Saved: the same-place choice comes back.
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xw(&buffer);
    QVERIFY(btn.saveXML(&xw));
    xw.setDevice(NULL);
    buffer.close();
    QVERIFY(buffer.buffer().contains("SamePlace=\"Step\""));
    {
        QByteArray xml = buffer.buffer();
        QBuffer in(&xml);
        in.open(QIODevice::ReadOnly | QIODevice::Text);
        QXmlStreamReader r(&in);
        r.readNextStartElement();
        VCButton loaded(&w, m_doc);
        QVERIFY(loaded.loadXML(r));
        QCOMPARE(loaded.samePlace(), VCButton::StepThrough);
        QCOMPARE(loaded.hits().count(), 4);
    }

    // Show checks: the overrides and the pair (Ok: told how to fire them).
    VCButton *onVc = new VCButton(VirtualConsole::instance()->contents(), m_doc);
    onVc->setCaption("HIT 1");
    onVc->setSamePlace(VCButton::StepThrough);
    onVc->setHits(btn.hits());
    ShowStatus::Entry e;
    for (const ShowStatus::Entry &x : ShowStatus::instance()->allEntries())
        if (x.key == "hits.places")
            e = x;
    QCOMPARE(e.key, QString("hits.places"));
    QCOMPARE(e.severity, ShowStatus::Ok);
    QVERIFY(e.items.join("\n").contains("HIT 1: Strobe (in cue"));
    QVERIFY(e.items.join("\n").contains("share a place"));
    QCOMPARE(HitMap::buttonNames(strobe), QStringList({ "HIT 1" }));
    QVERIFY(HitMap::overridesOnAButton(m_doc, strobe));
    QVERIFY(!HitMap::overridesOnAButton(m_doc, blackout));
    // A deleted hit: a warning.
    QVERIFY(st->removeCue(red));
    for (const ShowStatus::Entry &x : ShowStatus::instance()->allEntries())
        if (x.key == "hits.places")
            e = x;
    QCOMPARE(e.severity, ShowStatus::Warning);
    QVERIFY(e.items.join("\n").contains("deleted"));
    QVERIFY(st->undo());

    // Running: list on cue 1 -- Step through fires Strobe, then Pop; the
    // face marks the override.
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    ChaserAction action;
    action.m_action = ChaserNextStep;
    action.m_stepIndex = -1;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    chaser->setAction(action);
    chaser->start(m_doc->masterTimer(), FunctionParent::master());
    for (int i = 0; i < 3; ++i)
        m_doc->masterTimer()->timerTick();
    QCoreApplication::processEvents();
    QCOMPARE(st->armedTier(btn.hits()), QList<quint32>({ strobe, pop }));
    QVERIFY(btn.armedOverrides());
    btn.setAction(VCButton::Flash);
    btn.setKeySequence(QKeySequence(keySequenceA));
    m_doc->setMode(Doc::Operate);
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    QCOMPARE(btn.armedHit(), strobe);
    btn.slotKeyReleased(QKeySequence(keySequenceA));
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    QCOMPARE(btn.armedHit(), pop);
    QCOMPARE(btn.function(), st->lookFunction(pop));
    btn.slotKeyReleased(QKeySequence(keySequenceA));
    // Together: both at once.
    btn.setSamePlace(VCButton::Together);
    btn.slotKeyPressed(QKeySequence(keySequenceA));
    QCOMPARE(btn.m_together.count(), 1);
    QVERIFY(m_doc->function(btn.m_together.first())->flashing()
            || m_doc->function(st->lookFunction(strobe))->flashing()
            || m_doc->function(st->lookFunction(pop))->flashing());
    btn.slotKeyReleased(QKeySequence(keySequenceA));
    QVERIFY(btn.m_together.isEmpty());
    chaser->stop(FunctionParent::master());
    m_doc->masterTimer()->timerTick();
    m_doc->setMode(Doc::Design);
    HitMap::setAsker(HitMap::Asker());
    delete onVc;
}

void VCButton_Test::toggleAndFlash()
{
    QWidget w;

    Scene* sc = new Scene(m_doc);
    m_doc->addFunction(sc);

    VCButton toggleBtn(&w, m_doc);
    toggleBtn.setFunction(sc->id());
    toggleBtn.setAction(VCButton::Toggle);
    toggleBtn.setKeySequence(QKeySequence(keySequenceA));

    VCButton flashBtn(&w, m_doc);
    flashBtn.setFunction(sc->id());
    flashBtn.setAction(VCButton::Flash);
    flashBtn.setKeySequence(QKeySequence(keySequenceB));

    m_doc->setMode(Doc::Operate);

    // push toggle button
    toggleBtn.slotKeyPressed(QKeySequence(keySequenceA));
    // tell MasterTimer to process start queue
    m_doc->masterTimer()->timerTick();
    QCOMPARE(toggleBtn.state(), VCButton::Active);
    QCOMPARE(flashBtn.state(), VCButton::Inactive);

    // push flash button
    flashBtn.slotKeyPressed(QKeySequence(keySequenceB));
    QCOMPARE(toggleBtn.state(), VCButton::Active);
    QCOMPARE(flashBtn.state(), VCButton::Active);

    // flash button released
    flashBtn.slotKeyReleased(QKeySequence(keySequenceB));
    QCOMPARE(toggleBtn.state(), VCButton::Active);
    QCOMPARE(flashBtn.state(), VCButton::Inactive);

    // push toggle button once more
    toggleBtn.slotKeyPressed(QKeySequence(keySequenceA));
    // tell MasterTimer to process start queue
    m_doc->masterTimer()->timerTick();
    QCOMPARE(toggleBtn.state(), VCButton::Inactive);
    QCOMPARE(flashBtn.state(), VCButton::Inactive);
    QCOMPARE(sc->m_stop, true);
    toggleBtn.slotFunctionStopped(sc->id());
}

QTEST_MAIN(VCButton_Test)
