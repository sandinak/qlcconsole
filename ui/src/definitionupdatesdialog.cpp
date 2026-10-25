/*
  Q Light Controller Plus - qlcconsole
  definitionupdatesdialog.cpp

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
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QFileInfo>
#include <QLabel>

#include "definitionupdatesdialog.h"
#include "bulkresultdialog.h"
#include "fixture.h"
#include "apputil.h"
#include "doc.h"

namespace
{
    enum { IndexRole = Qt::UserRole };
    enum { ColWhat, ColFile, ColMine, ColCurrent, ColVerdict };
    int s_autoAnswer = -1;
}

DefinitionUpdatesDialog::DefinitionUpdatesDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Fixture Definition Updates"));
    setProperty("helpTopic", QStringLiteral("definition-updates.md"));
    QVBoxLayout *lay = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("Definitions in your fixture folder that are older than the "
                                  "current version of the same fixture -- the one that comes with "
                                  "qlcconsole, or a newer copy of your own. Nothing changes until "
                                  "you choose, and every update keeps a copy of the old file."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setObjectName(QStringLiteral("definitionUpdatesSummary"));
    lay->addWidget(m_summary);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("definitionUpdatesTree"));
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels(QStringList() << tr("Fixture") << tr("File") << tr("Yours")
                                          << tr("Current") << tr("Why"));
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->header()->setSectionResizeMode(ColWhat, QHeaderView::Stretch);
    m_tree->header()->setStretchLastSection(false);
    lay->addWidget(m_tree, 3);

    // What an update would change, for the row picked.
    m_detail = new QPlainTextEdit(this);
    m_detail->setObjectName(QStringLiteral("definitionUpdatesDetail"));
    m_detail->setReadOnly(true);
    lay->addWidget(m_detail, 2);

    QHBoxLayout *opts = new QHBoxLayout();
    m_customised = new QCheckBox(tr("Also list customised copies"), this);
    m_customised->setToolTip(tr("Copies that differ from the current version but are not older "
                                "-- changed on purpose, or it can't be told. Never called out of date."));
    opts->addWidget(m_customised);
    opts->addStretch();
    opts->addWidget(new QLabel(tr("Linked files:"), this));
    m_linkMode = new QComboBox(this);
    m_linkMode->setObjectName(QStringLiteral("definitionUpdatesLinkMode"));
    m_linkMode->addItem(tr("Update the original the link points to"), int(FixtureDefUpdates::UpdateLinkedOriginal));
    m_linkMode->addItem(tr("Replace the link with a file of its own"), int(FixtureDefUpdates::ReplaceLink));
    m_linkMode->setToolTip(tr("Some definitions here are links into the old QLC+ folder, which is "
                              "synced. Updating the original changes it on every computer that "
                              "syncs it; replacing the link changes only this one."));
    opts->addWidget(m_linkMode);
    lay->addLayout(opts);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_update = box->addButton(tr("Update from Current"), QDialogButtonBox::ActionRole);
    m_update->setToolTip(tr("Write the current version over the selected files (the old ones are "
                            "kept in the .backup folder)"));
    m_updateAll = box->addButton(tr("Update All"), QDialogButtonBox::ActionRole);
    m_updateAll->setToolTip(tr("Every out-of-date file listed (not the customised ones)"));
    m_keep = box->addButton(tr("Keep Mine"), QDialogButtonBox::ActionRole);
    m_keep->setToolTip(tr("Stop offering an update for this file while it stays as it is"));
    m_ignore = box->addButton(tr("Ignore This Version"), QDialogButtonBox::ActionRole);
    m_ignore->setToolTip(tr("Don't offer this current version again; a newer one will be offered"));
    m_undo = box->addButton(tr("Undo Last Update"), QDialogButtonBox::ActionRole);
    m_undo->setToolTip(tr("Put back the files the last update replaced"));
    connect(m_update, &QPushButton::clicked, this, &DefinitionUpdatesDialog::updateSelected);
    connect(m_updateAll, &QPushButton::clicked, this, &DefinitionUpdatesDialog::updateAll);
    connect(m_keep, &QPushButton::clicked, this, &DefinitionUpdatesDialog::keepMine);
    connect(m_ignore, &QPushButton::clicked, this, &DefinitionUpdatesDialog::ignoreVersion);
    connect(m_undo, &QPushButton::clicked, this, &DefinitionUpdatesDialog::undoLast);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &DefinitionUpdatesDialog::updateButtons);
    connect(m_customised, &QCheckBox::toggled, this, &DefinitionUpdatesDialog::rescan);

    rescan();
    resize(900, 560);
}

void DefinitionUpdatesDialog::setAutoAnswer(int yes)
{
    s_autoAnswer = yes;
}

bool DefinitionUpdatesDialog::ask(const QString &title, const QString &text)
{
    if (s_autoAnswer >= 0)
        return s_autoAnswer > 0;
    return QMessageBox::warning(this, title, text, QMessageBox::Yes | QMessageBox::Cancel,
                                QMessageBox::Cancel) == QMessageBox::Yes;
}

bool DefinitionUpdatesDialog::askList(const QString &title, const QString &intro,
                                      const QString &heading, const QStringList &items,
                                      const QString &question)
{
    if (s_autoAnswer >= 0)
        return s_autoAnswer > 0;
    return BulkResultDialog::confirm(this, title, intro, heading, items, question);
}

void DefinitionUpdatesDialog::tell(const QString &title, const QString &summary,
                                   const QStringList &done, const QString &doneHeading,
                                   const QStringList &failed, const QStringList &notes,
                                   const QHash<QString, QStringList> &details)
{
    m_report = done;
    if (!failed.isEmpty())
        m_report << tr("Not updated:") << failed;
    m_report << notes;
    if (s_autoAnswer >= 0 || (done.isEmpty() && failed.isEmpty()))
        return;
    /* A proper window, not an alert: two hundred lines grew one off the
       screen with nothing to scroll and no Close. */
    QList<BulkResultDialog::Entry> doneEntries;
    foreach (const QString &line, done)
    {
        BulkResultDialog::Entry e;
        e.text = line;
        for (QHash<QString, QStringList>::const_iterator it = details.constBegin();
             it != details.constEnd(); ++it)
        {
            if (line.startsWith(it.key() + QLatin1Char(':')))
                e.details = it.value();
        }
        doneEntries << e;
    }
    QList<QPair<QString, QList<BulkResultDialog::Entry> > > sections;
    sections << qMakePair(doneHeading, doneEntries)
             << qMakePair(tr("Errors"), BulkResultDialog::entries(failed));
    BulkResultDialog::tell(this, title, summary, sections, notes);
}

