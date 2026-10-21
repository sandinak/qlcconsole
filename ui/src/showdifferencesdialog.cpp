/*
  Q Light Controller Plus - qlcconsole
  showdifferencesdialog.cpp

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
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QLabel>

#include <QFile>

#include "showdifferencesdialog.h"
#include "fixturedefupdates.h"
#include "showembedding.h"
#include "doc.h"

namespace
{
    enum { KeyRole = Qt::UserRole, CompatibleRole };
    enum { ColWhat, ColNewer, ColSafe };
    int s_autoAnswer = -1;
}

void ShowDifferencesDialog::setAutoAnswer(int yes)
{
    s_autoAnswer = yes;
}

bool ShowDifferencesDialog::ask(const QString &title, const QString &text)
{
    if (s_autoAnswer >= 0)
        return s_autoAnswer > 0;
    return QMessageBox::warning(this, title, text, QMessageBox::Yes | QMessageBox::Cancel,
                                QMessageBox::Cancel) == QMessageBox::Yes;
}

void ShowDifferencesDialog::tell(const QString &title, const QString &text)
{
    if (s_autoAnswer < 0)
        QMessageBox::warning(this, title, text);
}

QString ShowDifferencesDialog::detailText() const
{
    return m_detail->text();
}

ShowDifferencesDialog::ShowDifferencesDialog(Doc *doc, QWidget *parent, bool everything)
    : QDialog(parent)
    , m_doc(doc)
    , m_everything(everything)
{
    setWindowTitle(everything ? tr("Show Definitions") : tr("Differences from this computer"));
    setProperty("helpTopic", QStringLiteral("definition-updates.md"));
    QVBoxLayout *lay = new QVBoxLayout(this);

    QLabel *intro = new QLabel(everything
        ? tr("The definitions and scripts this show carries, and how each compares with this "
             "computer's. Where they differ the show's is in use until you choose: use this "
             "computer's (the show carries it from the next save), keep the show's, or install "
             "the show's on this computer.")
        : tr("The show carries its own copies of these, and they are not "
             "the same as this computer's. The show's are in use; nothing "
             "changes until you choose."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(3);
    m_tree->setHeaderLabels(QStringList() << tr("What it is") << tr("Which looks newer")
                                          << tr("Safe to switch?"));
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->header()->setSectionResizeMode(ColWhat, QHeaderView::Stretch);
    m_tree->header()->setStretchLastSection(false);
    lay->addWidget(m_tree);

    // What switching would change, for the row picked.
    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    m_detail->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lay->addWidget(m_detail);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_use = box->addButton(tr("Use This Computer's"), QDialogButtonBox::ActionRole);
    m_use->setToolTip(tr("The show switches to this computer's copy now, and carries it "
                         "from the next save"));
    m_useAll = box->addButton(tr("Use This Computer's for All Compatible"), QDialogButtonBox::ActionRole);
    m_keep = box->addButton(tr("Keep the Show's"), QDialogButtonBox::ActionRole);
    m_keep->setToolTip(tr("Stop warning about it (remembered in the show)"));
    m_install = box->addButton(tr("Install the Show's"), QDialogButtonBox::ActionRole);
    m_install->setToolTip(tr("Write the show's copy over this computer's; the old file goes "
                             "to the Trash"));
    m_undo = box->addButton(tr("Undo"), QDialogButtonBox::ActionRole);
    m_undo->setObjectName(QStringLiteral("showDifferencesUndo"));
    m_undo->setToolTip(tr("Go back to the show's copies the last Use This Computer's switched from"));
    connect(m_undo, &QPushButton::clicked, this, &ShowDifferencesDialog::undoChoices);
    connect(m_use, &QPushButton::clicked, this, &ShowDifferencesDialog::useComputers);
    connect(m_useAll, &QPushButton::clicked, this, &ShowDifferencesDialog::useComputersForAllCompatible);
    connect(m_keep, &QPushButton::clicked, this, &ShowDifferencesDialog::keepShows);
    connect(m_install, &QPushButton::clicked, this, &ShowDifferencesDialog::installShows);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowDifferencesDialog::updateButtons);
    connect(m_doc->showEmbedding(), &ShowEmbedding::changed, this, &ShowDifferencesDialog::rebuild);

    rebuild();
    resize(760, 380);
}

void ShowDifferencesDialog::rebuild()
{
    const QStringList was = selectedKeys();
    ShowEmbedding *e = m_doc->showEmbedding();
    m_tree->blockSignals(true);
    m_tree->clear();
    const QList<ShowEmbedding::Item> rows = m_everything ? e->items() : e->differences();
    foreach (const ShowEmbedding::Item &it, rows)
    {
        QTreeWidgetItem *row = new QTreeWidgetItem(m_tree);
        row->setText(ColWhat, it.displayName());
        row->setData(ColWhat, KeyRole, it.key());
        row->setToolTip(ColWhat, QStringLiteral("%1\n%2").arg(it.showPath, it.computerPath));
        if (it.state != ShowEmbedding::Different || !it.usingShow)
        {
            QString state;
            if (it.state == ShowEmbedding::ShowOnly)
                state = it.usingShow ? tr("Only in the show") : tr("Only in the show (cannot be read)");
            else if (it.state == ShowEmbedding::Identical)
                state = tr("Same as this computer's");
            else
                state = tr("This computer's is in use");
            row->setText(ColNewer, state);
            row->setData(ColWhat, CompatibleRole, false);
            row->setSelected(was.contains(it.key()));
            continue;
        }
        const ShowEmbedding::Compatibility c = e->compatibility(it.key());
        QString newer = ShowEmbedding::newerText(e->newer(it));
        if (it.keptAgainst == it.computerSha && !it.keptAgainst.isEmpty())
            newer += tr(" (kept)");
        row->setText(ColNewer, newer);
        row->setText(ColSafe, c.compatible ? tr("Compatible") : tr("Not compatible"));
        row->setToolTip(ColSafe, c.compatible
                                     ? tr("Everything the show uses works the same with this computer's")
                                     : c.problems.join(QLatin1Char('\n')));
        if (!c.compatible)
            row->setForeground(ColSafe, QColor(0xe0, 0x40, 0x30));
        row->setData(ColWhat, CompatibleRole, c.compatible);
        row->setSelected(was.contains(it.key()));
    }
    m_tree->resizeColumnToContents(ColNewer);
    m_tree->resizeColumnToContents(ColSafe);
    m_tree->blockSignals(false);
    updateButtons();
    if (!m_everything && m_tree->topLevelItemCount() == 0 && isVisible() && !e->canUndo())
        accept();                   // nothing left to choose
}

void ShowDifferencesDialog::updateButtons()
{
    ShowEmbedding *e = m_doc->showEmbedding();
    const QStringList keys = selectedKeys();
    bool anyDifferent = false, anyShows = false;
    foreach (const QString &key, keys)
    {
        const ShowEmbedding::Item it = e->item(key);
        anyDifferent = anyDifferent || (it.state == ShowEmbedding::Different && it.usingShow);
        anyShows = anyShows || it.usingShow;
    }
    m_use->setEnabled(anyDifferent);
    m_keep->setEnabled(anyDifferent);
    m_install->setEnabled(anyShows);
    m_undo->setEnabled(e->canUndo());
    int compatible = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        if (m_tree->topLevelItem(i)->data(ColWhat, CompatibleRole).toBool())
            ++compatible;
    m_useAll->setEnabled(compatible > 0);

    if (keys.size() != 1)
    {
        m_detail->clear();
        return;
    }
    const ShowEmbedding::Item it = e->item(keys.first());
    QStringList lines;
    if (it.state == ShowEmbedding::Different && it.usingShow)
    {
        const ShowEmbedding::Compatibility c = e->compatibility(it.key());
        lines << (c.compatible
            ? tr("Switching is safe: every mode, channel and setting the show uses is the same "
                 "in this computer's copy.")
            : tr("Switching would change what the show does:\n%1").arg(c.problems.join(QLatin1Char('\n'))));
        if (e->newer(it) == ShowEmbedding::ShowNewer)
            lines << tr("The show's copy is newer: Install the Show's puts it on this computer.");
    }
    else if (it.state == ShowEmbedding::ShowOnly && it.usingShow)
        lines << tr("This computer has no copy: Install the Show's adds it to your folder.");
    /* A definition: what this computer's would change, channel by channel. */
    if (it.kind == ShowEmbedding::FixtureDefinition && it.state == ShowEmbedding::Different
        && !it.computerPath.isEmpty())
    {
        QFile f(it.computerPath);
        if (f.open(QIODevice::ReadOnly))
        {
            const FixtureDefUpdates::Info show = FixtureDefUpdates::infoFromBytes(it.bytes);
            const FixtureDefUpdates::Info here = FixtureDefUpdates::infoFromBytes(f.readAll());
            lines << tr("Show's: %1 · this computer's: %2")
                         .arg(show.creatorVersion.isEmpty() ? tr("unknown version") : show.creatorVersion,
                              here.creatorVersion.isEmpty() ? tr("unknown version") : here.creatorVersion);
            const QList<FixtureDefUpdates::Change> changes = FixtureDefUpdates::diff(show.bytes, here.bytes);
            if (changes.isEmpty())
                lines << tr("No difference in channels or modes.");
            else
            {
                lines << tr("This computer's copy, compared with the show's:");
                foreach (const FixtureDefUpdates::Change &ch, changes)
                    lines << QStringLiteral("  • %1").arg(ch.text());
            }
        }
    }
    m_detail->setText(lines.join(QLatin1Char('\n')));
}

