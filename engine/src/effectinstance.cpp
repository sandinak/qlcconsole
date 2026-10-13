/*
  Q Light Controller Plus
  effectinstance.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectinstance.h"
#include "speedmasters.h"
#include "spreadlayer.h"
#include "wledfixture.h"
#include "aimsolver.h"

#include "doc.h"
#include "scene.h"
#include "function.h"
#include "chaser.h"
#include "chaserstep.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "monitorproperties.h"
#include "righeads.h"
#include "stagetarget.h"
#include "scenevalue.h"
#include "universe.h"

#include "mastertimer.h"

#include <QJSValueIterator>
#include <QVarLengthArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QHash>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QtMath>
#include <cmath>
#include <algorithm>

EffectInstance::EffectInstance(Doc *doc, quint32 sceneId, quint32 effectPaletteId)
    : m_doc(doc)
    , m_sceneId(sceneId)
    , m_effectPaletteId(effectPaletteId)
{
    QLCPalette *pal = m_doc->palette(effectPaletteId);
    if (!pal)
    {
        qWarning() << "[EffectInstance] palette" << effectPaletteId << "not found";
        return;
    }

    m_wledEffect = pal->wledEffect();

    // Load the script
    if (!m_script.load(pal->scriptPath()))
    {
        qWarning() << "[EffectInstance] failed to load script" << pal->scriptPath();
        return;
    }

    // Seed input default values from script meta
    for (const EffectScript::InputDef &d : m_script.inputDefs())
        m_inputValues[d.name] = d.defaultValue;

    // Override with palette-stored param/input values
    const QMap<QString, double> &paletteParams = pal->effectParamValues();
    for (auto it = paletteParams.constBegin(); it != paletteParams.constEnd(); ++it)
        m_paramValues[it.key()] = it.value();

    // Param renames: if a stored value lives under a former name (alias) and the
    // current name has none, carry it over. Keeps Effects/saved looks working
    // when a Generator renames a param.
    for (const EffectScript::ParamDef &d : m_script.paramDefs())
    {
        if (m_paramValues.contains(d.name))
            continue;
        for (const QString &alias : d.aliases)
        {
            if (m_paramValues.contains(alias))
            {
                m_paramValues[d.name] = m_paramValues.value(alias);
                break;
            }
        }
    }

    // Seed param defaults from script meta for anything not in palette
    for (const EffectScript::ParamDef &d : m_script.paramDefs())
    {
        if (d.type == QLatin1String("path"))
            continue;  // path params seeded from string params below
        if (d.type == QLatin1String("text"))
        {
            // Words, not a number: the script's own until the look says else
            m_stringParamValues[d.name] = d.defaultText;
            continue;
        }
        if (!m_paramValues.contains(d.name))
            m_paramValues[d.name] = d.defaultValue;
    }

    // Seed string/path params from palette
    const QMap<QString, QString> &strParams = pal->effectStringParams();
    for (auto it = strParams.constBegin(); it != strParams.constEnd(); ++it)
        m_stringParamValues[it.key()] = it.value();

    m_state = m_script.newState();
    m_elapsed.start();
}

void EffectInstance::setInputValue(const QString &slotName, float norm01)
{
    m_inputValues[slotName] = qBound(0.0f, norm01, 1.0f);
}

float EffectInstance::inputValue(const QString &slotName) const
{
    return m_inputValues.value(slotName, 0.5f);
}

void EffectInstance::setParamValue(const QString &name, double value)
{
    m_paramValues[name] = value;
}

void EffectInstance::setStringParamValue(const QString &name, const QString &value)
{
    m_stringParamValues[name] = value;
}

void EffectInstance::reloadParamsFromPalette()
{
    // Live param edit (main thread): overlay the palette's current param values
    // onto the running instance WITHOUT resetting m_state, so a param change
    // (axis, note range from Learn, a slider) takes effect on the very next tick
    // while the effect keeps its phase/held-note levels. Without this the tick
    // reads m_paramValues, which was frozen at construction — the change was
    // written to the palette but never reached the live effect.
    QLCPalette *pal = m_doc->palette(m_effectPaletteId);
    if (pal == nullptr)
        return;
    const QMap<QString, double> &pp = pal->effectParamValues();
    for (auto it = pp.constBegin(); it != pp.constEnd(); ++it)
        m_paramValues[it.key()] = it.value();
    // Anything the palette doesn't pin keeps its current value (already seeded to
    // the script default at construction).
    const QMap<QString, QString> &sp = pal->effectStringParams();
    for (auto it = sp.constBegin(); it != sp.constEnd(); ++it)
        m_stringParamValues[it.key()] = it.value();
}

void EffectInstance::setDataChannels(const QHash<QString, QVariantMap> &channels)
{
    m_dataChannels = channels;
}

void EffectInstance::runTick()
{
    // Every bail-out below must CLEAR m_lastResults, not just return: writeDMX()
    // re-applies dmxWrites() into an Override-priority fader on every tick, so a
    // stale non-empty result set would pin this effect's final frame onto the
    // output forever, with no way for the scene to reclaim those channels.
    if (!m_script.isValid())
    {
        clearResults();
        return;
    }

    // A finished one-shot is frozen: "hold" keeps the last frame already in
    // m_lastResults, "release" left it cleared. Either way, skip the whole tick.
    if (m_oneShotDone)
        return;

    // Collect the fixture ids the scene targets, filtered by the script's
    // declared fixtureTypes (e.g. ["rgb"] excludes movers, ["moving"] excludes
    // wash fixtures).  If the script declares no types, all scene fixtures are used.
    // Actuation envelope: scale intensity/colour output by the host scene's
    // fade-in ramp so the effect fires *with* the scene (ramps up as the scene
    // fades in, then holds) instead of blasting at full independent of it.
    // While releasing (the look stopped with a Fade Out time) the scene is no
    // longer running, so drive the envelope down linearly instead.
    if (m_fadingOut)
    {
        const qint64 el = m_elapsed.elapsed() - m_fadeOutStartMs;
        m_envelope = qBound(0.0f, 1.0f - float(el) / float(m_fadeOutMs), 1.0f);
    }
    else
        m_envelope = sceneEnvelope();

    /* The scene's transitions: each cell's own envelope (envelopeFor()). The
       places are worked out once, the first time they are needed. */
    m_transitionIn = SceneTransition();
    m_transitionOut = SceneTransition();
    m_sceneRunning = false;
    if (Scene *sc = qobject_cast<Scene *>(m_doc->function(m_sceneId)))
    {
        m_transitionIn = sc->transitionIn();
        m_transitionOut = sc->transitionOut();
        m_sceneRunning = sc->isRunning();
        m_sceneElapsed = sc->elapsed();
        if (m_transitionIn.isActive() && !m_transitionMapInBuilt)
        {
            m_transitionMapIn = TransitionMap(m_doc, sc->transitionFixtures(), m_transitionIn.shape);
            m_transitionMapInBuilt = true;
        }
        if (m_fadingOut && m_transitionOut.isActive() && !m_transitionMapOutBuilt)
        {
            m_transitionMapOut = TransitionMap(m_doc, sc->transitionFixtures(), m_transitionOut.shape);
            m_transitionMapOutBuilt = true;
        }
    }

    // WLED fixtures in a WLED look run the effect themselves: theirs is a
    // handful of settings, not a frame.
    const QList<DmxWrite> device = wledDeviceWrites();

    int gridCols = 1, gridRows = 1;
    QList<EffectCell> cells = effectiveCells(gridCols, gridRows);
    if (cells.isEmpty())
    {
        if (device.isEmpty())
            clearResults();
        else
        {
            QMutexLocker locker(&m_mutex);
            m_lastResults = device;
            m_lastFrameEmpty = false;
        }
        return;
    }

    // Snapshot the scene's base per-channel values so an intensity flicker can
    // modulate *around* the Dimmer/Colour looks instead of overriding them.
    buildSceneBaseValues();

    QJSValue fixtures = buildFixturesArray(cells, gridCols, gridRows);
    QJSValue inputs   = buildInputsObject();
    QJSValue palettes = buildPalettesObject();
    QJSValue params   = buildParamsObject();
    QJSValue data     = buildDataChannelsObject();

    QJSValue result = m_script.callTick(fixtures, inputs, palettes, params, m_state, data);
    if (!result.isArray())
    {
        // The script threw, or returned something that isn't an intent list.
        clearResults();
        return;
    }

    QList<DmxWrite> writes = parseIntents(result, cells);

    // One-shot lifecycle: finished when phase reaches 1 or the script sets
    // state.done (it can finish early). On finish, "release" stops driving now
    // (fixtures fall back to the base); "hold" keeps this final frame. Either way
    // the frozen flag makes the next tick a no-op (see the top of runTick).
    if (m_script.isOneShot() && !m_oneShotDone)
    {
        const int dur = resolvedDurationMs();
        const double phase = dur > 0 ? qMin(1.0, double(m_elapsed.elapsed()) / dur) : 1.0;
        if (phase >= 1.0 || m_state.property("done").toBool())
        {
            m_oneShotDone = true;
            if (m_script.onFinish() == QLatin1String("release"))
                writes.clear();
        }
    }

    // --- Diagnostic: set QLC_EFFECT_DEBUG=<path> to trace note→column mapping ---
    // Logs only when the held-note set changes (not at 50 Hz). Shows the grid, the
    // held notes, and which grid COLUMNS the script actually lit — the ground
    // truth for "column 0 stays dark".
    static const QByteArray dbgPath = qgetenv("QLC_EFFECT_DEBUG");
    if (!dbgPath.isEmpty())
    {
        const QVariantMap midi = m_dataChannels.value(QStringLiteral("midi"));
        const QVariantList held = midi.value(QStringLiteral("held")).toList();
        QList<int> heldNotes;
        for (int i = 0; i < held.size(); ++i)
            if (held.at(i).toBool()) heldNotes << i;

        static QHash<const EffectInstance*, QString> lastKey;
        QString key;
        for (int n : heldNotes) key += QString::number(n) + ',';
        if (lastKey.value(this) != key)
        {
            lastKey[this] = key;
            // Which columns are lit in the raw script result?
            QSet<int> litCols;
            const int len = qMin(result.property("length").toInt(), cells.size());
            for (int i = 0; i < len; ++i)
            {
                QJSValue o = result.property(i);
                const int r = o.property("r").toInt(), g = o.property("g").toInt(),
                          b = o.property("b").toInt();
                const double dim = o.property("dimmer").toNumber();
                if (r > 0 || g > 0 || b > 0 || dim > 0.001)
                    litCols << cells.at(i).col;
            }
            QList<int> sortedCols(litCols.constBegin(), litCols.constEnd());
            std::sort(sortedCols.begin(), sortedCols.end());
            QString colStr;
            for (int c : sortedCols) colStr += QString::number(c) + ' ';

            // For two reference columns (0 = the "dark" edge, 64 = the "works"
            // edge), report which fixture/DMX the cell maps to and what the effect
            // actually WROTE there — the ground truth for "col 0 emits nothing".
            auto describeCol = [&](int wantCol) -> QString {
                for (int i = 0; i < cells.size(); ++i)
                {
                    if (cells.at(i).col != wantCol) continue;
                    const EffectCell &c = cells.at(i);
                    Fixture *fxi = m_doc->fixture(c.fixtureId);
                    if (!fxi) return QString("col%1=<no fixture %2>").arg(wantCol).arg(c.fixtureId);
                    QList<quint32> hc = fxi->head(c.head).channels();
                    int wr = 0; QString vals;
                    const int base = int(fxi->address());
                    for (const DmxWrite &w : writes)
                        if (w.fixtureId == fxi->id() &&
                            hc.contains(quint32(w.channel)))
                        { wr++; if (vals.size()<40) vals += QString("%1=%2 ").arg(w.channel).arg(w.value); }
                    // Scene BASE (non-effect Colour/Dimmer looks) for this head's
                    // first 3 channels — if this equals the strike colour, HTP
                    // merge hides the strike (same-colour-on-same-colour).
                    QString baseRGB;
                    for (int k = 0; k < qMin(3, hc.size()); ++k)
                        baseRGB += QString::number(sceneBaseValue(fxi->id(), hc.at(k), 0)) + " ";
                    return QString("col%1: fix%2 '%3' uni%4 addr%5 head%6 ch[%7..] writes=%8 {%9} base{%10}")
                        .arg(wantCol).arg(fxi->id()).arg(fxi->name().left(10))
                        .arg(fxi->universe()).arg(base).arg(c.head)
                        .arg(hc.isEmpty()?-1:int(hc.first())).arg(wr).arg(vals.trimmed())
                        .arg(baseRGB.trimmed());
                }
                return QString("col%1=<no cell>").arg(wantCol);
            };

            QFile f(QString::fromLocal8Bit(dbgPath));
            if (f.open(QIODevice::Append | QIODevice::Text))
            {
                QTextStream ts(&f);
                ts << "grid=" << gridCols << "x" << gridRows
                   << " cells=" << cells.size()
                   << " lo=" << int(m_paramValues.value("noteLow", -1))
                   << " hi=" << int(m_paramValues.value("noteHigh", -1))
                   << " axis=" << int(m_paramValues.value("axis", -1))
                   << " src=" << int(m_paramValues.value("midiSource", -1))
                   << " totalWrites=" << writes.size()
                   << " | held=[";
                for (int n : heldNotes) ts << n << ' ';
                ts << "] litCols=[" << colStr << "]\n";
                if (!heldNotes.isEmpty())
                    ts << "    " << describeCol(0) << "\n    " << describeCol(64) << "\n";
                f.close();
            }
        }
    }

    // Frame is "empty" (drives nothing) if every write is 0 — a note effect with
    // no held notes and no live ripples. The runner uses this to skip the next
    // tick until fresh input arrives (wait-for-input instead of polling at 50 Hz).
    writes += device;
    bool empty = true;
    for (const DmxWrite &w : writes)
        if (w.value != 0) { empty = false; break; }

    QMutexLocker locker(&m_mutex);
    m_lastResults = writes;
    m_lastFrameEmpty = empty;
}

