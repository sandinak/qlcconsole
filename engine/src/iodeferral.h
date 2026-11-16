/*
  Q Light Controller Plus
  iodeferral.h

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

#ifndef IODEFERRAL_H
#define IODEFERRAL_H

#include <functional>

#include "qlcengine_export.h"

/** @addtogroup engine Engine
 * @{
 */

/**
 * Device calls made after the locks, never under them.
 *
 * A patch change is bookkeeping (which plugin, which line, what the frame
 * sends) and device calls (a plugin's openOutput(), closeOutput(),
 * openInput(), closeInput(), setParameter(), a closing frame). The
 * bookkeeping needs the universe claim (InputOutputMap::m_universeMutex,
 * which the MasterTimer tick takes every frame) and a universe's frame lock
 * (which its writer thread takes every frame). A device call can block for
 * as long as the system does: a USB device re-enumerating left an IOKit
 * call that never returned (macOS 27, a show machine), and opened or closed
 * under either lock it stops the DMX output of every universe.
 *
 * So: an IODeferral is declared before the lock is taken. While one is open
 * on a thread, every device call the patches make (OutputPatch, InputPatch:
 * run()) is queued instead; the outermost one runs them, in the order they
 * were made, once it ends -- after the lock it was declared before has been
 * let go -- or at finish(). An OutputPatch with calls queued sends no frame
 * (its line is not ready) until they have run.
 *
 * Per thread: the universe threads never see the GUI thread's queue.
 *
 * Not covered: frame writes (QLCIOPlugin::writeUniverse() from a universe's
 * thread under its frame lock, and from Blackout/Blind under the universe
 * claim) -- a device that hangs while sending still stalls them. And the
 * queued calls run on the thread that queued them (the GUI's): one that
 * never returns still holds that thread.
 */
class QLC_ENGINE_EXPORT IODeferral
{
public:
    IODeferral();
    ~IODeferral();

    /** Run the queued calls now (the outermost one only; an inner one runs
     *  nothing). False when one of them failed (an open that did not
     *  open) -- true from an inner one, which cannot know yet. */
    bool finish();

    /** @p call now, when no IODeferral is open on this thread (its result),
     *  else queued for the outermost one (true). */
    static bool run(const std::function<bool()> &call);

    /** An IODeferral is open on this thread */
    static bool active();

    /** One open without a scope: InputOutputMap::claimUniverses() opens
     *  one, releaseUniverses() ends it once the universes are let go (so a
     *  patch change made under the claim -- a Show-only hold -- reaches the
     *  device after it). begin() says if it is the outermost; end() runs
     *  the queue when it is. */
    static bool begin();
    static void end(bool outer);

private:
    IODeferral(const IODeferral &) = delete;
    IODeferral &operator=(const IODeferral &) = delete;

    /** Every queued call, in order (the outermost only) */
    static bool runQueued();

    bool m_outer;
};

/** @} */

#endif
