/*
  Q Light Controller Plus
  showconverter.cpp

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
#include <QTextBrowser>
#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QTabWidget>
#include <QLabel>

#include "showconverter.h"
#include "virtualconsole.h"
#include "importsheet.h"
#include "vccuelist.h"
#include "vcframe.h"
#include "collection.h"
#include "function.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int kChaserRole = Qt::UserRole + 1;   //!< chaser, list, collection or scene id
const int kKindRole = Qt::UserRole + 2;
const int kPlanRole = Qt::UserRole + 3;     //!< a cue list's plan, shown while ticked

/** A row with no tick box (QTreeWidgetItem has one by default). */
void noTick(QTreeWidgetItem *item)
{
    item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
}

enum RowKind
{
    ChaserRow = 0,
    ConvertedRow,
    CollectionRow,
    SceneRow
};
}

bool ShowConverter::playsInVcCueList(quint32 chaserId)
{
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc == nullptr || vc->contents() == nullptr)
        return false;
    for (VCCueList *c : vc->contents()->findChildren<VCCueList *>())
        if (c->chaserID() == chaserId)
            return true;
    return false;
}

QStringList ShowConverter::cueListChasers(Doc *doc)
{
    QStringList out;
    ShowTreeController *st = doc->showTree();
    for (quint32 id : st->convertibleChasers())
        if (st->classifyChaser(id, playsInVcCueList(id)).kind == ShowTreeController::ChaserKind::CueList)
            out << doc->function(id)->name();
    return out;
}

ShowConverter::ShowConverter(Doc *doc, QWidget *parent, Mode mode)
    : QDialog(parent)
    , m_doc(doc)
    , m_kind(mode)
{
    const bool upgrade = mode == Upgrade;
    setObjectName(upgrade ? QStringLiteral("stShowUpgrade") : QStringLiteral("stShowConverter"));
    setWindowTitle(upgrade ? tr("Upgrade this show") : tr("Convert an old show"));
    setProperty("helpTopic", upgrade ? QStringLiteral("show-tree.md#upgrade-this-show")
                                     : QStringLiteral("show-tree.md#an-old-show-convert-it"));
    resize(upgrade ? 900 : 820, 640);

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(upgrade
        ? tr("Every chaser and collection of the show, with what would become of it. A chaser that waits for GO "
             "is a cue list: tick it to make it a Show Tree cue list, checked channel for channel. One that loops "
             "on its own is an effect and is kept as a function. Collections are read into the cues of the lists "
             "that play them, or left alone. Raw scenes into palettes, on the second page, is optional. Nothing "
             "changes until you press Upgrade; the whole upgrade is one undo step, and Restore original puts a "
             "chaser back. The show plays the old way at every point.")
        : tr("Each chaser, by how it runs. One that waits for GO (notes, cue numbers, a VC Cue "
             "List, show control) is a cue list: tick it to make it a Show Tree cue list. One "
             "that loops on its own — a strobe, a chase, a sweep — is an effect and stays a "
             "function: it plays as a ▣ layer wherever you drop it. Nothing changes until you "
             "press Convert, and each chaser is one undo step."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = new QTreeWidget(this);
    m_table->setObjectName(QStringLiteral("stConverterTable"));
    m_table->setRootIsDecorated(false);
    m_table->setHeaderLabels({ upgrade ? tr("Chaser / collection") : tr("Chaser"), tr("Used as"), tr("Plan"),
                               tr("Plays the same") });
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    if (upgrade)
    {
        m_pages = new QTabWidget(this);
        m_pages->setObjectName(QStringLiteral("stUpgradePages"));
        m_pages->addTab(m_table, tr("1. Chasers and collections"));
        m_captureTable = new QTreeWidget(this);
        m_captureTable->setObjectName(QStringLiteral("stUpgradeCapture"));
        m_captureTable->setRootIsDecorated(false);
        m_captureTable->setHeaderLabels({ tr("Raw scene"), tr("Played in"), tr("Into palettes"), tr("Stays raw") });
        m_captureTable->header()->setStretchLastSection(false);
        m_captureTable->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_pages->addTab(m_captureTable, tr("2. Raw scenes into palettes"));
        root->addWidget(m_pages, 2);
        connect(m_captureTable, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
            if (item != nullptr)
                showItem(item);
        });
        connect(m_captureTable, &QTreeWidget::itemChanged, this, [this]() { updateButtons(); });
        connect(m_pages, &QTabWidget::currentChanged, this, [this](int page) {
            QTreeWidget *t = page == 0 ? m_table : m_captureTable;
            if (t->currentItem() != nullptr)
                showItem(t->currentItem());
        });
    }
    else
    {
        root->addWidget(m_table, 1);
    }

    m_details = new QTextBrowser(this);
    m_details->setObjectName(QStringLiteral("stConverterDetails"));
    m_details->setOpenLinks(false);
    root->addWidget(m_details, 3);

    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(new QLabel(tr("Convert by:"), this));
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("stConverterMode"));
    m_mode->addItem(tr("Import in place: the list takes over (originals filed, VC and show control re-pointed)"), true);
    m_mode->addItem(tr("Import beside the chaser: keep both"), false);
    m_mode->setToolTip(tr("In place: the cue list is linked to its chaser; the chaser and what only it plays go to "
                          "Imported/<chaser> (originals); VC buttons, Cue Lists, sliders, Show timelines and show "
                          "control fire the list. Restore original (on the list) undoes it."));
    row->addWidget(m_mode, 1);
    root->addLayout(row);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    if (upgrade)
    {
        m_restoreButton = buttons->addButton(tr("Restore original…"), QDialogButtonBox::ActionRole);
        m_restoreButton->setObjectName(QStringLiteral("stUpgradeRestore"));
        m_restoreButton->setToolTip(tr("Put the chaser back as it was before it was imported in place: the list "
                                       "leaves the tree, what fired it fires the chaser again. One undo step."));
        connect(m_restoreButton, &QPushButton::clicked, this, [this]() {
            const QTreeWidgetItem *item = m_table->currentItem();
            if (item != nullptr && item->data(0, kKindRole).toInt() == ConvertedRow)
                restore(item->data(0, kChaserRole).toUInt());
        });
    }
    m_convertButton = buttons->addButton(upgrade ? tr("Upgrade") : tr("Convert"), QDialogButtonBox::AcceptRole);
    m_convertButton->setObjectName(QStringLiteral("stConverterConvert"));
    if (upgrade)
        m_convertButton->setToolTip(tr("Convert the ticked chasers and capture the ticked scenes: one undo step"));
    QPushButton *close = buttons->addButton(QDialogButtonBox::Close);
    close->setObjectName(QStringLiteral("stConverterClose"));
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_convertButton, &QPushButton::clicked, this, [this]() { convert(); });
    root->addWidget(buttons);

    connect(m_table, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr)
            showItem(item);
        updateButtons();
    });
    connect(m_table, &QTreeWidget::itemChanged, this, [this]() {
        updatePlanTexts();
        updateButtons();
    });

    build();
}

