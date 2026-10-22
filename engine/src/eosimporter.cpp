/*
  Q Light Controller
  eosimporter.cpp

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

#include <QFileInfo>
#include <QColor>
#include <QtMath>

#include <algorithm>
#include <optional>

#include "eosimporter.h"
#include "eosasciiparser.h"
#include "eosfixturemapper.h"
#include "qlcfixturedefcache.h"
#include "qlcmodifierscache.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"
#include "qlcphysical.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "chaserstep.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"
#include "showtreecontroller.h"
#include "showtreemodel.h"
#include "eosmagicsheetparser.h"
#include "monitorproperties.h"

namespace
{

/** How one Eos channel became a fixture. */
struct Placed
{
    quint32 fixture = Fixture::invalidId();
    const EosPersonality *pers = nullptr;   //!< null: a conventional dimmer
    int channels = 1;
};

typedef QMap<int, int> ParamState;          // Eos param -> value

/** A level, resolved through palette / preset references (presets may
 *  refer to palettes; palettes only hold hard levels). */
std::optional<int> resolveLevel(const EosShowData &data, const EosLevel &l, int chan, int param,
                                int depth, int *missing)
{
    switch (l.kind)
    {
        case EosLevel::Value:
            return l.value;
        case EosLevel::PaletteRef:
        case EosLevel::PresetRef:
        {
            const EosPalette *p = data.findPalette(l.refKind, l.ref);
            if (p == nullptr || depth > 2 || !p->levels.value(chan).contains(param))
            {
                if (missing != nullptr)
                    ++*missing;
                return std::nullopt;
            }
            return resolveLevel(data, p->levels.value(chan).value(param), chan, param, depth + 1, missing);
        }
        default:
            return std::nullopt;
    }
}

/** Apply @a levels to @a state (Release takes a parameter out). */
void applyLevels(const EosShowData &data, const EosLevels &levels, QHash<int, ParamState> &state,
                 int *missing)
{
    for (auto c = levels.constBegin(); c != levels.constEnd(); ++c)
        for (auto p = c.value().constBegin(); p != c.value().constEnd(); ++p)
        {
            if (p.value().kind == EosLevel::Release)
            {
                state[c.key()].remove(p.key());
                continue;
            }
            const std::optional<int> v = resolveLevel(data, p.value(), c.key(), p.key(), 0, missing);
            if (v.has_value())
                state[c.key()].insert(p.key(), *v);
        }
}

/** Write one Eos channel's parameters into @a scene, by the personality's
 *  DMX offsets -- the Eos personality describes the real fixture's DMX
 *  layout, and the mapper only picks a QLC mode of the same footprint. A
 *  personality with no intensity channel (virtual intensity) has its
 *  colour parameters scaled by the intensity, as Eos does. */
void writeChannel(const EosShowData &data, const Placed &pl, const ParamState &params, Scene *scene)
{
    auto set = [&](int offset, int value) {
        if (offset >= 1 && offset <= pl.channels)
            scene->setValue(pl.fixture, quint32(offset - 1), uchar(qBound(0, value, 255)));
    };

    if (pl.pers == nullptr)
    {
        if (params.contains(1))
            set(1, params.value(1));
        return;
    }

    const bool virtualIntensity = pl.pers->channel(1) == nullptr && params.contains(1);
    const int intensity = params.value(1, 255);
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
    {
        const EosPersChannel *pc = pl.pers->channel(it.key());
        if (pc == nullptr)
            continue;
        int v = it.value();
        const bool colour = data.paramTypes.value(it.key()).category == 3;
        if (pc->size == 2)
        {
            if (virtualIntensity && colour)
                v = v * intensity / 255;
            set(pc->offset, v >> 8);
            set(pc->lsbOffset, v & 0xFF);
        }
        else
        {
            v = qMin(v, 255);
            if (virtualIntensity && colour)
                v = v * intensity / 255;
            set(pc->offset, v);
        }
    }
}

/** True when the channel is lit, or has no intensity at all. */
bool lit(const Placed &pl, const ParamState &params)
{
    if (!params.contains(1))
        return pl.pers == nullptr ? false : pl.pers->channel(1) == nullptr;
    return params.value(1) > 0;
}

