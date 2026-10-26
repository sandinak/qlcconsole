/*
  Q Light Controller Plus
  fixture.h

  Copyright (C) Heikki Junnila
                Massimo Callegari

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

#ifndef FIXTURE_H
#define FIXTURE_H

#include <QObject>
#include <atomic>
#include <QMutex>
#include <QAtomicInt>
#include <QList>
#include <QIcon>
#include <QMap>

#include "qlcchannel.h"
#include "fixturepower.h"
#include "qlcfixturedef.h"
#include "rigowner.h"
#include "qlcengine_export.h"

class QString;

class QLCFixtureDefCache;
class ChannelModifier;
class QLCFixtureMode;
class QLCFixtureHead;
class FixtureConsole;
class SceneValue;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLFixture                 QStringLiteral("Fixture")
#define KXMLFixtureName             QStringLiteral("Name")
#define KXMLFixtureUniverse         QStringLiteral("Universe")
#define KXMLFixtureCrossUniverse    QStringLiteral("CrossUniverse")
#define KXMLFixtureAddress          QStringLiteral("Address")
#define KXMLFixturePatched          QStringLiteral("Patched")
#define KXMLFixtureConnection       QStringLiteral("Connection")
#define KXMLFixtureIPAddress        QStringLiteral("IPAddress")
#define KXMLFixtureTransport        QStringLiteral("Transport")
#define KXMLFixtureID               QStringLiteral("ID")
#define KXMLFixtureGeneric          QStringLiteral("Generic")
#define KXMLFixtureRGBPanel         QStringLiteral("RGBPanel")
#define KXMLFixtureChannels         QStringLiteral("Channels")
#define KXMLFixtureDimmer           QStringLiteral("Dimmer")
#define KXMLFixtureExcludeFade      QStringLiteral("ExcludeFade")
#define KXMLFixtureForcedHTP        QStringLiteral("ForcedHTP")
#define KXMLFixtureForcedLTP        QStringLiteral("ForcedLTP")
#define KXMLFixtureStageLook        QStringLiteral("StageLook")
#define KXMLFixtureOutputRules      QStringLiteral("OutputRules")
#define KXMLFixtureOwner            QStringLiteral("Owner")

#define KXMLFixtureChannelModifier  QStringLiteral("Modifier")
#define KXMLFixtureChannelIndex     QStringLiteral("Channel")
#define KXMLFixtureModifierName     QStringLiteral("Name")

typedef struct
{
    bool m_hasAlias;        /** Flag to enable/disable aliases check */
    QLCCapability *m_currCap; /** The current capability in use */
} ChannelAlias;

class QLC_ENGINE_EXPORT Fixture final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Fixture)

    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(int type READ type CONSTANT)
    Q_PROPERTY(quint32 universe READ universe WRITE setUniverse NOTIFY changed)
    Q_PROPERTY(quint32 address READ address WRITE setAddress NOTIFY changed)
    Q_PROPERTY(quint32 channels READ channels WRITE setChannels NOTIFY changed)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    /** Create a new fixture instance with the given QObject parent. */
    Fixture(QObject* parent = 0);

    /** Destructor */
    ~Fixture();

    /** Less-than operator for qSort() */
    bool operator<(const Fixture& fxi);

signals:
    /** Emitted whenever a Fixture property is changed */
    void changed(quint32);

    /*********************************************************************
     * Fixture ID
     *********************************************************************/
public:
    /**
     * Change the fixture instance's fixture ID. This is generally VERY
     * dangerous, since all functions (using this fixture) will cease to
     * work unless their fixture references are changed as well. Also, the
     * fixture will have to be moved within Doc's fixture array to reflect
     * the new ID, which is essentially the fixture's position in the array.
     *
     * @param id The new fixture id
     */
    void setID(quint32 id);

    /**
     * Get the fixture instance's fixture ID.
     *
     * @return The instance's fixture ID
     */
    quint32 id() const;

    /**
     * Get the invalid fixture ID (for comparison etc...)
     */
    static quint32 invalidId();

protected:
    /** Fixture ID */
    quint32 m_id;

    /*********************************************************************
     * Name
     *********************************************************************/
public:
    /**
     * Change the fixture instance's friendly name.
     *
     * @param name The new name to assign for the instance
     */
    void setName(const QString& name);

    /**
     * Get the fixture instance's friendly name.
     *
     * @return The instance's friendly name
     */
    QString name() const;

