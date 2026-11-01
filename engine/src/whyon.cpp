/*
  Q Light Controller Plus
  whyon.cpp

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

#include <QSharedPointer>
#include <climits>
#include <QtMath>

#include "whyon.h"
#include "showtreecontroller.h"
#include "effectscriptrunner.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "showtreemodel.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "grandmaster.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcpalette.h"
#include "fixturegroup.h"
#include "groupmasters.h"
#include "mastertimer.h"
#include "universe.h"
#include "function.h"
#include "fixture.h"
#include "teachingsettings.h"
#include "scene.h"
#include "doc.h"

namespace
{

std::function<QString(quint32)> &widgetNamer()
{
    static std::function<QString(quint32)> namer;
    return namer;
}

const quint32 kInvalid = 0xFFFFFFFF;

/** One fader's part in the channel, as the universe merged it */
struct Source
{
    int priority = 0;
    QString name;
    quint32 functionId = kInvalid;
    FadeChannel fc;
    int flags = 0;
    uchar own = 0;              //!< what it writes, after its intensity
    qreal intensity = 1.0;      //!< the fader's (and its parent's)
    bool hit = false;           //!< an override hit's fader
    qreal hitMix = 1.0;
    bool approx = false;        //!< a blend mode other than Normal
    // Filled by the replay:
    WhyOn::Verdict verdict = WhyOn::Verdict::Info;
    int beatenBy = -1;          //!< the source that took it over
};

/** The byte of @a value for @a address in @a fc (a 16-bit pair: MSB first) */
uchar byteOf(const FadeChannel &fc, quint32 value, quint32 address)
{
    const int n = fc.channelCount();
    if (n <= 1)
        return uchar(value);
    const int i = int(address - fc.addressInUniverse());
    return uchar((value >> (8 * (n - 1 - i))) & 0xFF);
}

/** What a fader writes on the channel, as GenericFader::write() works it out */
uchar ownValue(const FadeChannel &fc, qreal intensity, qreal faderIntensity, qreal parentIntensity,
               quint32 address)
{
    quint32 value = fc.current();
    const int flags = fc.flags();
    if (fc.canFade())
    {
        if ((flags & FadeChannel::CrossFade) && fc.fadeTime() == 0)
        {
            const bool rampUp = fc.target() > fc.start();
            value = rampUp ? fc.target() - fc.start() : fc.start() - fc.target();
            value = quint32(qreal(value) * faderIntensity);
            value = quint32(qreal(rampUp ? fc.start() + value : fc.start() - value) * parentIntensity);
        }
        else if (flags & FadeChannel::Intensity)
        {
            value = fc.current(intensity);
        }
    }
    return byteOf(fc, value, address);
}

QString quoted(const QString &s)
{
    return QStringLiteral("“%1”").arg(s);
}

QString cueText(const ShowTreeController *st, quint32 cueId)
{
    const ShowTree::CueList *list = st->listOfCue(cueId);
    if (list == nullptr)
        return QString();
    for (int i = 0; i < list->cues.count(); ++i)
        if (list->cues.at(i).id == cueId)
            return st->cueLabel(*list, i);
    return QString();
}

/** The Show Tree look key a channel's value belongs to */
QString keyFor(const QLCChannel *ch)
{
    if (ch == nullptr)
        return ShowTree::KeyDimmer;
    switch (ch->group())
    {
        case QLCChannel::Intensity:
            return ch->colour() == QLCChannel::NoColour ? ShowTree::KeyDimmer : ShowTree::KeyColour;
        case QLCChannel::Colour:
            return ShowTree::KeyColour;
        case QLCChannel::Pan:
        case QLCChannel::Tilt:
            return ShowTree::KeyPosition;
        case QLCChannel::Gobo:
            return ShowTree::KeyGobo;
        case QLCChannel::Shutter:
            return ShowTree::KeyShutter;
        case QLCChannel::Beam:
        case QLCChannel::Prism:
            return ShowTree::KeyBeam;
        default:
            return QString();
    }
}

/** Where a generated scene (or an opaque function layer) plays now */
struct CuePlace
{
    quint32 cueId = kInvalid;
    quint32 layerId = kInvalid;
    QString layerName;
    bool preview = false;
    bool hit = false;
    ShowTree::Origin origin = ShowTree::Origin::Own;
    quint32 fromCue = kInvalid;
};

bool placeIn(const ShowTreeController *st, quint32 cueId, quint32 fid, const QString &key, CuePlace *out)
{
    ShowTree::ResolvedCue rc;
    if (cueId == kInvalid || !st->resolvedCue(cueId, &rc))
        return false;
    for (const ShowTree::ResolvedLayer &rl : rc.layers)
    {
        const bool mine = rl.target.isOpaque() ? rl.target.function == fid
                                               : st->layerScene(cueId, rl.target) == fid;
        if (!mine)
            continue;
        out->cueId = cueId;
        out->layerId = rl.layerId;
        out->layerName = rl.name.isEmpty() ? st->targetLabel(rl.target) : rl.name;
        out->origin = rl.origin;
        out->fromCue = rl.fromCue;
        // The attribute's own tracking, where the layer has it.
        if (!key.isEmpty() && rl.values.contains(key))
        {
            const ShowTree::ResolvedValue &v = rl.values.value(key);
            out->origin = v.origin;
            out->fromCue = v.fromCue;
        }
        if (const ShowTree::CueList *list = st->listOfCue(cueId))
            out->hit = list->looks;
        return true;
    }
    return false;
}

/** The cue (running, or previewed) a Show Tree scene plays in now. A scene
 *  several cues share is named for the one playing it: never "the first cue
 *  that plays it". */
bool findPlace(const ShowTreeController *st, quint32 fid, const QString &key, bool previewFirst,
               CuePlace *out)
{
    if (st == nullptr)
        return false;
    if (previewFirst && placeIn(st, st->previewedCue(), fid, key, out))
    {
        out->preview = true;
        return true;
    }
    for (const ShowTree::Production &p : st->productions())
        for (const ShowTree::CueList &l : p.lists)
            if (placeIn(st, st->runningCue(l.id), fid, key, out))
                return true;
    if (!previewFirst && placeIn(st, st->previewedCue(), fid, key, out))
    {
        out->preview = true;
        return true;
    }
    // A hit (or anything else the tree made): where the tree has it.
    quint32 listId = kInvalid, cueId = kInvalid, layerId = kInvalid;
    if (st->isGenerated(fid) && st->generatedNode(fid, &listId, &cueId, &layerId) && cueId != kInvalid)
    {
        if (const ShowTree::CueList *list = st->listOfCue(cueId))
            out->hit = list->looks;
        out->cueId = cueId;
        out->layerId = layerId;
        if (const ShowTree::Cue *c = st->cue(cueId))
            if (const ShowTree::Layer *layer = c->layer(layerId))
                out->layerName = layer->name.isEmpty() ? st->targetLabel(layer->target) : layer->name;
        return true;
    }
    return false;
}

