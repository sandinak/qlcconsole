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
#include <QTreeWidget>
#include <QLineEdit>
#include <QPrinter>
#include <QMenu>
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
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "qlcfixturedef.h"
#include "fixturegroup.h"
#include "truss.h"
#include "fixture.h"
#include "doc.h"

#include "patchlistdialog_test.h"

namespace
{
/* No printers: the Labels dialog never sees this computer's own (a Brother
   installed here would pick its stock). Tests that want one set it. */
const QList<PatchLabelsDialog::InstalledPrinter> kNoPrinters;

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
    PatchLabelsDialog::setInstalledPrintersForTest(&kNoPrinters);
}

/* The fixture rows of the tree, top to bottom, by name. */
static QStringList treeNames(QTreeWidget *tree)
{
    QStringList out;
    QTreeWidgetItemIterator it(tree);
    while (*it)
    {
        if ((*it)->data(0, Qt::UserRole).toInt() >= 0)
            out << (*it)->text(PatchListDialog::FixtureColumn);
        ++it;
    }
    return out;
}

static QStringList headings(QTreeWidget *tree)
{
    QStringList out;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        if (tree->topLevelItem(i)->data(0, Qt::UserRole).toInt() < 0)
            out << tree->topLevelItem(i)->text(0);
    }
    return out;
}

static Fixture *rgbBar(Doc *doc, const QString &name, int cells, quint32 universe, quint32 address)
{
    Fixture *f = new Fixture(doc);
    f->setName(name);
    QLCFixtureDef *def = f->genericRGBPanelDef(cells, Fixture::RGB, false);
    QLCFixtureMode *mode = f->genericRGBPanelMode(def, Fixture::RGB, false, 100, 100);
    f->setFixtureDefinition(def, mode);
    f->setUniverse(universe);
    f->setAddress(address);
    f->setPatched(true);
    doc->addFixture(f);
    return f;
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
    QSettings().remove(SETTINGS_PATCH_LIST_GROUPBY);
    QSettings().remove(SETTINGS_PATCH_LABEL_TEMPLATE);
    QSettings().remove(SETTINGS_PATCH_LABEL_TAPE);
    QSettings().remove(SETTINGS_PATCH_LABEL_CUTGAP);
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
    dlg.setGroupBy(PatchList::ByNone);
    QCOMPARE(dlg.windowTitle(), QString("Patch List"));
    QTreeWidget *t = dlg.tree();
    QCOMPARE(t->topLevelItemCount(), 3);
    QCOMPARE(t->topLevelItem(0)->text(1), QString("Front"));
    QCOMPARE(t->topLevelItem(0)->text(0), QString("1 · 1–4"));
    QCOMPARE(t->topLevelItem(1)->text(1), QString("Back"));
    QCOMPARE(t->topLevelItem(1)->text(0), QString("1 · 21–24"));
    QCOMPARE(t->topLevelItem(2)->text(1), QString("Spare"));
    QCOMPARE(t->topLevelItem(2)->text(0), QString("— not patched"));
}

void PatchListDialog_Test::theColumnsHaveNoChannelCount()
{
    PatchListDialog dlg(m_doc);
    QStringList cols;
    for (int c = 0; c < dlg.tree()->columnCount(); ++c)
    {
        if (!dlg.tree()->isColumnHidden(c))
            cols << dlg.tree()->headerItem()->text(c);
    }
    // No Ch: the mode says it. No Owner: everything is the tour's.
    QCOMPARE(cols, QStringList({ "Patch", "Fixture", "Make / model", "Mode", "Connection",
                                 "Goes to", "Mounted on" }));
    // A generic dimmer's mode says its count already.
    QCOMPARE(dlg.itemFor("Front")->text(PatchListDialog::ModeColumn), QString("4 Channel"));
    // An RGB panel's mode name doesn't say its count: it follows.
    rgbBar(m_doc, "Bar", 4, 0, 40);
    PatchListDialog again(m_doc);
    const QString mode = again.itemFor("Bar")->text(PatchListDialog::ModeColumn);
    QVERIFY2(mode.endsWith("(12 ch)"), qPrintable(mode));

    // Somebody else's fixture: the Owner column.
    m_doc->fixtures().first()->setOwner(RigOwner::House);
    PatchListDialog owners(m_doc);
    QVERIFY(!owners.tree()->isColumnHidden(PatchListDialog::OwnerColumn));
    QCOMPARE(owners.itemFor(m_doc->fixtures().first()->name())->text(PatchListDialog::OwnerColumn),
             QString("House"));
}

