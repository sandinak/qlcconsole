/*
  Q Light Controller Plus
  monitorfixtureitem.h

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

#ifndef MONITORFIXTUREITEM_H
#define MONITORFIXTUREITEM_H

#include <QGraphicsItem>
#include <QAbstractGraphicsShapeItem>
#include <QFont>
#include <QElapsedTimer>

#include "fixturevisualtraits.h"

class Doc;
class MonitorProperties;
class QWidget;

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

struct FixtureHead
{
    // Ellipse for normal fixtures, rectangle for dense pixel arrays (RGB tape):
    // both derive from QAbstractGraphicsShapeItem (setPen/setBrush); geometry is
    // set via the setShapeRect/shapeRect helpers in the .cpp.
    QAbstractGraphicsShapeItem *m_item;
    QAbstractGraphicsShapeItem *m_back;

    //! the fixture's head number this draws (a colour ring is not in the list)
    int m_index = -1;

    //! cached rgb channels (channel indices)
    QList <quint32> m_rgb;

    //! cached white channels (channel indices) — folded into the rendered
    //! colour so RGBW fixtures don't render black when only white is lit.
    QList <quint32> m_white;

    //! cached amber channels (channel indices) — folded into the colour as warmth.
    QList <quint32> m_amber;

    //! cached cmy channels (channel indices)
    QList <quint32> m_cmy;

    //! cached color channels (channel indices)
    QList <quint32> m_colorWheels;

    //! map DMX values to colors
    /*! map channel -> array of 256 QColors
     */
    QHash<quint32, QList<QColor> > m_colorValues;

    /*! cached shutter channels (channel indices)
     */
    QList <quint32> m_shutterChannels;

    enum ShutterState
    {
        Closed,
        Strobe,
        Open
    };

    //! map DMX values to ON/OFF
    /*! map channel -> array of 256 bool values
     */
    QHash<quint32, QList<ShutterState> > m_shutterValues;
    QColor m_color;
    uchar m_dimmerValue;
    ShutterState m_shutterState;
    int m_strobePhase;
    QTimer* m_strobeTimer;

    quint32 m_dimmer;
    quint32 m_masterDimmer;
    // Top of each dimmer channel's intensity range (strobe/effect capabilities
    // may sit above it, e.g. Betopper LM70). Alpha scales by this so a fixture
    // driven to full dimmer reads full-bright even when that DMX value is <255.
    uchar m_dimmerCeiling;
    uchar m_masterDimmerCeiling;
    quint32 m_panChannel;
    int m_panMaxDegrees;
    qreal m_panDegrees;
    QColor m_panColor;
    quint32 m_tiltChannel;
    int m_tiltMaxDegrees;
    qreal m_tiltDegrees;
    QColor m_tiltColor;
};

class MonitorFixtureItem final : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

    friend class MonitorFixturePropertiesEditor;