void ShowConverter::build()
{
    ShowTreeController *st = m_doc->showTree();
    m_rows.clear();
    QSignalBlocker block(m_table);
    m_table->clear();
    for (quint32 id : st->convertibleChasers())
    {
        Row r;
        r.chaser = id;
        r.cls = st->classifyChaser(id, playsInVcCueList(id));
        if (r.cls.kind == ShowTreeController::ChaserKind::CueList)
            r.plan = st->planImport(id, &r.list);
        m_rows << r;

        const Function *f = m_doc->function(id);
        QTreeWidgetItem *item = new QTreeWidgetItem(m_table);
        item->setData(0, kChaserRole, id);
        item->setData(0, kKindRole, ChaserRow);
        item->setText(0, f->name());
        const bool cueList = r.cls.kind == ShowTreeController::ChaserKind::CueList;
        item->setText(1, (cueList ? tr("Cue list") : tr("Effect")) + QStringLiteral(" — ")
                         + r.cls.why.join(QStringLiteral(", ")));
        item->setToolTip(1, r.cls.why.join(QStringLiteral("\n")));
        if (cueList)
        {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
            QStringList plan;
            plan << tr("%n cue(s)", "", r.plan.cues) << tr("%n layer(s)", "", r.plan.layers);
            if (r.plan.opaqueLayers > 0)
                plan << tr("%n ▣", "", r.plan.opaqueLayers);
            if (r.plan.includes > 0)
                plan << tr("%n include(s)", "", r.plan.includes);
            if (r.plan.dropped + r.plan.flattened > 0)
                plan << tr("%n nested read in", "", r.plan.dropped + r.plan.flattened);
            item->setText(2, plan.join(QStringLiteral(" · ")));
            item->setData(0, kPlanRole, item->text(2));
            item->setText(3, r.plan.differChannels == 0
                                 ? tr("✓ 0 channels differ")
                                 : tr("⚠ %n channel(s) differ", "", r.plan.differChannels));
        }
        else
        {
            noTick(item);
            item->setText(2, m_kind == Upgrade ? tr("kept as a function") : tr("stays a function"));
            item->setText(3, QString());
            item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        }
    }
    if (m_kind == Upgrade)
    {
        buildUpgradeRows();
        buildCaptureRows();
        updatePlanTexts();
    }
    for (int c = 0; c < m_table->columnCount(); ++c)
        if (c != 1)
            m_table->resizeColumnToContents(c);
    if (m_table->topLevelItemCount() > 0)
    {
        // The first cue list, else the first row.
        QTreeWidgetItem *first = m_table->topLevelItem(0);
        for (int i = 0; i < m_table->topLevelItemCount(); ++i)
            if (m_table->topLevelItem(i)->flags() & Qt::ItemIsUserCheckable)
            {
                first = m_table->topLevelItem(i);
                break;
            }
        m_table->setCurrentItem(first);
        showItem(first);
    }
    else
    {
        m_details->setHtml(resultsHtml() + QStringLiteral("<p>%1</p>").arg(
                               (m_kind == Upgrade ? tr("No chasers or collections in this show.")
                                                  : tr("No chasers left to convert.")).toHtmlEscaped()));
    }
    updateButtons();
}

