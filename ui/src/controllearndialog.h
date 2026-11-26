/*
  Q Light Controller Plus
  controllearndialog.h

  Copyright (C) Branson Matheson

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

#ifndef CONTROLLEARNDIALOG_H
#define CONTROLLEARNDIALOG_H

#include <QDialog>

#include "controlmap.h"
#include "controlowners.h"
#include "qlcui_export.h"

class ControlCollisionBox;
class QTreeWidgetItem;
class QTreeWidget;
class QPushButton;
class QComboBox;
class QAction;
class QLabel;
class QMenu;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * Assign to Control… (board Q310; it was Learn Trigger…): listens for the next MIDI note or CC, or OSC message, on
 * any input universe -- or a key on the computer keyboard, with its
 * modifiers -- and binds that control to fire a function, a Show Tree cue
 * list, cue or hit (the Control Map, ControlMap). Pressing another control
 * before OK takes that one instead; the mode says how a press acts. Remove
 * trigger takes an existing binding away. A key the app or the dialog needs
 * is refused, saying why (KeyTriggers::refusal()); Esc, Tab, Return, Space
 * and the arrows keep working the dialog.
 *
 * A control something else already listens to (another trigger at the same
 * place, a Virtual Console widget, the encoder bank, a master, the PMJ...)
 * is refused, naming it (ControlOwners, board Q297), with Move It Here: that
 * takes it from there and binds it here in one go. Two triggers at the same
 * place on a button may Step Through Them instead (Q303).
 *
 * Works: the trigger's place (Q295) -- where it was learned (the hit's
 * place, or the place a scene was learned at) by default, the places around
 * it, the cue live now, and everywhere. A GO on a cue list or a jump to a
 * cue defaults to everywhere (its own place would arm it only once it is
 * already live); the place right-clicked is offered. A line under it says what the control does
 * where once it is learned ("Scene: Warm everywhere, except Hit: Stab in cue
 * 2.3"): the narrowest live place has it (Q296).
 *
 * A cue list's window learns its GO or its Back (When pressed: GO, GO while
 * held, Back, Stop the list), each a target of its own, so one list can
 * have both. A GO, a Back or a hit is refused on a fader (its profile says
 * so, or it moved like one: ControlMap::learnSawFader()); a latching button
 * (ControlMap::learnSawLatching()) gets Start only, said, and the
 * window suggests setting it to momentary.
 *
 * The window is a list (Draw the Desk's, phase A): every control something
 * has, each with who has it, shown before anything is pressed -- this
 * computer's desk roles as this computer's, never taken -- and the free
 * controls of each patched profile. A press or a move of a control selects
 * its row (added when it was not listed) and fires NOTHING, in Design and
 * Operate: no trigger, widget, master or channel group hears a press while
 * the window is open (InputOutputMap::setAssignListening()); the emergency
 * stop still stops. A click on a row only finds it: the press on the
 * device chooses. While a Run client (the phone page, Companion) is
 * connected, a line says a Stream Deck key is set in Companion.
 *
 * Offered (Q325) on cue lists (GO and Back), cues and hits in every view;
 * on raw scenes and effects only in the Classic QLC+ view (setClassicView()).
 */
class QLC_UI_EXPORT ControlLearnDialog final : public QDialog
{
    Q_OBJECT

public:
    /** @a where: the place it is learned at (the default of Works). */
    ControlLearnDialog(Doc *doc, const ControlMap::Target &target, QWidget *parent = nullptr,
                       const ControlMap::Place &where = ControlMap::Place());
    ~ControlLearnDialog();

    /** Adds "Assign to Control…" (or, bound, "Trigger: <control>…") for
     *  @a target to @a menu; choosing it opens the dialog. Nothing for a
     *  scene or effect outside the Classic QLC+ view (Q325). */
    static QAction *addTo(QMenu *menu, Doc *doc, const ControlMap::Target &target, QWidget *parent,
                          const ControlMap::Place &where = ControlMap::Place());

