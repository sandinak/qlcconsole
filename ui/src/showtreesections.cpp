/*
  Q Light Controller Plus
  showtreesections.cpp

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

/* The Show Tree's song sections (Q190): the production page's Numbering
   setting and Merge lists into one…, a section row's page (its name, Block
   at section start), + Section. The engine side is ShowTreeController's
   (engine/src/showtreesections.cpp). */

#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialog>
#include <QLabel>

#include "showtreeview.h"
#include "importsheet.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

/* What a scheme is called, and a line on what its sections are. */
static QString numberingName(Numbering n)
{
    switch (n)
    {
        case Numbering::ShowPartIteration: return ShowTreeView::tr("Show.Part.Iteration (0.0.1, 1.0.1, 1.2.1)");
        case Numbering::Hundreds: return ShowTreeView::tr("Hundreds per song (Eos / MA: 100, 101.5, 200)");
        case Numbering::Manual: return ShowTreeView::tr("Free numbers, sections by hand (QLab)");
        case Numbering::None: break;
    }
    return ShowTreeView::tr("None: no sections");
}

QString ShowTreeView::sectionRole(const Section &s)
{
    return s.key.isEmpty() ? QStringLiteral("#%1").arg(s.headCue) : s.key;
}

void ShowTreeView::buildSectionPages()
{
    // The production page: how its cues are numbered, and Merge.
    if (QFormLayout *f = qobject_cast<QFormLayout *>(m_prodPage->layout()))
    {
        m_prodNumbering = new QComboBox(m_prodPage);
        m_prodNumbering->setObjectName("stProdNumbering");
        for (Numbering n : { Numbering::None, Numbering::ShowPartIteration, Numbering::Hundreds,
                             Numbering::Manual })
            m_prodNumbering->addItem(numberingName(n), int(n));
        m_prodNumbering->setToolTip(tr("How this show numbers its cues, and so its song sections: the first "
                                       "number (0 preshow, 1 the first song...), the hundred (100s, 200s), or "
                                       "sections you start by hand. Sections only change how the list reads "
                                       "and how a new cue is numbered; nothing plays differently."));
        f->insertRow(f->rowCount() - 1, tr("Numbering"), m_prodNumbering);
        connect(m_prodNumbering, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            if (currentKind() == ProductionNode)
                m_st->setNumbering(currentId(), Numbering(m_prodNumbering->itemData(index).toInt()));
        });
        m_prodNumberingHint = new QLabel(m_prodPage);
        m_prodNumberingHint->setObjectName("stProdNumberingHint");
        m_prodNumberingHint->setWordWrap(true);
        m_prodNumberingHint->setTextFormat(Qt::RichText);
        connect(m_prodNumberingHint, &QLabel::linkActivated, this, [this](const QString &link) {
            if (currentKind() == ProductionNode && link.startsWith(QLatin1String("use:")))
                m_st->setNumbering(currentId(), stringToNumbering(link.mid(4)));
        });
        f->insertRow(f->rowCount() - 1, QString(), m_prodNumberingHint);
        m_prodMerge = new QPushButton(tr("Merge lists into one…"), m_prodPage);
        m_prodMerge->setObjectName("stProdMerge");
        m_prodMerge->setToolTip(tr("One cue list for the show: each list becomes a song section of it, in "
                                   "order, keeping its cue numbers (renumbered only on a clash). GO and Run "
                                   "then play the whole show. One undo step."));
        f->insertRow(f->rowCount() - 1, QString(), m_prodMerge);
        connect(m_prodMerge, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ProductionNode)
                mergeListsIntoOne(currentId());
        });
        m_prodOrder = new QPushButton(tr("Running order…"), m_prodPage);
        m_prodOrder->setObjectName("stProdOrder");
        m_prodOrder->setToolTip(tr("Tonight's song order for the Run and GO: drag the songs, untick one to leave "
                                   "it out. No cue is renumbered or moved; Reset goes back to number order. "
                                   "Saved with the show. One undo step."));
        f->insertRow(f->rowCount() - 1, QString(), m_prodOrder);
        connect(m_prodOrder, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ProductionNode)
                editRunningOrder(currentId());
        });
        m_prodOrderInfo = new QLabel(m_prodPage);
        m_prodOrderInfo->setObjectName("stProdOrderInfo");
        m_prodOrderInfo->setWordWrap(true);
        f->insertRow(f->rowCount() - 1, QString(), m_prodOrderInfo);
    }

    // The list page: + Section.
    if (m_listForm != nullptr)
    {
        m_listAddSection = new QPushButton(tr("+ Section"), m_listPage);
        m_listAddSection->setObjectName("stListAddSection");
        m_listAddSection->setToolTip(tr("A new song section at the end, its first cue numbered to the show's "
                                        "scheme (with free numbers: from the selected cue)"));
        m_listForm->addRow(QString(), m_listAddSection);
        connect(m_listAddSection, &QPushButton::clicked, this, [this]() { newSection(); });
    }

    // A section's page.
    m_sectionPage = new QWidget(m_pages);
    m_sectionPage->setObjectName("stSectionPage");
    QFormLayout *f = new QFormLayout(m_sectionPage);
    m_sectionTitle = new QLabel(m_sectionPage);
    QFont bold = m_sectionTitle->font();
    bold.setBold(true);
    m_sectionTitle->setFont(bold);
    f->addRow(m_sectionTitle);
    m_sectionName = new QLineEdit(m_sectionPage);
    m_sectionName->setObjectName("stSectionName");
    m_sectionName->setPlaceholderText(tr("Everybody Scream"));
    m_sectionName->setToolTip(tr("The song (or part of the show) this section is: its header reads "
                                 "\"1 — Everybody Scream\" here and in the Run tab"));
    f->addRow(tr("Name"), m_sectionName);
    connect(m_sectionName, &QLineEdit::editingFinished, this, [this]() {
        Section s;
        if (currentKind() == SectionNode && currentSection(&s))
            m_st->setSectionName(currentId(), s, m_sectionName->text());
    });
    m_sectionBlock = new QCheckBox(tr("Block at section start"), m_sectionPage);
    m_sectionBlock->setObjectName("stSectionBlock");
    m_sectionBlock->setToolTip(tr("Its first cue starts from nothing: what the song before left on stage "
                                  "doesn't carry into this one"));
    f->addRow(QString(), m_sectionBlock);
    connect(m_sectionBlock, &QCheckBox::toggled, this, [this](bool on) {
        Section s;
        if (!m_inspectorUpdating && currentKind() == SectionNode && currentSection(&s))
            m_st->setSectionBlock(currentId(), s, on);
    });
    m_sectionInfo = new QLabel(m_sectionPage);
    m_sectionInfo->setWordWrap(true);
    f->addRow(m_sectionInfo);
    QWidget *row = new QWidget(m_sectionPage);
    QHBoxLayout *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    QPushButton *add = new QPushButton(tr("+ Section"), row);
    add->setObjectName("stSectionAdd");
    connect(add, &QPushButton::clicked, this, [this]() { newSection(); });
    m_sectionRemove = new QPushButton(tr("Remove section"), row);
    m_sectionRemove->setObjectName("stSectionRemove");
    m_sectionRemove->setToolTip(tr("Its cues join the section before; no cue is deleted"));
    connect(m_sectionRemove, &QPushButton::clicked, this, [this]() {
        Section s;
        if (currentKind() == SectionNode && currentSection(&s))
        {
            const quint32 listId = currentId();
            if (m_st->removeSection(listId, s.headCue))
                selectNode(s.headCue);
        }
    });
    h->addWidget(add);
    h->addWidget(m_sectionRemove);
    h->addStretch(1);
    f->addRow(row);
    m_pages->addWidget(m_sectionPage);
}