/** Who started @a f, in words ("VC button Go", "a preview"); empty: the
 *  show itself (GO, a cue list, another function) */
QString startedBy(const Function *f)
{
    if (f == nullptr)
        return QString();
    for (const FunctionParent &p : f->sources())
    {
        if (p.type() == FunctionParent::ManualVCWidget || p.type() == FunctionParent::AutoVCWidget)
        {
            if (p.isPreview())
                return WhyOn::tr("a preview");
            if (p == FunctionParent::controlMap())
                return WhyOn::tr("the Control Map");
            const QString n = widgetNamer() ? widgetNamer()(p.id()) : QString();
            return n.isEmpty() ? WhyOn::tr("a Virtual Console widget") : n;
        }
    }
    return QString();
}

/** The effects (effect palettes) playing on @a fixtureId now, by name */
QStringList effectsOn(Doc *doc, quint32 fixtureId, quint32 *sceneId)
{
    QStringList names;
    EffectScriptRunner *runner = doc->effectScriptRunner();
    if (runner == nullptr)
        return names;
    for (Function *f : doc->functions())
    {
        Scene *s = qobject_cast<Scene *>(f);
        if (s == nullptr || !s->isRunning())
            continue;
        bool covers = s->fixtures().contains(fixtureId);
        for (quint32 g : s->fixtureGroups())
            if (!covers)
                if (FixtureGroup *grp = doc->fixtureGroup(g))
                    covers = grp->fixtureList().contains(fixtureId);
        if (!covers)
            continue;
        for (quint32 pid : s->palettes())
        {
            if (!runner->isPlaying(pid) || runner->sceneOf(pid) != s->id())
                continue;
            QLCPalette *p = doc->palette(pid);
            const QString n = p != nullptr ? p->name() : QString();
            if (!n.isEmpty() && !names.contains(n))
            {
                names << n;
                if (sceneId != nullptr && *sceneId == kInvalid)
                    *sceneId = s->id();
            }
        }
    }
    return names;
}

/** v2: who a fader that let go of a channel was, in words ("cue 1 One ·
 *  layer A", "the programmer"); fills the line's kind and where it goes */
QString writerName(Doc *doc, const Universe::LetGo &g, const QString &key, WhyOn::Line *l)
{
    ShowTreeController *st = doc->showTree();
    const QString &n = g.name;
    if (n == QLatin1String("Mark"))
    {
        l->kind = WhyOn::Kind::Mark;
        return WhyOn::tr("a move in black (Mark)");
    }
    if (g.priority == Universe::Park || n == QLatin1String("Park"))
    {
        l->kind = WhyOn::Kind::Park;
        return WhyOn::tr("Park");
    }
    if (g.priority == Universe::Focus)
    {
        l->kind = WhyOn::Kind::Focus;
        return n == QLatin1String("Refine") ? WhyOn::tr("Refine") : WhyOn::tr("Quick focus");
    }
    if (g.priority == Universe::Flash)
    {
        l->kind = WhyOn::Kind::Flash;
        return WhyOn::tr("Flash (identify)");
    }
    if (g.priority == Universe::Highlight)
    {
        l->kind = WhyOn::Kind::Highlight;
        return WhyOn::tr("Highlight");
    }
    if (n == QLatin1String("Last look"))
    {
        l->kind = WhyOn::Kind::LastLook;
        return WhyOn::tr("the last look");
    }
    if (n == QLatin1String("EffectScript"))
    {
        l->kind = WhyOn::Kind::Effect;
        return WhyOn::tr("an effect");
    }
    if (n == QLatin1String("FixtureTest") || n == QLatin1String("FixtureLocate"))
    {
        l->kind = WhyOn::Kind::Locate;
        return WhyOn::tr("Locate / fixture test");
    }
    if (g.priority == Universe::SimpleDesk && g.functionId == Function::invalidId())
    {
        l->kind = WhyOn::Kind::SimpleDesk;
        return n.isEmpty() || n == QLatin1String("Simple Desk") ? WhyOn::tr("the Simple Desk") : n;
    }
    if (g.priority == Universe::Tracked)
    {
        l->kind = WhyOn::Kind::TrackedRestack;
        return WhyOn::tr("cue list %1 (its tracked state)").arg(quoted(n.section(QStringLiteral(" (tracked)"), 0, 0)));
    }
    if (g.functionId != Function::invalidId())
    {
        Function *f = doc->function(g.functionId);
        if (f == nullptr)
            return WhyOn::tr("a function since deleted");
        l->functionId = f->id();
        CuePlace place;
        if (findPlace(st, f->id(), key, false, &place))
        {
            l->kind = place.hit ? WhyOn::Kind::Hit : WhyOn::Kind::Cue;
            l->cueId = place.cueId;
            l->layerId = place.layerId;
            const QString layer = place.layerName.isEmpty() ? QString()
                                  : WhyOn::tr(" · layer %1").arg(place.layerName);
            return place.hit ? WhyOn::tr("hit %1%2").arg(quoted(cueText(st, place.cueId)), layer)
                             : WhyOn::tr("cue %1%2").arg(cueText(st, place.cueId), layer);
        }
        l->kind = WhyOn::Kind::Function;
        return WhyOn::tr("%1 %2").arg(Function::typeToString(f->type()), quoted(f->name()));
    }
    if (n.startsWith(QLatin1String("VC ")))
    {
        l->kind = WhyOn::Kind::VCWidget;
        bool ok = false;
        const quint32 wid = n.mid(3).toUInt(&ok);
        if (ok)
            l->widgetId = wid;
        const QString who = ok && widgetNamer() ? widgetNamer()(wid) : QString();
        return who.isEmpty() ? WhyOn::tr("a Virtual Console slider") : who;
    }
    if (n == QLatin1String("Programmer"))
    {
        l->kind = WhyOn::Kind::Programmer;
        return WhyOn::tr("the programmer");
    }
    if (!n.isEmpty())
        return n;
    return g.alive || g.priority != 0 ? WhyOn::tr("a source with no name") : WhyOn::tr("a writer that has gone");
}

