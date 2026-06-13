/*
  Q Light Controller Plus
  effectinstance.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectinstance.h"

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
#include "scenevalue.h"
#include "universe.h"

#include <QDebug>
#include <QtMath>

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
        if (!m_paramValues.contains(d.name))
            m_paramValues[d.name] = d.defaultValue;
    }

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

void EffectInstance::runTick()
{
    if (!m_script.isValid())
        return;

    // Collect the fixture ids the scene targets
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (!scene)
        return;

    // Expand groups to fixture ids
    QList<quint32> fxIds;
    for (quint32 gid : scene->fixtureGroups())
    {
        FixtureGroup *grp = m_doc->fixtureGroup(gid);
        if (grp)
            fxIds.append(grp->fixtureList());
    }
    for (quint32 fid : scene->fixtures())
    {
        if (!fxIds.contains(fid))
            fxIds.append(fid);
    }

    // If the scene has no explicit targets, fall back to all fixture groups in
    // the doc so an effect palette with no baked values still runs.
    if (fxIds.isEmpty())
    {
        for (FixtureGroup *grp : m_doc->fixtureGroups())
        {
            if (grp)
                for (quint32 fid : grp->fixtureList())
                    if (!fxIds.contains(fid))
                        fxIds.append(fid);
        }
    }

    if (fxIds.isEmpty())
        return;

    QJSValue fixtures = buildFixturesArray();
    QJSValue inputs   = buildInputsObject();
    QJSValue palettes = buildPalettesObject();
    QJSValue params   = buildParamsObject();

    QJSValue result = m_script.callTick(fixtures, inputs, palettes, params, m_state);
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

QJSValue EffectInstance::buildFixturesArray()
{
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_sceneId));
    if (!scene)
        return m_script.engine()->newArray(0);

    QList<quint32> fxIds;
    for (quint32 gid : scene->fixtureGroups())
    {
        FixtureGroup *grp = m_doc->fixtureGroup(gid);
        if (grp)
            fxIds.append(grp->fixtureList());
    }
    for (quint32 fid : scene->fixtures())
    {
        if (!fxIds.contains(fid))
            fxIds.append(fid);
    }
    if (fxIds.isEmpty())
    {
        for (FixtureGroup *grp : m_doc->fixtureGroups())
            if (grp)
                for (quint32 fid : grp->fixtureList())
                    if (!fxIds.contains(fid))
                        fxIds.append(fid);
    }

    QJSValue arr = m_script.engine()->newArray(fxIds.size());
    const MonitorProperties *mp = m_doc->monitorProperties();

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
                    hasDimmer = true; break;
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

        // 3D position from MonitorProperties (0,0,0 if not set)
        QVector3D pos(0, 0, 0);
        if (mp)
            pos = mp->fixturePosition(fid, 0, 0);

        QJSValue posObj = m_script.engine()->newObject();
        posObj.setProperty("x", pos.x());
        posObj.setProperty("y", pos.y());
        posObj.setProperty("z", pos.z());
        obj.setProperty("pos", posObj);

        arr.setProperty(i, obj);
    }
    return arr;
}

QJSValue EffectInstance::buildInputsObject() const
{
    QJSValue obj = m_script.engine()->newObject();
    for (auto it = m_inputValues.constBegin(); it != m_inputValues.constEnd(); ++it)
        obj.setProperty(it.key(), (double)it.value());
    // Inject elapsed time in seconds
    obj.setProperty("_time", m_elapsed.elapsed() / 1000.0);
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
    for (auto it = m_paramValues.constBegin(); it != m_paramValues.constEnd(); ++it)
        obj.setProperty(it.key(), it.value());
    return obj;
}

QList<EffectInstance::DmxWrite>
EffectInstance::parseIntents(const QJSValue &intents,
                              const QList<quint32> &fxIds) const
{
    QList<DmxWrite> writes;

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

        // Pan
        QJSValue panVal = intent.property("pan");
        if (!panVal.isUndefined())
        {
            float deg = (float)panVal.toNumber();
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Pan, deg);
            for (const SceneValue &sv : svs)
                writes.append({uniIdx, (int)fxi->address() + (int)sv.channel, sv.value});
        }

        // Tilt
        QJSValue tiltVal = intent.property("tilt");
        if (!tiltVal.isUndefined())
        {
            float deg = (float)tiltVal.toNumber();
            const QList<SceneValue> svs = fxi->positionToValues(QLCChannel::Tilt, deg);
            for (const SceneValue &sv : svs)
                writes.append({uniIdx, (int)fxi->address() + (int)sv.channel, sv.value});
        }

        // Dimmer
        QJSValue dimVal = intent.property("dimmer");
        if (!dimVal.isUndefined())
        {
            uchar dmxVal = (uchar)qBound(0, (int)(dimVal.toNumber() * 255), 255);
            quint32 miCh = fxi->masterIntensityChannel();
            if (miCh != QLCChannel::invalid())
                writes.append({uniIdx, (int)fxi->address() + (int)miCh, dmxVal});
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
                if (ch && ch->group() == QLCChannel::Colour && ch->colour() == colour)
                    writes.append({uniIdx, (int)fxi->address() + (int)c, dmxVal});
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
                    writes.append({uniIdx, (int)fxi->address() + (int)c, bestDmx});
            }
        }
    }

    return writes;
}