void PatchListDialog_Test::groupedByLocationByDefault()
{
    MonitorProperties *mp = m_doc->monitorProperties();
    Truss *foh = mp->addTruss();
    foh->setName("FOH");
    Fixture *spot = dimmer(m_doc, "Spot 3", 0, 100);
    FixtureRigProps rp = mp->fixtureRigProps(spot->id());
    rp.trussId = foh->id();
    mp->setFixtureRigProps(spot->id(), rp);
    Fixture *back = m_doc->fixtures().at(1);              // "Back"
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName("Backs");
    m_doc->addFixtureGroup(g);
    g->assignFixture(back->id());

    PatchListDialog dlg(m_doc);
    QCOMPARE(dlg.groupBy(), PatchList::ByLocation);
    QCOMPARE(dlg.findChild<QComboBox *>("patchListGroupBy")->currentText(), QString("Location"));
    QCOMPARE(headings(dlg.tree()), QStringList({ "Backs (1)", "Truss FOH (1)", "Not placed (2)" }));
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Back", "Spot 3", "Front", "Spare" }));
    QVERIFY(dlg.tree()->topLevelItem(0)->isFirstColumnSpanned());

    dlg.setGroupBy(PatchList::ByUniverse);
    QCOMPARE(headings(dlg.tree()), QStringList({ "Universe 1 (3)", "Not patched (in the inventory) (1)" }));
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Front", "Back", "Spot 3", "Spare" }));
    dlg.setGroupBy(PatchList::ByType);
    QCOMPARE(headings(dlg.tree()), QStringList({ "Dimmer (4)" }));
    dlg.setGroupBy(PatchList::ByNone);
    QVERIFY(headings(dlg.tree()).isEmpty());
    QCOMPARE(dlg.tree()->topLevelItemCount(), 4);
}

void PatchListDialog_Test::groupByIsRemembered()
{
    {
        PatchListDialog dlg(m_doc);
        dlg.setGroupBy(PatchList::ByType);
    }
    QCOMPARE(QSettings().value(SETTINGS_PATCH_LIST_GROUPBY).toInt(), int(PatchList::ByType));
    PatchListDialog again(m_doc);
    QCOMPARE(again.groupBy(), PatchList::ByType);
}

void PatchListDialog_Test::theFilterPicksTheRows()
{
    rgbBar(m_doc, "Bar", 8, 0, 40);
    Fixture *house = dimmer(m_doc, "House par", 0, 100);
    house->setOwner(RigOwner::House);

    PatchListDialog dlg(m_doc);
    dlg.setGroupBy(PatchList::ByNone);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Front", "Back", "Bar", "House par", "Spare" }));
    dlg.setKind(PatchList::Pixel);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Bar" }));
    QVERIFY2(dlg.findChild<QLabel *>("patchListSummary")->text().startsWith("1 of 5 fixtures shown"),
             qPrintable(dlg.findChild<QLabel *>("patchListSummary")->text()));
    dlg.setKind(PatchList::Discrete);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Front", "Back", "House par", "Spare" }));
    dlg.setOwner(RigOwner::House);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "House par" }));
    dlg.setOwner(RigOwner::Tour);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Front", "Back", "Spare" }));
    QCOMPARE(rowNames(dlg.shownRows()), QStringList({ "Front", "Back", "Spare" }));
    dlg.setKind(PatchList::Pixel);
    QCOMPARE(treeNames(dlg.tree()), QStringList({ "Bar" }));
    dlg.setOwner(RigOwner::House);
    QVERIFY(dlg.shownRows().isEmpty());
}