/** v3: a fixture the running cue of a list doesn't light, and why: its
 *  layer was released, or the cue has none on it. Empty: no list runs, or
 *  the running cue has a layer on it. */
QString releasedReason(ShowTreeController *st, quint32 fid, WhyOn::Line *l)
{
    if (st == nullptr)
        return QString();
    // One cue: why it doesn't light the fixture; empty: it does.
    auto check = [st, fid, l](const ShowTree::CueList &list, quint32 cueId, bool preview) -> QString {
        ShowTree::ResolvedCue rc;
        if (cueId == kInvalid || !st->resolvedCue(cueId, &rc))
            return QString();
        for (const ShowTree::ResolvedLayer &rl : rc.layers)
            if (!rl.target.isOpaque() && st->targetFixtures(rl.target).contains(fid))
                return QString();
        const int at = list.indexOf(cueId);
        const QString cue = preview ? WhyOn::tr("Cue %1 is previewed (Design)").arg(st->cueLabel(list, at))
                                    : WhyOn::tr("Cue %1 runs").arg(st->cueLabel(list, at));
        l->cueId = cueId;
        for (int i = at - 1; i >= 0; --i)
        {
            const ShowTree::Cue &c = list.cues.at(i);
            for (const ShowTree::Layer &ly : c.layers)
            {
                if (ly.target.isOpaque() || !st->targetFixtures(ly.target).contains(fid))
                    continue;
                l->cueId = c.id;
                l->layerId = ly.id;
                const QString name = ly.name.isEmpty() ? st->targetLabel(ly.target) : ly.name;
                // A skipped cue tracks as if it weren't in the list: its look
                // never played, whatever it would have lit.
                if (c.skip)
                {
                    l->kind = WhyOn::Kind::Skipped;
                    return WhyOn::tr("%1, and cue %2 is skipped tonight, so its look (layer %3) never played")
                            .arg(cue, st->cueLabel(list, i), name);
                }
                if (ly.release)
                    return WhyOn::tr("%1, and layer %2 was released leaving cue %3")
                            .arg(cue, name, st->cueLabel(list, i));
                return WhyOn::tr("%1, and has no layer on this light (layer %2 last lit it, in cue %3)")
                        .arg(cue, name, st->cueLabel(list, i));
            }
        }
        return WhyOn::tr("%1, and no cue up to it lights this light").arg(cue);
    };
    // A preview is Design's; in Operate the cue open for edit is blind.
    const ShowTreeController::PreviewState ps = st->previewState();
    const quint32 previewed = (ps == ShowTreeController::PreviewState::Playing
                               || ps == ShowTreeController::PreviewState::Dark)
                              ? st->previewedCue() : kInvalid;
    if (const ShowTree::CueList *list = previewed != kInvalid ? st->listOfCue(previewed) : nullptr)
        if (!list->looks)
        {
            const QString why = check(*list, previewed, true);
            if (!why.isEmpty())
                return why;
        }
    for (const ShowTree::Production &p : st->productions())
        for (const ShowTree::CueList &list : p.lists)
        {
            if (list.looks)
                continue;
            const QString why = check(list, st->runningCue(list.id), false);
            if (!why.isEmpty())
                return why;
        }
    return QString();
}

/** Solo (the Show Tree preview's, the pad walk's) plays one layer of the
 *  previewed cue; this light is on another. Empty: not that. */
QString soloReason(ShowTreeController *st, quint32 fid, WhyOn::Line *l)
{
    if (st == nullptr || st->previewSoloLayer() == kInvalid)
        return QString();
    const ShowTreeController::PreviewState ps = st->previewState();
    if (ps != ShowTreeController::PreviewState::Playing && ps != ShowTreeController::PreviewState::Dark)
        return QString();
    const quint32 cueId = st->previewedCue();
    const ShowTree::CueList *list = cueId != kInvalid ? st->listOfCue(cueId) : nullptr;
    ShowTree::ResolvedCue rc;
    if (list == nullptr || !st->resolvedCue(cueId, &rc))
        return QString();
    // The soloed layer and its child layers, as the preview plays them.
    QList<ShowTree::Target> playing;
    QString soloName;
    for (const ShowTree::ResolvedLayer &rl : rc.layers)
        if (rl.layerId == st->previewSoloLayer() && rl.depth == 0)
        {
            playing << rl.target;
            soloName = rl.name.isEmpty() ? st->targetLabel(rl.target) : rl.name;
        }
    if (playing.isEmpty())
        return QString();
    for (bool more = true; more; )
    {
        more = false;
        for (const ShowTree::ResolvedLayer &rl : rc.layers)
            if (rl.depth > 0 && playing.contains(rl.parent) && !playing.contains(rl.target))
            {
                playing << rl.target;
                more = true;
            }
    }
    bool onOther = false;
    for (const ShowTree::ResolvedLayer &rl : rc.layers)
    {
        if (rl.target.isOpaque() || !st->targetFixtures(rl.target).contains(fid))
            continue;
        if (playing.contains(rl.target))
            return QString();       // the soloed layer has it: not Solo's doing
        onOther = true;
    }
    if (!onOther)
        return QString();
    l->cueId = cueId;
    l->layerId = st->previewSoloLayer();
    return WhyOn::tr("Solo is on: only layer %1 of cue %2 plays, so this light stays dark")
            .arg(soloName, st->cueLabel(*list, list->indexOf(cueId)));
}

/** In Operate the cue open for edit is blind: it reaches the stage only on
 *  GO. This light is in it. Empty: not that. */
QString blindEditReason(ShowTreeController *st, quint32 fid, WhyOn::Line *l)
{
    if (st == nullptr || st->previewState() != ShowTreeController::PreviewState::NotDesign)
        return QString();
    const quint32 cueId = st->previewedCue();
    const ShowTree::CueList *list = cueId != kInvalid ? st->listOfCue(cueId) : nullptr;
    ShowTree::ResolvedCue rc;
    if (list == nullptr || list->looks || !st->resolvedCue(cueId, &rc))
        return QString();
    bool lit = false;
    for (const ShowTree::ResolvedLayer &rl : rc.layers)
        if (!rl.target.isOpaque() && st->targetFixtures(rl.target).contains(fid))
            lit = true;
    if (!lit)
        return QString();
    l->cueId = cueId;
    return WhyOn::tr("Cue %1 is being edited blind (Operate): the edit reaches the stage only on GO")
            .arg(st->cueLabel(*list, list->indexOf(cueId)));
}

