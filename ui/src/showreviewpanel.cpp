/*
  Q Light Controller Plus - qlcconsole
  showreviewpanel.cpp

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
#include <QInputDialog>
#include <QMessageBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>

#include "showreviewpanel.h"
#include "monitorproperties.h"
#include "showreviewmarker.h"
#include "showtreeview.h"
#include "contextmenus.h"
#include "showstatus.h"
#include "doc.h"

namespace
{
    enum Roles
    {
        IndexRole = Qt::UserRole,   ///< the finding's index in m_findings (-1: none)
        FooterKeyRole,              ///< a footer row: its ShowStatus key
        FooterDataRole              ///< and its items' data
    };
}

ShowReviewPanel::Jumper ShowReviewPanel::s_jumper;
QPointer<ShowReviewPanel> ShowReviewPanel::s_shared;
Doc *ShowReviewPanel::s_doc = nullptr;
QPointer<QWidget> ShowReviewPanel::s_parent;

ShowReviewPanel::ShowReviewPanel(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("showReviewPanel"));
    setWindowTitle(tr("Show Review"));
    setProperty("helpTopic", QStringLiteral("show-review.md"));
    QVBoxLayout *lay = new QVBoxLayout(this);

    QHBoxLayout *top = new QHBoxLayout;
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("showReviewSummary"));
    top->addWidget(m_summary, 1);
    QPushButton *recheckButton = new QPushButton(tr("Re-check"), this);
    recheckButton->setObjectName(QStringLiteral("showReviewRecheck"));
    recheckButton->setToolTip(tr("Run every check again"));
    connect(recheckButton, &QPushButton::clicked, this, &ShowReviewPanel::recheck);
    top->addWidget(recheckButton);
    lay->addLayout(top);

    QHBoxLayout *filters = new QHBoxLayout;
    m_filter = new QComboBox(this);
    m_filter->setObjectName(QStringLiteral("showReviewFilter"));
    m_filter->addItem(tr("Open"), OpenOnly);
    m_filter->addItem(tr("Accepted"), AcceptedOnly);
    m_filter->addItem(tr("All"), All);
    filters->addWidget(m_filter);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("showReviewSearch"));
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    filters->addWidget(m_search, 1);
    lay->addLayout(filters);
    connect(m_filter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ShowReviewPanel::rebuild);
    connect(m_search, &QLineEdit::textChanged, this, &ShowReviewPanel::rebuild);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("showReviewTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels(QStringList() << tr("Check") << tr("Finding"));
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setWordWrap(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(0, 110);
    lay->addWidget(m_tree, 1);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowReviewPanel::updateButtons);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { fixSelected(); });
    // Right-click a row: the same answers as a warning's mark (ShowReviewMarker).
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ShowReviewPanel::slotContextMenu);

    QHBoxLayout *actions = new QHBoxLayout;
    m_fix = new QPushButton(tr("Fix ▸"), this);
    m_fix->setObjectName(QStringLiteral("showReviewFix"));
    m_fix->setToolTip(tr("Go to the place to fix it (or double-click the row)"));
    connect(m_fix, &QPushButton::clicked, this, [this]() { fixSelected(); });
    m_accept = new QPushButton(tr("Not a Problem…"), this);
    m_accept->setObjectName(QStringLiteral("showReviewAccept"));
    m_accept->setToolTip(tr("It doesn't matter here: say why. It is listed under Accepted; the note is "
                            "saved with the show, and the finding comes back if what it's about changes."));
    connect(m_accept, &QPushButton::clicked, this, &ShowReviewPanel::slotAccept);
    m_reopen = new QPushButton(tr("Reopen"), this);
    m_reopen->setObjectName(QStringLiteral("showReviewReopen"));
    connect(m_reopen, &QPushButton::clicked, this, [this]() { reopenSelected(); });
    m_cut = new QPushButton(tr("Apply This Cut"), this);
    m_cut->setObjectName(QStringLiteral("showReviewApplyCut"));
    m_cut->setToolTip(tr("Tape the bottom of the beam or scan window by the degrees suggested: it "
                         "goes in the fixture's Masking, and the finding is checked again"));
    connect(m_cut, &QPushButton::clicked, this, [this]() { applySuggestedCut(); });
    m_why = new QPushButton(tr("Why?"), this);
    m_why->setObjectName(QStringLiteral("showReviewWhy"));
    connect(m_why, &QPushButton::clicked, this, &ShowReviewPanel::slotWhy);
    m_off = new QPushButton(tr("Turn Check Off…"), this);
    m_off->setObjectName(QStringLiteral("showReviewOff"));
    m_off->setToolTip(tr("This check never applies to this show: turn it off for the show, with a note"));
    connect(m_off, &QPushButton::clicked, this, &ShowReviewPanel::slotTurnOff);
    actions->addWidget(m_fix);
    actions->addWidget(m_accept);
    actions->addWidget(m_cut);
    actions->addWidget(m_reopen);
    actions->addWidget(m_why);
    actions->addWidget(m_off);
    actions->addStretch(1);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    actions->addWidget(box);
    lay->addLayout(actions);

    if (m_doc != nullptr)
    {
        connect(m_doc->showReview(), &ShowReview::reviewed, this, &ShowReviewPanel::rebuild);
        connect(m_doc->showReview(), &ShowReview::acceptancesChanged, this, &ShowReviewPanel::rebuild);
    }
    connect(ShowStatus::instance(), &ShowStatus::changed, this, &ShowReviewPanel::rebuild);

    rebuild();
    resize(760, 480);
}

void ShowReviewPanel::setJumper(const Jumper &jumper)
{
    s_jumper = jumper;
}

void ShowReviewPanel::setShared(Doc *doc, QWidget *parent)
{
    s_doc = doc;
    s_parent = parent;
}

bool ShowReviewPanel::canOpenShared()
{
    return s_doc != nullptr;
}

ShowReviewPanel *ShowReviewPanel::shared()
{
    return s_shared.data();
}

ShowReviewPanel *ShowReviewPanel::openShared()
{
    if (s_doc == nullptr)
        return nullptr;
    if (s_shared.isNull())
    {
        s_shared = new ShowReviewPanel(s_doc, s_parent.data());
        s_shared->setAttribute(Qt::WA_DeleteOnClose);
    }
    // Opened: a fresh look, unless one ran a moment ago.
    const ShowReview::Result &last = s_doc->showReview()->lastResult();
    if (!last.ran || last.when.secsTo(QDateTime::currentDateTime()) > 2)
        s_doc->showReview()->run();
    s_shared->show();
    s_shared->raise();
    s_shared->activateWindow();
    return s_shared.data();
}

void ShowReviewPanel::setFilter(Filter filter)
{
    m_filter->setCurrentIndex(m_filter->findData(int(filter)));
}

void ShowReviewPanel::setSearch(const QString &text)
{
    m_search->setText(text);
}

void ShowReviewPanel::recheck()
{
    if (m_doc != nullptr)
        m_doc->showReview()->run();     // reviewed() redraws
}

static QString severityTitle(ReviewFinding::Severity s)
{
    switch (s)
    {
        case ReviewFinding::Blocker: return ShowReviewPanel::tr("Blockers");
        case ReviewFinding::Warning: return ShowReviewPanel::tr("Warnings");
        default: return ShowReviewPanel::tr("Notes");
    }
}

void ShowReviewPanel::rebuild()
{
    if (m_doc == nullptr)
        return;
    QSet<QString> wasSelected;
    for (const ReviewFinding &f : selectedFindings())
        wasSelected.insert(f.key());

    const ShowReview::Result &r = m_doc->showReview()->lastResult();
    m_findings = r.findings;
    const Filter filter = Filter(m_filter->currentData().toInt());
    const QString search = m_search->text().trimmed();

    // The footer's design entries no check reads yet (folded in by id).
    struct FooterRow { QString key, id, text, detail; ReviewFinding::Severity severity; QVariantList data; };
    QList<FooterRow> footer;
    for (const ShowStatus::Entry &e : ShowStatus::instance()->allEntries())
    {
        const QString id = ShowReview::footerCheckId(e.key);
        if (id.isEmpty() || ShowReview::check(id) != nullptr)
            continue;
        FooterRow row;
        row.key = e.key;
        row.id = id;
        row.text = e.summary.isEmpty() ? e.key : e.summary;
        row.detail = e.detail;
        row.severity = e.severity == ShowStatus::Error ? ReviewFinding::Blocker
                     : e.severity == ShowStatus::Warning ? ReviewFinding::Warning : ReviewFinding::Note;
        row.data = e.itemData;
        footer << row;
    }

    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem *groups[3] = { nullptr, nullptr, nullptr };
    QTreeWidgetItem *accepted = nullptr, *meantGroup = nullptr;
    int counts[3] = { 0, 0, 0 }, acceptedCount = 0, meantCount = 0;
    auto groupFor = [&](ReviewFinding::Severity s) {
        if (groups[s] == nullptr)
        {
            groups[s] = new QTreeWidgetItem();
            groups[s]->setData(0, IndexRole, -1);
            groups[s]->setFlags(groups[s]->flags() & ~Qt::ItemIsSelectable);
            QFont f = groups[s]->font(0);
            f.setBold(true);
            groups[s]->setFont(0, f);
        }
        return groups[s];
    };
    auto matches = [&](const QString &a, const QString &b) {
        return search.isEmpty() || a.contains(search, Qt::CaseInsensitive) || b.contains(search, Qt::CaseInsensitive);
    };

    for (int i = 0; i < m_findings.count(); ++i)
    {
        const ReviewFinding &f = m_findings.at(i);
        const bool isAccepted = f.state == ReviewFinding::Accepted;
        if ((filter == OpenOnly && isAccepted) || (filter == AcceptedOnly && !isAccepted))
            continue;
        if (!matches(f.check, f.message))
            continue;
        QTreeWidgetItem *parentItem;
        if (f.intended)
        {
            if (meantGroup == nullptr)
            {
                meantGroup = new QTreeWidgetItem();
                meantGroup->setData(0, IndexRole, -1);
                meantGroup->setFlags(meantGroup->flags() & ~Qt::ItemIsSelectable);
                QFont font = meantGroup->font(0);
                font.setBold(true);
                meantGroup->setFont(0, font);
                meantGroup->setForeground(0, ShowTreeView::meantColour(palette().color(QPalette::Base)));
            }
            parentItem = meantGroup;
            ++meantCount;
        }
        else if (isAccepted)
        {
            if (accepted == nullptr)
            {
                accepted = new QTreeWidgetItem();
                accepted->setData(0, IndexRole, -1);
                accepted->setFlags(accepted->flags() & ~Qt::ItemIsSelectable);
                QFont font = accepted->font(0);
                font.setBold(true);
                accepted->setFont(0, font);
            }
            parentItem = accepted;
            ++acceptedCount;
        }
        else
        {
            parentItem = groupFor(f.severity);
            counts[f.severity]++;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(parentItem);
        item->setText(0, f.check);
        QString text = f.message;
        if (f.state == ReviewFinding::Changed)
            text = tr("↺ Changed since accepted: %1").arg(text);
        if (f.intended)
        {
            QString who = f.acceptedBy.isEmpty() ? tr("Meant") : tr("Meant by %1").arg(f.acceptedBy);
            if (!f.acceptedWhen.isEmpty())
                who += QStringLiteral(" · ") + f.acceptedWhen.left(10);
            if (!f.acceptedNote.trimmed().isEmpty())
                who += QStringLiteral(" · “%1”").arg(f.acceptedNote);
            text += QStringLiteral("\n— ") + who + QStringLiteral(" · ") + tr("kept on its layer in the Show Tree");
        }
        else if (f.state != ReviewFinding::Open)
            text += QStringLiteral("\n— “%1” · %2 · %3").arg(f.acceptedNote, f.acceptedBy, f.acceptedWhen.left(10));
        item->setText(1, text);
        item->setData(0, IndexRole, i);
        const ShowReview::Check *c = ShowReview::check(f.check);
        // The check's id is the compact badge: its detail, as on every mark.
        item->setToolTip(0, ShowReviewMarker::tooltip(m_doc, { f }));
        if (c != nullptr)
        {
            item->setToolTip(1, QStringLiteral("<p><b>%1</b></p><p>%2</p>").arg(c->title.toHtmlEscaped(),
                                                                                c->why.toHtmlEscaped()));
        }
    }
    if (filter != AcceptedOnly)
    {
        for (const FooterRow &row : footer)
        {
            if (!matches(row.id, row.text))
                continue;
            QTreeWidgetItem *item = new QTreeWidgetItem(groupFor(row.severity));
            counts[row.severity]++;
            item->setText(0, row.id);
            item->setText(1, row.text);
            item->setToolTip(1, row.detail);
            item->setData(0, IndexRole, -1);
            item->setData(0, FooterKeyRole, row.key);
            item->setData(0, FooterDataRole, row.data);
        }
    }

    for (int s = ReviewFinding::Blocker; s >= ReviewFinding::Note; --s)
    {
        if (groups[s] == nullptr)
            continue;
        groups[s]->setText(0, QStringLiteral("%1 (%2)").arg(severityTitle(ReviewFinding::Severity(s))).arg(counts[s]));
        groups[s]->setFirstColumnSpanned(true);
        m_tree->addTopLevelItem(groups[s]);
        groups[s]->setFirstColumnSpanned(true);
        groups[s]->setExpanded(true);
    }
    if (meantGroup != nullptr)
    {
        meantGroup->setText(0, tr("Meant (%1)").arg(meantCount));
        m_tree->addTopLevelItem(meantGroup);
        meantGroup->setFirstColumnSpanned(true);
        meantGroup->setExpanded(true);
    }
    if (accepted != nullptr)
    {
        accepted->setText(0, tr("Accepted (%1)").arg(acceptedCount));
        m_tree->addTopLevelItem(accepted);
        accepted->setFirstColumnSpanned(true);
        accepted->setExpanded(true);
    }

    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        const int idx = (*it)->data(0, IndexRole).toInt();
        if (idx >= 0 && idx < m_findings.count() && wasSelected.contains(m_findings.at(idx).key()))
            (*it)->setSelected(true);
        ++it;
    }
    m_tree->blockSignals(false);

    if (!r.ran)
        m_summary->setText(tr("Not checked yet."));
    else
    {
        int meants = 0;
        for (const ReviewFinding &f : r.findings)
            meants += f.intended ? 1 : 0;
        const int accepts = r.accepted[0] + r.accepted[1] + r.accepted[2] - meants;
        QString s = tr("%1 blockers · %2 warnings · %3 notes open")
                        .arg(r.open[ReviewFinding::Blocker]).arg(r.open[ReviewFinding::Warning])
                        .arg(r.open[ReviewFinding::Note]);
        if (meants > 0)
            s += QStringLiteral(" · ") + tr("%1 meant").arg(meants);
        if (accepts > 0)
            s += QStringLiteral(" · ") + tr("%1 accepted").arg(accepts);
        s += QStringLiteral(" — ") + tr("checked %1 (%2 ms)").arg(r.when.toString(QStringLiteral("HH:mm"))).arg(r.ms);
        m_summary->setText(s);
    }
    updateButtons();
}

QList<ReviewFinding> ShowReviewPanel::selectedFindings() const
{
    QList<ReviewFinding> out;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
    {
        const int idx = item->data(0, IndexRole).toInt();
        if (idx >= 0 && idx < m_findings.count())
            out << m_findings.at(idx);
    }
    return out;
}

bool ShowReviewPanel::selectCheck(const QString &checkId)
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        if ((*it)->text(0) == checkId)
        {
            m_tree->clearSelection();
            (*it)->setSelected(true);
            m_tree->setCurrentItem(*it);
            return true;
        }
        ++it;
    }
    return false;
}

void ShowReviewPanel::updateButtons()
{
    const QList<ReviewFinding> sel = selectedFindings();
    bool anyOpen = false, anyAccepted = false;
    for (const ReviewFinding &f : sel)
    {
        if (f.state == ReviewFinding::Accepted && !f.checkOff)
            anyAccepted = true;
        if (f.state != ReviewFinding::Accepted || f.intended)
            anyOpen = true;     // open, or meant (not a problem is still an answer)
    }
    bool footerRow = false;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
        footerRow = footerRow || !item->data(0, FooterKeyRole).toString().isEmpty();
    m_fix->setEnabled((sel.count() == 1 && !sel.first().jump.isEmpty()) || (footerRow && sel.isEmpty()));
    m_accept->setEnabled(anyOpen);
    /* PWR-12 (light on the cyc): Accept is "I meant it", kept on the layer
       that lights it rather than as a note; Reopen takes that back; Fix
       shows the layer. */
    const bool meantRows = allMeantRows(sel, true), unmeantRows = allMeantRows(sel, false);
    bool backwash = !sel.isEmpty();
    for (const ReviewFinding &f : sel)
        backwash = backwash && f.check == QLatin1String("PWR-12");
    m_fix->setText(backwash ? tr("Show Me ▸") : tr("Fix ▸"));
    m_accept->setText(meantRows ? tr("I Meant It") : tr("Not a Problem…"));
    m_accept->setToolTip(meantRows ? tr("This light on the cyc is on purpose: kept on the layer that lights "
                                        "it (one undo step), so the warning goes away and stays away")
                                   : tr("It doesn't matter here: say why. It is listed under Accepted; the "
                                        "note is saved with the show, and the finding comes back if what "
                                        "it's about changes."));
    m_reopen->setText(unmeantRows ? tr("Not Meant") : tr("Reopen"));
    m_reopen->setEnabled(anyAccepted || (sel.count() == 1 && sel.first().checkOff));
    m_cut->setVisible(sel.count() == 1 && !sel.first().fixMask.isEmpty());
    m_why->setEnabled(sel.count() == 1);
    m_off->setEnabled(sel.count() == 1);
}

