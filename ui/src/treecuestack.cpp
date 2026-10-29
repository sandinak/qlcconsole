/*
  Q Light Controller Plus
  treecuestack.cpp

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

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QSharedPointer>
#include <QMimeData>
#include <QBuffer>
#include <QMenu>
#include <QSet>

#include "refinepanel.h"
#include "focusmenu.h"
#include "treecuestack.h"
#include "showtreecontroller.h"
#include "functionparent.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "rgbmatrix.h"
#include "function.h"
#include "scene.h"
#include "efx.h"
#include "doc.h"

using namespace ShowTree;

/** Solo plays under its own parent: the tree's preview letting go of the
 *  same scene doesn't stop it, and stopping it stops nothing else. */
static FunctionParent soloParent()
{
    return FunctionParent::preview(0x5);
}

static QString seconds(uint ms)
{
    return QStringLiteral("%1s").arg(QString::number(ms / 1000.0, 'f', ms % 1000 == 0 ? 0 : 1));
}

TreeCueStack::TreeCueStack(Doc *doc, ShowTreeView *view, quint32 cueId, QObject *parent)
    : WorkbenchStack(doc, parent)
    , m_view(view)
    , m_cue(cueId)
{
    // Every tree edit (here, in the tree, typed, undone): the rows again.
    if (ShowTreeController *c = st())
    {
        connect(c, &ShowTreeController::changed, this, &WorkbenchStack::changed);
        connect(c, &ShowTreeController::previewChanged, this, &WorkbenchStack::changed);
    }
}

TreeCueStack::~TreeCueStack()
{
    stopSolo();
}

ShowTreeController *TreeCueStack::st() const
{
    return m_doc != nullptr ? m_doc->showTree() : nullptr;
}

const CueList *TreeCueStack::list() const
{
    return st() != nullptr ? st()->listOfCue(m_cue) : nullptr;
}

const Cue *TreeCueStack::cue() const
{
    return st() != nullptr ? st()->cue(m_cue) : nullptr;
}

QList<TreeCueStack::Row> TreeCueStack::rows(ResolvedCue *resolved) const
{
    QList<Row> out;
    const CueList *l = list();
    const Cue *c = cue();
    if (l == nullptr || c == nullptr)
        return out;
    ResolvedCue rc;
    if (!st()->resolvedCue(m_cue, &rc))
        return out;
    QSet<QString> seen;
    for (const ResolvedLayer &rl : rc.layers)
    {
        Row r;
        r.target = rl.target;
        if (rl.own)
            for (const ShowTree::Layer &own : c->layers)
                if (own.target == rl.target)
                    r.layerId = own.id;
        seen.insert(rl.target.signature());
        out << r;
    }
    // Its own layers that put nothing on stage here: still its own.
    for (const ShowTree::Layer &own : c->layers)
        if (!seen.contains(own.target.signature()))
        {
            Row r;
            r.layerId = own.id;
            r.target = own.target;
            out << r;
        }
    if (resolved != nullptr)
        *resolved = rc;
    return out;
}

quint32 TreeCueStack::layerIdAt(int index) const
{
    const QList<Row> rs = rows();
    return index >= 0 && index < rs.size() ? rs.at(index).layerId : ShowTree::invalidId();
}

Target TreeCueStack::targetAt(int index) const
{
    const QList<Row> rs = rows();
    return index >= 0 && index < rs.size() ? rs.at(index).target : Target();
}

int TreeCueStack::indexOfLayer(quint32 layerId, const QString &signature) const
{
    const QList<Row> rs = rows();
    for (int i = 0; i < rs.size(); ++i)
        if ((layerId != ShowTree::invalidId() && rs.at(i).layerId == layerId)
            || (layerId == ShowTree::invalidId() && !signature.isEmpty()
                && rs.at(i).layerId == ShowTree::invalidId() && rs.at(i).target.signature() == signature))
            return i;
    return -1;
}

/****************************************************************************
 * What it is
 ****************************************************************************/

QString TreeCueStack::key() const
{
    return QStringLiteral("cue:%1").arg(m_cue);
}

WorkbenchStack *TreeCueStack::reopen(QObject *parent) const
{
    return cue() != nullptr ? new TreeCueStack(m_doc, m_view, m_cue, parent) : nullptr;
}

