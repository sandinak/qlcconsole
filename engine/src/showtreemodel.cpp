/*
  Q Light Controller Plus
  showtreemodel.cpp

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

#include <QRegularExpression>
#include <QColor>
#include <QtMath>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QVector>
#include <QHash>
#include <QDebug>
#include <algorithm>

#include "showtreemodel.h"

#define KXMLShowTreeProduction  QStringLiteral("Production")
#define KXMLShowTreeCueList     QStringLiteral("CueList")
#define KXMLShowTreeLooks       QStringLiteral("Looks")
#define KXMLShowTreeSubCues     QStringLiteral("SubCues")
#define KXMLShowTreeCue         QStringLiteral("Cue")
#define KXMLShowTreeLayer       QStringLiteral("Layer")
#define KXMLShowTreeLook        QStringLiteral("Look")
#define KXMLShowTreeContinues   QStringLiteral("Continues")

namespace ShowTree
{

quint32 invalidId()
{
    return 0xFFFFFFFF;
}

const QString KeyColour = QStringLiteral("Colour");
const QString KeyDimmer = QStringLiteral("Dimmer");
const QString KeyPosition = QStringLiteral("Position");
const QString KeyGobo = QStringLiteral("Beam.Gobo");
const QString KeyZoom = QStringLiteral("Beam.Zoom");
const QString KeyBeam = QStringLiteral("Beam.Beam");
const QString KeyShutter = QStringLiteral("Shutter");
const QString KeyEffectPrefix = QStringLiteral("Effect.");
const QString KeySpread = QStringLiteral("Position.Spread");
const QString KeyGrouping = QStringLiteral("Position.Grouping");
const QString KeyCross = QStringLiteral("Position.Cross");

bool isLineParamKey(const QString &key)
{
    return key == KeySpread || key == KeyGrouping || key == KeyCross;
}

QString effectKey(const QString &kind)
{
    return KeyEffectPrefix + (kind.isEmpty() ? QStringLiteral("Other") : kind);
}

bool isEffectKey(const QString &key)
{
    return key.startsWith(KeyEffectPrefix);
}

/* The look-stack order (docs/features/effects.md, "How a look stacks"):
   the static looks paint the base, then the effects play over it. Colour
   first, as a scene's colour looks are numbered in list order; an effect's
   own Colour/Dimmer follow it, which is what makes them its own. */
static int keyRank(const QString &key)
{
    static const QStringList base = { KeyColour, KeyDimmer, KeyPosition, KeySpread, KeyGrouping,
                                      KeyCross, KeyGobo, KeyZoom, KeyBeam, KeyShutter };
    static const QStringList effects = { QStringLiteral("Color"), QStringLiteral("Dimmer"),
                                         QStringLiteral("Position"), QStringLiteral("Beam"),
                                         QStringLiteral("Other") };
    if (isEffectKey(key))
    {
        const int i = effects.indexOf(key.mid(KeyEffectPrefix.length()));
        return 100 + (i < 0 ? effects.count() : i);
    }
    const int i = base.indexOf(key);
    return i < 0 ? base.count() : i;
}

bool keyLessThan(const QString &a, const QString &b)
{
    const int ra = keyRank(a), rb = keyRank(b);
    if (ra != rb)
        return ra < rb;
    return a < b;
}

QString keyLabel(const QString &key)
{
    if (key == KeyGobo)
        return QStringLiteral("Gobo");
    if (key == KeyZoom)
        return QStringLiteral("Zoom");
    if (key == KeyBeam)
        return QStringLiteral("Focus / Frost / Iris");
    if (key == KeySpread)
        return QStringLiteral("Spread");
    if (key == KeyGrouping)
        return QStringLiteral("Grouping");
    if (key == KeyCross)
        return QStringLiteral("Cross");
    if (isEffectKey(key))
    {
        const QString kind = key.mid(KeyEffectPrefix.length());
        if (kind == QLatin1String("Color"))
            return QStringLiteral("Colour effect");
        if (kind == QLatin1String("Other"))
            return QStringLiteral("Effect");
        return kind + QStringLiteral(" effect");
    }
    return key;
}

/****************************************************************************
 * The tree
 ****************************************************************************/

bool Look::operator==(const Look &other) const
{
    return key == other.key && palettes == other.palettes && own == other.own
           && none == other.none && value == other.value;
}

