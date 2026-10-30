/*
  Q Light Controller Plus
  vcbutton.h

  Copyright (c) Heikki Junnila
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

#ifndef VCBUTTON_H
#define VCBUTTON_H

#include <QKeySequence>
#include <QWidget>
#include <QIcon>

#include "vcwidget.h"
#include "function.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QMouseEvent;
class QPaintEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class VCButton;
class QAction;
class QPoint;
class QEvent;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCButton     QStringLiteral("Button")
#define KXMLQLCVCButtonIcon QStringLiteral("Icon")

#define KXMLQLCVCButtonFunction     QStringLiteral("Function")
#define KXMLQLCVCButtonFunctionID   QStringLiteral("ID")
#define KXMLQLCVCButtonFunctionHit  QStringLiteral("Hit")

#define KXMLQLCVCButtonAction           QStringLiteral("Action")
#define KXMLQLCVCButtonActionFlash      QStringLiteral("Flash")
#define KXMLQLCVCButtonActionToggle     QStringLiteral("Toggle")
#define KXMLQLCVCButtonActionBlackout   QStringLiteral("Blackout")
#define KXMLQLCVCButtonActionStopAll    QStringLiteral("StopAll")
#define KXMLQLCVCButtonActionSelect     QStringLiteral("SelectFixtures")
#define KXMLQLCVCButtonActionSaveProgrammer   QStringLiteral("SaveProgrammer")
#define KXMLQLCVCButtonActionRevertProgrammer QStringLiteral("RevertProgrammer")
#define KXMLQLCVCButtonActionPadModeSelect    QStringLiteral("PadModeSelect")
#define KXMLQLCVCButtonActionFixturePadCell   QStringLiteral("FixturePadCell")
#define KXMLQLCVCButtonActionChaserStepNext   QStringLiteral("ChaserStepNext")
#define KXMLQLCVCButtonActionChaserStepPrev   QStringLiteral("ChaserStepPrev")
#define KXMLQLCVCButtonActionSuspendTimeline  QStringLiteral("SuspendTimeline")
#define KXMLQLCVCButtonActionFollowTimecode   QStringLiteral("FollowTimecode")
#define KXMLQLCVCButtonActionClearLastLook    QStringLiteral("ClearLastLook")
#define KXMLQLCVCButtonAttrPadMode            QStringLiteral("PadMode")
#define KXMLQLCVCButtonAttrPadRow             QStringLiteral("PadRow")
#define KXMLQLCVCButtonAttrPadCol             QStringLiteral("PadCol")
#define KXMLQLCVCButtonSelectFixtures   QStringLiteral("SelectionFixtures")
#define KXMLQLCVCButtonSelectGroups     QStringLiteral("SelectionGroups")
#define KXMLQLCVCButtonSelectMode       QStringLiteral("SelectionMode")
#define KXMLQLCVCButtonSelectModeReplace QStringLiteral("Replace")
#define KXMLQLCVCButtonSelectModeAdd    QStringLiteral("Add")
#define KXMLQLCVCButtonSelectModeRemove QStringLiteral("Remove")
#define KXMLQLCVCButtonSelectModeToggle QStringLiteral("Toggle")

#define KXMLQLCVCButtonFlashOverride    QStringLiteral("Override")
#define KXMLQLCVCButtonFlashForceLTP    QStringLiteral("ForceLTP")

#define KXMLQLCVCButtonStopAllFadeTime  QStringLiteral("FadeOut")

#define KXMLQLCVCButtonKey QStringLiteral("Key")

#define KXMLQLCVCButtonIntensity        QStringLiteral("Intensity")
#define KXMLQLCVCButtonIntensityAdjust  QStringLiteral("Adjust")

class VCButton final : public VCWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(VCButton)

public:
    /** Default size for newly-created buttons */
    static const QSize defaultSize;

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCButton(QWidget* parent, Doc* doc);
    ~VCButton();

    /*********************************************************************
     * ID
     *********************************************************************/
