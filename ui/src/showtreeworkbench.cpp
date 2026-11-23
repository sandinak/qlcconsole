/*
  Q Light Controller Plus
  showtreeworkbench.cpp

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
 * W3: Show Tree <-> Workbench (docs/design/WORKFLOW_DESIGN.md §3.4, §4 W3).
 *
 * "Open in Workbench" on a cue, a look or a layer (✎, double-click, the
 * `wb` command) docks the Workbench -- Build's own, lent -- in place of
 * the tree's page: Library | Tree | Workbench. A TreeCueStack is loaded: the
 * cue's layers and what tracks in, every edit a tree step. The tree beside
 * it stays live: click another cue and the Workbench follows. ↩ Show Tree
 * puts the page back with the cue (and the layer the Workbench had
 * selected) selected, and hands the Workbench back to Build.
 *
 * The tree's own cue and layer pages are lent to the Workbench's editor
 * pane, so every attribute keeps its ⋯ menu, Release, Make reusable and the
 * line strip.
 */

#include <QStackedWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QLabel>
#include <QTimer>

#include "showtreeview.h"
#include "showtreecontroller.h"
#include "monitor/monitor.h"
#include "lookeditor.h"
#include "effectdesigner.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "workbench.h"
#include "treecuestack.h"
#include "function.h"
#include "doc.h"

using namespace ShowTree;

struct ShowTreeView::WorkbenchDock
{
    std::function<Workbench *()> borrow;
    std::function<void(Workbench *)> giveBack;
    QPointer<Workbench> bench;          //!< docked here now
    QPointer<Workbench> own;            //!< the tree's own (no lender)
    bool docked = false;
    quint32 cue = 0xFFFFFFFF;
    QList<int> sizes;                   //!< the splitter as it was
    bool inspectorLent = false;
    bool syncing = false;               //!< the tree and the Workbench selecting each other
    QList<QMetaObject::Connection> wiring;
    // Left with the tab (docked): put back as it was on the way in.
    bool suspended = false;
    quint32 resumeLayer = 0xFFFFFFFF;
    QString resumeSignature;
};

static QSplitter *mainSplitter(ShowTreeView *v)
{
    return v->findChild<QSplitter *>(QStringLiteral("stSplitter"));
}

static QSplitter *pageSplitter(ShowTreeView *v)
{
    return v->findChild<QSplitter *>(QStringLiteral("stVSplitter"));
}

void ShowTreeView::setWorkbenchLender(const std::function<Workbench *()> &borrow,
                                      const std::function<void(Workbench *)> &giveBack)
{
    if (m_wb == nullptr)
        m_wb = new WorkbenchDock;
    m_wb->borrow = borrow;
    m_wb->giveBack = giveBack;
}

bool ShowTreeView::isWorkbenchDocked() const
{
    return m_wb != nullptr && m_wb->docked && !m_wb->bench.isNull();
}

Workbench *ShowTreeView::dockedWorkbench() const
{
    return isWorkbenchDocked() ? m_wb->bench.data() : nullptr;
}

bool ShowTreeView::openSelectionInWorkbench()
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
    {
        m_hint->setText(tr("Select a cue, a look or a layer to open it in the Workbench"));
        return false;
    }
    const NodeKind kind = currentKind();
    if (kind == LayerNode)
        return openInWorkbench(cueId, currentId());
    if (kind == TrackedLayerNode)
        return openInWorkbench(cueId, invalidId(), m_tree->currentItem()->data(0, TargetRole).toString());
    return openInWorkbench(cueId);
}