public:
    MonitorFixtureItem(Doc *doc, quint32 fid);

    ~MonitorFixtureItem();

    /** Get the fixture name as displayed on the label */
    QString name() const { return m_name; }
    /** Re-read the fixture's name (label and tooltip) after a rename. */
    void refreshName();

    /** Re-read the look the fixture is drawn with (drawnStageLook()) after it
     *  changed: its shape, its heads' shape (round or square lens) and their
     *  layout. Nothing happens when it did not change -- cheap enough to call
     *  on every fixture change. */
    void refreshStageLook();
    /** The look it is drawn with now (tests). */
    Fixture::StageLook stageLook() const { return m_traits.look; }
    /** The lens hint drawn over its first head now (tests); empty: none. */
    QPainterPath lensHintPath() const;

    /** Set the position of this fixture using the monitor measure units */
    void setRealPosition(QPointF pos) { m_realPos = pos; }

    /** Return the position of this fixture express in the monitor measure units */
    QPointF realPosition() const { return m_realPos; }

    /** Sets the dimension of this fixture */
    void setSize(QSize size);
    /** Lay the heads out again at the same size: the model's LED layout
     *  changed (MonitorProperties::headLayoutChanged()). */
    void relayoutHeads() { setSize(QSize(m_width, m_height)); }

    /** The fixture cell size in scene pixels (its head-layout rectangle, drawn
     *  from the item origin). Half of this centres the icon on a point. */
    QSize cellSize() const { return QSize(m_width, m_height); }

    void setGelColor(QColor color) { m_gelColor = color; }
    QColor getColor() const { return m_gelColor; }

    /** Non-destructive 2D-map "view" overlay tint (Power/DMX/... views). When
     *  valid it overrides the live/gel fill so the plot can be colour-coded by
     *  circuit, universe, group, etc.; an invalid colour restores normal fill. */
    void setViewTint(const QColor &color);

    /** Return the fixture ID associated to this item */
    quint32 fixtureID() const { return m_fid; }

    /** Return the number of heads represented by this item */
    int headsCount() const { return m_heads.count(); }

    /** Show/hide this fixture item label */
    void showLabel(bool visible);

    /** The default hover tooltip: name, manufacturer/model and universe/address.
     *  Overlays temporarily replace it; restoreBaseToolTip() puts it back. */
    QString baseToolTip() const;
    void restoreBaseToolTip();

    /** Enable/disable the possibility to move this item.
     *  When disabled the item stays selectable (lock mode) */
    void setMovable(bool movable);

    /** Mark this fixture as bound to a truss (shows cyan border). */
    void setBoundToTruss(bool b) { m_boundToTruss = b; update(); }
    bool isBoundToTruss() const  { return m_boundToTruss; }

    /** True when this fixture's truss is itself the current selection (not
     *  just this fixture alone) — tints the selected-outline color to match
     *  the truss's own selected amber instead of the plain yellow, so the
     *  whole assembly reads as one selection. See
     *  MonitorGraphicsView::updateTrussAnchorLines(). */
    void setTrussGroupSelected(bool on) { if (m_trussGroupSelected != on) { m_trussGroupSelected = on; update(); } }
    bool isTrussGroupSelected() const   { return m_trussGroupSelected; }

    /** A fixture on a locked layer: outlined red, the same as every other
     *  locked object on the plot, and click-through (declines mouse buttons)
     *  so whatever is beneath it stays reachable. It used to be drawn faint
     *  instead -- a second way of saying "locked" that matched nothing else. */
    void setGhosted(bool g)
    {
        if (m_ghosted == g)
            return;
        m_ghosted = g;
        setAcceptedMouseButtons(g ? Qt::NoButton : Qt::AllButtons);
        update();
    }
    bool isGhosted() const { return m_ghosted; }

    /** The icon's own rectangle in ITEM coordinates -- the body, without the
     *  name label or the selection halo that boundingRect() has to include.
     *  What actually covers something underneath is this, not the text. */
    QRectF iconRect() const { return QRectF(0, 0, m_width, m_height); }

    /** Where the icon body (not the label) actually sits, in SCENE
     *  coordinates -- the one "where is this fixture drawn" a truss/pipe
     *  hit-test or attach should read. sceneBoundingRect().center() used to
     *  serve this in some call sites: it is NOT the same point, because
     *  boundingRect() also reserves space for the name label, so its centre
     *  shifts depending on whether labels happen to be showing on this
     *  fixture's layer (a cosmetic view setting, not model state) -- two
     *  call sites reading "the same" fixture's centre could disagree, and
     *  one COULD disagree with itself before/after a label toggle. This is
     *  mapToScene(), so it is also correct under rotation, unlike a plain
     *  pos() + cellSize()/2 offset. */
    QPointF iconCentreScene() const { return mapToScene(iconRect().center()); }

    /** The view's zoom and quarter turn (degrees), for the two things this
     *  item paints in SCREEN terms: the name label kept upright, and the
     *  selection halo's screen-wide pen. Its hit and repaint areas follow
     *  them; the plot calls this whenever it zooms or turns. */
    void setViewHint(qreal scale, qreal turnDeg);
    /** The name label as painted, upright on screen, in item coordinates. */
    QPolygonF labelPolygon() const;

    /** "Isolated" = drilled into its group to select this ONE fixture (double-
     *  click). Draws a distinct bright dashed border so it's obvious the sub-
     *  unit is selected, not the whole group. Cleared when deselected. */
    void setIsolated(bool on) { m_isolated = on; update(); }
    bool isIsolated() const   { return m_isolated; }

    /** Show a red escape-from-truss indicator while dragging far off the truss. */
    void setEscapeMode(bool e) { if (m_escapeMode != e) { m_escapeMode = e; update(); } }
    bool escapeMode() const    { return m_escapeMode; }

    /** "Release here and it will ATTACH to the truss under it."
     *
     *  The mirror of escape mode: red says releasing will break the binding,
     *  green says releasing will make one. Both are only ever true mid-drag,
     *  so the fixture answers "what happens if I let go" before you commit
     *  rather than after. */
    void setAttachMode(bool a) { if (m_attachMode != a) { m_attachMode = a; update(); } }
    bool attachMode() const    { return m_attachMode; }

    /** Under a structure that is over it on the plan (the top view draws
     *  every fixture on top, to keep it clickable): faded, outlined dashed. */
    void setBeneath(bool b)
    {
        if (m_beneath == b)
            return;
        m_beneath = b;
        setOpacity(b ? 0.45 : 1.0);
        update();
    }
    bool beneath() const { return m_beneath; }

    /** Highlight from an external source (e.g. Programming tab selection). */
    void setHighlighted(bool h) { m_highlighted = h; update(); }
    /** Focus mode: this light has changes not yet saved -- a yellow dashed
     *  ring (not on the highlighted head, drawn as it is) and a badge. */
    void setUnsaved(bool u) { if (u != m_unsaved) { m_unsaved = u; update(); } }
    bool isUnsaved() const { return m_unsaved; }
    /** Board Q133: its venue correction was skipped -- the fixture is not
     *  what was focused there -- so it needs focusing again: an orange "!"
     *  badge on its lower corner. */
    void setNeedsFocus(bool n) { if (n != m_needsFocus) { m_needsFocus = n; update(); } }
    bool needsFocus() const { return m_needsFocus; }

    /** Set the fixture's pan-zero facing (degrees clockwise from downstage),
     *  i.e. FixtureRigProps::panZeroDir. Drives the facing arrow on the icon.
     *  Only meaningful for moving heads (fixtures with a Pan channel). */
    void setFacing(float deg) { m_facingDeg = deg; update(); }
    float facing() const { return m_facingDeg; }

    /** Configure the snap-to-grid behaviour for this item.
     *  @param divisions number of cell subdivisions to snap to
     *         (1 = full cell, 2 = 1/2, 4 = 1/4). <= 0 disables snapping.
     *  @param cellPixels size of a grid cell in pixels
     *  @param xOffset horizontal grid origin offset in pixels
     *  @param yOffset vertical grid origin offset in pixels */
    void setSnap(int divisions, qreal cellPixels, qreal xOffset, qreal yOffset);

    /** Tell this item which way the canvas is currently looking at it, so a
     *  Mover can draw the matching silhouette: Top (plan) = round head with
     *  a flat lens edge in a square base footprint; Front/Side (elevation)
     *  = base + yoke arms + head. Mirrors MonitorGraphicsView::isElevation();
     *  updated whenever the view's POV changes. No effect on any other
     *  silhouette (Par/Bar/Generic look the same from every POV). */
    void setElevationView(bool on) { if (m_elevationView != on) { m_elevationView = on; update(); } }

