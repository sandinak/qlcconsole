/*
  Q Light Controller Plus
  outputrulekeeper.h

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

#ifndef OUTPUTRULEKEEPER_H
#define OUTPUTRULEKEEPER_H

#include <QObject>
#include <atomic>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QPair>

#include "fixture.h"
#include "doc.h"
#include "qlcengine_export.h"

class QXmlStreamReader;
class QXmlStreamWriter;

#define KXMLQLCOutputRuleSettings QStringLiteral("OutputRuleSettings")

/** @addtogroup engine Engine
 * @{
 */

/**
 * Keeps each universe's output-rule bits (Universe::setOutputRules()) in
 * step with the fixtures' Fixture::OutputRules: power relays and house,
 * work, hall and backstage-blue circuits that Blackout and the Grand Master
 * must not kill, and relays that switch rather than fade. Rebuilt when
 * fixtures are added, removed, changed (patched, moved, unpatched, their
 * rules edited) and when universes come and go -- so a fixture's bits
 * leave its old address with it.
 *
 * Which channels each rule covers:
 *  - ExemptGrandMaster, ExemptBlackout: every channel of the fixture. In
 *    the usual Intensity GM mode only intensity channels are scaled anyway,
 *    so this is the same; in All-channels mode an exempt fixture is exempt,
 *    not half-exempt. Blackout already keeps non-intensity channels, so for
 *    it the difference is the intensity ones holding their live level.
 *  - Switch, OnAtOpen: the fixture's intensity channels (all of them when
 *    it has none -- a relay pack defined some other way). Snapping a pan or
 *    colour channel to 0/255, or driving it to full, would make no sense.
 *
 *  - ShowOnly: every channel. The rules below.
 *
 * Show only: the rules. One mechanism, the live session
 * (GenericFader::liveEpoch()), and every writer stamped by it.
 *
 *  S1. Outside Operate, and in Blind, every frame sent has a Show-only
 *      channel at its safe value (safeValue(): dark, closed, through the
 *      channel's modifier). Blackout always wins, Ignore Blackout or not.
 *  S2. A live session starts at each switch to Operate, at All Off, and
 *      when a show is loaded or cleared (E2). In Operate a Show-only
 *      channel goes out only from a writer stamped in the current session
 *      (GenericFader::holdsShowOnly()); anything else that writes it is
 *      undone before the frame is sent. Input passthrough never reaches
 *      it (Universe::applyPassthrough()).
 *  S3. Stamps. Every stamp is taken from GenericFader::stampEpoch(): the
 *      live session for a person's action, an automatic writer's own
 *      session inside its StampScope.
 *      - A function is stamped by its sources: one added (or renewed: a GO
 *        on a list already playing) in this session is the show, unless it
 *        is a preview, an editor's Test or the Workbench. One started by
 *        another function takes that one's stamp -- at its start, and
 *        whenever that one is the show -- and keeps it for the session: a
 *        cue a Script launched still drives after the Script ends. One
 *        with no source left (it is stopping) is held, fade-out and all.
 *      - A Park, Mark or cue stack is stamped when set, started or stepped
 *        by hand; a Park or Mark loaded with the show (or a parked fixture
 *        patched later) is stamped by no session.
 *      - A Simple Desk channel, a VC slider, XY pad or audio DMX bar when
 *        it is really moved (a changed value from the UI or an input --
 *        never an echo, a feedback re-sync, a rebuild, a mode trip or a
 *        page load); the stamp is the control's, so a fader rebuilt from a
 *        control left up is as old as the control. A playback slider that
 *        starts its function again (after a stop) is as old as its last
 *        move. A stamp is for what is patched there: a Simple Desk channel
 *        moved before a Show-only fixture came onto its address holds it.
 *      - An automatic widget (VC audio triggers, a VC Clock) is the show
 *        only from the session the operator armed it in, in Operate: by
 *        switching it on, touching it, or working it by a key, an input or
 *        the web (Function::armAutoSource()); never by its own first
 *        start. Everything it drives -- its functions, its DMX bars, and a
 *        VC button, slider or cue list one of its bars works -- takes that
 *        session, never a hand's stamp or a GO's (no GO is noted).
 *      - Timecode: a Show, or a section's linked cue list, that external
 *        timecode starts takes the session the timecode link was armed in
 *        (Follow switched on in Operate: Show::timecodeArmEpoch()); loaded
 *        on with the show, or switched on in Design, it is not armed.
 *      - A last look never holds a Show-only channel. Previews, editors'
 *        Tests, Locate, focus and fixture tests are never stamped;
 *        Identify and Reset are refused for a Show-only fixture and for
 *        any fixture sharing its addresses.
 *
 * The emergency stop (All Off, emergencyStop()):
 *  E1. Every Show-only channel goes to its safe value at once: no fade and
 *      no waiting on the function list before the cut (cut first, then
 *      the functions). Every function is stopped, queued ones too -- on
 *      the main thread, queued there when the stop came from another.
 *  E2. It starts a new live session, exactly as the switch to Operate, a
 *      show loaded and a show cleared do.
 *  E3. A writer is stamped again only by an operator action after it: a GO
 *      or claim (for what it starts, and their children through the
 *      root's gate), a hand control really moved, an automatic widget
 *      re-armed in Operate, the timecode link re-armed.
 *  E4. There is no release: a GO on list B makes B's writers current and
 *      nothing else -- list A's fade-out, a stored level, a definition
 *      default, a last look, an armed trigger, schedule or timecode link, a
 *      rebuilt fader from before the stop all stay held.
 *  E5. A function stopped by it is held for the rest of its fade-out (S3:
 *      no source, held).
 *  E6. A Show-only channel's "nothing drives it" value is its safe value
 *      (Universe::setChannelDefaultValue(), Universe::reset()), so no
 *      definition default can come back.
 *  E7. The cut happens on the calling thread, at once; only the footer is
 *      told later. What the Simple Desk lets go of is only what was not
 *      moved since the stop.
 *
 * When OnAtOpen fixtures come on is the show's setting (saved with it,
 * Settings > Output): when you switch to Operate (the default, the safer
 * one) or as soon as the show is open. Blackout-exempt fixtures also hold
 * through Blind (kDefaultBlindHoldsExempt; board Q44).
 */
