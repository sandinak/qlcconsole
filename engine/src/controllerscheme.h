/*
  Q Light Controller Plus
  controllerscheme.h

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

#ifndef CONTROLLERSCHEME_H
#define CONTROLLERSCHEME_H

#include <QString>

#include "qlcengine_export.h"
#include <QColor>

#include "gamepadrole.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * The one controller layout every on-stage tool uses (docs/design/FOCUS_REFINE_DESIGN.md
 * section 2): Focus mode, Focus calls and Refine. A control does the same
 * KIND of thing in each -- the triggers step the big unit (a focus call, a
 * layer), the bumpers the small one (a fixture), Y changes the scope, A keeps,
 * B reverts -- and only the words for it change. Learn it once.
 *
 * verbFor() is the whole map, by gamepad role; label() is what the panels
 * and the help say a verb does in each tool.
 */
namespace ControllerScheme
{
    /** The tool a label is for. */
    enum Tool { FocusTool, FocusCallsTool, RefineTool };

    /** What a control does, the same in every tool. */
    enum Verb
    {
        NoVerb = 0,
        PreviousGroup, NextGroup,     ///< LT / RT: a focus call, a layer
        PreviousItem, NextItem,       ///< LB / RB: a fixture (or a group)
        CycleScope,                   ///< Y: group <-> fixture, layer -> group -> fixture
        Keep,                         ///< A: save the correction, keep the edit
        Revert,                       ///< B: back to what was kept
        Reset,                        ///< X: zero the correction, drop the exception
        Undo,                         ///< View/Back: undo the last keep
        Exit,                         ///< Menu/Start
        ZoomWider, ZoomNarrower,      ///< D-pad down (out) / up (in)
        FocusNearer, FocusFarther,    ///< D-pad left (looser) / right (tighter)
        Alternate,                    ///< LS click: second reference / D-pad page
        WhereAmI                      ///< RS click: say and flash where you are
    };

    /** What @p role does (NoVerb for the sticks and anything unused). The
     *  triggers are verbs too: see TriggerButton for reading them. */
    QLC_ENGINE_EXPORT Verb verbFor(GamepadRole::Role role);

    /** The role that does @p verb (None for NoVerb). */
    QLC_ENGINE_EXPORT GamepadRole::Role roleFor(Verb verb);

    /** What @p verb is called in @p tool, for the on-screen map ("Next focus
     *  call", "Next layer"...); empty when the tool does not use it. */
    QLC_ENGINE_EXPORT QString label(Tool tool, Verb verb);
    /** Board Q140, what the voice says on a D-pad press, in the crew's
     *  words: "Spot down." / "Flood out." (zoom), "Sharper." / "Softer."
     *  (focus); empty for any other verb. */
    QLC_ENGINE_EXPORT QString crewWord(Verb verb);

    /** What a stick does in @p tool ("Nudge aim", "Move"...). */
    QLC_ENGINE_EXPORT QString stickLabel(Tool tool, bool fine);

    /** Every verb in map order, for a table. */
    QLC_ENGINE_EXPORT QList<Verb> verbs();

    /** How the tools tell the operator on stage where they are (Branson):
     *  the rig's blinks (and the panel's lamp), the laptop's voice, or both.
     *  "Say the light" speaks whatever this is: it was asked for. */
    enum Feedback { LightsFeedback, VoiceFeedback, BothFeedback };
    /** "lights only", "voice only", "lights and voice". */
    QLC_ENGINE_EXPORT QString feedbackName(Feedback feedback);
    /** Both -> Lights -> Voice -> Both. */
    QLC_ENGINE_EXPORT Feedback nextFeedback(Feedback feedback);
    /** As a setting is kept: "lights", "voice", "both" (and back; anything
     *  else is Both). */
    QLC_ENGINE_EXPORT QString feedbackKey(Feedback feedback);
    QLC_ENGINE_EXPORT Feedback feedbackFromKey(const QString &key);

