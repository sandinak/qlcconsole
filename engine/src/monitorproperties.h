/*
  Q Light Controller Plus
  monitorproperties.h

  Copyright (c) Massimo Callegari

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

#ifndef MONITORPROPERTIES_H
#define MONITORPROPERTIES_H

#include <QObject>
#include <QMutex>
#include "qlcengine_export.h"
#include <functional>
#include <atomic>
#include <QVector3D>
#include <QPointF>
#include <QColor>
#include <QFont>
#include <QSize>
#include <QMap>

#include "truss.h"
#include "measurementunits.h"
#include "headlayout.h"
#include "stageplatform.h"
#include "pipe.h"
#include "hanger.h"
#include "scenery.h"
#include "beamocclusion.h"
#include "stand.h"
#include "tower.h"
#include "stagetarget.h"

class QXmlStreamReader;
class QXmlStreamWriter;

class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCMonitorProperties QStringLiteral("Monitor")

// Named structs, not typedef'd anonymous ones: the default member
// initialisers below make them non-C types, which GCC rejects as typedef
// names for linkage purposes (-Wnon-c-typedef-for-linkage).
struct PreviewItem
{
    QVector3D m_position;       ///< 3D item position
    QVector3D m_rotation;       ///< 3D item rotation
    QVector3D m_scale;          ///< 3D item scale
    QString m_name;             ///< Fixture/Item Custom name
    QString m_resource;         ///< Generic: source file
    QColor m_color;             ///< Generic: item color, Fixture: gel color
    int m_zoom;                 ///< Fixture: fixed zoom in degrees
    quint32 m_flags;            ///< Item flags as specified in the ItemsFlags enum
    quint32 m_layerId = 0;      ///< 2D-map organizational layer (0 = Default)
    quint32 m_groupId = 0;      ///< 2D-map group (0 = ungrouped)
};

struct FixturePreviewItem
{
    PreviewItem m_baseItem;                 ///< Base fixture item properties
    QMap<quint32, PreviewItem> m_subItems;  ///< Map of the heads/linked fixtures
};

class QLC_ENGINE_EXPORT MonitorProperties final : public QObject
{
    Q_OBJECT

public:
    MonitorProperties();
    ~MonitorProperties();

    enum DisplayMode { DMX, Graphics };
    enum ChannelStyle { DMXChannels, RelativeChannels };
    enum ValueStyle { DMXValues, PercentageValues };

    /** Get/Set the font used by the Monitor dialog UI */
    inline void setFont(QFont font) { m_font = font; }
    inline QFont font() const { return m_font; }

    /** Get/Set how to display the monitor */
    inline void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }
    inline DisplayMode displayMode() const { return m_displayMode; }

    /** Get/Set how to show DMX channel indices in DMX display mode */
    inline void setChannelStyle(ChannelStyle style) { m_channelStyle = style; }
    inline ChannelStyle channelStyle() const { return m_channelStyle; }

    /** Get/Set how to show DMX channel values in DMX display mode */
    inline void setValueStyle(ValueStyle style) { m_valueStyle = style; }
    inline ValueStyle valueStyle() const { return m_valueStyle; }

    /** Reset all the Monitor properties */
    void reset();

    /** The whole rig as XML (what saveXML() writes), for undo and Cancel. */
    QByteArray snapshot(const Doc *mainDocument) const;

    /** Put the rig back as a snapshot() had it. The structures it replaces
     *  are not deleted at once but retired for a while: the DMX thread looks
     *  stage targets and rig props up while a show runs, and an object it
     *  just found must not vanish under it. Emits rigPropsChanged() for every
     *  fixture and stageTargetsChanged(). */
    bool restoreSnapshot(const QByteArray &snap, const Doc *mainDocument);

private:
    /** Retired by restoreSnapshot(): deleters, and when each was retired. */
    QList<QPair<qint64, std::function<void()> > > m_retired;
    void freeRetired(bool all);

public:

private:
    QFont m_font;
    DisplayMode m_displayMode;
    ChannelStyle m_channelStyle;
    ValueStyle m_valueStyle;

    /********************************************************************
     * Environment
     ********************************************************************/
