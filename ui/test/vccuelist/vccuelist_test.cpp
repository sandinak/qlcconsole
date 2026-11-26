/*
  Q Light Controller Plus - Test Unit
  vccuelist_test.cpp

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
#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSignalSpy>
#include <QtTest>

#include "../common/shotdark.h"
#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "virtualconsole.h"
#include "genericfader.h"
#include "chaserrunner.h"
#include "mastertimer.h"
#include "universe.h"
#include "showcontrol.h"
#include "vccuelist.h"
#include "vcwidget.h"
#include "vcslider.h"
#include "vcsliderproperties.h"
#include "vcframe.h"
#include "vcsoloframe.h"

#include "qlcfixturedefcache.h"
#include "qlcinputsource.h"
#include "vccuelist_test.h"
#include "chaserstep.h"
#include "chaser.h"
#include "scene.h"
#include "efx.h"
#include "showtreecontroller.h"
#include "doc.h"
#include "showtreemodel.h"
#include "outputrulekeeper.h"
#include "functionparent.h"
#undef private
#undef protected

static const QKeySequence keySequenceA(Qt::Key_A);
static const QKeySequence keySequenceB(Qt::Key_B);
static const QKeySequence keySequenceC(Qt::Key_C);
static const QKeySequence keySequenceD(Qt::Key_D);

static Chaser* createChaser(Doc* doc)
{
    Fixture* fxi = new Fixture(doc);
    fxi->setChannels(1);
    doc->addFixture(fxi);

    Scene* s1 = new Scene(doc);
    s1->setName("First");
    s1->setValue(fxi->id(), 0, 255);
    doc->addFunction(s1);

    Scene* s2 = new Scene(doc);
    s2->setName("Second");
    s2->setValue(fxi->id(), 0, 127);
    doc->addFunction(s2);

    Scene* s3 = new Scene(doc);
    s3->setName("Third");
    s3->setValue(fxi->id(), 0, 64);
    doc->addFunction(s3);

    Scene* s4 = new Scene(doc);
    s4->setName("Fourth");
    s4->setValue(fxi->id(), 0, 32);
    doc->addFunction(s4);

    Chaser* c = new Chaser(doc);
    c->addStep(ChaserStep(s1->id()));
    c->addStep(ChaserStep(s2->id()));
    c->addStep(ChaserStep(s3->id()));
    c->addStep(ChaserStep(s4->id()));
    doc->addFunction(c);

    return c;
}

void VCCueList_Test::initTestCase()
{
    m_doc = NULL;
}

void VCCueList_Test::init()
{
    m_doc = new Doc(this);
    new VirtualConsole(NULL, m_doc);
}

void VCCueList_Test::cleanup()
{
    delete VirtualConsole::instance();
    delete m_doc;
}

void VCCueList_Test::initial()
{
    QWidget w;

    VCCueList cl(&w, m_doc);
    QCOMPARE(cl.objectName(), QString("VCCueList"));
    QCOMPARE(cl.frameStyle(), QFrame::Panel | QFrame::Sunken);
    QCOMPARE(cl.caption(), tr("Cue list"));
    QCOMPARE(cl.size(), QSize(300, 220));
    // QVERIFY(cl.m_runner == NULL);
    QVERIFY(cl.m_tree != NULL);
    QCOMPARE(cl.m_tree->isEnabled(), false);
    QCOMPARE(cl.m_tree->topLevelItemCount(), 0);
    QCOMPARE(cl.m_tree->selectionMode(), QAbstractItemView::SingleSelection);
    QCOMPARE(cl.m_tree->rootIsDecorated(), false);
    QCOMPARE(cl.chaserID(), Function::invalidId());

    QCOMPARE(cl.m_nextLatestValue, quint32(0));
    QCOMPARE(cl.m_previousLatestValue, quint32(0));
    QCOMPARE(cl.m_playbackLatestValue, quint32(0));

    QCOMPARE(cl.m_nextKeySequence, QKeySequence());
    QCOMPARE(cl.m_previousKeySequence, QKeySequence());
    QCOMPARE(cl.m_playbackKeySequence, QKeySequence());
    QCOMPARE(cl.m_stopKeySequence, QKeySequence());

    QVERIFY(cl.inputSource(VCCueList::nextInputSourceId) == NULL);
    QVERIFY(cl.inputSource(VCCueList::previousInputSourceId) == NULL);
    QVERIFY(cl.inputSource(VCCueList::playbackInputSourceId) == NULL);
    QVERIFY(cl.inputSource(VCCueList::stopInputSourceId) == NULL);
}

void VCCueList_Test::chaser()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);

    // Try to put a non-chaser as the chaser
    cl.setChaser(c->steps().first().fid);
    QCOMPARE(cl.chaserID(), Function::invalidId());
    QCOMPARE(cl.m_tree->topLevelItemCount(), 0);

    // Put a real chaser as the chaser
    cl.setChaser(c->id());
    QCOMPARE(cl.chaserID(), c->id());
    QCOMPARE(cl.m_tree->topLevelItemCount(), 4);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(0), QString("1"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(0), QString("2"));
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(0), QString("3"));
    QCOMPARE(cl.m_tree->topLevelItem(3)->text(0), QString("4"));
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(1), QString("First"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(1), QString("Second"));
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(1), QString("Third"));
    QCOMPARE(cl.m_tree->topLevelItem(3)->text(1), QString("Fourth"));
}

void VCCueList_Test::functionRemoved()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    cl.setChaser(c->id());

    // Chaser members are removed from list
    m_doc->deleteFunction(c->steps().first().fid);
    QCOMPARE(cl.m_tree->topLevelItemCount(), 4);
    // The refresh is deferred by a 100 ms timer. Waiting a fixed 150 ms and
    // asserting assumes the runner delivers that timer on schedule; a loaded
    // CI machine does not, and this failed on roughly one macOS run in five.
    // QTRY_COMPARE polls until the condition holds (or times out), which is
    // the same assertion without the guess about how long it takes.
    QTRY_COMPARE(cl.m_tree->topLevelItemCount(), 3);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(0), QString("1"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(0), QString("2"));
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(0), QString("3"));
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(1), QString("Second"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(1), QString("Third"));
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(1), QString("Fourth"));

    // Chaser is removed completely
    m_doc->deleteFunction(c->id());
    QTRY_COMPARE(cl.m_tree->topLevelItemCount(), 0);
    QCOMPARE(cl.chaserID(), Function::invalidId());
}

void VCCueList_Test::functionChanged()
{
}

void VCCueList_Test::keySequences()
{
    QWidget w;

    VCCueList cl(&w, m_doc);
    cl.setNextKeySequence(QKeySequence(keySequenceB));
    QCOMPARE(cl.nextKeySequence(), QKeySequence(keySequenceB));
    QCOMPARE(cl.previousKeySequence(), QKeySequence());
    QCOMPARE(cl.playbackKeySequence(), QKeySequence());

    cl.setPreviousKeySequence(QKeySequence(keySequenceA));
    QCOMPARE(cl.nextKeySequence(), QKeySequence(keySequenceB));
    QCOMPARE(cl.previousKeySequence(), QKeySequence(keySequenceA));
    QCOMPARE(cl.playbackKeySequence(), QKeySequence());

    cl.setPlaybackKeySequence(QKeySequence(keySequenceD));
    QCOMPARE(cl.nextKeySequence(), QKeySequence(keySequenceB));
    QCOMPARE(cl.previousKeySequence(), QKeySequence(keySequenceA));
    QCOMPARE(cl.playbackKeySequence(), QKeySequence(keySequenceD));
}

void VCCueList_Test::copy()
{
    // Input sources are tested by VCWidget tests. No point testing here.

    QWidget w;
    VCFrame parent(&w, m_doc);
    VCCueList cl(&parent, m_doc);
    Chaser* c = createChaser(m_doc);

    cl.setChaser(c->id());
    cl.setCaption("Wheeee");
    cl.setNextKeySequence(QKeySequence(keySequenceB));
    cl.setPreviousKeySequence(QKeySequence(keySequenceA));
    cl.setPlaybackKeySequence(QKeySequence(keySequenceC));

    VCCueList* cl2 = qobject_cast<VCCueList*> (cl.createCopy(&parent));
    QVERIFY(cl2 != NULL);
    QCOMPARE(cl2->caption(), QString("Wheeee"));
    QCOMPARE(cl2->nextKeySequence(), QKeySequence(keySequenceB));
    QCOMPARE(cl2->previousKeySequence(), QKeySequence(keySequenceA));
    QCOMPARE(cl2->playbackKeySequence(), QKeySequence(keySequenceC));
    QCOMPARE(cl2->chaserID(), c->id());
    QCOMPARE(cl2->m_tree->topLevelItemCount(), 4);

    VCCueList cl3(&parent, m_doc);
    cl3.copyFrom(NULL);
    QCOMPARE(cl3.m_tree->topLevelItemCount(), 0);
    QCOMPARE(cl3.caption(), tr("Cue list"));

    cl.copyFrom(&cl3);
    QCOMPARE(cl.caption(), cl3.caption());
    QCOMPARE(cl.chaserID(), Function::invalidId());
    QCOMPARE(cl.m_tree->topLevelItemCount(), 0);
    QCOMPARE(cl.nextKeySequence(), QKeySequence());
    QCOMPARE(cl.previousKeySequence(), QKeySequence());
    QCOMPARE(cl.playbackKeySequence(), QKeySequence());

    delete cl2;
}

void VCCueList_Test::modeChange()
{
    QWidget w;
    VCFrame parent(&w, m_doc);
    VCCueList cl(&parent, m_doc);
    Chaser* c = createChaser(m_doc);
    cl.setChaser(c->id());

    m_doc->setMode(Doc::Operate);
    // QCOMPARE(m_doc->masterTimer()->m_dmxSourceList.size(), 1);
    // QCOMPARE(m_doc->masterTimer()->m_dmxSourceList[0], &cl);
    // QVERIFY(cl.m_runner == NULL);
    QVERIFY(cl.m_tree->isEnabled() == true);

    // cl.createRunner();

    m_doc->setMode(Doc::Design);
    QCOMPARE(m_doc->masterTimer()->m_dmxSourceList.size(), 0);
    // QVERIFY(cl.m_runner == NULL);
    QVERIFY(cl.m_tree->isEnabled() == false);
}

void VCCueList_Test::loadXML()
{
    QWidget w;

    Scene* s1 = new Scene(m_doc);
    s1->setName("The first");
    m_doc->addFunction(s1);

    Scene* s2 = new Scene(m_doc);
    s2->setName("Another one");
    m_doc->addFunction(s2);

    Scene* s3 = new Scene(m_doc);
    s3->setName("The third one");
    m_doc->addFunction(s3);

    Chaser* c4 = new Chaser(m_doc);
    c4->setName("The defiant one");
    m_doc->addFunction(c4);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    xmlWriter.writeStartElement("CueList");
    xmlWriter.writeAttribute("Caption", "Test list");

    xmlWriter.writeStartElement("WindowState");
    xmlWriter.writeAttribute("Width", "42");
    xmlWriter.writeAttribute("Height", "69");
    xmlWriter.writeAttribute("X", "3");
    xmlWriter.writeAttribute("Y", "4");
    xmlWriter.writeAttribute("Visible", "True");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Appearance");
    QFont f(w.font());
    f.setPointSize(f.pointSize() + 3);
    xmlWriter.writeTextElement("Font", f.toString());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Next");
    xmlWriter.writeStartElement("Input");
    xmlWriter.writeAttribute("Universe", "0");
    xmlWriter.writeAttribute("Channel", "1");
    xmlWriter.writeEndElement();
    xmlWriter.writeTextElement("Key", QKeySequence(keySequenceD).toString());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Previous");
    xmlWriter.writeStartElement("Input");
    xmlWriter.writeAttribute("Universe", "2");
    xmlWriter.writeAttribute("Channel", "3");
    xmlWriter.writeEndElement();
    xmlWriter.writeTextElement("Key", QKeySequence(keySequenceC).toString());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Playback");
    xmlWriter.writeStartElement("Input");
    xmlWriter.writeAttribute("Universe", "4");
    xmlWriter.writeAttribute("Channel", "5");
    xmlWriter.writeEndElement();
    xmlWriter.writeTextElement("Key", QKeySequence(keySequenceA).toString());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Stop");
    xmlWriter.writeStartElement("Input");
    xmlWriter.writeAttribute("Universe", "0");
    xmlWriter.writeAttribute("Channel", "6");
    xmlWriter.writeEndElement();
    xmlWriter.writeTextElement("Key", QKeySequence(keySequenceB).toString());
    xmlWriter.writeEndElement();

    xmlWriter.writeStartElement("Foo");
    xmlWriter.writeEndElement();

    xmlWriter.writeTextElement("Function", QString::number(s1->id()));
    xmlWriter.writeTextElement("Function", QString::number(s2->id()));
    xmlWriter.writeTextElement("Function", QString::number(s3->id()));
    xmlWriter.writeTextElement("Function", QString::number(c4->id()));
    xmlWriter.writeTextElement("Function", QString::number(INT_MAX - 1));

    // Make sure that nonexistent (id:31337) functions don't appear in the list
    xmlWriter.writeTextElement("Function", QString::number(31337));

    xmlWriter.writeStartElement("Foobar");
    xmlWriter.writeEndElement();

    xmlWriter.writeEndDocument();
    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    VCCueList cl(&w, m_doc);
    QVERIFY(cl.loadXML(xmlReader) == true);
    QCOMPARE(cl.m_tree->topLevelItemCount(), 4);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(0).toInt(), 1);
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(0).toInt(), 2);
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(0).toInt(), 3);
    QCOMPARE(cl.m_tree->topLevelItem(3)->text(0).toInt(), 4);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(1), s1->name());
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(1), s2->name());
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(1), s3->name());
    QCOMPARE(cl.m_tree->topLevelItem(3)->text(1), c4->name());
    QSharedPointer<QLCInputSource> const& ni = cl.inputSource(VCCueList::nextInputSourceId);
    QCOMPARE(ni->universe(), quint32(0));
    QCOMPARE(ni->channel(), quint32(1));
    QCOMPARE(cl.nextKeySequence(), QKeySequence(keySequenceD));
    QSharedPointer<QLCInputSource> const& pi = cl.inputSource(VCCueList::previousInputSourceId);
    QCOMPARE(pi->universe(), quint32(2));
    QCOMPARE(pi->channel(), quint32(3));
    QCOMPARE(cl.previousKeySequence(), QKeySequence(keySequenceC));
    QSharedPointer<QLCInputSource> const& pli = cl.inputSource(VCCueList::playbackInputSourceId);
    QCOMPARE(pli->universe(), quint32(4));
    QCOMPARE(pli->channel(), quint32(5));
    QCOMPARE(cl.playbackKeySequence(), QKeySequence(keySequenceA));
    QSharedPointer<QLCInputSource> const& si = cl.inputSource(VCCueList::stopInputSourceId);
    QCOMPARE(si->universe(), quint32(0));
    QCOMPARE(si->channel(), quint32(6));
    QCOMPARE(cl.stopKeySequence(), QKeySequence(keySequenceB));

    QCOMPARE(cl.pos(), QPoint(3, 4));
    QCOMPARE(cl.size(), QSize(42, 69));
    QCOMPARE(cl.font().toString(), f.toString());

    cl.postLoad();
    QCOMPARE(cl.m_tree->topLevelItemCount(), 4);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(0).toInt(), 1);
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(0).toInt(), 2);
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(0).toInt(), 3);
    QCOMPARE(cl.m_tree->topLevelItem(0)->text(1), s1->name());
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(1), s2->name());
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(1), s3->name());
    QCOMPARE(cl.m_tree->topLevelItem(3)->text(1), c4->name());

    buffer.close();
    QByteArray bData = buffer.data();
    bData.replace("<CueList", "<CueLits");
    buffer.setData(bData);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    buffer.seek(0);
    xmlReader.setDevice(&buffer);
    xmlReader.readNextStartElement();

    QVERIFY(cl.loadXML(xmlReader) == false);
}

void VCCueList_Test::saveXML()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    cl.setChaser(c->id());

    cl.setCaption("Testing");
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 1)), VCCueList::nextInputSourceId);
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(1, 2)), VCCueList::previousInputSourceId);
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(2, 3)), VCCueList::playbackInputSourceId);
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(3, 4)), VCCueList::stopInputSourceId);
    cl.setNextKeySequence(QKeySequence(keySequenceB));
    cl.setPreviousKeySequence(QKeySequence(keySequenceA));
    cl.setPlaybackKeySequence(QKeySequence(keySequenceC));
    cl.setStopKeySequence(QKeySequence(keySequenceD));
    cl.setSideFaderMode(VCCueList::Crossfade);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);

    int chaser = 0, next = 0, nextKey = 0, nextInput = 0, previous = 0, previousKey = 0,
        previousInput = 0, playback = 0, playbackKey = 0, playbackInput = 0, stop = 0, stopKey = 0, stopInput = 0,
        wstate = 0, appearance = 0, nextPrevBehavior = 0, slidersMode = 0;

    QVERIFY(cl.saveXML(&xmlWriter) == true);

    xmlWriter.setDevice(NULL);
    buffer.close();

    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader xmlReader(&buffer);
    xmlReader.readNextStartElement();

    QCOMPARE(xmlReader.name().toString(), QString("CueList"));
    QCOMPARE(xmlReader.attributes().value("Caption").toString(), QString("Testing"));

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name().toString() == "Chaser")
        {
            QCOMPARE(xmlReader.readElementText().toUInt(), c->id());
            chaser++;
        }
        else if (xmlReader.name().toString() == "Function")
        {
            QFAIL("Function node should not be written anymore!");
        }
        else if (xmlReader.name().toString() == "Next")
        {
            next++;
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Key")
            {
                nextKey++;
                QCOMPARE(xmlReader.readElementText(), QKeySequence(keySequenceB).toString());
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }

            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Input")
            {
                nextInput++;
                // Handled by VCWidget tests, just check that the node is there
                xmlReader.skipCurrentElement();
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Previous")
        {
            previous++;
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Key")
            {
                previousKey++;
                QCOMPARE(xmlReader.readElementText(), QKeySequence(keySequenceA).toString());
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }

            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Input")
            {
                previousInput++;
                // Handled by VCWidget tests, just check that the node is there
                xmlReader.skipCurrentElement();
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Playback")
        {
            playback++;
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Key")
            {
                playbackKey++;
                QCOMPARE(xmlReader.readElementText(), QKeySequence(keySequenceC).toString());
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Input")
            {
                playbackInput++;
                // Handled by VCWidget tests, just check that the node is there
                xmlReader.skipCurrentElement();
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Stop")
        {
            stop++;
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Key")
            {
                stopKey++;
                QCOMPARE(xmlReader.readElementText(), QKeySequence(keySequenceD).toString());
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.readNextStartElement();
            if (xmlReader.name().toString() == "Input")
            {
                stopInput++;
                // Handled by VCWidget tests, just check that the node is there
                xmlReader.skipCurrentElement();
            }
            else
            {
                QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            }
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "WindowState")
        {
            // Handled by VCWidget tests, just check that the node is there
            wstate++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name().toString() == "Appearance")
        {
            // Handled by VCWidget tests, just check that the node is there
            appearance++;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name() == KXMLQLCVCCueListNextPrevBehavior)
        {
            ++nextPrevBehavior;
            xmlReader.skipCurrentElement();
        }
        else if (xmlReader.name() == KXMLQLCVCCueListSlidersMode)
        {
            ++slidersMode;
            xmlReader.skipCurrentElement();
        }
        else
        {
            QFAIL(QString("Unexpected tag: %1").arg(xmlReader.name().toString()).toUtf8().constData());
            xmlReader.skipCurrentElement();
        }
    }

    QCOMPARE(chaser, 1);
    QCOMPARE(next, 1);
    QCOMPARE(nextKey, 1);
    QCOMPARE(nextInput, 1);
    QCOMPARE(previous, 1);
    QCOMPARE(previousKey, 1);
    QCOMPARE(previousInput, 1);
    QCOMPARE(playback, 1);
    QCOMPARE(playbackKey, 1);
    QCOMPARE(playbackInput, 1);
    QCOMPARE(stop, 1);
    QCOMPARE(stopKey, 1);
    QCOMPARE(stopInput, 1);
    QCOMPARE(wstate, 1);
    QCOMPARE(appearance, 1);
    QCOMPARE(nextPrevBehavior, 1);
    QCOMPARE(slidersMode, 1);
}

/* GO on the beat (docs/design/PLAYBACK_TIMING_DESIGN.md, P1): a press that hasn't landed
   yet shows on the Next button until it does. */