void PatchListDialog_Test::copyPutsTheCsvOnTheClipboard()
{
    PatchListDialog dlg(m_doc);
    QApplication::clipboard()->clear();
    dlg.slotCopy();
    QCOMPARE(QApplication::clipboard()->text(), dlg.shown().toCsv(dlg.groupBy()));
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
    QCOMPARE(QString::fromUtf8(f.readAll()), dlg.list().toCsv(PatchList::ByLocation));

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
    dlg.itemFor("Spare")->setSelected(true);
    dlg.itemFor("Front")->setSelected(true);
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
        ld.setSheetKey("avery5160");
        QCOMPARE(ld.findChild<QLabel *>("labelCount")->text(), QString("3 labels, on 1 sheet"));
    }

    // A selection: those, unless all is asked for.
    const QList<PatchList::Row> picked = { all.at(1) };
    PatchLabelsDialog ld(all, picked, "Labels", "labels.pdf");
    ld.setSheetKey("avery5160");
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
        QCOMPARE(ld.sheetKey(), QString("tape"));           // the default
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
    QCOMPARE(gone.sheetKey(), QString("tape"));
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

void PatchListDialog_Test::aNetworkFixturesIpIsOnMouseover()
{
    Fixture *f = dimmer(m_doc, "Lantern", 1, 0);
    f->setConnection(Fixture::Network);
    f->setTransport(Fixture::WledTransport);
    f->setIPAddress("172.18.2.51");
    Fixture *g = dimmer(m_doc, "Lantern B", 2, 0);
    g->setConnection(Fixture::Network);
    g->setTransport(Fixture::WledTransport);
    g->setIPAddress("172.18.2.52");
    m_doc->inputOutputMap()->setTargetAlias("172.18.2.51", "lantern-2");

    PatchListDialog dlg(m_doc);
    QTreeWidgetItem *lantern = dlg.itemFor("Lantern");
    QTreeWidgetItem *lanternB = dlg.itemFor("Lantern B");
    QVERIFY(lantern != nullptr && lanternB != nullptr);
    // The name it goes by, as the label has it; the IP on mouseover.
    QCOMPARE(lantern->text(PatchListDialog::ConnectionColumn), QString("WLED lantern-2"));
    QCOMPARE(lantern->toolTip(PatchListDialog::ConnectionColumn), QString("WLED to lantern-2 (172.18.2.51)"));
    // No name: the IP, there and on mouseover.
    QCOMPARE(lanternB->text(PatchListDialog::ConnectionColumn), QString("WLED 172.18.2.52"));
    QCOMPARE(lanternB->toolTip(PatchListDialog::ConnectionColumn), QString("WLED to 172.18.2.52"));
    // DMX: nothing to add.
    QVERIFY(dlg.itemFor("Front")->toolTip(PatchListDialog::ConnectionColumn).isEmpty());
}

static QAction *action(QMenu *menu, const QString &name)
{
    foreach (QAction *a, menu->actions())
    {
        if (a->objectName() == name)
            return a;
    }
    return nullptr;
}

void PatchListDialog_Test::theRightClickMenu()
{
    PatchListDialog dlg(m_doc);
    QCOMPARE(dlg.tree()->contextMenuPolicy(), Qt::CustomContextMenu);

    // Nothing selected: labels for the list shown, not for a selection.
    std::unique_ptr<QMenu> menu(dlg.contextMenu());
    QVERIFY(action(menu.get(), "patchListPrintLabels") != nullptr);
    QVERIFY(!action(menu.get(), "patchListPrintLabels")->isEnabled());
    QVERIFY(action(menu.get(), "patchListPrintAllLabels")->isEnabled());
    QVERIFY(action(menu.get(), "patchListCopyAction") != nullptr);

    dlg.itemFor("Back")->setSelected(true);
    menu.reset(dlg.contextMenu());
    QAction *one = action(menu.get(), "patchListPrintLabels");
    QCOMPARE(one->text(), QString("Print Label..."));
    QVERIFY(one->isEnabled());

    dlg.itemFor("Front")->setSelected(true);
    menu.reset(dlg.contextMenu());
    QCOMPARE(action(menu.get(), "patchListPrintLabels")->text(), QString("Print 2 Labels..."));

    // It opens the labels on those two, the selection picked.
    QStringList seen;
    bool selectedOnly = false;
    whenModal<PatchLabelsDialog>([&seen, &selectedOnly](PatchLabelsDialog *ld) {
        seen = rowNames(ld->rows());
        selectedOnly = ld->selectedOnly();
        ld->reject();
    });
    action(menu.get(), "patchListPrintLabels")->trigger();
    QCOMPARE(seen, QStringList({ "Front", "Back" }));
    QVERIFY(selectedOnly);
}