void ShowTreeView::updateNumberingRows(quint32 productionId)
{
    if (m_prodNumbering == nullptr)
        return;
    const Production *p = m_st->production(productionId);
    if (p == nullptr)
        return;
    QSignalBlocker block(m_prodNumbering);
    m_prodNumbering->setCurrentIndex(qMax(0, m_prodNumbering->findData(int(p->numbering))));
    // A show with no scheme yet: what its numbers look like, to confirm.
    const Numbering suggested = suggestNumbering(*p);
    if (p->numbering == Numbering::None && suggested != Numbering::None)
        m_prodNumberingHint->setText(tr("Its cue numbers look like %1. <a href=\"use:%2\">Use it</a>")
                                     .arg(numberingName(suggested).toHtmlEscaped(), numberingToString(suggested)));
    else
        m_prodNumberingHint->clear();
    m_prodNumberingHint->setVisible(!m_prodNumberingHint->text().isEmpty());
    m_prodMerge->setEnabled(m_st->playableLists(productionId).count() > 1);
    QList<Section> songs;
    const QList<RunningSong> order = m_st->runningOrder(productionId, &songs);
    m_prodOrder->setEnabled(order.count() > 1);
    if (m_st->hasRunningOrder(productionId))
    {
        int left = 0;
        for (const RunningSong &r : order)
            left += r.skip ? 1 : 0;
        m_prodOrderInfo->setText(tr("Tonight's order differs from number order%1.")
                                 .arg(left > 0 ? tr(" (%n song(s) left out)", "", left) : QString()));
    }
    else
        m_prodOrderInfo->clear();
    m_prodOrderInfo->setVisible(!m_prodOrderInfo->text().isEmpty());
}