QStringList ShowDifferencesDialog::selectedKeys() const
{
    QStringList keys;
    foreach (QTreeWidgetItem *it, m_tree->selectedItems())
        keys << it->data(ColWhat, KeyRole).toString();
    return keys;
}

QStringList ShowDifferencesDialog::keys() const
{
    QStringList keys;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        keys << m_tree->topLevelItem(i)->data(ColWhat, KeyRole).toString();
    return keys;
}

void ShowDifferencesDialog::select(const QStringList &keys)
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *it = m_tree->topLevelItem(i);
        it->setSelected(keys.contains(it->data(ColWhat, KeyRole).toString()));
    }
}

void ShowDifferencesDialog::useComputers()
{
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList failed;
    e->clearUndo();
    foreach (const QString &key, selectedKeys())
    {
        const ShowEmbedding::Item it = e->item(key);
        if (it.state != ShowEmbedding::Different || !it.usingShow)
            continue;
        const ShowEmbedding::Compatibility c = e->compatibility(key);
        if (!c.compatible
            && !ask(tr("Use This Computer's"),
                    tr("This computer's copy of %1 is not compatible with how the show uses it:\n\n%2"
                       "\n\nSwitch anyway? (Undo switches back.)").arg(it.displayName(), c.problems.join(QLatin1Char('\n')))))
            continue;
        QString error;
        if (!e->useComputers(key, &error))
            failed << error;
    }
    updateButtons();
    if (!failed.isEmpty())
        tell(tr("Use This Computer's"), failed.join(QStringLiteral("\n\n")));
}