    /** How much the voice says (Branson, 2026-10-25): one setting beside
     *  Feedback, this computer's, for Quick focus, Venue focus and Focus
     *  this cue alike. Safety and warning lines are never shortened. */
    enum VoiceDetail { TerseVoice, StandardVoice, VerboseVoice };
    /** "Terse", "Standard", "Verbose". */
    QLC_ENGINE_EXPORT QString voiceDetailName(VoiceDetail detail);
    /** As kept: "terse", "standard", "verbose" (anything else: Standard). */
    QLC_ENGINE_EXPORT QString voiceDetailKey(VoiceDetail detail);
    QLC_ENGINE_EXPORT VoiceDetail voiceDetailFromKey(const QString &key);

    /* The phrase table: every line that changes with the detail, worded
       here only. @p index is 1-based; @p name the light's (may be empty);
       @p venue the venue saved to (empty: the show). */
    /** Stepping to a light: "F1 Spot 3." / "Fixture 1, Spot 3." /
     *  "Fixture 1 of 8, Spot 3." (no name: "F1." / "Fixture 1." / "Fixture
     *  1 of 8."). */
    QLC_ENGINE_EXPORT QString stepPhrase(VoiceDetail detail, int index, int count, const QString &name);
    /** A light saved: "Saved." / "Saved fixture 1." / "Saved Spot 3." --
     *  with " to Lincoln HS" in a venue, but for Terse. */
    QLC_ENGINE_EXPORT QString savedPhrase(VoiceDetail detail, int index, const QString &name,
                                          const QString &venue);
    /** Several saved at once (a whole call): "Saved." / "Saved 4 lights." (+ venue). */
    QLC_ENGINE_EXPORT QString savedSomePhrase(VoiceDetail detail, int lights, const QString &venue);
    /** Every light saved (A held): "Saved all." / "Saved all, 6 lights." /
     *  "Saved all to Lincoln HS, 6 lights." */
    QLC_ENGINE_EXPORT QString savedAllPhrase(VoiceDetail detail, int lights, const QString &venue);
    /** A light back to its last save: "Reverted." / "Fixture 1 reverted." /
     *  "Spot 3 back to its last save." */
    QLC_ENGINE_EXPORT QString revertedPhrase(VoiceDetail detail, int index, const QString &name);
    /** Every light not saved put back (B held): "Reverted all." /
     *  "Reverted all, 3 lights." / "3 lights back to their last save." */
    QLC_ENGINE_EXPORT QString revertedAllPhrase(VoiceDetail detail, int lights);
    /** Entering a call: "Call 2." / "Call 2, AT spots to Soloist DS." /
     *  "Call 2 of 5, AT spots to Soloist DS, SL 2, US 1." (@p where: the
     *  target's place, Verbose only). */
    QLC_ENGINE_EXPORT QString callPhrase(VoiceDetail detail, int index, int count, const QString &who,
                                         const QString &target, const QString &where);

    /** How much a spoken line matters (board Q131, gap 9):
     *  - Where: "fixture 3 of 8", the start line, a palette's name. Only the
     *    newest waits; a newer one replaces it (stepping through fixtures
     *    says where you stopped). Silent in Lights only.
     *  - Confirm: what a press did ("Spot 3 kept.", "Undone.", "Feedback:
     *    voice only."). Queued, never dropped. Silent in Lights only.
     *  - Important: warnings, refusals, the controller lost or back, the
     *    venue asked. Queued, never dropped, and spoken in EVERY feedback
     *    setting; the rig gives a slow long blink and the pad one long
     *    buzz, unlike a save's double blink and short buzz. */
    /*  - WhereNow (board Q171): a where line said at once, not held while
     *    the selection settles -- a tool's first line, entering a focus
     *    call. Otherwise a Where line. */
    enum Priority { WhereNowPriority = -1, WherePriority = 0, ConfirmPriority = 1, ImportantPriority = 2 };