void ShowConverter::updateButtons()
{
    m_convertButton->setEnabled(!chosen().isEmpty() || !chosenScenes().isEmpty());
    if (m_restoreButton != nullptr)
    {
        const QTreeWidgetItem *item = m_table->currentItem();
        m_restoreButton->setEnabled(m_restorer && item != nullptr
                                    && item->data(0, kKindRole).toInt() == ConvertedRow);
    }
}

QList<quint32> ShowConverter::chosen() const
{
    QList<quint32> out;
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *item = m_table->topLevelItem(i);
        if (item->data(0, kKindRole).toInt() == ChaserRow && (item->flags() & Qt::ItemIsUserCheckable)
            && item->checkState(0) == Qt::Checked)
            out << item->data(0, kChaserRole).toUInt();
    }
    return out;
}

void ShowConverter::setChosen(quint32 chaserId, bool on)
{
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_table->topLevelItem(i);
        if (item->data(0, kKindRole).toInt() == ChaserRow && item->data(0, kChaserRole).toUInt() == chaserId
            && (item->flags() & Qt::ItemIsUserCheckable))
            item->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
    }
    updateButtons();
}

bool ShowConverter::inPlace() const
{
    return m_mode->currentData().toBool();
}

void ShowConverter::setInPlace(bool on)
{
    m_mode->setCurrentIndex(on ? 0 : 1);
}

void ShowConverter::select(quint32 chaserId)
{
    const Function *f = m_doc->function(chaserId);
    for (const Row &r : m_rows)
    {
        if (r.chaser != chaserId || f == nullptr)
            continue;
        QString h = resultsHtml();
        h += QStringLiteral("<h2>%1</h2>").arg(f->name().toHtmlEscaped());
        if (r.cls.kind == ShowTreeController::ChaserKind::CueList)
        {
            h += QStringLiteral("<p>%1</p>").arg(tr("A cue list: it %1.")
                                                 .arg(r.cls.why.join(QStringLiteral(", "))).toHtmlEscaped());
            h += ImportSheet::html(m_doc, r.plan, &r.list, f->name(), true);
        }
        else
        {
            h += QStringLiteral("<p>%1</p>").arg(tr("An effect: it %1. It stays a function -- drop it on a cue "
                                                    "and it plays there as a ▣ layer, as it is. (Import chaser… "
                                                    "still takes it, if you want it as a list anyway.)")
                                                 .arg(r.cls.why.join(QStringLiteral(", "))).toHtmlEscaped());
        }
        m_details->setHtml(h);
        return;
    }
}

