/*
  Q Light Controller Plus
  qlcpalette.h

  Copyright (C) Massimo Callegari

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

#ifndef QLCPALETTE_H
#define QLCPALETTE_H

#include <QColor>
#include <QObject>
#include <QVariant>

class QXmlStreamReader;
class QXmlStreamWriter;
class SceneValue;
class Doc;
class Scene;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCPalette   QStringLiteral("Palette")
#define KXMLQLCPaletteID QStringLiteral("ID")

/**
 * QLCPalette represents a QLC+ Palette, which is the definition
 * of a capability such as color, position, dimmer, etc
 * that can be applied to an arbitrary group of fixtures.
 */
class QLCPalette final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(int type READ type CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int intValue1 READ intValue1 CONSTANT)
    Q_PROPERTY(int intValue2 READ intValue2 CONSTANT)
    Q_PROPERTY(float floatValue1 READ floatValue1 CONSTANT)
    Q_PROPERTY(QString strValue1 READ strValue1 CONSTANT)
    Q_PROPERTY(QColor rgbValue READ rgbValue CONSTANT)
    Q_PROPERTY(QColor wauvValue READ wauvValue CONSTANT)
    Q_PROPERTY(FanningType fanningType READ fanningType WRITE setFanningType NOTIFY fanningTypeChanged)
    Q_PROPERTY(FanningLayout fanningLayout READ fanningLayout WRITE setFanningLayout NOTIFY fanningLayoutChanged)
    Q_PROPERTY(int fanningAmount READ fanningAmount WRITE setFanningAmount NOTIFY fanningAmountChanged)
    Q_PROPERTY(QVariant fanningValue READ fanningValue WRITE setFanningValue NOTIFY fanningValueChanged)

public:
    /**
     * The Palette type. This is fundamental
     * to properly handle values
     */
    enum PaletteType
    {
        Undefined = 0,
        Dimmer,
        Color,
        Pan,
        Tilt,
        PanTilt,    ///< Raw pan/tilt degrees stored in m_values (XY pad driven)
        Aim,        ///< Aims at a named StageTarget using per-fixture rig geometry
        Shutter,    ///< Opens the beam — fixture-aware: finds ShutterOpen capability value
        Strobe,     ///< Strobe at a relative rate (0=off/slow, 1=fastest) — fixture-aware
        Gobo,
        Zoom,
        Beam,
        Effect      ///< Computed by a JS effect script (tick-driven)
    };
#if QT_VERSION >= 0x050500
    Q_ENUM(PaletteType)
#endif

    /************************************************************************
     * Initialization
     ************************************************************************/
public:
    QLCPalette(QLCPalette::PaletteType type, QObject *parent = 0);
    QLCPalette *createCopy();

    virtual ~QLCPalette();

    /************************************************************************
     * Properties
     ************************************************************************/
public:
    /** Get/Set the palette ID */
    quint32 id() const;
    void setID(quint32 id);

    /** Get an invalid palette id */
    static quint32 invalidId();

    /** Get the palette type */
    PaletteType type() const;

    /** Helper methods to convert palette type <-> string */
    static QString typeToString(QLCPalette::PaletteType type);
    static PaletteType stringToType(const QString& str);

    Q_INVOKABLE QString iconResource(bool svg = false) const;

    /** Get/Set the name of this palette */
    QString name() const;
    void setName(const QString& name);

    /** Get/Set the folder path of this palette in the Function Manager
     *  tree. Uses the same "Category/Folder/Sub" convention as Functions,
     *  with "Palettes" as the leading category segment. An empty path (or
     *  just "Palettes/") means the Palettes category root. */
    QString path() const;
    void setPath(const QString& path);

    /** Get/Set the value(s) for this Palette.
     *  Some types like Position will store 2 values */
    QVariant value() const;
    int intValue1() const;
    int intValue2() const;
    float floatValue1() const;
    QString strValue1() const;
    QColor rgbValue() const;
    QColor wauvValue() const;

    void setValue(QVariant val);
    void setValue(QVariant val1, QVariant val2);
    QVariantList values() const;
    void setValues(QVariantList values);
    void resetValues();

    /**
     * Expand this palette into concrete SceneValues for @a fixtures.
     *
     * @a owner is the Scene this palette is being expanded for, and it matters
     * for the Aim type: an Aim look aims at a SUBJECT (a person) when its own
     * scene also carries a follow-spot effect, and at a static point otherwise.
     * Pass it whenever you have it. Without it, that question can only be
     * answered by scanning every Function in the Doc — which is O(functions x
     * fixtures) and, worse, a data race when called from the MasterTimer thread
     * (Scene::write) while the GUI thread adds or deletes functions. Callers on
     * the DMX path MUST pass it; the nullptr fallback exists only for GUI-side
     * callers with no scene context.
     */
    QList<SceneValue> valuesFromFixtures(Doc *doc, QList<quint32>fixtures,
                                         const Scene *owner = nullptr);
    QList<SceneValue> valuesFromFixtureGroups(Doc *doc, QList<quint32>groups,
                                              const Scene *owner = nullptr);