public:
    /** @reimpl */
    void setID(quint32 id) override;

    /*********************************************************************
     * Clipboard
     *********************************************************************/
public:
    /** Create a copy of this widget to the given parent */
    VCWidget* createCopy(VCWidget* parent) const override;

protected:
    /** Copy the contents for this widget from another widget */
    bool copyFrom(const VCWidget* widget) override;

    /*********************************************************************
     * Properties
     *********************************************************************/
public:
    /** Edit this widget's properties */
    void editProperties() override;

    /*********************************************************************
     * Background image
     *********************************************************************/
public:
    /** Set the button's background image */
    void setBackgroundImage(const QString& path) override;

protected:
    QPixmap m_bgPixmap;

    /*********************************************************************
     * Background color
     *********************************************************************/
public:
    /** Set the button's background color */
    void setBackgroundColor(const QColor& color) override;

    /** Get the button's background color */
    QColor backgroundColor() const override;

    /** Reset the button's background color to whatever the platform uses */
    void resetBackgroundColor() override;

    /*********************************************************************
     * Foreground color
     *********************************************************************/
public:
    /** Set the button's foreground color */
    void setForegroundColor(const QColor& color) override;

    /** Get the button's foreground color */
    QColor foregroundColor() const override;

    /** Reset the button's foreground color to whatever the platform uses */
    virtual void resetForegroundColor() override;

    /*********************************************************************
     * Button icon
     *********************************************************************/
public:
    /** Get the button icon's path
        @return absolute path
     */
    QString iconPath() const;

    /** Set the icon's path */
    void setIconPath(const QString& iconPath);

private:

    /** Reload icon file from disk */
    void updateIcon();

public slots:
    void slotChooseIcon();
    void slotResetIcon();

protected:
    QString m_iconPath;
    QIcon m_icon;
    QSize m_iconSize;

    QAction* m_chooseIconAction;
    QAction* m_resetIconAction;

    /*********************************************************************
     * Function attachment
     *********************************************************************/
public:
    /**
     * Attach a function to a VCButton. This function is started when the
     * button is pressed down. A function other than its hit's: the button
     * no longer fires the hit (setHit()).
     *
     * @param function An ID of a function to attach
     */
    void setFunction(quint32 function);

    /**
     * Get the ID of the function attached to a VCButton
     *
     * @return The ID of the attached function or Function::invalidId()
     *         if there isn't one
     */
    quint32 function() const;

    /** @reimp */
    void adjustFunctionIntensity(Function *f, qreal value) override;

    /** @reimp */
    virtual void notifyFunctionStarting(quint32 fid, qreal intensity, bool excludeMonitored) override;

    /**
     * Fire a Show Tree hit (a look of a production's Hits section): the
     * button plays the hit's own Collection, found again by the hit when
     * the tree rebuilds or renames it. Invalid: no hit (setFunction() as
     * in QLC+). Saved as the Function element's "Hit" attribute.
     */
    void setHit(quint32 hitId);
    /** Its first hit (invalid: none). */
    quint32 hit() const { return m_hits.isEmpty() ? Function::invalidId() : m_hits.first(); }

    /**
     * Scoped hits (Q154): a button can hold several hits and fires the one
     * armed now with the narrowest place (cue > cue list > production >
     * global; ShowTreeController::armedHit()). None armed: it is dimmed and
     * does nothing. A hit it is holding when its place ends (the cue goes
     * on, the list stops) is let go over its fade out. Saved as the
     * Function element's "Hit" (the first) and a <Hit> child for each of
     * the others, which an older build skips (it fires the first).
     */
    void setHits(const QList<quint32> &hitIds);
    QList<quint32> hits() const { return m_hits; }
    /** Add hits (a drag from the Show Tree): those it hasn't yet, at the
     *  end. A button firing a plain function fires the hits instead; one
     *  not a Toggle or Flash becomes a Toggle. False: nothing added. */
    bool addHits(const QList<quint32> &hitIds);
    /** The hit it fires now (invalid: none armed, or no hits). */
    quint32 armedHit() const { return m_armedHit; }
    /** MIME type of hits dragged from the Show Tree (ShowTreeView). */
    static const char *hitMimeType();

    /** Several of its hits at the same place (Q157): how a press fires
     *  them there -- each press the next (StepThrough), or all at once
     *  (Together). Asked when the second is added; Unset: not asked yet
     *  (it fires the first). Saved as the Function element's SamePlace. */
    enum SamePlace { SamePlaceUnset, StepThrough, Together };
    void setSamePlace(SamePlace mode) { m_samePlace = mode; }
    SamePlace samePlace() const { return m_samePlace; }
    /** addHits() asking first, as the drop and Add hit… do (Q157): a hit
     *  at the place of one it has -- Step through, Together, Replace or
     *  Cancel; a hit narrower than one it has (or wider) -- override there?
     *  The answers come from HitMap::ask(). False: nothing added. */
    bool addHitsAsking(const QList<quint32> &hitIds, QWidget *parent);
    /** The armed hit fires instead of a wider armed one (the face's ↳). */
    bool armedOverrides() const;