void EffectInstance::clearResults()
{
    QMutexLocker locker(&m_mutex);
    m_lastResults.clear();
    m_lastFrameEmpty = true;
}

void EffectInstance::collectGarbage()
{
    if (m_script.isValid() && m_script.engine() != nullptr)
        m_script.engine()->collectGarbage();
}

QList<EffectInstance::DmxWrite> EffectInstance::dmxWrites() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastResults;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void EffectInstance::buildSceneBaseValues()
{
    m_sceneBaseValues.clear();
    m_lookDimmer = 1.0f;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (scene == nullptr)
        return;

    auto key = [](quint32 fid, quint32 ch) -> quint64 {
        return (quint64(fid) << 32) | ch;
    };

    // Baked scene values first.
    for (const SceneValue &sv : scene->values())
        m_sceneBaseValues[key(sv.fxi, sv.channel)] = sv.value;

    // Non-effect looks (Colour/Dimmer/…) override baked values (palette-paramount,
    // matching Scene::write). Effect palettes are skipped — that's us.
    const QList<quint32> groups = scene->fixtureGroups();
    const QList<quint32> fixtures = scene->fixtures();
    const QList<quint32> orderedPalettes = scene->palettes();
    for (quint32 pid : orderedPalettes)
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == nullptr || p->type() == QLCPalette::Effect)
            continue;
        // Colours placed AFTER an effect are the effect's own palette, not a
        // static base — the scene doesn't paint them, so they must not count as
        // this effect's base either (else the base would still wash it out).
        if (QLCPalette::isEffectScoped(m_doc, orderedPalettes, pid))
            continue;
        // Capture the look's master Dimmer (0..1) for host-side colour scaling.
        if (p->type() == QLCPalette::Dimmer)
            m_lookDimmer = qBound(0.0f, float(p->value().toDouble() / 255.0), 1.0f);
        const int cOff = QLCPalette::colorSetOffset(m_doc, orderedPalettes, pid);
        for (const SceneValue &sv : p->valuesFromFixtureGroups(m_doc, groups, scene, cOff))
            m_sceneBaseValues[key(sv.fxi, sv.channel)] = sv.value;
        for (const SceneValue &sv : p->valuesFromFixtures(m_doc, fixtures, scene, cOff))
            m_sceneBaseValues[key(sv.fxi, sv.channel)] = sv.value;
    }
}

uchar EffectInstance::sceneBaseValue(quint32 fid, quint32 channel, uchar dflt) const
{
    return m_sceneBaseValues.value((quint64(fid) << 32) | channel, dflt);
}

float EffectInstance::sceneEnvelope() const
{
    Function *f = m_doc->function(m_sceneId);
    if (f == nullptr || !f->isRunning())
        return 0.0f;

    const uint fadeIn = f->fadeInSpeed();
    // No fade-in, or a sentinel speed (default = UINT_MAX, infinite = UINT_MAX-1)
    // → full level immediately. Only a real finite fade-in ramps the envelope.
    if (fadeIn == 0 ||
        fadeIn == Function::defaultSpeed() ||
        fadeIn == Function::infiniteSpeed())
        return 1.0f;

    const quint32 el = f->elapsed();
    if (el >= fadeIn)
        return 1.0f;
    return float(el) / float(fadeIn);
}

int EffectInstance::resolvedDurationMs() const
{
    // 1. Look override — a positive per-effect duration on the palette wins
    //    (L3 UI sets the reserved "__durationMs" value; persists with the look).
    if (QLCPalette *pal = m_doc->palette(m_effectPaletteId))
    {
        const int ov = int(pal->effectParamValues().value(QStringLiteral("__durationMs"), 0.0));
        if (ov > 0)
            return ov;
    }

    // 2. Cue / chase timing, per the script's syncTo.
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (scene != nullptr)
    {
        auto finite = [](uint s) {
            return s != 0 && s != Function::defaultSpeed() && s != Function::infiniteSpeed();
        };
        const QString sync = m_script.syncTo();
        if (sync == QLatin1String("entrance"))
        {
            const uint fi = scene->fadeInSpeed();
            if (finite(fi)) return int(fi);
        }
        else if (sync == QLatin1String("exit"))
        {
            const uint fo = scene->fadeOutSpeed();
            if (finite(fo)) return int(fo);
        }
        else // "span" → the driving chase step's duration
        {
            for (Function *f : m_doc->functions())
            {
                if (f == nullptr || f->type() != Function::ChaserType || !f->isRunning())
                    continue;
                Chaser *ch = qobject_cast<Chaser*>(f);
                if (ch == nullptr)
                    continue;
                const int cur = ch->currentStepIndex();
                ChaserStep *cs = (cur >= 0) ? ch->stepAt(cur) : nullptr;
                if (cs != nullptr && cs->fid == m_sceneId)
                {
                    const uint d = ch->currentStepDuration();
                    if (d > 0)
                        return int(d);
                    break;
                }
            }
        }
    }

    // 3. Script default.
    return m_script.naturalDurationMs();
}

QList<quint32> EffectInstance::lookFixtureIds() const
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (!scene)
        return {};

    QList<quint32> fxIds;
    for (quint32 gid : scene->fixtureGroups())
    {
        FixtureGroup *grp = m_doc->fixtureGroup(gid);
        if (grp)
            fxIds.append(grp->fixtureList());
    }
    for (quint32 fid : scene->fixtures())
        if (!fxIds.contains(fid))
            fxIds.append(fid);
    return fxIds;
}

QList<quint32> EffectInstance::effectiveFixtureIds() const
{
    QList<quint32> fxIds = lookFixtureIds();
    // A WLED look's WLED fixtures run it themselves (wledDeviceWrites()).
    if (!m_wledEffect.isEmpty())
        fxIds.erase(std::remove_if(fxIds.begin(), fxIds.end(), [this](quint32 fid) {
            return WledFixture::runsEffects(m_doc->fixture(fid));
        }), fxIds.end());

    // An effect drives ONLY the fixtures/groups explicitly assigned to its look.
    // (There used to be a fallback that ran the effect on every fixture in the
    // doc when the look had no targets — convenient for authoring, but it meant
    // adding an always-on effect like Confetti immediately lit the whole rig
    // before any fixtures were chosen. No targets now means no output.)
    if (fxIds.isEmpty())
        return {};

    const QStringList types = m_script.fixtureTypes();
    if (types.isEmpty())
        return fxIds;

    // Filter by the script's declared capability types.
    // A fixture matches if it has at least one of the declared capabilities.
    QList<quint32> filtered;
    for (quint32 fid : fxIds)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi || !fxi->fixtureMode())
            continue;

        bool hasRGB = false, hasPT = false, hasDimmer = false;
        bool hasShutt = false, hasColorWheel = false, hasUV = false;
        for (quint32 c = 0; c < fxi->channels(); ++c)
        {
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (!ch) continue;
            switch (ch->group())
            {
                case QLCChannel::Pan:
                case QLCChannel::Tilt:
                    hasPT = true; break;
                case QLCChannel::Intensity:
                    if (ch->colour() == QLCChannel::Red   ||
                        ch->colour() == QLCChannel::Green ||
                        ch->colour() == QLCChannel::Blue)
                        hasRGB = true;
                    else if (ch->colour() == QLCChannel::UV)
                        hasUV = true;
                    else
                        hasDimmer = true;
                    break;
                case QLCChannel::Colour:
                    if (ch->colour() == QLCChannel::Red   ||
                        ch->colour() == QLCChannel::Green ||
                        ch->colour() == QLCChannel::Blue)
                        hasRGB = true;
                    else if (ch->colour() == QLCChannel::UV)
                        hasUV = true;
                    else
                        hasColorWheel = true;
                    break;
                case QLCChannel::Shutter:
                    hasShutt = true; break;
                default: break;
            }
        }

        bool matches = false;
        for (const QString &t : types)
        {
            const QString tl = t.toLower();
            // RGB variants: rgb, rgbw, rgba, rgbwa, rgbwauv, wash, color
            if ((tl == QLatin1String("rgb")      ||
                 tl == QLatin1String("rgbw")     ||
                 tl == QLatin1String("rgba")     ||
                 tl == QLatin1String("rgbwa")    ||
                 tl == QLatin1String("rgbwauv")  ||
                 tl == QLatin1String("wash")     ||
                 tl == QLatin1String("color"))   && hasRGB)       { matches = true; break; }
            // Moving head variants: moving, mover, movers, pantilt, spot, beam
            if ((tl == QLatin1String("moving")   ||
                 tl == QLatin1String("mover")    ||
                 tl == QLatin1String("pantilt")  ||
                 tl == QLatin1String("movers")   ||
                 tl == QLatin1String("spot")     ||
                 tl == QLatin1String("beam"))    && hasPT)         { matches = true; break; }
            if (tl == QLatin1String("dimmer")    && hasDimmer)     { matches = true; break; }
            if (tl == QLatin1String("shutter")   && hasShutt)      { matches = true; break; }
            if (tl == QLatin1String("colorwheel") && hasColorWheel){ matches = true; break; }
            if ((tl == QLatin1String("uv")       ||
                 tl == QLatin1String("uvonly"))  && hasUV)         { matches = true; break; }
        }
        if (matches)
            filtered.append(fid);
    }
    return filtered;
}