QStringList ShowConverter::convert()
{
    const QList<quint32> ids = chosen();
    QStringList results;
    if (m_kind == Upgrade)
    {
        if (!m_upgrader)
            return results;
        results = m_upgrader(ids, inPlace(), chosenScenes());
        m_results = results;
        build();
        return results;
    }
    if (!m_converter)
        return results;
    const bool place = inPlace();
    for (quint32 id : ids)
        results << m_converter(id, place);
    m_results = results;
    build();
    return results;
}

QString ShowConverter::text() const
{
    QStringList out;
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *item = m_table->topLevelItem(i);
        QString mark = (item->flags() & Qt::ItemIsUserCheckable)
                       ? (item->checkState(0) == Qt::Checked ? QStringLiteral("[x] ") : QStringLiteral("[ ] "))
                       : QStringLiteral("    ");
        QStringList cols;
        for (int c = 0; c < m_table->columnCount(); ++c)
            cols << item->text(c);
        out << mark + cols.join(QStringLiteral(" | "));
    }
    if (m_captureTable != nullptr)
    {
        out << QStringLiteral("--- %1").arg(m_pages->tabText(1));
        for (int i = 0; i < m_captureTable->topLevelItemCount(); ++i)
        {
            const QTreeWidgetItem *item = m_captureTable->topLevelItem(i);
            QString mark = !(item->flags() & Qt::ItemIsUserCheckable) ? QStringLiteral("  - ")
                           : !(item->flags() & Qt::ItemIsEnabled) ? QStringLiteral("( ) ")
                           : (item->checkState(0) == Qt::Checked ? QStringLiteral("[x] ") : QStringLiteral("[ ] "));
            QStringList cols;
            for (int c = 0; c < m_captureTable->columnCount(); ++c)
                cols << item->text(c);
            out << mark + cols.join(QStringLiteral(" | "));
        }
    }
    out << m_details->toPlainText();
    return out.join(QStringLiteral("\n"));
}

/****************************************************************************
 * Upgrade this show (§3.7): collections, lists made in place, raw scenes
 ****************************************************************************/

QString ShowConverter::resultsHtml() const
{
    if (m_results.isEmpty())
        return QString();
    QString h = QStringLiteral("<h3>%1</h3><ul>")
                .arg((m_kind == Upgrade ? tr("Upgraded") : tr("Converted")).toHtmlEscaped());
    for (const QString &l : m_results)
        h += QStringLiteral("<li>%1</li>").arg(l.toHtmlEscaped());
    h += QStringLiteral("</ul>");
    if (m_kind == Upgrade)
        h += QStringLiteral("<p>%1</p>").arg(tr("Undo (Edit ▸ Undo, or ⌘Z in the Show Tree) takes the whole "
                                                "upgrade back as one step. Restore original… puts one chaser "
                                                "back.").toHtmlEscaped());
    return h + QStringLiteral("<hr>");
}

void ShowConverter::buildUpgradeRows()
{
    ShowTreeController *st = m_doc->showTree();

    // Chasers imported in place already: Restore original.
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
        {
            if (!l.inPlace || l.source == invalidId())
                continue;
            const Function *src = m_doc->function(l.source);
            QTreeWidgetItem *item = new QTreeWidgetItem(m_table);
            item->setData(0, kChaserRole, l.id);
            item->setData(0, kKindRole, ConvertedRow);
            noTick(item);
            item->setText(0, src == nullptr ? l.name : src->name());
            item->setText(1, tr("Converted — the cue list %1, in place").arg(l.name));
            item->setText(2, tr("Restore original… puts the chaser back"));
        }

    // Collections: read into the cues of the lists that play them, or left
    // alone (they play as they are).
    for (Function *f : m_doc->functions())
    {
        const Collection *c = qobject_cast<const Collection *>(f);
        if (c == nullptr || !c->isVisible() || st->isGenerated(c->id()))
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(m_table);
        item->setData(0, kChaserRole, c->id());
        item->setData(0, kKindRole, CollectionRow);
        noTick(item);
        item->setText(0, c->name());
        item->setText(1, tr("Collection — %n function(s)", "", int(c->functions().count())));
    }
}