protected:
    /** setFunction() without touching the hit. */
    void attachFunction(quint32 fid);

protected slots:
    /** Invalidates the button's function if the function is destroyed */
    void slotFunctionRemoved(quint32 fid);
    /** The Show Tree changed, or what is armed: a hit's button follows its
     *  armed hit's Collection. */
    void slotShowTreeChanged();

protected:
    /** ID of the Function that this button is controlling */
    quint32 m_function;
    /** The Show Tree hits it fires, and the one armed now */
    QList<quint32> m_hits;
    quint32 m_armedHit;
    SamePlace m_samePlace = SamePlaceUnset;
    /** Step through: presses so far. Together: the others it started. */
    int m_hitStep = 0;
    QList<quint32> m_together;
    /** Before a press fires: which of a same-place tier (Q157). */
    void prepareHitPress();
    /** The Together others: 0 stop, 1 start, 2 flash, 3 unflash. */
    void together(int op);

    /*********************************************************************
     * Button state
     *********************************************************************/
public:
    enum ButtonState
    {
        Inactive,
        Monitoring,
        Active
    };

    void setState(ButtonState state);
    ButtonState state() const;
    void updateState();

signals:
    /** Signal emitted when the button has actually changed the graphic state */
    void stateChanged(int state);

protected:
    /* Set here, not by setState()/setAction() in the constructor: those
       compare with the value before, and read it uninitialised (M3). */
    ButtonState m_state = Inactive;
    bool m_ledStyle;

    /*********************************************************************
     * Key sequence handler
     *********************************************************************/
public:
    void setKeySequence(const QKeySequence& keySequence);
    QKeySequence keySequence() const;

protected slots:
    void slotKeyPressed(const QKeySequence& keySequence) override;
    void slotKeyReleased(const QKeySequence& keySequence) override;

protected:
    QKeySequence m_keySequence;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    void updateFeedback() override;

protected slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value) override;

    /*********************************************************************
     * Button action
     *********************************************************************/
public:
    /**
     * Toggle: Start/stop the assigned function.
     * Flash: Keep the function running as long as the button is kept down.
     * Blackout: Toggle blackout on/off.
     * StopAll: Stop all functions (panic button).
     * SelectFixtures: Modify the global programmer selection (replace,
     *                 add, remove, or toggle a configured fixture set).
     */
    enum Action { Toggle, Flash, Blackout, StopAll, SelectFixtures,
                  SaveProgrammer, RevertProgrammer,
                  PadModeSelect, FixturePadCell,
                  ChaserStepNext, ChaserStepPrev,
                  SuspendTimeline, FollowTimecode, ClearLastLook };

    /** SelectFixtures sub-mode: how the click affects the selection. */
    enum SelectionMode { SelectReplace, SelectAdd, SelectRemove, SelectToggle };

    /** Set this button's action */
    void setAction(Action action);

    /** Get this button's action */
    Action action() const;

    static QString actionToString(Action action);
    static Action stringToAction(const QString& str);

    void setStopAllFadeOutTime(int ms);
    int stopAllFadeTime() const;

    /*********************************************************************
     * SelectFixtures (Programmer mode)
     *********************************************************************/
