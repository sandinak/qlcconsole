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
        bool hasShutt = false, hasColorWheel = false;
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
                    else
                        hasDimmer = true;
                    break;
                case QLCChannel::Colour:
                    if (ch->colour() == QLCChannel::Red   ||
                        ch->colour() == QLCChannel::Green ||
                        ch->colour() == QLCChannel::Blue)
                        hasRGB = true;
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
            if ((tl == QLatin1String("rgb")     ||
                 tl == QLatin1String("rgbw")    ||
                 tl == QLatin1String("color"))  && hasRGB)       { matches = true; break; }
            if ((tl == QLatin1String("moving")  ||
                 tl == QLatin1String("mover")   ||
                 tl == QLatin1String("pantilt") ||
                 tl == QLatin1String("movers")) && hasPT)         { matches = true; break; }
            if (tl == QLatin1String("dimmer")   && hasDimmer)     { matches = true; break; }
            if (tl == QLatin1String("shutter")  && hasShutt)      { matches = true; break; }
            if (tl == QLatin1String("colorwheel") && hasColorWheel){ matches = true; break; }
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

QJSValue EffectInstance::buildPalettesObject() const
{
    QJSValue obj = m_script.engine()->newObject();

    QLCPalette *effectPal = m_doc->palette(m_effectPaletteId);
    if (!effectPal)
        return obj;

    const QMap<QString, quint32> &bindings = effectPal->effectPaletteBindings();
    for (const EffectScript::PaletteDef &def : m_script.paletteDefs())
    {
        quint32 pid = bindings.value(def.name, QLCPalette::invalidId());
        QLCPalette *pal = m_doc->palette(pid);
        if (!pal)
        {
            obj.setProperty(def.name, m_script.engine()->newObject()); // null-ish
            continue;
        }

        QJSValue pObj = m_script.engine()->newObject();
        // Expose palette values as a flat object (type-dependent)
        if (pal->type() == QLCPalette::Color)
        {
            QColor c = pal->colorValue();
            pObj.setProperty("r", c.red());
            pObj.setProperty("g", c.green());
            pObj.setProperty("b", c.blue());
        }
        else if (pal->type() == QLCPalette::Dimmer)
        {
            pObj.setProperty("dimmer", pal->value().toDouble() / 255.0);
        }
        obj.setProperty(def.name, pObj);
    }
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

        // Pan
        QJSValue panVal = intent.property("pan");
        if (!panVal.isUndefined())
        {
            float deg = (float)panVal.toNumber();
            spotDeg.setX(deg); spotPan = true;
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Pan, deg);
            for (const SceneValue &sv : svs)
                writes.append({uniIdx, (int)fxi->address() + (int)sv.channel, sv.value, fxi->id(), sv.channel});
        }

        // Tilt
        QJSValue tiltVal = intent.property("tilt");
        if (!tiltVal.isUndefined())
        {
            float deg = (float)tiltVal.toNumber();
            spotDeg.setY(deg); spotTilt = true;
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Tilt, deg);
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

        // Dimmer
        QJSValue dimVal = intent.property("dimmer");
        if (!dimVal.isUndefined())
        {
            uchar dmxVal = (uchar)qBound(0, (int)(dimVal.toNumber() * 255), 255);
            quint32 miCh = fxi->masterIntensityChannel();
            if (miCh != QLCChannel::invalid())
                writes.append({uniIdx, (int)fxi->address() + (int)miCh, dmxVal, fxi->id(), miCh});
        }

        // RGB colour channels (additive mixing fixtures)
        int colourWritesBefore = writes.size();
        auto writeColour = [&](const QString &prop, QLCChannel::PrimaryColour colour) {
            QJSValue v = intent.property(prop);
            if (v.isUndefined()) return;
            uchar dmxVal = (uchar)qBound(0, v.toInt(), 255);
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
                    writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal, fxi->id(), c});
            }
        };
        writeColour("r",  QLCChannel::Red);
        writeColour("g",  QLCChannel::Green);
        writeColour("b",  QLCChannel::Blue);
        writeColour("w",  QLCChannel::White);
        writeColour("a",  QLCChannel::Amber);
        writeColour("uv", QLCChannel::UV);

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
    }

    return writes;
}
