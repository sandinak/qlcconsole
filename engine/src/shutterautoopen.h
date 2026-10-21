/*
  Q Light Controller Plus
  shutterautoopen.h

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

#ifndef SHUTTERAUTOOPEN_H
#define SHUTTERAUTOOPEN_H

#include <QObject>

#include "universe.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QLCChannel;
class Fixture;
class Doc;

/** @addtogroup engine Engine
 * @{
 */

#define KXMLQLCShutterAutoOpen QStringLiteral("ShutterAutoOpen")

/**
 * A console's "auto open": a head given a level, whose shutter nothing
 * sets, has its shutter opened on output -- so a spot whose shutter rests
 * closed lights at Full from a dimmer look alone. An explicit shutter value
 * (closed, strobe, open) always wins, and a head with no level is left as
 * it is.
 *
 * The show's setting (saved with it, on by default). This keeps each
 * universe's rules (Universe::setAutoOpenRules()) in step with the
 * fixtures; the universes apply them after their faders, on their own
 * thread, so every source -- scenes, the Show Tree, the programmer,
 * sliders, the Simple Desk -- and every reader of the output (DMX, the 2D
 * monitor, the Rig Overview) see the same thing.
 */
class ShutterAutoOpen final : public QObject
{
    Q_OBJECT

public:
    explicit ShutterAutoOpen(Doc *doc);

    bool isEnabled() const { return m_enabled; }
    /** Turn it on or off for this show; marks the show modified. */
    void setEnabled(bool on);

    /** Rebuild the rules now (normally done for you, coalesced). */
    void rebuild();

    /** How many shutter channels are auto-opened (for tests and the UI). */
    int ruleCount() const { return m_ruleCount; }

    /** The DMX value that opens @p ch, or -1 when it has none: its default
     *  when that is an Open capability, else its first Open capability (by
     *  preset, then by a plain "Open" / "Shutter open" name). A strobe
     *  preset channel opens at 0. Iris channels have none. */
    static int openValue(const QLCChannel *ch);

    /** The rules for one fixture (its universe-relative addresses). */
    static QVector<Universe::AutoOpenRule> rulesFor(const Fixture *fx);

    /** Back to a new show's setting (on). */
    void reset();

    void saveXML(QXmlStreamWriter *doc) const;
    void loadXML(QXmlStreamReader &root);

signals:
    void enabledChanged(bool on);

private:
    void scheduleRebuild();

    Doc *m_doc;
    bool m_enabled = true;
    bool m_pending = false;
    int m_ruleCount = 0;
};

/** @} */

#endif