public:
    enum GridUnits { Meters, Feet };
    Q_ENUM(GridUnits)
    enum PointOfView { Undefined, TopView, FrontView, RightSideView, LeftSideView };
    Q_ENUM(PointOfView)
    enum StageType { StageSimple, StageBox, StageRock, StageTheatre };

    /** Get/Set the size of the grid in 2D display mode */
    inline void setGridSize(QVector3D size) { m_gridSize = size; }
    inline QVector3D gridSize() const { return m_gridSize; }
    /** Centre stage, on the floor, in world metres: the middle of the stage
     *  grid. (World 0,0 is the grid's corner; stageOrigin() only moves where
     *  the rulers count from.) */
    QVector3D stageCentre() const
    {
        const float mpu = m_gridUnits == Feet ? 0.3048f : 1.0f;
        return QVector3D(m_gridSize.x() * mpu / 2.0f, m_gridSize.z() * mpu / 2.0f, 0.0f);
    }

    /** Get/Set the grid measurement units to use in 2D display mode.
     *  The unit is the person's (Settings -> Units, which App applies with
     *  convertGridUnits() on load); a show's saved Units only says what its
     *  saved grid size is in. setGridUnits() relabels without converting. */
    inline void setGridUnits(GridUnits units) { m_gridUnits = units; }
    /** Change unit, keeping the stage the same physical size (the grid size
     *  is converted exactly, positions are mm and untouched) */
    void convertGridUnits(GridUnits units);
    /** The grid unit for a preference: metres for mm/cm/m, feet for in/ft */
    static GridUnits gridUnitsFor(MeasurementUnits::LengthUnit unit);
    inline GridUnits gridUnits() const { return m_gridUnits; }

    /** Get/Set the point of view to render the 2D preview */
    void setPointOfView(PointOfView pov);
    inline PointOfView pointOfView() const { return m_pointOfView; }

    /** Get/Set the type of stage to render in the 3D preview */
    inline void setStageType(StageType type) { m_stageType = type; }
    inline StageType stageType() const { return m_stageType; }

    /** Get/Set the layout lock state in 2D display mode.
     *  When locked, fixtures cannot be moved but can still be selected */
    inline void setLayoutLocked(bool locked) { m_layoutLocked = locked; }
    inline bool layoutLocked() const { return m_layoutLocked; }

    /** Get/Set the number of grid sub-divisions to render in 2D display mode */
    inline void setGridSubdivisions(int subdivisions) { m_gridSubdivisions = subdivisions; }
    inline int gridSubdivisions() const { return m_gridSubdivisions; }

    /** Get/Set the snap-to-grid level in 2D display mode.
     *  0 = off, 1 = full cell, 2 = 1/2 cell, 4 = 1/4 cell */
    inline void setSnapDivisions(int divisions) { m_snapDivisions = divisions; }
    inline int snapDivisions() const { return m_snapDivisions; }

    /** Default height (metres) above the floor/deck that target-aimed fixtures
     *  point at — i.e. the subject's body height, so beams land on the chest
     *  rather than the feet. A StageTarget with a non-zero Z overrides this. */
    inline void setAimSubjectHeight(float metres) { m_aimSubjectHeight = metres; }
    inline float aimSubjectHeight() const { return m_aimSubjectHeight; }

    /** Line of sight: which stage features stand in a beam's way
     *  (BeamOcclusion::Kind flags), and how thick a truss, pipe or stand must
     *  be (metres) before it counts. Only read where line of sight is asked
     *  for -- a follow spot's Fade When Blocked, an Aim look's "Dim lights
     *  that can't see the target", the reach warnings and aim traces. */
    inline void setOcclusionKinds(int kinds) { m_occlusionKinds = kinds; }
    inline int occlusionKinds() const { return m_occlusionKinds; }
    inline void setOcclusionMinSize(float metres) { m_occlusionMinSize = qMax(0.0f, metres); }
    inline float occlusionMinSize() const { return m_occlusionMinSize; }
    /** This stage's line-of-sight model as it stands now: rebuilt when
     *  anything that blocks moved, opened or flew since the last call, shared
     *  otherwise. With @p doc, a curtain driven by a fixture is where that
     *  fixture has it now (BeamOcclusion::livePoses()); without, every
     *  curtain is at its fixed state. Safe from any thread. */
    BeamOcclusion::ModelPtr occlusionModel(const Doc *doc = nullptr) const
    { return occlusionModel(BeamOcclusion::livePoses(doc, this)); }
    /** The same with each curtain in @p poses where it says (a view that
     *  draws the curtains where it shows them). */
    BeamOcclusion::ModelPtr occlusionModel(const BeamOcclusion::Poses &poses) const
    { return m_occlusion.get(this, m_occlusionKinds, m_occlusionMinSize, poses); }

    /** The world point (metres, stage coordinates) that the 2D-view rulers and
     *  coordinate readout treat as 0,0. Lets the operator zero measurements at
     *  stage centre / a downstage mark / anywhere. Default (0,0). */
    inline void setStageOrigin(const QPointF &metres) { m_stageOrigin = metres; }
    inline QPointF stageOrigin() const { return m_stageOrigin; }

    /********************************************************************
     * Stage reference marks
     *
     * The marks a crew spikes and focuses by, drawn on the plot and on the
     * Rig Overview's floor: the centre line's mark at the downstage edge
     * (DSC) and, optionally, the whole centre line; the curtain line; the
     * number line, ticked in the show's unit either side of centre; and the
     * apron, the deck downstage of the curtain line, built as its own piece.
     * They are objects on the plot -- the stage pieces, on their own Stage
     * layer in Plot layers -- each shown or hidden and locked on its own as
     * well as with the layer. Per show, saved with it only when used: an
     * older show loads with all of them off. Positions are world metres
     * along Y (downstage +), like every other position on the plot.
     ********************************************************************/
    enum StageMark
    {
        NoStageMarks    = 0,
        CentreMarkFlag  = (1 << 0),   ///< the centre line's mark at the downstage edge
        CentreLineFlag  = (1 << 1),   ///< the whole centre line, dashed, running upstage
        CurtainLineFlag = (1 << 2),   ///< where the main curtain / proscenium sits
        NumberLineFlag  = (1 << 3),   ///< distances left and right of centre
        ApronFlag       = (1 << 4)    ///< the apron, its own deck from the curtain line
    };
    inline int stageMarks() const { return m_stageMarks; }
    void setStageMarks(int marks);
    inline bool stageMarkVisible(StageMark mark) const { return (m_stageMarks & mark) != 0; }
    void setStageMarkVisible(StageMark mark, bool on);
    /** Drawn: turned on, and the Stage layer shown. */
    bool stageMarkShown(StageMark mark) const;

    /** The stage pieces as the plot and Plot layers list them. The centre
     *  piece is the DSC mark and the full centre line together (either or
     *  both, CentreMarkFlag / CentreLineFlag). */
    enum StagePiece { CentrePiece = 0, CurtainPiece = 1, NumberPiece = 2, ApronPiece = 3 };
    static QList<StagePiece> stagePieces();
    static QString stagePieceName(StagePiece piece);
    /** Shown or hidden on its own (the layer's eye aside). Showing the
     *  centre piece brings back the parts it had when it was hidden (the DSC
     *  mark, the first time). Showing any piece makes the Stage layer. */
    bool stagePieceVisible(StagePiece piece) const;
    void setStagePieceVisible(StagePiece piece, bool on);
    /** Locked on its own: not dragged on the plot. */
    bool stagePieceLocked(StagePiece piece) const { return (m_stagePieceLocks & (1 << int(piece))) != 0; }
    void setStagePieceLocked(StagePiece piece, bool on);
    /** Drawn: visible, and the Stage layer shown. */
    bool stagePieceShown(StagePiece piece) const;
    /** Can be dragged: shown, and neither it nor the Stage layer locked. */
    bool stagePieceMovable(StagePiece piece) const;

    /** The layer the stage pieces (and the apron's deck) are on: made the
     *  first time one is shown or Plot layers lists them, and kept with the
     *  show; it can be hidden and locked like any layer but not removed.
     *  0 (the Default layer's id) until it is made. */
    quint32 stageLayerId() const { return m_stageLayerId; }
    bool hasStageLayer() const { return m_stageLayerId != defaultLayerId && m_layers.contains(m_stageLayerId); }
    quint32 ensureStageLayer();

    /** The one grid unit in metres: a foot or a metre. The number line is
     *  ticked in it. */
    inline float gridUnitMetres() const { return m_gridUnits == Feet ? 0.3048f : 1.0f; }

    /** Where the stage's front edge crosses the centre line, in world
     *  metres along Y: the front of the deck (deckFrontY()), or of the apron
     *  piece when that is shown and reaches further downstage. */
    float downstageEdgeY() const;
    /** The front of the deck at the centre line, the apron piece left out:
     *  the biggest square-on platform across the centre line, carried on
     *  downstage by any platform butted against its front across the centre,
     *  a rounded front followed to its middle. With no such platform, the
     *  downstage edge of the stage grid. The curtain line is found, and
     *  measured, from here. */
    float deckFrontY() const;

    /** The curtain line. Automatic until moved: the most downstage main
     *  drape across the centre line, else a proscenium's, else a typical
     *  apron's depth (4 ft, or 1.2 m) upstage of the front of the deck. */
    float curtainLineY() const;
    float autoCurtainLineY() const;
    /** The scenery the automatic curtain line comes from, if any. */
    const Scenery *curtainLineScenery() const;
    inline bool curtainLineIsAuto() const { return !m_curtainLineSet; }
    void setCurtainLineY(float metres);
    void setCurtainLineAuto();

    /** The number line: at the downstage edge until moved. */
    float numberLineY() const;
    inline bool numberLineIsAuto() const { return !m_numberLineSet; }
    void setNumberLineY(float metres);
    inline void setNumberLineAuto() { m_numberLineSet = false; }

    /** The curtain line is the stage's main reference (the plaster line):
     *  its properties set it as how far upstage of the front of the deck
     *  (deckFrontY(), the apron piece left out: the apron starts at the
     *  curtain line) it is, and the number line as how far
     *  downstage of the curtain line; negative the other way. Both are kept,
     *  and saved (CurtainLineY / NumberLineY), as the plan's Y all the same:
     *  a line set where it belongs stays there when a deck is added or moved
     *  or the other line moves (the distance shown changes instead), and a
     *  show saved before reads exactly as it did. */
    inline float curtainLineUpstage() const { return deckFrontY() - curtainLineY(); }
    inline void setCurtainLineUpstage(float metres) { setCurtainLineY(deckFrontY() - metres); }
    inline float numberLineDownstageOfCurtain() const { return numberLineY() - curtainLineY(); }
    inline void setNumberLineDownstageOfCurtain(float metres) { setNumberLineY(curtainLineY() + metres); }

    /** How far downstage of the curtain line a plan Y is (negative upstage),
     *  and that written in crew terms: "4.2 m DS of curtain line",
     *  "1' 6\" US of curtain line", "on the curtain line". For a pipe or a
     *  truss over the audience, like the FOH electric. */
    inline float curtainLineOffset(float y) const { return y - curtainLineY(); }
    static QString formatCurtainLineOffset(float downstage, GridUnits units);
    QString curtainLineOffsetText(float y) const
    { return formatCurtainLineOffset(curtainLineOffset(y), m_gridUnits); }
    /** A distance on its own, as the screen writes it: "1.8 m", "5' 11\"". */
    static QString formatStageDistance(float metres, GridUnits units);

    /** The apron: the stage downstage of the curtain line, as its own
     *  piece. It starts at the curtain line and runs downstage by its depth,
     *  its front rounded by its front curve, as wide and as high as the main
     *  deck (the stage grid's width, on the floor, with none) and in its
     *  colour, and it is drawn and stood on as real deck (apronPlatform()).
     *  It never borrows another platform: a riser butted to the front stays
     *  as it is whatever the apron does. Automatic until set: it reaches the
     *  main deck's front, with the deck's own front curve. */
    inline bool apronAuto() const { return m_apronAuto; }
    void setApronAuto(bool on);
    /** Its depth at centre, from the curtain line (0 when it has none). */
    float apronDepth() const;
    /** The depth it has on Auto: from the curtain line to the main deck's
     *  front (0 or less when the deck stops upstage of the curtain line). */
    float autoApronDepth() const;
    /** Set the apron @p metres deep from the curtain line (Auto off): its
     *  front moves, nothing else does. False for a depth that is not a
     *  finite number of 0 or more. */
    bool setApronDepth(float metres);
    /** How far the middle of its front reaches past the corners (metres). */
    float apronFrontCurve() const;
    /** Set its front curve (Auto off, the depth kept as it is). */
    void setApronFrontCurve(float metres);
    /** The apron's deck: a platform on the Stage layer, made, kept to the
     *  curtain line and resized by syncApron(), and saved with the apron,
     *  not as a platform of its own. Null while the apron is hidden or has
     *  no depth. */
    StagePlatform *apronPlatform() const;
    bool isApronPlatform(quint32 platformId) const
    { return platformId == m_apronPlatformId && m_platforms.contains(platformId); }
    /** Make the apron's deck agree with the apron as it is now: made when
     *  it is shown, taken away when it is hidden, moved with the curtain
     *  line and the main deck. Every stage setter calls it; a view calls it
     *  after the deck or the curtain it is found from is edited. True when
     *  anything changed. */
    bool syncApron();
    /** The deck the stage is: the biggest square-on platform across the
     *  centre line (deckFrontY() starts from its front); null on a bare
     *  floor. Never the apron's own deck. */
    const StagePlatform *mainDeck() const;
    /** The main deck, then the pieces butted on in front of it at the centre
     *  line, upstage to downstage: what deckFrontY() is found from. */
    QList<StagePlatform *> stageDecks() const;

    /** Whether the plot and the Rig Overview write where each target is
     *  beside it (stagePositionText()). On unless turned off; saved with the
     *  show only when off. */
    inline bool targetPositionsVisible() const { return m_targetPositionsVisible; }
    inline void setTargetPositionsVisible(bool on) { m_targetPositionsVisible = on; }
    /** How many whole units the number line reaches either side of centre:
     *  as far as the stage grid goes. */
    int numberLineReach() const;
    /** How the number line is numbered, the same on the plot and the Rig
     *  Overview, mirrored about centre at any zoom: every @return units,
     *  the fewest of 1, 2, 5, 10, 20, 50 that leaves about 22 pixels a
     *  number when one unit is @p unitPixels on screen. */
    static int numberLineStride(double unitPixels);
    /** A long tick at @p k units from centre: every labelled one (every 5
     *  when every unit is labelled), so a long tick never falls between
     *  numbers. */
    static bool numberLineMajorTick(int k, int stride)
    { return stride <= 1 ? (k % 5) == 0 : (k % stride) == 0; }
    /** The label at @p k units from centre: "CL" at centre, so the centre
     *  reads as centre and nothing else; the distance either side. */
    static QString numberLineLabel(int k)
    { return k == 0 ? QStringLiteral("CL") : QString::number(k < 0 ? -k : k); }

    /** Where a point is in crew terms, against the stage marks' positions
     *  (whether or not the marks are shown): across from the centre line
     *  (+ stage left), up or down stage from the reference line (+ upstage),
     *  and how far above the floor. The reference line is the number line,
     *  or the curtain line when that is shown and the number line is not. */
    struct StageOffset
    {
        float across = 0.0f;        ///< metres, + toward stage left
        float upstage = 0.0f;       ///< metres, + upstage of the reference line
        float height = 0.0f;        ///< metres above the floor
        bool fromCurtainLine = false;
    };
    StageOffset stageOffset(const QVector3D &world) const;

    /** How a StageOffset is written. Compact: a label beside a target on
     *  screen, to a tenth of a metre or the nearest inch ("SL 2.4 · US 3.1 m",
     *  "SR 3' 4\" · DS 1' 6\"", "CL · on line"). Spoken: whole units, for a
     *  call or a voice ("SL 2, US 1", "CL, on the number line"). */
    enum OffsetStyle { CompactOffset, SpokenOffset };
    static QString formatStageOffset(const StageOffset &offset, GridUnits units,
                                     OffsetStyle style = CompactOffset);
    /** stageOffset() written in this show's unit. */
    QString stagePositionText(const QVector3D &world, OffsetStyle style = CompactOffset) const
    { return formatStageOffset(stageOffset(world), m_gridUnits, style); }