void DefinitionUpdatesDialog::rescan()
{
    const QStringList was = [this]() {
        QStringList f;
        foreach (const FixtureDefUpdates::Candidate &c, selectedCandidates())
            f << c.mine.path;
        return f;
    }();
    const FixtureDefUpdates::Scan scan = FixtureDefUpdates::scan(m_doc, m_customised->isChecked());
    m_candidates = scan.candidates;

    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem *used = new QTreeWidgetItem(m_tree, QStringList() << tr("Used by this show"));
    QTreeWidgetItem *unused = new QTreeWidgetItem(m_tree, QStringList() << tr("Not used by this show"));
    for (QTreeWidgetItem *g : { used, unused })
    {
        g->setFlags(Qt::ItemIsEnabled);
        g->setFirstColumnSpanned(true);
        QFont f = g->font(0);
        f.setBold(true);
        g->setFont(0, f);
    }
    int outOfDate = 0;
    for (int i = 0; i < m_candidates.size(); ++i)
    {
        const FixtureDefUpdates::Candidate &c = m_candidates.at(i);
        if (c.verdict == FixtureDefUpdates::OutOfDate)
            ++outOfDate;
        QTreeWidgetItem *row = new QTreeWidgetItem(c.usedByShow ? used : unused);
        row->setText(ColWhat, c.mine.manufacturer + QLatin1Char(' ') + c.mine.model);
        QString file = QFileInfo(c.mine.path).fileName();
        if (c.mine.inLegacyFolder)
            file += tr(" (in the QLC+ folder, not linked)");
        else if (c.mine.isLink)
            file += c.mine.linkIntoLegacy ? tr(" (link into the QLC+ folder)") : tr(" (link)");
        row->setText(ColFile, file);
        row->setToolTip(ColFile, c.mine.isLink ? QStringLiteral("%1\n→ %2").arg(c.mine.path, c.mine.realPath)
                                               : c.mine.path);
        row->setText(ColMine, c.mine.versionText());
        row->setText(ColCurrent, c.current.versionText());
        row->setToolTip(ColCurrent, c.current.path);
        bool layout = false;
        foreach (const FixtureDefUpdates::Change &ch, c.changes)
            layout = layout || ch.layout;
        QString why = FixtureDefUpdates::verdictText(c.verdict);
        if (layout)
            why += tr("; a mode's channels change");
        row->setText(ColVerdict, why);
        if (c.verdict == FixtureDefUpdates::Customised)
            row->setForeground(ColVerdict, palette().color(QPalette::Disabled, QPalette::Text));
        row->setData(ColWhat, IndexRole, i);
        row->setSelected(was.contains(c.mine.path));
    }
    used->setText(ColWhat, tr("Used by this show (%1)").arg(used->childCount()));
    unused->setText(ColWhat, tr("Not used by this show (%1)").arg(unused->childCount()));
    used->setHidden(used->childCount() == 0);
    unused->setHidden(unused->childCount() == 0);
    m_tree->expandAll();
    for (int c = ColFile; c <= ColVerdict; ++c)
        m_tree->resizeColumnToContents(c);
    m_tree->blockSignals(false);

    QStringList summary;
    summary << (outOfDate == 0 ? tr("All your definitions are up to date.")
                               : tr("%n definition(s) are older than the current version.", "", outOfDate));
    if (!scan.unreachable.isEmpty())
        summary << tr("%n link(s) in your fixture folder lead to files that are not there, so they "
                      "were not checked: %1", "", scan.unreachable.size())
                       .arg(scan.unreachable.mid(0, 6).join(QStringLiteral(", "))
                            + (scan.unreachable.size() > 6 ? QStringLiteral(", …") : QString()));
    m_summary->setText(summary.join(QLatin1Char(' ')));
    updateButtons();
}

