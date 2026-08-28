/*
  Q Light Controller Plus - Test Unit
  patchundo_test.cpp

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
#include <QtTest>

#define private public
#define protected public

#include "iopluginstub.h"
#include "patchundo_test.h"
#include "inputoutputmap.h"
#include "patchundo.h"
#include "outputpatch.h"
#include "inputpatch.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

#undef protected
#undef private

#define TESTPLUGINDIR "../iopluginstub"
#define ENGINEDIR "../../src"
#include "../common/resource_paths.h"

static QDir testPluginDir()
{
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    return dir;
}

static QString stubName(Doc *doc)
{
    return doc->ioPluginCache()->plugins().at(0)->name();
}

void PatchUndo_Test::initTestCase()
{
    m_doc = NULL;
}

void PatchUndo_Test::cleanupTestCase()
{
}

/* A fresh Doc per test. Plugin parameters live in the PLUGIN, keyed by
   universe, not in the patch -- so a shared Doc carries one test's outputIP
   into the next one's universe 0 and the failure lands somewhere unrelated to
   the cause. Found the hard way: these tests passed individually and failed as
   a suite. */
void PatchUndo_Test::init()
{
    m_doc = new Doc(this);
    m_doc->ioPluginCache()->load(testPluginDir());
    QVERIFY(m_doc->ioPluginCache()->plugins().size() != 0);
}

void PatchUndo_Test::cleanup()
{
    delete m_doc;
    m_doc = NULL;
}

void PatchUndo_Test::nothingHeldInitially()
{
    InputOutputMap iom(m_doc, 4);
    QVERIFY(iom.patchUndo() != NULL);
    QVERIFY(iom.patchUndo()->canUndo() == false);
    QVERIFY(iom.patchUndo()->summary().isEmpty());
    // Undoing nothing is a no-op, not a crash and not a false success.
    QVERIFY(iom.patchUndo()->undo() == false);
}

void PatchUndo_Test::captureThenUndoRestoresTheOutputPatch()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 1, false, 0) == true);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));

    iom.patchUndo()->capture(QList<quint32>() << 0, "retarget universe 1");
    QVERIFY(iom.patchUndo()->canUndo() == true);
    QCOMPARE(iom.patchUndo()->summary(), QString("retarget universe 1"));

    QVERIFY(iom.setOutputPatch(0, stub, "", 3, false, 0) == true);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(3));

    QVERIFY(iom.patchUndo()->undo() == true);
    QCOMPARE(iom.outputPatchesCount(0), 1);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));

    // One step: spending it leaves nothing held.
    QVERIFY(iom.patchUndo()->canUndo() == false);
}

void PatchUndo_Test::undoRestoresPluginParameters()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    iom.outputPatch(0, 0)->setPluginParameter("outputIP", "172.18.2.10");
    iom.outputPatch(0, 0)->setPluginParameter("outputUni", 17);

    iom.patchUndo()->capture(QList<quint32>() << 0, "retarget");

    iom.outputPatch(0, 0)->setPluginParameter("outputIP", "10.0.0.9");
    iom.outputPatch(0, 0)->setPluginParameter("outputUni", 3);

    QVERIFY(iom.patchUndo()->undo() == true);

    // Where a patch points is the whole reason undo exists here; restoring the
    // plugin line without its parameters would put the patch back on the right
    // interface and the wrong node.
    const QMap<QString, QVariant> p = iom.outputPatch(0, 0)->getPluginParameters();
    QCOMPARE(p.value("outputIP").toString(), QString("172.18.2.10"));
    QCOMPARE(p.value("outputUni").toInt(), 17);
}

void PatchUndo_Test::undoClearsParametersTheChangeAdded()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    // Captured with NO outputIP at all -- i.e. broadcasting.
    iom.patchUndo()->capture(QList<quint32>() << 0, "aim at a node");
    QVERIFY(iom.outputPatch(0, 0)->getPluginParameters().contains("outputIP") == false);

    iom.outputPatch(0, 0)->setPluginParameter("outputIP", "172.18.2.10");
    QVERIFY(iom.outputPatch(0, 0)->getPluginParameters().contains("outputIP"));

    QVERIFY(iom.patchUndo()->undo() == true);

    // Writing the captured map back only overwrites keys the capture held, so
    // a parameter the change ADDED would survive the undo and the patch would
    // stay unicast when it used to broadcast. Absent must restore as absent.
    QVERIFY(iom.outputPatch(0, 0)->getPluginParameters().contains("outputIP") == false);
}

void PatchUndo_Test::undoRemovesAnExtraOutputLeg()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    iom.patchUndo()->capture(QList<quint32>() << 0, "add a second output");

    QVERIFY(iom.setOutputPatch(0, stub, "", 1, false, 1) == true);
    QCOMPARE(iom.outputPatchesCount(0), 2);

    QVERIFY(iom.patchUndo()->undo() == true);
    QCOMPARE(iom.outputPatchesCount(0), 1);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
}

