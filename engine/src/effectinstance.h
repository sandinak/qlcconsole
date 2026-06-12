/*
  Q Light Controller Plus
  effectinstance.h

  One running instance of an effect script bound to a specific scene.
  Owns an EffectScript (its own QJSEngine), current input/param values,
  a persistent state object, and the last computed DMX write list.

  Threading model:
    runTick()   — must be called from the MAIN thread (QJSEngine constraint)
    dmxWrites() — may be called from the MasterTimer thread (mutex-protected)

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTINSTANCE_H
#define EFFECTINSTANCE_H

#include <QMutex>
#include <QJSValue>
#include <QHash>
#include <QList>
#include <QElapsedTimer>

#include "effectscript.h"

class Doc;

class EffectInstance
{
public:
    struct DmxWrite {
        int   universeId;
        int   addr;        // universe-local address (0-511)
        uchar value;
    };

    EffectInstance(Doc *doc, quint32 sceneId, quint32 effectPaletteId);

    bool isValid() const { return m_script.isValid(); }

    quint32 sceneId()         const { return m_sceneId; }
    quint32 effectPaletteId() const { return m_effectPaletteId; }

    // --- Input values (main thread, set by EffectScriptRunner on input events) ---
    void setInputValue(const QString &slotName, float norm01);
    float inputValue(const QString &slotName) const;

    // --- Param values ---
    void setParamValue(const QString &name, double value);

    // --- Run one tick (MAIN THREAD ONLY) ---
    void runTick();

    // --- Read last results (any thread, mutex-protected) ---
    QList<DmxWrite> dmxWrites() const;

private:
    QJSValue buildFixturesArray();
    QJSValue buildInputsObject() const;
    QJSValue buildPalettesObject() const;
    QJSValue buildParamsObject() const;

    QList<DmxWrite> parseIntents(const QJSValue &intents,
                                 const QList<quint32> &fxIds) const;

    Doc    *m_doc;
    quint32 m_sceneId;
    quint32 m_effectPaletteId;

    EffectScript     m_script;
    QJSValue         m_state;
    QElapsedTimer    m_elapsed;

    QHash<QString, float>   m_inputValues;
    QHash<QString, double>  m_paramValues;

    mutable QMutex   m_mutex;
    QList<DmxWrite>  m_lastResults;
};

#endif // EFFECTINSTANCE_H
