/*
  Q Light Controller Plus
  presses.h

  Button presses on a fixture channel (QLCCapability::isPress()): a value
  that makes what the channel drives take one more step each time it is
  sent -- the IR candle remote's Dim and Brighten -- unlike a state (On,
  Off), which is harmless to send again. What the Show Tree's bake reads to
  keep a press from tracking (ShowTreeController::pressGuards()) and Show
  Review's CUE-17 reads to warn about one. Main thread, never the DMX path.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef PRESSES_H
#define PRESSES_H

#include <QHash>
#include <QSet>
#include <QString>

#include "qlcengine_export.h"

class Doc;

/** @addtogroup engine Engine
 * @{
 */

namespace Presses
{
    /** A fixture's channel: fixture id in the high half, channel index in
     *  the low. */
    typedef quint64 Key;
    inline Key key(quint32 fixture, quint32 channel) { return (quint64(fixture) << 32) | quint64(channel); }
    inline quint32 fixtureOf(Key k) { return quint32(k >> 32); }
    inline quint32 channelOf(Key k) { return quint32(k & 0xFFFFFFFFu); }

    /** Every channel of the show's fixtures that has a press
     *  (QLCChannel::hasPress()): empty in a show with none, the usual case. */
    QLC_ENGINE_EXPORT QSet<Key> channels(Doc *doc);

    /** Whether @p value on channel @p k is a press. */
    QLC_ENGINE_EXPORT bool isPress(Doc *doc, Key k, uchar value);

    /** The button @p value presses or sets on @p k, as the definition names
     *  it ("Dim"); the value itself when it names none. */
    QLC_ENGINE_EXPORT QString buttonName(Doc *doc, Key k, uchar value);

    /** What function @p functionId puts on the channels of @p among when it
     *  plays: a Scene its own values; a Collection its members', in the
     *  order they start (a later one wins). Anything else -- a chase, an
     *  effect -- nothing: when it sends what isn't known ahead. */
    QLC_ENGINE_EXPORT QHash<Key, uchar> written(Doc *doc, quint32 functionId, const QSet<Key> &among);

    /** Whether function @p functionId presses a button on @p among over and
     *  over as it plays: a Chaser (or a Sequence) with a step that does, or
     *  a Collection with such a member. @p pressKey and @p pressValue, when
     *  given, get the first such press (its channel and value). */
    QLC_ENGINE_EXPORT bool repeats(Doc *doc, quint32 functionId, const QSet<Key> &among,
                                   Key *pressKey = nullptr, uchar *pressValue = nullptr);
}

/** @} */

#endif // PRESSES_H
