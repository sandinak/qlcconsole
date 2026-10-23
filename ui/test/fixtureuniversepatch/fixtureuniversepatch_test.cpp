/*
  Q Light Controller Plus - Test Unit
  fixtureuniversepatch_test.cpp

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
#include <QTreeWidgetItemIterator>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QMenu>
#include <QTimer>
#include <functional>
#include <memory>

#define KColumnName 0

#define protected public
#define private public
#include "fixturemanager.h"
#include "fixturetreewidget.h"
#include "universepatcher.h"
#include "iopluginstub.h"
#include "ioplugincache.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "patchundo.h"
#include "showstatus.h"
#include "universe.h"
#include "fixture.h"
#include "qlcfile.h"
#include "doc.h"
#undef private
#undef protected

#include "fixtureuniversepatch_test.h"

#define TESTPLUGINDIR "../../../engine/test/iopluginstub"

static const char *kNode = "172.18.2.50";

static IOPluginStub *stubOf(Doc *doc)
{
    if (doc->ioPluginCache() == NULL || doc->ioPluginCache()->plugins().isEmpty())
        return NULL;
    return static_cast<IOPluginStub *>(doc->ioPluginCache()->plugins().at(0));
}

/** A four-port node heard on the stub's first line, and eight universes. */
static IOPluginStub *fourPortNode(Doc *doc)
{
    IOPluginStub *stub = stubOf(doc);
    stub->m_supportsTargets = true;
    QLCIOPlugin::Device node;
    node.line = 0;
    node.name = "Quad";
    node.address = kNode;
    for (quint32 p = 0; p < 4; p++)
    {
        node.portLabels << QString("0:0:%1").arg(p);
        node.portUniverses << p;
    }
    stub->m_devices << node;
    InputOutputMap *iom = doc->inputOutputMap();
    while (iom->universesCount() < 8)
        iom->addUniverse();
    return stub;
}

static QTreeWidgetItem *universeRow(FixtureManager &mgr, quint32 uni)
{
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_UNIVERSE).isValid()
                && (*it)->data(KColumnName, PROP_UNIVERSE).toUInt() == uni)
            return *it;
    return nullptr;
}

/** Answer the dialogs about to open, in order. An answer to a list is the
    first item starting with it ("#N" picks the Nth); to a text box, the text;
    "<cancel>" cancels. A message box is answered Yes (or OK) and its text
    kept. */
class DialogAnswers : public QObject
{
public:
    DialogAnswers(const QStringList &answers) : m_answers(answers)
    {
        m_timer.setInterval(5);
        connect(&m_timer, &QTimer::timeout, this, [this]() { step(); });
        m_timer.start();
    }
    QList<QStringList> lists;
    QStringList messages;
    int left() const { return m_answers.count(); }

private:
    void step()
    {
        QWidget *w = QApplication::activeModalWidget();
        if (w == nullptr)
            return;
        if (QMessageBox *mb = qobject_cast<QMessageBox *>(w))
        {
            messages << mb->text();
            if (QAbstractButton *yes = mb->button(QMessageBox::Yes))
                yes->click();
            else if (QAbstractButton *ok = mb->button(QMessageBox::Ok))
                ok->click();
            else
                mb->accept();
            return;
        }
        QInputDialog *d = qobject_cast<QInputDialog *>(w);
        if (d == nullptr || m_answers.isEmpty())
            return;
        const QString a = m_answers.takeFirst();
        const QStringList items = d->comboBoxItems();
        if (items.isEmpty() == false)
            lists << items;
        if (a == "<cancel>")
        {
            d->reject();
            return;
        }
        if (items.isEmpty() == false)
        {
            QString pick;
            if (a.startsWith('#'))
                pick = items.value(a.mid(1).toInt());
            else
                foreach (const QString &it, items)
                    if (pick.isEmpty() && it.startsWith(a))
                        pick = it;
            if (pick.isEmpty())
            {
                d->reject();
                return;
            }
            d->setTextValue(pick);
        }
        else
            d->setTextValue(a);
        d->accept();
    }
    QStringList m_answers;
    QTimer m_timer;
};

