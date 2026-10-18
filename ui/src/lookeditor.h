/*
  Q Light Controller Plus
  lookeditor.h

  Fork-owned inline editor for a single palette ("look"), shown at the
  bottom of the Programming tab's canvas when a look is selected. Controls
  adapt to the palette type:
    - Color   : color picker
    - Dimmer  : intensity slider
    - PanTilt : X/Y grid (the VC XY pad widget)
    - Beam    : 3 sliders (Focus, Frost, Iris)
    - Gobo/Shutter and other single-value types : slider + named-capability
      picker with gobo/effect image thumbnails and a live preview

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef LOOKEDITOR_H
#define LOOKEDITOR_H

#include <QWidget>

class QStackedWidget;
class QColorDialog;
class VCXYPadArea;
class CapabilityBar;
class PathDrawWidget;
class GradientDirectionWidget;
class QSlider;
class QCheckBox;
class QDoubleSpinBox;
class LineSpreadPreview;
class QComboBox;
class QPushButton;
class QLabel;
class QLineEdit;
class QToolButton;
class QLCChannel;
class QLCPalette;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

class LookEditor final : public QWidget
{
    Q_OBJECT

public:
    LookEditor(Doc *doc, QWidget *parent = nullptr);
    ~LookEditor();

    /** The scene whose targets the edited palette applies to. Used to warn
     *  when e.g. a Gobo look has no target fixture that supports gobos. */
    void setContextScene(Scene *scene);

    /** The palette currently shown (invalidId() if none) — lets a caller that
     *  changed a palette's value from outside this widget (e.g. a control
     *  surface nudging pan/tilt directly on the engine side) ask for a
     *  redisplay via setPalette(paletteId()) without needing to track the
     *  id itself. */
    quint32 paletteId() const { return m_paletteId; }

    /** A line under the title saying where this palette is being shown, e.g.
     *  "trying it on <look>" when it isn't in the look on the canvas. Empty
     *  hides it. Set by the Programming tab, which owns the preview. */
    void setTrialNote(const QString &text);
    QString trialNote() const;

    /** The Used by line (PROGRAMMING_ROLE_DESIGN.md P-D13): where the
     *  edited palette is used, rich text whose links the host follows
     *  (usedByLinkActivated). Empty hides it. Either tab may set it. */
    void setUsedBy(const QString &html);
    /** The line as plain text (tests). */
    QString usedByText() const;

public slots:
    /** Edit the given palette; invalidId() shows the empty page. */
    void setPalette(quint32 paletteId);

signals:
    /** The edited palette's value changed (may affect name/icon — triggers full
     *  tree + canvas update in ProgrammingManager). */
    void paletteChanged(quint32 paletteId);
    /** Emitted when a palette is auto-renamed (e.g. an Effect renamed to its
     *  effect's name), so the source tree can re-reveal it at its new spot. */
    void paletteRenamed(quint32 paletteId);

    /** A live parameter value changed but the palette's name/icon are
     *  unaffected (e.g. effect script param slider). Only refreshes the DMX
     *  preview — does NOT rebuild the look list or palette tree. */
    void paletteValueChanged(quint32 paletteId);

    /** A link in the Used by line was clicked (see setUsedBy()). */
    void usedByLinkActivated(const QString &link);

    /** Emitted at the end of every setPalette() call — on a genuine focus
     *  change (a different look selected on screen) as well as a same-id
     *  redraw (e.g. after a control surface writes a new value into the
     *  currently-shown palette). invalidId() when nothing's shown. Lets an
     *  external listener (e.g. a control-surface overlay) recompute what's
     *  "in use right now" without polling. */
    void lookFocusChanged(quint32 paletteId);

private slots:
    void slotColorChanged(const QColor &c);
    void slotRgbSliderChanged();  //!< numbered R/G/B sliders (1-3)
    void slotColorExtraChanged(); //!< White/Amber/UV sliders
    void slotDimmerChanged(int v);
    void slotPanTiltChanged(const QPointF &p);
    void slotAimTargetChanged(int index);
    /** A line target's Spread (and grouping) changed in the Aim page. */
    void slotLineSettingsChanged();
    void slotSingleValueChanged(int v);
    void slotSingleCapabilityPicked(int index);
    /** "Wheel of" changed: list that fixture type's gobos/shutter states. */
    void slotSingleSourcePicked(int index);
    /** A Gobo look's name was typed: it matches by it on other types. */
    void slotGoboNameEdited();
    void slotBeamChanged(int);
    /** Per-look fade override (checkbox + seconds spin) changed. */
    void slotFadeTimeChanged();