void ShowDifferencesDialog::useComputersForAllCompatible()
{
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList failed;
    e->clearUndo();
    foreach (const QString &key, keys())
    {
        const ShowEmbedding::Item it = e->item(key);
        if (it.state != ShowEmbedding::Different || !it.usingShow || !e->compatibility(key).compatible)
            continue;
        QString error;
        if (!e->useComputers(key, &error))
            failed << error;
    }
    updateButtons();
    if (!failed.isEmpty())
        tell(tr("Use This Computer's"), failed.join(QStringLiteral("\n\n")));
}

void ShowDifferencesDialog::undoChoices()
{
    m_doc->showEmbedding()->undoChoices();
    updateButtons();
}

void ShowDifferencesDialog::keepShows()
{
    foreach (const QString &key, selectedKeys())
        m_doc->showEmbedding()->keepShows(key);
}

void ShowDifferencesDialog::installShows()
{
    const QStringList keys = selectedKeys();
    if (keys.isEmpty())
        return;
    ShowEmbedding *e = m_doc->showEmbedding();
    QStringList names;
    foreach (const QString &key, keys)
        if (e->item(key).usingShow)
            names << e->item(key).displayName();
    if (names.isEmpty()
        || !ask(tr("Install the Show's"),
                tr("Write the show's copy over this computer's for:\n\n%1\n\nThis computer's files go "
                   "to the Trash, and every show opened here uses the show's version from now on.")
                    .arg(names.join(QLatin1Char('\n')))))
        return;
    QStringList failed;
    foreach (const QString &key, keys)
    {
        if (!e->item(key).usingShow)
            continue;
        QString error;
        if (!e->installShows(key, &error))
            failed << error;
    }
    if (!failed.isEmpty())
        tell(tr("Install the Show's"), failed.join(QStringLiteral("\n\n")));
}