QList<EffectInstance::EffectCell>
EffectInstance::effectiveCells(int &gridCols, int &gridRows) const
{
    QList<EffectCell> cells;
    gridCols = 1;
    gridRows = 1;

    const QList<quint32> fxIds = effectiveFixtureIds();   // type-filtered fixtures
    if (fxIds.isEmpty())
        return cells;
    const QSet<quint32> allowed(fxIds.constBegin(), fxIds.constEnd());

    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    const QList<quint32> grpIds = scene ? scene->fixtureGroups() : QList<quint32>();

    // A multi-group look's stage grid, while it is fresh (see below).
    QList<quint32> stageKey;
    stageKey << grpIds << 0xFFFFFFFFu << fxIds;
    if (grpIds.size() > 1 && stageKey == m_stageKey
        && m_stageAge.isValid() && m_stageAge.elapsed() < 1000)
    {
        gridCols = m_stageCols;
        gridRows = m_stageRows;
        return m_stageCells;
    }

    // Per-HEAD cells from the fixture-group head layouts. Every head of a
    // multi-head/pixel fixture is its own cell -- this is what lets an effect
    // address individual pixels instead of a whole fixture (RGB Matrix
    // parity); a single-head fixture is just one cell.
    struct GroupLayout
    {
        QMap<QLCPoint, GroupHead> heads;
        int width = 0, height = 0;
    };
    QList<GroupLayout> layouts;
    for (quint32 gid : grpIds)
    {
        FixtureGroup *grp = m_doc->fixtureGroup(gid);
        if (grp == nullptr || grp->size().width() <= 0 || grp->size().height() <= 0
                           || grp->headsMap().isEmpty())
            continue;
        GroupLayout gl;
        gl.width = grp->size().width();
        gl.height = grp->size().height();
        const QMap<QLCPoint, GroupHead> heads = grp->headsMap();
        for (auto it = heads.constBegin(); it != heads.constEnd(); ++it)
            if (allowed.contains(it.value().fxi))
                gl.heads.insert(it.key(), it.value());
        if (!gl.heads.isEmpty())
            layouts << gl;
    }

    int maxRows = 0;
    for (const GroupLayout &gl : layouts)
        maxRows = qMax(maxRows, gl.height);

    /* Two or more groups: one grid laid out AS THE RIG STANDS, every head at
       the place on stage the rig view paints it (RigHeads), to scale -- across
       is across the stage (stage X, smaller on the left, as the group wizard
       lays groups out), up is height, or upstage when they all stand at one
       height. Laid side by side, each group in its own layout, a sine over
       towers, the bars between them and the step tops ran down the towers,
       then the bars, then the steps, with "up" meaning something different
       in each. Heads closer together than a cell share it; heads not on the
       stage go after the rest, in a strip of their own. */
    if (layouts.size() > 1)
    {
        struct Spot
        {
            quint32 fid = 0;
            int head = 0;
            QVector3D at;
            bool placed = false;
        };
        QList<Spot> spots;
        QSet<quint64> seen;
        QHash<quint32, QVector<QVector3D> > headsAt;
        const MonitorProperties *mp = m_doc->monitorProperties();
        for (const GroupLayout &gl : layouts)
            for (auto it = gl.heads.constBegin(); it != gl.heads.constEnd(); ++it)
            {
                const GroupHead &gh = it.value();
                const quint64 key = (quint64(gh.fxi) << 32) | quint32(gh.head);
                if (seen.contains(key))
                    continue;
                seen.insert(key);
                Spot sp;
                sp.fid = gh.fxi;
                sp.head = gh.head;
                if (!headsAt.contains(gh.fxi))      // once per fixture; empty = not on the stage
                    headsAt.insert(gh.fxi, (mp != nullptr && !mp->fixtureRigPosition(gh.fxi).isNull())
                                           ? RigHeads::headPositions(m_doc, gh.fxi)
                                           : QVector<QVector3D>());
                const QVector<QVector3D> &hp = headsAt[gh.fxi];
                if (gh.head >= 0 && gh.head < hp.size())
                {
                    sp.at = hp.at(gh.head);
                    sp.placed = true;
                }
                spots << sp;
            }

        double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, z0 = 1e9, z1 = -1e9;
        int placed = 0;
        for (const Spot &sp : spots)
        {
            if (!sp.placed)
                continue;
            ++placed;
            x0 = qMin(x0, double(sp.at.x())); x1 = qMax(x1, double(sp.at.x()));
            y0 = qMin(y0, double(sp.at.y())); y1 = qMax(y1, double(sp.at.y()));
            z0 = qMin(z0, double(sp.at.z())); z1 = qMax(z1, double(sp.at.z()));
        }
        if (placed > 0)
        {
            const bool byHeight = (z1 - z0) >= (y1 - y0);
            const double v0 = byHeight ? z0 : y0, v1 = byHeight ? z1 : y1;

            /* The cell: the rig's own pixel pitch -- the median spacing of
               neighbouring heads on one fixture -- so a pixel bar keeps its
               pixels apart. Coarser only when that would be a huge grid: the
               script draws every cell, every tick. */
            QList<double> pitches;
            for (auto it = headsAt.constBegin(); it != headsAt.constEnd(); ++it)
                for (int i = 1; i < it.value().size(); ++i)
                {
                    const double dd = double((it.value().at(i) - it.value().at(i - 1)).length());
                    if (dd > 0.005)
                        pitches << dd;
                }
            double cellM = 0.1;
            if (!pitches.isEmpty())
            {
                std::sort(pitches.begin(), pitches.end());
                cellM = pitches.at(pitches.size() / 2);
            }
            const double W = x1 - x0, V = v1 - v0;
            const double maxCells = 16384.0;
            if ((W / cellM + 1.0) * (V / cellM + 1.0) > maxCells)
            {
                const double area = W * V;
                cellM = (area > 1e-6) ? qSqrt(area / maxCells) : qMax(W, V) / (maxCells - 1.0);
                // sqrt(area/n) is for a grid with no edge; round up past the +1s.
                while ((W / cellM + 1.0) * (V / cellM + 1.0) > maxCells)
                    cellM *= 1.05;
            }
            const int cols = qMax(1, int(qRound(W / cellM)) + 1);
            const int rows = qMax(1, int(qRound(V / cellM)) + 1);
            int strip = 0;
            for (const Spot &sp : spots)
            {
                if (sp.placed)
                {
                    const double v = byHeight ? sp.at.z() : sp.at.y();
                    const int col = qBound(0, int(qRound((sp.at.x() - x0) / cellM)), cols - 1);
                    const int row = qBound(0, int(qRound((v1 - v) / cellM)), rows - 1);   // top first
                    cells.append({sp.fid, sp.head, col, row});
                }
                else
                {
                    cells.append({sp.fid, sp.head, cols + strip, 0});
                    ++strip;
                }
            }
            gridCols = cols + strip;
            gridRows = rows;
            m_stageCells = cells;
            m_stageCols = gridCols;
            m_stageRows = gridRows;
            m_stageKey = stageKey;
            m_stageAge.start();
            return cells;
        }
    }

    // One group (or nothing on the stage): side by side, left to right, so
    // two panels next to each other form ONE wide grid.
    int xOffset = 0;
    for (const GroupLayout &gl : layouts)
    {
        for (auto it = gl.heads.constBegin(); it != gl.heads.constEnd(); ++it)
            cells.append({it.value().fxi, it.value().head, xOffset + it.key().x(), it.key().y()});
        xOffset += gl.width;
    }

    if (!cells.isEmpty())
    {
        gridCols = qMax(1, xOffset);
        gridRows = qMax(1, maxRows);
        return cells;
    }

    // Fallback: no laid-out group → a 1-D strip of every head of the loose
    // fixtures, in fixture order.
    int col = 0;
    for (quint32 fid : fxIds)
    {
        Fixture *fxi = m_doc->fixture(fid);
        if (fxi == nullptr)
            continue;
        const int nh = qMax(1, fxi->heads());
        for (int h = 0; h < nh; ++h)
            cells.append({fid, h, col++, 0});
    }
    gridCols = qMax(1, col);
    gridRows = 1;
    return cells;
}

bool EffectInstance::aimCentre(QVector3D *centre) const
{
    const MonitorProperties *mp = m_doc->monitorProperties();
    Scene *sc = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (mp == nullptr || sc == nullptr)
        return false;
    for (quint32 spid : sc->palettes())
    {
        QLCPalette *ap = m_doc->palette(spid);
        if (ap == nullptr || ap->type() != QLCPalette::Aim)
            continue;
        if (StageTarget *st = mp->stageTarget(ap->stageTargetId()))
        {
            *centre = st->position();
            return true;                // first Aim look wins, as for Stage Shape
        }
    }
    return false;
}

QList<EffectInstance::SpreadDot> EffectInstance::spreadPreview(QPointF *centre, bool *hasCentre) const
{
    QList<SpreadDot> dots;
    QLCPalette *pal = m_doc->palette(m_effectPaletteId);
    const MonitorProperties *mp = m_doc->monitorProperties();
    const SpreadLayer::Settings spread = pal != nullptr ? SpreadLayer::fromParams(pal->effectParamValues())
                                                        : SpreadLayer::Settings();
    int cols = 0, rows = 0;
    const QList<EffectCell> cells = effectiveCells(cols, rows);
    QVector<SpreadLayer::Point> pts;
    for (int i = 0; i < cells.size(); ++i)
    {
        SpreadLayer::Point p;
        p.order = i;
        p.col = cells.at(i).col;
        p.row = cells.at(i).row;
        p.pos = mp != nullptr ? mp->fixtureRigPosition(cells.at(i).fixtureId) : QVector3D();
        pts << p;
    }
    QVector3D c = mp != nullptr ? mp->stageCentre() : QVector3D();
    const bool hc = SpreadLayer::needsCentre(spread.axis) && aimCentre(&c);
    if (!hc && mp != nullptr)
        c = mp->stageCentre();
    const QVector<double> ph = SpreadLayer::phases(pts, spread, &c);
    for (int i = 0; i < pts.size(); ++i)
    {
        SpreadDot d;
        d.plan = QPointF(pts.at(i).pos.x(), pts.at(i).pos.y());
        d.col = pts.at(i).col;
        d.row = pts.at(i).row;
        d.phase = ph.at(i);
        dots << d;
    }
    if (centre != nullptr)
        *centre = QPointF(c.x(), c.y());
    if (hasCentre != nullptr)
        *hasCentre = hc;
    return dots;
}