private:
    /** Whether any target fixture of the context scene has a channel in the
     *  given QLCChannel::Group. */
    bool targetsHaveChannelGroup(int group) const;

    /** A representative target-fixture channel of the given group, or NULL.
     *  Used to surface capability (gobo/shutter/strobe) names. */
    const QLCChannel *representativeChannel(int group) const;

public:
    /** One fixture TYPE whose channel of a group can name a gobo/shutter
     *  value: what the "Wheel of" chooser lists. */
    struct ChannelSource
    {
        QString key;                //!< manufacturer/model/mode, for dedup + memory
        QString label;              //!< shown in the chooser
        const QLCChannel *channel;  //!< the channel the palette's value lands on
        quint32 channelIndex = 0;   //!< its index in the fixture
        QList<quint32> fixtures;    //!< the fixtures of this type considered
    };
    /** The fixture types that can show a @p group value: the look's own
     *  targets when it has any that can, otherwise every fixture in the
     *  workspace -- a gobo made from the palette list has no look yet, and
     *  still needs a wheel to pick from. One entry per fixture type, most
     *  numerous first. Uses the same channel the palette writes to
     *  (QLCPalette: the mode's first MSB channel of the group). */
    QList<ChannelSource> singleChannelSources(int group) const;

private:
    /** Fill the named-capability picker from m_singleChannel for value @p v. */
    void fillSingleCapabilities(int v);
    /** The picture in that slot: a unit's swapped glass (the first fixture
     *  of the browsed type that has one, its name in @p swapName), else the
     *  definition's. */
    QString slotPicture(class QLCCapability *cap, QString *swapName = nullptr) const;

    /** True if any target fixture has the given QLCChannel::PrimaryColour
     *  emitter channel (White/Amber/UV). */
    bool targetsHaveColour(int primaryColour) const;

    /** Store the Color palette from the picker + White/Amber/UV sliders. */
    void commitColor();
    /** Show or hide the "this colour is nearly dark" line for the current
     *  slider values (see darkColourWarning()). */
    void updateColorWarning();

public:
    /** What to tell the user about a colour look this dark, or an empty
     *  string when it is bright enough to need no comment. A colour look is a
     *  LEVEL on every fixture that mixes colour -- the dimmer scales it, it
     *  cannot raise it -- so "red" at 4% leaves RGB heads dark while a
     *  colour-wheel head, which only takes the hue, still shines. */
    static QString darkColourWarning(const QColor &rgb, const QColor &wauv);

private:

    /** Capability name covering value v on the channel, or empty. */
    QString capabilityNameAt(const QLCChannel *ch, int v) const;

    /** Refresh the gobo/shutter image preview for value v on m_singleChannel
     *  (hides the preview when there's no Picture-preset capability). */
    void updateSinglePreview(int v);