bool ShowReviewPanel::allMeantRows(const QList<ReviewFinding> &findings, bool meant)
{
    if (findings.isEmpty())
        return false;
    for (const ReviewFinding &f : findings)
        if (!ShowReview::canMarkMeant(f, meant))
            return false;
    return true;
}

bool ShowReviewPanel::markMeantSelected(bool meant)
{
    if (m_doc == nullptr)
        return false;
    bool any = false;
    for (const ReviewFinding &f : selectedFindings())
        if (ShowReview::canMarkMeant(f, meant))
            any = ShowReview::setLightMeant(m_doc, f, meant) || any;
    if (any)
        recheck();
    return any;
}

bool ShowReviewPanel::selectFinding(const QString &key)
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it)
    {
        const int idx = (*it)->data(0, IndexRole).toInt();
        if (idx >= 0 && idx < m_findings.count() && m_findings.at(idx).key() == key)
        {
            m_tree->clearSelection();
            (*it)->setSelected(true);
            m_tree->setCurrentItem(*it);
            m_tree->scrollToItem(*it);
            return true;
        }
        ++it;
    }
    return false;
}

bool ShowReviewPanel::acceptSelected(const QString &note, const QString &by)
{
    if (m_doc == nullptr || note.trimmed().isEmpty())
        return false;
    bool any = false;
    for (const ReviewFinding &f : selectedFindings())
        if (f.state != ReviewFinding::Accepted || f.intended)
            any = ShowReviewMarker::notAProblem(m_doc, f, nullptr, note, by) || any;
    return any;
}