protected:
    /** Friendly name */
    QString m_name;

    /*********************************************************************
     * Fixture type
     *********************************************************************/
public:
    /**
     * Get the fixture's type as a string.
     *
     * @return Fixture type
     */
    QString typeString();

    QLCFixtureDef::FixtureType type() const;

    /*********************************************************************
     * Universe
     *********************************************************************/
public:
    /** How a fixture is fed. Not a property of the fixture definition (none
     *  carries it); the LD chooses it per fixture instance. */
    enum Connection
    {
        Dmx     = 0,    //!< DMX, through a converter node or wireless receiver
        Network = 1     //!< speaks ArtNet/sACN itself: its own endpoint, its own IP
    };

    /** False while the fixture is staged in the inventory. An unpatched
     *  fixture keeps whatever universe()/address() it holds (default 0/0) but
     *  those are NOT real: every DMX write path must skip it. Default true, so
     *  every fixture in an existing workspace stays exactly as it was. */
    bool isPatched() const { return m_patched; }
    void setPatched(bool patched) { m_patched = patched; }

    /** An ADDRESS CLASH: the file patched this fixture onto channels another
     *  fixture already held. It used to be dropped from the show on load --
     *  silently, with everything that referred to it. Now it loads unpatched
     *  (so it writes no DMX over the fixture it clashes with) but keeps the
     *  universe()/address() it asked for, and is SAVED as patched there, so
     *  the file is unchanged until someone gives it a home. Runtime only;
     *  cleared by Doc::patchFixtures() and Doc::unpatchFixture(). */
    bool addressClash() const { return m_addressClash; }
    void setAddressClash(bool clash) { m_addressClash = clash; }

    Connection connection() const { return Connection(m_connection); }
    void setConnection(Connection c) { m_connection = int(c); }

    /** Unicast target of a Network fixture; empty when unset or DMX. */
    QString ipAddress() const { return m_ipAddress; }
    void setIPAddress(const QString &ip) { m_ipAddress = ip; }

    /** How a Network fixture is sent to: Art-Net (or sACN) DMX, or WLED's
     *  own JSON API -- its look's state, sent only when it changes, for WLED
     *  on Wi-Fi (the WLED output plugin). The patch planner gives each its
     *  universe's output accordingly. Meaningless for a DMX fixture. */
    enum Transport { ArtNetTransport = 0, WledTransport = 1 };
    Transport transport() const { return Transport(m_transport); }
    void setTransport(Transport t) { m_transport = int(t); }

    /** What it draws, as measured, and the most it may (FixturePower). */
    PowerProfile powerProfile() const { return m_powerProfile; }
    void setPowerProfile(const PowerProfile &profile);

    /**
     * How this fixture's output is treated, beyond what its channels say --
     * for the circuits an Eos show patches as plain dimmers: power relays,
     * house, work, hall and backstage-blue lights (OutputRuleKeeper).
     *
     * ExemptBlackout:    Blackout (and, by default, Blind) leaves it as it is.
     * ExemptGrandMaster: the Grand Master does not scale it.
     * Switch:            a relay: on at 50% and over, off below; never fades.
     *                    The Grand Master cannot switch it off.
     * OnAtOpen:          full whenever nothing in the show sets it (when that
     *                    applies is the OutputRuleKeeper's setting).
     */
    enum OutputRule
    {
        ExemptBlackout    = 1 << 0,
        ExemptGrandMaster = 1 << 1,
        Switch            = 1 << 2,
        OnAtOpen          = 1 << 3
    };
    Q_DECLARE_FLAGS(OutputRules, OutputRule)

    /** Set the rules; emits changed() when they differ (the output follows) */
    void setOutputRules(OutputRules rules);
    OutputRules outputRules() const { return OutputRules(m_outputRules.loadRelaxed()); }

    /** "Blackout,GrandMaster,Switch,OnAtOpen" (as saved) and back */
    static QString outputRulesToString(OutputRules rules);
    static OutputRules stringToOutputRules(const QString &str);

    /**
     * Set the fixture instance's DMX universe
     *
     * @param universe A zero-based DMX universe (i.e. 0-7; not 1-8)
     */
    void setUniverse(quint32 universe);

    /**
     * Get the fixture instance's DMX universe
     *
     * @return A zero-based DMX address (i.e. 0-511; not 1-512)
     */
    quint32 universe() const;

    /** Set if this Fixture crosses a DMX universe
     *
     *  @param cross-universe enable flag
     */
    void setCrossUniverse(bool enable);

    /**
     * Get the fixture cross universe flag
     *
     * @return true if this Fixture crosses a universe
     */
    bool crossUniverse() const;

    /*********************************************************************
     * Address
     *********************************************************************/