class QLC_ENGINE_EXPORT OutputRuleKeeper final : public QObject
{
    Q_OBJECT

public:
    /** When OnAtOpen fixtures go to full when nothing sets them */
    enum OnAtOpenWhen
    {
        /** From the first switch to Operate on, until another show is opened
         *  (going back to Design does not drop the house lights) */
        OnAtOperate = 0,
        /** As soon as the show is open (the rig-network gate protects) */
        OnAtShowOpen
    };
    Q_ENUM(OnAtOpenWhen)

    /** A new show's (board Q45: Operate, the safer choice) */
    static constexpr OnAtOpenWhen kDefaultOnAtOpenWhen = OnAtOperate;
    /** Board Q44: Blackout-exempt fixtures hold through Blind */
    static constexpr bool kDefaultBlindHoldsExempt = true;

    explicit OutputRuleKeeper(Doc *doc);

    /** Rebuild every universe's rule bits now (normally done for you,
     *  coalesced) */
    void rebuild();

    OnAtOpenWhen onAtOpenWhen() const { return m_onAtOpenWhen; }
    /** The show's setting: marks the show modified */
    void setOnAtOpenWhen(OnAtOpenWhen when);

    /** Saved only when it differs from a new show's */
    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);
    /** True when OnAtOpen fixtures are being held at full right now */
    bool onAtOpenArmed() const;

    bool blindHoldsExempt() const { return m_blindHoldsExempt; }
    /** For this session (tests; the default is kDefaultBlindHoldsExempt) */
    void setBlindHoldsExempt(bool hold);

    /** The rule bits of one universe (Universe::ChannelType), UNIVERSE_SIZE
     *  bytes -- what rebuild() hands it */
    QByteArray bitsFor(quint32 universe) const;

    /** How many full rebuilds have run (tests) */
    int rebuildCount() const { return m_rebuilds; }
    /** How many fixtures have any rule (for tests and the UI) */
    int ruleFixtureCount() const { return m_count; }

    /** The Show-only fixtures (patched or not), in the show's order */
    QList<quint32> showOnlyFixtures() const { return m_showOnly; }
    /** Why the Show-only fixtures are held right now: Design, Blind --
     *  empty when nothing holds them (Operate, or there are none). In
     *  Operate the previews are still kept off them; that is not a hold
     *  of the show and says nothing here. */
    enum HeldBy { NotHeld = 0, HeldByDesign, HeldByBlind, HeldByEmergencyStop };
    HeldBy showOnlyHeldBy() const;
    /** "Lasers held: Design", "3 Show-only fixtures held: Blind" -- empty
     *  when not held. For the footer chip and the phone. */
    QString showOnlyHeldText() const;

    /** The emergency stop (ShowControl::AllOff): rules E1-E7. Safe from
     *  any thread (an OSC or MSC plugin's). */
    void emergencyStop();
    /** What the footer says: true from the emergency stop until the first
     *  GO after it (noteGo()), a switch of mode or another show. It holds
     *  nothing itself -- the rules do (E4: no release). */
    bool emergencyStopped() const;
    /** A GO was carried out (ShowControl, a VC cue list, the Show tab) */
    void noteGo();

    /** A Show-only channel's safe value: 0 for an intensity channel, and
     *  where nothing drives it (FixtureRest::restValue()) for the rest: a
     *  laser, fogger or hazer all 0, a gobo or colour wheel open. */
    static uchar safeValue(const Fixture *fx, quint32 channel);
    /** The safe values of one universe's Show-only channels (UNIVERSE_SIZE
     *  bytes, 0 elsewhere) -- what rebuild() hands it */
    QByteArray safeValuesFor(quint32 universe) const;

    /** The channel indices of @p fx a rule of @p rule covers (see above) */
    static QList<quint32> coveredChannels(const Fixture *fx, Fixture::OutputRule rule);

    /** A short plain-words line per rule set, for tooltips and badges:
     *  "Ignores Blackout, Grand Master" etc. Empty for none. */
    static QString describe(Fixture::OutputRules rules);

