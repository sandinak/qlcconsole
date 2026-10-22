/*
  Q Light Controller Plus
  showtreebridge.cpp

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

#include <algorithm>
#include <functional>

#include "showtreecontroller.h"
#include "qlcfixturemode.h"
#include "qlccapability.h"
#include "qlcphysical.h"
#include "qlcchannel.h"
#include "fixturegroup.h"
#include "showcontrol.h"
#include "speedmasters.h"
#include "collection.h"
#include "showfunction.h"
#include "track.h"
#include "show.h"
#include "chaser.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

using namespace ShowTree;

/* The bridge (WORKFLOW_DESIGN.md §3.5, phase W5): an old QLC+ show into the
   tree, and back. Nothing here runs on load or on its own -- each piece is
   asked for:

   - the show converter's classification (a chaser used as a cue list, or
     one used as an effect) and its plan, which is importChaser() built
     without touching the show, with the same-output check;
   - Import in place: the import, then the originals filed under
     Imported/<chaser> (originals) in the same undo step, the list linked to
     its source (CueList::source) -- and Restore original, which takes the
     list out and puts the originals back;
   - Capture into palettes: a raw scene read as layers of palettes, kept
     only where every cue still puts out what it did. */

/****************************************************************************
 * Classify
 ****************************************************************************/

static QString seconds(uint ms)
{
    if (ms % 1000 == 0)
        return QStringLiteral("%1 s").arg(ms / 1000);
    return QStringLiteral("%1 s").arg(double(ms) / 1000.0, 0, 'f', ms % 100 == 0 ? 1 : 2);
}

ShowTreeController::ChaserClass ShowTreeController::classifyChaser(quint32 chaserId, bool vcCueList) const
{
    ChaserClass out;
    const Chaser *chaser = qobject_cast<const Chaser *>(m_doc->function(chaserId));
    if (chaser == nullptr)
    {
        out.why << tr("not a chaser");
        return out;
    }
    const QList<ChaserStep> steps = chaser->steps();
    const uint infinite = Function::infiniteSpeed();

    // How long each step holds, as the chaser runs it.
    int waits = 0, notes = 0, numbers = 0, raw = 0;
    uint longest = 0;
    for (const ChaserStep &s : steps)
    {
        uint hold = 0;
        switch (chaser->durationMode())
        {
            case Chaser::Common:
                hold = chaser->duration();
                break;
            case Chaser::PerStep:
                hold = s.hold == infinite || s.duration == infinite ? infinite : s.duration;
                break;
            case Chaser::Default:
            {
                const Function *f = m_doc->function(s.fid);
                hold = f == nullptr ? 0 : f->duration();
                break;
            }
        }
        if (hold == infinite)
            ++waits;
        else
            longest = qMax(longest, hold);
        if (!s.note.isEmpty())
            ++notes;
        if (!s.cueNumber.isEmpty())
            ++numbers;
        const Scene *sc = qobject_cast<const Scene *>(m_doc->function(s.fid));
        if (sc != nullptr && !sc->values().isEmpty())
            ++raw;
    }

    QStringList cue;
    const int n = int(steps.count());
    if (waits > 0)
        cue << tr("waits for GO on %1 of %n step(s)", "", n).arg(waits);
    if (notes > 0)
        cue << tr("has notes on %n step(s)", "", notes);
    if (numbers > 0)
        cue << tr("numbers %n step(s) as cues", "", numbers);
    if (!chaser->cueListNumber().isEmpty())
        cue << tr("answers to cue-list number %1").arg(chaser->cueListNumber());
    if (m_doc->showControl() != nullptr && m_doc->showControl()->mainCueList() == chaserId)
        cue << tr("is the show's main cue list");
    if (vcCueList)
        cue << tr("plays in a VC Cue List");

    if (!cue.isEmpty())
    {
        out.kind = ChaserKind::CueList;
        out.why = cue;
        return out;
    }

    out.kind = ChaserKind::Effect;
    switch (chaser->runOrder())
    {
        case Function::Loop: out.why << tr("loops"); break;
        case Function::PingPong: out.why << tr("ping-pongs"); break;
        case Function::Random: out.why << tr("plays its steps at random"); break;
        case Function::SingleShot: out.why << tr("plays once, on its own timing"); break;
    }
    if (n > 0)
        out.why << (longest > 0 ? tr("steps of up to %1").arg(seconds(longest))
                                : tr("steps with no hold (it runs at its tempo)"));
    if (raw > 0 && raw == n)
        out.why << tr("its steps are raw scenes");
    return out;
}