void PatchListDialog_Test::aSectionHeadingSelectsItsRows()
{
    PatchListDialog dlg(m_doc);
    dlg.setGroupBy(PatchList::ByUniverse);
    dlg.tree()->topLevelItem(0)->setSelected(true);               // Universe 1
    QCOMPARE(rowNames(dlg.selectedRows()), QStringList({ "Front", "Back" }));
    dlg.itemFor("Back")->setSelected(true);                       // once only
    QCOMPARE(rowNames(dlg.selectedRows()), QStringList({ "Front", "Back" }));
}

void PatchListDialog_Test::theExportsFollowWhatIsShown()
{
    rgbBar(m_doc, "Bar", 8, 0, 40);
    PatchListDialog dlg(m_doc);
    dlg.setGroupBy(PatchList::ByUniverse);
    dlg.setKind(PatchList::Discrete);

    QTemporaryDir dir;
    const QString csvPath = dir.filePath("patch.csv");
    QVERIFY(dlg.saveCsv(csvPath));
    QFile f(csvPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList lines = QString::fromUtf8(f.readAll()).trimmed().split('\n');
    QCOMPARE(lines.size(), 4);                         // the header and three discrete fixtures
    QVERIFY(lines.at(0).startsWith("Universe,Fixture,"));
    QVERIFY(lines.at(1).startsWith("Universe 1,Front,"));
    QVERIFY(lines.at(3).startsWith("\"Not patched (in the inventory)\",Spare,")
            || lines.at(3).startsWith("Not patched (in the inventory),Spare,"));
    QVERIFY(!lines.join('\n').contains(",Bar,"));

    const QString xlsxPath = dir.filePath("patch.xlsx");
    QString error;
    QVERIFY2(dlg.saveXlsx(xlsxPath, &error), qPrintable(error));
    QFile x(xlsxPath);
    QVERIFY(x.open(QIODevice::ReadOnly));
    const QByteArray xlsx = x.readAll();
    QVERIFY(xlsx.startsWith("PK"));
    QCOMPARE(xlsx.size(), dlg.shown().toXlsx(PatchList::ByUniverse).size());     // written whole
    QVERIFY(xlsx.contains("Front") && !xlsx.contains(">Bar<"));
    QVERIFY(!dlg.saveXlsx("/nonexistent-dir/patch.xlsx", &error));
    QVERIFY(dlg.findChild<QPushButton *>("patchListXlsx") != nullptr);
}

void PatchListDialog_Test::theLabelTemplateIsRememberedAndPreviewed()
{
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.labelTemplate(), QString("{name} D{address}"));
        QCOMPARE(ld.tapeWidth(), 24);
        QLabel *preview = ld.findChild<QLabel *>("labelPreview");
        QVERIFY(!preview->pixmap().isNull());
        QCOMPARE(preview->toolTip(), QString("Front D1"));
        QVERIFY(ld.findChild<QLabel *>("labelCount")->text().contains("of 24 mm tape"));

        ld.findChild<QLineEdit *>("labelTemplate")->setText("{name} @ {address}");
        QCOMPARE(preview->toolTip(), QString("Front @ 1"));
        ld.setTapeWidth(12);
        QCOMPARE(ld.sheet().page.height(), 12.0);
        ld.setCutGap(6);
        // Not tape: the width and gap say nothing.
        ld.setSheetKey("avery5160");
        QVERIFY(!ld.findChild<QComboBox *>("labelTape")->isEnabled());
        ld.setSheetKey("tape");
        QVERIFY(ld.findChild<QComboBox *>("labelTape")->isEnabled());
    }
    PatchLabelsDialog again(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
    QCOMPARE(again.labelTemplate(), QString("{name} @ {address}"));
    QCOMPARE(again.tapeWidth(), 12);
    QCOMPARE(again.cutGap(), 6.0);
    // Empty is the default.
    again.setLabelTemplate(QString());
    QCOMPARE(again.labelTemplate(), QString("{name} D{address}"));
}