bool ShowTreeView::currentSection(Section *out) const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (item == nullptr)
        return false;
    if (item->data(0, KindRole).toInt() == SectionNode)
    {
        const QString role = item->data(0, SectionRole).toString();
        for (const Section &s : m_st->sections(item->data(0, IdRole).toUInt()))
            if (sectionRole(s) == role)
            {
                if (out != nullptr)
                    *out = s;
                return true;
            }
        return false;
    }
    const quint32 cueId = currentCueId();
    return cueId != invalidId() && m_st->sectionOfCue(cueId, out);
}

void ShowTreeView::showSectionPage(quint32 listId, const Section &section)
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr)
        return;
    m_sectionTitle->setText(sectionTitle(section));
    m_sectionName->setText(section.name);
    const bool tracking = list->mode == ListMode::Tracking;
    m_sectionBlock->setChecked(m_st->sectionBlocks(listId, section));
    m_sectionBlock->setEnabled(tracking);
    m_sectionRemove->setVisible(section.key.isEmpty());
    const int cues = section.last - section.first + 1;
    QString info = tr("%n cue(s), %1 to %2.", "", cues)
                   .arg(m_st->cueNumber(*list, section.first), m_st->cueNumber(*list, section.last));
    info += QStringLiteral(" ");
    info += section.key.isEmpty()
            ? tr("A section you started by hand: it runs to the next one.")
            : tr("Every cue numbered %1… is in it; + Cue with it selected adds one at its end, numbered so.")
              .arg(m_st->listNumbering(listId) == Numbering::Hundreds
                   ? section.key.left(section.key.length() - 2) : section.key + QLatin1Char('.'));
    if (!tracking)
        info += QStringLiteral(" ") + tr("The list is Non-tracking: every cue starts from nothing already.");
    m_sectionInfo->setText(info);
    m_pages->setCurrentWidget(m_sectionPage);
}