bool ShowReviewPanel::reopenSelected()
{
    if (m_doc == nullptr)
        return false;
    bool any = false;
    bool marks = false;
    for (const ReviewFinding &f : selectedFindings())
    {
        if (f.intended)
        {
            marks = ShowReview::setLightMeant(m_doc, f, false) || marks;
            continue;
        }
        if (f.checkOff)
            any = m_doc->showReview()->turnOn(f.check) || any;
        else
            any = m_doc->showReview()->reopen(f) || any;
    }
    if (marks)
        recheck();
    return any || marks;
}

bool ShowReviewPanel::fixSelected()
{
    const QList<QTreeWidgetItem *> items = m_tree->selectedItems();
    if (items.count() != 1)
        return false;
    QTreeWidgetItem *item = items.first();
    const QString footerKey = item->data(0, FooterKeyRole).toString();
    QString jump;
    QVariantList data;
    if (!footerKey.isEmpty())
    {
        jump = QStringLiteral("status:") + footerKey;
        data = item->data(0, FooterDataRole).toList();
    }
    else
    {
        const int idx = item->data(0, IndexRole).toInt();
        if (idx < 0 || idx >= m_findings.count())
            return false;
        jump = m_findings.at(idx).jump;
        data = m_findings.at(idx).jumpData;
    }
    return runJump(this, jump, data);
}