static QList<quint32> sortedIds(QList<quint32> ids)
{
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

QList<Target> Layer::pieces() const
{
    QList<Target> out;
    if (target.isOpaque())
        return out;
    for (quint32 g : target.groups)
    {
        Target t;
        t.groups << g;
        out << t;
    }
    for (quint32 set : target.sets)
    {
        Target t;
        t.sets << set;
        out << t;
    }
    if (!target.fixtures.isEmpty())
    {
        Target t;
        t.fixtures = target.fixtures;
        out << t;
    }
    return out;
}

QString Target::signature() const
{
    if (isOpaque())
        return QStringLiteral("x:%1").arg(function);
    QString sig = QStringLiteral("g:%1|f:%2").arg(idsToString(sortedIds(groups)),
                                                  idsToString(sortedIds(fixtures)));
    // Only when there are sets: a target without one reads as it always did
    // (generated scenes are found again by it).
    if (!sets.isEmpty())
        sig += QStringLiteral("|s:") + idsToString(sortedIds(sets));
    return sig;
}

bool effectiveMoveInBlack(bool showDefault, MoveInBlack list, bool cueOff)
{
    if (cueOff)
        return false;
    if (list == MoveInBlack::On)
        return true;
    if (list == MoveInBlack::Off)
        return false;
    return showDefault;
}

QString cueModeToString(CueMode mode)
{
    switch (mode)
    {
        case CueMode::Track: return QStringLiteral("Track");
        case CueMode::Block: return QStringLiteral("Block");
        case CueMode::CueOnly: return QStringLiteral("CueOnly");
        case CueMode::ListDefault: break;
    }
    return QString();
}

CueMode stringToCueMode(const QString &str)
{
    if (str == QLatin1String("Track"))
        return CueMode::Track;
    if (str == QLatin1String("Block"))
        return CueMode::Block;
    if (str == QLatin1String("CueOnly"))
        return CueMode::CueOnly;
    return CueMode::ListDefault;
}

template <typename L>
static const Look *findLook(const L &looks, const QString &key)
{
    for (const Look &l : looks)
        if (l.key == key)
            return &l;
    return nullptr;
}

static void setLookIn(QList<Look> &looks, const Look &look)
{
    for (int i = 0; i < looks.count(); ++i)
    {
        if (looks.at(i).key != look.key)
            continue;
        if (look.palettes.isEmpty() && !look.none && look.value.isEmpty())
            looks.removeAt(i);
        else
            looks[i] = look;
        return;
    }
    if (!look.palettes.isEmpty() || look.none || !look.value.isEmpty())
        looks.append(look);
}

const Look *Layer::look(const QString &key) const
{
    return findLook(looks, key);
}

void Layer::setLook(const Look &look)
{
    setLookIn(looks, look);
}

const Look *Cue::look(const QString &key) const
{
    return findLook(looks, key);
}

void Cue::setLook(const Look &look)
{
    setLookIn(looks, look);
}

Layer *Cue::layer(quint32 lid)
{
    for (Layer &l : layers)
        if (l.id == lid)
            return &l;
    return nullptr;
}

const Layer *Cue::layer(quint32 lid) const
{
    for (const Layer &l : layers)
        if (l.id == lid)
            return &l;
    return nullptr;
}

int CueList::indexOf(quint32 cueId) const
{
    for (int i = 0; i < cues.count(); ++i)
        if (cues.at(i).id == cueId)
            return i;
    return -1;
}

Cue *CueList::cue(quint32 cid)
{
    const int i = indexOf(cid);
    return i < 0 ? nullptr : &cues[i];
}

const Cue *CueList::cue(quint32 cid) const
{
    const int i = indexOf(cid);
    return i < 0 ? nullptr : &cues.at(i);
}

CueList *Production::list(quint32 lid)
{
    for (CueList &l : lists)
        if (l.id == lid)
            return &l;
    return nullptr;
}

CueList *Production::looks()
{
    for (CueList &l : lists)
        if (l.looks)
            return &l;
    return nullptr;
}

const CueList *Production::looks() const
{
    for (const CueList &l : lists)
        if (l.looks)
            return &l;
    return nullptr;
}

const CueList *Production::list(quint32 lid) const
{
    for (const CueList &l : lists)
        if (l.id == lid)
            return &l;
    return nullptr;
}

/****************************************************************************
 * Resolution
 ****************************************************************************/

QList<Look> ResolvedLayer::playedLooks() const
{
    QStringList keys = values.keys();
    std::sort(keys.begin(), keys.end(), keyLessThan);
    QList<Look> out;
    for (const QString &k : keys)
    {
        const Look &l = values.value(k).look;
        if (!l.none && !l.palettes.isEmpty())
            out << l;
    }
    return out;
}

bool ResolvedLayer::isDark() const
{
    return !target.isOpaque() && playedLooks().isEmpty();
}

const ResolvedLayer *ResolvedCue::layer(const Target &target) const
{
    const QString sig = target.signature();
    for (int i = layers.count() - 1; i >= 0; --i)
        if (layers.at(i).target.signature() == sig)
            return &layers.at(i);
    return nullptr;
}

CueMode effectiveMode(const CueList &list, const Cue &cue)
{
    // A busking look never tracks (P-D5): what it has is all it plays.
    if (list.looks)
        return CueMode::Block;
    // A sub-cue starts from the one before it, 4.1 from cue 4 (D25).
    if (list.isSubCues())
        return CueMode::Track;
    if (cue.mode != CueMode::ListDefault)
        return cue.mode;
    return list.mode == ListMode::Tracking ? CueMode::Track : CueMode::Block;
}

namespace
{

/** Whether cue @a k releases a layer it passes on to the base (D7). */
bool releasesToBase(const CueList &list, int k)
{
    const Cue &cue = list.cues.at(k);
    if (effectiveMode(list, cue) == CueMode::CueOnly || cue.skip)
        return false;               // a Cue-only (or skipped) cue's changes, its releases too, don't pass on
    for (const Layer &l : cue.layers)
        if (l.release && !l.target.isEmpty())
            return true;
    return false;
}

/** The cues resolving cue @a k reads straight away (indexes). */
QList<int> needs(const CueList &list, int k)
{
    QList<int> out;
    const Cue &cue = list.cues.at(k);
    const CueMode mode = effectiveMode(list, cue);
    const int before = playedBefore(list, k);
    if (before >= 0 && (mode == CueMode::Track || mode == CueMode::CueOnly))
        out << before;
    for (quint32 inc : cue.includes)
    {
        const int j = list.indexOf(inc);
        if (j >= 0 && j != k && !out.contains(j))
            out << j;
    }
    const int base = list.indexOf(list.baseCue);
    if (base >= 0 && base != k && !out.contains(base) && releasesToBase(list, k))
        out << base;
    return out;
}

bool dependsOnIndex(const CueList &list, int from, int on)
{
    if (from < 0 || on < 0)
        return false;
    QVector<bool> seen(list.cues.count(), false);
    QList<int> todo { from };
    while (!todo.isEmpty())
    {
        const int k = todo.takeLast();
        if (k == on)
            return true;
        if (seen.at(k))
            continue;
        seen[k] = true;
        todo << needs(list, k);
    }
    return false;
}

int indexOfTarget(const QList<ResolvedLayer> &layers, const QString &sig)
{
    for (int i = 0; i < layers.count(); ++i)
        if (layers.at(i).target.signature() == sig)
            return i;
    return -1;
}

/** Carried into the next cue: everything becomes "tracked", but a layer just
    released to the base says so for the one cue it lands in. */
QList<ResolvedLayer> carried(const QList<ResolvedLayer> &from)
{
    QList<ResolvedLayer> out = from;
    for (ResolvedLayer &l : out)
    {
        l.own = false;
        l.tookOver = Target();
        if (l.origin == Origin::Base)
            continue;
        l.origin = Origin::Tracked;
        for (ResolvedValue &v : l.values)
            v.origin = Origin::Tracked;
    }
    return out;
}

/** A cue that mixes can hold an included layer and its own on the same
    target, side by side (Cue::mixOverlap). What it passes on is one layer
    per target, as the one rule merges them: the later's values over the
    earlier's, in the later's place. */
QList<ResolvedLayer> mergedByTarget(const QList<ResolvedLayer> &from)
{
    QList<ResolvedLayer> out;
    for (const ResolvedLayer &l : from)
    {
        const int at = indexOfTarget(out, l.target.signature());
        if (at < 0)
        {
            out << l;
            continue;
        }
        ResolvedLayer merged = l;
        const ResolvedLayer earlier = out.takeAt(at);
        for (auto it = earlier.values.constBegin(); it != earlier.values.constEnd(); ++it)
            if (!merged.values.contains(it.key()))
                merged.values.insert(it.key(), it.value());
        out << merged;
    }
    return out;
}

/** passOn as it leaves a cue: a layer that was Base in it is plain tracked
    from then on. */
QList<ResolvedLayer> settled(const QList<ResolvedLayer> &from)
{
    QList<ResolvedLayer> out = from;
    for (ResolvedLayer &l : out)
    {
        if (l.origin != Origin::Base)
            continue;
        l.origin = Origin::Tracked;
        for (ResolvedValue &v : l.values)
            v.origin = Origin::Tracked;
    }
    return out;
}

struct Resolver
{
    const CueList &list;
    LineAimTest isLineAim;
    const QList<ResolvedLayer> *startState;
    QVector<ResolvedCue> out;
    QVector<int> state;     // 0 not yet, 1 in progress, 2 done

    Resolver(const CueList &l, const LineAimTest &lineAim, const QList<ResolvedLayer> *s)
        : list(l), isLineAim(lineAim), startState(s), out(l.cues.count()), state(l.cues.count(), 0)
    {
    }

    void resolveAt(int i)
    {
        if (state.at(i) != 0)
            return;
        state[i] = 1;

        const Cue &cue = list.cues.at(i);
        ResolvedCue &rc = out[i];
        rc.cueId = cue.id;

        const CueMode mode = effectiveMode(list, cue);
        QList<ResolvedLayer> start;
        // From the cue before -- the nearest not skipped: a skipped cue
        // passes on nothing of its own.
        const int before = playedBefore(list, i);
        if (before >= 0 && (mode == CueMode::Track || mode == CueMode::CueOnly))
        {
            resolveAt(before);
            start = carried(out.at(before).passOn);
        }
        else if (before < 0 && startState != nullptr && mode == CueMode::Track)
        {
            // The first sub-cue played: from what its cue puts on stage.
            start = carried(settled(*startState));
        }

        QList<ResolvedLayer> layers = start;

        // A cue that mixes plays an included cue's layers beside its own and
        // beside another include's, as the chaser's nested collection did:
        // they take no included layer's place (tracked ones as usual).
        const bool mix = cue.mixOverlap;
        auto carriedAt = [&](const QString &sig) {
            for (int k = 0; k < layers.count(); ++k)
                if (layers.at(k).target.signature() == sig
                    && !(mix && layers.at(k).origin == Origin::Included))
                    return k;
            return -1;
        };

        // Includes: the included cue's output, as if standing in it (D8).
        for (quint32 inc : cue.includes)
        {
            const int j = list.indexOf(inc);
            // One that would loop (it needs this cue, at any depth) is left
            // out; resolving it would read this cue half done.
            if (j < 0 || j == i || state.at(j) == 1 || dependsOnIndex(list, j, i))
            {
                rc.skippedIncludes << inc;
                continue;
            }
            resolveAt(j);
            for (const ResolvedLayer &il : out.at(j).layers)
            {
                const QString sig = il.target.signature();
                int k = carriedAt(sig);
                if (k < 0)
                {
                    ResolvedLayer copy = il;
                    copy.own = false;
                    copy.release = false;
                    copy.origin = Origin::Included;
                    copy.fromCue = inc;
                    copy.values.clear();
                    layers << copy;
                    k = layers.count() - 1;
                }
                for (auto it = il.values.constBegin(); it != il.values.constEnd(); ++it)
                {
                    ResolvedValue v = it.value();
                    v.origin = Origin::Included;
                    v.fromCue = inc;
                    layers[k].values.insert(it.key(), v);
                }
            }
        }

        // This cue's own layers: the layer's looks, else the cue's (D5, D6).
        QList<ResolvedLayer> own;
        QHash<QString, ResolvedLayer> taken;    // carried-in layers taken over, as they came in
        for (const Layer &layer : cue.layers)
        {
            if (layer.target.isEmpty())
                continue;
            const QString sig = layer.target.signature();
            ResolvedLayer r;
            bool took = false;
            // Take a carried-in layer's place: one still there, or (a split)
            // one an earlier own layer of this cue already took -- each piece
            // starts from it.
            auto take = [&](const Target &t, ResolvedLayer *into) -> bool {
                const QString tsig = t.signature();
                const int at = carriedAt(tsig);
                if (at >= 0)
                {
                    *into = layers.takeAt(at);
                    into->tookOver = into->target;
                    taken.insert(tsig, *into);
                    return true;
                }
                if (taken.contains(tsig) && !(t == layer.target))
                {
                    *into = taken.value(tsig);
                    return true;
                }
                return false;
            };
            took = take(layer.target, &r);
            // A layer whose groups changed here carries on from the one it
            // was: it takes that one's place and values.
            if (!took && !layer.continues.isEmpty() && !layer.target.isOpaque())
                took = take(layer.continues, &r);
            if (!layer.target.isOpaque())
            {
                // A merged layer also stands for the others it was made of.
                for (const Target &also : layer.continuesAlso)
                {
                    ResolvedLayer extra;
                    if (!take(also, &extra))
                        continue;
                    if (!took)
                    {
                        r = extra;
                        took = true;
                        continue;
                    }
                    for (auto it = extra.values.constBegin(); it != extra.values.constEnd(); ++it)
                        if (!r.values.contains(it.key()))
                            r.values.insert(it.key(), it.value());
                }
            }
            if (!took)
            {
                const int ko = indexOfTarget(own, sig);
                if (ko >= 0)
                    r = own.takeAt(ko);     // twice in one cue: the lower wins
            }
            r.target = layer.target;
            r.name = layer.name;
            r.layerId = layer.id;
            r.own = true;
            r.release = layer.release;
            r.origin = Origin::Own;
            r.fromCue = cue.id;

            // A child layer (§3.5): under which layer, how deep.
            r.parent = Target();
            r.depth = 0;
            {
                QList<quint32> seen { layer.id };
                const Layer *up = layer.parent == invalidId() ? nullptr : cue.layer(layer.parent);
                while (up != nullptr && !seen.contains(up->id))
                {
                    if (r.depth == 0)
                        r.parent = up->target;
                    ++r.depth;
                    seen << up->id;
                    up = up->parent == invalidId() ? nullptr : cue.layer(up->parent);
                }
            }
            const bool child = r.depth > 0;

            if (!layer.target.isOpaque())
            {
                // Cue looks reach the cue's own top layers (D6); a child is
                // lit by its parent for what it doesn't set. An effect or a
                // Position set at cue level is ONE across the layers it
                // reaches (D10, D17): marked with the cue as its span.
                if (!child)
                    for (const Look &cl : cue.looks)
                    {
                        if (isLineParamKey(cl.key))
                            continue;                   // below (D18)
                        ResolvedValue v { cl, Origin::CueLook, cue.id };
                        if (isEffectKey(cl.key) || cl.key == KeyPosition)
                            v.span = cue.id;
                        r.values.insert(cl.key, v);
                    }
                for (const Look &ll : layer.looks)
                    r.values.insert(ll.key, ResolvedValue { ll, Origin::Own, cue.id });
                // D18: the cue's Spread / Grouping / Cross reach only layers
                // whose Position, as it resolves here, aims at a line.
                if (!child)
                {
                    const auto pos = r.values.constFind(KeyPosition);
                    const bool line = pos != r.values.constEnd() && !pos.value().look.none
                                      && isLineAim && isLineAim(pos.value().look);
                    for (const Look &cl : cue.looks)
                    {
                        if (!isLineParamKey(cl.key) || !line || layer.look(cl.key) != nullptr)
                            continue;
                        ResolvedValue v { cl, Origin::CueLook, cue.id };
                        v.span = cue.id;
                        r.values.insert(cl.key, v);
                    }
                }
            }
            own << r;
        }
        // Carried-in layers sit under the cue's own (§2, overlap).
        layers << own;
        rc.layers = layers;

        // What the next cue starts from. A layer that came back to the base
        // in this cue is plain tracked from here on.
        QList<ResolvedLayer> pass = settled((mode == CueMode::CueOnly) ? start
                                            : mix ? mergedByTarget(layers) : layers);
        if (mode != CueMode::CueOnly && !cue.skip)
        {
            const int baseIdx = list.indexOf(list.baseCue);
            for (int k = pass.count() - 1; k >= 0; --k)
            {
                if (!(pass.at(k).own && pass.at(k).release))
                    continue;
                const Target t = pass.at(k).target;
                pass.removeAt(k);
                // Dark: no base, or a base that needs this cue (it tracks
                // or includes its way back here) -- a loop.
                if (baseIdx < 0 || baseIdx == i || state.at(baseIdx) == 1
                    || dependsOnIndex(list, baseIdx, i))
                    continue;
                resolveAt(baseIdx);
                const ResolvedLayer *b = out.at(baseIdx).layer(t);
                if (b == nullptr)
                    continue;                       // not the base's: dark
                ResolvedLayer back = *b;
                back.own = false;
                back.release = false;
                back.origin = Origin::Base;
                back.fromCue = list.baseCue;
                for (ResolvedValue &v : back.values)
                {
                    v.origin = Origin::Base;
                    v.fromCue = list.baseCue;
                }
                pass.insert(k, back);
            }
        }
        rc.passOn = pass;
        state[i] = 2;
    }
};

} // namespace

bool dependsOn(const CueList &list, quint32 fromCue, quint32 onCue)
{
    return dependsOnIndex(list, list.indexOf(fromCue), list.indexOf(onCue));
}

bool canInclude(const CueList &list, quint32 cueId, quint32 includedId, QString *why)
{
    if (list.looks)
    {
        if (why != nullptr)
            *why = QStringLiteral("a look plays only what it has (no includes)");
        return false;
    }
    auto no = [why](const QString &text) {
        if (why != nullptr)
            *why = text;
        return false;
    };
    if (list.isSubCues())
        return no(QStringLiteral("a sub-cue starts from the one before it (no includes)"));
    const int i = list.indexOf(cueId), j = list.indexOf(includedId);
    if (i < 0 || j < 0)
        return no(QStringLiteral("not a cue of this list"));
    if (i == j)
        return no(QStringLiteral("a cue can't include itself"));
    if (list.cues.at(i).includes.contains(includedId))
        return no(QStringLiteral("already included"));
    if (dependsOnIndex(list, j, i))
        return no(QStringLiteral("it would loop: that cue already plays from this one"));
    return true;
}

QList<quint32> rebakeChain(const CueList &list, quint32 cueId)
{
    const int n = list.cues.count();
    const int at = list.indexOf(cueId);
    if (at < 0)
        return {};

    // start[k]: what cue k starts from may change; content[k]: what it adds
    // itself (its own layers and looks, its includes) may.
    QVector<bool> start(n, false), content(n, false);
    content[at] = true;
    const int base = list.indexOf(list.baseCue);

    bool more = true;
    while (more)
    {
        more = false;
        auto mark = [&more](QVector<bool> &v, int k) {
            if (!v.at(k))
            {
                v[k] = true;
                more = true;
            }
        };
        for (int k = 0; k < n; ++k)
        {
            if (!start.at(k) && !content.at(k))
                continue;
            const Cue &cue = list.cues.at(k);
            // What it passes on: a Cue-only cue passes on what it started
            // from; a skipped one nothing of its own. It reaches the cues
            // after it up to the next one played (the skipped ones between
            // start from it too).
            const bool passes = !cue.skip && (effectiveMode(list, cue) != CueMode::CueOnly || start.at(k));
            if (passes)
                for (int m = k + 1; m < n; ++m)
                {
                    if (effectiveMode(list, list.cues.at(m)) != CueMode::Block)
                        mark(start, m);
                    if (!list.cues.at(m).skip)
                        break;
                }
            // Every cue that includes it plays its output.
            for (int m = 0; m < n; ++m)
                if (m != k && list.cues.at(m).includes.contains(cue.id))
                    mark(content, m);
            // The base: every cue that releases a layer back to it passes
            // the base's values on.
            if (k == base)
                for (int m = 0; m < n; ++m)
                {
                    if (m == k || !releasesToBase(list, m))
                        continue;
                    for (int j = m + 1; j < n; ++j)
                    {
                        if (effectiveMode(list, list.cues.at(j)) != CueMode::Block)
                            mark(start, j);
                        if (!list.cues.at(j).skip)
                            break;
                    }
                }
        }
    }

    QList<quint32> out;
    for (int k = 0; k < n; ++k)
        if (start.at(k) || content.at(k))
            out << list.cues.at(k).id;
    return out;
}

QList<ResolvedCue> resolve(const CueList &list, const LineAimTest &isLineAim, const QList<ResolvedLayer> *start)
{
    Resolver r(list, isLineAim, start);
    for (int i = 0; i < list.cues.count(); ++i)
        r.resolveAt(i);
    // A skipped cue passes on what it was handed: what the cue played
    // before it passes on (the first played: what the list starts from).
    for (int i = 0; i < list.cues.count(); ++i)
    {
        if (!list.cues.at(i).skip)
            continue;
        const int before = playedBefore(list, i);
        r.out[i].passOn = before >= 0 ? r.out.at(before).passOn
                          : start != nullptr ? settled(*start) : QList<ResolvedLayer>();
    }
    return QList<ResolvedCue>(r.out.begin(), r.out.end());
}

int playedBefore(const CueList &list, int index)
{
    int k = qMin(index, int(list.cues.count())) - 1;
    while (k >= 0 && list.cues.at(k).skip)
        --k;
    return k;
}

int playedAfter(const CueList &list, int index)
{
    for (int k = qMax(index + 1, 0); k < list.cues.count(); ++k)
        if (!list.cues.at(k).skip)
            return k;
    return -1;
}

Stage stageOf(const ResolvedCue &cue)
{
    Stage stage;
    for (const ResolvedLayer &l : cue.layers)
    {
        if (l.target.isOpaque())
        {
            stage[l.target.signature()][QStringLiteral("Function")] = { QStringLiteral("plays") };
            continue;
        }
        QStringList units;
        for (quint32 g : l.target.groups)
            units << QStringLiteral("g:%1").arg(g);
        for (quint32 set : l.target.sets)
            units << QStringLiteral("s:%1").arg(set);
        for (quint32 f : l.target.fixtures)
            units << QStringLiteral("f:%1").arg(f);
        QList<Look> played = l.playedLooks();
        // A line aim's parameters play too (with no palette of their own).
        for (const QString &k : { KeySpread, KeyGrouping, KeyCross })
            if (l.values.contains(k) && !l.values.value(k).look.none)
                played << l.values.value(k).look;
        for (const Look &look : played)
        {
            QString value = QStringLiteral("%1/%2").arg(idsToString(look.palettes),
                                                        idsToString(look.own));
            if (!look.value.isEmpty())
                value += QStringLiteral("=") + look.value;
            for (const QString &u : units)
            {
                QStringList &have = stage[u][look.key];
                if (look.key != KeyDimmer)
                {
                    have = QStringList { value };       // LTP: the later layer wins
                }
                else if (!have.contains(value))
                {
                    have << value;                      // HTP: every level plays
                    have.sort();
                }
            }
        }
    }
    return stage;
}

bool stageDiffers(const CueList &before, const CueList &after, quint32 *cueId, QString *unit, QString *key,
                  const LineAimTest &isLineAim)
{
    const QList<ResolvedCue> a = resolve(before, isLineAim);
    const QList<ResolvedCue> b = resolve(after, isLineAim);
    for (const ResolvedCue &rb : b)
    {
        Stage sa;
        for (const ResolvedCue &ra : a)
            if (ra.cueId == rb.cueId)
                sa = stageOf(ra);
        const Stage sb = stageOf(rb);
        if (sa == sb)
            continue;
        if (cueId != nullptr)
            *cueId = rb.cueId;
        QStringList units = sa.keys();
        for (const QString &u : sb.keys())
            if (!units.contains(u))
                units << u;
        for (const QString &u : units)
        {
            const QMap<QString, QStringList> ka = sa.value(u), kb = sb.value(u);
            if (ka == kb)
                continue;
            if (unit != nullptr)
                *unit = u;
            QStringList keys = ka.keys();
            for (const QString &k : kb.keys())
                if (!keys.contains(k))
                    keys << k;
            for (const QString &k : keys)
                if (ka.value(k) != kb.value(k))
                {
                    if (key != nullptr)
                        *key = k;
                    break;
                }
            break;
        }
        return true;
    }
    return false;
}

/****************************************************************************
 * Numbering
 ****************************************************************************/

static QList<double> numberParts(const QString &number)
{
    QList<double> parts;
    for (const QString &p : number.split(QLatin1Char('.')))
    {
        bool ok = false;
        const double d = p.toDouble(&ok);
        parts << (ok ? d : 0.0);
    }
    return parts;
}

/** As numbers, part by part: "0.0.4" < "0.0.4.5" < "0.0.5". */
static int compareNumbers(const QString &a, const QString &b)
{
    const QList<double> pa = numberParts(a), pb = numberParts(b);
    for (int i = 0; i < qMax(pa.count(), pb.count()); ++i)
    {
        const double x = i < pa.count() ? pa.at(i) : 0.0;
        const double y = i < pb.count() ? pb.at(i) : 0.0;
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

int compareCueNumbers(const QString &a, const QString &b)
{
    return compareNumbers(a, b);
}

static QString numberAt(const CueList &list, int index)
{
    const QString n = list.cues.at(index).number;
    return n.isEmpty() ? QString::number(index + 1) : n;
}

QString cueNumberAt(const CueList &list, int index, const QString &parentNumber)
{
    if (index < 0 || index >= list.cues.count())
        return QString();
    const QString n = numberAt(list, index);
    return list.isSubCues() && !parentNumber.isEmpty() ? parentNumber + QLatin1Char('.') + n : n;
}

int indexOfNumber(const CueList &list, const QString &number)
{
    if (number.trimmed().isEmpty())
        return -1;
    for (int i = 0; i < list.cues.count(); ++i)
        if (compareNumbers(numberAt(list, i), number.trimmed()) == 0)
            return i;
    return -1;
}

bool isCueNumber(const QString &number)
{
    static const QRegularExpression re(QStringLiteral("^\\d+(?:\\.\\d+)*$"));
    return re.match(number).hasMatch();
}

static bool numberFits(const QString &candidate, const QString &prev, const QString &next,
                       const QStringList &used)
{
    if (!prev.isEmpty() && compareNumbers(candidate, prev) <= 0)
        return false;
    if (!next.isEmpty() && compareNumbers(candidate, next) >= 0)
        return false;
    for (const QString &u : used)
        if (compareNumbers(u, candidate) == 0)
            return false;
    return true;
}

/* D23 between @a prev and @a next: the next whole number in the last place,
   else a point cue one level down (5, 6, 7, 8, 9, then 1, 2, 3, 4), else
   the same one more level down from prev.0 -- at most three levels below
   prev. */
static QString numberAfter(const QString &prev, const QString &next, const QStringList &used, int deeper)
{
    QStringList parts = prev.split(QLatin1Char('.'));
    bool ok = false;
    const int last = parts.last().toInt(&ok);
    if (ok)
    {
        parts.last() = QString::number(last + 1);
        const QString candidate = parts.join(QLatin1Char('.'));
        if (numberFits(candidate, prev, next, used))
            return candidate;
    }
    for (int k : { 5, 6, 7, 8, 9, 1, 2, 3, 4 })
    {
        const QString candidate = QStringLiteral("%1.%2").arg(prev).arg(k);
        if (numberFits(candidate, prev, next, used))
            return candidate;
    }
    if (deeper < 2)
        return numberAfter(prev + QStringLiteral(".0"), next, used, deeper + 1);
    return QString();
}

QString numberBetween(const QString &prev, const QString &next, const QStringList &used)
{
    if (!prev.isEmpty())
        return numberAfter(prev, next, used, 0);
    if (next.isEmpty())
        return QStringLiteral("1");

    // Above the first: one less in its last place while that stays 1 or
    // more (5 -> 4, 0.0.3 -> 0.0.2); else it cut short, longest first,
    // ending 1 or more (2.1 -> 2); else a point cue above it with its last
    // place that isn't 0 one less (1 -> 0.5, 0.0.1 -> 0.0.0.5, 1.0.0 -> 0.0.1).
    const QStringList parts = next.split(QLatin1Char('.'));
    QList<int> places;
    for (const QString &p : parts)
    {
        bool ok = false;
        places << p.toInt(&ok);
        if (!ok)
            return QString();
    }
    auto join = [](const QList<int> &ps) {
        QStringList out;
        for (int p : ps)
            out << QString::number(p);
        return out.join(QLatin1Char('.'));
    };
    if (places.last() > 1)
    {
        QList<int> one = places;
        one.last() -= 1;
        if (numberFits(join(one), QString(), next, used))
            return join(one);
    }
    for (int n = places.count() - 1; n > 0; --n)
    {
        const QList<int> cut = places.mid(0, n);
        if (cut.last() >= 1 && numberFits(join(cut), QString(), next, used))
            return join(cut);
    }
    for (int i = places.count() - 1; i >= 0; --i)
    {
        if (places.at(i) == 0)
            continue;
        QList<int> base = places;
        base[i] -= 1;
        return numberAfter(join(base), next, used, 0);
    }
    return QString();
}

QString numberForInsert(const CueList &list, int afterIndex, const QStringList &taken)
{
    // Sub-cues are numbered under their cue: 1, 2... (4.1, 4.2).
    if (list.cues.isEmpty() && list.isSubCues())
        return QStringLiteral("1");
    if (list.cues.isEmpty())
    {
        // A new list's first cue: from the list's leading digits (D23):
        // "01-Everybody Scream" -> 1.0.0.
        QRegularExpression lead(QStringLiteral("^(\\d+)"));
        QRegularExpressionMatch m = lead.match(list.name);
        if (m.hasMatch())
            return QStringLiteral("%1.0.0").arg(m.captured(1).toInt());
        return QStringLiteral("1");
    }

    QStringList used = taken;
    for (int i = 0; i < list.cues.count(); ++i)
        used << numberAt(list, i);

    if (afterIndex < 0)
        return numberBetween(QString(), numberAt(list, 0), used);

    afterIndex = qMin(afterIndex, int(list.cues.count()) - 1);
    const QString prev = numberAt(list, afterIndex);
    const QString next = afterIndex + 1 < list.cues.count() ? numberAt(list, afterIndex + 1) : QString();
    const QString n = numberBetween(prev, next, used);
    // Nothing fits three levels down: as before, a point of a point (out
    // of order, but never a number already taken).
    return n.isEmpty() ? QStringLiteral("%1.5.5").arg(prev) : n;
}

bool splitNumberedName(const QString &text, QString *number, QString *name)
{
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d+(?:\\.\\d+)*)\\s*(?:[-–—:]\\s*|\\s+|$)(.*)$"));
    QRegularExpressionMatch m = re.match(text);
    if (!m.hasMatch())
        return false;
    if (number != nullptr)
        *number = m.captured(1);
    if (name != nullptr)
        *name = m.captured(2).trimmed();
    return true;
}

/****************************************************************************
 * Text colours
 ****************************************************************************/

const QList<TextColour> &textColours()
{
    // Mid-tone and saturated: each reads on a white list as on a black one
    // once readableOn() has lightened or darkened it a little.
    static const QList<TextColour> colours = {
        { QStringLiteral("Red"), QStringLiteral("#e53935") },
        { QStringLiteral("Orange"), QStringLiteral("#fb8c00") },
        { QStringLiteral("Yellow"), QStringLiteral("#fdd835") },
        { QStringLiteral("Green"), QStringLiteral("#43a047") },
        { QStringLiteral("Cyan"), QStringLiteral("#00acc1") },
        { QStringLiteral("Blue"), QStringLiteral("#1e88e5") },
        { QStringLiteral("Purple"), QStringLiteral("#8e24aa") },
        { QStringLiteral("Pink"), QStringLiteral("#d81b60") },
    };
    return colours;
}

bool parseTextColour(const QString &text, QString *hex)
{
    const QString t = text.trimmed();
    QString out;
    bool ok = false;
    if (t.compare(QLatin1String("none"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("off"), Qt::CaseInsensitive) == 0
        || t.compare(QLatin1String("clear"), Qt::CaseInsensitive) == 0)
        ok = true;
    for (const TextColour &c : textColours())
        if (!ok && t.compare(c.name, Qt::CaseInsensitive) == 0)
        {
            out = c.hex;
            ok = true;
        }
    static const QRegularExpression re(QStringLiteral("^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$"));
    if (!ok && re.match(t).hasMatch())
    {
        out = QColor(t).name();
        ok = true;
    }
    if (ok && hex != nullptr)
        *hex = out;
    return ok;
}

QString textColourName(const QString &hex)
{
    if (hex.isEmpty())
        return QString();
    for (const TextColour &c : textColours())
        if (c.hex.compare(hex, Qt::CaseInsensitive) == 0)
            return c.name;
    return hex.toLower();
}

static double luminance(const QColor &c)
{
    auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

QColor readableOn(const QColor &colour, const QColor &background, double ratio)
{
    if (!colour.isValid() || !background.isValid() || contrastRatio(colour, background) >= ratio)
        return colour;
    // Toward white on a dark background, toward black on a light one (the
    // side with the more room: 0.179 is where white and black contrast it
    // equally), lightness only, so it stays the same colour.
    const bool lighter = luminance(background) < 0.179;
    float h = 0, s = 0, l = 0, a = 1;
    colour.getHslF(&h, &s, &l, &a);
    for (int i = 1; i <= 50; ++i)
    {
        const float nl = lighter ? l + (1.0f - l) * float(i) / 50.0f : l * (1.0f - float(i) / 50.0f);
        const QColor c = QColor::fromHslF(h, s, nl);
        if (contrastRatio(c, background) >= ratio)
            return c;
    }
    return lighter ? QColor(Qt::white) : QColor(Qt::black);
}

/****************************************************************************
 * XML
 ****************************************************************************/

QString idsToString(const QList<quint32> &ids)
{
    QStringList parts;
    for (quint32 id : ids)
        parts << QString::number(id);
    return parts.join(QLatin1Char(','));
}

QList<quint32> stringToIds(const QString &str)
{
    QList<quint32> ids;
    for (const QString &p : str.split(QLatin1Char(','), Qt::SkipEmptyParts))
    {
        bool ok = false;
        const quint32 id = p.trimmed().toUInt(&ok);
        if (ok)
            ids << id;
    }
    return ids;
}

static void saveLooks(QXmlStreamWriter *doc, const QList<Look> &looks)
{
    for (const Look &l : looks)
    {
        doc->writeStartElement(KXMLShowTreeLook);
        doc->writeAttribute(QStringLiteral("Key"), l.key);
        if (!l.palettes.isEmpty())
            doc->writeAttribute(QStringLiteral("Palettes"), idsToString(l.palettes));
        if (!l.own.isEmpty())
            doc->writeAttribute(QStringLiteral("Own"), idsToString(l.own));
        if (l.none)
            doc->writeAttribute(QStringLiteral("None"), QStringLiteral("1"));
        if (!l.value.isEmpty())
            doc->writeAttribute(QStringLiteral("Value"), l.value);
        doc->writeEndElement();
    }
}

static Look loadLook(QXmlStreamReader &root)
{
    const QXmlStreamAttributes a = root.attributes();
    Look l;
    l.key = a.value(QStringLiteral("Key")).toString();
    l.palettes = stringToIds(a.value(QStringLiteral("Palettes")).toString());
    l.own = stringToIds(a.value(QStringLiteral("Own")).toString());
    l.none = a.value(QStringLiteral("None")).toString() == QLatin1String("1");
    l.value = a.value(QStringLiteral("Value")).toString();
    root.skipCurrentElement();
    return l;
}

static void writeId(QXmlStreamWriter *doc, const QString &name, quint32 id)
{
    if (id != invalidId())
        doc->writeAttribute(name, QString::number(id));
}

static quint32 readId(const QXmlStreamAttributes &a, const QString &name)
{
    bool ok = false;
    const quint32 id = a.value(name).toString().toUInt(&ok);
    return ok ? id : invalidId();
}

static void saveLayer(QXmlStreamWriter *doc, const Layer &layer)
{
    doc->writeStartElement(KXMLShowTreeLayer);
    writeId(doc, QStringLiteral("ID"), layer.id);
    if (!layer.name.isEmpty())
        doc->writeAttribute(QStringLiteral("Name"), layer.name);
    if (!layer.target.groups.isEmpty())
        doc->writeAttribute(QStringLiteral("Groups"), idsToString(layer.target.groups));
    if (!layer.target.fixtures.isEmpty())
        doc->writeAttribute(QStringLiteral("Fixtures"), idsToString(layer.target.fixtures));
    if (!layer.target.sets.isEmpty())
        doc->writeAttribute(QStringLiteral("Sets"), idsToString(layer.target.sets));
    writeId(doc, QStringLiteral("Function"), layer.target.function);
    writeId(doc, QStringLiteral("Parent"), layer.parent);
    if (layer.release)
        doc->writeAttribute(QStringLiteral("Release"), QStringLiteral("1"));
    writeId(doc, QStringLiteral("Reusable"), layer.reusable);
    if (!layer.unlinkedFrom.isEmpty())
        doc->writeAttribute(QStringLiteral("UnlinkedFrom"), layer.unlinkedFrom);
    if (!layer.continues.groups.isEmpty())
        doc->writeAttribute(QStringLiteral("ContinuesGroups"), idsToString(layer.continues.groups));
    if (!layer.continues.fixtures.isEmpty())
        doc->writeAttribute(QStringLiteral("ContinuesFixtures"),
                            idsToString(layer.continues.fixtures));
    if (!layer.continues.sets.isEmpty())
        doc->writeAttribute(QStringLiteral("ContinuesSets"), idsToString(layer.continues.sets));
    for (const Target &also : layer.continuesAlso)
    {
        doc->writeStartElement(KXMLShowTreeContinues);
        if (!also.groups.isEmpty())
            doc->writeAttribute(QStringLiteral("Groups"), idsToString(also.groups));
        if (!also.fixtures.isEmpty())
            doc->writeAttribute(QStringLiteral("Fixtures"), idsToString(also.fixtures));
        if (!also.sets.isEmpty())
            doc->writeAttribute(QStringLiteral("Sets"), idsToString(also.sets));
        doc->writeEndElement();
    }
    saveLooks(doc, layer.looks);
    doc->writeEndElement();
}

void saveXML(QXmlStreamWriter *doc, const Production &production)
{
    doc->writeStartElement(KXMLShowTreeProduction);
    writeId(doc, QStringLiteral("ID"), production.id);
    doc->writeAttribute(QStringLiteral("Name"), production.name);
    writeId(doc, QStringLiteral("MainList"), production.mainList);
    if (production.timecode >= -1)
        doc->writeAttribute(QStringLiteral("Timecode"), QString::number(production.timecode));
    // Q190: only when it has one (an older build ignores it).
    if (production.numbering != Numbering::None)
        doc->writeAttribute(QStringLiteral("Numbering"), numberingToString(production.numbering));

    // Tonight's running order (0.3.1 ro): an older build skips the element.
    if (!production.runningOrder.isEmpty())
    {
        doc->writeStartElement(QStringLiteral("RunningOrder"));
        for (const RunningSong &r : production.runningOrder)
        {
            doc->writeStartElement(QStringLiteral("Song"));
            doc->writeAttribute(QStringLiteral("Section"), r.id);
            if (r.skip)
                doc->writeAttribute(QStringLiteral("Skip"), QStringLiteral("1"));
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }

    for (const CueList &list : production.lists)
    {
        // The Looks section is an element of its own: an older build skips
        // it (and plays the looks' generated collections as they are).
        // So is a cue's list of sub-cues (P6).
        doc->writeStartElement(list.looks ? KXMLShowTreeLooks
                               : list.isSubCues() ? KXMLShowTreeSubCues : KXMLShowTreeCueList);
        writeId(doc, QStringLiteral("ID"), list.id);
        doc->writeAttribute(QStringLiteral("Name"), list.name);
        if (!list.colour.isEmpty())
            doc->writeAttribute(QStringLiteral("Colour"), list.colour);
        // Q38: only when it isn't the show's (an older build ignores it).
        if (!list.looks && !list.isSubCues() && list.moveInBlack != MoveInBlack::Show)
            doc->writeAttribute(QStringLiteral("MoveInBlack"), list.moveInBlack == MoveInBlack::On
                                                               ? QStringLiteral("On") : QStringLiteral("Off"));
        writeId(doc, QStringLiteral("Cue"), list.subOf);
        if (list.isSubCues())
        {
            writeId(doc, QStringLiteral("Chaser"), list.chaser);
        }
        else if (!list.looks)
        {
            doc->writeAttribute(QStringLiteral("Mode"), list.mode == ListMode::Tracking
                                                        ? QStringLiteral("Tracking")
                                                        : QStringLiteral("NonTracking"));
            writeId(doc, QStringLiteral("Base"), list.baseCue);
            writeId(doc, QStringLiteral("Chaser"), list.chaser);
            // W5: the chaser it came from, and what Import in place filed.
            writeId(doc, QStringLiteral("Source"), list.source);
            if (list.inPlace)
                doc->writeAttribute(QStringLiteral("InPlace"), QStringLiteral("1"));
            for (const CueList::Original &o : list.originals)
            {
                doc->writeStartElement(QStringLiteral("Original"));
                writeId(doc, QStringLiteral("ID"), o.function);
                doc->writeAttribute(QStringLiteral("Path"), o.path);
                doc->writeEndElement();
            }
        }
        // Q190: its song sections' names (an older build skips them).
        for (const SectionHead &h : list.sections)
        {
            doc->writeStartElement(QStringLiteral("Section"));
            if (!h.key.isEmpty())
                doc->writeAttribute(QStringLiteral("Key"), h.key);
            writeId(doc, QStringLiteral("Cue"), h.cue);
            doc->writeAttribute(QStringLiteral("Name"), h.name);
            doc->writeEndElement();
        }
        for (const QString &f : list.folders)
        {
            doc->writeStartElement(QStringLiteral("Folder"));
            doc->writeAttribute(QStringLiteral("Path"), f);
            doc->writeEndElement();
        }

        for (const Cue &cue : list.cues)
        {
            doc->writeStartElement(KXMLShowTreeCue);
            writeId(doc, QStringLiteral("ID"), cue.id);
            if (!cue.number.isEmpty())
                doc->writeAttribute(QStringLiteral("Number"), cue.number);
            doc->writeAttribute(QStringLiteral("Name"), cue.name);
            if (!cue.note.isEmpty())
                doc->writeAttribute(QStringLiteral("Note"), cue.note);
            if (cue.mode != CueMode::ListDefault)
                doc->writeAttribute(QStringLiteral("Mode"), cueModeToString(cue.mode));
            doc->writeAttribute(QStringLiteral("FadeIn"), QString::number(cue.fadeIn));
            doc->writeAttribute(QStringLiteral("FadeOut"), QString::number(cue.fadeOut));
            doc->writeAttribute(QStringLiteral("Hold"), QString::number(cue.hold));
            if (!cue.includes.isEmpty())
                doc->writeAttribute(QStringLiteral("Includes"), idsToString(cue.includes));
            if (cue.mixOverlap)
                doc->writeAttribute(QStringLiteral("Overlap"), QStringLiteral("Mix"));
            writeId(doc, QStringLiteral("Reusable"), cue.reusable);
            if (!cue.unlinkedFrom.isEmpty())
                doc->writeAttribute(QStringLiteral("UnlinkedFrom"), cue.unlinkedFrom);
            writeId(doc, QStringLiteral("Collection"), cue.collection);
            if (list.looks && !cue.folder.isEmpty())
                doc->writeAttribute(QStringLiteral("Folder"), cue.folder);
            if (list.looks && cue.scope != HitScope::Production)
            {
                doc->writeAttribute(QStringLiteral("Scope"), cue.scope == HitScope::Global ? QStringLiteral("Global")
                                                             : cue.scope == HitScope::List ? QStringLiteral("List")
                                                                                           : QStringLiteral("Cue"));
                writeId(doc, QStringLiteral("ScopeOf"), cue.scopeOf);
            }
            if (cue.skip && !list.looks)
                doc->writeAttribute(QStringLiteral("Skip"), QStringLiteral("1"));
            if (!cue.colour.isEmpty())
                doc->writeAttribute(QStringLiteral("Colour"), cue.colour);
            if (cue.noMoveInBlack && !list.looks)
                doc->writeAttribute(QStringLiteral("NoMoveInBlack"), QStringLiteral("1"));
            if (cue.focusCall && !list.looks)
                doc->writeAttribute(QStringLiteral("FocusCall"), QStringLiteral("1"));
            if (list.isSubCues() && cue.follow)
            {
                doc->writeAttribute(QStringLiteral("Follow"), QStringLiteral("Auto"));
                doc->writeAttribute(QStringLiteral("Wait"), QString::number(cue.wait));
            }

            saveLooks(doc, cue.looks);
            for (const Layer &layer : cue.layers)
                saveLayer(doc, layer);
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

void saveXML(QXmlStreamWriter *doc, const GroupSet &set)
{
    doc->writeStartElement(QStringLiteral("GroupSet"));
    writeId(doc, QStringLiteral("ID"), set.id);
    doc->writeAttribute(QStringLiteral("Name"), set.name);
    if (!set.groups.isEmpty())
        doc->writeAttribute(QStringLiteral("Groups"), idsToString(set.groups));
    if (!set.fixtures.isEmpty())
        doc->writeAttribute(QStringLiteral("Fixtures"), idsToString(set.fixtures));
    doc->writeEndElement();
}

bool loadXML(QXmlStreamReader &root, GroupSet &set)
{
    if (root.name() != QLatin1String("GroupSet"))
        return false;
    const QXmlStreamAttributes a = root.attributes();
    set = GroupSet();
    set.id = readId(a, QStringLiteral("ID"));
    set.name = a.value(QStringLiteral("Name")).toString();
    set.groups = stringToIds(a.value(QStringLiteral("Groups")).toString());
    set.fixtures = stringToIds(a.value(QStringLiteral("Fixtures")).toString());
    root.skipCurrentElement();
    return set.id != invalidId();
}

/** A <Layer> (the reader on its start element). */
static Layer loadLayer(QXmlStreamReader &root)
{
    const QXmlStreamAttributes a = root.attributes();
    Layer layer;
    layer.id = readId(a, QStringLiteral("ID"));
    layer.name = a.value(QStringLiteral("Name")).toString();
    layer.target.groups = stringToIds(a.value(QStringLiteral("Groups")).toString());
    layer.target.fixtures = stringToIds(a.value(QStringLiteral("Fixtures")).toString());
    layer.target.sets = stringToIds(a.value(QStringLiteral("Sets")).toString());
    layer.target.function = readId(a, QStringLiteral("Function"));
    layer.parent = readId(a, QStringLiteral("Parent"));
    layer.release = a.value(QStringLiteral("Release")).toString() == QLatin1String("1");
    layer.reusable = readId(a, QStringLiteral("Reusable"));
    layer.unlinkedFrom = a.value(QStringLiteral("UnlinkedFrom")).toString();
    layer.continues.groups = stringToIds(a.value(QStringLiteral("ContinuesGroups")).toString());
    layer.continues.fixtures = stringToIds(a.value(QStringLiteral("ContinuesFixtures")).toString());
    layer.continues.sets = stringToIds(a.value(QStringLiteral("ContinuesSets")).toString());
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLShowTreeLook)
        {
            layer.looks << loadLook(root);
        }
        else if (root.name() == KXMLShowTreeContinues)
        {
            const QXmlStreamAttributes ca = root.attributes();
            Target also;
            also.groups = stringToIds(ca.value(QStringLiteral("Groups")).toString());
            also.fixtures = stringToIds(ca.value(QStringLiteral("Fixtures")).toString());
            also.sets = stringToIds(ca.value(QStringLiteral("Sets")).toString());
            if (!also.isEmpty())
                layer.continuesAlso << also;
            root.skipCurrentElement();
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    return layer;
}

/** A Colour= text colour: "#rrggbb", else none. */
static QString readColour(const QXmlStreamAttributes &a)
{
    QString hex;
    return parseTextColour(a.value(QStringLiteral("Colour")).toString(), &hex) ? hex : QString();
}

bool loadXML(QXmlStreamReader &root, Production &production)
{
    if (root.name() != KXMLShowTreeProduction)
        return false;

    QXmlStreamAttributes a = root.attributes();
    production = Production();
    production.id = readId(a, QStringLiteral("ID"));
    production.name = a.value(QStringLiteral("Name")).toString();
    production.mainList = readId(a, QStringLiteral("MainList"));
    if (a.hasAttribute(QStringLiteral("Timecode")))
    {
        bool ok = false;
        const int tc = a.value(QStringLiteral("Timecode")).toString().toInt(&ok);
        production.timecode = ok && tc >= -1 ? tc : -2;
    }
    production.numbering = stringToNumbering(a.value(QStringLiteral("Numbering")).toString());

    while (root.readNextStartElement())
    {
        if (root.name() == QLatin1String("RunningOrder"))
        {
            while (root.readNextStartElement())
            {
                if (root.name() == QLatin1String("Song"))
                {
                    RunningSong r;
                    r.id = root.attributes().value(QStringLiteral("Section")).toString();
                    r.skip = root.attributes().value(QStringLiteral("Skip")).toString() == QLatin1String("1");
                    if (!r.id.isEmpty())
                        production.runningOrder << r;
                }
                root.skipCurrentElement();
            }
            continue;
        }
        const bool looks = root.name() == KXMLShowTreeLooks;
        const bool subs = root.name() == KXMLShowTreeSubCues;
        if (root.name() != KXMLShowTreeCueList && !subs && (!looks || production.looks() != nullptr))
        {
            root.skipCurrentElement();
            continue;
        }
        a = root.attributes();
        CueList list;
        list.looks = looks;
        list.id = readId(a, QStringLiteral("ID"));
        list.name = a.value(QStringLiteral("Name")).toString();
        list.colour = readColour(a);
        list.mode = a.value(QStringLiteral("Mode")).toString() == QLatin1String("Tracking")
                    ? ListMode::Tracking : ListMode::NonTracking;
        list.baseCue = readId(a, QStringLiteral("Base"));
        list.chaser = readId(a, QStringLiteral("Chaser"));
        if (!looks && !subs)
        {
            const QString mib = a.value(QStringLiteral("MoveInBlack")).toString();
            list.moveInBlack = mib.compare(QLatin1String("On"), Qt::CaseInsensitive) == 0 ? MoveInBlack::On
                             : mib.compare(QLatin1String("Off"), Qt::CaseInsensitive) == 0 ? MoveInBlack::Off
                             : MoveInBlack::Show;
            list.source = readId(a, QStringLiteral("Source"));
            list.inPlace = a.value(QStringLiteral("InPlace")).toString() == QLatin1String("1");
        }
        if (subs)
        {
            list.subOf = readId(a, QStringLiteral("Cue"));
            list.mode = ListMode::Tracking;
            list.baseCue = invalidId();
            if (list.subOf == invalidId())
            {
                root.skipCurrentElement();
                continue;
            }
        }

        while (root.readNextStartElement())
        {
            if (looks && root.name() == QLatin1String("Folder"))
            {
                const QString f = root.attributes().value(QStringLiteral("Path")).toString();
                if (!f.isEmpty() && !list.folders.contains(f))
                    list.folders << f;
                root.skipCurrentElement();
                continue;
            }
            if (!looks && !subs && root.name() == QLatin1String("Section"))
            {
                const QXmlStreamAttributes sa = root.attributes();
                SectionHead h;
                h.key = sa.value(QStringLiteral("Key")).toString();
                h.cue = readId(sa, QStringLiteral("Cue"));
                h.name = sa.value(QStringLiteral("Name")).toString();
                if (!h.key.isEmpty() || h.cue != invalidId())
                    list.sections << h;
                root.skipCurrentElement();
                continue;
            }
            if (!looks && !subs && root.name() == QLatin1String("Original"))
            {
                const QXmlStreamAttributes oa = root.attributes();
                CueList::Original o;
                o.function = readId(oa, QStringLiteral("ID"));
                o.path = oa.value(QStringLiteral("Path")).toString();
                if (o.function != invalidId())
                    list.originals << o;
                root.skipCurrentElement();
                continue;
            }
            if (root.name() != KXMLShowTreeCue)
            {
                root.skipCurrentElement();
                continue;
            }
            a = root.attributes();
            Cue cue;
            cue.id = readId(a, QStringLiteral("ID"));
            cue.number = a.value(QStringLiteral("Number")).toString();
            cue.name = a.value(QStringLiteral("Name")).toString();
            cue.note = a.value(QStringLiteral("Note")).toString();
            cue.mode = stringToCueMode(a.value(QStringLiteral("Mode")).toString());
            cue.fadeIn = a.value(QStringLiteral("FadeIn")).toString().toUInt();
            cue.fadeOut = a.value(QStringLiteral("FadeOut")).toString().toUInt();
            if (a.hasAttribute(QStringLiteral("Hold")))
                cue.hold = a.value(QStringLiteral("Hold")).toString().toUInt();
            cue.includes = stringToIds(a.value(QStringLiteral("Includes")).toString());
            cue.mixOverlap = a.value(QStringLiteral("Overlap")).toString() == QLatin1String("Mix");
            cue.reusable = readId(a, QStringLiteral("Reusable"));
            cue.unlinkedFrom = a.value(QStringLiteral("UnlinkedFrom")).toString();
            cue.collection = readId(a, QStringLiteral("Collection"));
            cue.skip = !looks && a.value(QStringLiteral("Skip")).toString() == QLatin1String("1");
            cue.colour = readColour(a);
            cue.noMoveInBlack = !looks && a.value(QStringLiteral("NoMoveInBlack")).toString() == QLatin1String("1");
            cue.focusCall = !looks && a.value(QStringLiteral("FocusCall")).toString() == QLatin1String("1");
            if (looks)
            {
                cue.folder = a.value(QStringLiteral("Folder")).toString();
                const QString scope = a.value(QStringLiteral("Scope")).toString();
                cue.scope = scope == QLatin1String("Global") ? HitScope::Global
                            : scope == QLatin1String("List") ? HitScope::List
                            : scope == QLatin1String("Cue") ? HitScope::Cue : HitScope::Production;
                cue.scopeOf = readId(a, QStringLiteral("ScopeOf"));
                cue.mode = CueMode::ListDefault;
                cue.includes.clear();
            }
            if (subs)
            {
                cue.mode = CueMode::ListDefault;
                cue.includes.clear();
                cue.follow = a.value(QStringLiteral("Follow")).toString() == QLatin1String("Auto");
                cue.wait = a.value(QStringLiteral("Wait")).toString().toUInt();
            }

            while (root.readNextStartElement())
            {
                if (root.name() == KXMLShowTreeLook)
                {
                    cue.looks << loadLook(root);
                }
                else if (root.name() == KXMLShowTreeLayer)
                {
                    cue.layers << loadLayer(root);
                }
                else
                {
                    root.skipCurrentElement();
                }
            }
            list.cues << cue;
        }
        production.lists << list;
    }
    return true;
}

void saveXML(QXmlStreamWriter *doc, const ReusableLayer &reusable)
{
    // A layer's own element, inside: the same reader takes it.
    doc->writeStartElement(QStringLiteral("ReusableLayer"));
    writeId(doc, QStringLiteral("ID"), reusable.id);
    doc->writeAttribute(QStringLiteral("Name"), reusable.name);
    Layer l;
    l.name = reusable.name;
    l.target = reusable.target;
    l.looks = reusable.looks;
    saveLayer(doc, l);
    doc->writeEndElement();
}

bool loadXML(QXmlStreamReader &root, ReusableLayer &reusable)
{
    if (root.name() != QLatin1String("ReusableLayer"))
        return false;
    const QXmlStreamAttributes a = root.attributes();
    reusable = ReusableLayer();
    reusable.id = readId(a, QStringLiteral("ID"));
    reusable.name = a.value(QStringLiteral("Name")).toString();
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLShowTreeLayer)
        {
            const Layer l = loadLayer(root);
            reusable.target = l.target;
            reusable.looks = l.looks;
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    return reusable.id != invalidId();
}

void saveXML(QXmlStreamWriter *doc, const ReusableCue &reusable)
{
    doc->writeStartElement(QStringLiteral("ReusableCue"));
    writeId(doc, QStringLiteral("ID"), reusable.id);
    doc->writeAttribute(QStringLiteral("Name"), reusable.name);
    if (reusable.mode != CueMode::ListDefault)
        doc->writeAttribute(QStringLiteral("Mode"), cueModeToString(reusable.mode));
    if (reusable.mixOverlap)
        doc->writeAttribute(QStringLiteral("Overlap"), QStringLiteral("Mix"));
    if (!reusable.note.isEmpty())
        doc->writeAttribute(QStringLiteral("Note"), reusable.note);
    doc->writeAttribute(QStringLiteral("FadeIn"), QString::number(reusable.fadeIn));
    doc->writeAttribute(QStringLiteral("FadeOut"), QString::number(reusable.fadeOut));
    doc->writeAttribute(QStringLiteral("Hold"), QString::number(reusable.hold));
    saveLooks(doc, reusable.looks);
    for (const Layer &l : reusable.layers)
        saveLayer(doc, l);
    doc->writeEndElement();
}

bool loadXML(QXmlStreamReader &root, ReusableCue &reusable)
{
    if (root.name() != QLatin1String("ReusableCue"))
        return false;
    const QXmlStreamAttributes a = root.attributes();
    reusable = ReusableCue();
    reusable.id = readId(a, QStringLiteral("ID"));
    reusable.name = a.value(QStringLiteral("Name")).toString();
    reusable.mode = stringToCueMode(a.value(QStringLiteral("Mode")).toString());
    reusable.mixOverlap = a.value(QStringLiteral("Overlap")).toString() == QLatin1String("Mix");
    reusable.note = a.value(QStringLiteral("Note")).toString();
    reusable.fadeIn = a.value(QStringLiteral("FadeIn")).toString().toUInt();
    reusable.fadeOut = a.value(QStringLiteral("FadeOut")).toString().toUInt();
    if (a.hasAttribute(QStringLiteral("Hold")))
        reusable.hold = a.value(QStringLiteral("Hold")).toString().toUInt();
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLShowTreeLook)
            reusable.looks << loadLook(root);
        else if (root.name() == KXMLShowTreeLayer)
            reusable.layers << loadLayer(root);
        else
            root.skipCurrentElement();
    }
    return reusable.id != invalidId();
}

/****************************************************************************
 * Reusable content (P4)
 ****************************************************************************/

static QString looksContent(const QList<Look> &looks)
{
    QStringList parts;
    for (const Look &l : looks)
        parts << QStringLiteral("%1=%2/%3/%4/%5").arg(l.key, idsToString(l.palettes), idsToString(l.own),
                                                      l.none ? QStringLiteral("none") : QString(), l.value);
    parts.sort();
    return parts.join(QLatin1Char(';'));
}

QString layerContent(const Layer &layer)
{
    return QStringLiteral("%1|%2|%3").arg(layer.name, layer.target.signature(), looksContent(layer.looks));
}

QString cueContent(const Cue &cue)
{
    QStringList parts;
    parts << cue.name << cueModeToString(cue.mode) << (cue.mixOverlap ? QStringLiteral("mix") : QString())
          << looksContent(cue.looks);
    for (const Layer &l : cue.layers)
    {
        int parent = -1;
        for (int k = 0; k < cue.layers.count(); ++k)
            if (l.parent != invalidId() && cue.layers.at(k).id == l.parent)
                parent = k;
        parts << QStringLiteral("[%1|%2|%3|%4]").arg(layerContent(l)).arg(l.release ? 1 : 0).arg(parent)
                 .arg(l.reusable == invalidId() ? QString() : QString::number(l.reusable));
    }
    return parts.join(QLatin1Char('#'));
}

} // namespace ShowTree