QString displayNumber(const QString &n)
{
    // "1.00" -> "1", "0.50" -> "0.5"
    QString s = n.trimmed();
    if (s.contains(QLatin1Char('.')))
    {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    return s;
}

QString paletteKindName(QChar kind)
{
    switch (kind.unicode())
    {
        case 'I': return QStringLiteral("IP");
        case 'F': return QStringLiteral("FP");
        case 'C': return QStringLiteral("CP");
        case 'B': return QStringLiteral("BP");
        default:  return QStringLiteral("PR");
    }
}

/** The most common of @a values; @a mixed set when they differ. */
template <typename T>
T mostCommon(const QList<T> &values, bool *mixed)
{
    QHash<T, int> count;
    for (const T &v : values)
        count[v]++;
    T best = values.first();
    for (const T &v : values)
        if (count.value(v) > count.value(best))
            best = v;
    *mixed = count.count() > 1;
    return best;
}

int paramByName(const EosShowData &data, const QString &name)
{
    for (const EosParamType &p : data.paramTypes)
        if (p.name.compare(name, Qt::CaseInsensitive) == 0)
            return p.id;
    return -1;
}

} // namespace

bool EosImporter::build(const EosShowData &data, Doc *doc, const EosFixtureMapper &mapper,
                        EosImportResult &result, const QString &showName)
{
    if (doc == nullptr)
        return false;

    result.warnings << data.warnings;
    const QString show = !showName.isEmpty() ? showName
                         : !data.title.isEmpty() ? data.title : QObject::tr("Eos show");

    /*** Fixtures ***/
    QHash<int, Placed> placed;
    QHash<QString, int> genericByPers;
    int skippedParts = 0;
    for (const EosPatch &p : data.patch)
    {
        if (p.part > 1)
        {
            ++skippedParts;
            continue;
        }
        if (placed.contains(p.channel))
        {
            result.warnings << QObject::tr("Channel %1 is patched twice; the second patch was left out.")
                               .arg(p.channel);
            continue;
        }
        Placed pl;
        if (p.personality != 0)
        {
            auto it = data.personalities.constFind(p.personality);
            if (it == data.personalities.constEnd())
                result.warnings << QObject::tr("Channel %1: personality %2 isn't in the file; patched as a "
                                               "generic dimmer.").arg(p.channel).arg(p.personality);
            else
                pl.pers = &it.value();
        }

        Fixture *fx = new Fixture(doc);
        QString name = p.label.trimmed();
        fx->setName(name.isEmpty() ? QObject::tr("Ch %1").arg(p.channel)
                                   : QObject::tr("Ch %1 %2").arg(p.channel).arg(name));

        const int footprint = pl.pers != nullptr ? qMax(1, pl.pers->footprint) : 1;
        EosFixtureChoice choice;
        if (pl.pers != nullptr)
            choice = mapper.resolve(*pl.pers);
        // A close match is only a suggestion: the wizard asks, and what the
        // user takes comes back as a saved choice.
        if (choice.source == EosFixtureChoice::Suggested)
            choice = EosFixtureChoice();
        QLCFixtureDef *def = nullptr;
        QLCFixtureMode *mode = nullptr;
        if (!choice.isGeneric())
        {
            def = doc->fixtureDefCache()->fixtureDef(choice.manufacturer, choice.model);
            if (def != nullptr)
                for (QLCFixtureMode *m : def->modes())
                    if (m->name() == choice.mode)
                        mode = m;
        }
        if (def == nullptr || mode == nullptr)
        {
            def = fx->genericDimmerDef(footprint);
            mode = fx->genericDimmerMode(def, footprint);
            if (pl.pers != nullptr && !(footprint <= 1 && EosFixtureMapper::words(pl.pers->model)
                                                            .contains(QStringLiteral("dimmer"))))
            {
                result.fixturesUnmapped++;
                genericByPers[pl.pers->manufacturer + QLatin1Char(' ') + pl.pers->model]++;
            }
        }
        fx->setFixtureDefinition(def, mode);
        pl.channels = int(fx->channels());

        if (p.address <= 0)
        {
            fx->setPatched(false);
            result.fixturesUnpatched++;
        }
        else
        {
            fx->setUniverse(quint32((p.address - 1) / 512));
            fx->setAddress(quint32((p.address - 1) % 512));
        }

        bool added = doc->addFixture(fx, quint32(p.channel), fx->isPatched()
                                                             && fx->address() + fx->channels() > 512);
        if (!added && fx->isPatched())
        {
            fx->setPatched(false);
            added = doc->addFixture(fx, quint32(p.channel));
            if (added)
            {
                result.fixturesUnpatched++;
                result.warnings << QObject::tr("Channel %1: address %2 overlaps another channel; staged "
                                               "in the inventory instead.").arg(p.channel).arg(p.address);
            }
        }
        if (!added)
        {
            result.warnings << QObject::tr("Channel %1 could not be added.").arg(p.channel);
            delete fx;
            continue;
        }
        pl.fixture = fx->id();
        placed.insert(p.channel, pl);
        result.fixturesParsed++;
    }
    if (skippedParts > 0)
        result.warnings << QObject::tr("%n extra part(s) of multi-part fixtures left out (each fixture "
                                       "comes in as its first part).", "", skippedParts);
    for (auto it = genericByPers.constBegin(); it != genericByPers.constEnd(); ++it)
        result.warnings << QObject::tr("“%1”: no fixture definition chosen; %n channel(s) patched as a "
                                       "generic dimmer of the same size (the import wizard offers close "
                                       "matches).", "", it.value()).arg(it.key());

    /*** Groups ***/
    for (const EosGroup &g : data.groups)
    {
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(g.label.isEmpty() ? QObject::tr("Group %1").arg(displayNumber(g.number)) : g.label);
        grp->setPath(QStringLiteral("Eos"));
        if (!doc->addFixtureGroup(grp))
        {
            delete grp;
            continue;
        }
        int missing = 0;
        for (int c : g.channels)
        {
            if (placed.contains(c))
                grp->assignFixture(placed.value(c).fixture);
            else
                ++missing;
        }
        if (missing > 0)
            result.warnings << QObject::tr("Group %1: %n channel(s) in it aren't patched.", "", missing)
                               .arg(grp->name());
        result.fixtureGroupsParsed++;
    }

    /*** Palettes: IP -> Dimmer, CP -> Color, FP -> PanTilt ***/
    const int pRed = paramByName(data, QStringLiteral("Red")),
              pGreen = paramByName(data, QStringLiteral("Green")),
              pBlue = paramByName(data, QStringLiteral("Blue")),
              pCyan = paramByName(data, QStringLiteral("Cyan")),
              pMagenta = paramByName(data, QStringLiteral("Magenta")),
              pYellow = paramByName(data, QStringLiteral("Yellow")),
              pWhite = paramByName(data, QStringLiteral("White")),
              pAmber = paramByName(data, QStringLiteral("Amber")),
              pUV = paramByName(data, QStringLiteral("UV")),
              pPan = paramByName(data, QStringLiteral("Pan")),
              pTilt = paramByName(data, QStringLiteral("Tilt"));
    QStringList bpSkipped, mixedPalettes;
    for (const EosPalette &pal : data.palettes)
    {
        const QString title = QStringLiteral("%1 %2").arg(paletteKindName(pal.kind), displayNumber(pal.number));
        const QString name = pal.label.isEmpty() ? title : pal.label;
        QLCPalette *qp = nullptr;
        bool mixed = false;

        if (pal.kind == QLatin1Char('I'))
        {
            QList<int> v;
            for (auto c = pal.levels.constBegin(); c != pal.levels.constEnd(); ++c)
                if (c.value().contains(1) && c.value().value(1).kind == EosLevel::Value)
                    v << c.value().value(1).value;
            if (!v.isEmpty())
            {
                qp = new QLCPalette(QLCPalette::Dimmer);
                qp->setValue(qBound(0, mostCommon(v, &mixed), 255));
                qp->setPath(QStringLiteral("Palettes/Dimmer"));
            }
        }
        else if (pal.kind == QLatin1Char('C'))
        {
            QList<QString> v;
            for (auto c = pal.levels.constBegin(); c != pal.levels.constEnd(); ++c)
            {
                const QMap<int, EosLevel> &lv = c.value();
                auto val = [&lv](int param, int def) {
                    return param >= 0 && lv.contains(param) && lv.value(param).kind == EosLevel::Value
                           ? qMin(lv.value(param).value, 255) : def;
                };
                const bool rgb = (pRed >= 0 && lv.contains(pRed)) || (pGreen >= 0 && lv.contains(pGreen))
                                 || (pBlue >= 0 && lv.contains(pBlue));
                const bool cmy = (pCyan >= 0 && lv.contains(pCyan)) || (pMagenta >= 0 && lv.contains(pMagenta))
                                 || (pYellow >= 0 && lv.contains(pYellow));
                if (!rgb && !cmy)
                    continue;
                const QColor c3 = rgb ? QColor(val(pRed, 0), val(pGreen, 0), val(pBlue, 0))
                                      : QColor(255 - val(pCyan, 0), 255 - val(pMagenta, 0), 255 - val(pYellow, 0));
                const QColor wauv(val(pWhite, 0), val(pAmber, 0), val(pUV, 0));
                v << QLCPalette::colorToString(c3, wauv);
            }
            if (!v.isEmpty())
            {
                QString colour = mostCommon(v, &mixed);
                if (colour.endsWith(QLatin1String("000000")) && colour.length() == 13)
                    colour = colour.left(7);
                qp = new QLCPalette(QLCPalette::Color);
                qp->setValue(colour);
                qp->setPath(QStringLiteral("Palettes/Color"));
            }
        }
        else if (pal.kind == QLatin1Char('F'))
        {
            QList<QPair<int, int>> v;
            for (auto c = pal.levels.constBegin(); c != pal.levels.constEnd(); ++c)
            {
                if (!placed.contains(c.key()) || placed.value(c.key()).pers == nullptr)
                    continue;
                const Placed &pl = placed.value(c.key());
                const Fixture *fx = doc->fixture(pl.fixture);
                const QLCPhysical phys = fx->fixtureMode()->physical();
                const EosPersChannel *pc = pl.pers->channel(pPan), *tc = pl.pers->channel(pTilt);
                if (pc == nullptr || tc == nullptr || !c.value().contains(pPan) || !c.value().contains(pTilt))
                    continue;
                const double pmax = pc->size == 2 ? 65535.0 : 255.0, tmax = tc->size == 2 ? 65535.0 : 255.0;
                const int pan = qRound(c.value().value(pPan).value / pmax * phys.focusPanMax());
                const int tilt = qRound(c.value().value(pTilt).value / tmax * phys.focusTiltMax());
                v << qMakePair(pan, tilt);
            }
            if (!v.isEmpty())
            {
                const QPair<int, int> pt = mostCommon(v, &mixed);
                qp = new QLCPalette(QLCPalette::PanTilt);
                qp->setValue(pt.first, pt.second);
                qp->setPath(QStringLiteral("Palettes/Position"));
            }
        }
        else
        {
            bpSkipped << (pal.label.isEmpty() ? title : QStringLiteral("%1 “%2”").arg(title, pal.label));
            continue;
        }

        if (qp == nullptr)
        {
            result.warnings << QObject::tr("%1 “%2”: nothing in it that a palette here can hold; left out.")
                               .arg(title, name);
            continue;
        }
        qp->setName(name);
        if (!doc->addPalette(qp))
        {
            delete qp;
            continue;
        }
        if (mixed)
            mixedPalettes << (pal.label.isEmpty() ? title : QStringLiteral("%1 “%2”").arg(title, pal.label));
        result.palettesParsed++;
    }
    if (!mixedPalettes.isEmpty())
        result.warnings << QObject::tr("%n palette(s) hold different values on different channels; each plays "
                                       "the most common one (the cues keep every channel's own): %1", "",
                                       int(mixedPalettes.count())).arg(mixedPalettes.join(QStringLiteral(", ")));
    if (!bpSkipped.isEmpty())
        result.warnings << QObject::tr("%n beam palette(s) not imported (the cues keep their values): %1",
                                       "", int(bpSkipped.count())).arg(bpSkipped.join(QStringLiteral(", ")));

    int missingRefs = 0;
    auto sceneFromState = [&](const QHash<int, ParamState> &state, const QString &name, const QString &path,
                              bool litOnly) {
        Scene *s = new Scene(doc);
        s->setName(name);
        s->setPath(path);
        QList<int> chans = state.keys();
        std::sort(chans.begin(), chans.end());
        for (int c : chans)
        {
            if (!placed.contains(c))
                continue;
            const Placed &pl = placed.value(c);
            if (litOnly && !lit(pl, state.value(c)))
                continue;
            writeChannel(data, pl, state.value(c), s);
        }
        return s;
    };

    /*** Presets and subs: a Scene each ***/
    for (const EosPalette &pr : data.presets)
    {
        QHash<int, ParamState> state;
        applyLevels(data, pr.levels, state, &missingRefs);
        const QString n = QObject::tr("Preset %1").arg(displayNumber(pr.number));
        Scene *s = sceneFromState(state, pr.label.isEmpty() ? n : QStringLiteral("%1 %2").arg(n, pr.label),
                                  QStringLiteral("Eos/Presets"), false);
        if (doc->addFunction(s))
            result.presetsParsed++;
        else
            delete s;
    }
    for (const EosSub &sb : data.subs)
    {
        QHash<int, ParamState> state;
        applyLevels(data, sb.levels, state, &missingRefs);
        const QString n = QObject::tr("Sub %1").arg(displayNumber(sb.number));
        Scene *s = sceneFromState(state, sb.label.isEmpty() ? n : QStringLiteral("%1 %2").arg(n, sb.label),
                                  QStringLiteral("Eos/Subs"), false);
        if (sb.up > 0)
            s->setFadeInSpeed(uint(sb.up * 1000));
        if (sb.down > 0)
            s->setFadeOutSpeed(uint(sb.down * 1000));
        if (doc->addFunction(s))
            result.submastersParsed++;
        else
            delete s;
    }

    /*** Cue lists: a Chaser of one Scene per cue, its full tracked state ***/
    int delays = 0, links = 0, earlyFollows = 0;
    QStringList linkLines;
    for (const EosCueList &cl : data.cueLists)
    {
        if (cl.cues.isEmpty())
            continue;
        QString listName = data.cueLists.count() == 1 ? show
                           : QObject::tr("%1 — list %2").arg(show).arg(cl.number);
        if (!cl.label.isEmpty() && data.cueLists.count() > 1)
            listName += QStringLiteral(" ") + cl.label;
        const QString folder = QStringLiteral("Eos/%1").arg(cl.label.isEmpty()
                                                            ? QObject::tr("List %1").arg(cl.number) : cl.label);

        Chaser *chaser = new Chaser(doc);
        chaser->setName(listName);
        chaser->setPath(QStringLiteral("Eos"));
        chaser->setFadeInMode(Chaser::PerStep);
        chaser->setFadeOutMode(Chaser::PerStep);
        chaser->setDurationMode(Chaser::PerStep);
        chaser->setRunOrder(Function::SingleShot);
        if (!doc->addFunction(chaser))
        {
            delete chaser;
            continue;
        }

        QHash<int, ParamState> state;
        for (int i = 0; i < cl.cues.count(); ++i)
        {
            const EosCue &cue = cl.cues.at(i);
            applyLevels(data, cue.moves, state, &missingRefs);
            if (cue.hasTracked)
            {
                // Chan is the whole intensity picture: what it leaves out is out.
                for (auto it = placed.constBegin(); it != placed.constEnd(); ++it)
                    if (cue.tracked.contains(it.key()))
                        state[it.key()].insert(1, cue.tracked.value(it.key()));
                    else if (state.contains(it.key()) && state.value(it.key()).contains(1))
                        state[it.key()].insert(1, 0);
            }
            const QString number = displayNumber(cue.number);
            Scene *s = sceneFromState(state, cue.label.isEmpty() ? number
                                                                 : QStringLiteral("%1-%2").arg(number, cue.label),
                                      folder, true);
            if (!doc->addFunction(s))
            {
                delete s;
                continue;
            }

            ChaserStep step(s->id());
            step.cueNumber = number;
            step.fadeIn = cue.up > 0 ? uint(cue.up * 1000) : 0;
            step.fadeOut = cue.down >= 0 ? uint(cue.down * 1000) : step.fadeIn;
            step.hold = Function::infiniteSpeed();
            if (cue.follow >= 0)
            {
                const uint follow = uint(cue.follow * 1000);
                if (follow < step.fadeIn)
                    ++earlyFollows;
                step.hold = follow > step.fadeIn ? follow - step.fadeIn : 0;
            }
            else if (cue.hang >= 0)
            {
                step.hold = uint(cue.hang * 1000);
            }
            step.duration = step.hold == Function::infiniteSpeed() ? Function::infiniteSpeed()
                                                                     : step.fadeIn + step.hold;
            chaser->addStep(step);
            if (cue.upDelay > 0 || cue.downDelay > 0)
                ++delays;
            if (!cue.link.isEmpty())
            {
                ++links;
                linkLines << QObject::tr("cue %1 → %2").arg(number, cue.link);
            }
            result.cuesParsed++;
        }
        result.chasers << chaser->id();
        result.cueListsParsed++;
    }
    if (delays > 0)
        result.warnings << QObject::tr("%n cue(s) have a delay; delays are not carried (the fade starts "
                                       "at GO).", "", delays);
    if (links > 0)
        result.warnings << QObject::tr("%n cue(s) link elsewhere; links are not carried, the list plays in "
                                       "cue order: %1", "", links).arg(linkLines.mid(0, 6).join(QStringLiteral(", ")));
    if (earlyFollows > 0)
        result.warnings << QObject::tr("%n cue(s) follow on before their own fade ends; the next cue now "
                                       "starts when the fade is done.", "", earlyFollows);
    if (missingRefs > 0)
        result.warnings << QObject::tr("%n level(s) refer to a palette or preset entry that isn't in the "
                                       "file; they were left out.", "", missingRefs);

    if (!data.skipped.isEmpty())
    {
        QStringList parts;
        for (auto it = data.skipped.constBegin(); it != data.skipped.constEnd(); ++it)
            parts << QStringLiteral("%1 × %2").arg(it.key()).arg(it.value());
        result.warnings << QObject::tr("Not imported (no counterpart yet): %1").arg(parts.join(QStringLiteral(", ")));
    }
    return true;
}