/** Right-click @p item WITHOUT touching the selection, and either read the
    menu (@p pick empty) or choose the entry titled @p pick. Returns the
    menu's texts; @p found says whether @p pick was there. */
static QStringList rightClick(FixtureManager &mgr, QTreeWidgetItem *item,
                              const QString &pick = QString(), bool *found = nullptr)
{
    QStringList texts;
    bool hit = false;
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [&texts, &hit, pick, poll]() {
        QMenu *m = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        if (m == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        QAction *chosen = nullptr;
        foreach (QAction *a, m->actions())
        {
            if (a->isSeparator())
                continue;
            texts << a->text();
            if (!pick.isEmpty() && a->text() == pick && a->isEnabled())
                chosen = a;
        }
        if (chosen != nullptr)
        {
            hit = true;
            m->setActiveAction(chosen);
            QTest::keyClick(m, Qt::Key_Return);
            return;
        }
        m->close();
    };
    QTimer::singleShot(0, *poll);
    mgr.slotContextMenuRequested(mgr.m_fixtures_tree->visualItemRect(item).center());
    if (found != nullptr)
        *found = hit;
    return texts;
}

/** Select just @p item, as a plain click would. */
static void selectOnly(FixtureManager &mgr, QTreeWidgetItem *item)
{
    mgr.m_fixtures_tree->clearSelection();
    mgr.m_fixtures_tree->setCurrentItem(item);
    item->setSelected(true);
}

void FixtureUniversePatch_Test::init()
{
    m_doc = new Doc(this);
    QDir dir(TESTPLUGINDIR);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(m_doc->ioPluginCache()->plugins().isEmpty() == false,
             "the I/O plugin stub did not load -- check TESTPLUGINDIR");
    stubOf(m_doc)->init();
}

void FixtureUniversePatch_Test::cleanup()
{
    delete m_doc;
    m_doc = NULL;
}

void FixtureUniversePatch_Test::anUnpatchedUniverseOffersPatchOutput()
{
    fourPortNode(m_doc);
    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *row = universeRow(mgr, 0);
    QVERIFY(row != nullptr);
    selectOnly(mgr, row);

    const QStringList texts = rightClick(mgr, row);
    QVERIFY2(texts.contains("Patch output…"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Unpatch output"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Delete Universe…"), qPrintable(texts.join(" | ")));
    // One universe selected: no plural entries.
    foreach (const QString &t, texts)
        QVERIFY2(!t.contains("selected universes"), qPrintable(t));
}

void FixtureUniversePatch_Test::patchingFromTheMenuSetsTheOutputAndCanBeUndone()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *row = universeRow(mgr, 0);
    selectOnly(mgr, row);

    {
        // The stub is the only protocol: interface, then the node's port --
        // the Connections tab's picker, free ports first.
        DialogAnswers answers(QStringList() << "#0" << "Quad (172.18.2.50) — port 3");
        bool found = false;
        rightClick(mgr, row, "Patch output…", &found);
        QVERIFY(found);
        QCOMPARE(answers.left(), 0);
        QCOMPARE(answers.lists.count(), 2);
        QVERIFY(answers.lists.at(1).contains("Another address…"));
        QVERIFY(answers.lists.at(1).contains("Broadcast (no target)"));
    }

    UniversePatcher p(m_doc, nullptr);
    QString a;
    quint32 port = 99;
    QCOMPARE(iom->outputPatchesCount(0), 1);
    QVERIFY(p.patchTarget(0, stub->name(), 0, a, port));
    QCOMPARE(a, QString(kNode));
    QCOMPARE(port, quint32(2));

    // The row says where it goes now, without a rebuild.
    QCOMPARE(universeRow(mgr, 0), row);
    QVERIFY2(row->text(KColumnName).contains(kNode), qPrintable(row->text(KColumnName)));
    QVERIFY(row->isSelected());

    // One undo step, on the stack the Connections tab's Undo button reads.
    QVERIFY(iom->patchUndo()->canUndo());
    QVERIFY(iom->patchUndo()->undo());
    QCOMPARE(iom->outputPatchesCount(0), 0);
    QApplication::processEvents();          // the relabel is a zero-delay hop
    QVERIFY2(!row->text(KColumnName).contains(kNode), qPrintable(row->text(KColumnName)));
}

