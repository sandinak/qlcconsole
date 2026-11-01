/*
  Q Light Controller Plus
  showtreeplay.cpp

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

/*
 * Play / Stop and Solo on the Show Tree's own cue and layer pages.
 *
 * Until 0.4.0 they were only in the Workbench docked in the tree (W3,
 * 4984db0be): a cue opened on its page -- the usual way in -- had neither,
 * so they seemed to be gone. Here they are on both pages, the same
 * pair as the docked Workbench's:
 *
 *  - Play previews the selected cue (a layer page: the cue, the layer in
 *    context); Stop holds it until Play, or until another cue is picked.
 *  - Solo plays only the selected layer of the cue (and its child layers),
 *    as the pad walk's Solo does (ShowTreeController::setPreviewSolo).
 *    Moving to another layer of the cue solos that one; leaving the cue
 *    turns Solo off.
 *  - Operate: only the run is live, the tree is blind -- both greyed, saying
 *    why.
 *
 * Docked, the pages are lent to the Workbench, which has its own pair: the
 * pages' are hidden there.
 */

#include <QHBoxLayout>
#include <QToolButton>
#include <QPointer>
#include <utility>

#include "showtreeview.h"
#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

QWidget *ShowTreeView::makePageRig(QWidget *parent, const QString &prefix)
{
    QWidget *row = new QWidget(parent);
    row->setObjectName(prefix + QStringLiteral("Rig"));
    QHBoxLayout *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);

    QToolButton *play = new QToolButton(row);
    play->setObjectName(prefix + QStringLiteral("Play"));
    play->setText(tr("Play"));
    play->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(play, &QToolButton::clicked, this, &ShowTreeView::pagePlayClicked);
    h->addWidget(play);

    QToolButton *solo = new QToolButton(row);
    solo->setObjectName(prefix + QStringLiteral("Solo"));
    solo->setText(tr("Solo"));
    solo->setCheckable(true);
    solo->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(solo, &QToolButton::toggled, this, &ShowTreeView::pageSoloToggled);
    h->addWidget(solo);
    h->addStretch(1);

    m_pagePlay << play;
    m_pageSolo << solo;
    return row;
}

void ShowTreeView::updatePageRig()
{
    if (m_st.isNull() || m_tree == nullptr)
        return;
    const bool design = m_doc->mode() == Doc::Design;
    const quint32 cueId = currentCueId();
    const bool docked = isWorkbenchDocked();
    const ShowTreeController::PreviewState state = m_st->previewState();
    const bool playing = cueId != invalidId() && m_st->previewedCue() == cueId
                         && (state == ShowTreeController::PreviewState::Playing
                             || state == ShowTreeController::PreviewState::Dark);
    const bool solo = cueId != invalidId()
                      && (m_soloCue == cueId
                          || (m_st->previewSoloLayer() != invalidId() && m_st->previewedCue() == cueId));
    const bool onLayer = currentKind() == LayerNode || currentKind() == TrackedLayerNode;
    const QString operate = tr("Operate: only the run is live — editing here is blind and reaches the "
                               "stage on GO. Switch to Design to preview.");

    for (QToolButton *play : std::as_const(m_pagePlay))
    {
        play->parentWidget()->setVisible(!docked);
        play->setEnabled(design && cueId != invalidId());
        play->setText(playing ? tr("Stop") : tr("Play"));
        if (!design)
            play->setToolTip(operate);
        else if (playing)
            play->setToolTip(tr("Stop the preview: it stays off until Play, or until you pick another cue"));
        else if (state == ShowTreeController::PreviewState::Held)
            play->setToolTip(tr("Preview this cue over the running show (the edit wins on the channels it "
                                "plays), until the next GO"));
        else
            play->setToolTip(onLayer ? tr("Play this cue on the rig, this layer in context")
                                     : tr("Play this cue on the rig"));
    }
    for (QToolButton *button : std::as_const(m_pageSolo))
    {
        button->setEnabled(design && cueId != invalidId());
        QSignalBlocker block(button);
        button->setChecked(solo);
        if (!design)
            button->setToolTip(operate);
        else
            button->setToolTip(onLayer ? tr("Solo: only this layer plays. Select another layer of the cue "
                                            "and it plays alone instead; leaving the cue turns Solo off. "
                                            "Off: the whole cue plays, in context.")
                                       : tr("Solo: select a layer of this cue and only it plays; leaving the "
                                            "cue turns Solo off. Off: the whole cue plays, in context."));
    }
}

void ShowTreeView::pagePlayClicked()
{
    const quint32 cueId = currentCueId();
    if (m_doc->mode() != Doc::Design || cueId == invalidId() || isWorkbenchDocked())
        return;
    const ShowTreeController::PreviewState state = m_st->previewState();
    const bool playing = m_st->previewedCue() == cueId
                         && (state == ShowTreeController::PreviewState::Playing
                             || state == ShowTreeController::PreviewState::Dark);
    if (playing)
    {
        m_previewStopped = true;
    }
    else
    {
        commitPendingCueEdits();    // a time typed and not yet left plays too
        m_previewStopped = false;
        m_lastPickedCue = cueId;
        m_st->pickCue(cueId);       // the latest action wins (a hold by GO lets go)
    }
    syncPreview();
}

void ShowTreeView::pageSoloToggled(bool on)
{
    const quint32 cueId = currentCueId();
    if (m_doc->mode() != Doc::Design || cueId == invalidId() || isWorkbenchDocked())
    {
        updatePageRig();
        return;
    }
    m_soloCue = on ? cueId : invalidId();
    if (!on && m_st->previewSoloLayer() != invalidId())
        m_soloSet = true;           // the pad walk's Solo, turned off here
    if (on)
        m_previewStopped = false;   // Solo is asking to hear it
    syncPreview();
}

void ShowTreeView::syncSolo(quint32 cueId)
{
    // Leaving the cue drops Solo.
    if (m_soloCue != invalidId() && m_soloCue != cueId)
        m_soloCue = invalidId();
    if (m_soloCue == invalidId())
    {
        if (m_soloSet)
        {
            m_soloSet = false;
            m_st->setPreviewSolo(invalidId(), invalidId());
        }
        return;
    }
    const NodeKind kind = currentKind();
    const quint32 layerId = kind == LayerNode || kind == TrackedLayerNode ? currentId() : invalidId();
    m_soloSet = true;
    m_st->setPreviewSolo(m_soloCue, layerId);
}