bool ShowTreeView::dockWorkbench()
{
    if (m_wb == nullptr)
        m_wb = new WorkbenchDock;
    m_wb->suspended = false;
    if (isWorkbenchDocked())
        return true;
    Workbench *bench = m_wb->borrow ? m_wb->borrow() : nullptr;
    if (bench == nullptr)
    {
        if (m_wb->own.isNull())
            m_wb->own = new Workbench(m_doc, this);
        bench = m_wb->own;
    }
    QSplitter *split = mainSplitter(this);
    QSplitter *vsplit = pageSplitter(this);
    if (split == nullptr || vsplit == nullptr)
        return false;
    m_wb->sizes = split->sizes();
    m_wb->bench = bench;
    m_wb->docked = true;
    // Library | Tree | Workbench: the page's place, and more of the room.
    vsplit->hide();
    split->addWidget(bench);
    bench->show();
    {
        const QList<int> was = m_wb->sizes;
        int total = 0;
        for (int s : was)
            total += s;
        if (total > 0 && was.size() >= 3)
            split->setSizes({ total * 17 / 100, total * 23 / 100, 0, total * 60 / 100 });
    }
    bench->setReturnTo(tr("Show Tree"), [this]() {
        // After the click is done with the button.
        QTimer::singleShot(0, this, [this]() { closeWorkbench(); });
    });

    // Wired while docked: a palette edited in the Workbench plays at
    // once (as the tree's Look Editor does), clicks on the 2D view pick
    // the layer that lights them.
    m_wb->wiring << connect(bench, &Workbench::paletteEdited, this, [this](quint32 pid) {
        paletteEdited(pid, true);
    });
    m_wb->wiring << connect(bench->lookEditor(), &LookEditor::paletteValueChanged, this, [this](quint32 pid) {
        paletteEdited(pid, false);
    });
    // The cue gone (deleted, undone, another show): the page back.
    auto gone = [this]() {
        QTimer::singleShot(0, this, [this]() {
            // A function opened here (no cue): gone when the function is.
            if (!isWorkbenchDocked())
                return;
            const bool lost = m_wb->cue == invalidId()
                ? m_doc->function(m_wb->bench->loadedFunction()) == nullptr
                : m_st.isNull() || m_st->cue(m_wb->cue) == nullptr;
            if (lost)
            {
                m_wb->suspended = true;     // nothing to reselect
                closeWorkbench();
                m_wb->suspended = false;
            }
        });
    };
    m_wb->wiring << connect(m_st, &ShowTreeController::changed, this, gone);
    m_wb->wiring << connect(m_doc, &Doc::cleared, this, gone);
    if (Monitor *mon = Monitor::instance())
        m_wb->wiring << connect(mon, &Monitor::fixturesSelected, this, [this](const QList<quint32> &ids) {
            Workbench *b = dockedWorkbench();
            TreeCueStack *cs = b != nullptr ? qobject_cast<TreeCueStack *>(b->stack()) : nullptr;
            if (cs == nullptr || ids.isEmpty() || !b->isVisible())
                return;
            const int layer = cs->layerOfFixture(ids.first());
            if (layer < 0)
                return;
            ProgrammingOutline::Row r;
            r.kind = ProgrammingOutline::CueLayerRow;
            r.id = quint32(layer);
            r.scene = 0xFFFFFFFF;
            b->selectRow(r);
        });
    return true;
}

bool ShowTreeView::openInWorkbench(quint32 cueId, quint32 layerId, const QString &trackedSignature)
{
    if (m_st.isNull() || m_st->cue(cueId) == nullptr)
        return false;
    if (!dockWorkbench())
        return false;

    Workbench *bench = m_wb->bench;
    m_wb->syncing = true;
    if (m_wb->cue != cueId || qobject_cast<TreeCueStack *>(bench->stack()) == nullptr
        || qobject_cast<TreeCueStack *>(bench->stack())->cueId() != cueId)
        bench->setStack(new TreeCueStack(m_doc, this, cueId));
    m_wb->cue = cueId;
    // The cue (or its layer) selected in the tree too.
    if (layerId != invalidId())
        selectNode(layerId, cueId);
    else if (!trackedSignature.isEmpty())
        selectTracked(cueId, trackedSignature);
    else if (currentCueId() != cueId)
        selectNode(cueId);
    if (TreeCueStack *cs = qobject_cast<TreeCueStack *>(bench->stack()))
    {
        const int index = cs->indexOfLayer(layerId, trackedSignature);
        if (index >= 0)
        {
            ProgrammingOutline::Row r;
            r.kind = ProgrammingOutline::CueLayerRow;
            r.id = quint32(index);
            r.scene = 0xFFFFFFFF;
            const QList<WorkbenchStack::Layer> ls = cs->layers();
            if (ls.at(index).kind == WorkbenchStack::Layer::FunctionLayer)
            {
                r.kind = ProgrammingOutline::FunctionRow;
                r.id = r.scene = ls.at(index).functionId;
            }
            if (!bench->selectRow(r))
                bench->rows()->setCurrentItem(bench->rows()->layerItem(index));
        }
    }
    m_wb->syncing = false;
    m_hint->setText(tr("%1 is in the Workbench: ↩ Show Tree puts the page back").arg(bench->stack()->title()));
    updateButtons();
    slotPreviewChanged();
    return true;
}

