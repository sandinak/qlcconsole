/*
  Q Light Controller - Unit test
  outputpatch_test.cpp

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

#include "../common/stdprelude.h"
#define private public
#include "iopluginstub.h"
#include "outputpatch_test.h"
#include "outputpatch.h"
#include "qlcfile.h"
#include "doc.h"
#undef private

#define TESTPLUGINDIR "../iopluginstub"

namespace
{
/** A network plugin that keeps each universe's parameters: what a patch
 *  hands it is what it has (not registered with any Doc) */
class KeepingPlugin : public QLCIOPlugin
{
public:
    explicit KeepingPlugin(const QString &name) : m_name(name) {}
    void init() override {}
    QString name() const override { return m_name; }
    int capabilities() const override { return Output; }
    QString pluginInfo() const override { return QString(); }
    QStringList outputs() override { return QStringList() << QStringLiteral("eth0"); }
    bool openOutput(quint32, quint32) override { return true; }
    void closeOutput(quint32, quint32) override {}
    void setParameter(quint32 universe, quint32, Capability, QString name, QVariant value) override
    { m_params[universe][name] = value; }
    void unSetParameter(quint32 universe, quint32, Capability, QString name) override
    { m_params[universe].remove(name); }
    QMap<QString, QVariant> getParameters(quint32 universe, quint32, Capability) const override
    { return m_params.value(universe); }
    QMap<quint32, QMap<QString, QVariant> > m_params;
private:
    QString m_name;
};
}

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

void OutputPatch_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void OutputPatch_Test::cleanupTestCase()
{
    delete m_doc;
    m_doc = NULL;
}

void OutputPatch_Test::defaults()
{
    OutputPatch op(this);
    QVERIFY(op.m_plugin == NULL);
    QVERIFY(op.m_pluginLine == QLCIOPlugin::invalidLine());
    QVERIFY(op.pluginName() == KOutputNone);
    QVERIFY(op.outputName() == KOutputNone);
}

void OutputPatch_Test::patch()
{
    InputOutputMap om(m_doc, 4);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    OutputPatch* op = new OutputPatch(0, this);
    op->set(stub, 0);
    QVERIFY(op->m_plugin == stub);
    QVERIFY(op->m_pluginLine == 0);
    QVERIFY(op->pluginName() == stub->name());
    QVERIFY(op->outputName() == stub->outputs()[0]);
    QVERIFY(stub->m_openOutputs.size() == 1);
    QVERIFY(stub->m_openOutputs.at(0) == 0);

    op->set(stub, 3);
    QVERIFY(op->m_plugin == stub);
    QVERIFY(op->m_pluginLine == 3);
    QVERIFY(op->pluginName() == stub->name());
    QVERIFY(op->outputName() == stub->outputs()[3]);
    QVERIFY(stub->m_openOutputs.size() == 1);
    QVERIFY(stub->m_openOutputs.at(0) == 3);

    op->reconnect();
    QVERIFY(op->m_plugin == stub);
    QVERIFY(op->m_pluginLine == 3);
    QVERIFY(op->pluginName() == stub->name());
    QVERIFY(op->outputName() == stub->outputs()[3]);
    QVERIFY(stub->m_openOutputs.size() == 1);
    QVERIFY(stub->m_openOutputs.at(0) == 3);

    delete op;
    QVERIFY(stub->m_openOutputs.size() == 0);
}

void OutputPatch_Test::pending()
{
    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    OutputPatch* op = new OutputPatch(0, this);
    op->set(stub, 2);
    QVERIFY(op->isPending() == false);
    QVERIFY(op->isPatched() == true);
    QVERIFY(stub->m_openOutputs.size() == 1);

    // The interface this patch is looking for is not one the plugin
    // currently offers -- e.g. an ArtNet IP not present on this machine.
    // Nothing should open, but the plugin association and the identity it
    // was looking for both survive.
    op->setPending(stub, "not a real interface");
    QVERIFY(op->isPending() == true);
    QVERIFY(op->isPatched() == false);
    QVERIFY(op->plugin() == stub);
    QVERIFY(op->pluginName() == stub->name());
    QVERIFY(op->outputName() == "not a real interface");
    QVERIFY(op->output() == QLCIOPlugin::invalidLine());
    // setPending() must close whatever was previously open, same as set().
    QVERIFY(stub->m_openOutputs.size() == 0);

    // A pending patch's own target parameters (an ArtNet outputIP/outputUni,
    // say) must still read back correctly -- getPluginParameters() asking
    // the PLUGIN would get nothing back for an invalid line, so it has to
    // fall back to what setPluginParameter() already recorded locally.
    // Losing this made a pending unicast patch read back as broadcast.
    op->setPluginParameter("outputIP", "172.18.2.221");
    op->setPluginParameter("outputUni", 3);
    const QMap<QString, QVariant> params = op->getPluginParameters();
    QCOMPARE(params.value("outputIP").toString(), QString("172.18.2.221"));
    QCOMPARE(params.value("outputUni").toInt(), 3);

    // The interface showing up again (or simply choosing a concrete line by
    // hand) resolves it: set() always supersedes a pending state.
    op->set(stub, 1);
    QVERIFY(op->isPending() == false);
    QVERIFY(op->isPatched() == true);
    QVERIFY(op->outputName() == stub->outputs()[1]);
    QVERIFY(stub->m_openOutputs.size() == 1);
    QVERIFY(stub->m_openOutputs.at(0) == 1);

    delete op;
}