QJSValue EffectInstance::buildFixturesArray(const QList<EffectInstance::EffectCell> &cells,
                                            int gridCols, int gridRows)
{
    QJSEngine *engine = m_script.engine();
    const MonitorProperties *mp = m_doc->monitorProperties();
    QLCPalette *effectPal = m_doc->palette(m_effectPaletteId);
    ++m_factsTick;

    /* Each fixture once: its facts when it has changed, where it is now. */
    bool rebuild = cells != m_cellsBuilt || gridCols != m_cellsBuiltCols
                   || gridRows != m_cellsBuiltRows;
    bool moved = false;
    for (const EffectCell &cell : cells)
    {
        FixtureFacts &f = m_facts[cell.fixtureId];
        if (f.seen == m_factsTick)
            continue;
        f.seen = m_factsTick;
        Fixture *fxi = m_doc->fixture(cell.fixtureId);
        if (fxi == nullptr)
        {
            rebuild = rebuild || f.fixture != nullptr;
            f.fixture = nullptr;
            continue;
        }
        if (fxi != f.fixture || fxi->fixtureMode() != f.mode
            || fxi->heads() != f.heads || fxi->name() != f.name)
        {
            rebuild = true;
            f.fixture = fxi;
            f.mode = fxi->fixtureMode();
            f.name = fxi->name();
            f.heads = fxi->heads();
            readFixtureFacts(fxi, f);
        }

        // 3D rig position from MonitorProperties (0,0,0 if not set)
        const QVector3D pos = mp ? mp->fixtureRigPosition(cell.fixtureId) : QVector3D();
        if (f.posObj.isUndefined())
        {
            f.posObj = engine->newObject();
            f.pos = QVector3D(qQNaN(), 0, 0);   // so it is written below
        }
        if (pos != f.pos)
        {
            moved = true;
            f.pos = pos;
            f.posObj.setProperty("x", double(pos.x()));
            f.posObj.setProperty("y", double(pos.y()));
            f.posObj.setProperty("z", double(pos.z()));
        }
    }

    /* The cells: what does not change from tick to tick. */
    if (rebuild)
    {
        for (auto it = m_facts.begin(); it != m_facts.end(); )
        {
            if (it->seen != m_factsTick)
                it = m_facts.erase(it);
            else
                ++it;
        }
        m_cellObjects.clear();
        m_cellObjects.reserve(cells.size());
        for (const EffectCell &cell : cells)
        {
            FixtureFacts &f = m_facts[cell.fixtureId];
            f.sceneTargetSet = f.lastSpotSet = false;
            if (f.fixture == nullptr)
            {
                m_cellObjects.append(QJSValue());
                continue;
            }
            QJSValue obj = engine->newObject();
            obj.setProperty("id",   int(cell.fixtureId));
            obj.setProperty("head", cell.head);
            // Its name, for a script that works by how a rig is named ("DS1 #3",
            // "CS2t1") -- a test pattern that knows each step and strip.
            obj.setProperty("name", f.name);
            obj.setProperty("heads", f.heads);
            obj.setProperty("panRange",      f.panRange);
            obj.setProperty("tiltRange",     f.tiltRange);
            obj.setProperty("hasPanTilt",    f.hasPanTilt);
            obj.setProperty("hasRGB",        f.hasRGB);
            obj.setProperty("hasColorWheel", f.hasColorWheel);
            obj.setProperty("hasDimmer",     f.hasDimmer);
            obj.setProperty("hasGobo",       f.hasGobo);
            obj.setProperty("hasShutter",    f.hasShutter);
            // The lens a zoom channel sweeps: its intent 0..1 runs lensMin..lensMax.
            obj.setProperty("hasZoom",       f.hasZoom && f.lensMax > f.lensMin);
            obj.setProperty("lensMin",       double(f.lensMin));
            obj.setProperty("lensMax",       double(f.lensMax));
            obj.setProperty("pos", f.posObj);

            // grid: this HEAD's cell for pixel-style effects. col/row = cell position
            // (from the group head layout, or a 1-D strip); cols/rows = grid size.
            QJSValue gridObj = engine->newObject();
            gridObj.setProperty("col",  cell.col);
            gridObj.setProperty("row",  cell.row);
            gridObj.setProperty("cols", gridCols);
            gridObj.setProperty("rows", gridRows);
            obj.setProperty("grid", gridObj);
            obj.setProperty("aimAt", engine->newObject());
            m_cellObjects.append(obj);
        }
        m_cellsBuilt = cells;
        m_cellsBuiltCols = gridCols;
        m_cellsBuiltRows = gridRows;
    }

    /* The spread layer (S3): each head's place in the cycle, as "phase" in
       cycles. Worked out again only when the cells, the settings or -- for a
       stage axis -- where things stand have changed. */
    {
        const SpreadLayer::Settings spread = effectPal != nullptr
            ? SpreadLayer::fromParams(effectPal->effectParamValues()) : SpreadLayer::Settings();
        const bool onStage = spread.axis >= SpreadLayer::StageX;
        /* Out from / around a target: the look's Aim target (the one Stage
           Shape centres on); without one, the middle of the heads. */
        // No Aim target: centre stage.
        QVector3D centre = mp != nullptr ? mp->stageCentre() : QVector3D();
        const bool hasCentre = spread.isActive() && SpreadLayer::needsCentre(spread.axis)
                               && aimCentre(&centre);
        if (!hasCentre && mp != nullptr)
            centre = mp->stageCentre();
        const bool centreMoved = hasCentre != m_spreadHadCentre || centre != m_spreadCentre;
        if (rebuild || spread != m_spreadBuilt
            || (onStage && spread.isActive() && (moved || centreMoved)))
        {
            QVector<SpreadLayer::Point> pts;
            pts.reserve(cells.size());
            for (int i = 0; i < cells.size(); ++i)
            {
                SpreadLayer::Point p;
                p.order = i;
                p.col = cells.at(i).col;
                p.row = cells.at(i).row;
                p.pos = m_facts.value(cells.at(i).fixtureId).pos;
                pts << p;
            }
            const QVector<double> ph = SpreadLayer::phases(pts, spread, &centre);
            m_spreadCentre = centre;
            m_spreadHadCentre = hasCentre;
            for (int i = 0; i < m_cellObjects.size() && i < ph.size(); ++i)
                if (!m_cellObjects.at(i).isUndefined())
                    m_cellObjects[i].setProperty("phase", ph.at(i));
            m_spreadBuilt = spread;
        }
    }

    /* What changes as it runs, worked out once a fixture. */
    const bool aiming = mp && effectPal && !m_script.targetDefs().isEmpty();
    StageTarget *sceneTarget = nullptr;
    if (mp)
    {
        // sceneTarget: the per-fixture aim toward the SCENE's own Aim look (not a
        // manual binding).  The followspot reads its target IMPLICITLY from the
        // scene this way, so it needs no followTarget slot in its menu.  Present
        // only when the scene actually carries an Aim look; the script defers to
        // the host's converged aim whenever it is present.
        if (Scene *sc = qobject_cast<Scene*>(m_doc->function(m_sceneId)))
        {
            for (quint32 spid : sc->palettes())
            {
                QLCPalette *ap = m_doc->palette(spid);
                if (ap == NULL || ap->type() != QLCPalette::Aim)
                    continue;
                sceneTarget = mp->stageTarget(ap->stageTargetId());
                if (sceneTarget != NULL)
                    break; // first Aim look wins
            }
        }
    }
    QHash<quint32, QJSValue> aimAtOf, sceneTargetOf, lastSpotOf;
    if (aiming || sceneTarget || !m_lastSpotIn.isEmpty())
    {
        for (auto it = m_facts.begin(); it != m_facts.end(); ++it)
        {
            const quint32 fid = it.key();
            const FixtureFacts &f = it.value();
            if (f.fixture == nullptr)
                continue;
            if (aiming)
                aimAtOf.insert(fid, aimAtObject(fid, f.pos, effectPal));
            if (sceneTarget)
                sceneTargetOf.insert(fid, sceneTargetObject(fid, sceneTarget));
            // lastSpot: pan/tilt (degrees) where this fixture's beam was left by the
            // previous Operate-mode tick (possibly from a different scene).  Lets a
            // followspot script seed followMode = "lastPosition" so the beam holds
            // across scene transitions instead of snapping.  Property is absent when
            // no prior position is known (e.g. first use after launch).
            if (m_lastSpotIn.contains(fid))
            {
                const QPointF ls = m_lastSpotIn.value(fid);
                QJSValue lastObj = engine->newObject();
                lastObj.setProperty("pan",  ls.x());
                lastObj.setProperty("tilt", ls.y());
                lastSpotOf.insert(fid, lastObj);
            }
        }
    }

    QJSValue arr = engine->newArray(uint(cells.size()));
    QSet<quint32> sceneTargetCleared, lastSpotCleared;
    for (int i = 0; i < cells.size(); ++i)
    {
        QJSValue &obj = m_cellObjects[i];
        if (obj.isUndefined())
            continue;
        const quint32 fid = cells.at(i).fixtureId;
        FixtureFacts &f = m_facts[fid];
        if (aiming)
            obj.setProperty("aimAt", aimAtOf.value(fid));
        if (sceneTarget)
            obj.setProperty("sceneTarget", sceneTargetOf.value(fid));
        else if (f.sceneTargetSet)
        {
            obj.deleteProperty("sceneTarget");
            sceneTargetCleared.insert(fid);
        }
        /* lookAim: where the look's own Aim / Pan-Tilt points this head, as
           fractions of its pan and tilt range (the scripts' own units) -- the
           centre a movement effect moves round ("should I expect the lights
           to ballyhoo over the point of target 2?"). Absent when the look
           doesn't point it. */
        if (f.fixture != nullptr)
        {
            const quint32 panCh = f.fixture->channelNumber(QLCChannel::Pan, QLCChannel::MSB);
            const quint32 tiltCh = f.fixture->channelNumber(QLCChannel::Tilt, QLCChannel::MSB);
            const quint32 panFine = f.fixture->channelNumber(QLCChannel::Pan, QLCChannel::LSB);
            const quint32 tiltFine = f.fixture->channelNumber(QLCChannel::Tilt, QLCChannel::LSB);
            auto has = [this, fid](quint32 ch) {
                return ch != QLCChannel::invalid()
                       && m_sceneBaseValues.contains((quint64(fid) << 32) | ch);
            };
            if (has(panCh) || has(tiltCh))
            {
                auto frac = [this, fid](quint32 msb, quint32 lsb) {
                    const int hi = sceneBaseValue(fid, msb, 128);
                    const int lo = lsb != QLCChannel::invalid() ? sceneBaseValue(fid, lsb, 0) : 0;
                    return double((hi << 8) | lo) / 65535.0;
                };
                QJSValue la = engine->newObject();
                la.setProperty("pan", has(panCh) ? frac(panCh, panFine) : 0.5);
                la.setProperty("tilt", has(tiltCh) ? frac(tiltCh, tiltFine) : 0.5);
                obj.setProperty("lookAim", la);
            }
            else if (obj.hasOwnProperty("lookAim"))
                obj.deleteProperty("lookAim");
        }
        if (lastSpotOf.contains(fid))
            obj.setProperty("lastSpot", lastSpotOf.value(fid));
        else if (f.lastSpotSet)
        {
            obj.deleteProperty("lastSpot");
            lastSpotCleared.insert(fid);
        }
        arr.setProperty(quint32(i), obj);
    }
    for (auto it = m_facts.begin(); it != m_facts.end(); ++it)
    {
        if (sceneTarget)
            it->sceneTargetSet = true;
        else if (sceneTargetCleared.contains(it.key()))
            it->sceneTargetSet = false;
        if (lastSpotOf.contains(it.key()))
            it->lastSpotSet = true;
        else if (lastSpotCleared.contains(it.key()))
            it->lastSpotSet = false;
    }
    return arr;
}

