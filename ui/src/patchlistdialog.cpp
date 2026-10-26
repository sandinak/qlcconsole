/*
  Q Light Controller Plus
  patchlistdialog.cpp

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

#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QColor>
#include <QFont>
#include <QTreeWidgetItemIterator>
#include <QApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QClipboard>
#include <QComboBox>
#include <QFileInfo>
#include <QSettings>
#include <QLabel>
#include <QMenu>
#include <QFile>

#include <algorithm>

#include "patchlabelsdialog.h"
#include "patchlistdialog.h"
#include "app.h"
#include "doc.h"

/* The show's file name, for the heading and the suggested file names. */
static QString showFileName()
{
    foreach (QWidget *w, qApp->topLevelWidgets())
        if (App *app = qobject_cast<App *>(w))
            return QFileInfo(app->fileName()).fileName();
    return QString();
}

PatchListDialog::PatchListDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_list(PatchList::build(doc))
{
    const QString show = showFileName();
    m_title = show.isEmpty() ? tr("Patch list") : tr("Patch list — %1").arg(show);
    setWindowTitle(tr("Patch List"));
    setProperty("helpTopic", QStringLiteral("fixture-manager.md#the-patch-list"));
    resize(980, 560);

    QVBoxLayout *root = new QVBoxLayout(this);

    /* Group by / Show / Owner: the list, the exports and the labels. */
    QHBoxLayout *bar = new QHBoxLayout;
    root->addLayout(bar);
    m_groupBy = new QComboBox(this);
    m_groupBy->setObjectName(QStringLiteral("patchListGroupBy"));
    m_groupBy->addItem(tr("Location"), int(PatchList::ByLocation));
    m_groupBy->addItem(tr("Universe"), int(PatchList::ByUniverse));
    m_groupBy->addItem(tr("Type"), int(PatchList::ByType));
    m_groupBy->addItem(tr("None"), int(PatchList::ByNone));
    m_groupBy->setToolTip(tr("Location: what it hangs on (truss, bar, tower, riser, deck), else its "
                             "fixture group, else \"Not placed\". In address order within each."));
    bar->addWidget(new QLabel(tr("Group by:"), this));
    bar->addWidget(m_groupBy);
    bar->addSpacing(12);

    m_kind = new QComboBox(this);
    m_kind->setObjectName(QStringLiteral("patchListShow"));
    m_kind->addItem(tr("All fixtures"), int(PatchList::AllKinds));
    m_kind->addItem(tr("Discrete fixtures"), int(PatchList::Discrete));
    m_kind->addItem(tr("RGB Matrix & pixel fixtures"), int(PatchList::Pixel));
    m_kind->setToolTip(tr("Pixel / matrix: an \"LED Bar (Pixels)\" fixture, or one whose mode has "
                          "two or more heads with their own colour channels"));
    bar->addWidget(new QLabel(tr("Show:"), this));
    bar->addWidget(m_kind);
    bar->addSpacing(12);

    m_owner = new QComboBox(this);
    m_owner->setObjectName(QStringLiteral("patchListOwner"));
    m_owner->addItem(tr("Anybody's"), int(RigOwner::Unset));
    foreach (RigOwner::Owner o, RigOwner::owners())
        m_owner->addItem(RigOwner::displayName(o), int(o));
    bar->addWidget(new QLabel(tr("Owner:"), this));
    bar->addWidget(m_owner);
    bar->addStretch(1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("patchListSummary"));
    root->addWidget(m_summary);

    const QStringList cols = { tr("Patch"), tr("Fixture"), tr("Make / model"), tr("Mode"),
                               tr("Connection"), tr("Goes to"), tr("Mounted on"), tr("Owner") };
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("patchListTree"));
    m_tree->setColumnCount(cols.size());
    m_tree->setHeaderLabels(cols);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setUniformRowHeights(true);
    m_tree->setWordWrap(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    // The owner only when somebody else's fixtures are in the show.
    m_tree->setColumnHidden(OwnerColumn, !m_list.hasOwners());
    root->addWidget(m_tree, 1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *copyBtn = btns->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copyBtn->setObjectName(QStringLiteral("patchListCopy"));
    copyBtn->setToolTip(tr("Copy the list shown to the clipboard, ready to paste into a spreadsheet"));
    QPushButton *csvBtn = btns->addButton(tr("Save CSV..."), QDialogButtonBox::ActionRole);
    csvBtn->setObjectName(QStringLiteral("patchListCsv"));
    csvBtn->setToolTip(tr("Save the list shown, every column, as a spreadsheet (CSV)"));
    QPushButton *xlsxBtn = btns->addButton(tr("Save XLSX..."), QDialogButtonBox::ActionRole);
    xlsxBtn->setObjectName(QStringLiteral("patchListXlsx"));
    xlsxBtn->setToolTip(tr("Save the list shown, every column, as an Excel workbook "
                           "(Numbers and Google Sheets open it too)"));
    QPushButton *pdfBtn = btns->addButton(tr("Save PDF..."), QDialogButtonBox::ActionRole);
    pdfBtn->setObjectName(QStringLiteral("patchListPdf"));
    pdfBtn->setToolTip(tr("Save a printable list of what is shown, a table per group"));
    QPushButton *labelsBtn = btns->addButton(tr("Print Labels..."), QDialogButtonBox::ActionRole);
    labelsBtn->setObjectName(QStringLiteral("patchListLabels"));
    labelsBtn->setToolTip(tr("Print a label per fixture shown (or the selected rows), or save them as a PDF"));
    root->addWidget(btns);

    const int by = QSettings().value(SETTINGS_PATCH_LIST_GROUPBY, int(PatchList::ByLocation)).toInt();
    m_groupBy->setCurrentIndex(qMax(0, m_groupBy->findData(by)));
    slotRefill();

    connect(m_groupBy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        QSettings().setValue(SETTINGS_PATCH_LIST_GROUPBY, int(groupBy()));
        slotRefill();
    });
    connect(m_kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PatchListDialog::slotRefill);
    connect(m_owner, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PatchListDialog::slotRefill);
    connect(m_tree, &QWidget::customContextMenuRequested, this, &PatchListDialog::slotContextMenu);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(copyBtn, &QPushButton::clicked, this, &PatchListDialog::slotCopy);
    connect(csvBtn, &QPushButton::clicked, this, &PatchListDialog::slotSaveCsv);
    connect(xlsxBtn, &QPushButton::clicked, this, &PatchListDialog::slotSaveXlsx);
    connect(pdfBtn, &QPushButton::clicked, this, &PatchListDialog::slotSavePdf);
    connect(labelsBtn, &QPushButton::clicked, this, &PatchListDialog::slotLabels);
}