Doc *EosImporter::load(const EosImportBundle &bundle, EosImportResult &result, EosFixtureMapper *mapper,
                       Doc *into)
{
    EosShowData data;
    EosAsciiParser parser;
    if (!parser.parse(bundle.asciiPath, data))
    {
        result.warnings << data.warnings;
        return nullptr;
    }

    Doc *doc = into;
    if (doc == nullptr)
    {
        doc = new Doc(nullptr);
        doc->fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        doc->fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());
        doc->modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
        doc->modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());
    }

    EosFixtureMapper own(doc->fixtureDefCache());
    if (mapper == nullptr)
    {
        own.loadMap(bundle.fixtureMapPath.isEmpty() ? EosFixtureMapper::defaultMapPath()
                                                    : bundle.fixtureMapPath);
        mapper = &own;
    }

    build(data, doc, *mapper, result, data.title.isEmpty() ? QFileInfo(bundle.asciiPath).completeBaseName()
                                                           : data.title);

    if (!bundle.augment3dEsfPath.isEmpty())
        result.warnings << QObject::tr("Augment3d positions (%1) are not read yet.")
                           .arg(QFileInfo(bundle.augment3dEsfPath).fileName());

    // Magic sheets: channel objects become 2D-map positions, a layer per
    // sheet ("Imported — <sheet>"), the sheet's own coordinates at 10 mm a
    // unit (no calibration yet: move / scale the layer to fit the plot). A
    // channel on two sheets keeps the first sheet's place.
    for (const QString &path : bundle.magicSheetPaths)
    {
        EosMagicSheetParser parser;
        const EosMagicSheet sheet = parser.parse(path, result.warnings);
        if (sheet.name.isEmpty())
            continue;
        result.magicSheetsParsed++;
        placeMagicSheet(sheet, doc, result);
    }
    return doc;
}

