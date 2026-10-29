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
#include <QHash>
#include <QSet>

class Fixture;
class QXmlStreamReader;
class QXmlStreamWriter;
class SceneValue;
class Doc;
class Scene;
class QLCChannel;
class QLCCapability;

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
    Q_ENUM(PaletteType)

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

    /** A palette folder path as older shows stored it — the palette's OWN
     *  TYPE as the first folder ("Palettes/Color/01-Song/" for a Color
     *  palette, an effect auto-filed as "Palettes/Effect/<Category>/<Engine>/")
     *  — made type-independent ("Palettes/01-Song/"; an auto-filed effect is
     *  unfiled, ""). A first folder named like ANOTHER type, or like none
     *  ("Colour"), is a user folder and is kept. Applied on load only, to
     *  palettes without the Folders="1" marker. Paths not under "Palettes/"
     *  are returned unchanged. */
    static QString migrateLegacyPath(const QString &path, PaletteType type, bool anyTypeFolder = false);
    /** After a show's palettes are loaded (Doc::loadXML): when every
     *  unmarked palette path has a type's name first -- the old per-type
     *  layout throughout -- a first folder named like ANY type is dropped
     *  (an effect dragged under Color/01-Song joins 01-Song). Otherwise
     *  only each palette's own type name was (loadXML). */
    static void migrateLegacyPaths(const QList<QLCPalette *> &palettes);
    /** Whether @p name is a palette type's name as typeToString() gives it. */
    static bool isTypeFolderName(const QString &name);

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
    /**
     * @a colorSetOffset (Color type only) is the index of the fixture RGB set
     * this look's FIRST colour targets, so a scene can stack several colour looks
     * across a fixture's colour sets: the 1st colour look uses offset 0 (primary),
     * and each look advances the offset by the number of colours it carries. A
     * look's colour[k] lands on set[colorSetOffset + k]; sets past the fixture's
     * count are ignored. 0 = normal (primary) for a single colour look.
     */
    /** @p onlyHeads, when given, limits what is set per head to those heads
     *  of each fixture it names (a fixture group's own heads); a fixture it
     *  does not name keeps all of them. */
    QList<SceneValue> valuesFromFixtures(Doc *doc, QList<quint32>fixtures,
                                         const Scene *owner = nullptr, int colorSetOffset = 0,
                                         const QHash<quint32, QSet<int> > *onlyHeads = nullptr);
    QList<SceneValue> valuesFromFixtureGroups(Doc *doc, QList<quint32>groups,
                                              const Scene *owner = nullptr, int colorSetOffset = 0);

    /**
     * The RGB-set offset for @a paletteId given a scene's ordered look list:
     * the total number of colours carried by the Color looks that PRECEDE it.
     * Pass this as valuesFromFixtures(..., colorSetOffset) so stacked colour looks
     * fill a fixture's colour sets in order (1st look -> primary, 2nd -> secondary,
     * …). Non-colour palettes never advance the offset. Returns 0 for the first
     * colour look and for any non-Color palette.
     */
    static int colorSetOffset(Doc *doc, const QList<quint32> &orderedPalettes, quint32 paletteId);

    /**
     * Look-list ordering rule: true if @a paletteId is a Colour or Dimmer
     * palette placed under an Effect palette (before the next effect) -- it
     * belongs to that effect, applies to it only, and must NOT be painted onto
     * the fixtures as the look's static base. Anything else (and everything
     * above the first effect) is the look's own.
     */
    /** Whether @p fixture has a dimmer channel -- a master, or one in a head
     *  -- for a Dimmer look to set. Without one, the look's dimmer scales
     *  its colour instead (dimScaleWithoutDimmer()). */
    static bool fixtureHasDimmer(const Fixture *fixture);
    /** The look's own dimmer (0..255): the lowest Dimmer palette in
     *  @p orderedPalettes not under an effect, or -1 when there is none. */
    static int lookDimmerValue(Doc *doc, const QList<quint32> &orderedPalettes);
    /** @p value of @p channel on @p fixture with the look's dimmer
     *  @p lookDimmer applied when the fixture has no dimmer and the channel
     *  is a colour emitter; else @p value as is. */
    static uchar dimScaleWithoutDimmer(const Fixture *fixture, quint32 channel, uchar value, int lookDimmer);
    /** Whether @p effect takes the look's colours and dimmer as its input
     *  (reads palettes.look), so the Colour/Dimmer under it are its own. */
    static bool effectUsesLookColours(const QLCPalette *effect);
    static bool isEffectScoped(Doc *doc, const QList<quint32> &orderedPalettes, quint32 paletteId);

    /*********************************************************************
     * Shutter looks
     *********************************************************************/
    /** What a shutter value does to the beam. */
    enum ShutterIntent
    {
        ShutterIntentOpen,
        ShutterIntentClosed,
        ShutterIntentStrobe,
        /** Anything else: an effect ("Slow open - fast close"), "No
         *  function", a value no capability covers */
        ShutterIntentOther
    };

    /** What @p cap does: by its preset, else by a plain name ("Open",
     *  "Closed", "Blackout", "Strobe slow - fast"). */
    static ShutterIntent capabilityShutterIntent(const QLCCapability *cap);

    /** What @p value does on @p ch: a strobe-preset channel is open at 0
     *  and strobes above it; any other is read by its capability there. */
    static ShutterIntent channelShutterIntent(const QLCChannel *ch, int value);

    /** The shutter channel a Shutter look writes on @p fixture, or
     *  QLCChannel::invalid(). */
    static quint32 shutterChannel(const Fixture *fixture);

    /** What this Shutter look means, for @p fixtures: a look with no value
     *  (or 0, which every Shutter look made before Closed could be picked
     *  used for "open") opens; otherwise its value is read on the first of
     *  @p fixtures whose shutter has an Open, Closed or Strobe capability
     *  there -- the type it was picked from, for a look on one type. A
     *  strobe-preset channel (0 open, the rest strobe) never decides it: on
     *  its own it cannot tell a legacy "Open" at 255 from a strobe, so with
     *  nothing else to read it on the look stays open. @p reference, when
     *  given, gets the channel that decided it (nullptr when none did). */
    ShutterIntent shutterIntent(Doc *doc, const QList<quint32> &fixtures,
                                const QLCChannel **reference = nullptr) const;

    /** The DMX value this Shutter look puts on @p ch, a fixture's shutter
     *  channel, for @p intent (shutterIntent(), decided on @p reference):
     *  its own value where @p ch reads it the same way; else the value that
     *  does that on @p ch (its open, its closed, a strobe at the same
     *  rate). A shutter that cannot close, or cannot strobe, is sent open. */
    int shutterValueFor(const QLCChannel *ch, ShutterIntent intent,
                        const QLCChannel *reference) const;

    /** The value that opens @p ch (ShutterAutoOpen::openValue()). A channel
     *  with no Open capability gets @p fallback unless that closes or
     *  strobes it, else the middle of a capability that does neither ("No
     *  function"), else 255 (as before there was a Closed). */
    static int shutterOpenValue(const QLCChannel *ch, int fallback);

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
    /** The path as loaded without the Folders marker, until the show is
     *  loaded (migrateLegacyPaths()); null otherwise. */
    QString m_unmarkedPath;
    /** Loaded with the Folders marker: the show was saved with shared
     *  folders (until migrateLegacyPaths()). */
    bool m_loadedMarked = false;
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
    Q_ENUM(FanningType)

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
    Q_ENUM(FanningLayout)

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

    /** Aim looks: "Dim lights that can't see the target". On, a head this
     *  look points at its target with something solid on stage in the way
     *  (a closed drape, a wall, a riser -- BeamOcclusion) has its dimmer
     *  taken down while it is blocked, and back when it is clear. Off (the
     *  default) the look never touches a dimmer. */
    bool dimWhenBlocked() const { return m_dimWhenBlocked; }
    void setDimWhenBlocked(bool on) { m_dimWhenBlocked = on; }

