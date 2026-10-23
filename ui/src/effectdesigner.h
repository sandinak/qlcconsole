/*
  Q Light Controller Plus
  effectdesigner.h

  The effect designer (WORKFLOW_DESIGN.md W4): one widget that designs an
  effect for a set of heads -- a Show Tree layer's, or the Try on bench's
  -- at full height: the effect browser (presets, Recent, WLED) beside the
  effect's settings (the Look Editor's: parameters, Spread, speed master,
  on the beat), the effect's own colour and dimmer, and a live preview on
  just those heads. What it makes is a draft until the host keeps it: a
  one-off on the layer, a new palette, an update to the palette it started
  from, or a preset for other shows.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTDESIGNER_H
#define EFFECTDESIGNER_H

#include <QPointer>
#include <QDialog>
#include <QWidget>

#include "showtreemodel.h"

class QPushButton;
class QMenu;
class QToolButton;
class QComboBox;
class QLabel;
class EffectBrowser;
class LookEditor;
class QLCPalette;
class Scene;
class Doc;

/** @addtogroup ui_functions
 * @{
 */

/**
 * The designer. A host gives it the heads (setTargets(), or nothing for the
 * bench) and, optionally, the effect to start from (setEffect()); the
 * designer works on a DRAFT -- a hidden one-off palette that a hidden
 * preview scene plays on those heads, in Design mode while on screen --
 * and reports what the user chose with effectChosen(). Nothing in the show
 * changes until then, and cancel() (or destroying it) leaves no trace.
 *
 * Hosts: EffectLayerDialog (a Show Tree layer), the Workbench's editor
 * pane (Build ▸ Effects), and EffectsPage.
 */
class EffectDesigner final : public QWidget
{
    Q_OBJECT

public:
    /** What effectChosen() made. */
    enum Outcome
    {
        OneOff,         ///< a one-off (inline) palette, for one layer
        NewPalette,     ///< a named palette, filed as the show's are
        UpdatedPalette  ///< the palette it started from, now as designed
    };
    Q_ENUM(Outcome)

    explicit EffectDesigner(Doc *doc, QWidget *parent = nullptr);
    ~EffectDesigner() override;

    /** The draft differs from what it was opened with (or last kept: saved
     *  as a palette or a preset, updated, kept on a layer, made a layer),
     *  so leaving would drop it. */
    bool hasUnkeptChanges() const;

    /************************************************************************
     * What it plays on
     ************************************************************************/

    /** The heads the effect is designed for: a layer's groups and fixtures
     *  (@a label says what they are, "AT-W-ALL (6)"). Both empty: the
     *  bench, which plays on the Try on groups. */
    void setTargets(const QList<quint32> &groups, const QList<quint32> &fixtures,
                    const QString &label = QString());
    bool isBench() const { return m_groups.isEmpty() && m_fixtures.isEmpty(); }
    QList<quint32> targetGroups() const { return m_groups; }
    QList<quint32> targetFixtures() const { return m_fixtures; }

    /** The bench's groups (remembered between sessions, by name). */
    QList<quint32> tryOnGroups() const { return m_tryOn; }
    void setTryOnGroups(const QList<quint32> &groups);

    /** Looks played UNDER the effect in the preview -- a layer's colour and
     *  dimmer -- so it looks as it will in the cue. Not part of the result. */
    void setBaseLooks(const QList<quint32> &paletteIds);

    /************************************************************************
     * What it designs
     ************************************************************************/

    /** Start from @a paletteId, an Effect palette: its settings are copied
     *  into the draft, and the original changes only on updateOriginal().
     *  Invalid: a new effect. @a own are the effect's own Colour / Dimmer
     *  palettes (ShowTree::Look::own). */
    void setEffect(quint32 paletteId, const QList<quint32> &own = QList<quint32>());
    /** The palette it started from (invalid: a new one). */
    quint32 originalId() const { return m_originalId; }
    /** The draft being designed (invalid once the result is handed over). */
    quint32 draftId() const { return m_draftId; }
    QLCPalette *draft() const;
    /** The effect's own Colour and Dimmer palettes, as chosen. */
    QList<quint32> ownLooks() const;
    void setOwnLooks(const QList<quint32> &own);

    /** Pick an effect, preset or generator by its name in the browser
     *  ("Color Chase", "Matrix Fire"); false if there is none. */
    bool pickEffect(const QString &name);
    /** What the draft runs, as a person reads it ("Matrix Fire"); empty
     *  when nothing is picked yet. */
    QString effectName() const;
    /** The draft differs from what it started from. */
    bool isModified() const;

    /************************************************************************
     * What the host offers
     ************************************************************************/