QList<FixtureDefUpdates::Candidate> DefinitionUpdatesDialog::selectedCandidates() const
{
    QList<FixtureDefUpdates::Candidate> out;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
    {
        const QVariant v = it->data(ColWhat, IndexRole);
        if (v.isValid() && v.toInt() >= 0 && v.toInt() < m_candidates.size())
            out << m_candidates.at(v.toInt());
    }
    return out;
}

void DefinitionUpdatesDialog::updateButtons()
{
    const QList<FixtureDefUpdates::Candidate> sel = selectedCandidates();
    m_update->setEnabled(!sel.isEmpty());
    m_keep->setEnabled(!sel.isEmpty());
    m_ignore->setEnabled(!sel.isEmpty());
    int outOfDate = 0;
    for (const FixtureDefUpdates::Candidate &c : m_candidates)
        if (c.verdict == FixtureDefUpdates::OutOfDate)
            ++outOfDate;
    m_updateAll->setEnabled(outOfDate > 0);
    m_undo->setEnabled(!FixtureDefUpdates::lastUndoable().isEmpty());

    if (sel.size() != 1)
    {
        m_detail->setPlainText(sel.isEmpty() ? QString() : tr("%n selected", "", sel.size()));
        return;
    }
    const FixtureDefUpdates::Candidate &c = sel.first();
    QStringList t;
    t << tr("Yours: %1 (%2)").arg(c.mine.path, c.mine.versionText());
    if (c.mine.inLegacyFolder)
        t << tr("  in the old QLC+ folder, which syncs to other computers; no link here leads to it");
    else if (c.mine.isLink)
        t << (c.mine.linkIntoLegacy
              ? tr("  a link to %1 -- in the QLC+ folder, which syncs to other computers").arg(c.mine.realPath)
              : tr("  a link to %1").arg(c.mine.realPath));
    t << tr("Current: %1 (%2)").arg(c.current.path, c.current.versionText());
    t << FixtureDefUpdates::verdictText(c.verdict);
    t << QString();
    if (c.changes.isEmpty())
        t << tr("No difference in channels or modes (only in the details of the file).");
    else
    {
        t << tr("Updating changes:");
        foreach (const FixtureDefUpdates::Change &ch, c.changes)
            t << QStringLiteral("  • %1%2").arg(ch.text(), ch.layout ? tr("  [layout]") : QString());
    }
    if (c.usedByShow)
    {
        const FixtureDefUpdates::Impact im = FixtureDefUpdates::impactOf(m_doc, c);
        t << QString();
        if (im.clean())
            t << tr("The show's fixtures keep their modes and channels: updating is safe.");
        if (!im.refused.isEmpty())
            t << tr("Can't update while the show uses it this way:") << im.refused;
        if (!im.confirm.isEmpty())
            t << tr("Updating needs confirming -- a channel would do something else:") << im.confirm;
    }
    m_detail->setPlainText(t.join(QLatin1Char('\n')));
}

QString DefinitionUpdatesDialog::detailText() const
{
    return m_detail->toPlainText();
}

QStringList DefinitionUpdatesDialog::files() const
{
    QStringList out;
    for (const FixtureDefUpdates::Candidate &c : m_candidates)
        out << c.mine.path;
    return out;
}

void DefinitionUpdatesDialog::select(const QStringList &files)
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        const QVariant v = (*it)->data(ColWhat, IndexRole);
        if (v.isValid())
            (*it)->setSelected(files.contains(m_candidates.at(v.toInt()).mine.path));
        ++it;
    }
}

