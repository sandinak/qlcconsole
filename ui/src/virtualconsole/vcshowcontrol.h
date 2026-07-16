/*
  Q Light Controller Plus
  vcshowcontrol.h

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

#ifndef VCSHOWCONTROL_H
#define VCSHOWCONTROL_H

#include "vcwidget.h"

class QToolButton;
class QLabel;
class Show;

/** @addtogroup ui_vc_widgets
 * @{
 */

#define KXMLQLCVCShowControl         QStringLiteral("ShowControl")
#define KXMLQLCVCShowControlFunction QStringLiteral("Function")
#define KXMLQLCVCShowControlCueInfo  QStringLiteral("CueInfo")
#define KXMLQLCVCShowControlPlay     QStringLiteral("Play")
#define KXMLQLCVCShowControlStop     QStringLiteral("Stop")
#define KXMLQLCVCShowControlFollow   QStringLiteral("Follow")
#define KXMLQLCVCShowControlSuspend  QStringLiteral("Suspend")

/**
 * VCShowControl — the runtime face of a Show timeline in the Virtual Console.
 * A compact transport + status strip bound to a Show: it shows the running
 * position (timecode), the current section marker and a control state chip, and
 * offers Play / Stop, Follow-MTC and Suspend-timeline (VC takeover) controls.
 * Each control is MIDI-mappable. It drives the bound Show directly (via the
 * MasterTimer), so a timed show can be operated live from the VC without the
 * Show tab being open.
 */
class VCShowControl final : public VCWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(VCShowControl)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    VCShowControl(QWidget *parent, Doc *doc);
    ~VCShowControl();

    /** @reimp */
    void setID(quint32 id) override;

    /*********************************************************************
     * Bound Show
     *********************************************************************/
public:
    /** Bind this widget to a Show function. */
    void setShow(quint32 id);
    quint32 show() const { return m_showID; }

    /** Show/hide the current + next cue lines (name + note of the running
     *  chaser inside the show). */
    void setShowCueInfo(bool show);
    bool showCueInfo() const { return m_showCueInfo; }

protected:
    Show *showFunction() const;
    FunctionParent functionParent() const;

private:
    quint32 m_showID;

    /*********************************************************************
     * Transport / state
     *********************************************************************/
public:
    /** Start / stop the bound show (from the current timeline position). */
    void playShow();
    void stopShow();
    /** Arm / disarm timecode follow on the bound show. */
    void toggleFollow();
    /** Suspend / resume the bound show's output (VC takeover). */
    void toggleSuspend();

protected slots:
    void slotPlayClicked();
    void slotStopClicked();
    void slotFollowClicked();
    void slotSuspendClicked();

    void slotShowTimeChanged(quint32 ms);
    void slotShowRunning(quint32 id);
    void slotShowStopped(quint32 id);
    /** Track the raw incoming MIDI Time Code even when the show isn't running
     *  (matches the footer chip). */
    void slotTimecodeChanged(quint32 ms);

private:
    /** Refresh all labels + button states from the bound show. */
    void refresh();
    /** Format @p ms into the big timecode label. */
    void setTimecodeLabel(quint32 ms);
    /** Update the current/next cue lines from the running chaser (if any). */
    void refreshCueInfo();
    /** The label for a step: its note (description), else the fired function
     *  name, else "Cue N". */
    QString cueText(class Chaser *chaser, int stepIdx) const;

    bool m_showCueInfo;

    QLabel *m_nameLabel;
    QLabel *m_statusLabel;
    QLabel *m_timeLabel;
    QLabel *m_sectionLabel;
    QLabel *m_currentCueLabel;
    QLabel *m_nextCueLabel;
    QToolButton *m_stopButton;
    QToolButton *m_playButton;
    QToolButton *m_followButton;
    QToolButton *m_suspendButton;

    /*********************************************************************
     * QLC+ Mode
     *********************************************************************/
public slots:
    void slotModeChanged(Doc::Mode mode) override;

    /*********************************************************************
     * External input
     *********************************************************************/
public:
    void updateFeedback() override;
    static const quint8 playInputSourceId;
    static const quint8 stopInputSourceId;
    static const quint8 followInputSourceId;
    static const quint8 suspendInputSourceId;

protected slots:
    void slotInputValueChanged(quint32 universe, quint32 channel, uchar value) override;

private:
    quint32 m_playLatest;
    quint32 m_stopLatest;
    quint32 m_followLatest;
    quint32 m_suspendLatest;

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
};

/** @} */

#endif
