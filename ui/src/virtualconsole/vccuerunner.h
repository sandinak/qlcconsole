/*
  Q Light Controller Plus
  vccuerunner.h

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

#ifndef VCCUERUNNER_H
#define VCCUERUNNER_H

#include <QKeySequence>

#include "vcwidget.h"
#include "cuerunner.h"

#define KXMLQLCVCCueRunner              QStringLiteral("CueRunner")
#define KXMLQLCVCCueRunnerFollow        QStringLiteral("Follow")
#define KXMLQLCVCCueRunnerSize          QStringLiteral("Size")
#define KXMLQLCVCCueRunnerUpcoming      QStringLiteral("Upcoming")
#define KXMLQLCVCCueRunnerTimecode      QStringLiteral("Timecode")
#define KXMLQLCVCCueRunnerSecondary     QStringLiteral("BackStop")
#define KXMLQLCVCCueRunnerSubGo         QStringLiteral("SubGo")
#define KXMLQLCVCCueRunnerButtonHeight  QStringLiteral("ButtonHeight")
#define KXMLQLCVCCueRunnerGoShare       QStringLiteral("GoShare")

/** @addtogroup ui_vc_widgets
 * @{
 */

/**
 * The Cue Runner on the Virtual Console (docs/design/SHOW_TREE_DESIGN.md §6B, D26; P7):
 * the Run tab's runner, in a widget beside faders, buttons and speed
 * masters -- compact (on now, next, big GO + Sub GO) or list (the whole
 * runner). Each follows a Show Tree cue list, or the current production;
 * several may follow different lists. They all show the same state, since
 * the state is the list's chaser and ShowControl's. The VC Cue List stays
 * for plain chasers; Swap widget (VirtualConsole::swapWidget) turns one into
 * the other in place.
 *
 * Each action (GO, Sub GO, Back, Sub Back, Pause, Stop, LOAD next / previous)
 * takes an external input source and a key, as a VC Cue List's buttons do,
 * with feedback: lit while the action can do something (Pause: while
 * paused). Design mode sizes the info and the buttons with the handles
 * inside; Operate locks them and runs the show.
 */
class VCCueRunner final : public VCWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(VCCueRunner)

public:
    VCCueRunner(QWidget *parent, Doc *doc);
    ~VCCueRunner();

    void setID(quint32 id) override;

    /** The cue list it follows (invalid: the current production). */
    quint32 follow() const;
    void setFollow(quint32 listId);
    CueRunner::Size runnerSize() const;
    /** Compact or List (Full is the Run tab's: taken as List). */
    void setRunnerSize(CueRunner::Size size);
    CueRunner *runner() const { return m_runner; }

    /*********************************************************************
     * Actions: external input, keys, feedback
     *********************************************************************/
public:
    /** The mappable actions; each is also its input source id. */
    enum Action
    {
        Go = 0,
        SubGo,
        Back,
        SubBack,
        Pause,
        Stop,
        LoadNext,
        LoadPrevious,
        ActionCount
    };

    static QString actionName(Action action);
    /** The XML tag of an action's key and input. */
    static QString actionTag(Action action);

    QKeySequence keySequence(Action action) const;
    void setKeySequence(Action action, const QKeySequence &keySequence);

    /** Do @a action now (an input, a key). */
    void trigger(Action action);

    /** Lit: the action can do something now (Pause: paused). */
    bool feedbackOn(Action action) const;
    /** The value last sent to @a action's feedback (-1: none yet). */
    int lastFeedback(Action action) const { return m_lastFeedback[action]; }

    void updateFeedback() override;

protected:
    void enableWidgetUI(bool enable) override;

protected slots:
    void slotKeyPressed(const QKeySequence &keySequence) override;
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value) override;
    void slotModeChanged(Doc::Mode mode) override;

private:
    /** Send what changed (@a force: all of it). */
    void sendActionFeedback(bool force);

    QKeySequence m_keys[ActionCount];
    uchar m_latestValue[ActionCount];
    int m_lastFeedback[ActionCount];

    /*********************************************************************
     * Clipboard
     *********************************************************************/
public:
    VCWidget *createCopy(VCWidget *parent) const override;
    bool copyFrom(const VCWidget *widget) override;

    /*********************************************************************
     * Properties
     *********************************************************************/
public:
    void editProperties() override;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool loadXML(QXmlStreamReader &root) override;
    bool saveXML(QXmlStreamWriter *doc) override;

private:
    /** The runner's minimum, plus the margins. */
    void syncMinimumSize();

    CueRunner *m_runner;
};

/** @} */

#endif
