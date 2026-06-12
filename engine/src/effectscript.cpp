/*
  Q Light Controller Plus
  effectscript.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include "effectscript.h"

#include <QFile>
#include <QDebug>

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
    m_inputs.clear();
    m_palettes.clear();
    m_params.clear();
    m_fixtureTypes.clear();
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

    m_tickFn = m_script.property("tick");
    if (!m_tickFn.isCallable())
    {
        qWarning() << "[EffectScript]" << path << "missing tick() function";
        return false;
    }

    if (!parseMeta())
        return false;

    m_valid = true;
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
            def.name     = item.property("name").toString();
            def.type     = item.property("type").toString();
            def.optional = item.property("optional").toBool();
            if (!def.name.isEmpty())
                m_palettes.append(def);
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
            def.min          = (float)item.property("min").toNumber();
            def.max          = (float)item.property("max").toNumber();
            def.defaultValue = (float)item.property("defaultValue").toNumber();
            if (!def.name.isEmpty())
                m_params.append(def);
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

QJSValue EffectScript::callTick(const QJSValue &fixtures,
                                 const QJSValue &inputs,
                                 const QJSValue &palettes,
                                 const QJSValue &params,
                                 QJSValue       &state)
{
    if (!m_valid || !m_tickFn.isCallable())
        return QJSValue();

    QJSValueList args;
    args << fixtures << inputs << palettes << params << state;
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