QString TreeCueStack::title() const
{
    const CueList *l = list();
    if (l == nullptr)
        return tr("(gone)");
    return st()->cueLabel(*l, l->indexOf(m_cue));
}

QString TreeCueStack::titleColour() const
{
    return st() == nullptr ? QString() : st()->cueColour(m_cue);
}

QString TreeCueStack::kindText() const
{
    if (st() == nullptr)
        return QString();
    if (st()->isLook(m_cue))
        return tr("Look");
    if (st()->isSubCue(m_cue))
        return tr("Sub-cue");
    return tr("Cue");
}

QString TreeCueStack::where() const
{
    const CueList *l = list();
    if (l == nullptr)
        return QString();
    quint32 prod = ShowTree::invalidId();
    st()->cueList(l->id, &prod);
    const Production *p = st()->production(prod);
    QString name = l->name;
    if (l->looks)
        name = tr("Hits");
    else if (l->isSubCues())
        if (const CueList *parentList = st()->listOfCue(l->subOf))
            name = QStringLiteral("%1/%2").arg(parentList->name,
                                               st()->cueLabel(*parentList, parentList->indexOf(l->subOf)));
    return p != nullptr ? QStringLiteral("Show Tree/%1/%2").arg(p->name, name)
                        : QStringLiteral("Show Tree/%1").arg(name);
}

QStringList TreeCueStack::infoLines() const
{
    QStringList lines;
    const CueList *l = list();
    const Cue *c = cue();
    if (l == nullptr || c == nullptr)
        return lines;

    // Mode and timing, as the cue page has them.
    QStringList head;
    if (!l->looks)
    {
        const CueMode mode = effectiveMode(*l, *c);
        const QString m = mode == CueMode::Track ? tr("Track") : mode == CueMode::Block ? tr("Block")
                        : mode == CueMode::CueOnly ? tr("Cue-only") : tr("List default");
        head << (c->mode == CueMode::ListDefault ? tr("%1 (the list's)").arg(m) : m);
    }
    head << tr("fade in %1").arg(seconds(c->fadeIn)) << tr("out %1").arg(seconds(c->fadeOut));
    if (!l->looks)
        head << (c->hold == Function::infiniteSpeed() ? tr("hold ∞ (waits for GO)")
                                                      : tr("hold %1").arg(seconds(c->hold)));
    lines << head.join(QStringLiteral(" · "));

    int own = 0, carried = 0;
    ResolvedCue rc;
    const QList<Row> rs = rows(&rc);
    for (const Row &r : rs)
        (r.layerId != ShowTree::invalidId() ? own : carried)++;
    QString layersLine = tr("%n own layer(s)", "", own);
    if (carried > 0)
        layersLine += QStringLiteral(" · ") + tr("%n carried in (grey)", "", carried);
    if (!c->looks.isEmpty())
        layersLine += QStringLiteral(" · ") + tr("%n cue look(s)", "", int(c->looks.size()));
    lines << layersLine;

    if (!c->includes.isEmpty())
    {
        QStringList inc;
        for (quint32 id : c->includes)
            if (const CueList *il = st()->listOfCue(id))
                inc << st()->cueLabel(*il, il->indexOf(id));
        lines << tr("Includes: %1").arg(inc.join(QStringLiteral(", ")));
    }
    if (c->mixOverlap)
        lines << tr("≈ Layers mix where they overlap (as its chaser did)");
    if (c->skip)
        lines << tr("Skipped: GO passes over it");
    if (c->reusable != ShowTree::invalidId())
        lines << tr("⟲ A use of the reusable cue “%1”: an edit changes every use")
                     .arg(st()->reusableName(c->reusable));
    if (!c->note.isEmpty())
        lines << tr("Note: %1").arg(c->note);
    if (l->looks && m_view != nullptr)
    {
        const QStringList by = m_view->lookFiredBy(m_cue);
        lines << (by.isEmpty() ? tr("Fired by: nothing on the Virtual Console")
                               : tr("Fired by: %1").arg(by.join(QStringLiteral(", "))));
    }
    return lines;
}

quint32 TreeCueStack::previewFunction() const
{
    const Cue *c = cue();
    return c != nullptr ? c->collection : Function::invalidId();
}