    /** The save lamp's colours (gap 10), the panels' and the pad's light
     *  bar alike: colour-blind safe -- blue, all saved; yellow, this light
     *  unsaved; orange, another light unsaved -- and never the only sign:
     *  the lamp also shows a shape (a tick, a "!") and the count. */
    inline QColor savedColour() { return QColor(0x00, 0x72, 0xb2); }
    inline QColor thisUnsavedColour() { return QColor(0xf0, 0xc8, 0x20); }
    inline QColor othersUnsavedColour() { return QColor(0xe0, 0x6c, 0x00); }

    /** A short buzz (a keep) and the long one (something important). */
    constexpr int rumbleShortMs() { return 120; }
    constexpr int rumbleLongMs() { return 700; }

    /**
     * A press that means one thing when tapped and another when held
     * (RS: say the light; held: change the feedback). release() says a tap;
     * due() says, once, that it has been held long enough.
     */
    /** How long A is held to keep (save) everything, in seconds (board
     *  Q132: a deliberate second, longer than RS's and B's holds, so a
     *  firm press is never taken for it). A constant for now; it could
     *  become a setting. */
    constexpr double keepAllHoldSeconds() { return 1.0; }

    class QLC_ENGINE_EXPORT LongPress
    {
    public:
        /** RS (the feedback) and B (back to the last save). */
        static constexpr double defaultHold() { return 0.6; }
        explicit LongPress(double holdFor = defaultHold()) : m_holdFor(holdFor) {}
        double holdFor() const { return m_holdFor; }
        void press() { m_down = true; m_held = 0.0; m_fired = false; }
        /** Let go: true when it was a tap (not held to holdFor()). */
        bool release()
        {
            const bool tap = m_down && !m_fired;
            m_down = false;
            return tap;
        }
        /** True once, when it has been held for holdFor(). */
        bool due(double dt)
        {
            if (!m_down || m_fired)
                return false;
            m_held += dt;
            if (m_held + 1e-9 < holdFor())
                return false;
            m_fired = true;
            return true;
        }
        bool isDown() const { return m_down; }
        /** Let go without it counting as a tap (the controller went away). */
        void reset() { m_down = false; m_fired = false; m_held = 0.0; }

    private:
        double m_holdFor;
        bool m_down = false;
        bool m_fired = false;
        double m_held = 0.0;
    };

    /** Whether @p verb repeats while its control is held: the D-pad's zoom
     *  and focus arms. Bumpers, triggers and the rest step once a press. */
    QLC_ENGINE_EXPORT bool repeats(Verb verb);

    /**
     * A held control repeating (Branson: "when I hold a button on the
     * D-pad it doesn't repeat"): once on the press, again after delay(),
     * then every interval() until it is let go. The tool's controller tick
     * asks due() how many repeats have come round.
     */
    class QLC_ENGINE_EXPORT HoldRepeat
    {
    public:
        static constexpr double delay() { return 0.4; }
        static constexpr double interval() { return 0.08; }

        /** @p key went down: it repeats (when @p repeats) from now. */
        void press(int key, bool repeats);
        /** @p key went up: it stops, if it was the one held. */
        void release(int key);
        /** The key repeating (-1: none). */
        int held() const { return m_held; }
        /** The repeats due after @p dt more seconds held. */
        int due(double dt);
        void reset() { m_held = -1; m_wait = 0.0; }

    private:
        int m_held = -1;
        double m_wait = 0.0;
    };

    /**
     * A trigger read as a button: an axis 0 (released) .. 1, pressed past
     * pressAt() and released again below releaseAt(), so a trigger resting
     * a little in, or a noisy one, does not repeat.
     */
    class QLC_ENGINE_EXPORT TriggerButton
    {
    public:
        static constexpr float pressAt() { return 0.6f; }
        static constexpr float releaseAt() { return 0.35f; }

        /** A new reading; true on the press only. */
        bool update(float norm);
        bool isDown() const { return m_down; }
        void reset() { m_down = false; }

    private:
        bool m_down = false;
    };
}

/** @} */

#endif