void FixtureUniversePatch_Test::aPatchedUniverseSaysWhereItGoesAndCanBeUnpatched()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    UniversePatcher p(m_doc, nullptr);
    QVERIFY(iom->setOutputPatch(1, stub->name(), "", 0, false, 0));
    QVERIFY(p.applyTarget(1, stub->name(), 0, kNode, 1));

    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *row = universeRow(mgr, 1);
    selectOnly(mgr, row);

    const QStringList texts = rightClick(mgr, row);
    const QString where = QString("Output: %1 → Quad (172.18.2.50) · port 2 · universe 0:0:1")
                              .arg(stub->name());
    bool said = false;
    foreach (const QString &t, texts)
        said = said || t.startsWith(where);
    QVERIFY2(said, qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Change interface / target…"), qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Unpatch output"), qPrintable(texts.join(" | ")));
    QVERIFY2(!texts.contains("Patch output…"), qPrintable(texts.join(" | ")));

    // Change interface / target: interface asked again, then the port.
    {
        DialogAnswers answers(QStringList() << "#0" << "Quad (172.18.2.50) — port 4");
        bool found = false;
        rightClick(mgr, row, "Change interface / target…", &found);
        QVERIFY(found);
        QCOMPARE(answers.left(), 0);
    }
    QString a;
    quint32 port = 99;
    QVERIFY(p.patchTarget(1, stub->name(), 0, a, port));
    QCOMPARE(port, quint32(3));

    bool found = false;
    rightClick(mgr, row, "Unpatch output", &found);
    QVERIFY(found);
    QCOMPARE(iom->outputPatchesCount(1), 0);
    QVERIFY2(!row->text(KColumnName).contains(kNode), qPrintable(row->text(KColumnName)));

    QVERIFY(iom->patchUndo()->undo());
    QCOMPARE(iom->outputPatchesCount(1), 1);
    QVERIFY(p.patchTarget(1, stub->name(), 0, a, port));
    QCOMPARE(port, quint32(3));
}

void FixtureUniversePatch_Test::severalSelectedArePatchedInTheOrderTheyWerePicked()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    FixtureManager mgr(nullptr, m_doc);

    // Picked 3, 1, 4, 2 -- not the order they are listed in.
    mgr.m_fixtures_tree->clearSelection();
    const QList<quint32> picked = QList<quint32>() << 2 << 0 << 3 << 1;
    foreach (quint32 u, picked)
        universeRow(mgr, u)->setSelected(true);
    QCOMPARE(mgr.orderedSelectedUniverses(), picked);

    {
        // Interface, device, the port to start from, then the plan (Yes).
        DialogAnswers answers(QStringList() << "#0" << "Quad (172.18.2.50)" << "#0");
        bool found = false;
        const QStringList texts = rightClick(mgr, universeRow(mgr, 3),
                                             "Patch 4 selected universes to…", &found);
        QVERIFY2(found, qPrintable(texts.join(" | ")));
        QCOMPARE(answers.left(), 0);
        QCOMPARE(answers.messages.count(), 1);
        QVERIFY2(answers.messages.first().contains("in the order you selected them"),
                 qPrintable(answers.messages.first()));
    }

    UniversePatcher p(m_doc, nullptr);
    for (int k = 0; k < picked.count(); k++)
    {
        QString a;
        quint32 port = 99;
        QVERIFY(p.patchTarget(picked.at(k), stub->name(), 0, a, port));
        QCOMPARE(a, QString(kNode));
        QCOMPARE(port, quint32(k));          // port k carries the k-th picked
    }
    QCOMPARE(iom->outputPatchesCount(4), 0);

    // One undo step takes all four back.
    QVERIFY(iom->patchUndo()->undo());
    foreach (quint32 u, picked)
        QCOMPARE(iom->outputPatchesCount(u), 0);
}