void VCCueList_Test::aGoWaitingForItsBeatShowsOnNext()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser *c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    c->setGoQuantize(Chaser::GoOnBeat);
    cl.setChaser(c->id());
    m_doc->setMode(Doc::Operate);
    MasterTimer *timer = m_doc->masterTimer();
    timer->setBpm(30);                                  // two seconds a beat

    cl.slotNextCue();                                   // start: lands on a beat too
    timer->alignBeat(1.0);
    QTest::qWait(50);
    for (int i = 0; i < 3; ++i)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QVERIFY(c->goHeld());
    cl.slotProgressTimeout();
    QVERIFY(cl.m_nextButton->property("goHeld").toBool());
    QVERIFY(!cl.m_nextButton->styleSheet().isEmpty());

    // Once it lands, the button is itself again.
    QElapsedTimer t;
    t.start();
    while (c->goHeld() && t.elapsed() < 3000)
    {
        timer->timerTick();
        QTest::qWait(20);
    }
    QVERIFY(!c->goHeld());
    cl.slotProgressTimeout();
    QVERIFY(!cl.m_nextButton->property("goHeld").toBool());
    QVERIFY(cl.m_nextButton->styleSheet().isEmpty());
    m_doc->setMode(Doc::Design);
}

void VCCueList_Test::nextPrevious()
{
    QWidget w;

    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    cl.setChaser(c->id());
    QCOMPARE(c->steps().size(), 4);
    Scene* s1 = qobject_cast<Scene*> (m_doc->function(c->steps()[0].fid));
    Scene* s2 = qobject_cast<Scene*> (m_doc->function(c->steps()[1].fid));
    Scene* s3 = qobject_cast<Scene*> (m_doc->function(c->steps()[2].fid));
    Scene* s4 = qobject_cast<Scene*> (m_doc->function(c->steps()[3].fid));
    Q_ASSERT(s1 && s2 && s3 && s4);

    // Not in operate mode, check for crashes
    cl.slotNextCue();
    cl.slotPreviousCue();
    cl.slotItemActivated(cl.m_tree->topLevelItem(2));
    // QVERIFY(cl.m_runner == NULL);

    // Switch mode
    m_doc->setMode(Doc::Operate);
    MasterTimer* timer = m_doc->masterTimer();

    // Create runner with a next action -> first item should be activated
    cl.slotNextCue();
    // QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);

    cl.slotNextCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);

    cl.slotNextCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);

    cl.slotPreviousCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s2);

    cl.slotPreviousCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);

    // Wrap around to the last cue
    cl.slotPreviousCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s4);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s4);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s4);

    // Wrap around to the next cue
    cl.slotNextCue();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
}