void EffectInstance::readFixtureFacts(const Fixture *fxi, FixtureFacts &f) const
{
    f.panRange = 360.0f;
    f.tiltRange = 270.0f;
    f.lensMin = f.lensMax = 0.0f;
    f.hasPanTilt = f.hasRGB = f.hasColorWheel = f.hasDimmer = false;
    f.hasGobo = f.hasShutter = f.hasZoom = false;
    const QLCFixtureMode *mode = fxi->fixtureMode();
    if (mode == nullptr)
        return;

    const QLCPhysical &phy = mode->physical();
    if (phy.focusPanMax() > 0)  f.panRange  = (float)phy.focusPanMax();
    if (phy.focusTiltMax() > 0) f.tiltRange = (float)phy.focusTiltMax();
    f.lensMin = float(qMin(phy.lensDegreesMin(), phy.lensDegreesMax()));
    f.lensMax = float(qMax(phy.lensDegreesMin(), phy.lensDegreesMax()));

    bool hasR = false, hasG = false, hasB = false;
    quint32 nCh = fxi->channels();
    for (quint32 c = 0; c < nCh; ++c)
    {
        QLCChannel *ch = mode->channel(c);
        if (!ch) continue;
        switch (ch->group())
        {
            case QLCChannel::Pan:
            case QLCChannel::Tilt:
                f.hasPanTilt = true; break;
            case QLCChannel::Colour:
                if (ch->colour() == QLCChannel::Red)        hasR = true;
                else if (ch->colour() == QLCChannel::Green)  hasG = true;
                else if (ch->colour() == QLCChannel::Blue)   hasB = true;
                else f.hasColorWheel = true;
                break;
            case QLCChannel::Intensity:
                // IntensityRed/Green/Blue presets land in the Intensity group
                // but carry a colour value — treat them as additive RGB channels.
                if (ch->colour() == QLCChannel::Red)        hasR = true;
                else if (ch->colour() == QLCChannel::Green)  hasG = true;
                else if (ch->colour() == QLCChannel::Blue)   hasB = true;
                else f.hasDimmer = true;
                break;
            case QLCChannel::Beam:
                if (ch->preset() == QLCChannel::BeamZoomSmallBig
                    || ch->preset() == QLCChannel::BeamZoomBigSmall)
                    f.hasZoom = true;
                break;
            case QLCChannel::Gobo:
                f.hasGobo = true; break;
            case QLCChannel::Shutter:
                f.hasShutter = true; break;
            default: break;
        }
    }
    f.hasRGB = hasR && hasG && hasB;
}

QJSValue EffectInstance::aimAtObject(quint32 fid, const QVector3D &pos, QLCPalette *effectPal)
{
    // aimAt: pre-computed pan/tilt for each bound target slot
    QJSValue aimAt = m_script.engine()->newObject();
    const MonitorProperties *mp = m_doc->monitorProperties();
    const QMap<QString, quint32> &tgtBindings = effectPal->effectTargetBindings();
    for (const EffectScript::TargetDef &td : m_script.targetDefs())
    {
        quint32 tid = tgtBindings.value(td.name, StageTarget::invalidId());
        StageTarget *tgt = mp->stageTarget(tid);
        if (!tgt || pos == QVector3D())
        {
            aimAt.setProperty(td.name, m_script.engine()->newObject());
            continue;
        }
        // Follow spot: aim at the SUBJECT's body, not the target's
        // configured Z — through the shared rule, so a bound target
        // follows its structure and a Person is aimed at its own height.
        const QVector3D tgtPos = AimSolver::aimPointFor(m_doc, fid, tgt, true);

        // Use the SHARED aim solver — the exact same geometry the Aim
        // palette uses in Edit — so the beam doesn't jump on Edit→Run.
        // aimAt.pan/tilt are fixture degrees; the host converts via
        // positionToValues (see parseIntents).
        AimSolver::Solution sol;
        QJSValue aimObj = m_script.engine()->newObject();
        if (AimSolver::reach(m_doc, fid, tgtPos, sol))
        {
            aimObj.setProperty("pan",  (double)sol.panDeg);
            aimObj.setProperty("tilt", (double)sol.tiltDeg);
            aimObj.setProperty("reachMargin", (double)sol.marginDeg);
        }
        aimAt.setProperty(td.name, aimObj);
    }
    return aimAt;
}

QJSValue EffectInstance::sceneTargetObject(quint32 fid, StageTarget *st)
{
    const MonitorProperties *mp = m_doc->monitorProperties();
    QVector3D raw = st->position();

    QJSValue so = m_script.engine()->newObject();
    // Raw target world location (metres) — the anchor a shape /
    // formation effect centres on.  Shared by every fixture and
    // independent of rig geometry, so it is always present.
    so.setProperty("x", double(raw.x()));
    so.setProperty("y", double(raw.y()));
    so.setProperty("z", double(raw.z()));

    // Convenience aim angles at the SUBJECT's body (not the
    // target's configured Z), present only when this fixture has
    // usable rig geometry.  The followspot defers on these.
    // reachMargin: degrees to spare before the head's pan/tilt
    // limit (negative = it cannot point there), so a script can
    // fade a head out before it sticks.
    // distance: metres from the head to that point, so a script can
    // size the beam on the subject rather than at the lens.
    // Pointed where its beam covers the subject best; sized
    // at the subject itself (its chest), so the size it asks
    // the zoom for does not move with the aim it gets.
    const QVector3D tp = AimSolver::aimPointFor(m_doc, fid, st, true);
    const QVector3D subject = mp->targetAimPoint(st, true);
    AimSolver::Frame frame;
    if (AimSolver::frameFor(m_doc, fid, frame))
    {
        const AimSolver::Solution sol = AimSolver::solve(frame, tp);
        so.setProperty("pan",  double(sol.panDeg));
        so.setProperty("tilt", double(sol.tiltDeg));
        so.setProperty("reachMargin", double(sol.marginDeg));
        so.setProperty("distance", double((subject - frame.position).length()));
    }
    return so;
}

int EffectInstance::speedAssignment() const
{
    // The effect's own choice, else its look's (which may be handed down by
    // the chase step, chase or collection playing it).
    if (QLCPalette *pal = m_doc->palette(m_effectPaletteId))
    {
        const QMap<QString, double> &pp = pal->effectParamValues();
        auto it = pp.constFind(QStringLiteral("__speedMaster"));
        if (it != pp.constEnd() && int(it.value()) != SpeedMasters::Inherit)
            return int(it.value());
    }
    if (Function *f = m_doc->function(m_sceneId))
        return f->effectiveSpeedMaster();
    return SpeedMasters::Inherit;
}

QJSValue EffectInstance::buildInputsObject() const
{
    QJSValue obj = m_script.engine()->newObject();
    for (auto it = m_inputValues.constBegin(); it != m_inputValues.constEnd(); ++it)
        obj.setProperty(it.key(), (double)it.value());

    // Beat/tempo inputs from a beat clock so scripts can lock to music
    // tempo -- a SHARED clock, so every effect is on the same beat however far
    // apart they were started, and a tap or an external beat pulls them all
    // onto it (SPEED_SPREAD_DESIGN.md, S1):
    //   _beat      — 0.0→1.0 sawtooth phase within the current beat
    //   _bpm       — current tempo (0 when there is no clock, or it's paused)
    //   _beatCount — whole beats since this instance started
    //   _beatPos   — the clock's running beat count (fraction = _beat)
    // The clock is the main one, unless the effect is on a speed master (S2).
    // ON a master, _time counts that master's BEATS, not seconds: a speed
    // param of 1 is then one cycle a beat, the effect stays locked to the
    // master's beat, follows its tap and rate, and stops when it's paused.
    double pos = 0, bpm = 0;
    const int assignment = speedAssignment();
    const bool onMaster = assignment != SpeedMasters::Free && assignment != SpeedMasters::Inherit
                          && m_doc->speedMasters()->clockFor(assignment, &pos, &bpm);
    if (!onMaster)
    {
        MasterTimer *mt = m_doc->masterTimer();
        pos = mt != nullptr ? mt->beatPosition() : 0.0;
        bpm = mt != nullptr ? mt->bpm() : 0.0;
    }
    if (assignment != m_clockAssignment)
    {
        // A different clock: count beats from where it is now.
        m_clockAssignment = assignment;
        m_startBeat = std::floor(pos);
    }
    obj.setProperty("_time",      onMaster ? pos : double(m_elapsed.elapsed()) / 1000.0);
    obj.setProperty("_bpm",       bpm);
    obj.setProperty("_beat",      pos - std::floor(pos));
    obj.setProperty("_beatCount", int(std::floor(pos) - m_startBeat));
    obj.setProperty("_beatPos",   pos);

    // The host's spread is on: read fixture.phase, not the script's own.
    QLCPalette *spreadPal = m_doc->palette(m_effectPaletteId);
    const SpreadLayer::Settings spreadNow = spreadPal != nullptr
        ? SpreadLayer::fromParams(spreadPal->effectParamValues()) : SpreadLayer::Settings();
    obj.setProperty("_spread", spreadNow.isActive());
    obj.setProperty("_spreadAmount", spreadNow.amount);     // cycles, for effects that lay heads out by it

    // One-shot lifecycle: hand the effect its window so it fits any duration.
    //   phase    — 0→1 progress through the resolved run length
    //   duration — that length in ms
    if (m_script.isOneShot())
    {
        const int dur = resolvedDurationMs();
        obj.setProperty("phase", dur > 0 ? qMin(1.0, double(m_elapsed.elapsed()) / double(dur)) : 1.0);
        obj.setProperty("duration", double(dur));
    }

    return obj;
}