public:
    /**
     * Set the fixture instance's DMX address
     *
     * @param address A zero-based DMX address (i.e. 0-511; not 1-512)
     */
    void setAddress(quint32 address);

    /**
     * Get the fixture instance's DMX address
     *
     * @return A zero-based DMX address (i.e. 0-511; not 1-512)
     */
    quint32 address() const;

public:
    /**
     * Get the fixture instance's DMX address & universe as one
     *
     * @return The fixture's address & universe
     */
    quint32 universeAddress() const;

    /*********************************************************************
     * Channels
     *********************************************************************/
public:
    /**
     * Change the number of channels. Valid only for generic dimmers, whose
     * $fixtureDef == NULL && $fixtureMode == NULL.
     *
     * @param channels The new number of channels
     */
    void setChannels(quint32 channels);

    /**
     * Get the number of channels occupied by this fixture instance.
     * This takes also the selected mode into account, as different modes
     * can have different channel sets.
     *
     * @return Number of channels
     */
    quint32 channels() const;

    /**
     * Get a specific channel object by the channel's number. For generic
     * dimmers, the returned QLCChannel is the same for all channel numbers.
     *
     * @param channel The channel number to get
     * @return A QLCChannel* instance that should not be modified
     */
    const QLCChannel* channel(quint32 channel) const;

    /**
     * Get a fixture's channel's DMX address.
     *
     */
    quint32 channelAddress(quint32 channel) const;

    /**
     * Get a channel from the given group of channels and by its primary color
     *
     * @param group Group name of the channel
     * @param colour Primary color to search for
     * @return The first matching channel number
     */
    quint32 channel(QLCChannel::Group group,
        QLCChannel::PrimaryColour color = QLCChannel::NoColour) const;

    /**
     * Get a set of channels from the given group of channels and by their primary color
     *
     * @param group Group name of the channel
     * @param color Primary color to search for
     * @return A QSet containing the matching channel numbers
     */
    QSet <quint32> channels(
                    QLCChannel::Group group,
                    QLCChannel::PrimaryColour color = QLCChannel::NoColour) const;

    /** @see QLCFixtureHead */
    quint32 channelNumber(int type, int controlByte, int head = 0) const;

    /** @see QLCFixtureMode */
    quint32 masterIntensityChannel() const;

    /** @see QLCFixtureHead */
    QVector <quint32> rgbChannels(int head = 0) const;

    /** Ordered RGB channel SETS for a head (primary first). A head may expose
     *  several RGB triples (e.g. WLED primary/secondary/tertiary effect colours);
     *  rgbChannels() returns the first. See QLCFixtureHead::rgbChannelSets(). */
    QVector<QVector<quint32> > rgbChannelSets(int head = 0) const;

    /** @see QLCFixtureHead */
    QVector <quint32> cmyChannels(int head = 0) const;

    /** Return a list of DMX values based on the given position degrees
     *  and the provided type (Pan or Tilt) */
    QList<SceneValue> positionToValues(int type, float degrees, bool isRelative = false);

    /** Return a list of DMX values based on the given zoom degrees */
    QList<SceneValue> zoomToValues(float degrees, bool isRelative);

    /** Set a list of channel indices to exclude from fade transitions */
    void setExcludeFadeChannels(QList<int> indices);

    /** Get the list of channel indices to exclude from fade transitions */
    QList<int> excludeFadeChannels();

    /** Add a channel index to exclude from fade transitions */
    void setChannelCanFade(int idx, bool canFade);

    /** Check if a channel can be faded or not */
    bool channelCanFade(int index);

    /** Set a list of channel indices that are forced to be HTP */
    void setForcedHTPChannels(QList<int> indices);

    /** Get a list of channel indices that are forced to be HTP */
    QList<int> forcedHTPChannels();

    /** Set a list of channel indices that are forced to be LTP */
    void setForcedLTPChannels(QList<int> indices);

    /** Get a list of channel indices that are forced to be LTP */
    QList<int> forcedLTPChannels();

    /** Set a ChannelModifier to the channel with the given $idx */
    void setChannelModifier(quint32 idx, ChannelModifier *mod);

    /** Get the ChannelModifier for the channel with the given $idx.
     *  Returns NULL if no modifier has been assigned */
    ChannelModifier *channelModifier(quint32 idx);