void VCCueList_Test::manualActivation()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    cl.setChaser(c->id());
    Scene* s1 = qobject_cast<Scene*> (m_doc->function(c->steps()[0].fid));
    //Scene* s2 = qobject_cast<Scene*> (m_doc->function(c->steps()[1].fid));
    Scene* s3 = qobject_cast<Scene*> (m_doc->function(c->steps()[2].fid));
    //Scene* s4 = qobject_cast<Scene*> (m_doc->function(c->steps()[3].fid));
    Q_ASSERT(s1 /*&& s2*/ && s3/* && s4*/);

    // Switch mode
    m_doc->setMode(Doc::Operate);
    MasterTimer* timer = m_doc->masterTimer();

    // QVERIFY(cl.m_runner == NULL);
    cl.slotItemActivated(cl.m_tree->topLevelItem(2));
    // QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);

    // Same item
    cl.slotItemActivated(cl.m_tree->topLevelItem(2));
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s3);

    // Another item
    cl.slotItemActivated(cl.m_tree->topLevelItem(0));
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);

    // Crash check
    cl.slotItemActivated(NULL);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
    timer->timerTick();
    QCOMPARE(timer->runningFunctions(), 2);
    QCOMPARE(timer->m_functionList[1], s1);
}

void VCCueList_Test::keyboardNextPrevious()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    cl.setChaser(c->id());

    cl.setNextKeySequence(QKeySequence(keySequenceB));
    cl.setPreviousKeySequence(QKeySequence(keySequenceA));
    cl.setPlaybackKeySequence(QKeySequence(keySequenceD));

    // Switch mode
    m_doc->setMode(Doc::Operate);
    MasterTimer* timer = m_doc->masterTimer();

    // Next keyboard key
    cl.slotKeyPressed(QKeySequence(keySequenceB));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 0);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 0);

    // Next keyboard key
    cl.slotKeyPressed(QKeySequence(keySequenceB));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 1);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 1);

    // Unrecognized keyboard key
    cl.slotKeyPressed(QKeySequence(QKeySequence::SelectAll));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 1);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 1);

    // Previous keyboard key
    cl.slotKeyPressed(QKeySequence(keySequenceA));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 0);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 0);

    // Previous keyboard key
    cl.slotKeyPressed(QKeySequence(keySequenceA));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 3);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 3);

    // Next keyboard key
    cl.slotKeyPressed(QKeySequence(keySequenceB));
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 0);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 0);

    // Playback
    cl.slotKeyPressed(QKeySequence(keySequenceD));
    timer->timerTick();
    //QVERIFY(cl.m_runner == NULL);
    QCOMPARE(cl.m_tree->indexOfTopLevelItem(cl.m_tree->currentItem()), 0);
}