PatchList::GroupBy PatchListDialog::groupBy() const
{
    return PatchList::GroupBy(m_groupBy->currentData().toInt());
}

/* What is missing, where the eye goes: the house amber chip (black on
   #f5a623, as the status bar's warnings), which reads on every theme.
   The labels still print "—" for its address. */
void PatchListDialog::markUnpatched(QTreeWidgetItem *item)
{
    item->setText(PatchColumn, tr("— not patched"));
    QFont italic = item->font(PatchColumn);
    italic.setItalic(true);
    item->setFont(PatchColumn, italic);
    item->setBackground(PatchColumn, QColor(0xf5, 0xa6, 0x23));
    item->setForeground(PatchColumn, QColor(Qt::black));
    item->setToolTip(PatchColumn, tr("Not patched: this fixture has no DMX address yet, so it sends "
                                     "nothing. Patch it in the Fixture Manager. Its label prints "
                                     "\"—\" for the address."));
    item->setData(PatchColumn, UnpatchedRole, true);
}

void PatchListDialog::setGroupBy(PatchList::GroupBy by)
{
    m_groupBy->setCurrentIndex(qMax(0, m_groupBy->findData(int(by))));
}

PatchList::Filter PatchListDialog::filter() const
{
    PatchList::Filter f;
    f.kind = PatchList::Kind(m_kind->currentData().toInt());
    const RigOwner::Owner o = RigOwner::Owner(m_owner->currentData().toInt());
    if (o != RigOwner::Unset)
        f.owners << o;
    return f;
}