public:
    /** Set the explicit fixtures this button targets. */
    void setSelectionFixtures(const QList<quint32>& fixtureIds);
    QList<quint32> selectionFixtures() const;

    /** Set the fixture-group ids whose members this button targets.
        Members are resolved at click time. */
    void setSelectionGroups(const QList<quint32>& groupIds);
    QList<quint32> selectionGroups() const;

    /** How the button's fixtures affect the global selection. */
    void setSelectionMode(SelectionMode mode);
    SelectionMode selectionMode() const;

    static QString selectionModeToString(SelectionMode mode);
    static SelectionMode stringToSelectionMode(const QString& str);

    /** Resolve groups + explicit ids into the final fixture id set. */
    QList<quint32> resolveSelectionTargets() const;

    /** SaveProgrammer-action: open the Save dialog -- one row per
        Doc::editedSceneIds() entry (an already-running scene the
        programmer live-edited) and one per Doc::proposedSaveBuckets()
        entry (raw painted values, smart-categorized into new Scenes via
        Doc::saveBucketAsScene()/saveBucketAsGroupScene()) -- then clears
        the programmer values once every row's choice has been applied.
        NOT Doc::saveProgrammerAsScene(): that simpler, always-new-Scene
        method is dead code from this button's perspective -- nothing
        calls it. */
    void saveProgrammer();

    /** PadModeSelect: which pad-grid mode this button activates when
        pressed (and reflects the LED of when it equals doc->padMode()). */
    Doc::PadMode padMode() const { return m_padMode; }
    void setPadMode(Doc::PadMode mode);

    /** FixturePadCell: zero-based row/col within the 5×8 pad grid.
        Maps to fixture index `(row * 8 + col)` of the active group. */
    int padRow() const { return m_padRow; }
    int padCol() const { return m_padCol; }
    void setPadCell(int row, int col);

private:
    /** FixturePadCell only: resolve the fixture this pad represents
        based on the current active programmer group, or invalidId
        if there is no active group / fewer fixtures than the index. */
    quint32 resolveFixturePadFixture() const;

protected slots:
    /** Re-evaluate isOn() based on current programmer selection. */
    void slotProgrammerSelectionChanged();

    /** SaveProgrammer-action handler for Doc::programmerDirtyChanged.
        Drives the blink-while-dirty LED feedback + repaint. */
    void slotProgrammerDirtyChanged(bool dirty);

    /** SaveProgrammer-action blink tick — flips m_dirtyBlinkPhase and
        re-sends LED feedback so the hardware LED actually toggles. */
    void slotProgrammerDirtyBlink();

    /** PadModeSelect: re-evaluate Active state against doc->padMode(). */
    void slotPadModeChanged(Doc::PadMode mode);

    /** FixturePadCell: re-evaluate Active state from sub-selection. */
    void slotProgrammerSubSelectionChanged();

protected:
    QList<quint32> m_selectionFixtures;
    QList<quint32> m_selectionGroups;
    SelectionMode m_selectionMode;

    Action m_action = Toggle;
    int m_blackoutFadeOutTime;

    /** SaveProgrammer-action only: 500ms timer that toggles
        m_dirtyBlinkPhase while the programmer is dirty. */
    QTimer *m_dirtyBlinkTimer = nullptr;
    bool m_dirtyBlinkPhase = false;

    /** PadModeSelect only: which mode this button drives. */
    Doc::PadMode m_padMode = Doc::PadModeOff;

    /** FixturePadCell only: pad coordinates (zero-based, row 0 = top). */
    int m_padRow = -1;
    int m_padCol = -1;

    /*********************************************************************
     * Startup intensity adjustment
     *********************************************************************/
