/*
  Q Light Controller Plus
  fixturefields.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FIXTUREFIELDS_H
#define FIXTUREFIELDS_H

#include <QObject>
#include <functional>
#include <QList>

#include "../qlcui_export.h"

class StructureStudioView;
class ColourSwatchButton;
class QDoubleSpinBox;
class LightControls;
class QFormLayout;
class QComboBox;
class QSpinBox;
class QWidget;
class Monitor;
class QLabel;

/** @addtogroup ui_monitor
 * @{
 */

/**
 * A fixture's properties as one set of rows -- the same widgets, labels and
 * rules in the studio inspector (the quick subset) and in Fixture Properties
 * (everything): what it is mounted on, its place on a tower, Colour,
 * Orientation, Face, Placement, Angle, Tilt, Light, Glass and its DMX
 * address (with Patch… for several, or an unpatched one).
 *
 * It shows every fixture it is given: a row they differ on reads "mixed",
 * and changes nothing until it is set -- then it is set on all of them. A
 * change applies at once; aboutToEdit() comes first, for an undo snapshot.
 */
class QLC_UI_EXPORT FixtureFields final : public QObject
{
    Q_OBJECT

public:
    /** Adds its rows to @p form, widgets parented to @p parent. With a @p view
     *  (a structure's studio), a face is set the way the studio sets it --
     *  re-pinning a frame-group fixture to that face of its frame. */
    FixtureFields(Monitor *monitor, QFormLayout *form, QWidget *parent,
                  StructureStudioView *view = nullptr);

    /** Show @p fids. @p shown is the one whose kind (a bar, a mover, on a
     *  tower...) decides which rows there are; invalid = the first. */
    void setFixtures(const QList<quint32> &fids, quint32 shown);
    QList<quint32> fixtures() const { return m_fids; }
    quint32 shown() const { return m_shown; }

    /** Read them again (something else changed them). */
    void reload() { setFixtures(m_fids, m_shown); }

    /** Hide every row (the panel shows something else -- a bar). */
    void hideAll();

    /** "mixed" on a spin box, for rows of the same kind beside these: its
     *  lowest value, one step below @p lowest, reads "mixed". */
    static void spinMixed(QDoubleSpinBox *sp, bool mixed, double lowest);
    static bool spinIsMixed(const QDoubleSpinBox *sp);

    /** Fill @p cb with the stage looks, in sections (Moving, Fixed
     *  conventional, Fixed LED) under "Auto (<what its type is>)" for
     *  @p fx (nullptr: plain "Auto"). Each item's UserRole data is its
     *  Fixture::StageLook; the section headings are disabled. Shared with
     *  the Fixture Manager's Plot Symbol menu's wording. */
    static void fillStageLookCombo(QComboBox *cb, const class Fixture *fx);

signals:
    /** A field is about to change the fixtures: @p field names it, so a run of
     *  steps on one spin box can be one undo. */
    void aboutToEdit(const QString &field);
    /** A field changed the fixtures. */
    void edited();

private:
    void row(QWidget *w, bool visible);
    void finishEdit();
    void applyFace(int face);
    void moveDmx();
    /** Pixel path, turns, a wand's length and who holds it
     *  (docs/design/FOUNTAIN_WAND.md): built, wired and filled. */
    void buildPixelRows(QFormLayout *form, QWidget *parent);
    void showPixelRows(quint32 fid);
    /** Sprinkle loops: the span table, edited in its dialog. */
    void editSprinkleSpans();
    /** Ask the shown fixture's WLED for its segments (/json/state). */
    void readSprinkleSegments();
    /** A /json/state reply for @p fid's WLED (or why there is none, @p ok
     *  false): its segments become its spans. */
    void takeWledSegments(quint32 fid, const QByteArray &body, bool ok, const QString &why);
    /** Write Segments: the shown fixture's spans to its WLED as segments,
     *  asked first (WledDeviceWrite), busy then the result in m_sprNote. */
    void writeSprinkleSegments();
    /** Free string (anchors): Add Anchor (the plot then takes clicks on the
     *  plan; a dialog this sits in closes, keeping what was set) and the
     *  anchor table, edited in its dialog. */
    void addStringAnchor();
    void editStringAnchors();
    /** The free string's note: what to do first, or how it lies. */
    QString stringNote(quint32 fid) const;
    /** Write Segments for a sprinkle loop or a free string (@p freeString):
     *  its spans to its WLED, asked first, busy then the result in its note. */
    void writeSegmentsFor(bool freeString);
    /** The Write Segments confirmation (a test answers it instead). */
    std::function<bool(const QString &name, const QString &ip, int segments, int existing)> m_confirmWrite;

    Monitor *m_monitor;
    QFormLayout *m_form;
    QWidget *m_parent;
    StructureStudioView *m_view;
    QList<quint32> m_fids;
    quint32 m_shown;
    bool m_loading = false;

    QComboBox *m_look;          ///< its stage look (Fixture::StageLook)
    QComboBox *m_owner;         ///< who it belongs to (RigOwner, docs/design/VENUES.md)
    QLabel *m_mountOn;
    QComboBox *m_towerPos;
    ColourSwatchButton *m_colour;
    QComboBox *m_orientation;
    QComboBox *m_face;
    QComboBox *m_placement;
    QDoubleSpinBox *m_angle;
    QDoubleSpinBox *m_tilt;
    QComboBox *m_pixelPath;     ///< PixelPath::Shape
    QDoubleSpinBox *m_pixelTurns;
    QDoubleSpinBox *m_wandLength;
    QDoubleSpinBox *m_wandStrip;     ///< the lit string's length; 0 = LED tape
    QDoubleSpinBox *m_wandDiameter;
    QComboBox *m_wandStart;          ///< LED 1 at the grip or the tip
    QComboBox *m_heldBy;        ///< a Person target, or nobody
    QDoubleSpinBox *m_holdHeight;
    QDoubleSpinBox *m_holdTilt;
    QSpinBox *m_sprPoints;      ///< Sprinkle loops: places round the U
    QSpinBox *m_sprPer;         ///< and spans at each
    QWidget *m_sprBox;          ///< Edit Spans…, Read Segments
    class QPushButton *m_sprEdit;
    class QPushButton *m_sprRead;
    class QPushButton *m_sprWrite;
    class WledDeviceWrite *m_wledWrite = nullptr;   ///< Write Segments in progress
    QLabel *m_sprNote;          ///< the span table in words, or why a read failed
    QWidget *m_strBox;          ///< Free string: its anchors, Add Anchor, Edit Anchors…
    QLabel *m_strCount;
    class QPushButton *m_strAdd;
    class QPushButton *m_strEdit;
    class QPushButton *m_strWrite;
    QLabel *m_strNote;          ///< what to do first, how it lies, or a write's progress
    QLabel *m_writeNote = nullptr;  ///< the note the write in progress reports in
    class QNetworkAccessManager *m_nam = nullptr;
    LightControls *m_light;
    QWidget *m_dmxBox;
    QSpinBox *m_dmxUni;
    QSpinBox *m_dmxAddr;
    QLabel *m_dmxNote;
    class QToolButton *m_patch;
    QWidget *m_wledBox;         ///< WLED settings that do not match, + Fix
    QLabel *m_wledNote;
    class QToolButton *m_wledFix;
    QLabel *m_wledWriteNote;    ///< a device write: busy, or what it did
    QDoubleSpinBox *m_maxAmps;  ///< the most it may draw (PowerProfile::maxAmps)
    QLabel *m_powerNote;        ///< what has been measured
    class QToolButton *m_copyPower; ///< readings to every fixture like it
};

/** @} */

#endif
