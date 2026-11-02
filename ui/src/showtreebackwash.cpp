/*
  Q Light Controller Plus
  showtreebackwash.cpp

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

/* Backwash in the Show Tree (Show Review PWR-12): each cue that puts light
   on the cyc (or a scrim, or any scenery kept for light on purpose) that
   nobody meant is marked where you work -- "⚠ cyc" in its flags, the
   fixtures in the tooltip -- and a click on the mark opens the Show Review
   row, with I Meant It and Show Me. A layer marked meant offers to take it
   back from its right-click.

   The marks are read from Show Review's last result: it runs a moment after
   the show changes, in Design (App's review timer), never on the GO path. */

#include <QTreeWidgetItemIterator>
#include <QTreeWidget>
#include <QAction>
#include <QMenu>
#include <QBrush>
#include <QHash>

#include "showtreeview.h"
#include "showreviewpanel.h"
#include "showreview.h"
#include "monitorproperties.h"
#include "scenery.h"
#include "fixture.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
/* A cue row's own flags text and tooltip, before the mark: buildTree() makes
   new rows, so a row without these has not been marked yet. */
const int BaseFlagsRole = Qt::UserRole + 40;
const int BaseTipRole = Qt::UserRole + 41;
const int BackwashKeyRole = Qt::UserRole + 42;
const int BaseColourRole = Qt::UserRole + 43;
}

void ShowTreeView::setupBackwashMarks()
{
    if (m_doc == nullptr || m_doc->showReview() == nullptr)
        return;
    connect(m_doc->showReview(), &ShowReview::reviewed, this, &ShowTreeView::applyBackwashMarks);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
        if (item == nullptr || column != FlagsCol || item->data(0, KindRole).toInt() != CueNode)
            return;
        if (!item->data(FlagsCol, BackwashKeyRole).toString().isEmpty())
            openBackwashFinding(item->data(0, IdRole).toUInt());
    });
}

void ShowTreeView::applyBackwashMarks()
{
    if (m_doc == nullptr || m_doc->showReview() == nullptr || m_tree == nullptr)
        return;
    // Cue -> the open PWR-12 findings breaching in it, in Show Review's order.
    QHash<quint32, QList<int>> byCue;
    const QList<ReviewFinding> &found = m_doc->showReview()->lastResult().findings;
    for (int i = 0; i < found.count(); ++i)
    {
        const ReviewFinding &f = found.at(i);
        if (f.check != QLatin1String("PWR-12") || !f.isOpen())
            continue;
        for (quint32 cue : ShowReview::findingCues(f))
            byCue[cue] << i;
    }
    MonitorProperties *mp = m_doc->monitorProperties();
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->data(0, KindRole).toInt() != CueNode)
            continue;
        if (!item->data(FlagsCol, BaseFlagsRole).isValid())
        {
            item->setData(FlagsCol, BaseFlagsRole, item->text(FlagsCol));
            item->setData(FlagsCol, BaseTipRole, item->toolTip(FlagsCol));
            item->setData(FlagsCol, BaseColourRole, item->foreground(FlagsCol));
        }
        const QString base = item->data(FlagsCol, BaseFlagsRole).toString();
        const QString baseTip = item->data(FlagsCol, BaseTipRole).toString();
        const QList<int> hits = byCue.value(item->data(0, IdRole).toUInt());
        if (hits.isEmpty())
        {
            if (!item->data(FlagsCol, BackwashKeyRole).toString().isEmpty())
            {
                item->setText(FlagsCol, base);
                item->setToolTip(FlagsCol, baseTip);
                item->setData(FlagsCol, BackwashKeyRole, QString());
                item->setForeground(FlagsCol, item->data(FlagsCol, BaseColourRole).value<QBrush>());
            }
            continue;
        }
        // Which fixtures, on what: from the findings' targets.
        QStringList fixtures, surfaces;
        for (int i : hits)
        {
            for (const QString &t : found.at(i).targets)
            {
                const quint32 id = t.mid(t.indexOf(QLatin1Char(':')) + 1).toUInt();
                if (t.startsWith(QLatin1String("fx:")))
                {
                    const Fixture *fx = m_doc->fixture(id);
                    const QString n = fx != nullptr ? fx->name() : t;
                    if (!fixtures.contains(n))
                        fixtures << n;
                }
                else if (t.startsWith(QLatin1String("scenery:")) && mp != nullptr)
                {
                    const Scenery *sc = mp->scenery(id);
                    const QString n = sc != nullptr ? sc->name() : t;
                    if (!surfaces.contains(n))
                        surfaces << n;
                }
            }
        }
        const QString mark = tr("⚠ cyc");
        item->setText(FlagsCol, base.isEmpty() ? mark : base + QStringLiteral(", ") + mark);
        item->setForeground(FlagsCol, QColor(0xd0, 0x8a, 0x1c));
        const QString tip = tr("Lands on the cyc — not marked as meant: %1 (on %2). Click to open it in "
                               "Show Review: I Meant It, or Show Me.")
                            .arg(fixtures.join(QStringLiteral(", ")), surfaces.join(QStringLiteral(", ")));
        item->setToolTip(FlagsCol, baseTip.isEmpty() ? tip : baseTip + QStringLiteral("\n\n") + tip);
        item->setData(FlagsCol, BackwashKeyRole, found.at(hits.first()).key());
    }
}

QString ShowTreeView::backwashFindingKey(quint32 cueId) const
{
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == CueNode && (*it)->data(0, IdRole).toUInt() == cueId)
            return (*it)->data(FlagsCol, BackwashKeyRole).toString();
    return QString();
}

bool ShowTreeView::openBackwashFinding(quint32 cueId)
{
    const QString key = backwashFindingKey(cueId);
    if (key.isEmpty())
        return false;
    ShowReviewPanel *panel = ShowReviewPanel::openShared();
    if (panel == nullptr)
        return false;
    panel->setFilter(ShowReviewPanel::OpenOnly);
    panel->setSearch(QString());
    return panel->selectFinding(key);
}

void ShowTreeView::backwashLayerMenu(QMenu *menu, quint32 cueId, quint32 layerId)
{
    const Cue *cue = m_st->cue(cueId);
    const Layer *layer = cue != nullptr ? cue->layer(layerId) : nullptr;
    if (layer == nullptr || layer->meantOnScenery.isEmpty())
        return;
    QAction *a = menu->addAction(tr("Light on the Cyc: Not Meant"), this, [this, cueId, layerId]() {
        if (ShowReview::clearLayerMeant(m_doc, cueId, layerId))
            m_doc->showReview()->run();
    });
    a->setObjectName(QStringLiteral("stBackwashNotMeant"));
    a->setToolTip(tr("Take back I Meant It for this layer: Show Review warns again where its light "
                     "lands on the cyc"));
}