quint32 ShowTreeView::newSection(const QString &name, bool ask)
{
    const quint32 listId = currentListId();
    if (listId == invalidId() || m_st->isLooks(listId))
        return invalidId();
    const Numbering scheme = m_st->listNumbering(listId);
    if (scheme == Numbering::None)
    {
        m_hint->setText(tr("Pick how the show numbers its cues first (the production's Numbering): sections "
                           "follow it."));
        return invalidId();
    }
    QString n = name;
    if (ask && n.isEmpty())
    {
        bool ok = false;
        n = QInputDialog::getText(this, tr("New section"), tr("Name (the song: \"Everybody Scream\")"),
                                  QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return invalidId();
    }
    // Free numbers: the section starts at the selected cue (none: a new one).
    quint32 at = invalidId();
    if (scheme == Numbering::Manual && currentKind() == CueNode)
    {
        const CueList *l = m_st->listOfCue(currentCueId());
        if (l != nullptr && l->id == listId)
            at = currentCueId();
    }
    const quint32 cueId = m_st->addSection(listId, n, at);
    if (cueId != invalidId())
        selectNode(cueId);
    return cueId;
}

bool ShowTreeView::editRunningOrder(quint32 productionId, const QList<RunningSong> &order, bool reset)
{
    if (m_st->production(productionId) == nullptr)
        return false;
    if (reset)
        return m_st->resetRunningOrder(productionId);
    QList<Section> songs;
    const QList<RunningSong> have = m_st->runningOrder(productionId, &songs);
    if (!order.isEmpty())
        return m_st->setRunningOrder(productionId, order);
    if (m_orderPicker)
        return m_st->setRunningOrder(productionId, m_orderPicker(have));

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Running order"));
    QVBoxLayout *v = new QVBoxLayout(&dialog);
    QLabel *text = new QLabel(tr("The songs in the order the Run and GO play them tonight (drag to change it). "
                                 "Untick a song to leave it out: GO passes over it. No cue is renumbered or "
                                 "moved. A song that doesn't start Block still looks as it does in number "
                                 "order; set Block at section start on the songs you move."), &dialog);
    text->setWordWrap(true);
    v->addWidget(text);
    QListWidget *lw = new QListWidget(&dialog);
    lw->setObjectName("stOrderSongs");
    lw->setDragDropMode(QAbstractItemView::InternalMove);
    for (int i = 0; i < have.count(); ++i)
    {
        QListWidgetItem *it = new QListWidgetItem(sectionTitle(songs.at(i)), lw);
        it->setData(Qt::UserRole, have.at(i).id);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        it->setCheckState(have.at(i).skip ? Qt::Unchecked : Qt::Checked);
    }
    v->addWidget(lw);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton *resetButton = bb->addButton(tr("Reset to number order"), QDialogButtonBox::ResetRole);
    bool resetNow = false;
    connect(resetButton, &QPushButton::clicked, &dialog, [&]() { resetNow = true; dialog.accept(); });
    connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    v->addWidget(bb);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    if (resetNow)
        return m_st->resetRunningOrder(productionId);
    QList<RunningSong> out;
    for (int i = 0; i < lw->count(); ++i)
    {
        RunningSong r;
        r.id = lw->item(i)->data(Qt::UserRole).toString();
        r.skip = lw->item(i)->checkState() != Qt::Checked;
        out << r;
    }
    return m_st->setRunningOrder(productionId, out);
}

bool ShowTreeView::mergeListsIntoOne(quint32 productionId, const QList<quint32> &listIds)
{
    const Production *p = m_st->production(productionId);
    if (p == nullptr)
        return false;
    QList<quint32> ids = listIds;
    if (ids.isEmpty() && m_mergePicker)
        ids = m_mergePicker(productionId);
    else if (ids.isEmpty())
    {
        // Which lists, in what order: every cue list ticked, dragged to order.
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Merge lists into one"));
        QVBoxLayout *v = new QVBoxLayout(&dialog);
        QLabel *text = new QLabel(tr("The ticked lists become one cue list, each a song section of it, in this "
                                     "order (drag to change it). Cue numbers stay, unless two clash. Every cue "
                                     "plays as it did: a list's first cue becomes Block. One undo step."),
                                  &dialog);
        text->setWordWrap(true);
        v->addWidget(text);
        QListWidget *lw = new QListWidget(&dialog);
        lw->setObjectName("stMergeLists");
        lw->setDragDropMode(QAbstractItemView::InternalMove);
        for (quint32 lid : m_st->playableLists(productionId))
        {
            QListWidgetItem *it = new QListWidgetItem(m_st->cueList(lid)->name, lw);
            it->setData(Qt::UserRole, lid);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
            it->setCheckState(Qt::Checked);
        }
        v->addWidget(lw);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        bb->button(QDialogButtonBox::Ok)->setText(tr("Merge"));
        connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        v->addWidget(bb);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        for (int i = 0; i < lw->count(); ++i)
            if (lw->item(i)->checkState() == Qt::Checked)
                ids << lw->item(i)->data(Qt::UserRole).toUInt();
    }
    if (ids.isEmpty())
        return false;

    // What fires the lists that go: the one list, after.
    QList<ImportSheet::Repoint> repoints;
    const CueList *keep = m_st->cueList(ids.first());
    if (keep != nullptr)
        for (int i = 1; i < ids.count(); ++i)
            if (const CueList *l = m_st->cueList(ids.at(i)))
            {
                const ImportSheet::Repoint r = ImportSheet::planRepoint(m_doc, l->chaser, keep->chaser);
                if (r.count() > 0)
                    repoints << r;
            }

    MergeReport report;
    QString why;
    if (!m_st->mergeLists(productionId, ids, &report, &why))
    {
        m_hint->setText(why);
        return false;
    }
    QStringList fired;
    for (const ImportSheet::Repoint &r : repoints)
    {
        r.apply(m_doc, true);
        Doc *doc = m_doc;
        m_st->attachToLastStep([doc, r](bool undone) { r.apply(doc, !undone); });
        fired << r.lines();
    }
    selectNode(ids.first());

    QStringList details;
    if (!report.renumbered.isEmpty())
        details << tr("Renumbered (their numbers were taken):") << report.renumbered;
    if (!fired.isEmpty())
        details << tr("Now firing the one list (from its top): %1").arg(fired.join(QStringLiteral(", ")));
    details << report.notes;
    m_hint->setText(tr("%n list(s) merged into one.", "", int(ids.count()))
                    + (report.renumbered.isEmpty() ? QString()
                       : QStringLiteral(" ") + tr("%n cue(s) renumbered.", "", int(report.renumbered.count()))));
    if (!details.isEmpty())
    {
        if (m_confirm)
            m_confirm(tr("Merged"), m_hint->text(), details.join(QLatin1Char('\n')));
        else
            QMessageBox::information(this, tr("Merged"), m_hint->text() + QStringLiteral("\n\n")
                                     + details.join(QLatin1Char('\n')));
    }
    return true;
}