private:
    quint32 m_stageTargetId = UINT_MAX;
    bool m_dimWhenBlocked = false;

    /************************************************************************
     * Gobo by name (type == Gobo only), docs/design/SHOW_TREE_DESIGN.md §3A / §10.7
     ************************************************************************/
public:
    /** The gobo this look means, by name ("Breakup"). Empty: the look is a
     *  plain DMX value() on every fixture's gobo channel (as it always was).
     *  Named, it resolves per fixture definition: goboValueFor(). */
    QString goboName() const { return m_goboName; }
    void setGoboName(const QString &name) { m_goboName = name; }
    bool isGoboByName() const { return m_type == Gobo && !m_goboName.isEmpty(); }

    /** The slot picked by hand for a fixture definition (its
     *  definitionKey()): wins over the name match. */
    QMap<QString, int> goboValues() const { return m_goboValues; }
    void setGoboValue(const QString &definition, int value) { m_goboValues[definition] = value; }
    void removeGoboValue(const QString &definition) { m_goboValues.remove(definition); }
    void setGoboValues(const QMap<QString, int> &values) { m_goboValues = values; }

    /** "Manufacturer/Model": what goboValues() are keyed by. */
    static QString definitionKey(const Fixture *fixture);

    /** How a Gobo look found its slot on a fixture's wheel. */
    enum GoboMatch
    {
        GoboNoMatch = 0,    //!< nothing of that name: the fixture ignores it
        GoboPicked,         //!< the slot picked for its type (goboValues())
        GoboRaw,            //!< unnamed: value() as it always was
        GoboExact,          //!< same name (case, space, '-' and '_' ignored)
        GoboSingular,       //!< "Dot" = "Dots" = "Gobo 3 Dots"
        GoboPartial,        //!< the one slot whose name starts with/contains it
        GoboAmbiguous       //!< a tier found several: not guessed, ignored
    };

    struct GoboResolution
    {
        int value = -1;                 //!< DMX value, -1: not applied
        GoboMatch how = GoboNoMatch;
        QStringList candidates;         //!< GoboAmbiguous: the slots' names
    };

    /** What this Gobo look puts on @a fixture's gobo channel (the first
     *  wheel) and how it got there. In order: the slot picked for its
     *  definition; unnamed, value(); named, matchGoboName() over the wheel's
     *  capabilities (its middle value). */
    GoboResolution resolveGobo(const Fixture *fixture) const;
    /** resolveGobo()'s value: -1 when the look doesn't apply to @a fixture. */
    int goboValueFor(const Fixture *fixture) const { return resolveGobo(fixture).value; }

    /** The name match, in tiers; the first tier that finds anything decides:
     *  1. exact, ignoring case, spaces, '-' and '_' (a slot's name, or its
     *     picture as "ClayPaky/gobo00006" -- the folder counts: Chauvet's
     *     gobo00006 is other glass);
     *  2. singular/plural, ignoring the word "gobo" and numbers ("Dot" =
     *     "Dots" = "Gobo 3 Dots");
     *  3. the one slot whose name starts with or contains it (3+ letters).
     *  Tiers 2 and 3 compare slot names only, never pictures, and are not
     *  tried for a picture name (one with a '/'). @a gobos: each slot's
     *  names, its own name first, then its picture's. Returns the slot's
     *  index, or -1: none (@a how GoboNoMatch), or more than one in the
     *  deciding tier (GoboAmbiguous, their indexes in @a ambiguous). The
     *  same name on several slots is one gobo (its first slot). */
    static int matchGoboName(const QString &wanted, const QList<QStringList> &gobos,
                             GoboMatch *how = nullptr, QList<int> *ambiguous = nullptr);
    /** Whether two gobo names are the same gobo ("Break-up" = "breakup"). */
    static bool sameGoboName(const QString &a, const QString &b);