bool ShowReviewPanel::runJump(QWidget *parent, const QString &jumpTo, const QVariantList &data)
{
    QString jump = jumpTo;
    if (jump.startsWith(QStringLiteral("status:")))
    {
        // The footer's own fix for it (Patch…, Re-patch…, Open Connections).
        const QString key = jump.mid(7);
        if (!ShowStatus::instance()->actionLabel(key).isEmpty())
            return ShowStatus::instance()->runAction(key, parent, data);
        jump = QStringLiteral("fixtures");
    }
    return s_jumper ? s_jumper(parent, jump, data) : false;
}

void ShowReviewPanel::applySuggestedCut()
{
    const QList<ReviewFinding> sel = selectedFindings();
    MonitorProperties *mp = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (sel.count() != 1 || mp == nullptr)
        return;
    const QVariantList &fix = sel.first().fixMask;
    for (int i = 0; i + 1 < fix.size(); i += 2)
    {
        const quint32 fid = fix.at(i).toUInt();
        FixtureRigProps rp = mp->fixtureRigProps(fid);
        rp.maskBottom += fix.at(i + 1).toFloat();
        mp->setFixtureRigProps(fid, rp);
    }
    m_doc->setModified();
    recheck();
}

void ShowReviewPanel::slotAccept()
{
    const QList<ReviewFinding> sel = selectedFindings();
    if (sel.isEmpty())
        return;
    if (allMeantRows(sel, true))
    {
        markMeantSelected(true);        // no note: the mark is the answer
        return;
    }
    QString note, by;
    if (askAcceptNote(this, sel, &note, &by))
        acceptSelected(note, by);
}