QList<quint32> ShowTreeController::convertibleChasers() const
{
    QList<const Function *> fs;
    for (const Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType && f->isVisible() && !isGenerated(f->id())
            && importedInto(f->id()) == invalidId())
            fs << f;
    std::sort(fs.begin(), fs.end(), [](const Function *a, const Function *b) {
        const int c = a->name().compare(b->name(), Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a->id() < b->id();
    });
    QList<quint32> out;
    for (const Function *f : fs)
        out << f->id();
    return out;
}

ShowTreeController::ImportReport ShowTreeController::planImport(quint32 chaserId, CueList *listOut) const
{
    ImportReport report;
    CueList list;
    quint32 next = m_nextId;
    if (!buildImport(chaserId, &list, &report, [&next]() { return next++; }))
        return report;
    report.differChannels = importDifferences(chaserId, list, &report.differCues, &report.checkedCues);
    if (listOut != nullptr)
        *listOut = list;
    return report;
}

/****************************************************************************
 * Import in place, Restore original
 ****************************************************************************/

QString ShowTreeController::originalsFolder(const QString &chaserName)
{
    QString name = chaserName;
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    return QStringLiteral("Imported/%1 (originals)").arg(name);
}

ShowTreeController::ImportReport ShowTreeController::importInPlace(quint32 chaserId)
{
    ImportReport rep = importChaser(chaserId);
    if (rep.list != invalidId())
    {
        m_lastTimelines.clear();
        rep.filed = fileOriginals(rep.list);
        for (quint32 id : m_lastTimelines)
            if (const Function *f = m_doc->function(id))
                rep.timelines << f->name();
    }
    return rep;
}

/** Point every Show timeline item that plays @a from at @a to; the
    timelines changed. */
static QList<quint32> repointTimelines(Doc *doc, quint32 from, quint32 to)
{
    QList<quint32> shows;
    if (from == Function::invalidId() || to == Function::invalidId())
        return shows;
    for (Function *f : doc->functions())
    {
        Show *show = qobject_cast<Show *>(f);
        if (show == nullptr)
            continue;
        bool any = false;
        for (Track *t : show->tracks())
            for (ShowFunction *sf : t->showFunctions())
                if (sf->functionID() == from)
                {
                    sf->setFunctionID(to);
                    any = true;
                }
        if (any)
            shows << show->id();
    }
    return shows;
}

QList<quint32> ShowTreeController::originalsOf(quint32 listId, QList<quint32> *timelines) const
{
    QList<quint32> out;
    const CueList *list = cueList(listId);
    if (list == nullptr || list->inPlace || list->source == invalidId() || m_doc->function(list->source) == nullptr)
        return out;

    // The chaser and what it plays, down through collections and chasers.
    QSet<quint32> filed;
    std::function<void(quint32, int)> walk = [&](quint32 fid, int depth) {
        if (filed.contains(fid) || depth > 8 || isGenerated(fid) || importedInto(fid) != invalidId())
            return;
        const Function *f = m_doc->function(fid);
        if (f == nullptr)
            return;
        filed.insert(fid);
        if (const Collection *c = qobject_cast<const Collection *>(f))
            for (quint32 m : c->functions())
                walk(m, depth + 1);
        else if (const Chaser *ch = qobject_cast<const Chaser *>(f))
            for (const ChaserStep &s : ch->steps())
                walk(s.fid, depth + 1);
    };
    walk(list->source, 0);

    // A Show timeline that plays the chaser fires it, as a VC button does:
    // it will play the list (fileOriginals() points it there).
    QSet<quint32> shows;
    for (const Function *f : m_doc->functions())
        if (const Show *show = qobject_cast<const Show *>(f))
            for (const Track *t : show->tracks())
                for (const ShowFunction *sf : t->showFunctions())
                    if (sf->functionID() == list->source)
                        shows.insert(show->id());
    if (timelines != nullptr)
    {
        *timelines = shows.values();
        std::sort(timelines->begin(), timelines->end());
    }

    // What the tree plays as it is stays where it is: editing it still
    // changes the list.
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.isOpaque())
                        filed.remove(layer.target.function);
    // So does anything another of the show's functions plays.
    for (bool again = true; again;)
    {
        again = false;
        const QList<quint32> ids = filed.values();
        for (quint32 id : ids)
        {
            const QList<quint32> usage = const_cast<Doc *>(m_doc)->getUsage(id);
            for (int i = 0; i + 1 < usage.count(); i += 2)
            {
                const quint32 user = usage.at(i);
                if (id == list->source && shows.contains(user))
                    continue;
                if (!filed.contains(user) && !isGenerated(user))
                {
                    filed.remove(id);
                    again = true;
                    break;
                }
            }
        }
    }
    out = filed.values();
    std::sort(out.begin(), out.end());
    return out;
}

