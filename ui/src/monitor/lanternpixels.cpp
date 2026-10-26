/*
  Q Light Controller Plus
  lanternpixels.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QJSValue>
#include <cmath>
#include <QTimer>

#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectscript.h"
#include "inputoutputmap.h"
#include "qlcfixturemode.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"
#include "lanternpixels.h"
#include "fixture.h"
#include "doc.h"

namespace
{
/** A WLED fixture's channel values by the WLED definitions' channel names --
 *  -1 for a channel its mode does not have. */
int wledValue(Fixture *fx, const QByteArray &vals, const QString &name)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr)
        return -1;
    for (int i = 0; i < mode->channels().size() && i < vals.size(); ++i)
        if (mode->channels().at(i)->name() == name)
            return int(uchar(vals.at(i)));
    return -1;
}

/** The effect a WLED fixture is set to, by name ("Chase"), from its Effect
 *  channel -- empty when it is not a WLED or not in effect mode. */
QString wledEffect(Fixture *fx, const QByteArray &vals)
{
    const QLCFixtureMode *mode = fx->fixtureMode();
    if (mode == nullptr || fx->fixtureDef() == nullptr
        || fx->fixtureDef()->manufacturer() != QLatin1String("WLED"))
        return QString();
    for (int i = 0; i < mode->channels().size(); ++i)
    {
        const QLCChannel *ch = mode->channels().at(i);
        if (ch->name() != QLatin1String("Effect") || i >= vals.size())
            continue;
        const QLCCapability *cap = ch->searchCapability(uchar(vals.at(i)));
        return cap != nullptr ? cap->name() : QString();
    }
    return QString();
}
}

LanternPixels::LanternPixels(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    m_clock.start();
    /* A script's look moves while nothing in the console does: draw again
       at the rig view's pace while one ran in the last frame. */
    m_animate = new QTimer(this);
    m_animate->setInterval(40);
    connect(m_animate, &QTimer::timeout, this, [this]() {
        if (!m_scriptRan)
        {
            m_animate->stop();
            return;
        }
        m_scriptRan = false;
        emit changed();
    });
}

LanternPixels::~LanternPixels()
{
    unwatchAll();
}

void LanternPixels::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    if (!active)
        unwatchAll();
}

QList<QLCIOPlugin *> LanternPixels::pluginsFor(Fixture *fx) const
{
    QList<QLCIOPlugin *> out;
    InputOutputMap *iom = (m_doc != nullptr) ? m_doc->inputOutputMap() : nullptr;
    if (fx == nullptr || iom == nullptr)
        return out;
    const quint32 universe = fx->universe();
    for (int i = 0; i < iom->outputPatchesCount(universe); ++i)
    {
        OutputPatch *op = iom->outputPatch(universe, i);
        if (op != nullptr && op->plugin() != nullptr && !out.contains(op->plugin()))
            out << op->plugin();
    }
    return out;
}

void LanternPixels::watch(QLCIOPlugin *plugin, quint32 universe)
{
    const QPair<QLCIOPlugin *, quint32> key(plugin, universe);
    if (m_watched.contains(key))
        return;
    m_watched.insert(key);
    plugin->watchTargetPixels(universe, true);
    if (!m_connected.contains(plugin))
    {
        m_connected.insert(plugin);
        /* By name, not &QLCIOPlugin::targetPixelsChanged: each plugin DLL
           builds its own QLCIOPlugin, so on Windows that address is not the
           one the plugin's moc compares with, and nothing would connect. */
        connect(plugin, SIGNAL(targetPixelsChanged(quint32)), this, SIGNAL(changed()));
    }
}

void LanternPixels::unwatchAll()
{
    for (const auto &key : m_watched)
        key.first->watchTargetPixels(key.second, false);
    m_watched.clear();
}

int LanternPixels::deviceLeds(Fixture *fx) const
{
    foreach (QLCIOPlugin *plugin, pluginsFor(fx))
    {
        const int n = plugin->targetPixelCount(fx->universe());
        if (n > 0)
            return n;
    }
    return 0;
}

LanternPixels::Frame LanternPixels::frame(Fixture *fx, const QByteArray &vals, int count,
                                          const QColor &flat, double level, bool output)
{
    Frame f;
    if (fx == nullptr || count <= 0)
        return f;

    /* The device's own picture, when it is showing this look: every LED as
       the device has it, whatever its effect, firmware or palette. */
    if (output && m_active)
    {
        foreach (QLCIOPlugin *plugin, pluginsFor(fx))
        {
            watch(plugin, fx->universe());
            const QVector<quint32> px = plugin->targetPixels(fx->universe());
            if (px.isEmpty())
                continue;
            f.source = Device;
            f.leds.resize(count);
            for (int i = 0; i < count; ++i)
            {
                // A device driving more or fewer than are drawn: spread over them.
                const quint32 c = px.at(int(qint64(i) * px.size() / count));
                f.leds[i] = QColor(int((c >> 16) & 0xFF), int((c >> 8) & 0xFF), int(c & 0xFF));
            }
            return f;
        }
    }

    const QString effect = wledEffect(fx, vals);
    if (!effect.isEmpty() && effect != QLatin1String("Solid") && level > 0.0
        && runScript(fx, vals, effect, count, level, f))
        return f;

    QColor lit = flat;
    lit.setRgbF(float(flat.redF() * level), float(flat.greenF() * level),
                float(flat.blueF() * level));
    f.leds.fill(lit, count);

    if (!effect.isEmpty() && effect != QLatin1String("Solid") && level > 0.0)
        f.tag = tr("WLED: %1").arg(effect);
    return f;
}