void FixtureUniversePatch_Test::severalSelectedAreUnpatchedInOneStep()
{
    IOPluginStub *stub = fourPortNode(m_doc);
    InputOutputMap *iom = m_doc->inputOutputMap();
    UniversePatcher p(m_doc, nullptr);
    for (quint32 u = 0; u < 3; u++)
    {
        QVERIFY(iom->setOutputPatch(u, stub->name(), "", 0, false, 0));
        QVERIFY(p.applyTarget(u, stub->name(), 0, kNode, u));
    }

    FixtureManager mgr(nullptr, m_doc);
    mgr.m_fixtures_tree->clearSelection();
    // Two patched and one not: the count is of the patched ones.
    universeRow(mgr, 0)->setSelected(true);
    universeRow(mgr, 2)->setSelected(true);
    universeRow(mgr, 5)->setSelected(true);

    bool found = false;
    const QStringList texts = rightClick(mgr, universeRow(mgr, 0),
                                         "Unpatch 2 selected universes", &found);
    QVERIFY2(found, qPrintable(texts.join(" | ")));
    QVERIFY2(texts.contains("Patch 3 selected universes to…"), qPrintable(texts.join(" | ")));
    QCOMPARE(iom->outputPatchesCount(0), 0);
    QCOMPARE(iom->outputPatchesCount(2), 0);
    QCOMPARE(iom->outputPatchesCount(1), 1);     // not selected: untouched

    QVERIFY(iom->patchUndo()->undo());
    QCOMPARE(iom->outputPatchesCount(0), 1);
    QCOMPARE(iom->outputPatchesCount(2), 1);
}

void FixtureUniversePatch_Test::aFixtureRowsMenuIsUnchanged()
{
    fourPortNode(m_doc);
    Fixture *fxi = new Fixture(m_doc);
    fxi->setName("Probe");
    fxi->setChannels(4);
    fxi->setUniverse(0);
    fxi->setAddress(0);
    QVERIFY(m_doc->addFixture(fxi));

    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *fx = nullptr;
    for (QTreeWidgetItemIterator it(mgr.m_fixtures_tree); *it != nullptr; ++it)
        if ((*it)->data(KColumnName, PROP_ID).isValid()
                && (*it)->data(KColumnName, PROP_ID).toString().toUInt() == fxi->id()
                && !(*it)->data(KColumnName, PROP_HEAD).isValid())
            fx = *it;
    QVERIFY(fx != nullptr);
    selectOnly(mgr, fx);

    const QStringList texts = rightClick(mgr, fx);
    QVERIFY2(!texts.isEmpty(), "the context menu never appeared");
    foreach (const QString &t, texts)
        QVERIFY2((!t.contains("output", Qt::CaseInsensitive)
                  || t == "Output Rules…")      // the fixture's own (relays, house lights)
                 && !t.contains("selected universes")
                 && t != "Delete Universe…", qPrintable(texts.join(" | ")));
}

void FixtureUniversePatch_Test::showInConnectionsHandsOverTheUniverse()
{
    fourPortNode(m_doc);
    QVariantList handed;
    ShowStatus::instance()->setAction("connections.universe", "Show in Connections",
        [&handed](QWidget *, const QVariantList &items) { handed = items; });

    FixtureManager mgr(nullptr, m_doc);
    QTreeWidgetItem *row = universeRow(mgr, 4);
    selectOnly(mgr, row);
    bool found = false;
    rightClick(mgr, row, "Show in Connections", &found);
    ShowStatus::instance()->setAction("connections.universe", QString(), ShowStatus::Action());
    QVERIFY(found);
    QCOMPARE(handed, QVariantList() << quint32(4));
}

QTEST_MAIN(FixtureUniversePatch_Test)