protected:
    /** Find and store channel numbers (pan, tilt, intensity) */
    void findChannels();

protected:
    /** DMX address & universe */
    quint32 m_address;

    /** False for a fixture staged in the inventory: it exists in the show
     *  (placeable, groupable, usable in looks) but holds NO address, is
     *  registered in no universe and must never be written to DMX. */
    bool m_patched;
    bool m_addressClash;

    int m_connection;
    QString m_ipAddress;
    int m_transport = ArtNetTransport;
    PowerProfile m_powerProfile;
    /** OutputRules; read by the fader thread (FadeChannel), set on the UI's */
    QAtomicInt m_outputRules { 0 };

    /** Flag that indicates if this fixture crosses a DMX Universe */
    bool m_crossUniverse;

    /** Number of channels (ONLY for dimmer fixtures!) */
    quint32 m_channels;

    /** List holding the channels indices to exlude from fade transitions */
    QList<int> m_excludeFadeIndices;

    /** List holding the LTP channels indices that are forced to be HTP */
    QList<int> m_forcedHTPIndices;

    /** List holding the HTP channels indices that are forced to be LTP */
    QList<int> m_forcedLTPIndices;

    /** Hash holding the pair <channel index, modifier pointer>
     *  This is basically the place to store them to be saved/loaded
     *  on the project XML file */
    QMap<quint32, ChannelModifier*> m_channelModifiers;

    /*********************************************************************
     * Channel info
     *********************************************************************/
public:
    /** Store DMX values for this fixture. If values have changed,
     * it returns true, otherwise false */
    bool setChannelValues(const QByteArray &values);

    /** What an UNPATCHED fixture would show: a value for channel @p channel,
     *  for display only (the rig view, the plot) -- it has no address, so
     *  nothing goes out. The engine hands these over in place of the DMX it
     *  writes for a patched fixture ("is it possible to visualize w/o
     *  patching?"). Ignored for a patched fixture, whose values come from its
     *  universe. */
    void setPreviewValue(quint32 channel, uchar value);

    /** Return the current DMX values of this fixture */
    QByteArray channelValues();

    /** Retrieve the DMX value of the given channel index */
    uchar channelValueAt(int idx);

    /** Check if some alias has changed on channel $chIndex for $value */
    void checkAlias(int chIndex, uchar value);

signals:
    void valuesChanged();
    void aliasChanged();

protected:
    /** Runtime array to store DMX values and check for changes */
    QByteArray m_values;

    /** Runtime array to check for alias changes */
    QVector<ChannelAlias> m_aliasInfo;
    QMutex m_channelsInfoMutex;

    /*********************************************************************
     * Fixture definition
     *********************************************************************/
public:
    /**
     * Change the fixture's definition & mode. Use this when changing an
     * existing fixture to use another def.
     *
     * @param fixtureDef The new fixture definition
     * @param fixtureMode The new fixture mode (member of $fixtureDef)
     */
    void setFixtureDefinition(QLCFixtureDef *fixtureDef,
                              QLCFixtureMode *fixtureMode);

    /**
     * Get the fixture definition that this fixture instance is based on.
     *
     * @return A QLCFixture definition
     */
    QLCFixtureDef *fixtureDef() const;

    /**
     * Get the fixture mode that this fixture instance is based on.
     *
     * @return A QLCFixtureMode definition
     */
    QLCFixtureMode* fixtureMode() const;

    /**
     * Return the number of heads used by the fixture. If the fixture is a
     * generic dimmer, this returns the number of channels (assuming each one
     * controls one lamp == head). Otherwise returns the number of heads defined
     * in fixtureMode().
     *
     * @return Number of heads
     */
    int heads() const;

    /**
     * Get the fixture head at the given index. If $index is invalid, returns NULL.
     * Each fixture has at least one head. Dimmer fixtures have no heads since each
     * channel can be treated as a head.
     *
     * @param index The index of the head to return
     * @return The head at the given index or NULL
     */
    QLCFixtureHead head(int index) const;

    /** Its icon: its stage look's when one is set, else its type's. */
    Q_INVOKABLE QString iconResource(bool svg = false) const;

    QIcon getIconFromType() const;

    /** Where this fixture files under in a manufacturer -> model hierarchy (the
     *  inventory trees). A generic dimmer or RGB panel has no real manufacturer
     *  or model, so it files under "Generic" with a model that says what it is
     *  ("Dimmer, 4 ch", "RGB panel"), rather than a bare "Generic / Generic". */
    QString groupingManufacturer() const;
    QString groupingModel() const;

    /*********************************************************************
     * Stage look
     *********************************************************************/