void VCCueList_Test::submasterWorksAfterSliderModeRoundTrip()
{
    VirtualConsole *vc = VirtualConsole::instance();
    QVERIFY(vc != NULL);

    VCFrame *contents = vc->contents();
    QVERIFY(contents != NULL);

    VCSoloFrame *frame = new VCSoloFrame(contents, m_doc, true);
    vc->setupWidget(frame, contents);

    VCCueList *cl = new VCCueList(frame, m_doc);
    vc->setupWidget(cl, frame);
    cl->setSideFaderMode(VCCueList::Crossfade);

    VCSlider *slider = new VCSlider(frame, m_doc);
    vc->setupWidget(slider, frame);
    slider->setSliderMode(VCSlider::Submaster);
    slider->setLevelValue(UCHAR_MAX);
    slider->setSliderValue(UCHAR_MAX);

    Fixture *fxi = new Fixture(m_doc);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    fxi->setChannels(1);
    m_doc->addFixture(fxi);

    Scene *scene = new Scene(m_doc);
    scene->setValue(fxi->id(), 0, UCHAR_MAX);
    m_doc->addFunction(scene);

    Chaser *chaser = new Chaser(m_doc);
    chaser->setDuration(Function::infiniteSpeed());
    chaser->addStep(ChaserStep(scene->id()));
    m_doc->addFunction(chaser);

    cl->setChaser(chaser->id());

    MasterTimer *timer = m_doc->masterTimer();
    QList<Universe *> universes;
    QSignalSpy submasterSpy(slider, SIGNAL(submasterValueChanged(qreal)));

    m_doc->setMode(Doc::Operate);
    cl->slotPlayback();
    timer->timerTick();

    slider->m_slider->setSliderDown(true);
    slider->m_slider->setValue(127);
    slider->m_slider->setSliderDown(false);
    QVERIFY(submasterSpy.count() > 0);
    submasterSpy.clear();

    universes = m_doc->inputOutputMap()->claimUniverses();
    universes[0]->processFaders();
    QCOMPARE(universes[0]->postGMValue(0), uchar(127));
    m_doc->inputOutputMap()->releaseUniverses(false);

    QVERIFY(cl->m_intensityOverrideId != Function::invalidAttributeId());
    chaser->stop(FunctionParent::master());
    timer->timerTick();
    QVERIFY(chaser->m_overrideMap.isEmpty());
    QCOMPARE(cl->m_intensityOverrideId, Function::invalidAttributeId());
    m_doc->setMode(Doc::Design);

    {
        VCSliderProperties prop(slider, m_doc);
        prop.slotModePlaybackClicked();
        prop.accept();
    }
    QCOMPARE(slider->sliderMode(), VCSlider::Playback);

    {
        VCSliderProperties prop(slider, m_doc);
        prop.slotModeSubmasterClicked();
        prop.accept();
    }
    QCOMPARE(slider->sliderMode(), VCSlider::Submaster);

    m_doc->setMode(Doc::Operate);
    cl->slotPlayback();
    timer->timerTick();

    slider->m_slider->setSliderDown(true);
    slider->m_slider->setValue(127);
    slider->m_slider->setSliderDown(false);
    QVERIFY(submasterSpy.count() > 0);

    universes = m_doc->inputOutputMap()->claimUniverses();
    universes[0]->processFaders();
    QCOMPARE(universes[0]->postGMValue(0), uchar(127));
    m_doc->inputOutputMap()->releaseUniverses(false);
}