QList<WorkbenchStack::Layer> TreeCueStack::layers() const
{
    QList<Layer> out;
    const CueList *l = list();
    if (l == nullptr)
        return out;
    ResolvedCue rc;
    const QList<Row> rs = rows(&rc);
    for (const Row &r : rs)
    {
        Layer layer;
        layer.cueId = m_cue;
        layer.layerId = r.layerId;
        layer.targetSignature = r.target.signature();
        layer.tracked = r.layerId == ShowTree::invalidId();
        const ResolvedLayer *rl = rc.layer(r.target);
        QString name = rl != nullptr && !rl->name.isEmpty() ? rl->name : st()->targetLabel(r.target);
        if (rl != nullptr && rl->depth > 0)
            name = QString(rl->depth * 2, QLatin1Char(' ')) + QStringLiteral("↳ ") + name;
        layer.label = name;

        QStringList badge, tip;
        if (layer.tracked && rl != nullptr)
        {
            const QString from = m_view != nullptr ? m_view->carriedOrigin(rl->origin, rl->fromCue)
                                                   : tr("carried in");
            badge << QStringLiteral("⤳ ") + from;
            tip << tr("Carried into this cue: %1. Grey: set elsewhere. Drop a palette on it, or use a "
                      "value's menu, to set it here (it becomes this cue's own).").arg(from);
        }
        else if (!layer.tracked)
        {
            if (const Cue *c = cue())
                for (const ShowTree::Layer &ol : c->layers)
                    if (ol.id == r.layerId)
                    {
                        if (ol.release)
                            badge << tr("release");
                        if (ol.reusable != ShowTree::invalidId())
                        {
                            badge << QStringLiteral("⟲");
                            tip << tr("A use of the reusable layer “%1”: an edit changes every use.")
                                       .arg(st()->reusableName(ol.reusable));
                        }
                    }
            if (rl != nullptr && !rl->tookOver.isEmpty())
                tip << tr("Takes over %1 from the cue before.").arg(st()->targetLabel(rl->tookOver));
        }
        if (rl == nullptr)
        {
            badge << tr("not on stage");
            tip << tr("Puts nothing on stage in this cue.");
        }
        tip.prepend(st()->targetContents(r.target));

        if (r.target.isOpaque())
        {
            // Played as it is: a ▣ row that opens in the Workbench.
            layer.kind = Layer::FunctionLayer;
            layer.functionId = r.target.function;
            const Function *f = m_doc->function(r.target.function);
            if (f != nullptr)
                badge.prepend(tr("%1 ▣").arg(Function::typeToString(f->type())));
            tip << tr("Played as it is. ▣ Open it (double-click) to work on it here; ↩ comes back.");
        }
        else
        {
            layer.kind = Layer::CueLayer;
            layer.functionId = st()->layerScene(m_cue, r.target);
            if (rl != nullptr)
            {
                QStringList keys = rl->values.keys();
                std::sort(keys.begin(), keys.end(), keyLessThan);
                for (const QString &k : keys)
                {
                    const ResolvedValue &v = rl->values.value(k);
                    Layer::Attribute a;
                    a.key = k;
                    a.label = keyLabel(k);
                    a.value = isLineParamKey(k) && !v.look.none
                              ? ShowTreeController::lineParamText(k, v.look.value)
                              : m_view != nullptr ? m_view->lookText(v.look)
                              : v.look.none ? tr("off") : QString();
                    a.inherited = v.origin != Origin::Own;
                    if (a.inherited && m_view != nullptr)
                        a.origin = m_view->originText(*l, v);
                    a.palette = v.look.palettes.value(0, Function::invalidId());
                    layer.attributes << a;
                }
                if (keys.isEmpty())
                    badge << tr("dark");
            }
        }
        layer.badge = badge.join(QStringLiteral("  "));
        layer.tip = tip.join(QLatin1Char('\n'));
        out << layer;
    }
    return out;
}

/****************************************************************************
 * Edits: each one tree step (then a bake)
 ****************************************************************************/

bool TreeCueStack::canAddLayers() const
{
    return cue() != nullptr && m_view != nullptr;
}

bool TreeCueStack::canRemoveLayers() const
{
    return canAddLayers();
}

bool TreeCueStack::canReorderLayers() const
{
    return canAddLayers();
}