signals:
    /** The Show-only fixtures, or why they are held, changed */
    void showOnlyChanged();
    /** The emergency stop struck, starting session @p epoch (the Simple
     *  Desk lets go of what was not moved since) */
    void emergencyStopStruck(quint32 epoch);

private:
    void scheduleRebuild();
    /** A new live session (E2): Operate, All Off, a show loaded or cleared */
    void startSession();
    /** A fixture's Show-only channels marked where it is now, at once */
    void markNow(quint32 fixtureId);
    void pushPolicy();
    /** Every universe told whether the show may drive its Show-only
     *  channels (Universe::setShowOnlyLive(): Operate) */
    void pushShowOnlyLive();

    Doc *m_doc;
    bool m_pending = false;
    int m_count = 0;
    OnAtOpenWhen m_onAtOpenWhen = kDefaultOnAtOpenWhen;
    bool m_blindHoldsExempt = kDefaultBlindHoldsExempt;
    /** OnAtOperate: Operate has been entered since the show was opened */
    bool m_seenOperate = false;
    QList<quint32> m_showOnly;
    HeldBy m_lastHeldBy = NotHeld;
    /** The session the last emergency stop started (0: none) */
    std::atomic<quint32> m_stopEpoch { 0 };
    /** Where each Show-only fixture's channels are marked (absolute base,
     *  channel count) */
    QHash<quint32, QPair<quint32, quint32> > m_showOnlyAt;
    int m_rebuilds = 0;
    class QTimer *m_rebuildTimer = nullptr;
};

/** @} */

#endif