void VCCueList_Test::input()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser* c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    cl.setChaser(c->id());

    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(0, 1)), VCCueList::nextInputSourceId);
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(2, 3)), VCCueList::previousInputSourceId);
    cl.setInputSource(QSharedPointer<QLCInputSource>(new QLCInputSource(4, 5)), VCCueList::playbackInputSourceId);

    // Switch mode
    m_doc->setMode(Doc::Operate);
    MasterTimer* timer = m_doc->masterTimer();

    // Runner creation thru "next" input
    cl.slotInputValueChanged(5, 3, 255);
    //QVERIFY(cl.m_runner == NULL);

    cl.slotInputValueChanged(2, 15, 255);
    //QVERIFY(cl.m_runner == NULL);

    cl.slotInputValueChanged(0, 1, 255);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 0);

    cl.slotInputValueChanged(0, 1, 0);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 0);

    cl.slotInputValueChanged(0, 1, 255);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 1);

    // Runner creation thru previous input
    m_doc->setMode(Doc::Design);
    //QVERIFY(cl.m_runner == NULL);
    m_doc->setMode(Doc::Operate);

    cl.slotInputValueChanged(0, 3, 255);
    //QVERIFY(cl.m_runner == NULL);

    cl.slotInputValueChanged(2, 1, 255);
    //QVERIFY(cl.m_runner == NULL);

    cl.slotInputValueChanged(2, 3, 255);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 3);

    cl.slotInputValueChanged(2, 3, 0);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 3);

    cl.slotInputValueChanged(2, 3, 255);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QCOMPARE(cl.m_runner->currentStepIndex(), 2);

    cl.slotInputValueChanged(4, 5, 255);
    //QVERIFY(cl.m_runner != NULL);
    timer->timerTick();
    //QVERIFY(cl.m_runner == NULL);

    cl.slotInputValueChanged(4, 5, 0);
    //QVERIFY(cl.m_runner == NULL);
    timer->timerTick();
    //QVERIFY(cl.m_runner == NULL);
}

/* Show control (docs/design/SHOW_CONTROL_DESIGN.md): the cue list shows each step's
   cue, a skipped step greyed and struck through, and what GO plays next. */
void VCCueList_Test::cuesSkipsAndNextAreShown()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser *c = createChaser(m_doc);
    c->setStepSkipped(0, true);
    ChaserStep step = c->steps().at(1);
    step.cueNumber = "12.5";
    step.note = "on the downbeat";
    c->replaceStep(step, 1);
    cl.setChaser(c->id());
    cl.updateStepList();

    const int colCue = 6;
    QCOMPARE(cl.m_tree->headerItem()->text(colCue), QString("Cue"));
    QCOMPARE(cl.m_tree->header()->visualIndex(colCue), 1);
    QCOMPARE(cl.m_tree->topLevelItem(1)->text(colCue), QString("12.5"));
    QCOMPARE(cl.m_tree->topLevelItem(2)->text(colCue), QString("3"));
    QVERIFY(cl.m_tree->topLevelItem(0)->font(1).strikeOut());
    QVERIFY(!cl.m_tree->topLevelItem(1)->font(1).strikeOut());

    // Stopped, nothing picked: the first step not skipped -- and Play starts there.
    QCOMPARE(cl.standbyIndex(), 1);
    QCOMPARE(cl.getCurrentIndex(), 1);
    QCOMPARE(cl.m_standbyLabel->text(),
             QString("Next: 12.5 \u2014 Second \u2014 on the downbeat"));

    // A step picked: that one.
    cl.m_tree->setCurrentItem(cl.m_tree->topLevelItem(3));
    cl.updateStandby();
    QVERIFY(cl.m_standbyLabel->text().startsWith("Next: 4"));
}