private:
    QString m_goboName;
    QMap<QString, int> m_goboValues;

    /************************************************************************
     * Colour wheels and colour macros (type == Color)
     ************************************************************************/
public:
    /** A slot this Colour look means by NAME on colour-macro channels: a
     *  mover's colour ring ("Red/Yellow-Chase", "RGB/Random-Chase") has
     *  slots that are no single colour, so no colour picks them. Empty: the
     *  look's colour picks each wheel's/macro's nearest slot (as it always
     *  did). A channel with no slot of that name takes the nearest to the
     *  look's colour instead. */
    QString colourMacro() const { return m_colourMacro; }
    void setColourMacro(const QString &name) { m_colourMacro = name; }

    /** The one colour a wheel/macro slot shows: its picture colour, else the
     *  colour its name says ("Blue", "Light Blue", "off" = black, "Open" =
     *  white). Invalid for a slot that is no single colour: a split
     *  ("Blue/Purple"), a chase, a rainbow, a random. */
    static QColor capabilityColour(const QLCCapability *cap);

    /** The DMX value (a slot's middle) of the slot on colour wheel/macro
     *  @p ch nearest to @p col, or -1 when it has no slot of one colour.
     *  Black is "off" where the channel has one (else the open/white slot);
     *  a grey or white is the white slot; anything else is the slot of the
     *  nearest HUE -- RGB distance alone made black green and pink white.
     *  @p slotName, when given, gets the slot's name. */
    static int nearestColourValue(const QLCChannel *ch, const QColor &col,
                                  QString *slotName = nullptr);

    /** The DMX value of the slot named @p name on @p ch (case, spaces, '-'
     *  and '_' ignored; its middle), or -1. */
    static int namedSlotValue(const QLCChannel *ch, const QString &name);

    /** What this Colour look puts on colour wheel/macro channel @p ch: the
     *  slot named colourMacro() if the channel has one, else the nearest to
     *  @p col (nearestColourValue()). */
    int colourSlotValue(const QLCChannel *ch, const QColor &col) const;

    /** The colour wheel/macro channels a Colour look sets on @p fixture's
     *  head @p head: the head's own when it mixes no colour (no RGB set, no
     *  CMY); empty for a head that mixes its colour. @p head -1: the ones in
     *  no head, which belong to the whole fixture (a ring channel left out
     *  of every head) -- only on a fixture that mixes colour nowhere, since
     *  an RGB par's "colour macros" channel would override its mix. */
    static QList<quint32> colourSlotChannels(const Fixture *fixture, int head);

private:
    QString m_colourMacro;

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

    /** The WLED effect this look runs ON WLED fixtures ("Chase", WLED's own
     *  name; empty for none). Those fixtures are sent the effect, and the
     *  look's speed/intensity/palette/option and colours, on their own
     *  channels -- the device runs it. Every other fixture runs the script
     *  above: the port of that effect where there is one. */
    QString wledEffect() const { return m_wledEffect; }
    void setWledEffect(const QString &name) { m_wledEffect = name; }

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
    void removeEffectParamValue(const QString &name)
        { m_effectParamValues.remove(name); }
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
    QString m_wledEffect;
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