// Make a {r,g,b} JS object from a QColor.
static QJSValue colorToJS(QJSEngine *eng, const QColor &c)
{
    QJSValue o = eng->newObject();
    o.setProperty("r", c.red());
    o.setProperty("g", c.green());
    o.setProperty("b", c.blue());
    return o;
}

QJSValue EffectInstance::buildPalettesObject() const
{
    QJSEngine *eng = m_script.engine();
    QJSValue obj = eng->newObject();

    QLCPalette *effectPal = m_doc->palette(m_effectPaletteId);
    if (!effectPal)
        return obj;

    // The LOOK is the data source: collect the scene's own Colour/Dimmer
    // palettes in precedence order (the same order the user arranges them in the
    // Looks list). Unbound script slots are auto-filled from these, so a creator
    // just assembles a look (e.g. Red then Blue) and the effect consumes it —
    // no per-script colour binding required. Explicit bindings remain optional
    // per-slot overrides. Effect palettes (including this one) are skipped.
    // Ordering paradigm: Colour palettes placed AFTER this effect in the look list
    // are "the effect's colours" (they feed the gradient and are NOT painted onto
    // the fixtures as a static base). Colours BEFORE the effect are the static
    // base and are not consumed here. If nothing follows the effect, fall back to
    // ALL colours so existing looks keep working (the base then also shows —
    // reorder colours after the effect to separate them).
    QList<QColor> lookColors;
    double lookDimmer = 1.0;
    bool   haveLookDimmer = false;
    lookColours(lookColors, lookDimmer, &haveLookDimmer);

    const QMap<QString, quint32> &bindings = effectPal->effectPaletteBindings();
    int nextLookColor = 0;   // walks lookColors as unbound Colour slots claim them

    for (const EffectScript::PaletteDef &def : m_script.paletteDefs())
    {
        quint32 pid = bindings.value(def.name, QLCPalette::invalidId());
        QLCPalette *pal = m_doc->palette(pid);
        const QString type = def.type.toLower();

        QJSValue pObj = eng->newObject();   // empty/null-ish by default

        if (pal != NULL)
        {
            // Explicit binding wins.
            if (pal->type() == QLCPalette::Color)
                pObj = colorToJS(eng, pal->colorValue());
            else if (pal->type() == QLCPalette::Dimmer)
                pObj.setProperty("dimmer", pal->value().toDouble() / 255.0);
        }
        else if (type == QLatin1String("color") && nextLookColor < lookColors.size())
        {
            // Unbound colour slot → next look colour, in order.
            pObj = colorToJS(eng, lookColors.at(nextLookColor++));
        }
        else if (type == QLatin1String("dimmer") && haveLookDimmer)
        {
            // Unbound dimmer slot → the look's master dimmer.
            pObj.setProperty("dimmer", lookDimmer);
        }

        obj.setProperty(def.name, pObj);
    }

    // Also expose the whole ordered look directly, for scripts that want more
    // colours than they declare slots for (e.g. an N-way colour spread):
    //   palettes.look.colors = [{r,g,b}, ...]   (precedence order)
    //   palettes.look.dimmer = 0..1             (master dimmer, 1.0 if none)
    QJSValue colorsArr = eng->newArray(lookColors.size());
    for (int i = 0; i < lookColors.size(); i++)
        colorsArr.setProperty(i, colorToJS(eng, lookColors.at(i)));
    QJSValue lookObj = eng->newObject();
    lookObj.setProperty("colors", colorsArr);
    lookObj.setProperty("dimmer", lookDimmer);
    obj.setProperty("look", lookObj);

    return obj;
}

void EffectInstance::lookColours(QList<QColor> &colours, double &dimmer, bool *haveDimmer) const
{
    double lookDimmer = 1.0;
    bool   haveLookDimmer = false;
    QList<QColor> afterColors;   // colours positioned after this effect
    QList<QColor> allColors;     // every colour in the look (fallback)
    if (Scene *scene = qobject_cast<Scene *>(m_doc->function(m_sceneId)))
    {
        bool pastThisEffect = false;
        foreach (quint32 pid, scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (!p)
                continue;
            if (pid == m_effectPaletteId)
            {
                pastThisEffect = true;
                continue;
            }
            // The NEXT effect ends this effect's run of child colours (in the tree
            // view, those colours are nested under the other effect).
            if (p->type() == QLCPalette::Effect)
            {
                pastThisEffect = false;
                continue;
            }
            if (p->type() == QLCPalette::Color)
            {
                allColors.append(p->colorValue());
                if (pastThisEffect)
                    afterColors.append(p->colorValue());
            }
            else if (p->type() == QLCPalette::Dimmer)
            {
                lookDimmer = p->value().toDouble() / 255.0;
                haveLookDimmer = true;
            }
        }
    }
    colours = afterColors.isEmpty() ? allColors : afterColors;
    dimmer = lookDimmer;
    if (haveDimmer != nullptr)
        *haveDimmer = haveLookDimmer;
}

QList<EffectInstance::DmxWrite> EffectInstance::wledDeviceWrites() const
{
    QList<DmxWrite> out;
    if (m_wledEffect.isEmpty())
        return out;
    QList<QColor> colours;
    double dimmer = 1.0;
    lookColours(colours, dimmer);
    auto param = [this](const char *name, int fallback) {
        return qBound(0, qRound(m_paramValues.value(QLatin1String(name), fallback)), 255);
    };
    for (quint32 fid : lookFixtureIds())
    {
        Fixture *fx = m_doc->fixture(fid);
        if (!WledFixture::runsEffects(fx))
            continue;
        const int effect = WledFixture::effectValue(fx, m_wledEffect);
        if (effect < 0)
            continue;                       // its firmware's list lacks it
        auto put = [&](const QString &channel, int value, bool replace) {
            const int ch = WledFixture::channel(fx, channel);
            if (ch < 0)
                return;
            DmxWrite w;
            w.universeId = int(fx->universe());
            w.addr = int(fx->address()) + ch;
            w.value = uchar(qBound(0, value, 255));
            w.fixtureId = fid;
            w.channel = quint32(ch);
            w.replace = replace;
            out << w;
        };
        // Settings, not levels: they replace whatever the scene has.
        put(QStringLiteral("Effect"), effect, true);
        put(QStringLiteral("Effect Speed"), param("speed", 128), true);
        put(QStringLiteral("Effect Intensity"), param("intensity", 128), true);
        put(QStringLiteral("Palette"), param("palette", 0), true);
        put(QStringLiteral("Effect Option"), param("option", 0), true);
        static const char *const names[3][3] = {
            { "Red", "Green", "Blue" },
            { "Secondary Red", "Secondary Green", "Secondary Blue" },
            { "Tertiary Red", "Tertiary Green", "Tertiary Blue" } };
        for (int k = 0; k < 3; ++k)
        {
            const QColor c = colours.value(k, k == 0 ? QColor(Qt::white) : QColor(Qt::black));
            put(QLatin1String(names[k][0]), c.red(), true);
            put(QLatin1String(names[k][1]), c.green(), true);
            put(QLatin1String(names[k][2]), c.blue(), true);
        }
        // Its brightness is a level: the look's dimmer through the scene's
        // fade, merged with the scene's own (HTP).
        put(QStringLiteral("Master Dimmer"), qRound(255.0 * dimmer * double(envelopeFor(fid, -1))), false);
    }
    return out;
}

QString EffectInstance::error() const
{
    if (m_script.isValid())
        return QString();
    QLCPalette *pal = m_doc->palette(m_effectPaletteId);
    if (pal == nullptr)
        return QStringLiteral("the look is gone");
    if (pal->scriptPath().isEmpty())
        return QStringLiteral("no effect is chosen");
    return m_script.lastError().isEmpty() ? QStringLiteral("its script did not load")
                                          : m_script.lastError();
}

float EffectInstance::envelopeFor(quint32 fid, int head) const
{
    // Leaving by a transition: each cell goes when it reaches it.
    if (m_fadingOut && m_transitionOut.isActive())
    {
        const qint64 el = qMax<qint64>(0, m_elapsed.elapsed() - m_fadeOutStartMs);
        return float(1.0 - m_transitionOut.progress(m_transitionMapOut.place(fid, head), uint(el)));
    }
    // Arriving by one: each cell comes when it reaches it.
    if (!m_fadingOut && m_sceneRunning && m_transitionIn.isActive())
        return float(m_transitionIn.progress(m_transitionMapIn.place(fid, head), m_sceneElapsed));
    return m_envelope;
}

QJSValue EffectInstance::buildParamsObject() const
{
    QJSValue obj = m_script.engine()->newObject();

    // Scalar params
    for (auto it = m_paramValues.constBegin(); it != m_paramValues.constEnd(); ++it)
        obj.setProperty(it.key(), it.value());

    // Determine which params are path-typed so we can emit JS arrays for them
    QSet<QString> pathParams;
    for (const EffectScript::ParamDef &d : m_script.paramDefs())
        if (d.type == QLatin1String("path"))
            pathParams.insert(d.name);

    // String/path params
    for (auto it = m_stringParamValues.constBegin(); it != m_stringParamValues.constEnd(); ++it)
    {
        if (pathParams.contains(it.key()))
        {
            // Parse [[x,y], ...] JSON → JS array of 2-element arrays
            QJsonDocument jdoc = QJsonDocument::fromJson(it.value().toUtf8());
            QJSValue arr = m_script.engine()->newArray();
            int idx = 0;
            if (jdoc.isArray())
            {
                for (const QJsonValue &v : jdoc.array())
                {
                    if (!v.isArray() || v.toArray().size() < 2) continue;
                    QJSValue pair = m_script.engine()->newArray(2);
                    pair.setProperty(0, v.toArray()[0].toDouble());
                    pair.setProperty(1, v.toArray()[1].toDouble());
                    arr.setProperty(idx++, pair);
                }
            }
            arr.setProperty("length", idx);
            obj.setProperty(it.key(), arr);
        }
        else
        {
            obj.setProperty(it.key(), it.value());
        }
    }

    return obj;
}

QJSValue EffectInstance::buildDataChannelsObject() const
{
    QJSValue obj = m_script.engine()->newObject();
    const QStringList subscribed = m_script.dataChannelKeys();
    for (auto it = m_dataChannels.constBegin(); it != m_dataChannels.constEnd(); ++it)
    {
        if (!subscribed.isEmpty() && !subscribed.contains(it.key()))
            continue; // skip unsubscribed channels
        QJSValue chObj = m_script.engine()->newObject();
        const QVariantMap &vm = it.value();
        for (auto vit = vm.constBegin(); vit != vm.constEnd(); ++vit)
            chObj.setProperty(vit.key(), m_script.engine()->toScriptValue(vit.value()));
        obj.setProperty(it.key(), chObj);
    }
    return obj;
}