private:
    Doc *m_doc;
    Scene *m_contextScene;
    quint32 m_paletteId;
    bool m_loading;
    //!< "<paletteId>|<scriptPath>" the effect param panel was last built for, so
    //!< a redundant rebuild (selection often emits twice) is skipped — rebuilding
    //!< in place left a ghost set of sliders painted under the live ones.
    QString m_effectBuildKey;

    QLineEdit *m_nameEdit;   //!< editable palette name
    QLabel    *m_title;      //!< "used by N scene(s)" info line
    QLabel    *m_warning;
    QLabel    *m_trialNote;   //!< see setTrialNote()
    QLabel    *m_usedBy;      //!< see setUsedBy()

    // Per-look fade override row (per scene+palette; hidden when no context
    // scene or for Effect palettes, which the script runner fades itself).
    // Each spin's minimum is a special "step" value meaning "fall back to the
    // chaser step / scene fade" (independent in vs out — a pulse = 0 in, slow out).
    QWidget        *m_fadeRow = nullptr;
    class FadeTimeSpin *m_fadeInSpin = nullptr;
    class FadeTimeSpin *m_fadeOutSpin = nullptr;
    QLabel *m_fadeInLabel = nullptr;
    QLabel *m_fadeOutLabel = nullptr;
    QStackedWidget *m_stack;
    int m_pageEmpty, m_pageColor, m_pageDimmer, m_pagePanTilt, m_pageAim, m_pageBeam, m_pageSingle, m_pageStrobe;

    QColorDialog *m_colorDialog;
    //!< Numbered (1-6) sliders mirroring the PMJ-style control surface fader
    //!< mapping (PMJOverlay::slotRoleActivated's Level case) so it's visible
    //!< on screen which physical fader does what.
    QSlider *m_redSlider, *m_greenSlider, *m_blueSlider; //!< 1, 2, 3
    QSlider *m_whiteSlider, *m_amberSlider, *m_uvSlider; //!< 4, 5, 6 — extra colour emitters
    QWidget *m_whiteRow, *m_amberRow, *m_uvRow;          //!< hidden when absent
    CapabilityBar *m_dimmerBar; //!< gradient + strobe region marks
    QLabel *m_colorWarning;    //!< "nearly dark" note under the colour page
    QLabel *m_dimmerValue;
    QLabel *m_dimmerCap;       //!< capability name at current intensity (strobe etc.)
    VCXYPadArea *m_xyPad;
    QComboBox   *m_aimTargetCombo;  //!< stage target for Aim palettes
    /* A line target's controls (LINE_TARGET_DESIGN.md), shown only when
       the look aims at a line. */
    QWidget *m_lineBox = nullptr;
    QSlider *m_lineSpreadSlider = nullptr;       //!< -50..50 = -5..+5
    QDoubleSpinBox *m_lineSpreadSpin = nullptr;
    QComboBox *m_lineGroupCombo = nullptr;       //!< data: LineSpread group
    QComboBox *m_lineCrossCombo = nullptr;       //!< data: LineSpread::Cross
    QCheckBox *m_lineShareCheck = nullptr;
    QComboBox *m_lineOrderCombo = nullptr;       //!< data: LineSpread::Order
    LineSpreadPreview *m_linePreview = nullptr;
    /** Show the line controls for the look's target, filled from the look. */
    void syncLineControls();
    QSlider *m_beamFocusSlider, *m_beamFrostSlider, *m_beamIrisSlider;
    QLabel *m_beamFocusValue, *m_beamFrostValue, *m_beamIrisValue;
    QSlider *m_singleSlider;
    QLabel *m_singleValue;
    QComboBox *m_singleCombo;  //!< named gobo/shutter capabilities
    QLabel *m_singlePreview;   //!< gobo/shutter image for the current value
    const QLCChannel *m_singleChannel; //!< representative chan for the single page
    QLabel *m_singleSourceLabel;       //!< "Wheel of"
    QComboBox *m_singleSourceCombo;    //!< which fixture type's wheel to browse
    QList<ChannelSource> m_singleSources;
    QString m_singleSourceKey;         //!< last type chosen, kept across palettes
    QWidget *m_goboNameRow;            //!< Gobo: "Name on other types"
    QLineEdit *m_goboNameEdit;
    QLabel *m_goboMatches;             //!< Gobo: what each type plays for it
    /** Gobo: the value the look plays on the browsed type (by name, or as
     *  picked for it); -1 when it doesn't apply to it. */
    int goboValueForSource(const QLCPalette *p) const;
    void updateGoboMatches();

    // Strobe page
    QSlider *m_strobeSlider;   //!< 0-100 → rate 0.0-1.0
    QLabel  *m_strobeValue;

    // Effect page
    int m_pageEffect;
    class EffectBrowser *m_effectBrowser = nullptr;   ///< the picker over m_effectScriptCombo
    /** The picker's own window: the browser lives there, out of the way of
     *  the settings once an effect is picked ("taking up too much space when
     *  working WITH an effect"). Every pick applies at once; Cancel puts back
     *  what the look had when it opened. */
    QWidget     *m_effectPicker = nullptr;
    QLabel      *m_effectPickedLabel = nullptr;   ///< what the look runs, over its settings
    QPushButton *m_effectChangeButton = nullptr;  ///< opens the picker
    class QToolButton *m_effectInfoButton = nullptr; ///< "?": what it is, on hover; every setting on a click
    class QTextBrowser *m_effectPickerInfo = nullptr; ///< the picker's account of the one picked
    class QDialog *m_effectDetails = nullptr;     ///< the "?" window
    QString      m_effectSummaryHtml;             ///< description, works with, notes
    QString      m_effectDetailsHtml;             ///< ...and the generator and every setting
    struct EffectSnapshot
    {
        quint32 paletteId = 0xFFFFFFFF;
        QString name, path, script, preset, wled;
        QMap<QString, double> params;
        QMap<QString, QString> strings;
        QMap<QString, QPair<quint32, quint32>> bindings;
    } m_pickerSnapshot;