protected:
    /** This method returns a normalized factor between 0.0 and 1.0
     *  which will then be multiplied by a value to obtain the final
     *  DMX value.
     *  It considers the fanning algorithm and amount and with
     *  the provided progress it can calculate the X-axis value. */
    qreal valueFactor(qreal progress);

signals:
    void nameChanged();

private:
    quint32 m_id;
    PaletteType m_type;
    QString m_name;
    QString m_path;
    QVariantList m_values;

    /************************************************************************
     * Fanning
     ************************************************************************/
public:
    enum FanningType
    {
        Flat,
        Linear,
        Sine,
        Square,
        Saw
    };
#if QT_VERSION >= 0x050500
    Q_ENUM(FanningType)
#endif

    enum FanningLayout
    {
        XAscending,
        XDescending,
        XCentered,
        YAscending,
        YDescending,
        YCentered,
        ZAscending,
        ZDescending,
        ZCentered
    };
#if QT_VERSION >= 0x050500
    Q_ENUM(FanningLayout)
#endif

    /** Get/Set the fanning type */
    FanningType fanningType() const;
    void setFanningType(QLCPalette::FanningType type);

    /** Helper methods to convert fanning type <-> string */
    static QString fanningTypeToString(QLCPalette::FanningType type);
    static FanningType stringToFanningType(const QString& str);

    /** Get/Set the fanning layout */
    FanningLayout fanningLayout() const;
    void setFanningLayout(QLCPalette::FanningLayout layout);

    /** Helper methods to convert fanning layout <-> string */
    static QString fanningLayoutToString(QLCPalette::FanningLayout layout);
    static FanningLayout stringToFanningLayout(const QString& str);

    /** Get/Set the amount of fanning applied to this palette */
    int fanningAmount() const;
    void setFanningAmount(int amount);

    /** Get/Set the fanning value */
    QVariant fanningValue() const;
    void setFanningValue(QVariant value);

signals:
    void fanningTypeChanged();
    void fanningLayoutChanged();
    void fanningAmountChanged();
    void fanningValueChanged();

private:
    FanningType m_fanningType;
    FanningLayout m_fanningLayout;
    int m_fanningAmount;
    QVariant m_fanningValue;

    /************************************************************************
     * Color helpers
     ************************************************************************/
public:
    /** Helper method to pack two QColor into a Qt-like style string
     *  formatted like this: "#rrggbbwwaauv" */
    static QString colorToString(QColor rgb, QColor wauv);

    /** Helper method to convert a string created with colorToString
     *  back to 2 separate QColor */
    static bool stringToColor(QString str, QColor &rgb, QColor &wauv);

    /************************************************************************
     * Stage target linkage
     ************************************************************************/
public:
    /** Return the StageTarget ID this palette aims at, or StageTarget::invalidId(). */
    quint32 stageTargetId() const { return m_stageTargetId; }

    /** Associate this palette with a StageTarget (pass invalidId() to clear). */
    void setStageTargetId(quint32 id) { m_stageTargetId = id; }

private:
    quint32 m_stageTargetId = UINT_MAX;

    /************************************************************************
     * Effect script (type == Effect only)
     ************************************************************************/