void PatchListDialog_Test::labelsPrintToAPrinter()
{
    QTemporaryDir dir;
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
    ld.setSheetKey("ptp710bt");
    ld.setTapeWidth(24);

    /* A printer that writes a PDF: never the system's print dialog. */
    const QString path = dir.filePath("printed.pdf");
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    ld.setUpPrinter(&printer);
    QVERIFY(printer.fullPage());
    const QSizeF mm = printer.pageLayout().fullRect(QPageLayout::Millimeter).size();
    QVERIFY2(qAbs(mm.height() - 24) < 0.5 && qAbs(mm.width() - ld.sheet().page.width()) < 0.5,
             qPrintable(QString("%1 × %2").arg(mm.width()).arg(mm.height())));
    QString error;
    QVERIFY2(ld.print(&printer, &error), qPrintable(error));
    QCOMPARE(pdfPages(path), 3);                     // a label a page
}

void PatchListDialog_Test::theLabelsKnowTheShowUsesTwoUniverses()
{
    dimmer(m_doc, "Spot 3", 1, 100);
    PatchListDialog dlg(m_doc);
    dlg.itemFor("Spot 3")->setSelected(true);
    QString preview;
    whenModal<PatchLabelsDialog>([&preview](PatchLabelsDialog *ld) {
        ld->setLabelTemplate("{name} D{address}");
        preview = ld->findChild<QLabel *>("labelPreview")->toolTip();
        ld->reject();
    });
    dlg.slotPrintSelectedLabels();
    QCOMPARE(preview, QString("Spot 3 D2.101"));
}

/* What is missing stands out: the Patch cell of a fixture with no address
   is amber and italic, "— not patched", with a tooltip saying why; a
   patched row is left plain. Its label still prints "—". */
void PatchListDialog_Test::unpatchedRowsAreHighlighted()
{
    PatchListDialog dlg(m_doc);
    dlg.setGroupBy(PatchList::ByNone);
    QTreeWidgetItem *spare = dlg.itemFor("Spare");
    QTreeWidgetItem *front = dlg.itemFor("Front");
    QVERIFY(spare != nullptr && front != nullptr);

    const int col = PatchListDialog::PatchColumn;
    QCOMPARE(spare->text(col), QString("— not patched"));
    QVERIFY(spare->data(col, PatchListDialog::UnpatchedRole).toBool());
    QVERIFY(spare->font(col).italic());
    QCOMPARE(spare->background(col).color(), QColor(0xf5, 0xa6, 0x23));
    // Black on amber: legible whatever the theme's own text colour is.
    QCOMPARE(spare->foreground(col).color(), QColor(Qt::black));
    QVERIFY(spare->toolTip(col).contains("no DMX address"));
    // Only the Patch cell: the name reads as any other.
    QVERIFY(!spare->font(PatchListDialog::FixtureColumn).italic());

    QVERIFY(!front->data(col, PatchListDialog::UnpatchedRole).toBool());
    QVERIFY(!front->font(col).italic());
    QCOMPARE(front->background(col).style(), Qt::NoBrush);
    QVERIFY(front->toolTip(col).isEmpty());

    // The label: a dash, not the highlight's words.
    PatchList::Row row;
    foreach (const PatchList::Row &r, dlg.shownRows())
    {
        if (r.name == "Spare")
            row = r;
    }
    QCOMPARE(PatchLabels::render("{name} D{address}", row, false), QString("Spare D—"));
}

/* A Brother PT-P710BT installed here (by its queue's name, what the system
   calls it, or its model): with no stock chosen before, the dialog opens on
   its preset with that printer, and says so. */