void OutputPatch_Test::dump()
{
    QByteArray uni(513, char(0));
    uni[0] = 100;
    uni[169] = 50;
    uni[511] = 25;

    OutputPatch* op = new OutputPatch(0, this);

    IOPluginStub* stub = static_cast<IOPluginStub*>
                                (m_doc->ioPluginCache()->plugins().at(0));
    QVERIFY(stub != NULL);

    op->set(stub, 0);
    QVERIFY(stub->m_universe[0] == (char) 0);
    QVERIFY(stub->m_universe[169] == (char) 0);
    QVERIFY(stub->m_universe[511] == (char) 0);

    op->dump(0, uni, true);
    QVERIFY(stub->m_universe[0] == (char) 100);
    QVERIFY(stub->m_universe[169] == (char) 50);
    QVERIFY(stub->m_universe[511] == (char) 25);

    /* Test the pause state */
    op->setPaused(true);
    op->dump(0, uni, true);
    QVERIFY(stub->m_universe[0] == (char) 100);
    QVERIFY(stub->m_universe[169] == (char) 50);
    QVERIFY(stub->m_universe[511] == (char) 25);

    uni[0] = 1;
    uni[169] = 2;
    uni[511] = 3;

    op->dump(0, uni, true);
    QVERIFY(stub->m_universe[0] == (char) 100);
    QVERIFY(stub->m_universe[169] == (char) 50);
    QVERIFY(stub->m_universe[511] == (char) 25);

    op->setPaused(false);
    op->dump(0, uni, true);
    QVERIFY(stub->m_universe[0] == (char) 1);
    QVERIFY(stub->m_universe[169] == (char) 2);
    QVERIFY(stub->m_universe[511] == (char) 3);

    delete op;
}

/* Review 21 (stale cache): a patch moved to another plugin keeps none of
   the old one's parameters -- an Art-Net outputIP would be replayed onto
   the new plugin by reconnect() and saved with it (getPluginParameters()
   while pending). Same for setPending() onto another plugin. */
void OutputPatch_Test::anotherPluginForgetsTheOldOnesParameters()
{
    KeepingPlugin artnet(QStringLiteral("ArtNet")), sacn(QStringLiteral("E1.31"));
    {
        OutputPatch op(quint32(0));
        QVERIFY(op.set(&artnet, 0));
        op.setPluginParameter(QStringLiteral("outputIP"), QStringLiteral("10.0.0.5"));
        op.setPluginParameter(QStringLiteral("outputUni"), 7);
        QVERIFY(op.set(&sacn, 0));
        QVERIFY(op.reconnect());
        QVERIFY2(sacn.m_params.value(0).isEmpty(),
                 qPrintable("reconnect() replayed Art-Net's onto E1.31: "
                            + QStringList(sacn.m_params.value(0).keys()).join(", ")));
        op.setPending(&sacn, QStringLiteral("eth0"));
        QVERIFY(op.getPluginParameters().isEmpty());
    }
    {
        OutputPatch op(quint32(1));
        QVERIFY(op.set(&artnet, 0));
        op.setPluginParameter(QStringLiteral("outputIP"), QStringLiteral("10.0.0.6"));
        op.setPending(&sacn, QStringLiteral("eth0"));
        QVERIFY2(op.getPluginParameters().isEmpty(), "pending on E1.31, it still says Art-Net's outputIP");
    }
    {
        // The same plugin on another line keeps them: still its own target.
        OutputPatch op(quint32(2));
        QVERIFY(op.set(&artnet, 0));
        op.setPluginParameter(QStringLiteral("outputIP"), QStringLiteral("10.0.0.7"));
        op.setPending(&artnet, QStringLiteral("eth1"));
        QCOMPARE(op.getPluginParameters().value(QStringLiteral("outputIP")).toString(), QString("10.0.0.7"));
    }
}

QTEST_APPLESS_MAIN(OutputPatch_Test)