QList<quint32> EosImporter::toShowTree(Doc *doc, const QList<quint32> &chasers, QStringList *lines)
{
    QList<quint32> lists;
    ShowTreeController *st = doc == nullptr ? nullptr : doc->showTree();
    if (st == nullptr)
        return lists;

    const quint64 since = st->undoStamp();
    for (quint32 chaserId : chasers)
    {
        const Chaser *chaser = qobject_cast<const Chaser *>(doc->function(chaserId));
        if (chaser == nullptr)
            continue;
        const QString name = chaser->name();
        QList<quint32> scenes;
        for (const ChaserStep &step : chaser->steps())
            if (!scenes.contains(step.fid))
                scenes << step.fid;

        const ShowTreeController::ImportReport rep = st->importInPlace(chaserId);
        if (rep.list == ShowTree::invalidId())
        {
            if (lines != nullptr)
                *lines << QObject::tr("%1: not brought into the Show Tree (%2)")
                          .arg(name, rep.notes.join(QStringLiteral("; ")));
            continue;
        }
        // Each cue's raw state read as looks on its fixtures: palettes
        // reused by value (the show's IP / CP among them), else made.
        int captured = 0;
        for (quint32 sid : scenes)
        {
            ShowTreeController::CapturePlan done;
            if (st->captureIntoPalettes(sid, &done))
                ++captured;
        }
        // Eos tracks; so does the tree: what only repeats the cue before
        // is taken out, each cue checked to put out what it did.
        ShowTreeController::ConvertPlan plan;
        const bool tracked = st->convertToTracking(rep.list, &plan);
        lists << rep.list;
        if (lines != nullptr)
        {
            *lines << QObject::tr("%1: a cue list of %n cue(s) in the Show Tree.", "", rep.cues).arg(name);
            if (captured > 0)
                *lines << QObject::tr("%1: %n cue(s) read into looks and palettes.", "", captured).arg(name);
            if (tracked)
                *lines << QObject::tr("%1: tracking -- %n cue(s) track, %2 kept as blocks.", "", plan.tracking)
                          .arg(name).arg(plan.blocks);
            if (rep.differChannels > 0)
                *lines << QObject::tr("%1: %n channel(s) play differently from the Eos cues.", "",
                                      rep.differChannels).arg(name);
        }
    }
    st->groupStepsSince(since, QObject::tr("Import Eos show"));
    return lists;
}

