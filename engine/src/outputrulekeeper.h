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
#include <QByteArray>
#include <QList>

#include "fixture.h"
#include "doc.h"

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
 * When OnAtOpen fixtures come on is the show's setting (saved with it,
 * Settings > Output): when you switch to Operate (the default, the safer
 * one) or as soon as the show is open. Blackout-exempt fixtures also hold
 * through Blind (kDefaultBlindHoldsExempt; board Q44).
 */
class OutputRuleKeeper final : public QObject
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

    /** How many fixtures have any rule (for tests and the UI) */
    int ruleFixtureCount() const { return m_count; }

    /** The channel indices of @p fx a rule of @p rule covers (see above) */
    static QList<quint32> coveredChannels(const Fixture *fx, Fixture::OutputRule rule);

    /** A short plain-words line per rule set, for tooltips and badges:
     *  "Ignores Blackout, Grand Master" etc. Empty for none. */
    static QString describe(Fixture::OutputRules rules);

private:
    void scheduleRebuild();
    void pushPolicy();

    Doc *m_doc;
    bool m_pending = false;
    int m_count = 0;
    OnAtOpenWhen m_onAtOpenWhen = kDefaultOnAtOpenWhen;
    bool m_blindHoldsExempt = kDefaultBlindHoldsExempt;
    /** OnAtOperate: Operate has been entered since the show was opened */
    bool m_seenOperate = false;
};

/** @} */

#endif