void PatchListDialog::setKind(PatchList::Kind kind)
{
    m_kind->setCurrentIndex(qMax(0, m_kind->findData(int(kind))));
}

void PatchListDialog::setOwner(RigOwner::Owner owner)
{
    m_owner->setCurrentIndex(qMax(0, m_owner->findData(int(owner))));
}

PatchList PatchListDialog::shown() const
{
    return m_list.filtered(filter());
}

QList<PatchList::Row> PatchListDialog::shownRows() const
{
    return m_shown;
}

void PatchListDialog::slotRefill()
{
    const PatchList list = shown();
    const PatchList::GroupBy by = groupBy();
    m_tree->clear();
    m_shown.clear();
    m_tree->setRootIsDecorated(by != PatchList::ByNone);

    int patched = 0;
    foreach (const PatchList::Row &r, list.rows())
    {
        if (r.patched)
            patched++;
    }
    const int all = m_list.rows().size();
    const QString counts = list.rows().size() == all
        ? tr("%1 fixtures, %2 patched").arg(all).arg(patched)
        : tr("%1 of %2 fixtures shown, %3 patched").arg(list.rows().size()).arg(all).arg(patched);
    const QString order = by == PatchList::ByNone
        ? tr("in universe and address order, the inventory last")
        : tr("by %1, in address order within each").arg(PatchList::groupByName(by).toLower());
    m_summary->setText(QStringLiteral("%1, %2.").arg(counts, order));

    foreach (const PatchList::Section &s, list.sections(by))
    {
        QTreeWidgetItem *parent = nullptr;
        if (by != PatchList::ByNone)
        {
            parent = new QTreeWidgetItem(m_tree);
            parent->setText(0, tr("%1 (%2)").arg(s.title).arg(s.rows.size()));
            parent->setData(0, Qt::UserRole, -1);
            QFont bold = parent->font(0);
            bold.setBold(true);
            parent->setFont(0, bold);
            parent->setFirstColumnSpanned(true);
        }
        foreach (const PatchList::Row &r, s.rows)
        {
            /* A network fixture by the name its IP goes by, as its label
               has it; the IP itself on mouseover. */
            const QString ip = r.ip.isEmpty() ? tr("(no IP)") : r.ip;
            const QString conn = r.network
                ? tr("%1 %2").arg(r.transport, r.host.isEmpty() ? ip : r.host)
                : tr("DMX");
            const QStringList cells = { r.patchText(), r.name, r.makeModel(), r.modeText(),
                                        conn, r.output, r.mountedOn,
                                        RigOwner::displayName(r.ownerKind) };
            QTreeWidgetItem *item = parent != nullptr ? new QTreeWidgetItem(parent)
                                                      : new QTreeWidgetItem(m_tree);
            for (int c = 0; c < cells.size(); ++c)
                item->setText(c, cells.at(c));
            item->setData(0, Qt::UserRole, m_shown.size());
            if (r.network)
            {
                item->setToolTip(ConnectionColumn,
                    r.host.isEmpty() ? tr("%1 to %2").arg(r.transport, ip)
                                     : tr("%1 to %2 (%3)").arg(r.transport, r.host, ip));
            }
            if (!r.output.isEmpty())
                item->setToolTip(GoesToColumn, r.output);
            if (!r.patched && !r.clash)
                markUnpatched(item);
            m_shown << r;
        }
    }
    m_tree->expandAll();
    for (int c = 0; c < ColumnCount; ++c)
        m_tree->resizeColumnToContents(c);
}

QTreeWidgetItem *PatchListDialog::itemFor(const QString &name) const
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        if ((*it)->data(0, Qt::UserRole).toInt() >= 0 && (*it)->text(FixtureColumn) == name)
            return *it;
        ++it;
    }
    return nullptr;
}

bool PatchListDialog::saveCsv(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        if (error != NULL)
            *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << shown().toCsv(groupBy());
    return true;
}

bool PatchListDialog::saveXlsx(const QString &path, QString *error) const
{
    return shown().writeXlsx(path, groupBy(), error);
}