bool ShowTreeView::openFunctionInWorkbench(quint32 functionId)
{
    /* Board Q228: Build is not a tab of the qlcconsole view, so a scene,
       collection or chaser opened from anywhere opens here, in the
       Workbench docked in place of the page (as a docked Workbench's ▣ Open
       already did); ↩ Show Tree comes back. */
    const Function *f = m_doc->function(functionId);
    if (f == nullptr || !dockWorkbench())
        return false;
    Workbench *bench = m_wb->bench;
    m_wb->syncing = true;
    m_wb->cue = invalidId();
    bench->open(functionId);
    m_wb->syncing = false;
    m_hint->setText(tr("“%1” is open in the Workbench: ↩ Show Tree puts the page back").arg(f->name()));
    updateButtons();
    return bench->loadedFunction() == functionId;
}

bool ShowTreeView::closeWorkbench()
{
    if (!isWorkbenchDocked())
        return false;
    Workbench *bench = m_wb->bench;
    // What it had selected comes back selected in the tree.
    quint32 cueId = m_wb->cue, layerId = invalidId();
    QString signature;
    if (TreeCueStack *cs = qobject_cast<TreeCueStack *>(bench->stack()))
    {
        cueId = cs->cueId();
        const int layer = bench->rows()->layerOf(bench->rows()->currentItem());
        if (layer >= 0)
        {
            layerId = cs->layerIdAt(layer);
            if (layerId == invalidId())
                signature = cs->targetAt(layer).signature();
        }
    }
    m_wb->resumeLayer = layerId;
    m_wb->resumeSignature = signature;

    for (const QMetaObject::Connection &c : m_wb->wiring)
        disconnect(c);
    m_wb->wiring.clear();
    bench->setReturnTo(QString(), nullptr);
    bench->clear();                 // the page lent goes back with this
    takeBackInspector();
    m_wb->docked = false;
    m_wb->bench.clear();
    m_wb->cue = invalidId();

    QSplitter *split = mainSplitter(this);
    QSplitter *vsplit = pageSplitter(this);
    if (bench == m_wb->own)
    {
        bench->hide();
        bench->setParent(this);
    }
    else if (m_wb->giveBack)
        m_wb->giveBack(bench);
    if (vsplit != nullptr)
        vsplit->show();
    if (split != nullptr && !m_wb->sizes.isEmpty())
        split->setSizes(m_wb->sizes);

    if (!m_wb->suspended && cueId != invalidId())
    {
        if (layerId != invalidId())
            selectNode(layerId, cueId);
        else if (!signature.isEmpty())
            selectTracked(cueId, signature);
        else
            selectNode(cueId);
        updateInspector();
        syncPreview();
        m_tree->setFocus(Qt::OtherFocusReason);
    }
    updateButtons();
    return true;
}

/****************************************************************************
 * W4: the effect designer in the pane (board Q263, Q264). There is no modal
 * "Design effect" window any more: every door docks the Workbench and
 * puts the designer in its editor pane -- on a cue layer's own fixtures, or
 * on the bench for an effect that has no home yet.
 ****************************************************************************/

bool ShowTreeView::designEffectOnLayer(quint32 cueId, quint32 layerId, const Target &target,
                                       const QString &key)
{
    if (m_st.isNull() || m_st->cue(cueId) == nullptr)
        return false;
    // Another effect being designed and not kept: asked before it goes.
    if (Workbench *open = dockedWorkbench())
        if (open->keepsDesigning())
            return false;
    const Cue *cue = m_st->cue(cueId);
    // A layer carried into this cue has no id of its own here: its target
    // names it (a value kept on it makes it the cue's own, as the tree does).
    const bool own = cue->layer(layerId) != nullptr;
    const quint32 lid = own ? layerId : invalidId();
    const QString signature = own ? QString() : target.signature();
    if (!openInWorkbench(cueId, lid, signature))
        return false;
    Workbench *bench = dockedWorkbench();
    if (bench == nullptr || !bench->designEffectOn(cueId, lid, signature, key))
    {
        m_hint->setText(tr("Select the layer to design an effect for"));
        return false;
    }
    followDesignerIntoTheLibrary(qobject_cast<EffectDesigner *>(bench->effectPane()), false);
    m_hint->setText(tr("Designing the effect for %1: Keep on This Layer, or Save in This Show "
                       "(↩ Show Tree puts the page back)").arg(m_st->targetLabel(target)));
    updateButtons();
    return true;
}