bool LanternPixels::runScript(Fixture *fx, const QByteArray &vals, const QString &effect,
                              int count, double level, Frame &f)
{
    EffectScriptRunner *runner = m_doc->effectScriptRunner();
    if (runner == nullptr)
        return false;
    const QString path = runner->cache()->scriptPathForWled(effect);
    if (path.isEmpty())
        return false;

    QSharedPointer<EffectScript> script = m_scripts.value(path);
    if (!m_scripts.contains(path))
    {
        script.reset(new EffectScript);
        if (!script->load(path))
            script.clear();
        m_scripts.insert(path, script);         // a broken one is not retried every frame
    }
    if (script.isNull())
        return false;

    QJSEngine *eng = script->engine();
    const quint32 fid = fx->id();
    if (m_stateScript.value(fid) != path)
    {
        m_states.insert(fid, script->newState());   // a new effect starts fresh
        m_stateScript.insert(fid, path);
    }

    // The LEDs as a string of fixtures, the first first.
    QJSValue fixtures = eng->newArray(uint(count));
    for (int i = 0; i < count; ++i)
    {
        QJSValue o = eng->newObject();
        o.setProperty(QStringLiteral("id"), int(fid));
        o.setProperty(QStringLiteral("head"), i);
        o.setProperty(QStringLiteral("hasRGB"), true);
        QJSValue grid = eng->newObject();
        grid.setProperty(QStringLiteral("col"), i);
        grid.setProperty(QStringLiteral("row"), 0);
        grid.setProperty(QStringLiteral("cols"), count);
        grid.setProperty(QStringLiteral("rows"), 1);
        o.setProperty(QStringLiteral("grid"), grid);
        fixtures.setProperty(uint(i), o);
    }
    QJSValue inputs = eng->newObject();
    inputs.setProperty(QStringLiteral("_time"), double(m_clock.elapsed()) / 1000.0);

    // Its three colours, as the device is sent them.
    static const char *const colourNames[3][3] = {
        { "Red", "Green", "Blue" },
        { "Secondary Red", "Secondary Green", "Secondary Blue" },
        { "Tertiary Red", "Tertiary Green", "Tertiary Blue" } };
    QJSValue colours = eng->newArray(3);
    for (int k = 0; k < 3; ++k)
    {
        QJSValue c = eng->newObject();
        c.setProperty(QStringLiteral("r"), qMax(0, wledValue(fx, vals, QLatin1String(colourNames[k][0]))));
        c.setProperty(QStringLiteral("g"), qMax(0, wledValue(fx, vals, QLatin1String(colourNames[k][1]))));
        c.setProperty(QStringLiteral("b"), qMax(0, wledValue(fx, vals, QLatin1String(colourNames[k][2]))));
        colours.setProperty(uint(k), c);
    }
    QJSValue look = eng->newObject();
    look.setProperty(QStringLiteral("colors"), colours);
    look.setProperty(QStringLiteral("dimmer"), 1.0);
    QJSValue palettes = eng->newObject();
    palettes.setProperty(QStringLiteral("look"), look);

    QJSValue params = eng->newObject();
    for (const EffectScript::ParamDef &def : script->paramDefs())
        params.setProperty(def.name, double(def.defaultValue));
    const struct { const char *param, *channel; } fromChannels[] = {
        { "speed", "Effect Speed" }, { "intensity", "Effect Intensity" },
        { "palette", "Palette" }, { "option", "Effect Option" } };
    for (const auto &m : fromChannels)
    {
        const int v = wledValue(fx, vals, QLatin1String(m.channel));
        if (v >= 0)
            params.setProperty(QLatin1String(m.param), v);
    }

    QJSValue &state = m_states[fid];
    const QJSValue result = script->callTick(fixtures, inputs, palettes, params, state,
                                             eng->newObject());
    if (!result.isArray())
    {
        m_scripts.insert(path, QSharedPointer<EffectScript>());   // it said why, once
        return false;
    }

    f.source = Script;
    f.leds.resize(count);
    for (int i = 0; i < count; ++i)
    {
        const QJSValue o = result.property(uint(i));
        const auto ch = [&](const char *k) {
            const double v = o.property(QLatin1String(k)).toNumber();   // may be undefined
            return std::isfinite(v) ? qBound(0, qRound(qBound(0.0, v, 1e6) * level), 255) : 0;
        };
        f.leds[i] = o.isObject() ? QColor(ch("r"), ch("g"), ch("b")) : QColor(Qt::black);
    }
    m_scriptRan = true;
    if (!m_animate->isActive())
        m_animate->start();
    return true;
}