int ShowTreeController::fileOriginals(quint32 listId)
{
    CueList *list = nullptr;
    for (Production &p : m_productions)
        if ((list = p.list(listId)) != nullptr)
            break;
    if (list == nullptr || list->inPlace || list->source == invalidId() || m_undo.isEmpty())
        return 0;
    const Function *source = m_doc->function(list->source);
    if (source == nullptr)
        return 0;
    const QList<quint32> ids = originalsOf(listId);

    // A Show timeline that plays the chaser plays the list from now on
    // (and Restore original points it back).
    const quint32 from = list->source, to = list->chaser;
    const QList<quint32> timelines = repointTimelines(m_doc, from, to);

    const QString folder = originalsFolder(source->name());
    QList<CueList::Original> originals;
    for (quint32 id : ids)
    {
        Function *f = m_doc->function(id);
        CueList::Original o;
        o.function = id;
        o.path = f->path(true);
        originals << o;
    }
    list->inPlace = true;
    list->originals = originals;

    // The folders and the timelines are the show's, outside the tree: with
    // the import's step.
    Doc *doc = m_doc;
    auto file = [doc, originals, folder](bool back) {
        for (const CueList::Original &o : originals)
            if (Function *f = doc->function(o.function))
            {
                f->setPath(back ? o.path : folder);
                emit f->changed(f->id());
            }
        doc->setModified();
    };
    file(false);
    attachToLastStep([file, doc, from, to, timelines](bool undone) {
        file(undone);
        if (!timelines.isEmpty())
            repointTimelines(doc, undone ? to : from, undone ? from : to);
    });
    m_lastTimelines = timelines;
    emit changed();
    return int(originals.count());
}

quint32 ShowTreeController::importedInto(quint32 functionId) const
{
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            if (l.inPlace)
                for (const CueList::Original &o : l.originals)
                    if (o.function == functionId)
                        return l.id;
    return invalidId();
}

bool ShowTreeController::canRestoreOriginal(quint32 listId, QString *why) const
{
    auto refuse = [why](const QString &reason) {
        if (why != nullptr)
            *why = reason;
        return false;
    };
    const CueList *list = cueList(listId);
    if (list == nullptr)
        return refuse(tr("No such cue list."));
    if (list->source == invalidId())
        return refuse(tr("“%1” wasn't imported from a chaser.").arg(list->name));
    if (!list->inPlace)
        return refuse(tr("“%1” was imported beside its chaser, which is still where it was: delete the list "
                         "to go back.").arg(list->name));
    if (m_doc->function(list->source) == nullptr)
        return refuse(tr("The chaser “%1” was imported from is no longer in the show.").arg(list->name));
    return true;
}

bool ShowTreeController::restoreOriginal(quint32 listId, QString *why)
{
    if (!canRestoreOriginal(listId, why))
        return false;
    quint32 prodId = invalidId();
    const CueList *list = cueList(listId, &prodId);
    const QList<CueList::Original> originals = list->originals;
    const quint32 listChaser = list->chaser, sourceId = list->source;
    const Function *source = m_doc->function(list->source);
    const QString folder = originalsFolder(source->name());
    QSet<quint32> cues;
    for (const Cue &c : list->cues)
        cues.insert(c.id);

    // Timelines that play the list play the chaser again -- first, so the
    // bake lets go of the list's chaser.
    const QList<quint32> timelines = repointTimelines(m_doc, listChaser, sourceId);
    const bool ok = edit(tr("Restore original %1").arg(source->name()), [&](QList<Production> &ps) {
        for (int pi = 0; pi < ps.count(); ++pi)
        {
            Production &p = ps[pi];
            if (p.id != prodId)
                continue;
            // The list, and the sub-cue lists of its cues.
            for (int i = int(p.lists.count()) - 1; i >= 0; --i)
                if (p.lists.at(i).id == listId || cues.contains(p.lists.at(i).subOf))
                    p.lists.removeAt(i);
            // A production left with nothing of its own goes too.
            bool empty = true;
            for (const CueList &l : p.lists)
                if (!l.looks || !l.cues.isEmpty())
                    empty = false;
            if (empty)
                ps.removeAt(pi);
            return true;
        }
        return false;
    });
    if (!ok)
    {
        repointTimelines(m_doc, sourceId, listChaser);
        return false;
    }

    Doc *doc = m_doc;
    auto file = [doc, originals, folder](bool back) {
        for (const CueList::Original &o : originals)
            if (Function *f = doc->function(o.function))
            {
                f->setPath(back ? o.path : folder);
                emit f->changed(f->id());
            }
        doc->setModified();
    };
    file(true);
    // Done before the list goes (the timelines first): undone once the
    // tree has the list -- and its chaser -- back.
    attachToLastStep([file, doc, listChaser, sourceId, timelines](bool undone) {
        file(!undone);
        if (!timelines.isEmpty())
            repointTimelines(doc, undone ? sourceId : listChaser, undone ? listChaser : sourceId);
    }, true);
    return true;
}