public:
    /** What the fixture looks like on stage: its icon in the fixture lists
     *  and how the rig view (the 2D Monitor) and the studio's flat views
     *  draw it. AutoLook, the default, follows the definition's type as
     *  before; the others are the LD's word for it, for a definition whose
     *  type says too little ("Color Changer" covers a PAR, a staff and a
     *  bar). It changes nothing about how the fixture is driven. */
    enum StageLook
    {
        AutoLook = 0,
        FollowspotLook,
        MovingSpotLook,
        MovingRoundWashLook,
        MovingSquareWashLook,
        LanternLook,            ///< a practical: a hurricane or porch lantern, glowing all round
        StaffLook,              ///< a practical: a pole with LED strip wound round it
        SquareWashLook,
        BarWashLook,            ///< a linear LED bar or batten
        UplightWashLook,
        ParCanLook,             ///< a conventional PAR: a round can
        EllipsoidalLook,        ///< a fixed profile spot: barrel, lens, shutters
        FresnelLook,            ///< a short round body, a stepped lens
        LedParLook,             ///< a round multi-LED wash
        CycStripLook,           ///< a cyc / strip light: a wide flood of cells
        StrobeLook,             ///< a strobe or blinder
        CurtainLook             ///< scenery: drives a curtain (Scenery::isCurtainFixture())
        /* New looks go at the END: the int is runtime only (the file holds
           stageLookToString()'s key), but tests and pickers walk the enum. */
    };
    /** The picker's sections. */
    enum StageLookSection { MovingSection, ConventionalSection, LedSection, PracticalSection,
                            ScenerySection };
    Q_ENUM(StageLook)

    /** Read without a lock: the rig views classify fixtures on worker
     *  threads while the GUI thread may be changing it. */
    StageLook stageLook() const { return StageLook(m_stageLook.load(std::memory_order_relaxed)); }
    /** Set the look; emits changed() when it differs. */
    void setStageLook(StageLook look);

    /** Every look, Auto first, then section by section (Moving, Fixed
     *  conventional, Fixed LED, Practicals, Scenery) in the order a picker
     *  lists them. */
    static QList<StageLook> stageLooks();
    /** The picker section @p look is listed under (Auto: none, -1). */
    static int stageLookSection(StageLook look);
    /** A section's name for people ("Fixed LED"), translated. */
    static QString stageLookSectionName(StageLookSection section);
    /** The look closest to what the definition says this is: a Color
     *  Changer is an LED PAR, an LED bar a bar wash, a Strobe a strobe, a
     *  moving head a spot or a wash by its gobo / zoom channels or its name,
     *  a dimmer (or a generic dimmer channel) an ellipsoidal. AutoLook when nothing is close (a hazer, a fan).
     *  What "Auto (...)" names in a picker, and what an Auto fixture is
     *  drawn as when the show says so (MonitorProperties::autoStageLooks()).
     *  Worked out when the definition is set, so reading it is free -- the
     *  rig views read it per fixture per frame, on worker threads too. */
    StageLook autoStageLook() const { return StageLook(m_autoStageLook.load(std::memory_order_relaxed)); }
    /** The XML / stable key of @p look ("Auto", "Followspot", ...). */
    static QString stageLookToString(StageLook look);
    /** The look for @p key; AutoLook for anything unknown (a newer file). */
    static StageLook stringToStageLook(const QString &key);
    /** Its name for people ("Moving head spot"), translated. */
    static QString stageLookName(StageLook look);
    /** Its icon resource (":/look_staff.png", or with @p svg
     *  "qrc:/look_staff.svg"); empty for AutoLook. */
    static QString stageLookIconResource(StageLook look, bool svg = false);
    /** The type-derived icon, ignoring any stage look. */
    QString typeIconResource(bool svg = false) const;