    /** Whether a control is chosen (heard, or the one already bound). */
    bool hasControl() const { return m_universe != 0xFFFFFFFF; }
    quint32 universe() const { return m_universe; }
    quint32 channel() const { return m_channel; }
    ControlMap::Mode mode() const;
    /** What OK binds: the target the window opened on -- or, on a cue
     *  list, its GO or its Back, as When pressed says. */
    ControlMap::Target target() const;
    void setMode(ControlMap::Mode mode);
    /** Works: the place chosen. */
    ControlMap::Place place() const;
    void setPlace(const ControlMap::Place &place);
    /** What the control will do where, once learned (empty: no control yet). */
    QString placeText() const;

    /** The Classic QLC+ view is showing (App::setWorkspaceView()): only
     *  there are raw scenes and effects offered Assign to Control… (Q325). */
    static void setClassicView(bool classic);
    static bool classicView();

    /** The list's row of a control (nullptr: not listed). */
    QTreeWidgetItem *rowOf(quint32 universe, quint32 channel) const;
    /** The control the list has selected (0xFFFFFFFF: none). */
    quint32 selectedChannel() const;

public slots:
    void accept() override;
    void done(int r) override;

    /** A key pressed in the dialog (what its key filter does; public for
     *  the tests). True: learned or refused (used up); false: the dialog's
     *  own key, left to work the dialog. */
    bool learnKey(int key, Qt::KeyboardModifiers modifiers);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void slotHeard(quint32 universe, quint32 channel);
    void slotRemove();
    /** Move It Here: the owners let go, then this binds it (accept()). */
    void slotMoveHere();
    /** Step Through Them: kept beside the one(s) at this place (Q303). */
    void slotStepThrough();
    /** When pressed moved between a list's GO and its Back. */
    void slotModeChanged(int index);

private:
    void updateText();
    /** The list: every control with an owner and every patched profile's
     *  controls, each with who has it. */
    void buildList();
    /** The row of a control, added when it was not listed; its text read again. */
    QTreeWidgetItem *ensureRow(quint32 universe, quint32 channel);
    /** Column 0 wide enough for every control, @a row's (bold) in full. */
    void fitControlColumn(QTreeWidgetItem *row);
    /** The list wide enough for @a row's owner text in full (to a cap). */
    void fitOwnerText(QTreeWidgetItem *row);
    void updateRow(QTreeWidgetItem *row);
    /** A press or move heard (raw, before any listener): its row selected. */
    void selectControl(quint32 universe, quint32 channel);
    /** The Companion line, shown while a Run client is connected. */
    void updateCompanion();
    /** Who else listens to the control chosen now: refused when any. */
    void checkOwners();
    /** This dialog's own listener (excluded from the owners). */
    ControlOwners::Owner self() const;
    /** The place of Works' item @a index. */
    ControlMap::Place placeAt(int index) const;
    /** Whether a fader is refused for this target (a GO, a Back, a hit). */
    bool refusesFader() const;
    /** When pressed: each item's target type (a list's GO or Back). */
    static constexpr int kTypeRole = Qt::UserRole + 1;
    /** kTypeRole of a list's Stop the list: the GO's or the Back's, whichever
        the window is on (both may stop the list; Control Map table). */
    static constexpr int kEitherListType = -1;

private:
    Doc *m_doc;
    ControlMap::Target m_target;
    quint32 m_universe = 0xFFFFFFFF;
    quint32 m_channel = 0xFFFFFFFF;
    QLabel *m_heard;
    QLabel *m_note;
    QLabel *m_keyNote;
    QLabel *m_companion;
    QTreeWidget *m_list;
    QComboBox *m_mode;
    QComboBox *m_place;
    QLabel *m_placeNote;
    bool m_stepThrough = false;
    /** a latching button heard made Toggle Start only (once) */
    bool m_latchChose = false;
    quint64 m_latchChoseChannel = 0;   //!< the control whose latch guess changed the mode
    // The binding as the dialog opened: stepping through with siblings there.
    bool m_boundStepping = false;
    quint32 m_boundUniverse = 0xFFFFFFFF;
    quint32 m_boundChannel = 0xFFFFFFFF;
    ControlMap::Place m_boundPlace;
    bool keepsStepping() const;
    QPushButton *m_ok;
    QPushButton *m_remove;
    ControlCollisionBox *m_collision;
};

/** @} */

#endif
