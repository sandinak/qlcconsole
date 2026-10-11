/*
  Q Light Controller Plus - Test Unit
  patchlistdialog_test.cpp

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
#include <QTemporaryDir>
#include <QTableWidget>
#include <QApplication>
#include <QFileDialog>
#include <QPushButton>
#include <QClipboard>
#include <functional>
#include <memory>

#include "patchlistdialog.h"
#include "fixture.h"
#include "doc.h"

#include "patchlistdialog_test.h"

namespace
{
/* Call @p act with the next modal widget of type T once it is up. */
template <class T>
void whenModal(const std::function<void(T *)> &act)
{
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [act, poll]() {
        T *w = qobject_cast<T *>(QApplication::activeModalWidget());
        if (w == nullptr)
        {
            QTimer::singleShot(10, *poll);
            return;
        }
        act(w);
    };
    QTimer::singleShot(0, *poll);
}

Fixture *dimmer(Doc *doc, const QString &name, quint32 universe, quint32 address, bool patched = true)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    f->setChannels(4);
    f->setUniverse(universe);
    f->setAddress(address);
    f->setPatched(patched);
    if (!doc->addFixture(f))
    {
        delete f;
        return nullptr;
    }
    return f;
}
} // namespace

void PatchListDialog_Test::init()
{
    m_doc = new Doc(this);
    dimmer(m_doc, "Spare", 0, 0, false);
    dimmer(m_doc, "Back", 0, 20);
    dimmer(m_doc, "Front", 0, 0);
}

void PatchListDialog_Test::cleanup()
{
    delete m_doc;
}

void PatchListDialog_Test::theTableIsInPatchOrder()
{
    PatchListDialog dlg(m_doc);
    QCOMPARE(dlg.windowTitle(), QString("Patch List"));
    QCOMPARE(dlg.table()->rowCount(), 3);
    QCOMPARE(dlg.table()->item(0, 1)->text(), QString("Front"));
    QCOMPARE(dlg.table()->item(0, 0)->text(), QString("1 · 1–4"));
    QCOMPARE(dlg.table()->item(1, 1)->text(), QString("Back"));
    QCOMPARE(dlg.table()->item(1, 0)->text(), QString("1 · 21–24"));
    QCOMPARE(dlg.table()->item(2, 1)->text(), QString("Spare"));
    QCOMPARE(dlg.table()->item(2, 0)->text(), QString("not patched"));
}

void PatchListDialog_Test::copyPutsTheCsvOnTheClipboard()
{
    PatchListDialog dlg(m_doc);
    QApplication::clipboard()->clear();
    dlg.slotCopy();
    QCOMPARE(QApplication::clipboard()->text(), dlg.list().toCsv());
}

void PatchListDialog_Test::saveCsvWritesTheSpreadsheet()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("patch.csv");
    PatchListDialog dlg(m_doc);
    QVERIFY(dlg.saveCsv(path));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(f.readAll()), dlg.list().toCsv());

    QString error;
    QVERIFY(!dlg.saveCsv("/nonexistent-dir/patch.csv", &error));
    QVERIFY(!error.isEmpty());
}

void PatchListDialog_Test::savePdfAsksWhereAndWrites()
{
    QTemporaryDir dir;
    PatchListDialog dlg(m_doc);
    QPushButton *pdf = dlg.findChild<QPushButton *>("patchListPdf");
    QVERIFY(pdf != nullptr);

    const QString path = dir.filePath("patch.pdf");
    whenModal<QFileDialog>([path](QFileDialog *fd) {
        fd->selectFile(path);
        QMetaObject::invokeMethod(fd, "accept");
    });
    pdf->click();
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().startsWith("%PDF"));
}

void PatchListDialog_Test::f1OpensThePatchListHelp()
{
    PatchListDialog dlg(m_doc);
    QCOMPARE(dlg.property("helpTopic").toString(), QString("fixture-manager.md#the-patch-list"));
}

QTEST_MAIN(PatchListDialog_Test)