/* A Show Tree list in a VC Cue List: each cue in its text colour (its own,
   else its list's; a sub-cue its cue's), readable on the list; skipped
   stays grey; Next: in the next cue's. A colour taken away: as before. */
void VCCueList_Test::showTreeTextColoursAreShown()
{
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::Tracking);
    QList<quint32> c;
    for (int i = 0; i < 4; ++i)
        c << st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    const quint32 sub = st->addSubCue(c[1], "Hit");
    QVERIFY(st->setListColour(list, "Cyan"));
    QVERIFY(st->setCuesColour({ c[0] }, "Pink"));
    QVERIFY(st->setCuesSkipped({ c[3] }, true));

    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(st->cueList(list)->chaser);
    cl.updateStepList();
    const QPalette tp = cl.m_tree->palette();
    auto onTree = [&tp](const char *hex) {
        return ShowTree::readableOn(ShowTree::readableOn(QColor(hex), tp.color(QPalette::Base)),
                                    tp.color(QPalette::AlternateBase));
    };
    const int colName = 1;
    QCOMPARE(cl.m_tree->topLevelItem(0)->foreground(colName).color(), onTree("#d81b60"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->foreground(colName).color(), onTree("#00acc1"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->child(0)->foreground(colName).color(), onTree("#00acc1"));
    QCOMPARE(cl.m_tree->topLevelItem(3)->foreground(colName).color(),
             tp.color(QPalette::Disabled, QPalette::Text));          // skipped: grey
    QVERIFY(ShowTree::contrastRatio(cl.m_tree->topLevelItem(1)->foreground(colName).color(),
                                    tp.color(QPalette::Base)) >= 4.5);
    // Next: the first cue, in pink.
    QCOMPARE(cl.m_standbyLabel->palette().color(QPalette::WindowText),
             ShowTree::readableOn(QColor("#d81b60"), cl.palette().color(QPalette::Window)));

    // Changed in the tree: shown at once (no rebuild needed).
    QVERIFY(st->setCuesColour({ sub }, "Yellow"));
    QCOMPARE(cl.m_tree->topLevelItem(1)->child(0)->foreground(colName).color(), onTree("#fdd835"));
    QVERIFY(st->setCuesColour({ c[0] }, "Orange"));
    QCOMPARE(cl.m_tree->topLevelItem(0)->foreground(colName).color(), onTree("#fb8c00"));
    QCOMPARE(cl.m_standbyLabel->palette().color(QPalette::WindowText),
             ShowTree::readableOn(QColor("#fb8c00"), cl.palette().color(QPalette::Window)));

    // Taken away: the list's plain text again.
    QVERIFY(st->setListColour(list, QString()));
    QVERIFY(st->setCuesColour({ c[0], sub }, QString()));
    QTRY_VERIFY(!cl.m_tree->topLevelItem(1)->data(colName, Qt::ForegroundRole).isValid());
    QVERIFY(!cl.m_tree->topLevelItem(0)->data(colName, Qt::ForegroundRole).isValid());
    QCOMPARE(cl.m_standbyLabel->palette().color(QPalette::WindowText), cl.palette().color(QPalette::WindowText));
}

void VCCueList_Test::showTreeListGoesThroughTheOneGoPath()
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::NonTracking);
    for (int i = 0; i < 4; ++i)
        st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    Chaser *tree = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(tree != NULL);
    QVERIFY(sc->isTreeGoList(tree->id()));

    // The run handler stands for the App's: it notes the route and carries on.
    QList<QPair<ShowControl::Command, ShowControl::Route>> seen;
    QStringList cues;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route route) {
        seen << qMakePair(r.command, route);
        cues << r.cue;
        return sc->execute(r);
    });
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        m_doc->masterTimer()->stopAllFunctions();
    });

    m_doc->setMode(Doc::Operate);
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(tree->id());
    MasterTimer *timer = m_doc->masterTimer();

    // Next on a stopped list: a GO.
    cl.slotNextCue();
    QCOMPARE(seen.size(), 1);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Go, ShowControl::RouteVcCueList));
    timer->timerTick();
    timer->timerTick();
    QVERIFY(tree->isRunning());

    // Next on a running list: a GO; Previous: a Back.
    cl.slotNextCue();
    QCOMPARE(seen.size(), 2);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Go, ShowControl::RouteVcCueList));
    cl.slotPreviousCue();
    QCOMPARE(seen.size(), 3);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Back, ShowControl::RouteVcCueList));

    // A double-click on a row: that cue, by its number.
    cl.slotItemActivated(cl.m_tree->topLevelItem(2));
    QCOMPARE(seen.size(), 4);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Go, ShowControl::RouteVcCueList));
    QCOMPARE(cues.last(), tree->stepCue(2));

    // Stop: the Stop command.
    cl.slotStop();
    QCOMPARE(seen.size(), 5);
    QCOMPARE(seen.last(), qMakePair(ShowControl::Stop, ShowControl::RouteVcCueList));
    timer->timerTick();
    timer->timerTick();
    QVERIFY(!tree->isRunning());
}

// Changelog: one GO path -- the VC Cue List's Sub GO / Sub Back buttons on a
// Show Tree list enter ShowControl like every other GO, never step the chaser.
void VCCueList_Test::showTreeSubGoAndBackGoThroughShowControl()
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    const quint32 list = st->addCueList(st->addProduction("P2"), "L2", ShowTree::ListMode::NonTracking);
    const quint32 c1 = st->addCue(list, -1, "C1");
    st->addCue(list, 0, "C2");
    QVERIFY(st->addSubCue(c1, "S1") != ShowTree::invalidId());
    QVERIFY(st->addSubCue(c1, "S2") != ShowTree::invalidId());
    Chaser *tree = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(tree != NULL);

    QList<QPair<ShowControl::Command, ShowControl::Route>> seen;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route route) {
        seen << qMakePair(r.command, route);
        return sc->execute(r);
    });
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        m_doc->masterTimer()->stopAllFunctions();
    });

    m_doc->setMode(Doc::Operate);
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(tree->id());
    MasterTimer *timer = m_doc->masterTimer();

    // Not running: Sub GO does nothing, and asks no one.
    cl.slotSubGo();
    QCOMPARE(seen.size(), 0);

    cl.slotNextCue();
    timer->timerTick();
    timer->timerTick();
    QVERIFY(tree->isRunning());
    seen.clear();

    cl.slotSubGo();
    QCOMPARE(seen.size(), 1);
    QCOMPARE(seen.last(), qMakePair(ShowControl::SubGo, ShowControl::RouteVcCueList));
    cl.slotSubBack();
    QCOMPARE(seen.size(), 2);
    QCOMPARE(seen.last(), qMakePair(ShowControl::SubBack, ShowControl::RouteVcCueList));
    QVERIFY(tree->isRunning());
    cl.slotStop();
    for (int i = 0; i < 4; ++i)
        timer->timerTick();
    QVERIFY(!tree->isRunning());
}