void ShowConverter::buildCaptureRows()
{
    ShowTreeController *st = m_doc->showTree();
    QSignalBlocker block(m_captureTable);
    m_captureTable->clear();
    m_captureRows.clear();

    QList<quint32> order;
    QHash<quint32, CaptureRow> rows;
    auto note = [&](quint32 fid, quint32 chaser) {
        if (qobject_cast<const Scene *>(m_doc->function(fid)) == nullptr || st->isGenerated(fid))
            return;
        if (!rows.contains(fid))
        {
            order << fid;
            rows[fid].scene = fid;
        }
        CaptureRow &r = rows[fid];
        if (chaser == invalidId())
            r.inTree = true;
        else if (!r.chasers.contains(chaser))
            r.chasers << chaser;
    };
    // What the tree plays now, then what the cue lists would.
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.isOpaque())
                        note(layer.target.function, invalidId());
    for (const Row &r : m_rows)
        if (r.cls.kind == ShowTreeController::ChaserKind::CueList)
            for (const Cue &c : r.list.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.isOpaque())
                        note(layer.target.function, r.chaser);

    for (quint32 fid : order)
    {
        CaptureRow r = rows.value(fid);
        r.plan = st->planCapture(fid);
        m_captureRows << r;

        QTreeWidgetItem *item = new QTreeWidgetItem(m_captureTable);
        item->setData(0, kChaserRole, fid);
        item->setData(0, kKindRole, SceneRow);
        item->setText(0, m_doc->function(fid)->name());
        QStringList where;
        QSet<quint32> lists;
        for (const auto &u : r.plan.uses)
            if (!lists.contains(u.first))
            {
                lists.insert(u.first);
                if (const CueList *l = st->cueList(u.first))
                    where << l->name;
            }
        for (quint32 ch : r.chasers)
            if (const Function *f = m_doc->function(ch))
                where << tr("%1 (once converted)").arg(f->name());
        item->setText(1, where.join(QStringLiteral(", ")));
        // Offered when a palette plays some of it exactly; whether the tree
        // plays it is known once the chasers are converted.
        if (!r.plan.layers.isEmpty())
        {
            int made = 0;
            for (const ShowTreeController::CapturePalette &cp : r.plan.palettes)
                if (cp.reuse == QLCPalette::invalidId())
                    ++made;
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Unchecked);
            item->setText(2, tr("%n layer(s)", "", int(r.plan.layers.count())) + QStringLiteral(" · ")
                             + tr("%n palette(s)", "", int(r.plan.palettes.count()))
                             + (made > 0 ? QStringLiteral(" (%1)").arg(tr("%n new", "", made)) : QString()));
            item->setText(3, r.plan.leftovers.isEmpty() ? tr("nothing")
                                                        : tr("%n channel(s)", "", int(r.plan.leftovers.count())));
        }
        else
        {
            noTick(item);
            item->setText(2, tr("stays raw: %1").arg(r.plan.why));
            item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        }
    }
    for (int c = 0; c < m_captureTable->columnCount(); ++c)
        if (c != 1)
            m_captureTable->resizeColumnToContents(c);
    if (m_captureTable->topLevelItemCount() > 0)
        m_captureTable->setCurrentItem(m_captureTable->topLevelItem(0));
    m_pages->setTabText(1, tr("2. Raw scenes into palettes (%1)").arg(m_captureTable->topLevelItemCount()));
}

