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

#include "vcwidget.h"
#include "cuerunner.h"

#define KXMLQLCVCCueRunner          QStringLiteral("CueRunner")
#define KXMLQLCVCCueRunnerFollow    QStringLiteral("Follow")
#define KXMLQLCVCCueRunnerSize      QStringLiteral("Size")

/** @addtogroup ui_vc_widgets
 * @{
 */

/**
 * The Cue Runner on the Virtual Console (SHOW_TREE_DESIGN.md §6B, D26; P7):
 * the Run tab's runner, in a widget beside faders, buttons and speed
 * masters -- compact (on now, next, big GO + Sub GO) or list (the whole
 * runner). Each follows a Show Tree cue list, or the current production;
 * several may follow different lists. They all show the same state, since
 * the state is the list's chaser and ShowControl's. The VC Cue List stays
 * for plain chasers; Swap widget (VirtualConsole::swapWidget) turns one into
 * the other in place.
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

    /** No external input of its own (yet): GO comes from show control,
     *  the keys and the footer, as for the Run tab. */
    void updateFeedback() override {}

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
    CueRunner *m_runner;
};

/** @} */

#endif