QString TreeCueStack::addPlan(const QList<quint32> &functionIds) const
{
    if (!canAddLayers())
        return QStringLiteral("!") + tr("The cue is gone.");
    QStringList names;
    for (quint32 fid : functionIds)
        if (const Function *f = m_doc->function(fid))
        {
            if (st()->isGenerated(fid))
                return QStringLiteral("!") + tr("“%1” is the Show Tree's own.").arg(f->name());
            names << f->name();
        }
    if (names.isEmpty())
        return QStringLiteral("!") + tr("Nothing to add.");
    return names.size() == 1 ? tr("Play “%1” as it is, a layer of %2").arg(names.first(), title())
                             : tr("Play %n function(s) as they are, layers of %1", "", int(names.size())).arg(title());
}

bool TreeCueStack::addLayers(const QList<quint32> &functionIds, int at)
{
    Q_UNUSED(at)        // a cue's new layer goes last, as in the tree
    if (addPlan(functionIds).startsWith(QLatin1Char('!')))
        return false;
    return m_view->dropFunctions(m_cue, functionIds);
}

bool TreeCueStack::removeLayers(const QList<int> &indexes)
{
    if (!canRemoveLayers())
        return false;
    const QList<Row> rs = rows();
    QList<quint32> going;
    for (int i : indexes)
        if (i >= 0 && i < rs.size() && rs.at(i).layerId != ShowTree::invalidId()
            && !going.contains(rs.at(i).layerId))
            going << rs.at(i).layerId;
    // A layer carried in isn't this cue's to take out: Release, or set its
    // values off, does that.
    if (going.isEmpty())
        return false;
    if (going.size() == 1)
        return st()->removeLayer(m_cue, going.first());
    st()->beginBatch(tr("remove %n layer(s) from %1", "", int(going.size())).arg(title()));
    bool ok = false;
    for (quint32 id : going)
        ok = st()->removeLayer(m_cue, id) || ok;
    st()->endBatch(ok);
    return ok;
}

bool TreeCueStack::moveLayer(int from, int to)
{
    if (!canReorderLayers())
        return false;
    const QList<Row> rs = rows();
    if (from < 0 || from >= rs.size())
        return false;
    to = qBound(0, to, int(rs.size()) - 1);
    // Only the cue's own layers move, among themselves.
    const quint32 moving = rs.at(from).layerId;
    const quint32 onto = rs.at(to).layerId;
    if (from == to || moving == ShowTree::invalidId() || onto == ShowTree::invalidId())
        return false;
    const quint32 cueId = m_cue;
    return st()->edit(tr("reorder layers in %1").arg(title()), [cueId, moving, onto](QList<Production> &ps) {
        for (Production &p : ps)
            for (CueList &l : p.lists)
                if (Cue *c = l.cue(cueId))
                {
                    int a = -1, b = -1;
                    for (int i = 0; i < c->layers.size(); ++i)
                    {
                        if (c->layers.at(i).id == moving)
                            a = i;
                        if (c->layers.at(i).id == onto)
                            b = i;
                    }
                    if (a < 0 || b < 0)
                        return false;
                    c->layers.move(a, b);
                    return true;
                }
        return false;
    });
}

quint32 TreeCueStack::addNew(Function::Type type, int after, const QList<GroupHead> &heads, quint32 group)
{
    Q_UNUSED(after)
    if (!canAddLayers())
        return Function::invalidId();
    Function *f = nullptr;
    QString base;
    switch (type)
    {
    case Function::SceneType:     f = new Scene(m_doc);     base = tr("New Scene"); break;
    case Function::EFXType:       f = new EFX(m_doc);       base = tr("New EFX"); break;
    case Function::RGBMatrixType: f = new RGBMatrix(m_doc); base = tr("New RGB Matrix"); break;
    default:
        return Function::invalidId();
    }
    if (!m_doc->addFunction(f))
    {
        delete f;
        return Function::invalidId();
    }
    const quint32 fid = f->id();
    f->setName(QStringLiteral("%1 %2").arg(base).arg(fid));
    if (EFX *efx = qobject_cast<EFX *>(f))
    {
        for (const GroupHead &h : heads)
            if (m_doc->fixture(h.fxi) != nullptr)
                efx->addFixture(h.fxi, h.head);
    }
    else if (RGBMatrix *m = qobject_cast<RGBMatrix *>(f))
    {
        if (m_doc->fixtureGroup(group) != nullptr)
            m->setFixtureGroup(group);
    }
    Target t;
    t.function = fid;
    if (st()->addLayer(m_cue, t) == ShowTree::invalidId())
    {
        m_doc->deleteFunction(fid);
        return Function::invalidId();
    }
    /* One step with the tree's: undone, the new function goes too; redone,
       it comes back with its id (and the tree bakes it in again). */
    Doc *doc = m_doc;
    ShowTreeController *controller = st();
    QSharedPointer<QByteArray> kept(new QByteArray);
    controller->attachToLastStep([doc, controller, fid, kept](bool undone) {
        if (undone)
        {
            Function *fn = doc->function(fid);
            if (fn == nullptr)
                return;
            kept->clear();
            QBuffer buf(kept.data());
            buf.open(QIODevice::WriteOnly);
            QXmlStreamWriter w(&buf);
            w.writeStartDocument();
            fn->saveXML(&w);
            w.writeEndDocument();
            doc->deleteFunction(fid);
        }
        else if (doc->function(fid) == nullptr && !kept->isEmpty())
        {
            QXmlStreamReader r(*kept);
            while (!r.atEnd() && !r.isStartElement())
                r.readNext();
            if (r.isStartElement())
                Function::loader(r, doc);
            controller->bake();
        }
    });
    return fid;
}