void PatchListDialog_Test::anInstalledBrotherPicksItsStockAndPrinter()
{
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();

    const QList<PatchLabelsDialog::InstalledPrinter> byName = {
        { "HP_LaserJet", "HP LaserJet", "HP LaserJet Pro" },
        { "Brother_PT_P710BT", "Brother PT-P710BT", "Brother PT-P710BT" } };
    PatchLabelsDialog::setInstalledPrintersForTest(&byName);
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("ptp710bt"));
        QCOMPARE(ld.brotherFound(), QString("Brother_PT_P710BT"));
        QCOMPARE(ld.printerName(), QString("Brother_PT_P710BT"));
        QLabel *found = ld.findChild<QLabel *>("labelPrinterFound");
        QVERIFY(found != nullptr);
        QVERIFY(!found->isHidden());
        QCOMPARE(found->text(), QString("Brother PT-P710BT found: Brother PT-P710BT"));
        // Opening it chose nothing: no stock is remembered from it.
        QVERIFY(!QSettings().contains(SETTINGS_PATCH_LABEL_SHEET));
    }

    // The model alone says it (an older PT-P710, a queue named otherwise).
    const QList<PatchLabelsDialog::InstalledPrinter> byModel = {
        { "Label_Printer", "Label printer", "Brother PT-P710 CUPS" } };
    PatchLabelsDialog::setInstalledPrintersForTest(&byModel);
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("ptp710bt"));
        QCOMPARE(ld.printerName(), QString("Label_Printer"));
    }
    PatchLabelsDialog::setInstalledPrintersForTest(&kNoPrinters);
}

/* A stock chosen before is what is in the printer: it wins, the Brother is
   still mentioned, and the printer is the system's choice until its preset
   is picked. A remembered stock that has gone away doesn't count. */
void PatchListDialog_Test::aSavedStockWinsOverTheBrother()
{
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    const QList<PatchLabelsDialog::InstalledPrinter> brother = {
        { "Brother_PT_P710BT", "Brother PT-P710BT", "Brother PT-P710BT" } };
    PatchLabelsDialog::setInstalledPrintersForTest(&brother);

    QSettings().setValue(SETTINGS_PATCH_LABEL_SHEET, "roll62");
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("roll62"));
        QVERIFY(ld.printerName().isEmpty());
        QVERIFY(!ld.findChild<QLabel *>("labelPrinterFound")->isHidden());
        ld.setSheetKey("ptp710bt");
        QCOMPARE(ld.printerName(), QString("Brother_PT_P710BT"));
    }

    QSettings().setValue(SETTINGS_PATCH_LABEL_SHEET, "no such stock");
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("ptp710bt"));
    }
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
    PatchLabelsDialog::setInstalledPrintersForTest(&kNoPrinters);
}

/* No Brother: the default stock, nothing said, and its preset (picked by
   hand) leaves the printer to the system. */
void PatchListDialog_Test::noBrotherNoPrinterPicked()
{
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
    const QList<PatchList::Row> all = PatchList::build(m_doc).rows();
    const QList<PatchLabelsDialog::InstalledPrinter> other = {
        { "HP_LaserJet", "HP LaserJet", "HP LaserJet Pro" },
        { "Brother_HL_L2350DW", "Brother HL-L2350DW", "Brother HL-L2350DW" } };
    PatchLabelsDialog::setInstalledPrintersForTest(&other);
    {
        PatchLabelsDialog ld(all, QList<PatchList::Row>(), "Labels", "labels.pdf");
        QCOMPARE(ld.sheetKey(), QString("tape"));
        QVERIFY(ld.brotherFound().isEmpty());
        QVERIFY(ld.findChild<QLabel *>("labelPrinterFound")->isHidden());
        ld.setSheetKey("ptp710bt");
        QVERIFY(ld.printerName().isEmpty());
    }
    PatchLabelsDialog::setInstalledPrintersForTest(&kNoPrinters);
    QSettings().remove(SETTINGS_PATCH_LABEL_SHEET);
}

QTEST_MAIN(PatchListDialog_Test)