public slots:
    /** Update the fixture values for rendering, passing the
     *  universe array of values */
    void slotUpdateValues();
    void slotStrobeTimer();

protected:
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    void computeTiltPosition(FixtureHead *h, uchar value);
    void computePanPosition(FixtureHead *h, uchar value);

    /** Radius (item-local px) the facing arrow extends from the icon centre. */
    qreal facingReach() const;
    /** Persist the current facing into FixtureRigProps::panZeroDir + mark the
     *  workspace modified. */
    void commitFacing();

    QColor computeColor(const FixtureHead *head, const QByteArray & values);
    uchar computeAlpha(const FixtureHead *head, const QByteArray & values);
    FixtureHead::ShutterState computeShutter(const FixtureHead *head, const QByteArray & values);

    /** Draw the fixture's outer body outline/fill into $r, shaped by
     *  m_traits.kind (Mover = ellipse "puck", Par = rounded can, Bar/Generic =
     *  sharp rect) so the same silhouette classifier used by the Structure
     *  Studio elevation editor also reads on the 2D plan view. Shared by the
     *  background fill, selection halo, truss-bind ring and marked outline,
     *  which all otherwise independently drew the same plain rect. */
    void drawBody(QPainter *painter, const QRectF &r) const;

signals:
    void itemDropped(MonitorFixtureItem *);

    /** Emitted when the fixture's mounting height (Z) or head orientation
     *  changes, so the view can reposition / relayout it. */
    void mountingChanged(quint32 fid);

