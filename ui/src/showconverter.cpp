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
#include <QLabel>

#include "showconverter.h"
#include "virtualconsole.h"
#include "importsheet.h"
#include "vccuelist.h"
#include "vcframe.h"
#include "function.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const int kChaserRole = Qt::UserRole + 1;
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

ShowConverter::ShowConverter(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("stShowConverter"));
    setWindowTitle(tr("Convert an old show"));
    setProperty("helpTopic", QStringLiteral("show-tree.md#an-old-show-convert-it"));
    resize(820, 640);

    QVBoxLayout *root = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("Each chaser, by how it runs. One that waits for GO (notes, cue numbers, a VC Cue "
                                  "List, show control) is a cue list: tick it to make it a Show Tree cue list. One "
                                  "that loops on its own — a strobe, a chase, a sweep — is an effect and stays a "
                                  "function: it plays as a ▣ layer wherever you drop it. Nothing changes until you "
                                  "press Convert, and each chaser is one undo step."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    m_table = new QTreeWidget(this);
    m_table->setObjectName(QStringLiteral("stConverterTable"));
    m_table->setRootIsDecorated(false);
    m_table->setHeaderLabels({ tr("Chaser"), tr("Used as"), tr("Plan"), tr("Plays the same") });
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    root->addWidget(m_table, 1);

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
    m_convertButton = buttons->addButton(tr("Convert"), QDialogButtonBox::AcceptRole);
    m_convertButton->setObjectName(QStringLiteral("stConverterConvert"));
    QPushButton *close = buttons->addButton(QDialogButtonBox::Close);
    close->setObjectName(QStringLiteral("stConverterClose"));
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_convertButton, &QPushButton::clicked, this, [this]() { convert(); });
    root->addWidget(buttons);

    connect(m_table, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr)
            select(item->data(0, kChaserRole).toUInt());
    });
    connect(m_table, &QTreeWidget::itemChanged, this, [this]() { updateButtons(); });

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
            item->setText(3, r.plan.differChannels == 0
                                 ? tr("✓ 0 channels differ")
                                 : tr("⚠ %n channel(s) differ", "", r.plan.differChannels));
        }
        else
        {
            item->setText(2, tr("stays a function"));
            item->setText(3, QString());
            item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        }
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
        select(first->data(0, kChaserRole).toUInt());
    }
    else
    {
        m_details->setHtml(QStringLiteral("<p>%1</p>").arg(tr("No chasers left to convert.").toHtmlEscaped()));
    }
    updateButtons();
}

void ShowConverter::updateButtons()
{
    m_convertButton->setEnabled(!chosen().isEmpty());
}

QList<quint32> ShowConverter::chosen() const
{
    QList<quint32> out;
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        const QTreeWidgetItem *item = m_table->topLevelItem(i);
        if ((item->flags() & Qt::ItemIsUserCheckable) && item->checkState(0) == Qt::Checked)
            out << item->data(0, kChaserRole).toUInt();
    }
    return out;
}

void ShowConverter::setChosen(quint32 chaserId, bool on)
{
    for (int i = 0; i < m_table->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *item = m_table->topLevelItem(i);
        if (item->data(0, kChaserRole).toUInt() == chaserId && (item->flags() & Qt::ItemIsUserCheckable))
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
        QString h;
        if (!m_results.isEmpty())
        {
            h += QStringLiteral("<h3>%1</h3><ul>").arg(tr("Converted").toHtmlEscaped());
            for (const QString &l : m_results)
                h += QStringLiteral("<li>%1</li>").arg(l.toHtmlEscaped());
            h += QStringLiteral("</ul><hr>");
        }
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
    out << m_details->toPlainText();
    return out.join(QStringLiteral("\n"));
}