/** A shutter value that closes it: a Close capability, or one named so */
const QLCCapability *closedCapability(const QLCChannel *ch, uchar value)
{
    if (ch == nullptr)
        return nullptr;
    const QLCCapability *cap = ch->searchCapability(value);
    if (cap == nullptr)
        return nullptr;
    if (cap->preset() == QLCCapability::ShutterClose)
        return cap;
    const QString n = cap->name().toLower();
    if (n.contains(QLatin1String("open")))
        return nullptr;
    if (n.contains(QLatin1String("close")) || n.contains(QLatin1String("blackout"))
        || n.contains(QLatin1String("shut")))
        return cap;
    return nullptr;
}

QString fadeNote(const FadeChannel &fc, quint32 address)
{
    if (fc.isReady() || fc.fadeTime() == 0 || fc.fadeTime() == UINT_MAX)
        return QString();
    const uchar from = byteOf(fc, fc.start(), address);
    const uchar to = byteOf(fc, fc.target(), address);
    if (from == to)
        return QString();
    return WhyOn::tr(", fading %1 → %2 over %3 s")
            .arg(WhyOn::percent(from), WhyOn::percent(to))
            .arg(QString::number(fc.fadeTime() / 1000.0, 'f', 1));
}

} // namespace

/****************************************************************************
 * WhyOn
 ****************************************************************************/

void WhyOn::setWidgetNamer(const std::function<QString(quint32)> &namer)
{
    widgetNamer() = namer;
}

QString WhyOn::percent(int dmx)
{
    return tr("%1 %").arg(qRound(qreal(qBound(0, dmx, 255)) * 100.0 / 255.0));
}

QString WhyOn::Answer::toText() const
{
    QStringList out;
    if (!subject.isEmpty())
        out << subject;
    out << headline;
    for (const Line &l : whyNot)
        out << QStringLiteral("  ✖ ") + l.text;
    for (const Line &l : lines)
        out << QStringLiteral("  • ") + l.text;
    if (approximate)
        out << WhyOn::tr("  (≈ approximate: read between frames, or a blend the replay can only estimate)");
    return out.join(QLatin1Char('\n'));
}

QStringList WhyOn::Answer::texts() const
{
    QStringList out;
    for (const Line &l : lines)
        out << l.text;
    return out;
}

QStringList WhyOn::Answer::whyNotTexts() const
{
    QStringList out;
    for (const Line &l : whyNot)
        out << l.text;
    return out;
}

quint32 WhyOn::defaultChannel(Doc *doc, quint32 fixtureId)
{
    Fixture *fxi = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    if (fxi == nullptr || fxi->channels() == 0)
        return QLCChannel::invalid();
    const quint32 master = fxi->masterIntensityChannel();
    if (master != QLCChannel::invalid())
        return master;
    for (quint32 c = 0; c < fxi->channels(); ++c)
        if (const QLCChannel *ch = fxi->channel(c))
            if (ch->group() == QLCChannel::Intensity)
                return c;
    return 0;
}

WhyOn::Answer WhyOn::explain(Doc *doc, quint32 fixtureId, quint32 channel, Wording wording)
{
    Answer a;
    Fixture *fxi = doc != nullptr ? doc->fixture(fixtureId) : nullptr;
    if (fxi == nullptr)
        return a;
    if (channel == QLCChannel::invalid())
        channel = defaultChannel(doc, fixtureId);
    if (channel == QLCChannel::invalid() || channel >= fxi->channels())
        return a;
    if (!fxi->isPatched() || fxi->address() == QLCChannel::invalid() || fxi->universe() == Universe::invalid())
    {
        a.fixtureId = fixtureId;
        a.channel = channel;
        a.subject = fxi->name();
        a.headline = tr("Not patched: it has no address, so nothing reaches it.");
        Line l;
        l.kind = Kind::NotPatched;
        l.verdict = Verdict::Blocks;
        l.text = tr("It isn't patched: give it an address first (Fixture Manager)");
        a.whyNot << l;
        return a;
    }
    a = explainAddress(doc, fxi->universe(), int(fxi->address() + channel), wording);
    return a;
}

WhyOn::Answer WhyOn::explainAddress(Doc *doc, quint32 universe, int address, Wording wording)
{
    return explainAt(doc, universe, address, wording, true);
}