protected:
    std::atomic<int> m_stageLook { AutoLook };
    /** autoStageLook(), cached by setFixtureDefinition(). No definition: a
     *  generic dimmer channel, an ellipsoidal. */
    std::atomic<int> m_autoStageLook { EllipsoidalLook };
    StageLook computeAutoStageLook() const;

    /*********************************************************************
     * Owner (board Q137, VENUES.md)
     *********************************************************************/
public:
    /** Who it belongs to, as set on the fixture itself: Tour, House or
     *  Rental, or Unset (default) = from its group (RigOwner::effective()). */
    RigOwner::Owner owner() const { return RigOwner::Owner(m_owner.load(std::memory_order_relaxed)); }
    /** Set it; emits changed() when it differs. */
    void setOwner(RigOwner::Owner owner);

protected:
    std::atomic<int> m_owner { RigOwner::Unset };

public:
    QRectF degreesRange(int head) const;

    /** Its definition could not be found when the show loaded -- in cloud
     *  storage and not on this computer, or simply missing -- so it stands
     *  in as a generic dimmer of the same channel count. It REMEMBERS what it
     *  is: saving writes back the real manufacturer, model and mode, never
     *  "Generic", so a show saved in that state loses nothing. Giving it a
     *  definition (setFixtureDefinition()) clears this. */
    bool definitionUnavailable() const { return !m_unavailableModel.isEmpty(); }
    QString unavailableManufacturer() const { return m_unavailableManufacturer; }
    QString unavailableModel() const { return m_unavailableModel; }
    QString unavailableMode() const { return m_unavailableMode; }

protected:
    QString m_unavailableManufacturer, m_unavailableModel, m_unavailableMode;

    /** The fixture definition that this instance is based on */
    QLCFixtureDef* m_fixtureDef;

    /** The mode within the fixture definition that this instance uses */
    QLCFixtureMode* m_fixtureMode;

    /*********************************************************************
     * Generic Dimmer
     *********************************************************************/
public:
    /** Creates and returns a definition for a generic dimmer pack */
    QLCFixtureDef *genericDimmerDef(int channels);

    /** Creates and returns a fixture mode for a generic dimmer pack */
    QLCFixtureMode *genericDimmerMode(QLCFixtureDef *def, int channels);

    /*********************************************************************
     * Generic RGB panel
     *********************************************************************/
public:
    enum Components {
        RGB = 0,
        BGR,
        BRG,
        GBR,
        GRB,
        RGBW,
        RBG
    };
    Q_ENUM(Components)

protected:
    QString componentsToString(Components comp, bool is16bit);
    Components stringToComponents(QString str, bool &is16bit);

public:
    /** Creates and returns a definition for a generic RGB panel row */
    QLCFixtureDef *genericRGBPanelDef(int columns, Components components, bool is16bit);

    /** Creates and returns a fixture mode for a generic RGB panel row */
    QLCFixtureMode *genericRGBPanelMode(QLCFixtureDef *def, Components components, bool is16bit,
                                        quint32 width, quint32 height);

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Load a fixture from the given XML node and attempt to add it to
     * the given QLC Doc instance.
     *
     * @param root The Fixture node to load from
     * @param doc The doc that owns all fixtures
     */
    static bool loader(QXmlStreamReader &root, Doc* doc);

    /**
     * Load a fixture's contents from the given XML node.
     *
     * @param root An XML subtree containing a single fixture instance
     * @return true if the fixture was loaded successfully, otherwise false
     */
    bool loadXML(QXmlStreamReader &xmlDoc, Doc* doc,
                 QLCFixtureDefCache* fixtureDefCache);

    /**
     * Save the fixture instance into an XML document, under the given
     * XML element (tag).
     *
     * @param doc The master XML document to save to.
     * @param wksp_root The workspace root element
     */
    bool saveXML(QXmlStreamWriter *doc) const;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Fixture::OutputRules)

/** @} */

#endif