/****************************************************************************
 * Capture into palettes
 ****************************************************************************/

namespace
{
QString keyOfType(QLCPalette::PaletteType type)
{
    switch (type)
    {
        case QLCPalette::Dimmer: return KeyDimmer;
        case QLCPalette::Color: return KeyColour;
        case QLCPalette::Pan:
        case QLCPalette::Tilt:
        case QLCPalette::PanTilt: return KeyPosition;
        case QLCPalette::Gobo: return KeyGobo;
        default: break;
    }
    return QString();
}

bool sameValues(QLCPalette::PaletteType type, const QVariantList &a, const QVariantList &b)
{
    if (a.count() != b.count())
        return false;
    for (int i = 0; i < a.count(); ++i)
    {
        if (type == QLCPalette::Color)
        {
            if (a.at(i).toString().compare(b.at(i).toString(), Qt::CaseInsensitive) != 0)
                return false;
        }
        else if (a.at(i).toInt() != b.at(i).toInt())
        {
            return false;
        }
    }
    return true;
}
}

ShowTreeController::CapturePlan ShowTreeController::planCapture(quint32 sceneId) const
{
    CapturePlan plan;
    plan.scene = sceneId;
    auto refuse = [&plan](const QString &why) {
        plan.ok = false;
        plan.why = why;
        return plan;
    };
    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(sceneId));
    if (scene == nullptr)
        return refuse(tr("It isn't a scene."));
    if (isGenerated(sceneId))
        return refuse(tr("It is made by the Show Tree already."));
    const QList<SceneValue> values = scene->values();
    if (values.isEmpty())
        return refuse(tr("It has no channel values of its own."));
    if (!scene->palettes().isEmpty() || !scene->fixtureGroups().isEmpty())
        return refuse(tr("It has looks as well as channel values."));
    if (scene->fadeInSpeed() != 0 || scene->fadeOutSpeed() != 0 || !scene->paletteFades().isEmpty())
        return refuse(tr("It has fade times of its own."));
    if (scene->transitionIn().isActive() || scene->transitionOut().isActive())
        return refuse(tr("It has a transition."));
    if (scene->blendMode() != Universe::NormalBlend)
        return refuse(tr("It has a blend mode."));
    if (scene->speedMaster() != SpeedMasters::Inherit)
        return refuse(tr("It is on a speed master."));

    // Where the tree plays it as it is.
    for (const Production &p : m_productions)
        for (const CueList &l : p.lists)
            for (const Cue &c : l.cues)
                for (const Layer &layer : c.layers)
                    if (layer.target.function == sceneId)
                    {
                        plan.uses << qMakePair(l.id, c.id);
                        break;
                    }

    auto isHtp = [this](quint32 fid, quint32 ch) {
        Fixture *fx = m_doc->fixture(fid);
        if (fx == nullptr)
            return false;
        if (fx->forcedHTPChannels().contains(int(ch)))
            return true;
        if (fx->forcedLTPChannels().contains(int(ch)))
            return false;
        const QLCChannel *c = fx->channel(ch);
        return c != nullptr && c->group() == QLCChannel::Intensity;
    };

    QMap<quint32, QMap<quint32, uchar>> byFixture;
    for (const SceneValue &sv : values)
        byFixture[sv.fxi].insert(sv.channel, sv.value);

    // The palettes, once each by value.
    auto paletteIndex = [&plan](QLCPalette::PaletteType type, const QVariantList &vals) {
        for (int i = 0; i < plan.palettes.count(); ++i)
            if (plan.palettes.at(i).type == type && sameValues(type, plan.palettes.at(i).values, vals))
                return i;
        CapturePalette cp;
        cp.type = type;
        cp.values = vals;
        plan.palettes << cp;
        return int(plan.palettes.count()) - 1;
    };

    QMap<quint32, QMap<QString, int>> looksOf;    // fixture -> key -> palette
    for (auto fit = byFixture.constBegin(); fit != byFixture.constEnd(); ++fit)
    {
        const quint32 fid = fit.key();
        const QMap<quint32, uchar> &vals = fit.value();
        Fixture *fx = m_doc->fixture(fid);
        QSet<quint32> covered;
        QMap<QString, int> looks;

        // Whether a look of @a type and @a vals plays exactly these values
        // on the fixture: every channel it sets is the scene's value (or an
        // intensity it leaves at 0, which adds nothing), and it sets at
        // least one the scene sets that no other look took.
        auto fits = [&](QLCPalette::PaletteType type, const QVariantList &v, QList<quint32> *chs) {
            if (fx == nullptr)
                return false;
            QLCPalette p(type);
            p.setValues(v);
            const QList<SceneValue> out = p.valuesFromFixtures(m_doc, QList<quint32>() << fid);
            bool any = false;
            for (const SceneValue &sv : out)
            {
                if (vals.contains(sv.channel))
                {
                    if (vals.value(sv.channel) != sv.value || covered.contains(sv.channel))
                        return false;
                    any = true;
                    if (!chs->contains(sv.channel))
                        *chs << sv.channel;
                }
                else if (!(sv.value == 0 && isHtp(fid, sv.channel)))
                {
                    return false;
                }
            }
            return any;
        };
        auto take = [&](QLCPalette::PaletteType type, const QVariantList &v) {
            QList<quint32> chs;
            if (!fits(type, v, &chs))
                return false;
            for (quint32 c : chs)
                covered.insert(c);
            looks.insert(keyOfType(type), paletteIndex(type, v));
            return true;
        };

        if (fx != nullptr)
        {
            // Dimmer: the level whose master / head dimmers are the scene's.
            bool dimmerSet = false;
            const quint32 master = fx->type() == QLCFixtureDef::Dimmer ? 0 : fx->masterIntensityChannel();
            if (master != QLCChannel::invalid() && vals.contains(master))
                dimmerSet = true;
            for (int h = 0; h < fx->heads(); ++h)
            {
                const quint32 c = fx->channelNumber(QLCChannel::Intensity, QLCChannel::MSB, h);
                if (c != QLCChannel::invalid() && vals.contains(c))
                    dimmerSet = true;
            }
            if (dimmerSet && QLCPalette::fixtureHasDimmer(fx))
                for (int v = 0; v <= 255; ++v)
                    if (take(QLCPalette::Dimmer, QVariantList() << v))
                        break;

            // Colour: from the mix (RGB, + white / amber / UV), CMY, or the
            // wheel slot it sits on.
            QStringList colours;
            const QVector<QVector<quint32>> sets = fx->rgbChannelSets(0);
            auto val = [&vals](quint32 c) { return c == QLCChannel::invalid() ? 0 : int(vals.value(c, 0)); };
            const quint32 wCh = fx->channelNumber(QLCChannel::White, QLCChannel::MSB, 0);
            const quint32 aCh = fx->channelNumber(QLCChannel::Amber, QLCChannel::MSB, 0);
            const quint32 uCh = fx->channelNumber(QLCChannel::UV, QLCChannel::MSB, 0);
            const int w = val(wCh), a = val(aCh), u = val(uCh);
            if (!sets.isEmpty())
            {
                const int r = val(sets.at(0).at(0)), g = val(sets.at(0).at(1)), b = val(sets.at(0).at(2));
                if (r + w <= 255 && g + w <= 255 && b + w <= 255)
                    colours << QLCPalette::colorToString(QColor(r + w, g + w, b + w), QColor(w, a, u));
                colours << QColor(r, g, b).name();
            }
            const QVector<quint32> cmy = fx->cmyChannels(0);
            if (cmy.size() == 3)
            {
                QColor c = QColor::fromCmyk(val(cmy.at(0)), val(cmy.at(1)), val(cmy.at(2)), 0).toRgb();
                colours << QLCPalette::colorToString(c, QColor(w, a, u)) << c.name();
            }
            QList<quint32> wheels = QLCPalette::colourSlotChannels(fx, 0) + QLCPalette::colourSlotChannels(fx, -1);
            for (quint32 wc : wheels)
            {
                const QLCChannel *ch = fx->channel(wc);
                if (ch == nullptr || !vals.contains(wc))
                    continue;
                const QColor c = QLCPalette::capabilityColour(ch->searchCapability(vals.value(wc)));
                if (c.isValid())
                    colours << c.name();
            }
            for (const QString &c : colours)
                if (take(QLCPalette::Color, QVariantList() << c))
                    break;

            // Position: the whole degrees that land on the scene's pan and
            // tilt, as a Position look writes them.
            const quint32 panCh = fx->channelNumber(QLCChannel::Pan, QLCChannel::MSB, 0);
            const quint32 tiltCh = fx->channelNumber(QLCChannel::Tilt, QLCChannel::MSB, 0);
            const QLCFixtureMode *mode = fx->fixtureMode();
            auto findDegrees = [&](QLCPalette::PaletteType type, quint32 ch, int maxDeg) {
                if (ch == QLCChannel::invalid() || !vals.contains(ch))
                    return -1;
                QList<quint32> chs;
                for (int d = 0; d <= maxDeg; ++d)
                {
                    chs.clear();
                    if (fits(type, QVariantList() << d, &chs))
                        return d;
                }
                return -1;
            };
            if (mode != nullptr)
            {
                int panMax = mode->physical().focusPanMax(), tiltMax = mode->physical().focusTiltMax();
                if (panMax <= 0)
                    panMax = 360;
                if (tiltMax <= 0)
                    tiltMax = 360;
                const int pan = findDegrees(QLCPalette::Pan, panCh, panMax);
                const int tilt = findDegrees(QLCPalette::Tilt, tiltCh, tiltMax);
                if (pan >= 0 && tilt >= 0)
                    take(QLCPalette::PanTilt, QVariantList() << pan << tilt);
                else if (pan >= 0)
                    take(QLCPalette::Pan, QVariantList() << pan);
                else if (tilt >= 0)
                    take(QLCPalette::Tilt, QVariantList() << tilt);
            }

            // Gobo: the wheel's raw value, as an unnamed Gobo look plays it.
            for (quint32 c = 0; c < fx->channels(); ++c)
            {
                const QLCChannel *ch = fx->channel(c);
                if (ch != nullptr && ch->group() == QLCChannel::Gobo && vals.contains(c))
                {
                    take(QLCPalette::Gobo, QVariantList() << int(vals.value(c)));
                    break;
                }
            }
        }

        QStringList rest;
        for (auto c = vals.constBegin(); c != vals.constEnd(); ++c)
        {
            if (covered.contains(c.key()))
                continue;
            plan.leftovers << SceneValue(fid, c.key(), c.value());
            const QLCChannel *ch = fx == nullptr ? nullptr : fx->channel(c.key());
            rest << QStringLiteral("%1 %2").arg(ch == nullptr ? tr("channel %1").arg(c.key() + 1) : ch->name())
                                           .arg(c.value());
        }
        plan.capturedChannels += int(covered.count());
        if (!rest.isEmpty())
            plan.leftoverLines << QStringLiteral("%1: %2").arg(fx == nullptr ? tr("fixture %1").arg(fid) : fx->name(),
                                                               rest.join(QStringLiteral(", ")));
        if (!looks.isEmpty())
            looksOf.insert(fid, looks);
    }

    // Reused by value, else named as the show names its own.
    QStringList names;
    for (CapturePalette &cp : plan.palettes)
    {
        if (cp.type == QLCPalette::Dimmer || cp.type == QLCPalette::Color)
            cp.reuse = findPalette(cp.type, cp.values);
        if (cp.reuse == QLCPalette::invalidId())
            for (const QLCPalette *p : m_doc->palettes())
                if (p->type() == cp.type && !m_inlinePalettes.contains(p->id())
                    && p->fanningType() == QLCPalette::Flat && p->goboName().isEmpty()
                    && p->colourMacro().isEmpty() && sameValues(cp.type, p->values(), cp.values))
                {
                    cp.reuse = p->id();
                    break;
                }
        if (cp.reuse != QLCPalette::invalidId())
        {
            cp.name = m_doc->palette(cp.reuse)->name();
            continue;
        }
        if (cp.type == QLCPalette::Dimmer || cp.type == QLCPalette::Color)
        {
            cp.name = suggestPaletteName(cp.type, cp.values);
        }
        else
        {
            const QString what = cp.type == QLCPalette::Gobo ? tr("Gobo") : tr("Position");
            cp.name = QStringLiteral("%1 %2").arg(scene->name(), what);
        }
        QString unique = cp.name;
        for (int n = 2; names.contains(unique); ++n)
            unique = QStringLiteral("%1 %2").arg(cp.name).arg(n);
        cp.name = unique;
        names << unique;
    }

    // Fixtures with the same looks are one layer -- on a group when one
    // holds exactly them, every head.
    QList<QMap<QString, int>> order;
    QHash<int, QList<quint32>> members;
    for (auto it = looksOf.constBegin(); it != looksOf.constEnd(); ++it)
    {
        int at = int(order.indexOf(it.value()));
        if (at < 0)
        {
            order << it.value();
            at = int(order.count()) - 1;
        }
        members[at] << it.key();
    }
    for (int i = 0; i < order.count(); ++i)
    {
        CaptureLayer cl;
        cl.looks = order.at(i);
        QList<quint32> fxs = members.value(i);
        std::sort(fxs.begin(), fxs.end());
        int heads = 0;
        for (quint32 f : fxs)
            if (const Fixture *fx = m_doc->fixture(f))
                heads += fx->heads();
        for (const FixtureGroup *g : m_doc->fixtureGroups())
        {
            QList<quint32> gf = g->fixtureList();
            std::sort(gf.begin(), gf.end());
            gf.erase(std::unique(gf.begin(), gf.end()), gf.end());
            if (gf == fxs && g->headList().count() == heads)
            {
                cl.target.groups << g->id();
                break;
            }
        }
        if (cl.target.groups.isEmpty())
            cl.target.fixtures = fxs;
        plan.layers << cl;
    }

    if (plan.layers.isEmpty())
        return refuse(tr("No palette plays any of its values exactly; it stays raw."));
    if (plan.uses.isEmpty())
        return refuse(tr("The Show Tree doesn't play it as it is."));
    plan.ok = true;
    return plan;
}