void PatchUndo_Test::undoRestoresARemovedOutputLeg()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    QVERIFY(iom.setOutputPatch(0, stub, "", 2, false, 1) == true);
    iom.outputPatch(0, 1)->setPluginParameter("outputIP", "172.18.2.11");
    QCOMPARE(iom.outputPatchesCount(0), 2);

    iom.patchUndo()->capture(QList<quint32>() << 0, "unpatch one leg");

    // Drop the FIRST leg, so restoring has to rebuild the order rather than
    // just append: getting this wrong swaps which node each leg feeds.
    QVERIFY(iom.setOutputPatch(0, QString(), "", 0, false, 0) == true);
    QCOMPARE(iom.outputPatchesCount(0), 1);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(2));

    QVERIFY(iom.patchUndo()->undo() == true);
    QCOMPARE(iom.outputPatchesCount(0), 2);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(0));
    QCOMPARE(iom.outputPatch(0, 1)->output(), quint32(2));
    QCOMPARE(iom.outputPatch(0, 1)->getPluginParameters().value("outputIP").toString(),
             QString("172.18.2.11"));
}

void PatchUndo_Test::undoRestoresTheInputPatchAndProfile()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setInputPatch(0, stub, "", 2) == true);
    iom.inputPatch(0)->setPluginParameter("midichannel", 5);

    iom.patchUndo()->capture(QList<quint32>() << 0, "change input");

    QVERIFY(iom.setInputPatch(0, stub, "", 3) == true);
    QCOMPARE(iom.inputPatch(0)->input(), quint32(3));

    QVERIFY(iom.patchUndo()->undo() == true);
    QVERIFY(iom.inputPatch(0) != NULL);
    QCOMPARE(iom.inputPatch(0)->input(), quint32(2));
    QCOMPARE(iom.inputPatch(0)->getPluginParameters().value("midichannel").toInt(), 5);
}

void PatchUndo_Test::undoClearsAnInputPatchThatDidNotExist()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    // Nothing patched at capture time.
    QVERIFY(iom.inputPatch(0) == NULL || iom.inputPatch(0)->plugin() == NULL);
    iom.patchUndo()->capture(QList<quint32>() << 0, "patch an input");

    QVERIFY(iom.setInputPatch(0, stub, "", 1) == true);
    QVERIFY(iom.inputPatch(0) != NULL);

    QVERIFY(iom.patchUndo()->undo() == true);

    // "There was no patch here" must restore as a CLEARED patch, not as a
    // patch to an empty plugin -- otherwise undoing a first-time patch leaves
    // a broken one behind instead of nothing.
    QVERIFY(iom.inputPatch(0) == NULL || iom.inputPatch(0)->plugin() == NULL);
}

void PatchUndo_Test::undoIsOneStepDeep()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    iom.patchUndo()->capture(QList<quint32>() << 0, "first");
    QVERIFY(iom.setOutputPatch(0, stub, "", 1, false, 0) == true);

    // A second capture replaces the first: the older state is gone, not
    // stacked. Restoring a state two changes stale would look like it worked.
    iom.patchUndo()->capture(QList<quint32>() << 0, "second");
    QCOMPARE(iom.patchUndo()->summary(), QString("second"));
    QVERIFY(iom.setOutputPatch(0, stub, "", 2, false, 0) == true);

    QVERIFY(iom.patchUndo()->undo() == true);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));   // not 0
}

void PatchUndo_Test::undoSkipsUniversesThatWentAway()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 1, false, 0) == true);
    QVERIFY(iom.setOutputPatch(3, stub, "", 1, false, 0) == true);
    iom.patchUndo()->capture(QList<quint32>() << 0 << 3, "bulk retarget");

    QVERIFY(iom.setOutputPatch(0, stub, "", 2, false, 0) == true);
    QVERIFY(iom.setOutputPatch(3, stub, "", 2, false, 0) == true);

    // The last universe is removed after the capture. Universe add/remove is
    // explicitly out of scope, so its entry is skipped -- but the rest of the
    // step must still restore rather than the whole undo failing.
    QVERIFY(iom.removeUniverse(3) == true);

    QVERIFY(iom.patchUndo()->undo() == true);
    QCOMPARE(iom.outputPatch(0, 0)->output(), quint32(1));
}

void PatchUndo_Test::captureIgnoresDuplicatesAndUnknownUniverses()
{
    InputOutputMap iom(m_doc, 4);

    iom.patchUndo()->capture(QList<quint32>() << 0 << 0 << 0, "dupes");
    QCOMPARE(iom.patchUndo()->m_held.count(), 1);

    // A universe that does not exist contributes nothing rather than a bogus
    // entry that would "restore" onto whatever later takes that id.
    iom.patchUndo()->capture(QList<quint32>() << 99, "ghost");
    QVERIFY(iom.patchUndo()->canUndo() == false);
}

void PatchUndo_Test::clearForgetsTheHeldStep()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);

    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);
    iom.patchUndo()->capture(QList<quint32>() << 0, "something");
    QVERIFY(iom.patchUndo()->canUndo() == true);

    iom.patchUndo()->clear();
    QVERIFY(iom.patchUndo()->canUndo() == false);
    QVERIFY(iom.patchUndo()->summary().isEmpty());
}

void PatchUndo_Test::changedSignalTracksAvailability()
{
    InputOutputMap iom(m_doc, 4);
    const QString stub = stubName(m_doc);
    QVERIFY(iom.setOutputPatch(0, stub, "", 0, false, 0) == true);

    QSignalSpy spy(iom.patchUndo(), SIGNAL(changed()));

    iom.patchUndo()->capture(QList<quint32>() << 0, "a");
    QCOMPARE(spy.count(), 1);

    // Undo spends the step, so the menu has to be told to grey out again.
    iom.patchUndo()->undo();
    QCOMPARE(spy.count(), 2);
    QVERIFY(iom.patchUndo()->canUndo() == false);
}

QTEST_GUILESS_MAIN(PatchUndo_Test)
