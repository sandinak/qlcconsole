/*
  Q Light Controller Plus
  controlcollisionbox.h

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

#ifndef CONTROLCOLLISIONBOX_H
#define CONTROLCOLLISIONBOX_H

#include <QFrame>

#include "controlowners.h"
#include "qlcui_export.h"

class QPushButton;
class QLabel;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The refusal every Learn shows when the control it heard already has a
 * job (ControlOwners, board Q297): "<control> already does <thing>
 * (<where>). One job per control." with [Move It Here] [Cancel]. Move It
 * Here is not there when the owner can't let go (the PMJ, the emergency
 * stop); the reason is said instead. Two triggers at the same place on a
 * button may instead Step Through Them (Q303). Hidden until a refusal; the Learn
 * that owns it takes the control on moveRequested(), after
 * ControlOwners::releaseAll() -- or, in a window that applies on OK, when
 * that is accepted (setMovesOnOk()).
 */
class QLC_UI_EXPORT ControlCollisionBox final : public QFrame
{
    Q_OBJECT

public:
    explicit ControlCollisionBox(QWidget *parent = nullptr);

    /** Shows the refusal for @a owners of a control. */
    void refuse(Doc *doc, quint32 universe, quint32 channel, const QList<ControlOwners::Owner> &owners);
    /** Shows the refusal for a run of controls (the encoder bank's): each
     *  owner named with its own control (ControlOwners::runRefusal()).
     *  @a taken: (channel, owner); channel() is the first taken one. */
    void refuseRun(Doc *doc, quint32 universe, const QList<QPair<quint32, ControlOwners::Owner>> &taken);
    /** Offers Step Through Them (Q303): the owners are triggers at the same
     *  place and the control is a button, so it may fire each in turn. */
    void setStepThrough(bool offered);
    bool offersStepThrough() const { return m_stepOffered; }
    QPushButton *stepButton() const { return m_step; }
    /** In a window that applies on OK, the note says Move It Here takes
     *  the control only then (ControlOwners::moveOnOkNote()). */
    void setMovesOnOk(bool onOk);
    bool movesOnOk() const { return m_onOk; }
    /** Adds a line under the refusal (the surface pages' jobs of the
     *  control, which a move doesn't take). */
    void appendText(const QString &line);
    /** Hides it and forgets the owners. */
    void clear();

    bool isRefusing() const { return !m_owners.isEmpty(); }
    QList<ControlOwners::Owner> owners() const { return m_owners; }
    quint32 universe() const { return m_universe; }
    quint32 channel() const { return m_channel; }
    QString text() const;
    QPushButton *moveButton() const { return m_move; }
    QPushButton *cancelButton() const { return m_cancel; }

signals:
    /** Move It Here was pressed (the owners are not released yet). */
    void moveRequested();
    /** Step Through Them was pressed: keep both, each press the next. */
    void stepThroughRequested();
    /** Cancel was pressed: the control is not taken. */
    void cancelled();

private:
    QLabel *m_text;
    QLabel *m_note;
    QPushButton *m_move;
    QPushButton *m_step;
    QPushButton *m_cancel;
    QLabel *m_stepNote;
    bool m_stepOffered = false;
    bool m_onOk = false;
    QList<ControlOwners::Owner> m_owners;
    quint32 m_universe = 0;
    quint32 m_channel = 0;
};

/** @} */

#endif
