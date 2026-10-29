/*
  Q Light Controller - Unit test
  inputpatch_test.cpp

  Copyright (c) Heikki Junnila

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

#define private public
#include "iopluginstub.h"
#include "inputpatch_test.h"
#include "qlcioplugin.h"
#include "inputpatch.h"
#include "qlcfile.h"
#include "doc.h"
#undef private

#define TESTPLUGINDIR "../iopluginstub"

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

void InputPatch_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void InputPatch_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void InputPatch_Test::defaults()
{
    InputPatch ip(0, this);
    QVERIFY(ip.m_plugin == NULL);
    QVERIFY(ip.m_pluginLine == QLCIOPlugin::invalidLine());
    QVERIFY(ip.m_profile == NULL);
    QVERIFY(ip.m_pageSetCh == USHRT_MAX);
    QVERIFY(ip.pluginName() == KInputNone);
    QVERIFY(ip.inputName() == KInputNone);
    QVERIFY(ip.profileName() == KInputNone);
    QVERIFY(ip.isPatched() == false);
    QVERIFY(ip.input() == QLCIOPlugin::invalidLine());

    InputPatch ip2(this);
    QVERIFY(ip2.m_plugin == NULL);
    QVERIFY(ip2.m_pluginLine == QLCIOPlugin::invalidLine());
    QVERIFY(ip2.m_profile == NULL);
    QVERIFY(ip2.m_pageSetCh == USHRT_MAX);
    QVERIFY(ip2.pluginName() == KInputNone);
    QVERIFY(ip2.inputName() == KInputNone);
    QVERIFY(ip2.profileName() == KInputNone);
    QVERIFY(ip2.isPatched() == false);
    QVERIFY(ip2.input() == QLCIOPlugin::invalidLine());
}

void InputPatch_Test::patch()
{
    QCOMPARE(m_doc->ioPluginCache()->plugins().size(), 1);
    IOPluginStub* stub = static_cast<IOPluginStub*> (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QLCInputProfile prof1;
    prof1.setManufacturer("Foo");
    prof1.setManufacturer("Bar");

    InputPatch* ip = new InputPatch(0, this);
    QVERIFY(ip->set(stub, 0, &prof1) == true);
    QVERIFY(ip->m_plugin == stub);
    QVERIFY(ip->m_pluginLine == 0);
    QVERIFY(ip->m_profile == &prof1);
    QVERIFY(ip->pluginName() == stub->name());
    QVERIFY(ip->inputName() == stub->inputs()[0]);
    QVERIFY(ip->profileName() == prof1.name());
    QVERIFY(ip->isPatched() == true);
    QVERIFY(stub->m_openInputs.size() == 1);
    QVERIFY(stub->m_openInputs.at(0) == 0);

    QLCInputProfile prof2;
    prof2.setManufacturer("Xyzzy");
    prof2.setManufacturer("Foobar");

    QVERIFY(ip->set(stub, 3, &prof2) == true);
    QVERIFY(ip->m_plugin == stub);
    QVERIFY(ip->m_pluginLine == 3);
    QVERIFY(ip->m_profile == &prof2);
    QVERIFY(ip->pluginName() == stub->name());
    QVERIFY(ip->inputName() == stub->inputs()[3]);
    QVERIFY(ip->profileName() == prof2.name());
    QVERIFY(stub->m_openInputs.size() == 1);
    QVERIFY(stub->m_openInputs.at(0) == 3);

    ip->reconnect();
    QVERIFY(ip->m_plugin == stub);
    QVERIFY(ip->m_pluginLine == 3);
    QVERIFY(ip->m_profile == &prof2);
    QVERIFY(ip->pluginName() == stub->name());
    QVERIFY(ip->inputName() == stub->inputs()[3]);
    QVERIFY(ip->profileName() == prof2.name());
    QVERIFY(stub->m_openInputs.size() == 1);
    QVERIFY(stub->m_openInputs.at(0) == 3);

    QVERIFY(ip->set(&prof1) == true);
    QVERIFY(ip->m_plugin == stub);
    QVERIFY(ip->m_pluginLine == 3);
    QVERIFY(ip->m_profile == &prof1);
    QVERIFY(ip->pluginName() == stub->name());
    QVERIFY(ip->inputName() == stub->inputs()[3]);
    QVERIFY(ip->profileName() == prof1.name());
    QVERIFY(stub->m_openInputs.size() == 1);
    QVERIFY(stub->m_openInputs.at(0) == 3);

    delete ip;
    QVERIFY(stub->m_openInputs.size() == 0);

    InputPatch* ip2 = new InputPatch(0, this);
    QVERIFY(ip2->set(&prof1) == false);
}

void InputPatch_Test::parameters()
{
    InputPatch* ip = new InputPatch(0, this);
    IOPluginStub* stub = static_cast<IOPluginStub*> (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    QVERIFY(ip->set(stub, 0, NULL) == true);
    QVERIFY(ip->m_plugin == stub);

    ip->setPluginParameter("Foo", 42);

    QVERIFY(ip->m_parametersCache.count() == 1);
    QVERIFY(ip->getPluginParameters().count() == 1);
    QVERIFY(ip->getPluginParameters().key(42) == "Foo");
    QVERIFY(ip->getPluginParameters().value("Foo") == 42);

    delete ip;
}

namespace
{
/** Counts the values it is given, and the queued calls that brought them. */
class InputCounter : public QObject
{
public:
    int values = 0;
    int metaCalls = 0;
    QSet<quint32> universes;
    void onValue(quint32 universe, quint32, uchar, const QString &)
    {
        values++;
        universes.insert(universe);
    }
protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::MetaCall)
            metaCalls++;
        return QObject::event(e);
    }
};
}

void InputPatch_Test::flushFromAUniverseThreadIsOneEvent()
{
    InputPatch ip(7, this);
    InputCounter counter;
    connect(&ip, &InputPatch::inputValueChanged, &counter, &InputCounter::onValue);

    // One frame: every channel of the universe changed.
    for (quint32 ch = 0; ch < 512; ch++)
        ip.m_inputBuffer.insert(ch, InputPatch::InputValue(uchar(ch % 250 + 1), QString()));

    // Flushed where Universe::processFaders() flushes: its own thread.
    QThread *worker = QThread::create([&ip]() { ip.flush(7); });
    worker->start();
    QVERIFY(worker->wait(5000));
    delete worker;
    QVERIFY(ip.m_inputBuffer.isEmpty());

    QTRY_COMPARE(counter.values, 512);
    QCOMPARE(counter.universes, QSet<quint32>() << 7);
    // Delivered from the GUI thread, directly: no queued event per value.
    QCOMPARE(counter.metaCalls, 0);

    // A flush on the patch's own thread still emits at once.
    ip.m_inputBuffer.insert(3, InputPatch::InputValue(9, QString()));
    ip.flush(7);
    QCOMPARE(counter.values, 513);

    // Another universe's flush leaves this patch's values alone.
    ip.m_inputBuffer.insert(4, InputPatch::InputValue(9, QString()));
    ip.flush(8);
    QCOMPARE(counter.values, 513);
    QCOMPARE(ip.m_inputBuffer.size(), 1);
}

QTEST_GUILESS_MAIN(InputPatch_Test)