void PatchListDialog::slotCopy()
{
    QApplication::clipboard()->setText(shown().toCsv(groupBy()));
}

/* "show-patch-list.csv" beside nothing in particular: the dialog remembers
   the folder itself. */
static QString suggestedName(const QString &ext)
{
    const QString show = QFileInfo(showFileName()).completeBaseName();
    return show.isEmpty() ? QStringLiteral("patch-list.%1").arg(ext)
                          : QStringLiteral("%1-patch-list.%2").arg(show, ext);
}

void PatchListDialog::slotSaveCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Patch List"),
                                                      suggestedName(QStringLiteral("csv")),
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!saveCsv(path, &error))
        QMessageBox::warning(this, tr("Save Patch List"),
                             tr("Could not save %1:\n%2").arg(path, error));
}

void PatchListDialog::slotSaveXlsx()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Patch List"),
                                                      suggestedName(QStringLiteral("xlsx")),
                                                      tr("Excel workbooks (*.xlsx)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!saveXlsx(path, &error))
        QMessageBox::warning(this, tr("Save Patch List"),
                             tr("Could not save %1:\n%2").arg(path, error));
}

void PatchListDialog::slotSavePdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Patch List"),
                                                      suggestedName(QStringLiteral("pdf")),
                                                      tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!shown().writePdf(path, m_title, &error, groupBy()))
        QMessageBox::warning(this, tr("Save Patch List"),
                             tr("Could not save %1:\n%2").arg(path, error));
}

QList<PatchList::Row> PatchListDialog::selectedRows() const
{
    QList<int> picked;
    foreach (QTreeWidgetItem *item, m_tree->selectedItems())
    {
        const int at = item->data(0, Qt::UserRole).toInt();
        if (at >= 0)
        {
            picked << at;
            continue;
        }
        for (int i = 0; i < item->childCount(); ++i)
            picked << item->child(i)->data(0, Qt::UserRole).toInt();
    }
    std::sort(picked.begin(), picked.end());
    picked.erase(std::unique(picked.begin(), picked.end()), picked.end());
    QList<PatchList::Row> out;
    foreach (int r, picked)
    {
        if (r >= 0 && r < m_shown.size())
            out << m_shown.at(r);
    }
    return out;
}

QMenu *PatchListDialog::contextMenu()
{
    const int n = selectedRows().size();
    QMenu *menu = new QMenu(this);
    QAction *print = menu->addAction(n == 1 ? tr("Print Label...") : tr("Print %1 Labels...").arg(n),
                                     this, &PatchListDialog::slotPrintSelectedLabels);
    print->setObjectName(QStringLiteral("patchListPrintLabels"));
    print->setEnabled(n > 0);
    QAction *all = menu->addAction(tr("Print Labels for the List Shown..."), this,
                                   [this]() { openLabels(QList<PatchList::Row>()); });
    all->setObjectName(QStringLiteral("patchListPrintAllLabels"));
    all->setEnabled(!m_shown.isEmpty());
    menu->addSeparator();
    QAction *copy = menu->addAction(tr("Copy the List"), this, &PatchListDialog::slotCopy);
    copy->setObjectName(QStringLiteral("patchListCopyAction"));
    return menu;
}

void PatchListDialog::slotContextMenu(const QPoint &pos)
{
    QMenu *menu = contextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(m_tree->viewport()->mapToGlobal(pos));
}

void PatchListDialog::slotPrintSelectedLabels()
{
    const QList<PatchList::Row> picked = selectedRows();
    if (!picked.isEmpty())
        openLabels(picked);
}

void PatchListDialog::slotLabels()
{
    openLabels(selectedRows());
}

void PatchListDialog::openLabels(const QList<PatchList::Row> &selected)
{
    const QString show = showFileName();
    const QString title = show.isEmpty() ? tr("Fixture labels") : tr("Fixture labels — %1").arg(show);
    PatchLabelsDialog dlg(m_shown, selected, title,
                          suggestedName(QStringLiteral("pdf")).replace(QStringLiteral("patch-list"),
                                                                       QStringLiteral("labels")),
                          this);
    dlg.setMultiUniverse(m_list.multiUniverse());
    dlg.exec();
}