public:
    /** Absolute or search-relative path to the .js script file. */
    QString scriptPath() const { return m_scriptPath; }
    void setScriptPath(const QString &p) { m_scriptPath = p; }

    /**
     * Persist this effect's script state across stop/start.
     *
     * Off (default): every time a scene carrying this look starts, the effect
     * gets a fresh engine and runs from its first frame.
     *
     * On: the running engine is PARKED when the last scene using this look
     * stops, and re-adopted — state, phase and all — by the next scene that
     * uses THIS SAME palette. So a slow effect (a trickle down the front cloth)
     * keeps its place across a scene change instead of snapping back to frame 0.
     *
     * Identity is the palette: a different Effect look is a different effect and
     * always starts clean. This is the exact analogue of RGBMatrix::persistent(),
     * where identity is the matrix function.
     */
    bool persistent() const { return m_persistent; }
    void setPersistent(bool p) { m_persistent = p; }

    /** Name of the effect PRESET this palette was stamped from (empty if the
     *  user picked a raw engine script). Purely identity/display — the script
     *  and param values above are authoritative, so the look loads fine even
     *  if the preset file is gone. */
    QString effectPreset() const { return m_effectPreset; }
    void setEffectPreset(const QString &name) { m_effectPreset = name; }

    /** Input slot bindings: slot name → (universe, channel).
     *  The EffectScriptRunner maps incoming input events to slot values. */
    QMap<QString, QPair<quint32,quint32>> effectInputBindings() const
        { return m_effectInputBindings; }
    void setEffectInputBindings(const QMap<QString, QPair<quint32,quint32>> &b)
        { m_effectInputBindings = b; }
    void setEffectInputBinding(const QString &slot, quint32 universe, quint32 channel)
        { m_effectInputBindings[slot] = qMakePair(universe, channel); }
    void clearEffectInputBindings() { m_effectInputBindings.clear(); }

    /** Palette slot bindings: slot name → palette ID. */
    QMap<QString, quint32> effectPaletteBindings() const
        { return m_effectPaletteBindings; }
    void setEffectPaletteBinding(const QString &slot, quint32 paletteId)
        { m_effectPaletteBindings[slot] = paletteId; }
    void clearEffectPaletteBinding(const QString &slot)
        { m_effectPaletteBindings.remove(slot); }

    /** Target slot bindings: slot name → StageTarget ID. */
    QMap<QString, quint32> effectTargetBindings() const
        { return m_effectTargetBindings; }
    void setEffectTargetBinding(const QString &slot, quint32 targetId)
        { m_effectTargetBindings[slot] = targetId; }
    void clearEffectTargetBinding(const QString &slot)
        { m_effectTargetBindings.remove(slot); }

    /** Script parameter overrides: param name → value. */
    QMap<QString, double> effectParamValues() const
        { return m_effectParamValues; }
    void setEffectParamValue(const QString &name, double value)
        { m_effectParamValues[name] = value; }
    /** Replace ALL numeric param overrides at once (used when stamping a
     *  preset). String/path params are cleared too so nothing stale lingers. */
    void setEffectParamValues(const QMap<QString, double> &values)
        { m_effectParamValues = values; m_effectStringParams.clear(); }
    void clearEffectParamValues()
        { m_effectParamValues.clear(); m_effectStringParams.clear(); }

    /** String parameter overrides (e.g. type:"path" XY path JSON). */
    QMap<QString, QString> effectStringParams() const
        { return m_effectStringParams; }
    QString effectStringParam(const QString &name) const
        { return m_effectStringParams.value(name); }
    void setEffectStringParam(const QString &name, const QString &value)
        { m_effectStringParams[name] = value; }

    /** Convenience: get a single colour value for Color-type palettes
     *  (used by EffectInstance to build the palettes object for scripts). */
    QColor colorValue() const;

private:
    QString m_scriptPath;
    bool m_persistent = false;
    QString m_effectPreset;
    QMap<QString, QPair<quint32,quint32>> m_effectInputBindings;
    QMap<QString, quint32>               m_effectPaletteBindings;
    QMap<QString, quint32>               m_effectTargetBindings;
    QMap<QString, double>                m_effectParamValues;
    QMap<QString, QString>               m_effectStringParams;

    /************************************************************************
     * Load & Save
     ************************************************************************/
public:
    /** Helper method to allocate and add a Palette to a Doc */
    static bool loader(QXmlStreamReader &xmlDoc, Doc *doc);

    /** Load a Palette from the given QXmlStreamReader */
    bool loadXML(QXmlStreamReader &doc);

    /** Save a Palette to the given XML tag in the given document */
    bool saveXML(QXmlStreamWriter *doc);
};

/** @} */

#endif