    /** Offer "Keep on this layer" (a one-off): only a host with a layer. */
    void setOneOffAllowed(bool allowed);
    /** What a one-off is named after the effect, "0.0.3 Announcer AT-W-ALL". */
    void setOneOffLabel(const QString &where) { m_oneOffWhere = where; }
    /** Show the In context / Solo choice (the host does what it means). */
    void setContextChoice(bool shown);
    bool solo() const { return m_solo; }
    void setSolo(bool solo);
    /** Show Cancel (a dialog host); a page host has none. */
    void setCancelVisible(bool visible);

    /************************************************************************
     * The outcomes (the buttons call these)
     ************************************************************************/

    /** Keep the draft as a one-off for the layer. Editing a layer's own
     *  one-off writes into that one instead of making another. Returns the
     *  palette, invalid if there's nothing to keep. */
    quint32 keepOneOff();
    /** Save as a named palette, filed as the show's palettes are. Empty
     *  @a name asks for one (the effect's name offered). */
    quint32 saveAsPalette(const QString &name = QString(), const QString &folder = QString());
    /** Write the design into the palette it started from. */
    bool updateOriginal();
    /** Throw the draft away. */
    void cancel();

    /************************************************************************
     * Make a layer (the bench): the Try on groups + this effect, as a new
     * layer of a Show Tree cue or in a Build scene
     ************************************************************************/

    /** Where the bench was opened from -- a cue (invalid: none), else a
     *  plain scene -- Make a layer's first choice. */
    void setCameFrom(quint32 cueId, quint32 sceneId);
    quint32 cameFromCue() const { return m_fromCue; }
    quint32 cameFromScene() const { return m_fromScene; }
    /** Why Make a layer can't now ("Pick an effect first"); empty: it can. */
    QString whyNoLayer() const;
    /** The palette a layer made now would play: the palette it started
     *  from, saved and unchanged; invalid: a one-off copy of the design
     *  (made by the make*() calls, as Keep on this layer makes one). */
    quint32 savedPaletteForLayer() const;

    /** A new layer of cue @a cueId on the Try on groups, its Effect look
     *  this effect (with its own looks): one Show Tree undo step. Returns
     *  the layer (invalid: none made). */
    quint32 makeLayerInCue(quint32 cueId);
    /** Plain scene @a sceneId gets the Try on groups (those it lacks) and
     *  this effect as a look: one undo step on Build's history. */
    bool makeLayerInScene(quint32 sceneId);
    /** A new scene named @a name (empty: asks, the effect and the groups
     *  offered) with the Try on groups and this effect: one undo step on
     *  Build's history. Returns its id (invalid: none made). */
    quint32 makeLayerInNewScene(const QString &name = QString());
    /** Make a layer ▾'s menu, as it is now (filled on show; tests). */
    QMenu *makeLayerMenu();

    /** Show a layer layerMade() reports: the cue's layer selected in the
     *  Show Tree (or in @a workbench, when the tree has it docked), the
     *  scene loaded in Build's Workbench (or @a workbench) with the look
     *  selected. Deferred: the designer may go with what it replaces. */
    static void revealLayer(Doc *doc, class Workbench *workbench, quint32 cueId, quint32 layerId,
                            quint32 sceneId, quint32 paletteId);

    /************************************************************************
     * Preview
     ************************************************************************/

    /** The hidden scene the preview runs (invalid when there's no draft). */
    quint32 previewSceneId() const { return m_sceneId; }
    bool isPreviewing() const;
    /** What the status line says ("▶ On the rig: 6 heads"). */
    QString previewText() const;

    /** The parts, for hosts and tests. */
    LookEditor *lookEditor() const { return m_look; }
    EffectBrowser *browser() const { return m_browser; }

signals:
    /** The user chose: @a paletteId plays the effect, with @a own. */
    void effectChosen(quint32 paletteId, const QList<quint32> &own, EffectDesigner::Outcome outcome);
    void cancelled();
    void soloChanged(bool solo);
    void presetSaved(const QString &name);
    /** Make a layer made one: layer @a layerId of cue @a cueId, or in
     *  scene @a sceneId (the other invalid); @a paletteId plays the effect. */
    void layerMade(quint32 cueId, quint32 layerId, quint32 sceneId, quint32 paletteId);

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    /** A draft (and its preview scene) to work on, made if there's none. */
    void ensureDraft();
    /** Rebuild the preview scene's targets and palette order. */
    void rebuildScene();
    /** Re-apply an edit to the running preview (or start it). */
    void refreshPreview(bool params);
    void startPreview();
    void stopPreview();
    /** Hand the result over: stop and delete the preview scene, and the
     *  draft too unless @a keepDraft. */
    void release(bool keepDraft);
    void updateHeader();
    void updateButtons();
    void fillOwnCombos();
    void fillTryOnMenu();
    void loadTryOn();
    void scheduleBrowserReload();
    void fillMakeLayerMenu();
    void updateMakeLayerButton();
    /** The palette a made layer plays (savedPaletteForLayer(), else a new
     *  one-off copy of the draft named for @a where; @a made: a one-off). */
    quint32 paletteForLayer(const QString &where, bool *made);
    /** A one-off paletteForLayer() made that nothing took: gone again. */
    void dropOneOff(quint32 paletteId);
    /** "Towers, Truss" */
    QString tryOnNames() const;
    /** The fixtures the preview plays on. */
    QList<quint32> previewFixtures() const;
    /** Keep the show's modified flag as it was across a change of our own. */
    template <typename F> void quietly(F f);

public:
    /** The bench's Try on groups as last picked (by name), in @a doc. */
    static QList<quint32> savedTryOnGroups(Doc *doc);
    /** Plug the designer into the Workbench's editor pane
     *  (Workbench::setEffectPaneFactory): the bench's requests -- Build ▸
     *  Effects -- get a designer on the Try on groups, told where it was
     *  opened from (EffectRequest::fromCue / fromScene) for Make a layer,
     *  whose new layer it then shows (revealLayer()). A cue layer's are
     *  declined for now (the Show Tree's Design effect… dialog does those),
     *  so the Workbench shows what it did before. */
    static void plugIntoWorkbench();

