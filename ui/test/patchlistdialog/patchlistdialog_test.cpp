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
#include <QRegularExpression>
#include <QFileDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QSettings>
#include <functional>
#include <memory>

#include "patchlabelsdialog.h"
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

/* The stock remembered goes to a scratch settings file, never the user's. */
void PatchListDialog_Test::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
}

/* The pages in a PDF: its page objects ("/Type /Page", not "/Pages"). */
static int pdfPages(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    static const QRegularExpression page(QStringLiteral("/Type\\s*/Page(?![a-zA-Z])"));
    int n = 0;
    QRegularExpressionMatchIterator it = page.globalMatch(QString::fromLatin1(f.readAll()));
    while (it.hasNext())
    {
        it.next();
        n++;
    }
    return n;
}

static QStringList rowNames(const QList<PatchList::Row> &rows)
{
    QStringList out;
    foreach (const PatchList::Row &r, rows)
        out << r.name;
    return out;
}

void PatchListDialog_Test::init()
{
    /* Qt's own file dialog, which the test can drive: on Linux the native
       one can't show under the offscreen platform, returns at once with no
       file, and the save "fails" (the Linux CI). */
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
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

    // The suggested name, accepted directly: on Linux the file dialog puts
    // its suggested name back over one typed in.
    const QString path = dir.filePath("patch-list.pdf");
    whenModal<QFileDialog>([path](QFileDialog *fd) {
        fd->setDirectory(QFileInfo(path).absolutePath());
        fd->selectFile(path);
        static_cast<QDialog *>(fd)->done(QDialog::Accepted);
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

void PatchListDialog_Test::selectedRowsAreInListOrder()
{
    PatchListDialog dlg(m_doc);
    QVERIFY(dlg.selectedRows().isEmpty());
    QItemSelectionModel *sel = dlg.table()->selectionModel();
    sel->select(dlg.table()->model()->index(2, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    sel->select(dlg.table()->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    QCOMPARE(rowNames(dlg.selectedRows()), QStringList({ "Front", "Spare" }));
}

void PatchListDialog_Test::labelsOpensTheLabelsDialog()
{
    PatchListDialog dlg(m_doc);
    QPushButton *labels = dlg.findChild<QPushButton *>("patchListLabels");
    QVERIFY(labels != nullptr);
    int seen = -1;
    QString help;
    whenModal<PatchLabelsDialog>([&seen, &help](PatchLabelsDialog *ld) {
        seen = ld->rows().size();
        help = ld->property("helpTopic").toString();
        ld->reject();
    });
    labels->click();
    QCOMPARE(seen, 3);
    QCOMPARE(help, QString("fixture-manager.md#labels"));
}

void PatchListDialog_Test::labelsAreForAllOrTheSelectedRows()
{
    PatchListDialog dlg(m_doc);
    const QList<PatchList::Row> all = dlg.list().rows();

    // Nothing selected: all of them, and "selected" can't be picked.
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QVERIFY(!ld.selectedOnly());
        ld.setSelectedOnly(true);
        QVERIFY(!ld.selectedOnly());
        QCOMPARE(ld.rows().size(), 3);
        QCOMPARE(ld.findChild<QLabel *>("labelCount")->text(), QString("3 labels, on 1 sheet"));
    }

    // A selection: those, unless all is asked for.
    const QList<PatchList::Row> picked = { all.at(1) };
    PatchLabelsDialog ld(all, picked, "Labels", "labels.pdf");
    QVERIFY(ld.selectedOnly());
    QCOMPARE(rowNames(ld.rows()), QStringList({ "Back" }));
    QCOMPARE(ld.findChild<QLabel *>("labelCount")->text(), QString("1 label, on 1 sheet"));
    ld.setSelectedOnly(false);
    QCOMPARE(ld.rows().size(), 3);
}

void PatchListDialog_Test::theLabelStockIsRemembered()
{
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("avery5160"));      // the default
        ld.findChild<QComboBox *>("labelSheet")->setCurrentIndex(
            ld.findChild<QComboBox *>("labelSheet")->findData("roll62"));
        QCOMPARE(ld.sheetKey(), QString("roll62"));
        QCOMPARE(ld.findChild<QLabel *>("labelCount")->text(), QString("3 labels"));
    }
    QCOMPARE(QSettings().value(SETTINGS_PATCH_LABEL_SHEET).toString(), QString("roll62"));
    PatchLabelsDialog again(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
    QCOMPARE(again.sheetKey(), QString("roll62"));

    // A stock that has gone away: the default.
    QSettings().setValue(SETTINGS_PATCH_LABEL_SHEET, "no such stock");
    PatchLabelsDialog gone(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
    QCOMPARE(gone.sheetKey(), QString("avery5160"));
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
}

void PatchListDialog_Test::saveLabelsAsksWhereAndWrites()
{
    QTemporaryDir dir;
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
    ld.setSheetKey("roll62");
    QPushButton *save = ld.findChild<QPushButton *>("saveLabelsPdf");
    QVERIFY(save != nullptr);

    const QString path = dir.filePath("labels.pdf");
    whenModal<QFileDialog>([path](QFileDialog *fd) {
        fd->setDirectory(QFileInfo(path).absolutePath());
        fd->selectFile(path);
        static_cast<QDialog *>(fd)->done(QDialog::Accepted);
    });
    save->click();
    QCOMPARE(ld.result(), int(QDialog::Accepted));
    QCOMPARE(pdfPages(path), 3);                // the roll: a label a page

    ld.setSheetKey("avery5160");
    QString error;
    QVERIFY2(ld.savePdf(path, &error), qPrintable(error));
    QCOMPARE(pdfPages(path), 1);
    QVERIFY(!ld.savePdf("/nonexistent-dir/labels.pdf", &error));
    QVERIFY(!error.isEmpty());
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
}

QTEST_MAIN(PatchListDialog_Test)