void ShowConverter::updatePlanTexts()
{
    if (m_kind != Upgrade)
        return;
    ShowTreeController *st = m_doc->showTree();
    const QList<quint32> ticked = chosen();
    QSignalBlocker block(m_table);

    // What the ticked cue lists play, down through collections and chasers.
    QHash<quint32, QStringList> readInto;       // collection -> chaser names
    QHash<quint32, QStringList> playedBy;       // ... by an unticked or effect chaser
    for (const Row &r : m_rows)
    {
        const Function *ch = m_doc->function(r.chaser);
        if (ch == nullptr)
            continue;
        const bool into = ticked.contains(r.chaser);
        QSet<quint32> seen;
        std::function<void(quint32, int)> walk = [&](quint32 fid, int depth) {
            if (seen.contains(fid) || depth > 8)
                return;
            seen.insert(fid);
            const Function *f = m_doc->function(fid);
            if (const Collection *c = qobject_cast<const Collection *>(f))
            {
                (into ? readInto : playedBy)[fid] << ch->name();
                for (quint32 m : c->functions())
                    walk(m, depth + 1);
            }
            else if (const Chaser *sub = qobject_cast<const Chaser *>(f))
            {
                for (const ChaserStep &s : sub->steps())
                    walk(s.fid, depth + 1);
            }
        };
        walk(r.chaser, 0);
    }
    QSet<quint32> opaque;
    for (const Production &p : st->productions())
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.isOpaque())
                        opaque.insert(layer.target.function);

    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_table->topLevelItem(i);
        const quint32 id = item->data(0, kChaserRole).toUInt();
        switch (item->data(0, kKindRole).toInt())
        {
            case ChaserRow:
                if (item->flags() & Qt::ItemIsUserCheckable)
                {
                    const bool on = item->checkState(0) == Qt::Checked;
                    item->setText(2, on ? tr("cue list: %1").arg(item->data(0, kPlanRole).toString())
                                        : tr("leave alone: plays as it is"));
                    if (!on)
                        item->setText(3, QString());
                    else if (const Row *r = [&]() -> const Row * {
                                 for (const Row &x : m_rows)
                                     if (x.chaser == id)
                                         return &x;
                                 return nullptr;
                             }())
                        item->setText(3, r->plan.differChannels == 0
                                             ? tr("✓ 0 channels differ")
                                             : tr("⚠ %n channel(s) differ", "", r->plan.differChannels));
                }
                break;
            case CollectionRow:
            {
                QString plan;
                const quint32 into = st->importedInto(id);
                if (into != invalidId())
                {
                    const CueList *l = st->cueList(into);
                    plan = tr("filed with the originals of %1 (Restore original puts it back)")
                           .arg(l == nullptr ? QString() : l->name);
                }
                else if (readInto.contains(id))
                {
                    plan = tr("read into the cues of %1").arg(readInto.value(id).join(QStringLiteral(", ")));
                }
                else if (opaque.contains(id))
                {
                    plan = tr("leave alone: a ▣ layer in the tree plays it as it is");
                }
                else if (playedBy.contains(id))
                {
                    plan = tr("leave alone: %1 plays it as it is").arg(playedBy.value(id).join(QStringLiteral(", ")));
                }
                else
                {
                    plan = tr("leave alone: plays as it is (drop it on a cue for a ▣ layer)");
                }
                item->setText(2, plan);
                break;
            }
            default:
                break;
        }
    }

    // A raw scene only a list would play: offered while its chaser is ticked.
    if (m_captureTable != nullptr)
    {
        QSignalBlocker cblock(m_captureTable);
        for (int i = 0; i < m_captureTable->topLevelItemCount() && i < m_captureRows.count(); ++i)
        {
            QTreeWidgetItem *item = m_captureTable->topLevelItem(i);
            if (!(item->flags() & Qt::ItemIsUserCheckable))
                continue;
            const CaptureRow &r = m_captureRows.at(i);
            bool available = r.inTree;
            for (quint32 ch : r.chasers)
                available = available || ticked.contains(ch);
            item->setFlags(available ? item->flags() | Qt::ItemIsEnabled : item->flags() & ~Qt::ItemIsEnabled);
            if (!available)
                item->setCheckState(0, Qt::Unchecked);
            item->setToolTip(0, available ? QString()
                                          : tr("Tick a chaser that plays it on the first page to offer it"));
        }
    }
}