private:
    QVector3D m_gridSize;
    GridUnits m_gridUnits;
    PointOfView m_pointOfView;
    StageType m_stageType;
    bool m_layoutLocked;
    int m_gridSubdivisions;
    int m_snapDivisions;
    float m_aimSubjectHeight;
    QPointF m_stageOrigin;
    int m_stageMarks = NoStageMarks;
    int m_centreParts = CentreMarkFlag;     ///< what the centre piece shows when shown again
    int m_stagePieceLocks = 0;              ///< 1 << StagePiece
    quint32 m_stageLayerId = 0;             ///< 0: not made yet
    bool m_apronAuto = true;
    float m_apronDepth = 0.0f;              ///< set depth, metres
    float m_apronFrontCurve = 0.0f;         ///< set front curve, metres
    quint32 m_apronPlatformId = 0xFFFFFFFF; ///< its deck's platform id, kept while hidden
    bool m_loadingXml = false;              ///< syncApron() waits for the platforms
    void loadStagePieces(QXmlStreamReader &root);
    void saveStagePieces(QXmlStreamWriter *doc) const;
    bool m_curtainLineSet = false;
    float m_curtainLineY = 0.0f;
    bool m_numberLineSet = false;
    float m_numberLineY = 0.0f;
    bool m_targetPositionsVisible = true;
    int m_occlusionKinds = BeamOcclusion::defaultKinds();
    float m_occlusionMinSize = BeamOcclusion::defaultMinSize();
    mutable BeamOcclusion::Cache m_occlusion;

    /********************************************************************
     * Items flags
     ********************************************************************/