QList<EffectInstance::DmxWrite>
EffectInstance::parseIntents(const QJSValue &intents,
                              const QList<EffectInstance::EffectCell> &cells) const
{
    QList<DmxWrite> writes;
    m_lastIntentDeg.clear();

    int len = qMin(intents.property("length").toInt(), cells.size());
    for (int i = 0; i < len; ++i)
    {
        QJSValue intent = intents.property(i);
        if (!intent.isObject())
            continue;
        /* Its own keys, read once: asking each answer for every key it might
           have, head after head, was most of what reading them cost -- an
           answer usually has three. */
        QVarLengthArray<QPair<QString, QJSValue>, 8> keys;
        QJSValueIterator kit(intent);
        while (kit.hasNext())
        {
            kit.next();
            keys.append(qMakePair(kit.name(), kit.value()));
        }
        auto iv = [&keys](const char *name) -> QJSValue {
            for (const auto &k : keys)
                if (k.first == QLatin1String(name))
                    return k.second;
            return QJSValue();
        };

        const EffectCell &cell = cells.at(i);
        quint32 fid  = cell.fixtureId;
        int     head = cell.head;
        const float envelope = envelopeFor(fid, head);   // this cell's, by the transition
        Fixture *fxi = m_doc->fixture(fid);
        /* Unpatched fixtures too: their channels are inert (no address, no
           DMX), and what the effect would do goes to the fixture for the rig
           view -- a look on the inventory can be seen before it is patched. */
        if (!fxi || !fxi->fixtureMode())
            continue;
        // The set of channels this CELL owns: just this head's channels for a
        // multi-head/pixel fixture, or the whole fixture for a single-head one.
        // Colour + colour-scaled-dimmer writes target only these, so a pixel
        // effect lights individual pixels instead of the entire fixture.
        const QLCFixtureHead cellHead = fxi->head(head);
        QList<quint32> headChannels = cellHead.channels();
        if (headChannels.isEmpty())
            for (quint32 c = 0; c < fxi->channels(); ++c) headChannels << c;

        int uniIdx = (int)fxi->universe();

        // Track the pan/tilt degrees this fixture is driven to so the runner can
        // persist them for followMode = "lastPosition" handoff between scenes.
        QPointF spotDeg;
        bool spotPan = false, spotTilt = false;

        // Resolve pan/tilt degrees from either a world-space aim intent
        // (aimX/aimY/aimZ, absolute metres) solved per-fixture by the shared
        // AimSolver — the same geometry the Aim palette uses, so a beam points
        // the same way in Edit and Run — or from explicit pan/tilt degrees.
        // Explicit angles win when both are present.
        bool  havePan = false, haveTilt = false;
        float panDeg  = 0.0f,  tiltDeg  = 0.0f;

        QJSValue axVal = iv("aimX");
        QJSValue ayVal = iv("aimY");
        if (!axVal.isUndefined() && !ayVal.isUndefined())
        {
            float az = iv("aimZ").isUndefined()
                       ? 0.0f : (float)iv("aimZ").toNumber();
            QVector3D world((float)axVal.toNumber(), (float)ayVal.toNumber(), az);
            float pd = 0.0f, td = 0.0f;
            if (AimSolver::aimDegrees(m_doc, fid, world, pd, td))
            {
                panDeg = pd; tiltDeg = td;
                havePan = true; haveTilt = true;
            }
        }

        QJSValue panVal = iv("pan");
        if (!panVal.isUndefined())  { panDeg  = (float)panVal.toNumber();  havePan  = true; }
        QJSValue tiltVal = iv("tilt");
        if (!tiltVal.isUndefined()) { tiltDeg = (float)tiltVal.toNumber(); haveTilt = true; }

        // Pan
        if (havePan)
        {
            spotDeg.setX(panDeg); spotPan = true;
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Pan, panDeg);
            for (const SceneValue &sv : svs)
                writes.append({uniIdx, (int)fxi->address() + (int)sv.channel, sv.value, fxi->id(), sv.channel});
        }

        // Tilt
        if (haveTilt)
        {
            spotDeg.setY(tiltDeg); spotTilt = true;
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Tilt, tiltDeg);
            for (const SceneValue &sv : svs)
                writes.append({uniIdx, (int)fxi->address() + (int)sv.channel, sv.value, fxi->id(), sv.channel});
        }

        if (spotPan || spotTilt)
        {
            // Preserve the untouched axis from any prior known position so a
            // pan-only or tilt-only intent doesn't zero the other axis.
            QPointF prev = m_lastIntentDeg.value(fid, m_lastSpotIn.value(fid));
            if (spotPan)  prev.setX(spotDeg.x());
            if (spotTilt) prev.setY(spotDeg.y());
            m_lastIntentDeg[fid] = prev;
        }

        // Dimmer — treated as an intensity *multiplier* (0..1) on the fixture's
        // scene-assigned brightness, NOT an absolute level.  This lets a candle
        // (or any intensity effect) flicker AROUND the base a Dimmer/Colour look
        // sets, keeping the look's colour, and works on any fixture:
        //   • master-dimmer fixtures  → scale the master intensity channel
        //   • colour-only fixtures    → scale the scene's R/G/B (its colour)
        // Written REPLACE so the effect can dip the value BELOW the scene base
        // (HTP could only push it up).  Per-fixture phase (script `spread`) makes
        // fixtures flicker independently rather than in unison.
        QJSValue dimVal = iv("dimmer");
        // An intent that brings its own colour (a rainbow's hue) and a dimmer
        // on a fixture with no master dimmer: the dimmer dims THAT colour.
        // Scaling the emitters from the look's base instead put every one --
        // white too -- at full under the hue, and a rainbow came out white.
        const bool carriesColour = !iv("r").isUndefined() || !iv("g").isUndefined()
                                || !iv("b").isUndefined() || !iv("w").isUndefined();
        float ownColourMul = 1.0f;
        if (!dimVal.isUndefined())
        {
            const float mul = qBound(0.0f, (float)dimVal.toNumber(), 1.0f) * envelope;
            quint32 miCh = fxi->masterIntensityChannel();
            if (miCh != QLCChannel::invalid())
            {
                // Base from the scene's Dimmer look; full if the scene sets none.
                const uchar base = sceneBaseValue(fid, miCh, 255);
                const uchar out  = (uchar)qBound(0, (int)qRound(base * mul), 255);
                writes.append({uniIdx, (int)fxi->address() + (int)miCh,
                               out, fxi->id(), miCh, /*replace=*/true});
            }
            else if (carriesColour)
            {
                ownColourMul = qBound(0.0f, (float)dimVal.toNumber(), 1.0f);
            }
            else if (fxi->fixtureMode() != nullptr)
            {
                // No master dimmer: dim by scaling THIS head's colour channels.
                for (quint32 c : headChannels)
                {
                    QLCChannel *ch = fxi->fixtureMode()->channel(c);
                    if (ch == nullptr) continue;
                    // Only channels that actually EMIT a colour can stand in for a
                    // dimmer. A colour-WHEEL channel is in the Colour group but its
                    // value is a wheel POSITION (colour() == NoColour) — scaling it
                    // by the dimmer multiplier does not dim anything, it spins the
                    // wheel to an arbitrary slot at flicker rate.
                    const bool isEmitter =
                        (ch->group() == QLCChannel::Intensity ||
                         ch->group() == QLCChannel::Colour) &&
                        ch->colour() != QLCChannel::NoColour;
                    if (!isEmitter) continue;
                    // Default to full so a fixture with no Colour look still
                    // flickers (user choice) rather than staying dark.
                    const uchar base = sceneBaseValue(fid, c, 255);
                    const uchar out = (uchar)qBound(0, (int)qRound(base * mul), 255);
                    writes.append({uniIdx, (int)fxi->address() + (int)c,
                                   out, fxi->id(), c, /*replace=*/true, mul, base});
                }
            }
        }

        // RGB colour channels (additive mixing fixtures)
        int colourWritesBefore = writes.size();
        // Respect the look's master Dimmer on colour output, but ONLY for fixtures
        // with no master intensity channel — dimmered fixtures already carry the
        // look Dimmer on that channel (via the scene base), so scaling colour too
        // would double-dim them.
        const float lookColorMul =
            (fxi->masterIntensityChannel() == QLCChannel::invalid()) ? m_lookDimmer : 1.0f;
        // Write DMX @value onto every channel carrying primary @colour.
        // Returns true if the fixture actually has such an emitter.
        // An intent with a colour in it OWNS its head's colour: written over
        // the look's static colour, not merged highest-wins with it (cyan
        // under a rainbow turned every hue pastel), zeros and all. One that's
        // all black writes nothing and leaves the head to the look.
        // Only while it's on: an effect whose look has stopped (envelope 0)
        // must not hold its heads black over whatever plays there next.
        const bool ownsColour = carriesColour && envelope > 0.0f
            && (iv("r").toInt() > 0 || iv("g").toInt() > 0 || iv("b").toInt() > 0 || iv("w").toInt() > 0);
        auto writeEmitter = [&](QLCChannel::PrimaryColour colour, uchar dmxValRaw) -> bool {
            // Scale colour output by the scene's actuation envelope (so the effect
            // ramps in/holds with the scene) and by the look's master Dimmer.
            uchar dmxVal = (uchar)qBound(0, (int)qRound(dmxValRaw * envelope * lookColorMul * ownColourMul), 255);
            bool matched = false;
            // Only THIS head's channels — so a pixel effect colours one pixel, not
            // the whole (multi-head) fixture.
            for (quint32 c : headChannels)
            {
                if (!fxi->fixtureMode()) break;
                QLCChannel *ch = fxi->fixtureMode()->channel(c);
                if (ch == nullptr) continue;
                // Accept both Colour-group and Intensity-group (IntensityRed/Green/Blue
                // presets) channels — the fixture definition preset determines which
                // group they land in, but both carry a meaningful colour value.
                bool isColourCh = (ch->colour() == colour) &&
                                  (ch->group() == QLCChannel::Colour ||
                                   ch->group() == QLCChannel::Intensity);
                if (isColourCh)
                {
                    // Skip zero-value colour writes: they're HTP-merged (replace=false)
                    // so max(base,0) leaves the base untouched — writing them just
                    // churns the fader and, more importantly, keeps an idle frame
                    // looking "busy". Dropping them lets a note effect with nothing
                    // lit produce an EMPTY frame, which the runner uses to idle.
                    if (ownsColour)
                        writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal, fxi->id(), c,
                                       /*replace=*/true});
                    else if (dmxVal != 0)
                        writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal, fxi->id(), c});
                    matched = true;   // the emitter exists regardless of this value
                }
            }
            return matched;
        };
        auto writeColour = [&](const char *prop, QLCChannel::PrimaryColour colour) -> bool {
            QJSValue v = iv(prop);
            if (v.isUndefined()) return true; // nothing requested — not a "miss"
            uchar dmxVal = (uchar)qBound(0, v.toInt(), 255);
            return writeEmitter(colour, dmxVal);
        };
        // Map an emitter intent the fixture lacks onto its RGB(W) channels,
        // approximating the missing emitter's hue (per-primary scale 0..1).
        // Also drives White (wMix): RGB intensity is HTP, so if the look's base
        // colour already saturates R/G the boost would be masked — White usually
        // has headroom, so it carries the visible pulse while R/G add the hue.
        auto writeAsRGB = [&](const char *prop,
                              float rMix, float gMix, float bMix, float wMix) {
            QJSValue v = iv(prop);
            if (v.isUndefined()) return;
            uchar amount = (uchar)qBound(0, v.toInt(), 255);
            writeEmitter(QLCChannel::Red,   (uchar)qBound(0, (int)qRound(amount * rMix), 255));
            writeEmitter(QLCChannel::Green, (uchar)qBound(0, (int)qRound(amount * gMix), 255));
            writeEmitter(QLCChannel::Blue,  (uchar)qBound(0, (int)qRound(amount * bMix), 255));
            writeEmitter(QLCChannel::White, (uchar)qBound(0, (int)qRound(amount * wMix), 255));
        };
        writeColour("r",  QLCChannel::Red);
        writeColour("g",  QLCChannel::Green);
        writeColour("b",  QLCChannel::Blue);
        // White/Amber/UV: use the discrete emitter if present, otherwise adapt
        // the intent onto RGB(W) so it still works on fixtures that lack it.
        if (!writeColour("w", QLCChannel::White))
            writeAsRGB("w", 1.0f, 1.0f, 1.0f, 0.0f);
        // A colour that says nothing of white means none: the look's white
        // mustn't wash it out on an RGBW head.
        if (ownsColour && iv("w").isUndefined())
            writeEmitter(QLCChannel::White, 0);
        if (!writeColour("a", QLCChannel::Amber))
        {
            // Fixture has no discrete amber emitter.  A colour-only warmth pulse
            // (white/RGB) is HTP-merged, so against a lit scene it can only push
            // channels UP a little and never dim — far too subtle to read as a
            // candle.  Instead express the amber pulse as a BRIGHTNESS flicker on
            // the master dimmer, written with REPLACE semantics so it drives the
            // full range (incl. downward) and the scene's warm colour flickers.
            QJSValue av = iv("a");
            quint32 miCh = fxi->masterIntensityChannel();
            if (!av.isUndefined() && miCh != QLCChannel::invalid())
            {
                uchar amt = (uchar)qBound(0, (int)qRound(av.toInt() * envelope), 255);
                writes.append({uniIdx, (int)fxi->address() + (int)miCh,
                               amt, fxi->id(), miCh, /*replace=*/true});
            }
            else
            {
                // No master dimmer either — fall back to the additive colour mix.
                writeAsRGB("a", 1.0f, 0.494f, 0.0f, 1.0f);
            }
        }
        writeColour("uv", QLCChannel::UV);             // no meaningful RGB approximation

        // Color-wheel fallback: if r/g/b intent present but no additive RGB found,
        // find the closest color-wheel slot and write it.
        bool hasColorIntent = !iv("r").isUndefined()
                           || !iv("g").isUndefined()
                           || !iv("b").isUndefined();
        if (hasColorIntent && writes.size() == colourWritesBefore)
        {
            int tR = qBound(0, iv("r").toInt(), 255);
            int tG = qBound(0, iv("g").toInt(), 255);
            int tB = qBound(0, iv("b").toInt(), 255);
            quint32 nCh = fxi->channels();
            for (quint32 c = 0; c < nCh; ++c)
            {
                if (!fxi->fixtureMode()) break;
                QLCChannel *ch = fxi->fixtureMode()->channel(c);
                if (!ch || ch->group() != QLCChannel::Colour) continue;

                // Find capability with closest colour
                uchar bestDmx = 0;
                int bestDist = INT_MAX;
                const QList<QLCCapability*> caps = ch->capabilities();
                for (QLCCapability *cap : caps)
                {
                    QColor capColor = cap->resource(0).value<QColor>();
                    if (!capColor.isValid())
                        capColor = QColor(cap->name().trimmed());
                    if (!capColor.isValid())
                        continue;
                    int dr = capColor.red()   - tR;
                    int dg = capColor.green()  - tG;
                    int db = capColor.blue()   - tB;
                    int dist = dr*dr + dg*dg + db*db;
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestDmx  = cap->middle();
                    }
                }
                if (bestDist < INT_MAX)
                    writes.append({uniIdx, (int)fxi->address() + (int)c, bestDmx, fxi->id(), c});
            }
        }

        // Shutter channel: "shutterOpen" (0=closed, 1=open) or
        // "strobeRate" (0..1 normalized within the fixture's strobe capability range).
        QJSValue shutterOpenVal = iv("shutterOpen");
        QJSValue strobeRateVal  = iv("strobeRate");
        if (!shutterOpenVal.isUndefined() || !strobeRateVal.isUndefined())
        {
            quint32 nCh = fxi->channels();
            for (quint32 c = 0; c < nCh; ++c)
            {
                if (!fxi->fixtureMode()) break;
                QLCChannel *ch = fxi->fixtureMode()->channel(c);
                if (!ch || ch->group() != QLCChannel::Shutter)
                    continue;

                uchar dmxVal = 0;
                bool found   = false;

                if (!strobeRateVal.isUndefined())
                {
                    // Map 0..1 rate onto the first ShutterStrobeSlowFast (or
                    // ShutterStrobeFastSlow) capability range we find.
                    double rate = qBound(0.0, strobeRateVal.toNumber(), 1.0);
                    for (QLCCapability *cap : ch->capabilities())
                    {
                        if (cap->preset() == QLCCapability::StrobeSlowToFast)
                        {
                            dmxVal = (uchar)(cap->min() + rate * (cap->max() - cap->min()));
                            found  = true; break;
                        }
                        if (cap->preset() == QLCCapability::StrobeFastToSlow)
                        {
                            dmxVal = (uchar)(cap->max() - rate * (cap->max() - cap->min()));
                            found  = true; break;
                        }
                    }
                }
                else
                {
                    // shutterOpen: 1 = open, 0 = closed — find the matching
                    // capability by QLCCapability preset type.
                    bool wantOpen = shutterOpenVal.toNumber() > 0.5;
                    QLCCapability::Preset want = wantOpen
                        ? QLCCapability::ShutterOpen : QLCCapability::ShutterClose;
                    for (QLCCapability *cap : ch->capabilities())
                    {
                        if (cap->preset() == want)
                        {
                            dmxVal = cap->middle();
                            found  = true; break;
                        }
                    }
                    // Fallback: name-based search if presets aren't tagged.
                    if (!found)
                    {
                        QString target = wantOpen ? "open" : "clos";
                        for (QLCCapability *cap : ch->capabilities())
                        {
                            if (cap->name().toLower().contains(target))
                            {
                                dmxVal = cap->middle();
                                found  = true; break;
                            }
                        }
                    }
                }

                if (found)
                    writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal, fxi->id(), c});
            }
        }

        // Beam, iris, zoom, gobo intents.
        //   focus  0..1  — BeamFocusNearFar (or FarNear inverted)
        //   frost  0..1  — Beam-group Custom channel named "frost"
        //   zoom   0..1  — BeamZoomSmallBig (or BigSmall inverted)
        //   iris   0..1  — ShutterIrisMinToMax (or MaxToMin inverted); in Shutter group
        //   gobo   int   — 0-based index into the GoboWheel's non-shake capabilities
        QJSValue focusV = iv("focus");
        QJSValue frostV = iv("frost");
        QJSValue zoomV  = iv("zoom");
        QJSValue irisV  = iv("iris");
        QJSValue goboV  = iv("gobo");

        bool hasBeamIntent = !focusV.isUndefined() || !frostV.isUndefined()
                          || !zoomV.isUndefined()  || !irisV.isUndefined()
                          || !goboV.isUndefined();
        if (hasBeamIntent)
        {
            quint32 nCh = fxi->channels();
            for (quint32 c = 0; c < nCh; ++c)
            {
                if (!fxi->fixtureMode()) break;
                QLCChannel *ch = fxi->fixtureMode()->channel(c);
                if (!ch) continue;

                QLCChannel::Group   grp = ch->group();
                QLCChannel::Preset  pst = ch->preset();

                if (grp == QLCChannel::Beam)
                {
                    if (!focusV.isUndefined())
                    {
                        uchar v = (uchar)qBound(0, (int)(focusV.toNumber() * 255), 255);
                        if (pst == QLCChannel::BeamFocusNearFar)
                            writes.append({uniIdx, (int)fxi->address()+(int)c, v, fxi->id(), c});
                        else if (pst == QLCChannel::BeamFocusFarNear)
                            writes.append({uniIdx, (int)fxi->address()+(int)c, (uchar)(255-v), fxi->id(), c});
                    }
                    if (!frostV.isUndefined() && pst == QLCChannel::Custom
                        && ch->name().contains("frost", Qt::CaseInsensitive))
                    {
                        uchar v = (uchar)qBound(0, (int)(frostV.toNumber() * 255), 255);
                        writes.append({uniIdx, (int)fxi->address()+(int)c, v, fxi->id(), c});
                    }
                    if (!zoomV.isUndefined())
                    {
                        uchar v = (uchar)qBound(0, (int)(zoomV.toNumber() * 255), 255);
                        if (pst == QLCChannel::BeamZoomSmallBig)
                            writes.append({uniIdx, (int)fxi->address()+(int)c, v, fxi->id(), c});
                        else if (pst == QLCChannel::BeamZoomBigSmall)
                            writes.append({uniIdx, (int)fxi->address()+(int)c, (uchar)(255-v), fxi->id(), c});
                    }
                }
                else if (grp == QLCChannel::Shutter && !irisV.isUndefined())
                {
                    uchar v = (uchar)qBound(0, (int)(irisV.toNumber() * 255), 255);
                    if (pst == QLCChannel::ShutterIrisMinToMax)
                        writes.append({uniIdx, (int)fxi->address()+(int)c, v, fxi->id(), c});
                    else if (pst == QLCChannel::ShutterIrisMaxToMin)
                        writes.append({uniIdx, (int)fxi->address()+(int)c, (uchar)(255-v), fxi->id(), c});
                }
                else if (grp == QLCChannel::Gobo && !goboV.isUndefined()
                         && pst == QLCChannel::GoboWheel)
                {
                    int wantIdx = qBound(0, (int)goboV.toNumber(), 255);
                    int gIdx    = 0;
                    for (QLCCapability *cap : ch->capabilities())
                    {
                        // Skip shake/macro entries; only count selectable slots.
                        if (cap->preset() == QLCCapability::GoboShakeMacro
                            || cap->preset() == QLCCapability::GoboMacro)
                            continue;
                        if (gIdx == wantIdx)
                        {
                            writes.append({uniIdx, (int)fxi->address()+(int)c, cap->middle(), fxi->id(), c});
                            break;
                        }
                        ++gIdx;
                    }
                }
            }
        }
    }

    return writes;
}