void ShowTreeView::followDesignerIntoTheLibrary(EffectDesigner *designer, bool nameNewOnes)
{
    if (designer == nullptr)
        return;
    connect(designer, &EffectDesigner::effectChosen, this,
            [this, nameNewOnes](quint32 paletteId, const QList<quint32> &, EffectDesigner::Outcome outcome) {
        if (outcome == EffectDesigner::OneOff)
            return;             // a one-off is the cue's, never listed in All
        /* Deferred: the designer lets go of its hidden preview scene right
           after this, and a function leaving the show reloads the Library
           -- which would throw away the row we just opened to type in. */
        QTimer::singleShot(0, this, [this, paletteId, outcome, nameNewOnes]() {
            const QLCPalette *p = m_doc->palette(paletteId);
            if (p == nullptr)
                return;
            if (outcome == EffectDesigner::NewPalette && nameNewOnes)
            {
                // Saved under the effect's name: the name is open to type
                // over, as Build ▸ Effects opens it (no name box first).
                // reloadLibrary() opens it, so a reload of its own (the
                // palette added, the preview scene gone) cannot throw the
                // editor away first.
                m_hint->setText(tr("Saved in this show as “%1”: type its name").arg(p->name()));
                m_libRenameAfterReload = paletteId;
                reloadLibrary();
                /* The designer's preview scene leaves the show a moment
                   later and reloads the Library again, closing the editor
                   the first reload opened: open it once more when that has
                   settled (the pending id is spent by the first reload). */
                QTimer::singleShot(400, this, [this, paletteId]() {
                    if (m_doc->palette(paletteId) == nullptr || m_libRenaming)
                        return;
                    selectLibraryPalette(paletteId);
                    startLibraryRename();
                });
            }
            else
            {
                reloadLibrary();
                selectLibraryPalette(paletteId);
                paletteEdited(paletteId, true);
            }
        });
    });
}

QList<quint32> ShowTreeView::effectTryOnGroups() const
{
    QList<quint32> groups;
    if (m_st.isNull())
        return groups;
    auto add = [this, &groups](const Target &t) {
        for (quint32 g : t.groups)
            if (!groups.contains(g))
                groups << g;
        for (quint32 sid : t.sets)
            if (const GroupSet *gs = m_st->groupSet(sid))
                for (quint32 g : gs->groups)
                    if (!groups.contains(g))
                        groups << g;
    };
    const quint32 cueId = currentCueId();
    const Cue *cue = cueId == invalidId() ? nullptr : m_st->cue(cueId);
    if (cue == nullptr)
        return groups;
    // The selected layer's groups; else every layer of the selected cue, so
    // a new effect plays on something the person can see.
    if (currentKind() == LayerNode)
        if (const Layer *l = cue->layer(currentId()))
            add(l->target);
    if (groups.isEmpty())
        for (const Layer &l : cue->layers)
            add(l.target);
    return groups;
}