/****************************************************************************
 * The tree's drops, menus and pages
 ****************************************************************************/

QString TreeCueStack::dropPlanOn(int layer, const QMimeData *mime, bool separate) const
{
    if (m_view.isNull() || mime == nullptr || cue() == nullptr)
        return QString();
    const QList<Row> rs = rows();
    quint32 lid = ShowTree::invalidId();
    Target t;
    if (layer >= 0 && layer < rs.size())
    {
        lid = rs.at(layer).layerId;
        if (lid == ShowTree::invalidId())
            t = rs.at(layer).target;
    }
    const QString hint = m_view->dropHint(m_cue, lid, mime, separate, t);
    if (!m_view->acceptsDrop(m_cue, lid, mime, t))
        return hint.isEmpty() ? QString() : QStringLiteral("!") + hint;
    return hint.isEmpty() ? tr("Drop it on %1").arg(title()) : hint;
}

bool TreeCueStack::dropOn(int layer, const QMimeData *mime, bool separate)
{
    if (dropPlanOn(layer, mime, separate).isEmpty() || dropPlanOn(layer, mime, separate).startsWith(QLatin1Char('!')))
        return false;
    const QList<Row> rs = rows();
    quint32 lid = ShowTree::invalidId();
    Target t;
    if (layer >= 0 && layer < rs.size())
    {
        lid = rs.at(layer).layerId;
        if (lid == ShowTree::invalidId())
            t = rs.at(layer).target;
    }
    return m_view->dropMime(m_cue, lid, mime, separate, t, /*stayOnCue*/ true);
}

