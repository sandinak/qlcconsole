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
    if (fxi->address() == QLCChannel::invalid() || fxi->universe() == Universe::invalid())
    {
        a.fixtureId = fixtureId;
        a.channel = channel;
        a.subject = fxi->name();
        a.headline = tr("Not patched: it has no address, so nothing reaches it.");
        return a;
    }
    a = explainAddress(doc, fxi->universe(), int(fxi->address() + channel), wording);
    return a;
}

WhyOn::Answer WhyOn::explainAddress(Doc *doc, quint32 universe, int address, Wording wording)
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

        if (s.priority == Universe::Park || s.name == QLatin1String("Park"))
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
        if (intensityCh)
            l.text = tr("Nothing sets it now: an intensity is dark unless something sets it");
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

    // The headline: where it is, and the one thing that set it.
    QString head = simple ? tr("It's at %1.").arg(percent(a.out)) : tr("At %1.").arg(percent(a.out));
    if (!a.sent)
        head += simple ? tr(" The rig doesn't get it right now.") : tr(" Not sent (see below).");
    if (!winnerText.isEmpty())
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