bool ShowReviewPanel::askAcceptNote(QWidget *parent, const QList<ReviewFinding> &sel,
                                    QString *noteOut, QString *byOut)
{
    if (sel.isEmpty())
        return false;
    QDialog dlg(parent);
    dlg.setWindowTitle(tr("Not a Problem"));
    QFormLayout *form = new QFormLayout(&dlg);
    QLabel *what = new QLabel(sel.count() == 1 ? sel.first().message
                                               : tr("%n finding(s)", "", sel.count()), &dlg);
    what->setWordWrap(true);
    form->addRow(what);
    QLineEdit *note = new QLineEdit(&dlg);
    note->setObjectName(QStringLiteral("showReviewNote"));
    note->setPlaceholderText(tr("Why it isn't a problem here (saved with the show)"));
    if (sel.count() == 1 && sel.first().state == ReviewFinding::Changed)
        note->setText(sel.first().acceptedNote);
    form->addRow(tr("Note:"), note);
    QLineEdit *by = new QLineEdit(ShowReview::defaultName(), &dlg);
    by->setObjectName(QStringLiteral("showReviewBy"));
    form->addRow(tr("Your name:"), by);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText(tr("Not a Problem"));
    box->button(QDialogButtonBox::Ok)->setEnabled(!note->text().trimmed().isEmpty());
    connect(note, &QLineEdit::textChanged, box, [box](const QString &t) {
        box->button(QDialogButtonBox::Ok)->setEnabled(!t.trimmed().isEmpty());
    });
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(box);
    dlg.resize(480, dlg.sizeHint().height());
    if (dlg.exec() != QDialog::Accepted)
        return false;
    if (by->text().trimmed() != ShowReview::defaultName())
        ShowReview::setDefaultName(by->text());
    if (noteOut != nullptr)
        *noteOut = note->text();
    if (byOut != nullptr)
        *byOut = by->text();
    return true;
}