bool ShowTreeController::captureIntoPalettes(quint32 sceneId, CapturePlan *planOut)
{
    CapturePlan plan = planCapture(sceneId);
    auto done = [&](bool ok) {
        if (planOut != nullptr)
            *planOut = plan;
        return ok;
    };
    if (!plan.ok)
        return done(false);
    const Scene *scene = qobject_cast<const Scene *>(m_doc->function(sceneId));

    beginBatch(tr("Capture %1 into palettes").arg(scene->name()));

    // The palettes: reused, else made (named and filed; reusable only if
    // the capture holds).
    QList<quint32> pids;
    for (const CapturePalette &cp : plan.palettes)
    {
        quint32 pid = cp.reuse;
        if (pid == QLCPalette::invalidId())
        {
            if (cp.type == QLCPalette::Dimmer || cp.type == QLCPalette::Color)
            {
                pid = createNamedPalette(cp.type, cp.values);
            }
            else
            {
                pid = createInlinePalette(cp.type, cp.values, cp.name);
                if (QLCPalette *p = m_doc->palette(pid))
                    p->setPath(suggestPaletteFolder(cp.type));
                m_batchNamed << pid;
            }
            plan.made << pid;
        }
        pids << pid;
    }

    // What no palette plays: a scene of its own, beside the layers.
    Scene *rest = nullptr;
    if (!plan.leftovers.isEmpty())
    {
        rest = new Scene(m_doc);
        rest->setName(tr("%1 (rest)").arg(scene->name()));
        rest->setPath(scene->path(true));
        for (const SceneValue &sv : plan.leftovers)
            rest->setValue(sv.fxi, sv.channel, sv.value);
        if (!m_doc->addFunction(rest))
        {
            delete rest;
            endBatch(false);
            plan.why = tr("Couldn't make a scene for the values that stay raw.");
            plan.ok = false;
            return done(false);
        }
        plan.rest = rest->id();
    }

    QList<Layer> made;
    for (const CaptureLayer &cl : plan.layers)
    {
        Layer l;
        l.target = cl.target;
        for (auto it = cl.looks.constBegin(); it != cl.looks.constEnd(); ++it)
        {
            Look look;
            look.key = keyForPalette(pids.at(it.value()));
            look.palettes << pids.at(it.value());
            l.looks << look;
        }
        std::sort(l.looks.begin(), l.looks.end(), [](const Look &a, const Look &b) {
            return keyLessThan(a.key, b.key);
        });
        if (plan.layers.count() == 1)
            l.name = scene->name();
        made << l;
    }

    // Each use, kept only where every cue of its list still puts out what
    // it did (a channel only one side sets is 0 on the other, as the
    // import's check reads it).
    const LineAimTest aimTest = lineAimTest();
    typedef QPair<QHash<quint64, uchar>, QStringList> Out;
    auto outputs = [&](const CueList &l) {
        QList<Out> o;
        const QList<ResolvedCue> r = resolve(l, aimTest);
        for (int i = 0; i < l.cues.count(); ++i)
        {
            QStringList e;
            const QHash<quint64, uchar> s = staticCueOutput(l, r, i, &e);
            o << Out(s, e);
        }
        return o;
    };
    auto differ = [](const QList<Out> &a, const QList<Out> &b, int cue) {
        const QHash<quint64, uchar> &x = a.at(cue).first, &y = b.at(cue).first;
        int n = 0;
        QSet<quint64> keys;
        for (auto it = x.constBegin(); it != x.constEnd(); ++it)
            keys.insert(it.key());
        for (auto it = y.constBegin(); it != y.constEnd(); ++it)
            keys.insert(it.key());
        for (quint64 k : keys)
            if (x.value(k) != y.value(k))
                ++n;
        if (n == 0 && a.at(cue).second != b.at(cue).second)
            n = 1;
        return n;
    };

    QList<quint32> lists;
    for (const auto &u : plan.uses)
        if (!lists.contains(u.first))
            lists << u.first;
    for (quint32 listId : lists)
    {
        const CueList *orig = cueList(listId);
        if (orig == nullptr)
            continue;
        CueList cur = *orig;
        const QList<Out> before = outputs(cur);
        bool changedList = false;
        for (const auto &u : plan.uses)
        {
            if (u.first != listId)
                continue;
            CueList cand = cur;
            Cue *c = cand.cue(u.second);
            if (c == nullptr)
                continue;
            for (int k = 0; k < c->layers.count(); ++k)
            {
                if (c->layers.at(k).target.function != sceneId)
                    continue;
                const Layer old = c->layers.takeAt(k);
                QList<Layer> repl;
                for (const Layer &t : made)
                {
                    Layer x = t;
                    x.id = newId();
                    x.release = old.release;
                    repl << x;
                }
                if (rest != nullptr)
                {
                    Layer r;
                    r.id = newId();
                    r.target.function = rest->id();
                    r.release = old.release;
                    repl << r;
                }
                for (int j = 0; j < repl.count(); ++j)
                    c->layers.insert(k + j, repl.at(j));
                k += int(repl.count()) - 1;
            }
            const QList<Out> after = outputs(cand);
            int n = 0;
            QStringList where;
            for (int i = 0; i < before.count() && i < after.count(); ++i)
            {
                const int d = differ(before, after, i);
                if (d > 0)
                    where << cueLabel(cand, i);
                n += d;
            }
            const int at = cand.indexOf(u.second);
            if (n == 0)
            {
                cur = cand;
                changedList = true;
                plan.captured++;
            }
            else
            {
                plan.kept << tr("%1 cue %2: as layers it would change %n channel(s) (in %3), so it plays the "
                                "scene as it is still.", "", n)
                             .arg(orig->name, cueLabel(cand, at), where.join(QStringLiteral(", ")));
            }
        }
        if (changedList)
            edit(QString(), [&](QList<Production> &ps) {
                for (Production &p : ps)
                    if (CueList *l = p.list(listId))
                    {
                        *l = cur;
                        return true;
                    }
                return false;
            });
    }

    const bool any = plan.captured > 0;
    endBatch(any);
    if (!any)
    {
        if (rest != nullptr)
            m_doc->deleteFunction(rest->id());
        plan.rest = invalidId();
        plan.made.clear();
        plan.ok = false;
        plan.why = tr("Captured nowhere: every cue it plays in would change.");
        return done(false);
    }

    // Outside the tree, with this step: the rest scene (made again on redo,
    // with its id) and the palettes made (redo's snapshot brings those
    // back; undo takes them away unless something of the show's uses them).
    const quint32 restId = rest == nullptr ? invalidId() : rest->id();
    const QString restName = rest == nullptr ? QString() : rest->name();
    const QString restPath = rest == nullptr ? QString() : rest->path(true);
    const QList<SceneValue> vals = plan.leftovers;
    const QList<quint32> madePalettes = plan.made;
    Doc *doc = m_doc;
    // The rest scene was made before the tree's change: undone after it (no
    // layer plays it any more), made again before it on redo.
    attachToLastStep([this, doc, restId, restName, restPath, vals, madePalettes](bool undone) {
        if (undone)
        {
            if (restId != invalidId())
                doc->deleteFunction(restId);
            for (quint32 pid : madePalettes)
            {
                bool used = false;
                for (const Function *f : doc->functions())
                    if (const Scene *sc = qobject_cast<const Scene *>(f))
                        if (!isGenerated(sc->id()) && sc->palettes().contains(pid))
                            used = true;
                if (!used)
                    doc->deletePalette(pid);
            }
            return;
        }
        if (restId == invalidId() || doc->function(restId) != nullptr)
            return;
        Scene *s = new Scene(doc);
        s->setName(restName);
        s->setPath(restPath);
        for (const SceneValue &sv : vals)
            s->setValue(sv.fxi, sv.channel, sv.value);
        if (!doc->addFunction(s, restId))
            delete s;
    }, true);
    return done(true);
}
