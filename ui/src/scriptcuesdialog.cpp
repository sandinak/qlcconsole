/*
  Q Light Controller Plus
  scriptcuesdialog.cpp

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
#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QTimer>
#include <QLabel>

#include "scriptcuesdialog.h"
#include "doc.h"

using namespace ShowTree;

namespace {
enum Column { NumberColumn, NameColumn, NoteColumn, LineColumn };
const int kLineRole = Qt::UserRole + 1;
} // namespace

ScriptCuesDialog::ScriptCuesDialog(Doc *doc, quint32 productionId, quint32 listId, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_production(productionId)
    , m_list(listId)
    , m_tree(new QTreeWidget(this))
    , m_intro(new QLabel(this))
    , m_summary(new QLabel(this))
    , m_make(nullptr)
    , m_renumber(new QTimer(this))
{
    setObjectName(QStringLiteral("scriptCuesDialog"));
    setWindowTitle(tr("Cues from the Script"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#cues-from-the-script"));

    const CueList *list = m_doc->showTree()->cueList(m_list);
    m_intro->setObjectName(QStringLiteral("scriptCuesIntro"));
    m_intro->setWordWrap(true);
    m_intro->setText(tr("A cue for each SCENE and LIGHTING line, numbered for the show, called on its line, "
                        "with the line's words as its note. Untick the lines you don't want. "
                        "Nothing is made until you press Make. Into cue list: <b>%1</b>")
                         .arg((list == nullptr ? QString() : list->name).toHtmlEscaped()));

    m_tree->setObjectName(QStringLiteral("scriptCuesTree"));
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({ tr("Cue"), tr("Name"), tr("Note (the script's words)"), tr("Line") });
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(NumberColumn, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(NoteColumn, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(LineColumn, QHeaderView::ResizeToContents);
    m_tree->setColumnWidth(NameColumn, fontMetrics().averageCharWidth() * 30);

    m_summary->setObjectName(QStringLiteral("scriptCuesSummary"));
    m_summary->setWordWrap(true);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_make = buttons->addButton(tr("Make Cues"), QDialogButtonBox::AcceptRole);
    m_make->setObjectName(QStringLiteral("scriptCuesMake"));
    m_make->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &ScriptCuesDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ScriptCuesDialog::reject);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->addWidget(m_intro);
    root->addWidget(m_tree, 1);
    root->addWidget(m_summary);
    root->addWidget(buttons);

    // A song's box ticks all its lines at once: number them again once.
    m_renumber->setSingleShot(true);
    m_renumber->setInterval(0);
    connect(m_renumber, &QTimer::timeout, this, &ScriptCuesDialog::renumber);
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int column) {
        if (!m_filling && column == NumberColumn)
            m_renumber->start();
    });

    resize(960, 600);
    fill();
}

void ScriptCuesDialog::fill()
{
    m_filling = true;
    ShowTreeController *st = m_doc->showTree();
    m_plan = st->proposeScriptCues(m_production, m_list);
    m_tree->clear();
    m_rows.clear();
    QTreeWidgetItem *song = nullptr;
    QString songKey;
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    for (const ShowTreeController::ScriptCue &sc : m_plan)
    {
        QTreeWidgetItem *parent = nullptr;
        if (!sc.song.isEmpty())
        {
            if (song == nullptr || sc.song != songKey)
            {
                songKey = sc.song;
                song = new QTreeWidgetItem(m_tree);
                QString title = sc.songTitle.isEmpty() ? sc.song : QStringLiteral("%1 — %2").arg(sc.song, sc.songTitle);
                if (sc.newSong)
                    title += QLatin1Char(' ') + tr("(a new song, named from the script)");
                song->setText(NumberColumn, title);
                QFont f = song->font(NumberColumn);
                f.setBold(true);
                song->setFont(NumberColumn, f);
                song->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                song->setCheckState(NumberColumn, Qt::Checked);
                song->setFirstColumnSpanned(true);
                song->setExpanded(true);
            }
            parent = song;
        }
        QTreeWidgetItem *row = parent == nullptr ? new QTreeWidgetItem(m_tree) : new QTreeWidgetItem(parent);
        row->setData(NumberColumn, kLineRole, sc.direction.line);
        row->setText(NoteColumn, sc.direction.text);
        row->setToolTip(NoteColumn, sc.direction.text);
        row->setText(LineColumn, QString::number(sc.direction.line + 1));
        if (sc.cuedBy != invalidId())
        {
            row->setFlags(Qt::ItemIsEnabled);
            row->setText(NumberColumn, QStringLiteral("—"));
            row->setText(NameColumn, tr("Has a cue: GO %1").arg(sc.cuedAs));
            for (int c = 0; c < m_tree->columnCount(); ++c)
                row->setForeground(c, grey);
        }
        else
        {
            row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            row->setCheckState(NumberColumn, Qt::Checked);
            row->setText(NameColumn, sc.name);
            m_rows.insert(sc.direction.line, row);
        }
    }
    // A song whose every line has a cue has nothing to tick.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *s = m_tree->topLevelItem(i);
        if (s->childCount() == 0)
            continue;
        bool any = false;
        for (int k = 0; k < s->childCount(); ++k)
            any |= (s->child(k)->flags() & Qt::ItemIsUserCheckable) != 0;
        if (!any)
        {
            s->setFlags(Qt::ItemIsEnabled);
            s->setData(NumberColumn, Qt::CheckStateRole, QVariant());
        }
    }
    m_filling = false;
    renumber();
}

QSet<int> ScriptCuesDialog::leftOut() const
{
    QSet<int> out;
    for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it)
        if (it.value()->checkState(NumberColumn) == Qt::Unchecked)
            out.insert(it.key());
    return out;
}

void ScriptCuesDialog::setLineTicked(int line, bool ticked)
{
    QTreeWidgetItem *row = m_rows.value(line);
    if (row == nullptr)
        return;
    row->setCheckState(NumberColumn, ticked ? Qt::Checked : Qt::Unchecked);
    m_renumber->stop();
    renumber();
}

bool ScriptCuesDialog::isLineTicked(int line) const
{
    QTreeWidgetItem *row = m_rows.value(line);
    return row != nullptr && row->checkState(NumberColumn) == Qt::Checked;
}

int ScriptCuesDialog::toMake() const
{
    int n = 0;
    for (const ShowTreeController::ScriptCue &sc : m_plan)
        n += sc.made() ? 1 : 0;
    return n;
}

void ScriptCuesDialog::renumber()
{
    m_filling = true;
    m_plan = m_doc->showTree()->proposeScriptCues(m_production, m_list, leftOut());
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    const QColor text = palette().color(QPalette::Text);
    for (const ShowTreeController::ScriptCue &sc : m_plan)
    {
        QTreeWidgetItem *row = m_rows.value(sc.direction.line);
        if (row == nullptr)
            continue;
        if (sc.made())
            row->setText(NumberColumn, sc.number);
        else if (sc.noRoom)
            row->setText(NumberColumn, tr("no number free"));
        else
            row->setText(NumberColumn, QString());
        for (int c = NameColumn; c < m_tree->columnCount(); ++c)
            row->setForeground(c, sc.made() ? text : grey);
    }
    m_filling = false;
    updateSummary();
}

void ScriptCuesDialog::updateSummary()
{
    int cued = 0, noRoom = 0;
    for (const ShowTreeController::ScriptCue &sc : m_plan)
    {
        cued += sc.cuedBy != invalidId() ? 1 : 0;
        noRoom += sc.noRoom ? 1 : 0;
    }
    const int make = toMake();
    QStringList parts;
    if (m_plan.isEmpty())
        parts << tr("The script has no SCENE: or LIGHTING: lines.");
    else
        parts << (make == 1 ? tr("1 cue to make") : tr("%1 cues to make").arg(make));
    if (cued > 0)
        parts << (cued == 1 ? tr("1 line already has a cue (skipped)")
                            : tr("%1 lines already have a cue (skipped)").arg(cued));
    if (noRoom > 0)
        parts << (noRoom == 1 ? tr("1 line has no number free: renumber its song")
                              : tr("%1 lines have no number free: renumber their song").arg(noRoom));

    // Cues not on the script yet: new ones go after them in their song.
    const ShowTreeController *st = m_doc->showTree();
    const CueList *list = st->cueList(m_list);
    if (list != nullptr && make > 0)
    {
        const CallingScript script = st->callingScript(m_production);
        int off = 0;
        for (const Cue &c : list->cues)
        {
            const ScriptAnchor *a = script.anchor(c.id);
            off += a == nullptr || a->go < 0 ? 1 : 0;
        }
        if (off > 0)
            parts << (off == 1 ? tr("1 cue of %1 is not on the script yet: new cues in its song go after it "
                                    "(drag it onto its line first to number around it)").arg(list->name)
                               : tr("%1 cues of %2 are not on the script yet: new cues in their song go after "
                                    "them (drag them onto their lines first to number around them)")
                                     .arg(off).arg(list->name));
    }
    m_summary->setText(parts.join(QStringLiteral(" · ")));
    m_make->setText(make == 1 ? tr("Make 1 Cue") : make > 1 ? tr("Make %1 Cues").arg(make) : tr("Make Cues"));
    m_make->setEnabled(make > 0);
}

void ScriptCuesDialog::accept()
{
    m_message.clear();
    m_made = m_doc->showTree()->makeScriptCues(m_production, m_list, leftOut(), &m_message);
    if (m_made.isEmpty())
    {
        m_summary->setText(m_message);
        return;
    }
    QDialog::accept();
}