    /** Copy everything an Effect palette runs from @a from to @a to (not its
     *  name or folder). */
    static void copyEffect(const QLCPalette *from, QLCPalette *to);
    /** Whether two Effect palettes run the same thing. */
    static bool sameEffect(const QLCPalette *a, const QLCPalette *b);

private:
    QPointer<Doc> m_doc;
    QList<quint32> m_groups, m_fixtures;
    QString m_targetLabel;
    QList<quint32> m_tryOn;
    QList<quint32> m_base;
    quint32 m_originalId;
    quint32 m_draftId;
    quint32 m_sceneId;
    quint32 m_ownColour;
    quint32 m_ownDimmer;
    QStringList m_lastSignature;
    /** The draft as opened or last kept (hasUnkeptChanges()). */
    QString m_keptSignature;
    QString draftSignature() const;
    void markKept();
    QString m_oneOffWhere;
    quint32 m_fromCue;
    quint32 m_fromScene;
    bool m_oneOffAllowed = false;
    bool m_solo = false;
    bool m_reloadQueued = false;
    bool m_previewStarted = false;
    /** The show was saved (not modified) when this draft was made: Cancel
     *  puts it back that way. */
    bool m_cleanAtStart = false;

    QLabel *m_title;
    QLabel *m_onRig;
    QToolButton *m_tryOnBtn;
    QWidget *m_contextRow;
    QPushButton *m_contextBtn, *m_soloBtn;
    EffectBrowser *m_browser;
    QComboBox *m_effectCombo;   ///< the Look Editor's, which the browser mirrors
    LookEditor *m_look;
    QComboBox *m_ownColourCombo, *m_ownDimmerCombo;
    QPushButton *m_cancelBtn, *m_updateBtn, *m_savePalBtn, *m_keepBtn;
    QToolButton *m_makeLayerBtn = nullptr;
    QLabel *m_hint;
};

/**
 * The Show Tree's host: the designer for one layer of one cue, in a window
 * of its own. "Design effect…" on a layer (its ⋯ menu on an Effect row, or
 * the tree's right-click). What is kept goes on the layer as its Effect
 * look -- replacing the effect it had -- in one undo step.
 */
class EffectLayerDialog final : public QDialog
{
    Q_OBJECT

public:
    /** The layer @a layerId of cue @a cueId; a carried-in layer has no id
     *  of its own and is named by @a target (a value set on it makes it the
     *  cue's own, as the tree does). @a key: the Effect row it was opened
     *  from ("Effect.Color"), or empty / "Effect." for the layer's effect,
     *  if any. */
    EffectLayerDialog(Doc *doc, quint32 cueId, quint32 layerId, const ShowTree::Target &target,
                      const QString &key = QString(), QWidget *parent = nullptr);
    ~EffectLayerDialog() override;

    EffectDesigner *designer() const { return m_designer; }
    /** The Effect look the layer had (empty key: none). */
    ShowTree::Look previousLook() const { return m_previous; }

    /** Put @a paletteId (with @a own) on the layer as its effect. */
    bool apply(quint32 paletteId, const QList<quint32> &own);

    /** Open it for the layer and wait. True when an effect was put on it. */
    static bool design(Doc *doc, QWidget *parent, quint32 cueId, quint32 layerId,
                       const ShowTree::Target &target, const QString &key = QString());

private:
    Doc *m_doc;
    quint32 m_cueId;
    quint32 m_layerId;
    ShowTree::Target m_target;
    ShowTree::Look m_previous;
    bool m_previousOwn = false;
    quint32 m_previewedBefore;
    EffectDesigner *m_designer;
    bool m_applied = false;
};

/** @} */

#endif
