/*
  Q Light Controller Plus
  effectscript.h

  Loads a single effect .js file, validates it against the API contract,
  and provides callTick() to execute the script's tick() function.

  Script format (IIFE returning an effect object):

    (function() {
        var effect = new Object;
        effect.apiVersion = 1;
        effect.name        = "My Effect";
        effect.description  = "One-line description shown as combo tooltip";
        effect.notes        = "Longer paragraph shown in the effect editor panel";
        effect.fixtureTypes = ["moving", "rgb"];  // tags: moving, rgb, dimmer, shutter
        effect.author      = "Your name";

        // Input slots: normalized float values (0.0–1.0) fed by the host
        // from any input source (joystick axis, MIDI CC, OSC, …).
        // _time (seconds since start) is always injected by the host.
        effect.inputs = [
            { name: "x", description: "Pan axis",  defaultValue: 0.5 },
            { name: "y", description: "Tilt axis", defaultValue: 0.5 }
        ];

        // Named palette references the host resolves before each tick.
        // type matches a QLCPalette type string: "Color", "Dimmer", "PanTilt", etc.
        effect.palettes = [
            { name: "color",  type: "Color",  optional: true  },
            { name: "dimmer", type: "Dimmer", optional: true  }
        ];

        // Stage target references. The host pre-computes per-fixture aim angles
        // and injects them into each fixture descriptor as f.aimAt[name] = {pan, tilt}.
        effect.targets = [
            { name: "center", optional: true, description: "Aim center point" }
        ];

        // Parameters shown in the look editor.
        // When 'values' is present the param is an integer index shown as a dropdown.
        // When type is "path" the param is an XY drawn-path widget; min/max/defaultValue
        // are ignored and the value is stored as a JSON array [[x,y], ...] of 0-1 pairs.
        effect.parameters = [
            { name: "speed", description: "Scale", min: 0.1, max: 3.0, defaultValue: 1.0 },
            { name: "mode",  description: "Mode",  min: 0,   max: 2,   defaultValue: 0,
              values: ["Alternate", "Gradient", "Spread"] },
            { name: "path",  description: "Movement path", type: "path" }
        ];

        // Called every ~50 Hz tick while the scene is running.
        //   fixtures  – array of FixtureDescriptor objects (see below)
        //   inputs    – { slotName: 0.0-1.0, …,
        //                 _time:      seconds since scene start,
        //                 _beat:      0.0-1.0 sawtooth phase within current beat,
        //                 _bpm:       current BPM (0 = no beat source),
        //                 _beatCount: integer count of beats since start }
        //   palettes  – { paletteName: resolvedValues | null, … }
        //   params    – { paramName: value, … }
        //   state     – persistent plain object (reset on scene start)
        //
        // Returns an array (one entry per fixture, same order as fixtures[]).
        // Each entry is a plain object with any of:
        //   pan, tilt (degrees), dimmer (0-1),
        //   r, g, b, w, a, uv (0-255), gobo (index), shutter (0-1)
        // Omit fields the script doesn't control.
        effect.tick = function(fixtures, inputs, palettes, params, state) {
            return fixtures.map(function(f) {
                // f.aimAt.center = { pan: degrees, tilt: degrees }  (when target bound)
                return { pan: inputs.x * f.panRange, tilt: inputs.y * f.tiltRange };
            });
        };

        return effect;
    })()

  FixtureDescriptor fields available inside tick():
    id         – quint32 fixture id
    head       – head index (0 for single-head)
    panRange   – degrees (from QLCPhysical, default 360)
    tiltRange  – degrees (from QLCPhysical, default 270)
    hasPanTilt – bool
    hasRGB     – bool
    hasDimmer  – bool
    hasGobo    – bool
    hasShutter – bool
    pos        – { x, y, z } in metres (rig 3D world position, or {0,0,0})
    aimAt      – { <targetName>: { pan: deg, tilt: deg }, … }
                 pre-computed aim angles for each bound effect.targets entry

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef EFFECTSCRIPT_H
#define EFFECTSCRIPT_H

#include <QJSEngine>
#include <QJSValue>
#include <QString>
#include <QList>
#include <QStringList>

class EffectScript
{
public:
    EffectScript();

    /** Load and evaluate @p path. Returns true iff the script is valid. */
    bool load(const QString &path);

    bool isValid() const { return m_valid; }
    QString filePath() const { return m_filePath; }

    // --- Metadata (available after a successful load()) ---
    QString     name()         const { return m_name; }
    QString     description()  const { return m_description; }
    QString     notes()        const { return m_notes; }
    QString     author()       const { return m_author; }
    int         apiVersion()   const { return m_apiVersion; }
    /** Generator content version (effect.version; default 1). Bumped by the
     *  author when params/behaviour change; used to reason about Effects
     *  (presets) authored against an older Generator. */
    int         version()      const { return m_version; }

    /** Fixture-type tags declared in the script (e.g. "moving", "rgb", "dimmer").
     *  Empty list means the script works with any fixture type. */
    QStringList fixtureTypes() const { return m_fixtureTypes; }

    struct InputDef {
        QString name, description;
        float defaultValue = 0.5f;
    };
    struct PaletteDef {
        QString name, description, type; // type: "Color", "Dimmer", "PanTilt", …
        bool optional = true;
    };
    struct TargetDef {
        QString name, description;
        bool optional = true;
    };
    struct ParamDef {
        QString name, description;
        QString type;           // "" or "path" (XY drawn path); "path" params ignore min/max
        float min = 0.0f, max = 1.0f, defaultValue = 0.5f;
        QStringList enumValues; // non-empty → render as dropdown; value is integer index
        QStringList aliases;    // former names; stored values under an alias map to this param
    };

    QList<InputDef>   inputDefs()       const { return m_inputs;          }
    QList<PaletteDef> paletteDefs()    const { return m_palettes;        }
    QList<TargetDef>  targetDefs()     const { return m_targets;         }
    QList<ParamDef>   paramDefs()      const { return m_params;          }
    /** Named real-time data channels this script subscribes to (e.g. "joystick"). */
    QStringList       dataChannelKeys() const { return m_dataChannelKeys; }

    /** Call tick(fixtures, inputs, palettes, params, state, data).
     *  Must be called from the thread that created this EffectScript.
     *  @p state is an in/out persistent JS object (survives between ticks).
     *  @p data is an object keyed by channel name (e.g. data.joystick.pan).
     *  Returns the raw QJSValue returned by the script (array of intents),
     *  or an invalid QJSValue on error. */
    QJSValue callTick(const QJSValue &fixtures,
                      const QJSValue &inputs,
                      const QJSValue &palettes,
                      const QJSValue &params,
                      QJSValue       &state,
                      const QJSValue &data);

    /** Create a fresh empty state object in this engine. */
    QJSValue newState();

    /** Access the engine (e.g. to build JS arg objects). */
    QJSEngine *engine() const { return const_cast<QJSEngine*>(&m_engine); }

private:
    bool parseMeta();

    QJSEngine m_engine;
    QJSValue  m_script;   // the object returned by the IIFE
    QJSValue  m_tickFn;   // the tick() function ref

    QString     m_filePath;
    QString     m_name, m_description, m_notes, m_author;
    int         m_apiVersion = 0;
    int         m_version = 1;
    QStringList m_fixtureTypes;

    QList<InputDef>   m_inputs;
    QList<PaletteDef> m_palettes;
    QList<TargetDef>  m_targets;
    QList<ParamDef>   m_params;
    QStringList       m_dataChannelKeys;

    bool m_valid = false;
};

#endif // EFFECTSCRIPT_H