void DefinitionUpdatesDialog::runUpdate(const QList<FixtureDefUpdates::Candidate> &list)
{
    if (list.isEmpty())
        return;
    QStringList confirm, names;
    for (const FixtureDefUpdates::Candidate &c : list)
    {
        names << QStringLiteral("%1 (%2)").arg(c.mine.manufacturer + QLatin1Char(' ') + c.mine.model,
                                               QFileInfo(c.mine.path).fileName());
        if (c.usedByShow)
            confirm << FixtureDefUpdates::impactOf(m_doc, c).confirm;
    }
    bool confirmed = false;
    if (!confirm.isEmpty())
    {
        if (!askList(tr("Update from Current"),
                     tr("In the open show, these channels would do something else after the update."),
                     tr("Channels"), confirm,
                     tr("The fixtures keep their addresses and modes; the functions listed keep their "
                        "values, now on the new channels. Update anyway?")))
            return;
        confirmed = true;
    }
    else if (!askList(tr("Update from Current"), tr("Write the current version over these files."),
                      tr("Definitions"), names,
                      tr("The old files are kept, and Undo Last Update puts them back. Update them?")))
        return;

    /* What each one changes, and which fixtures of the show use it, for
       the report: asked before the update, which changes both. */
    QHash<QString, QStringList> details;
    for (const FixtureDefUpdates::Candidate &c : list)
    {
        QStringList d;
        if (c.usedByShow && m_doc != nullptr)
        {
            QStringList fixtures;
            foreach (Fixture *f, FixtureDefUpdates::impactOf(m_doc, c).fixtures)
                if (f != nullptr)
                    fixtures << f->name();
            if (!fixtures.isEmpty())
                d << tr("Fixtures: %1").arg(fixtures.join(QStringLiteral(", ")));
        }
        foreach (const FixtureDefUpdates::Change &ch, c.changes)
            d << ch.text();
        details.insert(c.mine.manufacturer + QLatin1Char(' ') + c.mine.model, d);
    }
    const int before = m_candidates.count();

    const FixtureDefUpdates::LinkMode mode = FixtureDefUpdates::LinkMode(m_linkMode->currentData().toInt());
    const FixtureDefUpdates::Result r = FixtureDefUpdates::update(m_doc, list, mode, confirmed);
    QStringList notes;
    if (!r.backupFolder.isEmpty() && !r.updated.isEmpty())
        notes << tr("The old files are in %1.").arg(r.backupFolder);
    bool showUses = false;
    for (const FixtureDefUpdates::Candidate &c : list)
        showUses = showUses || c.usedByShow;
    if (!r.updated.isEmpty() && showUses && m_doc != nullptr)
        m_doc->setModified();     // a .qlcc carries the new version from the next save
    rescan();
    AppUtil::updateDefinitionUpdatesStatus(m_doc);
    const QString summary = tr("Updated %1, unchanged %2, %3")
        .arg(r.updated.count()).arg(qMax(0, before - r.updated.count() - r.failed.count()))
        .arg(r.failed.count() == 1 ? tr("1 error") : tr("%1 errors").arg(r.failed.count()));
    tell(tr("Update from Current"), summary, r.updated, tr("Updated"), r.failed, notes, details);
}

void DefinitionUpdatesDialog::updateSelected()
{
    runUpdate(selectedCandidates());
}

void DefinitionUpdatesDialog::updateAll()
{
    QList<FixtureDefUpdates::Candidate> list;
    for (const FixtureDefUpdates::Candidate &c : m_candidates)
        if (c.verdict == FixtureDefUpdates::OutOfDate)
            list << c;
    runUpdate(list);
}

void DefinitionUpdatesDialog::keepMine()
{
    foreach (const FixtureDefUpdates::Candidate &c, selectedCandidates())
        FixtureDefUpdates::keepMine(c);
    rescan();
    AppUtil::updateDefinitionUpdatesStatus(m_doc);
}

void DefinitionUpdatesDialog::ignoreVersion()
{
    foreach (const FixtureDefUpdates::Candidate &c, selectedCandidates())
        FixtureDefUpdates::ignoreVersion(c);
    rescan();
    AppUtil::updateDefinitionUpdatesStatus(m_doc);
}

void DefinitionUpdatesDialog::undoLast()
{
    const QString folder = FixtureDefUpdates::lastUndoable();
    if (folder.isEmpty())
        return;
    if (!ask(tr("Undo Last Update"), tr("Put back the files the last update replaced (kept in %1)?").arg(folder)))
        return;
    const FixtureDefUpdates::Result r = FixtureDefUpdates::undo(m_doc, folder);
    rescan();
    AppUtil::updateDefinitionUpdatesStatus(m_doc);
    const QString summary = tr("Put back %1, %2").arg(r.updated.count())
        .arg(r.failed.count() == 1 ? tr("1 error") : tr("%1 errors").arg(r.failed.count()));
    tell(tr("Undo Last Update"), summary, r.updated, tr("Put back"), r.failed, QStringList());
}
