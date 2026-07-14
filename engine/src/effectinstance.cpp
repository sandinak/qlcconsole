/*
  Q Light Controller Plus
  effectinstance.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectinstance.h"
#include "aimsolver.h"

#include "doc.h"
#include "scene.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "qlcchannel.h"
#include "qlccapability.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "scenevalue.h"
#include "universe.h"

#include "mastertimer.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QDebug>
#include <QtMath>
#include <cmath>

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

void EffectInstance::setDataChannels(const QHash<QString, QVariantMap> &channels)
{
    m_dataChannels = channels;
}

void EffectInstance::runTick()
{
    if (!m_script.isValid())
        return;

    // Collect the fixture ids the scene targets, filtered by the script's
    // declared fixtureTypes (e.g. ["rgb"] excludes movers, ["moving"] excludes
    // wash fixtures).  If the script declares no types, all scene fixtures are used.
    QList<quint32> fxIds = effectiveFixtureIds();
    if (fxIds.isEmpty())
        return;

    // Actuation envelope: scale intensity/colour output by the host scene's
    // fade-in ramp so the effect fires *with* the scene (ramps up as the scene
    // fades in, then holds) instead of blasting at full independent of it.
    m_envelope = sceneEnvelope();

    // Snapshot the scene's base per-channel values so an intensity flicker can
    // modulate *around* the Dimmer/Colour looks instead of overriding them.
    buildSceneBaseValues();

    QJSValue fixtures = buildFixturesArray(fxIds);
    QJSValue inputs   = buildInputsObject();
    QJSValue palettes = buildPalettesObject();
    QJSValue params   = buildParamsObject();
    QJSValue data     = buildDataChannelsObject();

    QJSValue result = m_script.callTick(fixtures, inputs, palettes, params, m_state, data);
    if (!result.isArray())
        return;

    QList<DmxWrite> writes = parseIntents(result, fxIds);

    QMutexLocker locker(&m_mutex);
    m_lastResults = writes;
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
    for (quint32 pid : scene->palettes())
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == nullptr || p->type() == QLCPalette::Effect)
            continue;
        for (const SceneValue &sv : p->valuesFromFixtureGroups(m_doc, groups))
            m_sceneBaseValues[key(sv.fxi, sv.channel)] = sv.value;
        for (const SceneValue &sv : p->valuesFromFixtures(m_doc, fixtures))
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

QList<quint32> EffectInstance::effectiveFixtureIds() const
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

    // Fallback: if the scene has no explicit targets, include all fixture groups
    // in the doc so an effect palette with no baked values still runs.
    if (fxIds.isEmpty())
    {
        for (FixtureGroup *grp : m_doc->fixtureGroups())
            if (grp)
                for (quint32 fid : grp->fixtureList())
                    if (!fxIds.contains(fid))
                        fxIds.append(fid);
    }

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

QJSValue EffectInstance::buildFixturesArray(const QList<quint32> &fxIds)
{
    QJSValue arr = m_script.engine()->newArray(fxIds.size());
    const MonitorProperties *mp = m_doc->monitorProperties();
    QLCPalette *effectPal = m_doc->palette(m_effectPaletteId);

    for (int i = 0; i < fxIds.size(); ++i)
    {
        quint32 fid = fxIds.at(i);
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi)
            continue;

        QJSValue obj = m_script.engine()->newObject();
        obj.setProperty("id",   (int)fid);
        obj.setProperty("head", 0);

        float panRange  = 360.0f;
        float tiltRange = 270.0f;
        bool  hasPT     = false;
        bool  hasRGB    = false;
        bool  hasColorWheel = false;
        bool  hasDimmer = false;
        bool  hasGobo   = false;
        bool  hasShutt  = false;

        if (fxi->fixtureMode())
        {
            const QLCPhysical &phy = fxi->fixtureMode()->physical();
            if (phy.focusPanMax() > 0)  panRange  = (float)phy.focusPanMax();
            if (phy.focusTiltMax() > 0) tiltRange = (float)phy.focusTiltMax();
        }

        bool hasR = false, hasG = false, hasB = false;
        quint32 nCh = fxi->channels();
        for (quint32 c = 0; c < nCh; ++c)
        {
            if (!fxi->fixtureMode()) break;
            QLCChannel *ch = fxi->fixtureMode()->channel(c);
            if (!ch) continue;
            switch (ch->group())
            {
                case QLCChannel::Pan:
                case QLCChannel::Tilt:
                    hasPT = true; break;
                case QLCChannel::Colour:
                    if (ch->colour() == QLCChannel::Red)        hasR = true;
                    else if (ch->colour() == QLCChannel::Green)  hasG = true;
                    else if (ch->colour() == QLCChannel::Blue)   hasB = true;
                    else hasColorWheel = true;
                    break;
                case QLCChannel::Intensity:
                    // IntensityRed/Green/Blue presets land in the Intensity group
                    // but carry a colour value — treat them as additive RGB channels.
                    if (ch->colour() == QLCChannel::Red)        hasR = true;
                    else if (ch->colour() == QLCChannel::Green)  hasG = true;
                    else if (ch->colour() == QLCChannel::Blue)   hasB = true;
                    else hasDimmer = true;
                    break;
                case QLCChannel::Gobo:
                    hasGobo = true; break;
                case QLCChannel::Shutter:
                    hasShutt = true; break;
                default: break;
            }
        }
        hasRGB = hasR && hasG && hasB;

        obj.setProperty("panRange",      panRange);
        obj.setProperty("tiltRange",     tiltRange);
        obj.setProperty("hasPanTilt",    hasPT);
        obj.setProperty("hasRGB",        hasRGB);
        obj.setProperty("hasColorWheel", hasColorWheel);
        obj.setProperty("hasDimmer",     hasDimmer);
        obj.setProperty("hasGobo",       hasGobo);
        obj.setProperty("hasShutter",    hasShutt);

        // 3D rig position from MonitorProperties (0,0,0 if not set)
        QVector3D pos(0, 0, 0);
        if (mp)
            pos = mp->fixtureRigPosition(fid);

        QJSValue posObj = m_script.engine()->newObject();
        posObj.setProperty("x", (double)pos.x());
        posObj.setProperty("y", (double)pos.y());
        posObj.setProperty("z", (double)pos.z());
        obj.setProperty("pos", posObj);

        // aimAt: pre-computed pan/tilt for each bound target slot
        QJSValue aimAt = m_script.engine()->newObject();
        if (mp && effectPal)
        {
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
                // configured Z — subject height above the platform/deck.
                QVector3D tgtPos = tgt->position();
                tgtPos.setZ(mp->platformHeightAt(tgtPos.x(), tgtPos.y()) + mp->aimSubjectHeight());

                // Use the SHARED aim solver — the exact same geometry the Aim
                // palette uses in Edit — so the beam doesn't jump on Edit→Run.
                // aimAt.pan/tilt are fixture degrees; the host converts via
                // positionToValues (see parseIntents).
                float panDeg = 0.0f, tiltDeg = 0.0f;
                QJSValue aimObj = m_script.engine()->newObject();
                if (AimSolver::aimDegrees(m_doc, fid, tgtPos, panDeg, tiltDeg))
                {
                    aimObj.setProperty("pan",  (double)panDeg);
                    aimObj.setProperty("tilt", (double)tiltDeg);
                }
                aimAt.setProperty(td.name, aimObj);
            }
        }
        obj.setProperty("aimAt", aimAt);

        // sceneTarget: the per-fixture aim toward the SCENE's own Aim look (not a
        // manual binding).  The followspot reads its target IMPLICITLY from the
        // scene this way, so it needs no followTarget slot in its menu.  Present
        // only when the scene actually carries an Aim look; the script defers to
        // the host's converged aim whenever it is present.
        if (mp)
        {
            Scene *sc = qobject_cast<Scene*>(m_doc->function(m_sceneId));
            if (sc)
            {
                for (quint32 spid : sc->palettes())
                {
                    QLCPalette *ap = m_doc->palette(spid);
                    if (ap == NULL || ap->type() != QLCPalette::Aim)
                        continue;
                    StageTarget *st = mp->stageTarget(ap->stageTargetId());
                    if (st == NULL)
                        continue;
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
                    QVector3D tp = raw;
                    tp.setZ(mp->platformHeightAt(tp.x(), tp.y()) + mp->aimSubjectHeight());
                    float pd = 0.0f, td = 0.0f;
                    if (AimSolver::aimDegrees(m_doc, fid, tp, pd, td))
                    {
                        so.setProperty("pan",  double(pd));
                        so.setProperty("tilt", double(td));
                    }
                    obj.setProperty("sceneTarget", so);
                    break; // first Aim look wins
                }
            }
        }

        // lastSpot: pan/tilt (degrees) where this fixture's beam was left by the
        // previous Operate-mode tick (possibly from a different scene).  Lets a
        // followspot script seed followMode = "lastPosition" so the beam holds
        // across scene transitions instead of snapping.  Property is absent when
        // no prior position is known (e.g. first use after launch).
        if (m_lastSpotIn.contains(fid))
        {
            const QPointF ls = m_lastSpotIn.value(fid);
            QJSValue lastObj = m_script.engine()->newObject();
            lastObj.setProperty("pan",  ls.x());
            lastObj.setProperty("tilt", ls.y());
            obj.setProperty("lastSpot", lastObj);
        }

        arr.setProperty(i, obj);
    }
    return arr;
}

QJSValue EffectInstance::buildInputsObject() const
{
    QJSValue obj = m_script.engine()->newObject();
    for (auto it = m_inputValues.constBegin(); it != m_inputValues.constEnd(); ++it)
        obj.setProperty(it.key(), (double)it.value());

    const double elapsedMs = (double)m_elapsed.elapsed();
    obj.setProperty("_time", elapsedMs / 1000.0);

    // Beat/tempo inputs from MasterTimer so scripts can lock to music tempo.
    //   _beat      — 0.0→1.0 sawtooth phase within the current beat period
    //   _bpm       — current BPM (0 when no beat source is active)
    //   _beatCount — integer count of beats since this instance started
    MasterTimer *mt = m_doc->masterTimer();
    const int beatDurationMs = mt ? mt->beatTimeDuration() : 0;
    const int bpm            = mt ? mt->bpmNumber()        : 0;
    if (beatDurationMs > 0)
    {
        const double beatPhase = std::fmod(elapsedMs, (double)beatDurationMs)
                                 / (double)beatDurationMs;
        const int beatCount    = (int)(elapsedMs / (double)beatDurationMs);
        obj.setProperty("_bpm",       (double)bpm);
        obj.setProperty("_beat",      beatPhase);
        obj.setProperty("_beatCount", beatCount);
    }
    else
    {
        obj.setProperty("_bpm",       0.0);
        obj.setProperty("_beat",      0.0);
        obj.setProperty("_beatCount", 0);
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
    QList<QColor> lookColors;
    double lookDimmer = 1.0;
    bool   haveLookDimmer = false;
    if (Scene *scene = qobject_cast<Scene *>(m_doc->function(m_sceneId)))
    {
        foreach (quint32 pid, scene->palettes())
        {
            QLCPalette *p = m_doc->palette(pid);
            if (!p)
                continue;
            if (p->type() == QLCPalette::Color)
                lookColors.append(p->colorValue());
            else if (p->type() == QLCPalette::Dimmer)
            {
                lookDimmer = p->value().toDouble() / 255.0;
                haveLookDimmer = true;
            }
        }
    }

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
                              const QList<quint32> &fxIds) const
{
    QList<DmxWrite> writes;
    m_lastIntentDeg.clear();

    int len = qMin(intents.property("length").toInt(), fxIds.size());
    for (int i = 0; i < len; ++i)
    {
        QJSValue intent = intents.property(i);
        if (!intent.isObject())
            continue;

        quint32 fid = fxIds.at(i);
        Fixture *fxi = m_doc->fixture(fid);
        if (!fxi || !fxi->fixtureMode())
            continue;

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

        QJSValue axVal = intent.property("aimX");
        QJSValue ayVal = intent.property("aimY");
        if (!axVal.isUndefined() && !ayVal.isUndefined())
        {
            float az = intent.property("aimZ").isUndefined()
                       ? 0.0f : (float)intent.property("aimZ").toNumber();
            QVector3D world((float)axVal.toNumber(), (float)ayVal.toNumber(), az);
            float pd = 0.0f, td = 0.0f;
            if (AimSolver::aimDegrees(m_doc, fid, world, pd, td))
            {
                panDeg = pd; tiltDeg = td;
                havePan = true; haveTilt = true;
            }
        }

        QJSValue panVal = intent.property("pan");
        if (!panVal.isUndefined())  { panDeg  = (float)panVal.toNumber();  havePan  = true; }
        QJSValue tiltVal = intent.property("tilt");
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
        QJSValue dimVal = intent.property("dimmer");
        if (!dimVal.isUndefined())
        {
            const float mul = qBound(0.0f, (float)dimVal.toNumber(), 1.0f) * m_envelope;
            quint32 miCh = fxi->masterIntensityChannel();
            if (miCh != QLCChannel::invalid())
            {
                // Base from the scene's Dimmer look; full if the scene sets none.
                const uchar base = sceneBaseValue(fid, miCh, 255);
                const uchar out  = (uchar)qBound(0, (int)qRound(base * mul), 255);
                writes.append({uniIdx, (int)fxi->address() + (int)miCh,
                               out, fxi->id(), miCh, /*replace=*/true});
            }
            else if (fxi->fixtureMode() != nullptr)
            {
                // No master dimmer: dim the fixture by scaling its colour channels.
                for (quint32 c = 0; c < fxi->channels(); ++c)
                {
                    QLCChannel *ch = fxi->fixtureMode()->channel(c);
                    if (ch == nullptr) continue;
                    const bool isColour =
                        (ch->group() == QLCChannel::Intensity &&
                         ch->colour() != QLCChannel::NoColour) ||
                        ch->group() == QLCChannel::Colour;
                    if (!isColour) continue;
                    // Default to full so a fixture with no Colour look still
                    // flickers (user choice) rather than staying dark.
                    const uchar base = sceneBaseValue(fid, c, 255);
                    const uchar out = (uchar)qBound(0, (int)qRound(base * mul), 255);
                    writes.append({uniIdx, (int)fxi->address() + (int)c,
                                   out, fxi->id(), c, /*replace=*/true});
                }
            }
        }

        // RGB colour channels (additive mixing fixtures)
        int colourWritesBefore = writes.size();
        // Write DMX @value onto every channel carrying primary @colour.
        // Returns true if the fixture actually has such an emitter.
        auto writeEmitter = [&](QLCChannel::PrimaryColour colour, uchar dmxValRaw) -> bool {
            // Scale colour output by the scene's actuation envelope so the effect
            // ramps in/holds with the scene instead of firing independently.
            uchar dmxVal = (uchar)qBound(0, (int)qRound(dmxValRaw * m_envelope), 255);
            bool matched = false;
            quint32 nCh = fxi->channels();
            for (quint32 c = 0; c < nCh; ++c)
            {
                if (!fxi->fixtureMode()) break;
                QLCChannel *ch = fxi->fixtureMode()->channel(c);
                // Accept both Colour-group and Intensity-group (IntensityRed/Green/Blue
                // presets) channels — the fixture definition preset determines which
                // group they land in, but both carry a meaningful colour value.
                bool isColourCh = (ch->colour() == colour) &&
                                  (ch->group() == QLCChannel::Colour ||
                                   ch->group() == QLCChannel::Intensity);
                if (isColourCh)
                {
                    writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal, fxi->id(), c});
                    matched = true;
                }
            }
            return matched;
        };
        auto writeColour = [&](const QString &prop, QLCChannel::PrimaryColour colour) -> bool {
            QJSValue v = intent.property(prop);
            if (v.isUndefined()) return true; // nothing requested — not a "miss"
            uchar dmxVal = (uchar)qBound(0, v.toInt(), 255);
            return writeEmitter(colour, dmxVal);
        };
        // Map an emitter intent the fixture lacks onto its RGB(W) channels,
        // approximating the missing emitter's hue (per-primary scale 0..1).
        // Also drives White (wMix): RGB intensity is HTP, so if the look's base
        // colour already saturates R/G the boost would be masked — White usually
        // has headroom, so it carries the visible pulse while R/G add the hue.
        auto writeAsRGB = [&](const QString &prop,
                              float rMix, float gMix, float bMix, float wMix) {
            QJSValue v = intent.property(prop);
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
        if (!writeColour("a", QLCChannel::Amber))
        {
            // Fixture has no discrete amber emitter.  A colour-only warmth pulse
            // (white/RGB) is HTP-merged, so against a lit scene it can only push
            // channels UP a little and never dim — far too subtle to read as a
            // candle.  Instead express the amber pulse as a BRIGHTNESS flicker on
            // the master dimmer, written with REPLACE semantics so it drives the
            // full range (incl. downward) and the scene's warm colour flickers.
            QJSValue av = intent.property("a");
            quint32 miCh = fxi->masterIntensityChannel();
            if (!av.isUndefined() && miCh != QLCChannel::invalid())
            {
                uchar amt = (uchar)qBound(0, (int)qRound(av.toInt() * m_envelope), 255);
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
        bool hasColorIntent = !intent.property("r").isUndefined()
                           || !intent.property("g").isUndefined()
                           || !intent.property("b").isUndefined();
        if (hasColorIntent && writes.size() == colourWritesBefore)
        {
            int tR = qBound(0, intent.property("r").toInt(), 255);
            int tG = qBound(0, intent.property("g").toInt(), 255);
            int tB = qBound(0, intent.property("b").toInt(), 255);
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
        QJSValue shutterOpenVal = intent.property("shutterOpen");
        QJSValue strobeRateVal  = intent.property("strobeRate");
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
        QJSValue focusV = intent.property("focus");
        QJSValue frostV = intent.property("frost");
        QJSValue zoomV  = intent.property("zoom");
        QJSValue irisV  = intent.property("iris");
        QJSValue goboV  = intent.property("gobo");

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