void VCCueList_Test::plainChaserStillStepsDirectly()
{
    ShowControl *sc = m_doc->showControl();
    int routed = 0;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route) {
        ++routed;
        return sc->execute(r);
    });
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        m_doc->masterTimer()->stopAllFunctions();
    });

    Chaser *c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    QVERIFY(!sc->isTreeGoList(c->id()));
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(c->id());
    m_doc->setMode(Doc::Operate);
    MasterTimer *timer = m_doc->masterTimer();
    Scene *s1 = qobject_cast<Scene *>(m_doc->function(c->steps()[0].fid));
    Scene *s2 = qobject_cast<Scene *>(m_doc->function(c->steps()[1].fid));

    cl.slotNextCue();
    timer->timerTick();
    timer->timerTick();
    QVERIFY(c->isRunning());
    QCOMPARE(timer->m_functionList[1], s1);
    cl.slotNextCue();
    timer->timerTick();
    QCOMPARE(timer->m_functionList[1], s2);
    cl.slotPreviousCue();
    timer->timerTick();
    QCOMPARE(timer->m_functionList[1], s1);
    cl.slotItemActivated(cl.m_tree->topLevelItem(2));
    cl.slotStop();
    timer->timerTick();
    timer->timerTick();
    QVERIFY(!c->isRunning());
    QCOMPARE(routed, 0);               // none of it went through run()
}

void VCCueList_Test::aNextOnAListRunningFromAPreviewIsTheShow()
{
    QWidget w;
    VCCueList cl(&w, m_doc);
    Chaser *c = createChaser(m_doc);
    c->setDuration(Function::infiniteSpeed());
    cl.setChaser(c->id());
    Fixture *fx = m_doc->fixtures().first();
    fx->setOutputRules(Fixture::ShowOnly);
    m_doc->outputRuleKeeper()->rebuild();
    m_doc->setMode(Doc::Operate);
    MasterTimer *timer = m_doc->masterTimer();
    c->start(timer, FunctionParent::preview(0x7));     // the Workbench plays it
    for (int i = 0; i < 3; i++)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QVERIFY(c->previewGate()->load());                // not the show
    cl.slotNextCue();
    for (int i = 0; i < 3; i++)
        timer->timerTick();
    QVERIFY(c->isRunning());
    QVERIFY(c->previewGate()->load() == false);       // the show now
    Function *step = m_doc->function(c->steps().at(1).fid);
    QVERIFY(step->isRunning());
    QVERIFY(step->previewGate()->load() == false);    // and what it plays
}

/****************************************************************************
 * Song sections (Q190)
 ****************************************************************************/

namespace
{
/* Three songs merged into one list (Merge lists into one…): sections
   "0 — Preshow", "1 — Scream", "2 — Thriller", their cues under each. */
quint32 threeSongs(Doc *doc, int cuesEach = 2)
{
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Dance Hysteria");
    const char *names[] = { "00-Preshow", "01-Scream", "02-Thriller" };
    QList<quint32> lists;
    for (int l = 0; l < 3; ++l)
    {
        const quint32 list = st->addCueList(prod, names[l], ShowTree::ListMode::NonTracking);
        for (int i = 0; i < cuesEach; ++i)
            st->addCue(list, i - 1, QString("%1 cue %2").arg(QString(names[l]).mid(3)).arg(i + 1));
        lists << list;
    }
    if (!st->mergeLists(prod, lists))
        return ShowTree::invalidId();
    st->setCurrentProduction(prod);
    return lists.first();
}

bool isSectionRow(const QTreeWidgetItem *item)
{
    return item != NULL && item->data(0, Qt::UserRole + 51).toBool();
}
} // namespace

void VCCueList_Test::songSectionsShowAsRowsInOrder()
{
    const quint32 list = threeSongs(m_doc);
    QVERIFY(list != ShowTree::invalidId());
    ShowTreeController *st = m_doc->showTree();
    QCOMPARE(st->sections(list).count(), 3);
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(ch != NULL);
    QCOMPARE(ch->stepsCount(), 6);

    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(ch->id());
    QTreeWidget *tree = cl.m_tree;

    // A header row above each song's first cue, in list order.
    QCOMPARE(tree->topLevelItemCount(), 9);
    QStringList rows;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        rows << (isSectionRow(tree->topLevelItem(i)) ? QStringLiteral("# ") + tree->topLevelItem(i)->text(0)
                                                      : QString::number(cl.topLevelIndexOf(tree->topLevelItem(i))));
    QCOMPARE(rows, QStringList({ "# " + ShowTree::sectionTitle(st->sections(list).at(0)), "0", "1",
                                 "# " + ShowTree::sectionTitle(st->sections(list).at(1)), "2", "3",
                                 "# " + ShowTree::sectionTitle(st->sections(list).at(2)), "4", "5" }));
    QVERIFY(tree->topLevelItem(4)->text(1).endsWith(m_doc->function(ch->steps().at(2).fid)->name()));

    // The Run tab's look: across the row, bold, a heading (never selectable).
    QTreeWidgetItem *header = tree->topLevelItem(3);
    QVERIFY(isSectionRow(header));
    QVERIFY(header->isFirstColumnSpanned());
    QVERIFY(header->font(0).bold());
    QVERIFY(!(header->flags() & Qt::ItemIsSelectable));
    QVERIFY(header->flags() & Qt::ItemIsEnabled);
    QVERIFY(!(header->flags() & Qt::ItemIsEditable));

    // The cue numbers count cues, not rows.
    QCOMPARE(cl.stepCount(), 6);
    QCOMPARE(cl.stepItem(2), tree->topLevelItem(4));
    QCOMPARE(cl.stepItem(2)->text(0), QString("3"));
    QCOMPARE(cl.topLevelIndexOf(header), -1);

    // A section renamed in the tree: the row follows.
    QVERIFY(st->setSectionName(list, st->sections(list).at(1), "Everybody Scream"));
    QTRY_COMPARE(tree->topLevelItem(3)->text(0), QString("1 — Everybody Scream"));
    QCOMPARE(cl.stepCount(), 6);
}

