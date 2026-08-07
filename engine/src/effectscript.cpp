/*
  Q Light Controller Plus
  effectscript.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscript.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QDebug>

// --- RGBScript compatibility host (see loadRGBScript) ------------------------

// Seeded xorshift Math.random so RGBScripts (which use Math.random freely) stay
// reproducible — the effect-script purity contract, relocated into a seed rather
// than removed. Per-instance engine, so each run is deterministic.
static const char *RGB_SEED_RANDOM =
    "(function(){var s=305419896;Math.random=function(){"
    "s^=s<<13;s^=s>>>17;s^=s<<5;return (s>>>0)/4294967296;};})();";

// Applies the host's param values to the RGBScript via its declared setter fns
// (__rgbProps maps param name -> setter method name on the algo).
static const char *RGB_APPLY_PROPS_FN =
    "function __rgbApplyProps(A,params){for(var k in __rgbProps){var fn=__rgbProps[k];"
    "if(params[k]!==undefined&&typeof A[fn]==='function'){try{A[fn](params[k]);}catch(e){}}}}";

// The effect wrapper: an object with tick() that drives the RGBScript. Reads the
// grid size from the fixtures, applies params, feeds the look's colour(s), steps
// from time, calls rgbMap(), and maps each fixture's cell to an r/g/b intent.
static const char *RGB_HOST_WRAPPER = R"JS(
(function(){
  var A = __rgbAlgo;
  var effect = new Object;
  effect.apiVersion = 1;
  effect.name = A.name || "RGB Effect";
  effect.tick = function(fixtures, inputs, palettes, params, state){
    var n = fixtures.length; if (n === 0) return [];
    var g0 = fixtures[0].grid || {cols:n, rows:1};
    var cols = g0.cols || n, rows = g0.rows || 1;

    // Colour: feed the look's first colour as the mono rgb; stacked look colours
    // via rgbMapSetColors for multi-colour (API v3+) scripts.
    var lk = (palettes.look && palettes.look.colors) ? palettes.look.colors : [];
    var rgbInt = 0xFFFFFF;
    if (lk.length && lk[0].r !== undefined) rgbInt = (lk[0].r<<16)|(lk[0].g<<8)|lk[0].b;

    var steps = 1; try { steps = A.rgbMapStepCount(cols, rows) | 0; } catch(e){}
    if (steps < 1) steps = 1;
    var rate = (params.rgbSpeed !== undefined) ? params.rgbSpeed : 8;
    var raw  = Math.floor((inputs._time||0) * rate);
    // Loop (wrap) vs Once (play through, then hold the last frame).
    var once = ((params.rgbLoop|0) === 1);
    var step = once ? Math.min(raw, steps - 1) : (raw % steps);
    if (step < 0) step += steps;

    // Regenerate the matrix ONLY when something that affects it changes — the
    // step, the grid, the colour, or a property. rgbMap() is expensive (a full
    // cols*rows matrix) and, for scripts using Math.random, non-deterministic per
    // call: running it every tick both pegs the CPU on a big grid AND makes such
    // scripts flash. The real RGB Matrix calls rgbMap once per step; match that.
    var psig = ""; for (var pk in params) psig += pk + "=" + params[pk] + ";";
    var key = step + "|" + cols + "x" + rows + "|" + rgbInt + "|" + psig;
    if (state.__rgbKey !== key) {
      __rgbApplyProps(A, params);
      if ((A.apiVersion|0) >= 3 && typeof A.rgbMapSetColors === "function" && lk.length){
        var arr = []; for (var i=0;i<lk.length;i++) arr.push((lk[i].r<<16)|(lk[i].g<<8)|(lk[i].b));
        try { A.rgbMapSetColors(arr); } catch(e){}
      }
      try { state.__rgbMap = A.rgbMap(cols, rows, rgbInt, step); } catch(e){ state.__rgbMap = null; }
      state.__rgbKey = key;
    }
    var map = state.__rgbMap;

    return fixtures.map(function(f){
      var g = f.grid || {col:0,row:0};
      var rr = map && map[g.row];
      var c = rr ? (rr[g.col]|0) : 0;
      var out = { r:(c>>16)&255, g:(c>>8)&255, b:(c&255) };
      if (f.hasDimmer) out.dimmer = Math.max(out.r, Math.max(out.g, out.b))/255;
      return out;
    });
  };
  return effect;
})()
)JS";

EffectScript::EffectScript()
{
}

bool EffectScript::load(const QString &path)
{
    m_valid = false;
    m_filePath = path;
    m_name.clear();
    m_description.clear();
    m_notes.clear();
    m_author.clear();
    m_apiVersion = 0;
    m_lifecycle = QStringLiteral("loop");
    m_syncTo    = QStringLiteral("span");
    m_onFinish  = QStringLiteral("hold");
    m_naturalDurationMs = 2000;
    m_inputs.clear();
    m_palettes.clear();
    m_targets.clear();
    m_params.clear();
    m_fixtureTypes.clear();
    m_dataChannelKeys.clear();
    m_script = QJSValue();
    m_tickFn  = QJSValue();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[EffectScript] Cannot open" << path;
        return false;
    }
    const QString contents = QString::fromUtf8(f.readAll());
    f.close();

    m_script = m_engine.evaluate(contents, path);
    qDebug() << "[ES-DIAG] evaluate: isError=" << m_script.isError()
             << "isObject=" << m_script.isObject()
             << "isCallable=" << m_script.isCallable()
             << "isUndefined=" << m_script.isUndefined();
    if (m_script.isError())
    {
        qWarning() << "[EffectScript]" << path << "evaluate error:"
                   << m_script.toString();
        return false;
    }
    if (!m_script.isObject())
    {
        qWarning() << "[EffectScript]" << path
                   << "script must return an object (use an IIFE)";
        return false;
    }

    // RGBScript compatibility: a script exposing the QLC+ RGB Matrix API
    // (rgbMap()/rgbMapStepCount()) but no tick() is run through a host wrapper
    // that maps its per-cell output onto the effect fixtures via the grid context.
    if (m_script.property("rgbMap").isCallable() &&
        !m_script.property("tick").isCallable())
        return loadRGBScript();

    m_tickFn = m_script.property("tick");
    qDebug() << "[ES-DIAG] tick: isCallable=" << m_tickFn.isCallable();
    if (!m_tickFn.isCallable())
    {
        qWarning() << "[EffectScript]" << path << "missing tick() function";
        return false;
    }

    if (!parseMeta())
    {
        qWarning() << "[EffectScript]" << path << "parseMeta() failed";
        return false;
    }

    m_valid = true;
    qDebug() << "[ES-DIAG] load OK:" << path;
    return true;
}

bool EffectScript::parseMeta()
{
    m_apiVersion = m_script.property("apiVersion").toInt();
    if (m_apiVersion < 1)
    {
        qWarning() << "[EffectScript]" << m_filePath
                   << "apiVersion must be >= 1";
        return false;
    }

    m_name        = m_script.property("name").toString();
    m_description = m_script.property("description").toString();
    m_notes       = m_script.property("notes").toString();
    m_author      = m_script.property("author").toString();
    m_version     = m_script.property("version").toInt();
    if (m_version < 1)
        m_version = 1;   // default for Generators that don't declare one

    // Lifecycle (see EFFECT_LIFECYCLE_DESIGN.md). Absent → loop (today's default).
    m_lifecycle = m_script.property("lifecycle").toString().toLower();
    if (m_lifecycle.isEmpty())
        m_lifecycle = QStringLiteral("loop");
    m_syncTo = m_script.property("syncTo").toString().toLower();
    if (m_syncTo.isEmpty())
        m_syncTo = QStringLiteral("span");
    m_onFinish = m_script.property("onFinish").toString().toLower();
    if (m_onFinish.isEmpty())
        m_onFinish = QStringLiteral("hold");
    m_naturalDurationMs = m_script.property("naturalDurationMs").toInt();
    if (m_naturalDurationMs <= 0)
        m_naturalDurationMs = 2000;

    if (m_name.isEmpty())
    {
        qWarning() << "[EffectScript]" << m_filePath << "missing 'name'";
        return false;
    }

    // --- inputs ---
    QJSValue inArr = m_script.property("inputs");
    if (inArr.isArray())
    {
        int len = inArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            QJSValue item = inArr.property(i);
            InputDef def;
            def.name         = item.property("name").toString();
            def.description  = item.property("description").toString();
            def.defaultValue = (float)item.property("defaultValue").toNumber();
            if (!def.name.isEmpty())
                m_inputs.append(def);
        }
    }

    // --- palettes ---
    QJSValue palArr = m_script.property("palettes");
    if (palArr.isArray())
    {
        int len = palArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            QJSValue item = palArr.property(i);
            PaletteDef def;
            def.name        = item.property("name").toString();
            def.description = item.property("description").toString();
            def.type        = item.property("type").toString();
            def.optional    = item.property("optional").toBool();
            if (!def.name.isEmpty())
                m_palettes.append(def);
        }
    }

    // --- targets ---
    QJSValue tgtArr = m_script.property("targets");
    if (tgtArr.isArray())
    {
        int len = tgtArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            QJSValue item = tgtArr.property(i);
            TargetDef def;
            def.name        = item.property("name").toString();
            def.description = item.property("description").toString();
            def.optional    = item.property("optional").toBool();
            if (!def.name.isEmpty())
                m_targets.append(def);
        }
    }

    // --- parameters ---
    QJSValue prArr = m_script.property("parameters");
    if (prArr.isArray())
    {
        int len = prArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            QJSValue item = prArr.property(i);
            ParamDef def;
            def.name         = item.property("name").toString();
            def.description  = item.property("description").toString();
            def.type         = item.property("type").toString();
            def.min          = (float)item.property("min").toNumber();
            def.max          = (float)item.property("max").toNumber();
            def.defaultValue = (float)item.property("defaultValue").toNumber();
            QJSValue vals = item.property("values");
            if (vals.isArray())
            {
                int vlen = vals.property("length").toInt();
                for (int vi = 0; vi < vlen; ++vi)
                    def.enumValues << vals.property(vi).toString();
            }
            QJSValue aliases = item.property("aliases");
            if (aliases.isArray())
            {
                int alen = aliases.property("length").toInt();
                for (int ai = 0; ai < alen; ++ai)
                    def.aliases << aliases.property(ai).toString();
            }
            if (!def.name.isEmpty())
                m_params.append(def);
        }
    }

    // --- dataChannels ---
    QJSValue dcArr = m_script.property("dataChannels");
    if (dcArr.isArray())
    {
        int len = dcArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            const QString ch = dcArr.property(i).toString().trimmed();
            if (!ch.isEmpty())
                m_dataChannelKeys.append(ch);
        }
    }

    // --- fixtureTypes ---
    QJSValue ftArr = m_script.property("fixtureTypes");
    if (ftArr.isArray())
    {
        int len = ftArr.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            const QString t = ftArr.property(i).toString().trimmed();
            if (!t.isEmpty())
                m_fixtureTypes.append(t);
        }
    }

    return true;
}

bool EffectScript::loadRGBScript()
{
    QJSValue algo = m_script;   // the RGBScript's algo object (from the IIFE)

    m_name   = algo.property("name").toString();
    if (m_name.isEmpty())
        m_name = QFileInfo(m_filePath).baseName();
    m_author       = algo.property("author").toString();
    m_apiVersion   = 1;
    m_version      = 1;
    m_fixtureTypes = QStringList() << "rgb" << "dimmer";

    // Translate the RGBScript's `properties` strings into effect params, and build
    // a name -> setter-method map so the host can push param values back in.
    //   "name:density|type:range|display:Density|values:1,100|write:setDensity|read:getDensity"
    QJSValue setterMap = m_engine.newObject();
    QJSValue props = algo.property("properties");
    if (props.isArray())
    {
        const int len = props.property("length").toInt();
        for (int i = 0; i < len; ++i)
        {
            const QString spec = props.property(i).toString();
            QMap<QString, QString> kv;
            const QStringList pairs = spec.split('|', Qt::SkipEmptyParts);
            for (const QString &pair : pairs)
            {
                const int c = pair.indexOf(':');
                if (c > 0)
                    kv.insert(pair.left(c).trimmed(), pair.mid(c + 1).trimmed());
            }
            const QString pname = kv.value("name");
            if (pname.isEmpty())
                continue;

            ParamDef def;
            def.name        = pname;
            def.description = kv.value("display", pname);
            const QString type = kv.value("type");
            if (type == QLatin1String("range"))
            {
                const QStringList vv = kv.value("values").split(',');
                if (vv.size() == 2) { def.min = vv[0].toFloat(); def.max = vv[1].toFloat(); }
            }
            else if (type == QLatin1String("bool"))
            {
                def.min = 0.0f; def.max = 1.0f;
                def.enumValues << QStringLiteral("Off") << QStringLiteral("On");
            }
            else if (type == QLatin1String("list"))
            {
                // Dropdown: "values:A,B,C" → integer-index enum (the RGBScript's
                // setter takes the index).
                const QStringList opts = kv.value("values").split(',', Qt::SkipEmptyParts);
                for (const QString &o : opts)
                    def.enumValues << o.trimmed();
                def.min = 0.0f;
                def.max = float(qMax(0, def.enumValues.size() - 1));
            }
            // Default from the read fn, else the current property value.
            const QString readFn = kv.value("read");
            QJSValue cur;
            if (!readFn.isEmpty() && algo.property(readFn).isCallable())
                cur = algo.property(readFn).callWithInstance(algo);
            else
                cur = algo.property(pname);
            if (cur.isNumber())
                def.defaultValue = (float)cur.toNumber();
            m_params.append(def);

            const QString writeFn = kv.value("write");
            if (!writeFn.isEmpty())
                setterMap.setProperty(pname, writeFn);
        }
    }

    // Animation-rate param: RGBScripts step through rgbMapStepCount frames driven
    // by the RGB Matrix function clock; here we drive from time, so expose a rate.
    {
        ParamDef def;
        def.name = QStringLiteral("rgbSpeed");
        def.description = QStringLiteral("Animation speed (steps/sec)");
        def.min = 0.5f; def.max = 60.0f; def.defaultValue = 8.0f;
        m_params.append(def);
    }
    // Loop vs single-shot: Once plays the animation through a single time and
    // holds the final frame instead of restarting.
    {
        ParamDef def;
        def.name = QStringLiteral("rgbLoop");
        def.description = QStringLiteral("Playback");
        def.enumValues = QStringList() << QStringLiteral("Loop")
                                       << QStringLiteral("Once (hold last)");
        def.defaultValue = 0.0f;
        m_params.append(def);
    }

    // Wire up the host wrapper and run it to produce the effect object.
    m_engine.globalObject().setProperty(QStringLiteral("__rgbAlgo"), algo);
    m_engine.globalObject().setProperty(QStringLiteral("__rgbProps"), setterMap);
    m_engine.evaluate(QString::fromLatin1(RGB_SEED_RANDOM));
    m_engine.evaluate(QString::fromLatin1(RGB_APPLY_PROPS_FN));

    QJSValue wrapped = m_engine.evaluate(QString::fromLatin1(RGB_HOST_WRAPPER),
                                         QStringLiteral("rgbhost"));
    if (wrapped.isError() || !wrapped.isObject())
    {
        qWarning() << "[EffectScript] RGB host wrapper failed for" << m_filePath
                   << (wrapped.isError() ? wrapped.toString() : QString());
        return false;
    }
    m_script = wrapped;
    m_tickFn = m_script.property("tick");
    if (!m_tickFn.isCallable())
    {
        qWarning() << "[EffectScript] RGB host wrapper has no tick()" << m_filePath;
        return false;
    }

    m_valid = true;
    qDebug() << "[ES-DIAG] RGBScript host load OK:" << m_filePath << "name=" << m_name;
    return true;
}

QJSValue EffectScript::callTick(const QJSValue &fixtures,
                                 const QJSValue &inputs,
                                 const QJSValue &palettes,
                                 const QJSValue &params,
                                 QJSValue       &state,
                                 const QJSValue &data)
{
    if (!m_valid || !m_tickFn.isCallable())
        return QJSValue();

    QJSValueList args;
    args << fixtures << inputs << palettes << params << state << data;
    QJSValue result = m_tickFn.call(args);

    if (result.isError())
    {
        qWarning() << "[EffectScript]" << m_filePath
                   << "tick() error:" << result.toString();
        return QJSValue();
    }
    return result;
}

QJSValue EffectScript::newState()
{
    return m_engine.newObject();
}