void ShowConverter::showItem(QTreeWidgetItem *item)
{
    const quint32 id = item->data(0, kChaserRole).toUInt();
    const int kind = item->data(0, kKindRole).toInt();
    if (kind == ChaserRow)
    {
        select(id);
        return;
    }
    ShowTreeController *st = m_doc->showTree();
    QString h = resultsHtml();
    h += QStringLiteral("<h2>%1</h2>").arg(item->text(0).toHtmlEscaped());
    if (kind == ConvertedRow)
    {
        const CueList *l = st->cueList(id);
        h += QStringLiteral("<p>%1</p>").arg(
                 tr("Imported in place: the cue list %1 took over from this chaser, which is filed with its "
                    "originals. Restore original… puts the chaser back and points what fires the list at it "
                    "again (it asks first; one undo step).").arg(l == nullptr ? QString() : l->name).toHtmlEscaped());
    }
    else if (kind == CollectionRow)
    {
        const Collection *c = qobject_cast<const Collection *>(m_doc->function(id));
        QStringList members;
        if (c != nullptr)
            for (quint32 m : c->functions())
                if (const Function *f = m_doc->function(m))
                    members << f->name();
        h += QStringLiteral("<p>%1</p>").arg(item->text(2).toHtmlEscaped());
        h += QStringLiteral("<p>%1</p>").arg(tr("Plays: %1").arg(members.join(QStringLiteral(", "))).toHtmlEscaped());
        h += QStringLiteral("<p>%1</p>").arg(
                 tr("A collection the cue lists play becomes their cues' layers on import (a nested one is read in "
                    "too). The collection itself stays, and plays the old way wherever else it is used.")
                 .toHtmlEscaped());
    }
    else if (kind == SceneRow)
    {
        for (const CaptureRow &r : m_captureRows)
        {
            if (r.scene != id)
                continue;
            if (r.plan.layers.isEmpty())
            {
                h += QStringLiteral("<p>%1</p>").arg(tr("Stays raw: %1").arg(r.plan.why).toHtmlEscaped());
                break;
            }
            h += QStringLiteral("<p>%1</p>").arg(
                     tr("Capture into palettes: %1 of its %2 channel values become looks that put out exactly "
                        "them. Checked channel for channel when applied: a cue that would change keeps the scene "
                        "as it is. The scene itself is kept.")
                     .arg(r.plan.capturedChannels).arg(r.plan.capturedChannels + int(r.plan.leftovers.count()))
                     .toHtmlEscaped());
            h += QStringLiteral("<ul>");
            for (const QString &l : captureLines(m_doc, r.plan))
                h += QStringLiteral("<li>%1</li>").arg(l.toHtmlEscaped());
            h += QStringLiteral("</ul>");
            break;
        }
    }
    m_details->setHtml(h);
}

QStringList ShowConverter::captureLines(Doc *doc, const ShowTreeController::CapturePlan &plan)
{
    QStringList out;
    ShowTreeController *st = doc->showTree();
    const Function *scene = doc->function(plan.scene);
    for (const ShowTreeController::CaptureLayer &cl : plan.layers)
    {
        QStringList looks;
        for (auto it = cl.looks.constBegin(); it != cl.looks.constEnd(); ++it)
        {
            const ShowTreeController::CapturePalette &cp = plan.palettes.at(it.value());
            looks << QStringLiteral("%1 %2%3").arg(keyLabel(it.key()), cp.name,
                                                   cp.reuse == QLCPalette::invalidId() ? tr(" (new)") : QString());
        }
        out << tr("Layer on %1: %2").arg(st->targetLabel(cl.target), looks.join(QStringLiteral(", ")));
    }
    if (!plan.leftoverLines.isEmpty())
        out << tr("Stays raw, in “%1 (rest)” beside it: %2")
               .arg(scene == nullptr ? QString() : scene->name(), plan.leftoverLines.join(QStringLiteral("; ")));
    return out;
}

QList<quint32> ShowConverter::chosenScenes() const
{
    QList<quint32> out;
    if (m_captureTable == nullptr)
        return out;
    for (int i = 0; i < m_captureTable->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *item = m_captureTable->topLevelItem(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && (item->flags() & Qt::ItemIsEnabled)
            && item->checkState(0) == Qt::Checked)
            out << item->data(0, kChaserRole).toUInt();
    }
    return out;
}

void ShowConverter::setCaptureChosen(quint32 sceneId, bool on)
{
    if (m_captureTable == nullptr)
        return;
    for (int i = 0; i < m_captureTable->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_captureTable->topLevelItem(i);
        if (item->data(0, kChaserRole).toUInt() == sceneId && (item->flags() & Qt::ItemIsUserCheckable)
            && (item->flags() & Qt::ItemIsEnabled))
            item->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
    }
    updateButtons();
}

QList<quint32> ShowConverter::convertedLists() const
{
    QList<quint32> out;
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
        if (m_table->topLevelItem(i)->data(0, kKindRole).toInt() == ConvertedRow)
            out << m_table->topLevelItem(i)->data(0, kChaserRole).toUInt();
    return out;
}

bool ShowConverter::restore(quint32 listId)
{
    if (!m_restorer || !convertedLists().contains(listId))
        return false;
    const CueList *l = m_doc->showTree()->cueList(listId);
    const Function *src = l == nullptr ? nullptr : m_doc->function(l->source);
    const QString name = src == nullptr ? QString() : src->name();
    if (!m_restorer(listId))
        return false;
    m_results = QStringList(tr("%1: restored, as it was before the import.").arg(name));
    build();
    return true;
}