bool TreeCueStack::showMenu(int layer, int attribute, const QPoint &globalPos)
{
    if (m_view.isNull())
        return false;
    const QList<Layer> ls = layers();
    if (layer < 0 || layer >= ls.size())
        return false;
    const quint32 lid = ls.at(layer).layerId;
    const Target t = targetAt(layer);
    if (attribute >= 0 && attribute < ls.at(layer).attributes.size())
    {
        // The tree's own menu for the value: set, clear, off, Level…,
        // Colour…, the palettes of its type (and W4's Design effect…).
        const QString key = ls.at(layer).attributes.at(attribute).key;
        if (isLineParamKey(key))
        {
            QMenu menu(m_view);
            m_view->lineParamMenu(&menu, m_cue, lid, lid == ShowTree::invalidId() ? t : Target(), key);
            menu.exec(globalPos);
        }
        else
            m_view->attributeMenu(m_cue, lid, lid == ShowTree::invalidId() ? t : Target(), t, key, globalPos);
        return true;
    }

    QMenu menu(m_view);
    menu.setToolTipsVisible(true);
    // Focus this cue (docs/design/FOCUS_REFINE_DESIGN.md): this layer from the stage.
    {
        const quint32 cue = m_cue;
        QAction *refine = menu.addAction(tr("Focus this cue\342\200\246"), &menu,
                                         [this, cue, lid]() {
            RefinePanel::open(m_doc, m_view->window(), cue, lid);
        });
        refine->setObjectName(QStringLiteral("cueStackRefine"));
        FocusMenu::setAvailable(refine, FocusMenu::focusCueBlocked(m_doc),
                                tr("Move, zoom, focus and recolour this cue from the stage with the controller, "
                                   "starting on this layer"));
        menu.addSeparator();
    }
    QAction *solo = menu.addAction(tr("Solo this layer"));
    solo->setCheckable(true);
    solo->setChecked(m_solo && focusIndex() == layer);
    solo->setToolTip(tr("Only this layer plays; off, the whole cue plays in context"));
    connect(solo, &QAction::toggled, this, [this, layer](bool on) {
        setFocusLayer(layer);
        setSolo(on);
        emit changed();
    });
    if (!t.isOpaque())
    {
        QAction *release = menu.addAction(tr("Release (leaving this cue)"));
        release->setCheckable(true);
        bool on = false;
        if (const Cue *c = cue())
            if (const ShowTree::Layer *own = c->layer(lid))
                on = own->release;
        release->setChecked(on);
        release->setToolTip(tr("Leaving this cue, the layer goes back to the base cue (else dark)"));
        connect(release, &QAction::toggled, this, [this, lid, t](bool r) {
            if (lid != ShowTree::invalidId())
                st()->setLayerRelease(m_cue, lid, r);
            else
                st()->setTargetRelease(m_cue, t, r);
        });
    }
    if (lid != ShowTree::invalidId())
    {
        menu.addSeparator();
        menu.addAction(tr("Remove the layer"), this, [this, layer]() { removeLayers({ layer }); })
            ->setToolTip(tr("Take it out of this cue (one undo step)"));
    }
    menu.addSeparator();
    menu.addAction(tr("Show in the tree"), this, [this, lid, t]() {
        if (m_view.isNull())
            return;
        if (lid != ShowTree::invalidId())
            m_view->selectNode(lid, m_cue);
        else
            m_view->selectTracked(m_cue, t.signature());
    });
    menu.exec(globalPos);
    return true;
}

bool TreeCueStack::clearAttribute(int layer, int attribute)
{
    const QList<Layer> ls = layers();
    if (m_view.isNull() || layer < 0 || layer >= ls.size() || attribute < 0
        || attribute >= ls.at(layer).attributes.size())
        return false;
    const Layer &l = ls.at(layer);
    // Only what is set here clears: a carried-in value is the earlier cue's.
    if (l.layerId == ShowTree::invalidId() || l.attributes.at(attribute).inherited)
        return false;
    return m_view->clearAttribute(m_cue, l.layerId, l.attributes.at(attribute).key);
}

QWidget *TreeCueStack::lendPane(int layer, int attribute)
{
    Q_UNUSED(attribute)
    if (m_view.isNull() || cue() == nullptr)
        return nullptr;
    const QList<Row> rs = rows();
    if (layer < 0 || layer >= rs.size())
        return m_view->lendInspector(m_cue, ShowTree::invalidId(), QString());
    return m_view->lendInspector(m_cue, rs.at(layer).layerId,
                                 rs.at(layer).layerId == ShowTree::invalidId() ? rs.at(layer).target.signature()
                                                                                : QString());
}

void TreeCueStack::returnPane(QWidget *pane)
{
    Q_UNUSED(pane)
    if (!m_view.isNull())
        m_view->takeBackInspector();
}

QList<quint32> TreeCueStack::layerFixtures(int layer) const
{
    const Target t = targetAt(layer);
    if (t.isEmpty() || t.isOpaque() || st() == nullptr)
        return QList<quint32>();
    return st()->targetFixtures(t);
}

int TreeCueStack::layerOfFixture(quint32 fixtureId) const
{
    // The cue's own layer first (it is what an edit changes), then one
    // carried in; the last on stage wins, as it wins the channels.
    const QList<Row> rs = rows();
    int carried = -1;
    for (int i = rs.size() - 1; i >= 0; --i)
    {
        if (rs.at(i).target.isOpaque() || !st()->targetFixtures(rs.at(i).target).contains(fixtureId))
            continue;
        if (rs.at(i).layerId != ShowTree::invalidId())
            return i;
        if (carried < 0)
            carried = i;
    }
    return carried;
}

quint32 TreeCueStack::layerScene(int layer) const
{
    const Target t = targetAt(layer);
    if (t.isEmpty() || st() == nullptr)
        return Function::invalidId();
    return t.isOpaque() ? t.function : st()->layerScene(m_cue, t);
}