public:
    /**
     * Make the button adjust the attached function's intensity when the
     * button is used to start the function.
     *
     * @param adjust true to make the button adjust intensity, false to disable
     *               intensity adjustment
     */
    void enableStartupIntensity(bool enable);

    /** Check, whether the button adjusts intensity */
    bool isStartupIntensityEnabled() const;

    /**
     * Set the amount of intensity adjustment.
     *
     * @param fraction Intensity adjustment amount (0.0 - 1.0)
     */
    void setStartupIntensity(qreal fraction);

    /** Get the amount of intensity adjustment. */
    qreal startupIntensity() const;

protected:
    bool m_startupIntensityEnabled;
    qreal m_startupIntensity;

protected slots:
    void slotAttributeChanged(int value);

    /*****************************************************************************
    * Flash Properties
    *****************************************************************************/
public:
    /** Gets if flashing overrides newer values */
    bool flashOverrides() const;
    /** Sets if flashing should override values */
    void setFlashOverride(bool shouldOverride);
    /** Gets if flash channels should behave like LTP channels */
    bool flashForceLTP() const;
    /** Sets if the flash channels should behave like LTP channels */
    void setFlashForceLTP(bool forceLTP);

private:
    bool m_flashOverrides;
    bool m_flashForceLTP;


    /*********************************************************************
     * Button press / release handlers
     *********************************************************************/
public:
    /** Handler for button presses (mouse/key)button down, not click */
    void pressFunction();

    /** Handler for button releases (mouse/key)button up, not click */
    void releaseFunction();

    /** Blink the button for $ms milliseconds */
    void blink(int ms);

private:
    FunctionParent functionParent() const;

protected slots:
    /** Handler for function running signal */
    void slotFunctionRunning(quint32 fid);

    /** Handler for function stop signal */
    void slotFunctionStopped(quint32 fid);

    /** Basically the same as slotFunctionStopped() but for flash signal */
    void slotFunctionFlashing(quint32 fid, bool state);

    /** Slot for brief widget blink when controlled function stops */
    void slotBlink();

    /** Listent to OutputMap::blackoutChanged() signals in Blackout mode */
    void slotBlackoutChanged(bool state);

    /** SuspendTimeline: reflect the running timeline's suspended state in the
     *  button LED (active = suspended / VC in control). */
    void slotTimelineControlChanged();

    /** FollowTimecode: reflect the current show's follow-arm state (active =
     *  following). */
    void slotFollowTimecodeChanged(bool enabled);

protected:
    /** Check if the button's parent is a VCSoloFrame */
    bool isChildOfSoloFrame() const;

    /*********************************************************************
    * Custom menu
    *********************************************************************/
public:
    /** Get a custom menu specific to this widget. Must be deleted. */
    QMenu* customMenu(QMenu* parentMenu) const override;

    /*********************************************************************
     * Intensity
     *********************************************************************/
public:
    /** @reimp */
    void adjustIntensity(qreal val) override;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    /**
     * Load a VCButton's properties from an XML document node
     *
     * @param doc An XML document to load from
     * @param btn_root A VCButton XML root node containing button properties
     * @return true if successful; otherwise false
     */
    bool loadXML(QXmlStreamReader &root) override;

    /**
     * Save a VCButton's properties to an XML document node
     *
     * @param doc The master XML document to save to
     * @param frame_root The button's VCFrame XML parent node to save to
     */
    bool saveXML(QXmlStreamWriter *doc) override;

    /*********************************************************************
     * Event Handlers
     *********************************************************************/
protected:
    void paintEvent(QPaintEvent* e) override;
    /** Hits dragged from the Show Tree (Q154) */
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
};

/** @} */

#endif