void ShowReviewPanel::slotContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (item == nullptr || m_doc == nullptr)
        return;
    if (!item->isSelected())
    {
        m_tree->clearSelection();
        item->setSelected(true);
    }
    const QList<ReviewFinding> sel = selectedFindings();
    if (sel.isEmpty())
        return;
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("showReviewRowMenu"));
    menu.setToolTipsVisible(true);
    {
        ContextMenu cm(&menu);
        ShowReviewMarker::fillMenu(cm, m_doc, sel, this, 10, false);
    }
    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void ShowReviewPanel::slotWhy()
{
    const QList<ReviewFinding> sel = selectedFindings();
    if (sel.count() != 1)
        return;
    const ShowReview::Check *c = ShowReview::check(sel.first().check);
    if (c == nullptr)
        return;
    QMessageBox::information(this, QStringLiteral("%1 %2").arg(c->id, c->title), c->why);
}

void ShowReviewPanel::slotTurnOff()
{
    const QList<ReviewFinding> sel = selectedFindings();
    if (sel.count() != 1 || m_doc == nullptr)
        return;
    const ShowReview::Check *c = ShowReview::check(sel.first().check);
    bool ok = false;
    const QString note = QInputDialog::getText(this, tr("Turn Check Off"),
        tr("Turn %1 (%2) off for this show. Why doesn't it apply here?")
            .arg(sel.first().check, c != nullptr ? c->title : QString()),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !note.trimmed().isEmpty())
        m_doc->showReview()->turnOff(sel.first().check, note, ShowReview::defaultName());
}