/****************************************************************************
 * Preview: the cue in context (the tree's), or Solo
 ****************************************************************************/

void TreeCueStack::startOwnPreview()
{
    m_playing = true;
    if (st() == nullptr)
        return;
    if (m_solo)
    {
        playSolo();
        return;
    }
    // The tree's preview first, then let go of Solo: a scene both play
    // never stops in between.
    st()->previewCue(m_cue);
    stopSolo();
}

void TreeCueStack::stopOwnPreview()
{
    m_playing = false;
    stopSolo();
    if (st() != nullptr && st()->previewedCue() == m_cue)
        st()->stopPreview();
}

bool TreeCueStack::ownPreviewHeld() const
{
    if (st() == nullptr || !m_playing)
        return false;
    if (m_solo)
        return st()->previewHeld() && !st()->listsRunning().isEmpty();
    return st()->previewState() == ShowTreeController::PreviewState::Held && st()->previewedCue() == m_cue;
}

void TreeCueStack::releaseRunHold()
{
    if (st() != nullptr)
        st()->resumePreview();
}

QString TreeCueStack::ownPreviewText() const
{
    if (m_solo && ownPreviewHeld())
        return tr("■ Solo held — %1 running. Play to preview over it.")
                   .arg(st()->listsRunning().join(QStringLiteral(", ")));
    if (m_solo)
    {
        const QList<Layer> ls = layers();
        const Function *f = m_doc->function(m_soloScene);
        const int focus = focusIndex();
        if (f == nullptr || focus < 0 || focus >= ls.size())
            return tr("Solo: select a layer — it plays alone");
        return tr("▶ Solo: %1 — only this layer").arg(ls.at(focus).label.trimmed());
    }
    if (!m_view.isNull())
    {
        const QString text = m_view->previewText();
        if (!text.isEmpty() && ownPreviewHeld())
            return text;
        if (!text.isEmpty())
            return text + tr(" — in context");
    }
    return QString();
}

void TreeCueStack::setSolo(bool solo)
{
    if (m_solo == solo)
        return;
    m_solo = solo;
    if (m_playing)
        startOwnPreview();
}

int TreeCueStack::focusIndex() const
{
    if (m_focusSignature.isEmpty())
        return -1;
    const QList<Row> rs = rows();
    for (int i = 0; i < rs.size(); ++i)
        if (rs.at(i).target.signature() == m_focusSignature)
            return i;
    return -1;
}

void TreeCueStack::setFocusLayer(int layer)
{
    const QString sig = layer >= 0 ? targetAt(layer).signature() : QString();
    if (m_focus == layer && sig == m_focusSignature)
        return;
    m_focus = layer;
    m_focusSignature = sig;
    // The tree beside the Workbench shows the same layer (or the cue).
    if (!m_view.isNull())
    {
        const quint32 lid = layerIdAt(layer);
        m_view->selectForWorkbench(m_cue, lid, lid == ShowTree::invalidId() ? sig : QString());
    }
    if (m_playing && m_solo)
        playSolo();
}

void TreeCueStack::playSolo()
{
    // RC8: held by a running show until Play, as the cue in context is.
    if (st() != nullptr && st()->previewHeld() && !st()->listsRunning().isEmpty())
    {
        stopSolo();
        return;
    }
    const quint32 sid = layerScene(focusIndex());
    Function *f = m_doc->function(sid);
    if (f != nullptr && sid == m_soloScene && f->isRunning())
    {
        // The same scene (re-baked in place): it re-reads itself.
        if (Scene *s = qobject_cast<Scene *>(f))
            s->resetRuntime();
    }
    else
    {
        // The new one first, then the old one and the cue let go.
        const quint32 old = m_soloScene;
        if (f != nullptr)
            f->start(m_doc->masterTimer(), soloParent());
        if (old != sid)
            stopSolo();
        m_soloScene = f != nullptr ? sid : Function::invalidId();
    }
    if (st() != nullptr && st()->previewedCue() == m_cue)
        st()->stopPreview();
}

void TreeCueStack::stopSolo()
{
    if (m_soloScene == Function::invalidId() || m_doc == nullptr)
        return;
    if (Function *f = m_doc->function(m_soloScene))
        f->stop(soloParent());
    m_soloScene = Function::invalidId();
}