int EosImporter::placeMagicSheet(const EosMagicSheet &sheet, Doc *doc, EosImportResult &result)
{
    MonitorProperties *mon = doc == nullptr ? nullptr : doc->monitorProperties();
    if (mon == nullptr)
        return 0;
    const QList<EosMagicSheetObject> chans = sheet.channels();
    quint32 layer = MonitorProperties::defaultLayerId;
    int placed = 0, missing = 0;
    QSet<int> seen;
    for (const EosMagicSheetObject &o : chans)
    {
        bool ok = false;
        const int channel = o.targetId.toInt(&ok);
        if (!ok || seen.contains(channel))
            continue;
        seen.insert(channel);
        if (doc->fixture(quint32(channel)) == nullptr)
        {
            ++missing;
            continue;
        }
        if (mon->containsFixture(quint32(channel)))
            continue;
        if (layer == MonitorProperties::defaultLayerId)
            layer = mon->addLayer(QObject::tr("Imported — %1").arg(sheet.name));
        FixturePreviewItem item;
        item.m_baseItem.m_position = QVector3D(float(o.position.x() * 10.0), float(o.position.y() * 10.0), 0);
        item.m_baseItem.m_rotation = QVector3D();
        item.m_baseItem.m_scale = QVector3D(1, 1, 1);
        item.m_baseItem.m_zoom = 0;
        item.m_baseItem.m_flags = 0;
        item.m_baseItem.m_layerId = layer;
        mon->setFixtureProperties(quint32(channel), item);
        ++placed;
    }
    result.channelObjectsPlaced += placed;

    QStringList dropped;
    const int controls = int(sheet.objects.count() - chans.count());
    if (controls > 0)
        dropped << QObject::tr("%n control object(s) (groups, palettes, cues…)", "", controls);
    if (sheet.imagesDropped > 0)
        dropped << QObject::tr("%n image(s), shape(s) and label(s)", "", sheet.imagesDropped);
    if (sheet.unknownKindsDropped > 0)
        dropped << QObject::tr("%n object(s) of a kind not known here", "", sheet.unknownKindsDropped);
    result.warnings << QObject::tr("Magic sheet “%1”: %n channel(s) placed on the 2D map (layer “Imported — %1”, "
                                   "uncalibrated: move and scale it to the plot)", "", placed).arg(sheet.name)
                       + (dropped.isEmpty() ? QString() : QObject::tr("; not carried: %1").arg(dropped.join(QStringLiteral(", "))))
                       + (missing > 0 ? QObject::tr("; %n channel(s) on it aren't patched", "", missing) : QString())
                       + QLatin1Char('.');
    return placed;
}
