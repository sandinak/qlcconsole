/*
  Q Light Controller Plus
  midistepcontroller.cpp

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

#include "midistepcontroller.h"
#include "virtualconsole/vccuelist.h"
#include "chaser.h"
#include "chaserrunner.h"
#include "chaseraction.h"
#include "doc.h"

#include <QDebug>

// Static constant definitions
const int MidiStepController::MIN_DEBOUNCE_INTERVAL_MS;
const int MidiStepController::MAX_DEBOUNCE_INTERVAL_MS;
const int MidiStepController::DEFAULT_DEBOUNCE_INTERVAL_MS;
const int MidiStepController::DEFAULT_TIMEOUT_MS;

MidiStepController::MidiStepController(VCCueList *parent)
    : QObject(parent)
    , m_cueList(parent)
    , m_enabled(false)
    , m_twoNoteMode(false)
    , m_timeout(DEFAULT_TIMEOUT_MS)
    , m_debounceInterval(DEFAULT_DEBOUNCE_INTERVAL_MS)
    , m_changeTimer(new QTimer(this))
    , m_pendingStep(-1)
    , m_hasPendingChange(false)
    , m_twoNoteTimer(new QTimer(this))
    , m_firstNoteValue(-1)
    , m_waitingForSecondNote(false)
{
    // Setup change timer for rate limiting
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(m_debounceInterval);
    connect(m_changeTimer, &QTimer::timeout, this, &MidiStepController::executeStepChange);
    
    // Setup two-note timeout timer
    m_twoNoteTimer->setSingleShot(true);
    connect(m_twoNoteTimer, &QTimer::timeout, this, &MidiStepController::handleTwoNoteTimeout);
}

MidiStepController::~MidiStepController()
{
    // Stop timers
    if (m_changeTimer)
        m_changeTimer->stop();
    if (m_twoNoteTimer)
        m_twoNoteTimer->stop();
}

void MidiStepController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
        
    m_enabled = enabled;
    
    if (!enabled)
    {
        // Reset state when disabled
        m_changeTimer->stop();
        m_twoNoteTimer->stop();
        m_hasPendingChange = false;
        m_waitingForSecondNote = false;
        m_firstNoteValue = -1;
        m_pendingStep = -1;
    }
}

void MidiStepController::setTwoNoteMode(bool enabled)
{
    if (m_twoNoteMode == enabled)
        return;
        
    m_twoNoteMode = enabled;
    
    // Reset two-note state when mode changes
    m_twoNoteTimer->stop();
    m_waitingForSecondNote = false;
    m_firstNoteValue = -1;
}

void MidiStepController::setTimeout(int timeout)
{
    m_timeout = qMax(100, timeout); // Minimum 100ms timeout
    m_twoNoteTimer->setInterval(m_timeout);
}

void MidiStepController::setDebounceInterval(int interval)
{
    // Clamp debounce interval to valid range
    m_debounceInterval = qBound(MIN_DEBOUNCE_INTERVAL_MS, interval, MAX_DEBOUNCE_INTERVAL_MS);

    // Update the change timer interval
    m_changeTimer->setInterval(m_debounceInterval);

    qDebug() << "MidiStepController: Debounce interval set to" << m_debounceInterval << "ms";
}

void MidiStepController::handleFirstNote(uchar velocity)
{
    if (!m_enabled || !m_cueList)
        return;

    qDebug() << "MidiStepController::handleFirstNote: velocity=" << velocity << "twoNoteMode:" << m_twoNoteMode;

    if (m_twoNoteMode)
    {
        // Two-note mode: store first velocity and wait for second
        m_firstNoteValue = velocity;
        m_waitingForSecondNote = true;
        m_twoNoteTimer->start(m_timeout);
        qDebug() << "MidiStepController: Waiting for second velocity, timeout:" << m_timeout << "ms";
    }
    else
    {
        // Single velocity mode: MIDI velocity 1 = Step 1 (internal index 0)
        // MIDI velocity 0 is reserved for "no step" or special functions
        if (velocity == 0)
        {
            qDebug() << "MidiStepController: MIDI velocity 0 ignored (reserved)";
            return;
        }

        int stepIndex = velocity - 1; // Convert MIDI velocity 1-127 to step index 0-126
        queueStepChange(stepIndex);
    }
}

void MidiStepController::handleSecondNote(uchar velocity)
{
    if (!m_enabled || !m_cueList || !m_twoNoteMode || !m_waitingForSecondNote)
        return;

    qDebug() << "MidiStepController::handleSecondNote: velocity=" << velocity;

    // Stop timeout timer
    m_twoNoteTimer->stop();

    // Calculate target step: (first velocity * 128 + second velocity) - 1
    // This maps MIDI velocities 1,1 to step index 0 (Step 1 in UI)
    int midiValue = (m_firstNoteValue * 128) + velocity;
    int targetStep = (midiValue == 0) ? -1 : midiValue - 1; // Reserve 0,0 for "no step"

    qDebug() << "MidiStepController: Two-velocity calculation:" << m_firstNoteValue << "*128 +" << velocity << "=" << midiValue << "-> step index" << targetStep;

    // Reset two-note state
    m_waitingForSecondNote = false;
    m_firstNoteValue = -1;

    // Skip if reserved value
    if (targetStep < 0)
    {
        qDebug() << "MidiStepController: MIDI velocities 0,0 ignored (reserved)";
        return;
    }

    // Queue the step change
    queueStepChange(targetStep);
}

void MidiStepController::handleTwoNoteTimeout()
{
    if (!m_waitingForSecondNote)
        return;

    qDebug() << "MidiStepController: Two-velocity timeout, using first velocity value:" << m_firstNoteValue;

    // Timeout: use first velocity value as single step (apply same 1-based mapping)
    if (m_firstNoteValue == 0)
    {
        qDebug() << "MidiStepController: MIDI velocity 0 timeout ignored (reserved)";
        m_waitingForSecondNote = false;
        m_firstNoteValue = -1;
        return;
    }

    int stepIndex = m_firstNoteValue - 1; // Convert MIDI velocity 1-127 to step index 0-126

    // Reset two-note state
    m_waitingForSecondNote = false;
    m_firstNoteValue = -1;

    // Queue the step change
    queueStepChange(stepIndex);
}

void MidiStepController::queueStepChange(int stepIndex)
{
    if (!m_enabled || !m_cueList)
        return;
        
    // Validate step index
    if (!isValidStep(stepIndex))
    {
        qWarning() << "MidiStepController: Invalid step index:" << stepIndex;
        return;
    }
    
    qDebug() << "MidiStepController::queueStepChange:" << stepIndex;
    
    // Rate limiting check using configurable debounce interval
    QTime currentTime = QTime::currentTime();
    if (m_lastChangeTime.isValid() && m_lastChangeTime.msecsTo(currentTime) < m_debounceInterval)
    {
        // Too soon - queue for later
        m_pendingStep = stepIndex;
        m_hasPendingChange = true;

        if (!m_changeTimer->isActive())
        {
            int remainingTime = m_debounceInterval - m_lastChangeTime.msecsTo(currentTime);
            m_changeTimer->start(qMax(10, remainingTime));
        }

        qDebug() << "MidiStepController: Rate limited (debounce:" << m_debounceInterval << "ms), queuing step" << stepIndex;
        return;
    }
    
    // Execute immediately
    m_pendingStep = stepIndex;
    m_hasPendingChange = true;
    executeStepChange();
}

void MidiStepController::executeStepChange()
{
    if (!m_enabled || !m_cueList || !m_hasPendingChange)
        return;
        
    int stepIndex = m_pendingStep;
    m_hasPendingChange = false;
    m_pendingStep = -1;
    
    qDebug() << "MidiStepController::executeStepChange:" << stepIndex;
    
    // Validate step index again
    if (!isValidStep(stepIndex))
    {
        qWarning() << "MidiStepController: Invalid step index at execution:" << stepIndex;
        return;
    }
    
    // Get chaser safely
    Chaser* chaser = getChaser();
    if (!chaser)
    {
        qWarning() << "MidiStepController: No chaser attached";
        return;
    }
    
    // Use safer chaser control method instead of setPlaybackIndex
    if (chaser->isRunning())
    {
        // Chaser is running - use ChaserAction for safe step change
        ChaserAction action;
        action.m_action = ChaserSetStepIndex;
        action.m_stepIndex = stepIndex;
        action.m_masterIntensity = m_cueList->intensity();
        action.m_stepIntensity = m_cueList->getPrimaryIntensity();
        // Force FromFunction fade mode to preserve fade in/out times
        action.m_fadeMode = Chaser::FromFunction;

        chaser->setAction(action);
        qDebug() << "MidiStepController: Set chaser action to step" << stepIndex << "with FromFunction fade mode";
    }
    else
    {
        // Chaser not running - start it at the specified step
        m_cueList->startChaser(stepIndex);
        qDebug() << "MidiStepController: Started chaser at step" << stepIndex;
    }
    
    // Update timestamp
    m_lastChangeTime = QTime::currentTime();
}

Chaser* MidiStepController::getChaser() const
{
    if (!m_cueList)
        return nullptr;
        
    return m_cueList->chaser();
}

bool MidiStepController::isValidStep(int stepIndex) const
{
    if (stepIndex < 0)
        return false;
        
    Chaser* chaser = getChaser();
    if (!chaser)
        return false;
        
    return stepIndex < chaser->stepsCount();
}
