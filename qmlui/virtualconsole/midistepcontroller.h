/*
  Q Light Controller Plus
  midistepcontroller.h

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

#ifndef MIDISTEPCONTROLLER_H
#define MIDISTEPCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QTime>

class VCCueList;
class Chaser;

/**
 * External MIDI Step Controller for VCCueList
 *
 * This class provides safe MIDI step selection functionality that bypasses
 * the problematic rapid setPlaybackIndex calls in VCCueList. It implements
 * rate limiting, queuing, and safer chaser control methods.
 *
 * MIDI Velocity Mapping:
 * - Single Velocity Mode: MIDI velocity 1-127 maps to Step 1-127 (internal index 0-126)
 * - Two Velocity Mode: MIDI velocities (1,1) to (127,127) map to Steps 1-16383
 * - MIDI velocity 0 is reserved and ignored in both modes
 * - Note: Uses MIDI velocity value, not note number, for step selection
 */
class MidiStepController : public QObject
{
    Q_OBJECT

public:
    explicit MidiStepController(VCCueList *parent = nullptr);
    virtual ~MidiStepController();

    /** Enable/disable MIDI step selection */
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /** Set two-note mode for extended step range (0-16383) */
    void setTwoNoteMode(bool enabled);
    bool twoNoteMode() const { return m_twoNoteMode; }

    /** Set timeout for two-note mode (milliseconds) */
    void setTimeout(int timeout);
    int timeout() const { return m_timeout; }

    /** Set debounce interval for rate limiting (milliseconds) */
    void setDebounceInterval(int interval);
    int debounceInterval() const { return m_debounceInterval; }

    /** Handle MIDI input for step selection (uses velocity value) */
    void handleFirstNote(uchar velocity);
    void handleSecondNote(uchar velocity);

private slots:
    /** Execute queued step change safely */
    void executeStepChange();
    
    /** Handle two-note timeout */
    void handleTwoNoteTimeout();

private:
    /** Queue a step change with rate limiting */
    void queueStepChange(int stepIndex);
    
    /** Get the attached chaser safely */
    Chaser* getChaser() const;
    
    /** Validate step index against chaser */
    bool isValidStep(int stepIndex) const;

private:
    VCCueList* m_cueList;           // Parent cue list
    
    // Configuration
    bool m_enabled;                 // MIDI step selection enabled
    bool m_twoNoteMode;            // Two-note mode for extended range
    int m_timeout;                 // Two-note timeout in milliseconds
    int m_debounceInterval;        // Debounce interval for rate limiting
    
    // Rate limiting
    QTimer* m_changeTimer;         // Timer for rate-limited step changes
    QTime m_lastChangeTime;        // Last change timestamp
    int m_pendingStep;             // Pending step index
    bool m_hasPendingChange;       // Has pending change
    
    // Two-note mode state
    QTimer* m_twoNoteTimer;        // Two-note timeout timer
    int m_firstNoteValue;          // First note value
    bool m_waitingForSecondNote;   // Waiting for second note
    
    // Constants
    static const int DEFAULT_DEBOUNCE_INTERVAL_MS = 100;  // Default debounce interval
    static const int MIN_DEBOUNCE_INTERVAL_MS = 10;       // Minimum debounce interval
    static const int MAX_DEBOUNCE_INTERVAL_MS = 1000;     // Maximum debounce interval
    static const int DEFAULT_TIMEOUT_MS = 500;            // Default two-note timeout
};

#endif // MIDISTEPCONTROLLER_H