WhyOn::Answer WhyOn::explainAt(Doc *doc, quint32 universe, int address, Wording wording, bool whyNot)
{
    Answer a;
    if (doc == nullptr || address < 0 || address >= 512)
        return a;
    InputOutputMap *io = doc->inputOutputMap();
    Universe *uni = io->universe(universe);
    if (uni == nullptr)
        return a;
    const bool simple = wording == Wording::Simple;

    a.universe = universe;
    a.address = address;

    // The fixture there, if any, for its name and the Show Tree key.
    const quint32 fid = doc->fixtureForAddress((universe << 9) | quint32(address));
    Fixture *fxi = doc->fixture(fid);
    const QLCChannel *qch = nullptr;
    if (fxi != nullptr)
    {
        a.fixtureId = fxi->id();
        a.channel = quint32(address) - fxi->address();
        qch = fxi->channel(a.channel);
        a.subject = qch != nullptr ? QStringLiteral("%1 · %2").arg(fxi->name(), qch->name())
                                   : fxi->name();
    }
    else
    {
        a.subject = tr("Universe %1, channel %2").arg(universe + 1).arg(address + 1);
    }
    const QString key = keyFor(qch);

    /* The stage values first, then the faders: each under its own lock,
       never both (the frame lock is taken before the faders' lock, and
       the UI must not hold either while it calls into the Doc). */
    const Universe::ChannelSnapshot snap = uni->channelSnapshot(address);
    a.preGM = snap.preGM;
    a.out = snap.postGM;
    const bool htp = snap.mask & Universe::HTP;
    const bool intensityCh = snap.mask & Universe::Intensity;

    QList<Source> sources;
    const QList<QSharedPointer<GenericFader> > faders = uni->faders();
    for (const QSharedPointer<GenericFader> &f : faders)
    {
        if (f.isNull() || !f->isEnabled() || f->deleteRequested())
            continue;
        Source s;
        if (!f->peekAddress(quint32(address), &s.fc))
            continue;
        s.priority = f->priority();
        s.name = f->name();
        s.functionId = f->parentFunctionID();
        s.flags = s.fc.flags();
        s.intensity = f->intensity() * f->parentIntensity();
        s.own = ownValue(s.fc, s.intensity, f->intensity(), f->parentIntensity(), quint32(address));
        s.hitMix = f->overrideMix(&s.hit);
        s.approx = f->blendMode() != Universe::NormalBlend;
        sources << s;
    }

    /* The replay: the universe's merge, in the order it writes. An
       intensity starts the frame at 0; any other channel keeps what it had. */
    int cur = intensityCh ? 0 : snap.preGM;
    int winner = -1;
    bool overrideWritten = false;
    for (int i = 0; i < sources.count(); ++i)
    {
        Source &s = sources[i];
        int v = s.own;
        bool forced = false;
        if (s.hit)
        {
            v = cur + qRound((v - cur) * qMax(qreal(0), s.hitMix));
            forced = true;
        }
        else if (s.flags & FadeChannel::Override)
        {
            if (s.flags & FadeChannel::Mix)
            {
                const float under = float(cur) * s.fc.mixScale();
                float own = float(v);
                if ((s.flags & FadeChannel::MixHTP) && under > own)
                    own = under;
                v = qBound(0, qRound(under + (own - under) * s.fc.mixDepth()), 255);
            }
            if ((s.flags & FadeChannel::EditOverride) && htp && overrideWritten)
                v = qMax(v, cur);
            if (s.flags & FadeChannel::EditOverride)
                overrideWritten = true;
            forced = true;
        }
        else if (s.flags & FadeChannel::Relative)
        {
            s.approx = true;
            s.verdict = Verdict::Info;
            continue;
        }
        else if (s.flags & FadeChannel::Flashing)
        {
            forced = (s.flags & FadeChannel::ForceLTP) || !htp;
        }
        else
        {
            forced = !htp;
        }

        if (!forced && v < cur)
        {
            s.verdict = Verdict::Lower;
            s.beatenBy = winner;
            continue;
        }
        if (!forced && v == cur && winner >= 0)
        {
            // The same level: the one already there keeps it (HTP).
            s.verdict = Verdict::Lower;
            s.beatenBy = winner;
            continue;
        }
        if (winner >= 0)
        {
            Source &w = sources[winner];
            if (s.hit || (s.flags & FadeChannel::Override))
                w.verdict = Verdict::Replaced;
            else
                w.verdict = forced ? Verdict::Under : Verdict::Lower;
            w.beatenBy = i;
        }
        s.verdict = Verdict::Wins;
        winner = i;
        cur = v;
    }
    for (const Source &s : sources)
        if (s.approx)
            a.approximate = true;
    if (winner >= 0 && cur != snap.preGM)
        a.approximate = true;      // a frame moved on between the reads

    ShowTreeController *st = doc->showTree();

    // Name each source, top-down: the last written (highest) first.
    QString winnerText;
    for (int i = sources.count() - 1; i >= 0; --i)
    {
        const Source &s = sources.at(i);
        Line l;
        l.verdict = s.verdict;
        l.value = s.own;
        const QString level = percent(s.own);
        QString what;

        if (s.name == QLatin1String("Mark"))
        {
            l.kind = Kind::Mark;
            what = tr("A move in black (Mark) presets it to %1, while the light is dark").arg(level);
        }
        else if (s.priority == Universe::Park || s.name == QLatin1String("Park"))
        {
            l.kind = Kind::Park;
            what = tr("Park holds it at %1").arg(level);
        }
        else if (s.priority == Universe::Focus)
        {
            l.kind = Kind::Focus;
            what = s.name == QLatin1String("Refine") ? tr("Refine (walking the show) holds it at %1").arg(level)
                                                     : tr("Quick focus holds it at %1").arg(level);
        }
        else if (s.priority == Universe::Flash)
        {
            l.kind = Kind::Flash;
            what = tr("Flash (identify) holds it at %1").arg(level);
        }
        else if (s.priority == Universe::Highlight)
        {
            l.kind = Kind::Highlight;
            what = tr("Highlight holds it at %1").arg(level);
        }
        else if (s.priority == Universe::Hold || s.name == QLatin1String("Last look"))
        {
            l.kind = Kind::LastLook;
            what = tr("Last look (held after the show stopped) sets %1").arg(level);
        }
        else if (s.name == QLatin1String("EffectScript"))
        {
            l.kind = Kind::Effect;
            quint32 sceneId = kInvalid;
            const QStringList names = fxi != nullptr ? effectsOn(doc, fxi->id(), &sceneId) : QStringList();
            l.functionId = sceneId;
            const QString who = names.isEmpty() ? tr("An effect") : tr("Effect %1").arg(quoted(names.join(QStringLiteral(", "))));
            if (s.flags & FadeChannel::Mix)
                what = tr("%1 moves it to %2, at %3 depth (fading)")
                        .arg(who, level).arg(tr("%1 %").arg(qRound(s.fc.mixDepth() * 100.0f)));
            else
                what = tr("%1 moves it to %2, at full depth").arg(who, level);
        }
        else if (s.name == QLatin1String("FixtureTest") || s.name == QLatin1String("FixtureLocate"))
        {
            l.kind = Kind::Locate;
            what = tr("Locate / fixture test sets %1").arg(level);
        }
        else if (s.priority == Universe::SimpleDesk && s.functionId == Function::invalidId())
        {
            l.kind = Kind::SimpleDesk;
            l.absAddress = (universe << 9) | quint32(address);
            what = s.name.isEmpty() || s.name == QLatin1String("Simple Desk")
                   ? tr("Simple Desk sets %1").arg(level)
                   : tr("%1 sets %2").arg(s.name, level);
        }
        else if (s.priority == Universe::Tracked)
        {
            l.kind = Kind::TrackedRestack;
            what = tr("Cue list %1 put its tracked state back (after Back or a jump): %2")
                    .arg(quoted(s.name.section(QStringLiteral(" (tracked)"), 0, 0)), level);
        }
        else if (s.functionId != Function::invalidId() && doc->function(s.functionId) != nullptr)
        {
            Function *f = doc->function(s.functionId);
            l.functionId = f->id();
            CuePlace place;
            const bool previewFirst = f->startedOnlyByPreview() || (s.flags & FadeChannel::EditOverride);
            if (findPlace(st, f->id(), key, previewFirst, &place))
            {
                l.cueId = place.cueId;
                l.layerId = place.layerId;
                const QString cue = cueText(st, place.cueId);
                const QString layer = place.layerName.isEmpty() ? QString()
                                      : tr(" · layer %1").arg(place.layerName);
                if (place.hit)
                {
                    l.kind = Kind::Hit;
                    const bool over = s.hit;
                    if (over && s.hitMix < 0.999)
                        what = tr("Override hit %1%2 is pulling it to %3 (%4 in)")
                                .arg(quoted(cue), layer, level).arg(tr("%1 %").arg(qRound(s.hitMix * 100)));
                    else if (over)
                        what = tr("Override hit %1%2 holds it at %3").arg(quoted(cue), layer, level);
                    else
                        what = tr("Hit %1%2 sets %3").arg(quoted(cue), layer, level);
                }
                else
                {
                    l.kind = place.preview ? Kind::Preview : Kind::Cue;
                    what = place.preview
                           ? tr("Preview of cue %1%2 (Design: selected, not a GO) sets %3").arg(cue, layer, level)
                           : tr("Cue %1%2 sets %3").arg(cue, layer, level);
                    const QString from = cueText(st, place.fromCue);
                    switch (place.origin)
                    {
                        case ShowTree::Origin::Tracked:
                            what += tr(", tracked from cue %1").arg(from);
                            break;
                        case ShowTree::Origin::Included:
                            what += tr(", from included cue %1").arg(from);
                            break;
                        case ShowTree::Origin::Base:
                            what += tr(", back to the base cue %1 (the layer is released)").arg(from);
                            break;
                        case ShowTree::Origin::CueLook:
                            what += tr(", from the cue's own look");
                            break;
                        case ShowTree::Origin::Own:
                        default:
                            break;
                    }
                }
            }
            else
            {
                const bool isEffect = f->type() == Function::EFXType || f->type() == Function::RGBMatrixType;
                l.kind = isEffect ? Kind::Effect : Kind::Function;
                const QString label = st != nullptr && st->isGenerated(f->id()) && !st->describeGenerated(f->id()).isEmpty()
                                      ? st->describeGenerated(f->id())
                                      : tr("%1 %2").arg(Function::typeToString(f->type()), quoted(f->name()));
                what = isEffect ? tr("Effect %1 moves it: %2").arg(label, level)
                                : tr("%1 sets %2").arg(label, level);
                const QString by = startedBy(f);
                if (!by.isEmpty())
                {
                    what += tr(" (started by %1)").arg(by);
                    for (const FunctionParent &p : f->sources())
                        if (p.type() == FunctionParent::ManualVCWidget && !p.isPreview())
                            l.widgetId = p.id();
                }
            }
            if (s.intensity < 0.999 && (s.flags & FadeChannel::Intensity) && !s.hit)
                what += tr(", at %1 intensity").arg(tr("%1 %").arg(qRound(s.intensity * 100)));
        }
        else if (s.name.startsWith(QLatin1String("VC ")))
        {
            l.kind = Kind::VCWidget;
            bool ok = false;
            const quint32 wid = s.name.mid(3).toUInt(&ok);
            QString who = ok && widgetNamer() ? widgetNamer()(wid) : QString();
            if (ok)
                l.widgetId = wid;
            if (who.isEmpty())
                who = tr("A Virtual Console slider");
            what = tr("%1 sets %2").arg(who, level);
        }
        else if (s.name == QLatin1String("Programmer"))
        {
            l.kind = Kind::Programmer;
            what = tr("The programmer sets %1").arg(level);
        }
        else if (!s.name.isEmpty())
        {
            l.kind = Kind::Other;
            what = tr("%1 sets %2").arg(s.name, level);
        }
        else
        {
            l.kind = Kind::Other;
            what = tr("A source with no name sets %1").arg(level);
        }
        what += fadeNote(s.fc, quint32(address));

        // Why it loses, in crew words.
        if (s.verdict != Verdict::Wins && s.verdict != Verdict::Info)
        {
            switch (s.verdict)
            {
                case Verdict::Lower:
                    what += simple ? tr(" (lower, so it doesn't show)")
                                   : tr(" (lower: on this channel the highest level wins, HTP)");
                    break;
                case Verdict::Under:
                    what += simple ? tr(" (something above takes over)")
                                   : tr(" (under: a later source sets it, LTP)");
                    break;
                case Verdict::Replaced:
                    what += tr(" (replaced by what is above it)");
                    break;
                default:
                    break;
            }
        }
        if (s.verdict == Verdict::Wins)
            winnerText = what;
        l.text = what;
        a.lines << l;
    }

    if (sources.isEmpty())
    {
        Line l;
        l.kind = Kind::Nothing;
        l.verdict = Verdict::Info;
        const Universe::LetGo gone = intensityCh ? Universe::LetGo() : uni->letGo(address);
        if (intensityCh)
            l.text = tr("Nothing sets it now: an intensity is dark unless something sets it");
        else if (gone.isValid())
        {
            // v2: the writer that left it there, if nothing wrote it since.
            l.kind = Kind::LeftBehind;
            const QString who = writerName(doc, gone, key, &l);
            const Kind named = l.kind;
            l.kind = Kind::LeftBehind;
            if (named == Kind::SimpleDesk)
                l.absAddress = (universe << 9) | quint32(address);
            l.value = a.preGM;
            const QString ago = QString::number(gone.framesAgo / qreal(MasterTimer::frequency()), 'f', 1);
            l.text = simple ? tr("Nothing sets it now: %1 left it at %2, %3 s ago, and it stays there")
                              .arg(who, percent(a.preGM), ago)
                            : tr("Nothing sets it now: %1 left it at %2, %3 s ago (it keeps the last value it was given)")
                              .arg(who, percent(a.preGM), ago);
        }
        else if (snap.framesSinceWrite > 1 && snap.framesSinceWrite < 0x7FFFFFFF)
            l.text = tr("Nothing sets it now: it kept the last value it was given (%1 s ago)")
                    .arg(QString::number(snap.framesSinceWrite / qreal(MasterTimer::frequency()), 'f', 1));
        else
            l.text = tr("Nothing sets it now: it kept the last value it was given");
        winnerText = l.text;
        a.lines << l;
    }

    // The stages after the faders, in the order the universe applies them.
    int level = a.preGM;
    const bool parked = !sources.isEmpty() && sources.last().priority == Universe::Park
                        && sources.last().verdict == Verdict::Wins;

    /* Group masters scale an intensity before the Grand Master (the two
       multiply), never a parked, Ignore-GM or relay channel. */
    const bool mastersApply = level > 0 && intensityCh && !parked && !snap.pastGM
                              && !(snap.mask & (Universe::GMExempt | Universe::SwitchOutput));
    if (snap.groupMaster != 255 && mastersApply)
    {
        Line l;
        l.kind = Kind::GroupMaster;
        l.verdict = Verdict::Scales;
        QStringList names;
        if (fxi != nullptr)
            for (FixtureGroup *grp : doc->groupMasters()->groups())
                if (grp->fixtureList().contains(fxi->id()) && grp->masterLevel() != 255)
                {
                    names << tr("%1 at %2").arg(quoted(grp->name()), percent(grp->masterLevel()));
                    if (l.groupId == kInvalid)
                        l.groupId = grp->id();
                }
        level = int((uint(level) * snap.groupMaster + 127) / 255);
        if (names.count() == 1)
            l.text = tr("Group master %1 scales it to %2").arg(names.first(), percent(level));
        else if (names.count() > 1)
            l.text = tr("Group masters %1 scale it to %2").arg(names.join(tr(" \u00D7 ")), percent(level));
        else
            l.text = tr("A group master at %1 scales it to %2").arg(percent(snap.groupMaster), percent(level));
        a.lines << l;
    }

    const uchar gmValue = io->grandMasterValue();
    const bool gmApplies = (io->grandMasterChannelMode() == GrandMaster::Intensity && intensityCh)
                           || io->grandMasterChannelMode() == GrandMaster::AllChannels;
    if (gmApplies && gmValue < 255)
    {
        Line l;
        l.kind = Kind::GrandMaster;
        const QString gmLevel = percent(gmValue);
        if (parked || snap.pastGM)
        {
            l.verdict = Verdict::Info;
            l.text = tr("Grand Master is at %1, but Park goes past it").arg(gmLevel);
        }
        else if (snap.mask & (Universe::GMExempt | Universe::SwitchOutput))
        {
            l.verdict = Verdict::Info;
            l.text = tr("Grand Master is at %1, but this light ignores it (output rules)").arg(gmLevel);
        }
        else if (level > 0)
        {
            l.verdict = Verdict::Scales;
            if (io->grandMasterValueMode() == GrandMaster::Limit)
            {
                level = qMin(level, int(gmValue));
                l.text = tr("Grand Master at %1 limits it to %2").arg(gmLevel, percent(level));
            }
            else
            {
                level = int(qFloor(level * (gmValue / 255.0) + 0.5));
                l.text = tr("Grand Master at %1 scales it to %2").arg(gmLevel, percent(level));
            }
        }
        if (!l.text.isEmpty())
            a.lines << l;
    }
    if (!snap.modifier.isEmpty())
    {
        Line l;
        l.kind = Kind::Modifier;
        l.verdict = Verdict::Scales;
        l.text = tr("Channel modifier %1 changes it (curve)").arg(quoted(snap.modifier));
        a.lines << l;
    }
    if (snap.passthrough > 0 && snap.passthrough >= a.out && snap.passthrough > level)
    {
        Line l;
        l.kind = Kind::Passthrough;
        l.verdict = Verdict::Scales;
        l.text = tr("Input passthrough raises it to %1").arg(percent(snap.passthrough));
        a.lines << l;
    }
    if (io->outputInhibited())
    {
        Line l;
        l.kind = Kind::Blind;
        l.verdict = Verdict::Blocks;
        l.text = (snap.mask & Universe::BlackoutExempt)
                 ? tr("Blind is on, but this light is exempt from Blackout and Blind (output rules)")
                 : tr("Blind is on: the rig doesn't get it (the screens still show it)");
        if (!(snap.mask & Universe::BlackoutExempt))
            a.sent = false;
        a.lines << l;
    }
    if (doc->teachingSettings() != nullptr && doc->teachingSettings()->holdsOutput())
    {
        Line l;
        l.kind = Kind::Blind;
        l.verdict = Verdict::Blocks;
        l.text = tr("Teaching mode holds network output: a networked rig doesn't get it until the teacher lets it through");
        a.lines << l;
    }
    if (io->blackout())
    {
        Line l;
        l.kind = Kind::Blackout;
        l.verdict = Verdict::Blocks;
        l.text = (snap.mask & Universe::BlackoutExempt)
                 ? tr("Blackout is on, but this light is exempt (output rules)")
                 : tr("Blackout is on: the rig gets 0");
        if (!(snap.mask & Universe::BlackoutExempt))
            a.sent = false;
        a.lines << l;
    }

    if (whyNot && fxi != nullptr)
        addWhyNot(doc, a);

    // The headline: where it is, and the one thing that set it -- or, dark,
    // the first reason it is not on (no output only when it's the one: the
    // screens show the light without it).
    QString head = simple ? tr("It's at %1.").arg(percent(a.out)) : tr("At %1.").arg(percent(a.out));
    if (!a.sent)
        head += simple ? tr(" The rig doesn't get it right now.") : tr(" Not sent (see below).");
    const Line *reason = nullptr;
    for (const Line &l : a.whyNot)
        if (reason == nullptr && l.kind != Kind::NoOutput)
            reason = &l;
    if (reason == nullptr && !a.whyNot.isEmpty())
        reason = &a.whyNot.first();
    if (reason != nullptr)
        head += simple ? tr(" It isn't on: %1.").arg(reason->text) : tr(" Not on: %1.").arg(reason->text);
    else if (!winnerText.isEmpty())
    {
        QString w = winnerText;
        if (simple)
            head += tr(" %1.").arg(w);
        else
            head += QLatin1Char(' ') + w + QLatin1Char('.');
    }
    a.headline = head;
    return a;
}

