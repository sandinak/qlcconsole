/*
  Q Light Controller Plus
  mappingcheck.h

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

#ifndef MAPPINGCHECK_H
#define MAPPINGCHECK_H

#include <QStringList>
#include <QList>
#include <QSet>

#include "padnames.h"
#include "gamepadrole.h"

/** @addtogroup ui UI
 * @{
 */

/**
 * The walk behind Controller Test's Check Mapping: one control at a time, in
 * the pad's own printed names, each passing when that GamepadRole arrives
 * from the controller. A different role is a mismatch ("Expected B, read
 * X"). No Qt widgets and no engine: the dialog feeds it roles and shows what
 * it says.
 */
class MappingCheck
{
public:
    enum State { Pending, Passed, Wrong, Skipped };
    enum Result { Ignored, Matched, Mismatch };

    struct Step
    {
        GamepadRole::Role role;
        QString name;      ///< as printed on the pad ("Cross (\342\234\225)", "L1", "Left stick up")
        QString hint;      ///< where it is, in plain words
        QString action;    ///< "Press", "Pull ... fully", "Push"
    };

    explicit MappingCheck(PadNames::Family family = PadNames::Xbox);

    PadNames::Family family() const { return m_family; }
    int count() const { return m_steps.size(); }
    const Step &step(int i) const { return m_steps.at(i); }
    State state(int i) const { return m_states.at(i); }
    /** For a Wrong control: the name of what arrived instead. */
    QString readAs(int i) const { return m_read.at(i); }

    /** The control being asked for; -1 once the walk is over. */
    int current() const { return m_current; }
    bool finished() const { return m_current < 0; }
    /** "Press B \342\200\224 the red right face button". */
    QString prompt() const;

    /** A role arrived with @p value (0..255; buttons are 255 down, 0 up).
     *  Matched when it is the control asked for, Mismatch when it is
     *  another control pressed (or pushed) in earnest, Ignored for a
     *  release, a touchpad finger, a stick at rest or a role already
     *  counted. */
    Result feed(GamepadRole::Role role, int value);

    /** Ask for the current control again (after a mismatch). */
    void retry();
    /** Leave the current control: a Wrong one stays Wrong, else Skipped. */
    void skip();
    /** End the walk where it is; what was not reached is Skipped. */
    void stop();

    int passedCount() const;
    int wrongCount() const;
    int skippedCount() const;
    /** "14 of 16 match; 2 wrong: B read as X, ...". */
    QString summary() const;
    /** Every control, one line each, then the summary: what the log holds. */
    QString report() const;
    /** "Expected B, read X" for a Wrong control. */
    QString mismatchText(int i) const;
    /** One checklist line: "\342\234\223 A", "\342\234\227 B (read X)", "- LT (skipped)", "\342\200\246 X". */
    QString line(int i) const;

    /** What @p role is called on a pad of @p family. */
    static QString roleName(GamepadRole::Role role, PadNames::Family family);

private:
    void advance();
    static bool active(GamepadRole::Role role, int value);

    PadNames::Family m_family;
    QList<Step> m_steps;
    QList<State> m_states;
    QStringList m_read;
    QSet<int> m_held;      ///< roles still down since they last counted
    int m_current = 0;
};

/** @} */

#endif