bool ShowTreeView::designEffectPalette(quint32 paletteId)
{
    const QLCPalette *p = m_doc->palette(paletteId);
    if (paletteId != QLCPalette::invalidId() && (p == nullptr || p->type() != QLCPalette::Effect))
        return false;           // every other type is the Look Editor's (Q264)
    // Another effect being designed and not kept: asked before it goes.
    if (Workbench *open = dockedWorkbench())
        if (open->keepsDesigning())
            return false;
    // Read before docking: docking moves the tree's selection about.
    const QList<quint32> tryOn = effectTryOnGroups();
    const quint32 cueId = currentCueId();
    const bool onCue = cueId != invalidId() && !m_st.isNull() && m_st->cue(cueId) != nullptr;
    if (onCue)
    {
        // Docked on the cue: the effect plays in context, and Make a Layer
        // can put it straight on it.
        const bool layer = currentKind() == LayerNode;
        if (!openInWorkbench(cueId, layer ? currentId() : invalidId()))
            return false;
    }
    else if (!dockWorkbench())
    {
        return false;
    }
    Workbench *bench = dockedWorkbench();
    if (bench == nullptr)
        return false;
    Workbench::EffectRequest req;
    req.palette = paletteId;
    req.fromCue = onCue ? cueId : invalidId();
    req.tryOn = tryOn;
    // Saved where the Library stands (the song's folder), as + Palette does.
    const QString where = libraryPaletteFolder();
    if (!where.isEmpty())
        req.folder = PaletteOps::pathForFolder(where);
    if (!bench->designEffect(req))
        return false;
    followDesignerIntoTheLibrary(qobject_cast<EffectDesigner *>(bench->effectPane()), true);
    m_hint->setText(p != nullptr
                    ? tr("Designing “%1”: Update it, or Save in This Show as a new one beside it "
                         "(↩ Show Tree puts the page back)").arg(p->name())
                    : tr("Designing a new effect: Try on picks the lights it plays on, Save in This Show "
                         "names and keeps it (↩ Show Tree puts the page back)"));
    updateButtons();
    return true;
}

void ShowTreeView::workbenchFollowTree()
{
    if (!isWorkbenchDocked() || m_wb->syncing || m_reloading)
        return;
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
        return;
    /* An effect is being designed in the pane: the cue must not change
       under it without a word (board Q263). Keep designing puts the tree
       back where the Workbench is. */
    if (!m_wb->bench.isNull() && m_wb->bench->keepsDesigning())
    {
        selectForWorkbench(m_wb->cue, invalidId(), QString());
        return;
    }
    const NodeKind kind = currentKind();
    if (kind == LayerNode)
        openInWorkbench(cueId, currentId());
    else if (kind == TrackedLayerNode)
        openInWorkbench(cueId, invalidId(), m_tree->currentItem()->data(0, TargetRole).toString());
    else if (cueId != m_wb->cue)
        openInWorkbench(cueId);
}

QWidget *ShowTreeView::lendInspector(quint32 cueId, quint32 layerId, const QString &signature)
{
    if (!isWorkbenchDocked())
        return nullptr;
    // The page follows the tree's selection: select what it is to show.
    selectForWorkbench(cueId, layerId, signature);
    updateInspector();
    m_wb->inspectorLent = true;
    return m_pages;
}

void ShowTreeView::takeBackInspector()
{
    if (m_wb == nullptr || !m_wb->inspectorLent)
        return;
    m_wb->inspectorLent = false;
    if (QSplitter *vsplit = pageSplitter(this))
    {
        vsplit->insertWidget(0, m_pages);
        m_pages->show();
    }
}

void ShowTreeView::suspendWorkbench()
{
    // Leaving the tab: Build gets its Workbench back; coming back docks it
    // again, as it was.
    if (!isWorkbenchDocked() || !m_wb->borrow)
        return;
    const quint32 cueId = m_wb->cue;
    m_wb->suspended = true;
    closeWorkbench();
    m_wb->cue = cueId;
}

void ShowTreeView::resumeWorkbench()
{
    if (m_wb == nullptr || !m_wb->suspended)
        return;
    m_wb->suspended = false;
    const quint32 cueId = m_wb->cue;
    m_wb->cue = invalidId();
    if (!m_st.isNull() && m_st->cue(cueId) != nullptr)
        openInWorkbench(cueId, m_wb->resumeLayer, m_wb->resumeSignature);
}

void ShowTreeView::destroyWorkbenchDock()
{
    if (m_wb == nullptr)
        return;
    if (isWorkbenchDocked())
    {
        m_wb->suspended = true;      // nothing to reselect on the way out
        closeWorkbench();
    }
    delete m_wb;
    m_wb = nullptr;
}

void ShowTreeView::selectForWorkbench(quint32 cueId, quint32 layerId, const QString &signature)
{
    if (!isWorkbenchDocked() || m_wb->syncing)
        return;
    m_wb->syncing = true;
    if (layerId != invalidId())
        selectNode(layerId, cueId);
    else if (!signature.isEmpty())
        selectTracked(cueId, signature);
    else
        selectNode(cueId);
    m_wb->syncing = false;
}