void WhyOn::addWhyNot(Doc *doc, Answer &a)
{
    Fixture *fxi = doc->fixture(a.fixtureId);
    InputOutputMap *io = doc->inputOutputMap();
    Universe *uni = io->universe(a.universe);
    if (fxi == nullptr || uni == nullptr)
        return;
    const QLCChannel *asked = fxi->channel(a.channel);
    if (asked == nullptr
        || (asked->group() != QLCChannel::Intensity && asked->group() != QLCChannel::Shutter))
        return;

    // The light's parts: its dimmer, its colour emitters, its shutters.
    const quint32 base = fxi->address();
    quint32 dim = fxi->masterIntensityChannel();
    QList<quint32> emitters, shutters;
    for (quint32 c = 0; c < fxi->channels(); ++c)
    {
        const QLCChannel *ch = fxi->channel(c);
        if (ch == nullptr)
            continue;
        if (ch->group() == QLCChannel::Intensity && ch->colour() != QLCChannel::NoColour)
            emitters << c;
        else if (ch->group() == QLCChannel::Intensity && dim == QLCChannel::invalid())
            dim = c;
        else if (ch->group() == QLCChannel::Shutter)
            shutters << c;
    }
    auto outOf = [&](quint32 c) -> int {
        return c == a.channel ? int(a.out) : int(uni->channelSnapshot(int(base + c)).postGM);
    };
    const int dimOut = dim != QLCChannel::invalid() ? outOf(dim) : -1;
    const bool dimDark = dimOut >= 0 && dimOut <= kDark;
    QList<quint32> closed;
    for (quint32 c : shutters)
        if (closedCapability(fxi->channel(c), uchar(outOf(c))) != nullptr)
            closed << c;
    bool colourBlack = !emitters.isEmpty();
    for (quint32 c : emitters)
        if (outOf(c) > kDark)
            colourBlack = false;
    if (!dimDark && closed.isEmpty() && !colourBlack)
        return;

    auto add = [&a](Kind kind, const QString &text) -> Line & {
        Line l;
        l.kind = kind;
        l.verdict = Verdict::Blocks;
        l.text = text;
        a.whyNot << l;
        return a.whyNot.last();
    };
    const Universe::ChannelSnapshot snap = uni->channelSnapshot(a.address);
    const bool exempt = snap.mask & Universe::BlackoutExempt;

    // Nothing reaches the rig.
    if (io->outputPatchesCount(a.universe) == 0)
        add(Kind::NoOutput, tr("Universe %1 has no output: nothing leaves the computer for it (Connections)")
                                .arg(a.universe + 1));
    if (uni->isNotOurs(a.address))
        add(Kind::NotOurs, tr("This channel isn't ours: the venue's console runs it"));
    if (io->outputInhibited() && !exempt)
        add(Kind::Blind, tr("Blind is on: the rig gets nothing (the screens still show it)"));
    if (doc->teachingSettings() != nullptr && doc->teachingSettings()->holdsOutput())
        add(Kind::Blind, tr("Teaching mode holds network output until the teacher lets it through"));
    if (io->blackout() && !exempt)
        add(Kind::Blackout, tr("Blackout is on"));

    // The masters, the limiter.
    const bool ruled = snap.mask & (Universe::GMExempt | Universe::SwitchOutput);
    if (dimDark && io->grandMasterValue() == 0 && !ruled && !snap.pastGM)
        add(Kind::GrandMaster, tr("The Grand Master is at 0 %"));
    if (dimDark && !ruled && !snap.pastGM)
        for (FixtureGroup *grp : doc->groupMasters()->groups())
            if (grp->masterLevel() == 0 && grp->fixtureList().contains(fxi->id()))
                add(Kind::GroupMaster, tr("Group master %1 is at 0 %").arg(quoted(grp->name()))).groupId = grp->id();
    const QHash<quint32, QPair<double, double> > held = uni->powerHeldBack();
    if (held.contains(fxi->id()))
        add(Kind::PowerLimit, tr("The power limiter holds it back: it asks for %1 A, %2 A allowed")
                                  .arg(held.value(fxi->id()).first, 0, 'f', 1)
                                  .arg(held.value(fxi->id()).second, 0, 'f', 1));

    // What holds the dimmer down.
    if (dimDark)
    {
        const Answer d = dim == a.channel ? a : explainAt(doc, a.universe, int(base + dim), Wording::Crew, false);
        const Line *winner = nullptr;
        for (const Line &l : d.lines)
            if (l.verdict == Verdict::Wins)
                winner = &l;
        if (winner == nullptr)
        {
            Line l;
            Kind kind = Kind::Solo;
            QString why = soloReason(doc->showTree(), fxi->id(), &l);
            if (why.isEmpty())
            {
                kind = Kind::BlindEdit;
                why = blindEditReason(doc->showTree(), fxi->id(), &l);
            }
            if (why.isEmpty())
            {
                kind = Kind::Released;
                l = Line();
                why = releasedReason(doc->showTree(), fxi->id(), &l);
                if (l.kind == Kind::Skipped)
                    kind = Kind::Skipped;
            }
            if (!why.isEmpty())
            {
                Line &r = add(kind, why);
                r.cueId = l.cueId;
                r.layerId = l.layerId;
            }
            else
            {
                add(Kind::Nothing, tr("Nothing sets its dimmer: no cue, scene, desk or slider is on it"));
            }
        }
        else if (winner->kind == Kind::Park)
        {
            add(Kind::Park, tr("It's parked at %1 (Park beats everything)").arg(percent(winner->value)));
        }
        else if (winner->value >= 0 && winner->value <= kDark)
        {
            // Asked about the dimmer itself: the line above says it already.
            Line &r = add(winner->kind, dim == a.channel ? winner->text
                                        : tr("Its dimmer is at %1: %2").arg(percent(winner->value), winner->text));
            r.cueId = winner->cueId;
            r.layerId = winner->layerId;
            r.functionId = winner->functionId;
            r.widgetId = winner->widgetId;
            r.absAddress = winner->absAddress;
        }
    }

    // The beam itself: a closed shutter, a colour at black.
    for (quint32 c : closed)
    {
        const QLCChannel *ch = fxi->channel(c);
        const QLCCapability *cap = closedCapability(ch, uchar(outOf(c)));
        QString text = tr("Its shutter is closed (%1 at %2: %3)")
                       .arg(ch->name()).arg(outOf(c)).arg(quoted(cap != nullptr ? cap->name() : QString()));
        Line &r = add(Kind::Shutter, text);
        const Answer sh = c == a.channel ? a : explainAt(doc, a.universe, int(base + c), Wording::Crew, false);
        for (const Line &l : sh.lines)
            if (l.verdict == Verdict::Wins)
            {
                r.text += QStringLiteral(": ") + l.text;
                r.cueId = l.cueId;
                r.layerId = l.layerId;
                r.functionId = l.functionId;
                r.widgetId = l.widgetId;
            }
    }
    if (colourBlack)
    {
        QStringList names;
        for (quint32 c : emitters)
            names << fxi->channel(c)->name();
        add(Kind::ColourBlack, emitters.count() == 1
                               ? tr("Its colour is at black: %1 is at 0 %").arg(names.first())
                               : tr("Its colour is at black: %1 are all at 0 %").arg(names.join(QStringLiteral(", "))));
    }
}