public:
    enum ItemFlags
    {
        HiddenFlag          = (1 << 0),
        InvertedPanFlag     = (1 << 1),
        InvertedTiltFlag    = (1 << 2),
        MeshZUpFlag         = (1 << 3),
        VerticalStripFlag   = (1 << 4)   ///< lay multi-head fixtures out as a vertical strip
    };
    Q_ENUM(ItemFlags)

    /********************************************************************
     * Fixture items
     ********************************************************************/
public:
    /** Get/Set the Fixture labels visibility status */
    inline void setLabelsVisible(bool visible) { m_showLabels = visible; }
    inline bool labelsVisible() const { return m_showLabels; }

    /** Draw fixtures left on Auto as their closest stage look
     *  (Fixture::autoStageLook()) in the plot and the studio, rather than by
     *  their definition's type as always. Per show, saved with it (only when
     *  on), so a plot looks the same on every computer; off by default.
     *  Read lock-free: the rig views classify fixtures on worker threads.
     *  Emits autoStageLooksChanged() when it changes. */
    bool autoStageLooks() const { return m_autoStageLooks.load(std::memory_order_relaxed); }
    void setAutoStageLooks(bool on);

    /** Remove a Fixture entry from the Monitor map by Fixture ID */
    void removeFixture(quint32 fid);

    /** Remove a Fixture entry from the Monitor map by item ID */
    void removeFixture(quint32 fid, quint16 head, quint16 linked);

    quint32 fixtureSubID(quint32 headIndex, quint32 linkedIndex) const;
    quint16 fixtureHeadIndex(quint32 mapID) const;
    quint16 fixtureLinkedIndex(quint32 mapID) const;

    /** Returns true if the Fixture with ID $fid is present in the monitor map */
    inline bool containsFixture(quint32 fid) { return m_fixtureItems.contains(fid); }

    /** Returns true if the provided Fixture ID, head index and linked index are in the map */
    bool containsItem(quint32 fid, quint16 head, quint16 linked);

    /** Get/Set the position of a Fixture with with the given $fid, $head and $linked index */
    void setFixturePosition(quint32 fid, quint16 head, quint16 linked, QVector3D pos);
    QVector3D fixturePosition(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the rotation of a Fixture with with the given $fid, $head and $linked index */
    void setFixtureRotation(quint32 fid, quint16 head, quint16 linked, QVector3D degrees);
    QVector3D fixtureRotation(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the color of a gel used to render a Fixture with with the given $fid, $head and $linked index */
    void setFixtureGelColor(quint32 fid, quint16 head, quint16 linked, QColor col);
    QColor fixtureGelColor(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the fixed zoom degrees used to render a Fixture with with the given $fid, $head and $linked index */
    void setFixtureFixedZoom(quint32 fid, quint16 head, quint16 linked, int degrees);
    int fixtureFixedZoom(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the name of a Fixture with with the given $fid, $head and $linked index */
    void setFixtureName(quint32 fid, quint16 head, quint16 linked, QString name);
    QString fixtureName(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the flags of a Fixture with with the given $fid, $head and $linked index */
    void setFixtureFlags(quint32 fid, quint16 head, quint16 linked, quint32 flags);
    quint32 fixtureFlags(quint32 fid, quint16 head, quint16 linked) const;

    /** Get/Set the 2D-map organizational layer of a whole Fixture (base item). */
    void setFixtureLayer(quint32 fid, quint32 layerId);
    quint32 fixtureLayer(quint32 fid) const;

    /** Get/Set the 2D-map group of a whole Fixture (base item; 0 = ungrouped). */
    void setFixtureGroup(quint32 fid, quint32 groupId);
    quint32 fixtureGroup(quint32 fid) const;

    /** Get/Set all the Fixture item properties of a Fixture with ID $fid */
    inline FixturePreviewItem fixtureProperties(quint32 fid) const { return m_fixtureItems[fid]; }
    inline void setFixtureProperties(quint32 fid, FixturePreviewItem props) { m_fixtureItems[fid] = props; }

    /** Get/Set a single Fixture item property with the given $fid, $head and $linked index */
    PreviewItem fixtureItem(quint32 fid, quint16 head, quint16 linked) const;
    void setFixtureItem(quint32 fid, quint16 head, quint16 linked, PreviewItem props);

    /** Get a list of Fixture IDs currently set in the Monitor */
    QList <quint32> fixtureItemsID() const { return m_fixtureItems.keys(); }

    /** Return a list of the base ID and sub IDs for a fixture with the given $fid */
    QList<quint32> fixtureIDList(quint32 fid) const;

private:
    bool m_showLabels;
    std::atomic<bool> m_autoStageLooks { false };
    QMap <quint32, FixturePreviewItem> m_fixtureItems;

    /********************************************************************
     * Generic items
     ********************************************************************/
public:
    /** Returns true if the item with ID $itemID is present in the monitor map */
    inline bool containsItem(quint32 itemID) { return m_genericItems.contains(itemID); }

    /** Remove an existing item from the generic items map */
    inline void removeItem(quint32 itemID) { m_genericItems.take(itemID); }

    /** Returns a list of all the generic item IDs */
    QList<quint32> genericItemsID();

    /** Get/Set the custom name for an item with ID $itemID */
    QString itemName(quint32 itemID);
    void setItemName(quint32 itemID, QString name);

    /** Get/Set the resource string for an item with ID $itemID */
    QString itemResource(quint32 itemID);
    void setItemResource(quint32 itemID, QString resource);

    /** Get/Set the 3D position of an item with ID $itemID */
    QVector3D itemPosition(quint32 itemID);
    void setItemPosition(quint32 itemID, QVector3D pos);

    /** Get/Set the 3D rotation of an item with ID $itemID */
    QVector3D itemRotation(quint32 itemID);
    void setItemRotation(quint32 itemID, QVector3D rot);

    /** Get/Set the 3D scale of an item with ID $itemID */
    QVector3D itemScale(quint32 itemID);
    void setItemScale(quint32 itemID, QVector3D scale);

    /** Get/Set the flags of an item with ID $itemID */
    quint32 itemFlags(quint32 itemID);
    void setItemFlags(quint32 itemID, quint32 flags);

private:
    QMap <quint32, PreviewItem> m_genericItems;

    /********************************************************************
     * Layers (2D map organization: visibility / lock / stacking)
     ********************************************************************/
public:
    /** A named organizational layer for 2D-map items. Every placeable item
     *  (fixture / truss / platform / target / power source) carries a layer id;
     *  a layer bundles visibility, lock and stacking order for its members.
     *  Layer id 0 is the always-present "Default" layer. */
    struct MonitorLayer
    {
        quint32 id = 0;
        QString name;
        bool    visible = true;
        bool    locked  = false;
        bool    labels  = true;   ///< show item labels for members of this layer
        int     order   = 0;      ///< ascending stacking / list order
    };

    /** Fixed id of the always-present Default layer. */
    static const quint32 defaultLayerId = 0;

    /** All layers, sorted by ascending order then id. Always includes Default. */
    QList<MonitorLayer> layers() const;

    /** Look up a layer by id. Unknown ids resolve to the Default layer so that
     *  an item referencing a deleted layer still renders (as Default). */
    MonitorLayer layer(quint32 id) const;

    /** Locks are per object AND per layer: an object is held still when its
     *  own lock is on or its layer's is. Each object's own Locked flag is
     *  its own again (between 2026-09-25 and 0.3 it only mirrored the layer,
     *  and a show loaded then had every object locked on its own moved onto
     *  a locked layer named "Locked").
     *
     *  Run once a workspace has loaded. A show saved before own locks came
     *  back (no ItemLocks mark): an object's lock that only repeats its
     *  locked layer's is cleared -- the layer still holds it -- and the
     *  "Locked" layer is taken apart: its objects keep their lock as their
     *  own, each goes back to the layer its folder, mount or fixtures are on
     *  where that says, and the layer goes when nothing is left on it (else
     *  it stays, as an ordinary unlocked layer). Returns how many objects
     *  went back to another layer. */
    int migrateLegacyLocks(class PowerDistribution *pd, class NetworkTopology *nt);
    /** False for a show loaded from before own locks came back, until
     *  migrateLegacyLocks() has run. */
    bool itemLocksAreOwn() const { return m_itemLocksOwn; }

    /** True if a layer with this id currently exists. */
    bool hasLayer(quint32 id) const { return m_layers.contains(id); }

    /** Create a new layer with the given name; returns its new id. */
    quint32 addLayer(const QString &name);

    /** Remove a layer. The Default layer cannot be removed. Members that still
     *  reference the removed layer resolve to Default via layer(). */
    void removeLayer(quint32 id);

    /** Layer property mutators (no-ops on an unknown id). */
    void setLayerName(quint32 id, const QString &name);
    void setLayerVisible(quint32 id, bool visible);
    void setLayerLocked(quint32 id, bool locked);
    void setLayerLabels(quint32 id, bool labels);
    void setLayerOrder(quint32 id, int order);

    /** The layer that newly-created items are assigned to. Falls back to
     *  Default if set to an unknown id. Persisted with the workspace. */
    quint32 activeLayerId() const { return m_activeLayerId; }
    void setActiveLayerId(quint32 id) { m_activeLayerId = hasLayer(id) ? id : defaultLayerId; }
    /** Where a NEW object lands: the active layer -- unless it is locked,
     *  where a new object could not be touched (not selected, not moved,
     *  not edited) the moment it was placed; then the first unlocked layer
     *  in list order, never the stage pieces' own. All locked: the active. */
    quint32 placementLayerId() const;

    /** Next unused layer id. */
    quint32 nextLayerId() const;

private:
    /** Ensure the Default layer (id 0) exists. Called by ctor/reset/load. */
    void ensureDefaultLayer();

    QMap<quint32, MonitorLayer> m_layers;
    quint32 m_activeLayerId = defaultLayerId;
    bool m_itemLocksOwn = true;   ///< see migrateLegacyLocks()

    /********************************************************************
     * Groups (2D map: named, layer-scoped, nestable collections)
     ********************************************************************/
public:
    /** A named collection of map items that select and move together. A group
     *  lives under exactly one layer, and may nest inside a parent group
     *  (parentGroupId != 0) to form group-of-groups. Items reference their
     *  IMMEDIATE group via their per-item groupId (0 = ungrouped). */
    struct MonitorGroup
    {
        quint32 id = 0;
        QString name;
        quint32 layerId = 0;        ///< layer this group sits under
        quint32 parentGroupId = 0;  ///< 0 = top-level (directly under the layer)
        /** Auto-groups created for a truss/riser record their anchor item here,
         *  so the Layers tree can present the group AS that item. Empty kind =
         *  a plain folder (e.g. a manually-created group). */
        QString anchorKind;         ///< "" / "truss" / "platform"
        quint32 anchorId = 0;
        /** Per-group position lock: freezes every member's position (like a
         *  per-item lock — still selectable, just not draggable). */
        bool    locked = false;
        /** Studio frame. When hasFrame is true the group is a first-class
         *  studio object: a rigid body with its own local coordinate frame.
         *  Member fixtures store a group-local offset (FixtureRigProps::
         *  groupLocal, metres) and their world position is DERIVED as
         *  origin + Rz(rotation) * local (see fixtureRigPosition). When false
         *  the group is a plain spatial grouping (members keep absolute
         *  positions) — the backward-compatible default. */
        bool      hasFrame = false;
        QVector3D origin;            ///< world position of the local-frame origin (metres)
        float     rotation = 0.0f;   ///< frame yaw about Z, degrees
        /** Fixture Studio binding (Phase 2). When boundFxGroup != 0 this studio
         *  group is bound to an engine FixtureGroup: seeding lays members out
         *  from that group's CELL matrix at pitchX/pitchY metres per cell. The
         *  binding is provenance + seed spacing only — the cell layout stays the
         *  script's coordinate space, decoupled from these metric offsets. */
        quint32   boundFxGroup = 0;
        float     pitchX = 0.5f;      ///< metres per cell in X (seed spacing)
        float     pitchY = 0.5f;      ///< metres per cell in Y (seed spacing)
    };

    /** Set a group's position-lock flag (no-op if the group is unknown). */
    void setGroupLocked(quint32 gid, bool locked);
    /** True if @p gid or any of its ancestor groups is position-locked. */
    bool groupChainLocked(quint32 gid) const;

    /** All groups (unordered). */
    QList<MonitorGroup> groups() const { return m_groups.values(); }
    MonitorGroup group(quint32 id) const { return m_groups.value(id); }
    bool hasGroup(quint32 id) const { return m_groups.contains(id); }

    /** Register a new group with an explicit id (the view allocates ids so it
     *  can account for items on every model, incl. power sources). */
    void createGroup(quint32 id, const QString &name, quint32 layerId, quint32 parentGroupId = 0);

    /** Create a registry entry for a pre-existing item groupId that has none
     *  (migration from the anonymous first-cut groups). No-op if present. */
    void ensureGroup(quint32 id, quint32 layerId);

    /** Remove a group from the registry (does NOT rewrite item groupIds — the
     *  caller promotes members first). */
    void removeGroup(quint32 id);

    void setGroupName(quint32 id, const QString &name);
    void setGroupLayer(quint32 id, quint32 layerId);
    void setGroupParent(quint32 id, quint32 parentGroupId);
    /** Mark a group as anchored to an item (kind "truss"/"platform"); the tree
     *  then renders the group as that item. Pass an empty kind to clear. */
    void setGroupAnchor(quint32 id, const QString &kind, quint32 anchorId);

    /** Studio-frame mutators (no-op on unknown id). Toggling a frame does NOT
     *  itself convert member positions — callers seed/adopt groupLocal via
     *  worldToGroupLocal() around the toggle. */
    void setGroupHasFrame(quint32 id, bool on);
    void setGroupFrame(quint32 id, const QVector3D &origin, float rotationDeg);
    void setGroupOrigin(quint32 id, const QVector3D &origin);
    void setGroupRotation(quint32 id, float rotationDeg);

    /** Record the FixtureGroup a studio group is bound to and its seed spacing
     *  (metres per cell). Provenance + seed only; does not move members. */
    void setGroupBinding(quint32 id, quint32 fxGroupId, float pitchX, float pitchY);

    /** The nearest frame-bearing group for fixture @p fid — walks the fixture's
     *  immediate group up its parent chain and returns the first group whose
     *  hasFrame is set; 0 if the fixture is not under any studio frame. */
    quint32 fixtureFrameGroup(quint32 fid) const;

    /** Map a point between world space (metres) and a frame group's local
     *  space (metres). Identity if @p groupId is unknown or frameless. */
    QVector3D groupLocalToWorld(quint32 groupId, const QVector3D &local) const;
    QVector3D worldToGroupLocal(quint32 groupId, const QVector3D &world) const;

    /** Immediate child groups of @p parentGroupId (pass 0 for top-level). */
    QList<MonitorGroup> childGroups(quint32 parentGroupId) const;

    /** Next unused group id (registry is authoritative once migrated). */
    quint32 nextGroupId() const;

private:
    QMap<quint32, MonitorGroup> m_groups;

    /********************************************************************
     * Trusses
     ********************************************************************/
public:
    /** Return all defined trusses (owned by this object). */
    QList<Truss*> trusses() const { return m_trusses.values(); }

    /** Look up a truss by ID; returns nullptr if not found. */
    Truss *truss(quint32 id) const { return m_trusses.value(id, nullptr); }

    /** Add a new truss with a unique ID.  Takes ownership. */
    Truss *addTruss();

    /** Remove and delete the truss with the given ID. */
    void removeTruss(quint32 id);

    /** Next unused truss ID. */
    quint32 nextTrussId() const;

    /** Re-derive the origin of every child-bar truss from its parent (attach
     *  point along the parent minus its drop). Call after any truss moves and
     *  once after load. Iterates a few passes so a bar-on-a-bar also settles. */
    void recomputeChildTrusses();

    /** Re-derive the base position of every truss-hung pipe from its parent
     *  truss (pipe top at the truss point, hanging below). */
    void recomputePipeAnchors();

    /** Re-slave every platform-anchored studio group's local frame to its
     *  platform (origin = the platform's reference corner, no rotation), so
     *  members follow the platform when it moves. Call after any platform
     *  move/resize and once after load. See recomputeAnchoredFrames impl. */
    void recomputeAnchoredFrames();

    /** Turn a fixture about the vertical by @p deltaDeg: the face it is on
     *  (studioYaw) and, for a moving head, where its pan counts from. Its
     *  position is the caller's business. */
    void turnFixtureFacing(quint32 fid, float deltaDeg);
    /** Turn platform @p id to face @p deg, and what is on it with it: what
     *  stands on its deck goes round its centre, and everything on its faces
     *  or in its studio frame turns to match. */
    void turnPlatform(quint32 id, float deg);
    /** Turn tower @p id to face @p deg, with what is on its shelves and the
     *  hangers on its sides. */
    void turnTower(quint32 id, float deg);

    /** Rebuild the plot item for any fixture that is rigged on a structure but
     *  has no FxItem -- removeFixture() drops the item and leaves the rig
     *  props, stranding the fixture out of everything driven by
     *  fixtureItemsID() (Layers tree, 2D plot) while the studio editor still
     *  lists it on its structure. Also collapses any fixture carrying more than
     *  one structural mount down to its primaryMount(). Called once after
     *  load. */
    void repairOrphanedMounts();

    /** Put back the agreement between a riser-mounted fixture's face and the
     *  plane it is drawn on. See FixtureRigProps::setStudioMount(): the two are
     *  one fact stored twice, numbered in opposite directions, and until that
     *  setter existed any UI that offered a "Face" control could set the plane
     *  and leave the face behind. Files written in that window are still out
     *  there, so they are corrected on the way in rather than left to render as
     *  a step that does not match its neighbours. */
    void repairMountPlanes();

    /** The single-fixture correction repairMountPlanes() applies to every
     *  entry in m_rigProps on load, exposed so a dialog committing a fresh
     *  mount/orientation choice for ONE fixture (no reload in between) can
     *  apply the same fix immediately instead of waiting for the next file
     *  load to catch it. */
    void repairMountPlane(FixtureRigProps &rp) const;

    /** Put a fixture on @p face: its plane and which way it faces
     *  (studioMount + studioFlip). Mounted on the OUTSIDE of a truss, it also
     *  goes to that side of it -- the face of a tower it is clamped to, or for
     *  a horizontal run on top, underneath, or the downstage/upstage (stage
     *  left/right) chord -- which is what "on the back" means for a fixture on
     *  a truss. Inside one, or not on a truss, only the facing changes. */
    void setFixtureFace(FixtureRigProps &rp, FixtureRigProps::Face face) const;

private:
    QMap<quint32, Truss*> m_trusses;

    /********************************************************************
     * Stage platforms (risers)
     ********************************************************************/
public:
    /** Return all defined platforms (owned by this object). */
    QList<StagePlatform*> platforms() const { return m_platforms.values(); }

    /** Look up a platform by ID; returns nullptr if not found. */
    StagePlatform *platform(quint32 id) const { return m_platforms.value(id, nullptr); }

    /** Add a new platform with a unique ID.  Takes ownership. */
    StagePlatform *addPlatform();

    /** Remove and delete the platform with the given ID. */
    void removePlatform(quint32 id);

    /** Next unused platform ID. */
    quint32 nextPlatformId() const;

    /** Return the platform height (metres) at floor position (xMetres, yMetres).
     *  Returns 0 if the point is not on any platform. */
    float platformHeightAt(float xMetres, float yMetres) const;

    /** Where a target actually IS, and where light aimed at it should land.
     *
     *  One rule, in one place, because there are now two views drawing it and a
     *  palette resolving it -- and every time that logic has been written twice
     *  the two copies have disagreed (the rig view pointed at a target's own Z
     *  while QLCPalette pointed at chest height above it).
     *
     *  Resolves, in order:
     *    - a BOUND target takes its position from the structure it follows, so
     *      moving or restacking the step moves the target with it;
     *    - a target with an aim height is raised that far above whatever it is
     *      standing on -- a deck if there is one under it, else the floor;
     *    - anything else is exactly where it says it is.
     *
     *  @param subjectMode  the caller knows this is a follow-spot aim. A target
     *                      with its own Kind no longer needs telling, but a
     *                      plain one still takes the global aimSubjectHeight()
     *                      this way, which is how every existing show behaves.
     */
    QVector3D targetAimPoint(const class StageTarget *t, bool subjectMode = false) const;

    /** A line target's control points in world space: its points around
     *  targetAimPoint() (its centre, following any binding). Empty for any
     *  other kind. */
    QVector<QVector3D> targetLinePoints(const class StageTarget *t) const;

    /** The point a head at @p from with a beam @p beamDeg wide should aim at,
     *  for target @p t. A follow spot on a Person: the chest while the beam is
     *  narrow; as it widens, lower -- so its top edge stays just over the
     *  head, covering more of them without washing the wall above -- down to
     *  the waist at most. Anything else: targetAimPoint(). */
    QVector3D targetAimPointFor(const class StageTarget *t, bool subjectMode,
                                const QVector3D &from, double beamDeg) const;

    /** How far above its footing @p t is aimed. A per-target override wins;
     *  otherwise a Person is aimed at the follow-spot subject height -- the
     *  one setting that says how high a subject's chest is, so changing it
     *  moves every person with it -- and a plain mark takes that height only
     *  for a follow-spot aim (@p subjectMode). Other kinds use their own. */
    float targetAimHeight(const class StageTarget *t, bool subjectMode = false) const;

    /** How tall the figure drawn for @p t is, in metres; 0 when it is not a
     *  Person. The aim height is a chest, about four fifths of the way up a
     *  standing person, so the figure grows and shrinks with it. */
    float targetFigureHeight(const class StageTarget *t) const;

    /** Base (bottom) height of a platform: the deck height of the tallest LOWER
     *  platform whose footprint it sits within (0 = on the floor). Lets steps
     *  stack — a platform on top of another rises from that one's deck. */
    float platformBaseZ(quint32 id) const;

    /** Id of the TOPMOST platform whose footprint contains (xMetres, yMetres),
     *  or FixtureRigProps::invalidPlatformId() if none. Used to auto deck-mount a
     *  fixture dropped over a platform. */
    quint32 platformIdAt(float xMetres, float yMetres) const;

    /********************************************************************
     * Booms (vertical fixture pipes on a stand)
     ********************************************************************/
public:
    /** Return all defined pipes (owned by this object). */
    QList<Pipe*> pipes() const { return m_pipes.values(); }

    /** Look up a pipe by ID; returns nullptr if not found. */
    Pipe *pipe(quint32 id) const { return m_pipes.value(id, nullptr); }

    /** Add a new pipe with a unique ID. Takes ownership. */
    Pipe *addPipe();

    /** Remove and delete the pipe with the given ID. */
    void removePipe(quint32 id);

    /** Next unused pipe ID. */
    quint32 nextPipeId() const;

    /********************************************************************
     * Stands (placeable support bases)
     ********************************************************************/
public:
    QList<Stand*> stands() const { return m_stands.values(); }
    Stand *stand(quint32 id) const { return m_stands.value(id, nullptr); }
    Stand *addStand();
    void removeStand(quint32 id);
    quint32 nextStandId() const;

    /** Re-derive the base of every stand-mounted pipe from its stand top. */
    void recomputeStandMounts();

    /********************************************************************
     * Towers (large box-truss + shelves)
     ********************************************************************/
public:
    QList<Tower*> towers() const { return m_towers.values(); }
    Tower *tower(quint32 id) const { return m_towers.value(id, nullptr); }
    Tower *addTower();
    void removeTower(quint32 id);
    quint32 nextTowerId() const;

    /********************************************************************
     * Hangers (a bracket and chain on a truss or tower, one fixture each)
     ********************************************************************/
public:
    QList<Hanger*> hangers() const { return m_hangers.values(); }
    Hanger *hanger(quint32 id) const { return m_hangers.value(id, nullptr); }
    /** A new hanger on @p kind @p parentId, named "Hanger N". */
    Hanger *addHanger(Hanger::ParentKind kind, quint32 parentId);
    /** Remove it; a fixture hung on it comes off (keeps its place). */
    void removeHanger(quint32 id);
    quint32 nextHangerId() const;
    /** Hangers on @p kind @p parentId. */
    QList<Hanger*> hangersOn(Hanger::ParentKind kind, quint32 parentId) const;
    /** The fixture on hanger @p id, or UINT_MAX. */
    quint32 fixtureOnHanger(quint32 id) const;
    /** Re-derive every hanger's fastening point from its parent. */
    void recomputeHangers();

    /********************************************************************
     * Scenery (cyc, back wall, drapes -- surfaces light lands on)
     ********************************************************************/
public:
    QList<Scenery*> sceneries() const { return m_sceneries.values(); }
    Scenery *scenery(quint32 id) const { return m_sceneries.value(id, nullptr); }
    Scenery *addScenery();
    void removeScenery(quint32 id);
    quint32 nextSceneryId() const;

private:
    QMap<quint32, Scenery*> m_sceneries;
    QMap<quint32, StagePlatform*> m_platforms;
    QMap<quint32, Pipe*> m_pipes;
    QMap<quint32, Stand*> m_stands;
    QMap<quint32, Tower*> m_towers;
    QMap<quint32, Hanger*> m_hangers;

    /********************************************************************
     * Background / scenic images (placeable objects)
     ********************************************************************/
public:
    /** A placeable image on the 2-D map. Each image lives on a plane and only
     *  renders in the matching view: Floor → 2D Top (lies flat), FrontBackdrop
     *  → Front elevation, SideBackdrop → Side elevation. The rectangle is in
     *  METRES; the meaning of its axes depends on the plane:
     *   - Floor:  (originX, originY) = upstage-left corner in XY; width→X,
     *             height→Y (depth); drawn at Z = originZ.
     *   - Front:  originX = horizontal (X), originZ = bottom edge; width→X,
     *             height→Z (up).
     *   - Side:   originY = horizontal (Y), originZ = bottom edge; width→Y,
     *             height→Z (up). */
    struct MonitorImage
    {
        quint32 id = 0;
        QString name;
        QString source;                 ///< image file path
        enum Plane { Floor = 0, FrontBackdrop = 1, SideBackdrop = 2 };
        int     plane = Floor;
        float   originX = 0.0f, originY = 0.0f, originZ = 0.0f;
        float   width = 2.0f, height = 2.0f;
        float   rotation = 0.0f;         ///< degrees clockwise, about the centre
        quint32 layerId = 0;
        quint32 groupId = 0;
        bool    locked = false;
    };

    /** All defined images. */
    QList<MonitorImage> images() const { return m_images.values(); }
    /** Look up an image by id (default-constructed MonitorImage if absent). */
    MonitorImage image(quint32 id) const { return m_images.value(id); }
    bool hasImage(quint32 id) const { return m_images.contains(id); }
    /** Create a new image object from @p source; returns its new id. */
    quint32 addImage(const QString &source);
    /** Insert or replace an image (keyed by img.id). */
    void setImage(const MonitorImage &img);
    /** Remove the image with the given id. */
    void removeImage(quint32 id);
    /** Next unused image id. */
    quint32 nextImageId() const;

private:
    QMap<quint32, MonitorImage> m_images;

    /********************************************************************
     * Stage targets
     ********************************************************************/
public:
    /** Return all defined targets (owned by this object). */
    QList<StageTarget*> stageTargets() const { return m_stageTargets.values(); }

    /** Look up a target by ID; returns nullptr if not found. */
    StageTarget *stageTarget(quint32 id) const { return m_stageTargets.value(id, nullptr); }

    /** Add a new target with a unique ID.  Takes ownership. */
    StageTarget *addStageTarget();

    /** Remove and delete the target with the given ID. */
    void removeStageTarget(quint32 id);

    /** Next unused target ID. */
    quint32 nextStageTargetId() const;

private:
    QMap<quint32, StageTarget*> m_stageTargets;

    /********************************************************************
     * Fixture rig properties
     ********************************************************************/
public:
    /** Return the rig assignment for fixture $fid (default-constructed if
     *  never set: free-placed, top-hung, panZeroDir=0). */
    FixtureRigProps fixtureRigProps(quint32 fid) const;
    void setFixtureRigProps(quint32 fid, const FixtureRigProps &props);
    void removeFixtureRigProps(quint32 fid) { m_rigProps.remove(fid); }

    /** What fixture $fid is mounted ON, in words ("Truss T1 · 2.5 m along",
     *  "Tower T2 · shelf 3", "free / on the floor"). One wording for the
     *  plot's inspector, Fixture Properties and the patch list. */
    QString mountDescription(quint32 fid) const;
    QString mountDescription(const FixtureRigProps &rp, quint32 fid) const;

signals:
    /** The show's "draw Auto fixtures as their closest stage look" changed
     *  (setAutoStageLooks(), or a show / undo snapshot loaded with it): every
     *  view that draws fixtures classifies them again. */
    void autoStageLooksChanged(bool on);

    /** Emitted when a fixture's rig assignment changes (mounting, pan-zero
     *  facing, truss, offset) so live Aim looks can re-resolve immediately
     *  instead of only after the scene is restarted. */
    void rigPropsChanged(quint32 fid);

    /** Emitted when a stage target is added, removed, moved or edited, so
     *  whatever aims at targets (the look panel's "can't reach" check) can
     *  re-evaluate. StageTarget's own setters are signal-less (they are also
     *  written from the DMX thread), so the UI that edits them calls
     *  notifyStageTargetsChanged(). */
    void stageTargetsChanged();

public:
    void notifyStageTargetsChanged() { emit stageTargetsChanged(); }

public:
    bool hasFixtureRigProps(quint32 fid) const { return m_rigProps.contains(fid); }
    const QMap<quint32, FixtureRigProps> &fixtureRigPropsMap() const { return m_rigProps; }

    /** Compute the derived world-space 3-D position of fixture $fid.
     *  For truss-assigned fixtures the position is derived from the truss
     *  geometry + trussOffset; the existing fixturePosition() X/Y/Z is
     *  used as a fallback for free-placed fixtures.
     *  Returns QVector3D(0,0,0) if the fixture is unknown. */
    QVector3D fixtureRigPosition(quint32 fid) const;
private:
    /** Where its structural mount puts it (fixtureRigPosition() without a
     *  hanger). */
    QVector3D fixtureBasePosition(quint32 fid) const;
public:

    /** A venue's corrections are applied (VenueCorrections): @p fid's rig
     *  props carry them, and @p base is the show's own calibration, which
     *  saveXML() writes in their place -- so the show file never takes a
     *  venue's corrections. */
    void setCalibrationBase(quint32 fid, const RigCalibration &base) { m_calibrationBase[fid] = base; }
    void clearCalibrationBase(quint32 fid) { m_calibrationBase.remove(fid); }
    bool hasCalibrationBase(quint32 fid) const { return m_calibrationBase.contains(fid); }
    RigCalibration calibrationBase(quint32 fid) const { return m_calibrationBase.value(fid); }
    QList<quint32> calibrationBaseFixtures() const { return m_calibrationBase.keys(); }

private:
    QMap<quint32, FixtureRigProps> m_rigProps;
    QHash<quint32, RigCalibration> m_calibrationBase;

    /********************************************************************
     * Gobo swaps
     *
     * A gobo slot of ONE fixture fitted with different glass -- a logo, a
     * show's custom gobo -- the way a tech swaps it in the unit, leaving the
     * shared fixture definition alone. Keyed by the gobo wheel's channel
     * index and the slot's first DMX value (its capability's min). The image
     * is as GoboLibrary::import() stored it (see GoboLibrary::resolve()).
     * Survives taking the fixture off the plot; Doc drops it with the fixture.
     ********************************************************************/
public:
    /** Fit @p image in that slot; an empty @p image restores the original. */
    void setGoboSwap(quint32 fid, quint32 channel, uchar slotMin, const QString &image);
    QString goboSwap(quint32 fid, quint32 channel, uchar slotMin) const;
    /** Every swap on @p fid: (channel, slotMin) -> image. */
    QMap<QPair<quint32, int>, QString> goboSwaps(quint32 fid) const;
    void clearGoboSwaps(quint32 fid) { m_goboSwaps.remove(fid); }

private:
    QMap<quint32, QMap<QPair<quint32, int>, QString> > m_goboSwaps;

    /********************************************************************
     * LED maps (HeadLayout): which head of a model sits in which cell.
     * The show's own copy of each it uses, so it travels with the file;
     * the operator's library (HeadLayoutLibrary) holds the master.
     ********************************************************************/
public:
    /** @p fx's map: the show's copy, else the library's, else head order. */
    HeadLayout headLayoutFor(const Fixture *fx) const;
    /** True when the show or the library has a map for @p fx -- i.e. head
     *  order is not merely assumed. */
    bool hasHeadLayout(const Fixture *fx) const;
    /** True when the show itself holds a map for @p key (not only the
     *  library or head order). */
    bool hasShowHeadLayout(const QString &key) const;
    /** Keep @p layout in the show (replacing any for its key). Emits
     *  headLayoutChanged() when that changes it. */
    void setHeadLayout(const HeadLayout &layout);
    QList<HeadLayout> headLayouts() const;

signals:
    /** A model's LED map changed in the show (the LED Layout editor, Split
     *  by Head Type): views that draw its heads redraw them. */
    void headLayoutChanged(const QString &key);

private:
    /* Read from the DMX thread (a matrix effect's canvas) while the editor
       writes it. */
    mutable QMutex m_headLayoutMutex;
    QHash<QString, HeadLayout> m_headLayouts;

    /********************************************************************
     * 2D view background
     ********************************************************************/
public:
    /** Get/Set a background image to be displayed in 2D mode */
    inline void setCommonBackgroundImage(QString filename) { m_commonBackgroundImage = filename; }
    inline QString commonBackgroundImage() const { return m_commonBackgroundImage; }

    /** Get/Set a flat background color (overrides the default dark background;
     *  ignored when a background image is also set). */
    inline void setCommonBackgroundColor(const QColor &c) { m_commonBackgroundColor = c; }
    inline QColor commonBackgroundColor() const { return m_commonBackgroundColor; }

    /** Set a picture found at $path to be displayed when $fid is started */
    void setCustomBackgroundItem(quint32 fid, QString path) { m_customBackgroundImages[fid] = path; }

    /** Helper method to set a whole list of custom pictures mapped by Function IDs */
    void setCustomBackgroundList(QMap<quint32, QString>list) { m_customBackgroundImages = list; }

    /** Reset any previously set background pictures list */
    void resetCustomBackgroundList() { m_customBackgroundImages.clear(); }

    /** Returns the map of custom background pictures organized as Function ID/Picture path */
    QMap<quint32, QString> customBackgroundList() const { return m_customBackgroundImages; }

    /** Returns the path of a custom picture set for a Function with $id */
    QString customBackground(quint32 fid);

private:
    QString m_commonBackgroundImage;
    QColor  m_commonBackgroundColor;
    QMap <quint32, QString> m_customBackgroundImages;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Load the Monitor properties from the given XML node.
     *
     * @param root An XML subtree containing the Monitor properties
     * @return true if the properties were loaded successfully, otherwise false
     */
    bool loadXML(QXmlStreamReader &root, const Doc* mainDocument);

    /**
     * Save the Monitor properties into an XML document, under the given
     * XML element (tag).
     *
     * @param doc The master XML document to save to.
     * @param wksp_root The workspace root element
     */
    /** Pattern pictures travel with the show: a truss sock or tower face
     *  picture picked from elsewhere (~/Downloads, say) is copied into the
     *  show's images/ folder (Doc::keepWithShow()) and the rig uses the copy
     *  from then on, which saveXML() writes relative (board Q85). Called
     *  when the show is saved, not for undo snapshots. */
    void keepPatternsWithShow(const Doc *mainDocument);

    /** @p showFile: writing the show file itself (Doc::saveXML), which
     *  leaves out a venue's house fixtures -- they are in the venue's file
     *  (VENUES.md). Undo snapshots keep everything. */
    bool saveXML(QXmlStreamWriter *doc, const Doc * mainDocument, bool showFile = false) const;
};

/** @} */

#endif // MONITORPROPERTIES_H