void VCCueList_Test::goStepsPastASectionRow()
{
    const quint32 list = threeSongs(m_doc);
    QVERIFY(list != ShowTree::invalidId());
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    QVERIFY(ch != NULL);
    QList<ShowControl::Request> seen;
    sc->setRunHandler([&](const ShowControl::Request &r, ShowControl::Route) {
        seen << r;
        return sc->execute(r);
    });
    auto cleanup = qScopeGuard([&]() {
        sc->setRunHandler(ShowControl::RunHandler());
        m_doc->masterTimer()->stopAllFunctions();
    });

    m_doc->setMode(Doc::Operate);
    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(ch->id());
    QTreeWidget *tree = cl.m_tree;
    QLabel *section = cl.findChild<QLabel *>("cueListSection");
    QVERIFY(section != NULL);
    QVERIFY(!section->isHidden());

    // Stopped, Next selects: from the last cue of a song to the next song's
    // first, over its header.
    cl.setNextPrevBehavior(VCCueList::Select);
    tree->setCurrentItem(cl.stepItem(1));
    cl.slotNextCue();
    QCOMPARE(tree->currentItem(), cl.stepItem(2));
    QCOMPARE(section->text(), ShowTree::sectionTitle(st->sections(list).at(1)));
    cl.slotPreviousCue();
    QCOMPARE(tree->currentItem(), cl.stepItem(1));
    QCOMPARE(section->text(), ShowTree::sectionTitle(st->sections(list).at(0)));

    // The arrow keys (or a click) landing on a header go on to a cue: down
    // into the song's first, up out of it to the one before.
    tree->setCurrentItem(tree->topLevelItem(6));
    QCOMPARE(tree->currentItem(), cl.stepItem(4));
    tree->setCurrentItem(tree->topLevelItem(6));
    QCOMPARE(tree->currentItem(), cl.stepItem(3));

    // A double-click on a header plays nothing.
    cl.slotItemActivated(tree->topLevelItem(3));
    QVERIFY(seen.isEmpty());

    // GO from cue 2 (the end of the first song): the next song's first cue,
    // by its number -- the header is no step.
    MasterTimer *timer = m_doc->masterTimer();
    cl.slotItemActivated(cl.stepItem(1));
    QCOMPARE(seen.size(), 1);
    QCOMPARE(seen.last().cue, ch->stepCue(1));
    timer->timerTick();
    timer->timerTick();
    QVERIFY(ch->isRunning());
    QTRY_COMPARE(tree->currentItem(), cl.stepItem(1));
    cl.slotNextCue();
    QCOMPARE(seen.size(), 2);
    QCOMPARE(seen.last().command, ShowControl::Go);
    timer->timerTick();
    timer->timerTick();
    QTRY_COMPARE(ch->currentStepIndex(), 2);
    QTRY_COMPARE(tree->currentItem(), cl.stepItem(2));
    QVERIFY(!isSectionRow(tree->currentItem()));
    // The cue playing's song, in sight above the list.
    QCOMPARE(section->text(), ShowTree::sectionTitle(st->sections(list).at(1)));

    cl.slotStop();
    timer->timerTick();
    timer->timerTick();
    QVERIFY(!ch->isRunning());
}

void VCCueList_Test::aListWithNoSectionsLooksAsBefore()
{
    // A Show Tree list with no scheme, and a plain chaser: rows are steps.
    ShowTreeController *st = m_doc->showTree();
    const quint32 list = st->addCueList(st->addProduction("P"), "L", ShowTree::ListMode::NonTracking);
    for (int i = 0; i < 3; ++i)
        st->addCue(list, i - 1, QString("C%1").arg(i + 1));
    QVERIFY(st->sections(list).isEmpty());

    QWidget w;
    VCCueList cl(&w, m_doc);
    cl.setChaser(st->cueList(list)->chaser);
    QCOMPARE(cl.m_tree->topLevelItemCount(), 3);
    for (int i = 0; i < 3; ++i)
    {
        QCOMPARE(cl.stepItem(i), cl.m_tree->topLevelItem(i));
        QVERIFY(!isSectionRow(cl.m_tree->topLevelItem(i)));
        QCOMPARE(cl.m_tree->topLevelItem(i)->text(0), QString::number(i + 1));
    }
    QLabel *section = cl.findChild<QLabel *>("cueListSection");
    QVERIFY(section != NULL);
    QVERIFY(section->isHidden());

    Scene *s1 = new Scene(m_doc);
    m_doc->addFunction(s1);
    Chaser *plain = new Chaser(m_doc);
    plain->addStep(ChaserStep(s1->id()));
    plain->addStep(ChaserStep(s1->id()));
    m_doc->addFunction(plain);
    cl.setChaser(plain->id());
    QCOMPARE(cl.m_tree->topLevelItemCount(), 2);
    QCOMPARE(cl.stepCount(), 2);
    QVERIFY(section->isHidden());

    // And a saved layout reads back as it was written (no new element).
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly | QIODevice::Text);
    QXmlStreamWriter xmlWriter(&buffer);
    QVERIFY(cl.saveXML(&xmlWriter));
    QVERIFY(!buffer.data().contains("Section"));
}

void VCCueList_Test::songSectionShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));

    const quint32 list = threeSongs(m_doc, 9);
    QVERIFY(list != ShowTree::invalidId());
    ShowTreeController *st = m_doc->showTree();
    QVERIFY(st->setSectionName(list, st->sections(list).at(1), "Everybody Scream"));
    const QPalette light = QApplication::palette();
    for (int theme = 0; theme < 2; ++theme)
    {
        if (theme == 1)
            shotUseDarkTheme();
        QWidget host;
        VCCueList cl(&host, m_doc);
        cl.setCaption("Main");
        cl.setChaser(st->cueList(list)->chaser);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        // The current cue far down the second song: its header scrolled off.
        cl.m_tree->setCurrentItem(cl.stepItem(14));
        for (const QSize &z : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            host.resize(z);
            cl.setGeometry(QRect(QPoint(0, 0), z));
            for (int i = 0; i < 5; ++i)
                QApplication::processEvents();
            cl.m_tree->scrollToItem(cl.stepItem(14), QAbstractItemView::PositionAtBottom);
            QTest::qWait(30);
            const QString file = QStringLiteral("%1-vccuelist-sections-%2-%3x%4.png")
                                     .arg(prefix, theme == 0 ? "light" : "dark")
                                     .arg(z.width()).arg(z.height());
            QVERIFY(host.grab().save(QDir(dir).filePath(file)));
        }
    }
    QApplication::setPalette(light);
}

QTEST_MAIN(VCCueList_Test)