private:
    Doc *m_doc;

    /** The Fixture ID this item is associated to */
    quint32 m_fid;

    /** The fixture name */
    QString m_name;

    /** Width of the item */
    int m_width;

    /** Height of the item */
    int m_height;

    /** Position of the item top-left corner in millimeters */
    QPointF m_realPos;

    QList <FixtureHead *> m_heads;

    /** In case of a dimmer, this hold the gel color to apply */
    QColor m_gelColor;

    /** 2D-map view overlay tint; invalid = no overlay (normal rendering). */
    QColor m_viewTint;

    /** Number of cell subdivisions used to snap the position.
     *  <= 0 means snapping is disabled */
    int m_snapDivisions;

    /** Size of a grid cell in pixels, used to compute snapping */
    qreal m_snapCellPixels;

    /** Grid origin offsets in pixels, used to align the snapping */
    qreal m_snapXOffset;
    qreal m_snapYOffset;

    /** Flag to show/hide a fixture label */
    bool m_labelVisibility;

    /** Font used if label is visible */
    QFont m_font;

    QRect m_labelRect;

    /** setViewHint(): the view's scale and turn. */
    qreal m_viewScale = 1.0;
    qreal m_viewTurn = 0.0;

    bool m_boundToTruss  = false;
    bool m_trussGroupSelected = false;
    bool m_escapeMode    = false;
    bool m_attachMode    = false;
    bool m_beneath       = false;   ///< setBeneath()
    bool m_isolated      = false;   ///< drilled-in single selection within a group
    bool m_ghosted       = false;   ///< locked-layer indicator: red + click-through
    bool m_highlighted   = false;
    bool m_unsaved       = false;
    bool m_needsFocus    = false;

    /** True if any head has a Pan channel (i.e. a moving head): only these
     *  show a facing arrow and accept Alt-drag rotation. */
    bool m_hasPan        = false;

    /** Shape-family classification (Mover/Par/Bar/Generic) shared with the
     *  Structure Studio elevation editor, computed once at construction. */
    FixtureVisualTraits m_traits;

    /** A mover's colour ring (FixtureVisualTraits::ringChannel), drawn as a
     *  band round its head: a child item over the head, its segments from
     *  colourRingLive(). Null for a fixture without one. A chase keeps a
     *  timer stepping it. */
    class ColourRingGraphic *m_ringItem = nullptr;
    /** Over the first head, the look's lens hint (stageLookLensHint()):
     *  a gobo, LEDs, rings, cells. Null when the look has none. */
    class LensHintGraphic *m_hintItem = nullptr;
    void updateLensHint();
    /** A practical lantern's glow (its first head's live colour, alpha its
     *  level), drawn over its body by paint(). */
    QColor m_lanternGlow;
    /** Dense (pixel tape) heads: tiled squares whatever the look. */
    bool m_denseHeads = false;
    QTimer *m_ringTimer = nullptr;
    QElapsedTimer m_ringClock;
public:
    /** The ring's segments as drawn now (tests); empty: no ring. */
    QVector<QColor> ringSegments() const;

    /** "Set Mounting Height… (2.50 m)" -- in the show's units (feet on a
     *  feet show), as the plot's other dialogs. */
    static QString mountingHeightText(const MonitorProperties *props, quint32 fid);
    /** Ask for @p fid's height above the floor, in the show's units, and
     *  set it. True when it was set. */
    static bool askMountingHeight(QWidget *parent, MonitorProperties *props, quint32 fid);
private:

    /** True when the canvas's current POV is Front/Side (an elevation), set
     *  via setElevationView(); see there. */
    bool m_elevationView = false;

    /** Pan-zero facing in degrees clockwise from downstage (FixtureRigProps
     *  panZeroDir). 0 = faces downstage. */
    float m_facingDeg    = 0.0f;

    /** True while an Alt-drag facing rotation is in progress. */
    bool m_rotating      = false;

    /** True when the layout is unlocked (rig-setup mode): the facing arrow is
     *  shown and Alt-drag rotation / facing menu are enabled. Mirrors the
     *  ItemIsMovable flag set by MonitorGraphicsView::setLayoutLocked(). */
    bool m_editable      = true;

    /** Set once the pointer actually moves during a press, so a plain click
     *  (select / double-click) does not emit a spurious drop that re-snaps the
     *  fixture's position. Reset on each press. */
    bool m_dragMoved     = false;
};

/** @} */

#endif // MONITORFIXTUREITEM_H