public:
    /** Open the effect picker over the look being edited. */
    void openEffectPicker();
    /** Close it: keep what was picked, or put back what was there. */
    void closeEffectPicker(bool keep);
    bool effectPickerOpen() const;
    /** The "?" window: what the effect is and what each setting does. */
    void showEffectDetails();
    /** The look's targets include a WLED device (or there's no scene to
     *  tell: then true). */
    bool lookHasWledTarget() const;
private:
    void updateEffectHeader();
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
private:
    QComboBox   *m_effectScriptCombo;
    QLabel      *m_effectDescLabel;   //!< one-line description below combo
    QLabel      *m_effectNotesLabel;  //!< longer paragraph in edit panel
    QLabel      *m_effectTypesLabel;  //!< "Works with: …" fixture-type chips
    QWidget     *m_effectDynWidget;   //!< rebuilt when script selection changes
    QCheckBox   *m_effectPersistentCb = nullptr; //!< QLCPalette::persistent()
    QPushButton *m_saveAsEffectButton = nullptr;
    QPushButton *m_exportEffectButton = nullptr;
    QPushButton *m_importEffectButton = nullptr;
    QPushButton *m_newScriptButton = nullptr;
    QPushButton *m_editScriptButton = nullptr;

    // "Learn range" — capture a MIDI controller's lowest/highest played note and
    // write them into this effect's noteLow/noteHigh params (persisted in the
    // palette, so it "locks in" and survives restarts, unlike a script-side
    // learn that lives only in transient JS state).
    QPushButton *m_learnRangeButton = nullptr;
    QLabel      *m_learnRangeStatus = nullptr;
    bool         m_learning = false;
    int          m_learnLo = 127;
    int          m_learnHi = 0;
    QString      m_learnLoParam, m_learnHiParam;
    bool         m_learnHasAutoRange = false;   //!< flip autoRange→Manual on lock
    void startLearnRange();
    void stopLearnRange(bool commit);

    /** Open @p filePath in the in-app or OS editor per the app preference. */
    void openScriptInEditor(const QString &filePath);

private slots:
    void slotNameEdited();
    void slotStrobeChanged(int value);
    void slotEffectScriptChanged(int index);
    /** Save the current Generator + settings as a named Effect (preset). */
    void slotSaveAsEffect();
    /** Export the current Effect to a portable .qxfx (script source + settings). */
    void slotExportEffect();
    /** Import a .qxfx: install its script (if missing) + save the preset. */
    void slotImportEffect();
    /** Create a new effect script from a template and open it in the editor. */
    void slotNewEffectScript();
    /** Pick an effect from a filterable tree rather than a screen-tall list. */
    void slotBrowseEffects();
    /** Open the current Generator's .js in the editor. */
    void slotEditEffectScript();
    void slotEffectParamChanged(int value);
    void slotEffectEnumParamChanged(int index);
    void slotEffectPaletteBindingChanged(int index);
    void slotEffectTargetBindingChanged(int index);
    void slotEffectBindInput();
    /** Toggle Learn-range capture on/off (button click). */
    void slotLearnRangeClicked();
    /** Live MIDI while learning: track lowest/highest note played. */
    void slotLearnMidiInput(quint32 universe, quint32 channel, uchar value,
                            const QString &key);

private:
    void rebuildEffectDynWidget();

    /** Fill the effect picker combo with presets + raw scripts grouped by
     *  category, and select the entry matching palette @p p. */
    /** WLED's effects by name: a WLED fixture's in the show (its firmware's
     *  list), else the installed WLED definition's. Empty when neither. */
    QStringList wledEffectNames() const;
    void populateEffectPicker(QLCPalette *p);

    /** Auto-set palette name from its value when it still has the factory
     *  default name ("New Color", "New Dimmer", "New Effect"). */
    void maybeAutoName(QLCPalette *p);

    /** True if @p path is the bare default or an auto-generated
     *  Effect/<Category>/<Engine> folder (i.e. safe to re-derive). */
    bool isAutoEffectPath(const QString &path) const;

    /** Move an Effect palette into Palettes/Effect/<category>/<engine>/ to
     *  mirror the picker, unless the user has chosen a custom folder. */
    void maybeAutoPath(QLCPalette *p, const QString &category, const QString &engine);
};

/** @} */

#endif
