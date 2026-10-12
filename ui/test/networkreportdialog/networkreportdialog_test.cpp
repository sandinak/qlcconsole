/*
  Q Light Controller Plus - Test Unit
  networkreportdialog_test.cpp

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
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QClipboard>
#include <functional>
#include <memory>

#define private public
#include "networkreportdialog.h"
#undef private
#include "monitorproperties.h"
#include "networktopology.h"
#include "fixture.h"
#include "doc.h"

#include "networkreportdialog_test.h"

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
} // namespace

void NetworkReportDialog_Test::init()
{
    /* Qt's own file dialog, which the test can drive: on Linux the native
       one can't show under the offscreen platform, returns at once with no
       file, and the save "fails" (the Linux CI). */
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    m_doc = new Doc(this);
    Fixture *f = new Fixture(m_doc);
    f->setName("SR 1");
    f->setChannels(4);
    QVERIFY(m_doc->addFixture(f));
    m_doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, QVector3D(5000, 0, 0));

    NetworkNode n;
    n.name = "SR Node";
    n.placed = true;
    NetworkPort p;
    p.universe = 0;
    n.ports << p;
    m_doc->networkTopology()->nodes() << n;
}

void NetworkReportDialog_Test::cleanup()
{
    delete m_doc;
}

void NetworkReportDialog_Test::itShowsTheReportInThePlotsUnits()
{
    {
        NetworkReportDialog dlg(m_doc);
        QCOMPARE(dlg.windowTitle(), QString("Network Report"));
        QVERIFY(dlg.m_text->isReadOnly());
        QCOMPARE(dlg.m_text->toPlainText(), dlg.report().toText(false));
        QVERIFY(dlg.m_text->toPlainText().contains("run ~5.0 m"));
    }
    m_doc->monitorProperties()->setGridUnits(MonitorProperties::Feet);
    NetworkReportDialog dlg(m_doc);
    QVERIFY2(dlg.m_text->toPlainText().contains("run ~16.4 ft"), qPrintable(dlg.m_text->toPlainText()));
}

void NetworkReportDialog_Test::copyPutsTheSummaryOnTheClipboard()
{
    NetworkReportDialog dlg(m_doc);
    QApplication::clipboard()->clear();
    dlg.m_copyBtn->click();
    QCOMPARE(QApplication::clipboard()->text(), dlg.m_text->toPlainText());
}

void NetworkReportDialog_Test::saveCsvWritesTheSpreadsheet()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("net.csv");
    NetworkReportDialog dlg(m_doc);
    QVERIFY(dlg.saveCsv(path));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(f.readAll()), dlg.report().toCsv(false));
}

void NetworkReportDialog_Test::saveCsvReportsAPathItCannotWrite()
{
    NetworkReportDialog dlg(m_doc);
    QString error;
    QVERIFY(!dlg.saveCsv("/nonexistent-dir/net.csv", &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!dlg.saveCsv("/nonexistent-dir/net.csv"));
}

void NetworkReportDialog_Test::saveCsvAsksWhereAndCanBeCancelled()
{
    QTemporaryDir dir;
    NetworkReportDialog dlg(m_doc);

    // Cancelled: nothing written, nothing said.
    whenModal<QFileDialog>([](QFileDialog *fd) { fd->reject(); });
    dlg.slotSaveCsv();
    QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());

    // Chosen: written there.
    // The suggested name: on Linux the dialog puts it back over a typed one.
    const QString path = dir.filePath("network-report.csv");
    QStringList chosen;
    whenModal<QFileDialog>([path, &chosen](QFileDialog *fd) {
        fd->setDirectory(QFileInfo(path).absolutePath());
        fd->selectFile(path);
        chosen = fd->selectedFiles();
        static_cast<QDialog *>(fd)->done(QDialog::Accepted);
    });
    dlg.slotSaveCsv();
    QVERIFY2(QFile::exists(path), qPrintable(chosen.join(", ")));

    // Somewhere it cannot write -- a read-only folder (Linux's file dialog
    // won't even accept one that doesn't exist): told so.
    const QString locked = dir.filePath("locked");
    QVERIFY(QDir().mkpath(locked));
    QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QString told;
    whenModal<QFileDialog>([&told, locked](QFileDialog *fd) {
        whenModal<QMessageBox>([&told](QMessageBox *mb) {
            told = mb->text();
            mb->accept();
        });
        fd->setDirectory(locked);
        fd->selectFile(locked + "/network-report.csv");
        static_cast<QDialog *>(fd)->done(QDialog::Accepted);
    });
    dlg.slotSaveCsv();
    QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QVERIFY2(told.startsWith("Could not save ") && told.contains("locked/network-report.csv"), qPrintable(told));
}

QTEST_MAIN(NetworkReportDialog_Test)
