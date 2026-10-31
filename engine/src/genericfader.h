/*
  Q Light Controller
  genericfader.h

  Copyright (c) Heikki Junnila

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

#ifndef GENERICFADER
#define GENERICFADER

#include <QObject>
#include <functional>
#include <QList>
#include <QHash>
#include <mutex>
#if defined(Q_OS_WIN)
#include <QRecursiveMutex>
#endif

#include "universe.h"
#include "scenevalue.h"

class FadeChannel;

/** @addtogroup engine Engine
 * @{
 */

/**
 *  GenericFader represents all the fading channels for one Function (or feature)
 *  and one Universe. For example a Scene will request one GenericFader for all the
 *  channels of all the fixtures on a specific Universe.
 *  In this way, Universes will handle a list of dedicated faders, without
 *  any lookup
 */

class GenericFader final : public QObject
{
    Q_OBJECT

public:
    GenericFader(QObject *parent = 0);
    ~GenericFader();

    /** Get/Set an arbitrary name for this fader */
    QString name() const;
    void setName(QString name);

    /** Get/Set the ID of the Function controlling this fader.
     *  VC widgets won't set this. */
    quint32 parentFunctionID() const;
    void setParentFunctionID(quint32 fid);

    /** Get/Set a priority for this fader.
     *  The Universe class is in charge of sorting faders by priority */
    int priority() const;
    void setPriority(int priority);

    /** Get/Set if this fader should handle primary/secondary channels
     *  when a caller requests a FadeChannel */
    bool handleSecondary();
    void setHandleSecondary(bool enable);

    /** Build a hash for a fader channel which is unique in a Universe.
     *  This is used to map channels and access them quickly */
    static quint32 channelHash(quint32 fixtureID, quint32 channel);

    /**
     * Add a channel that shall be faded from ch.start() to ch.target() within
     * the time specified by ch.fadeTime(). If ch.target() == 0, the channel will
     * be removed automatically from the fader when done.
     *
     * If the fader already contains the same channel, the one whose current
     * value is higher remains in the fader. With LTP channels this might result
     * in the value jumping in a weird way but LTP channels are rarely faded anyway.
     * With HTP channels the lower value has no meaning in the first place.
     *
     * @param ch The channel to fade
     */
    void add(const FadeChannel& ch);

    /** Replace an existing FaderChannel */
    void replace(const FadeChannel& ch);

    /** The doc its channels' fixtures are in -- for an UNPATCHED fixture's
     *  channels, whose values go to the fixture for display (no address).
     *  Set by getChannelFader(); a writer that only add()s or replace()s
     *  (the effect runner) sets it here. */
    void setDoc(const Doc *doc) { m_doc = doc; }

    /** Remove a channel whose fixture & channel match with $fc's */
    void remove(FadeChannel *ch);

    /** Remove all channels */
    void removeAll();

    /** Let go of the channels @a which picks while the rest play on, as a
     *  stop would: over @a fadeTime ms an intensity fades to zero and any
     *  other channel to what is under it, then each goes; with no fade they
     *  go at once. (A scene's group losing a member, Scene::write.) */
    void release(const std::function<bool(const FadeChannel &)> &which, uint fadeTime);

    /** Swap the whole channel set for $channels in one step. A source that
     *  rebuilds its fader every tick must use this, not removeAll() + replace():
     *  universes process their faders on their own threads, and one that got
     *  in between saw an empty fader and sent a blank frame. */
    void replaceAll(const QList<FadeChannel> &channels);

    /** Add ($on) or remove @a flag on every channel this fader holds, e.g.
     *  FadeChannel::Override on a scene that is to win over the rest. */
    void setChannelsFlag(int flag, bool on);

    /** Get/Set a request of deletion of this fader */
    bool deleteRequested();
    void requestDelete();

    /** The lock guarding a fader's channels.
     *
     *  std::recursive_mutex where TSan can see it (it can't see into a
     *  QRecursiveMutex, which locks out of line). Not on Windows: MinGW's
     *  libstdc++ gives std::recursive_mutex no destructor (it has a static
     *  initializer, so it assumes the pthread mutex owns nothing), while
     *  winpthreads allocates the mutex and a kernel Event on its first lock.
     *  Every fader leaked both, and faders come and go all the time (a
     *  scene asks for one per universe each time it starts): the Windows
     *  stress run leaked ~5000 Events a second -- 3 million handles and
     *  300 MB in 10 minutes -- with a CreateEvent call on the timer thread
     *  for each. QRecursiveMutex is a word and a futex: nothing to create,
     *  nothing to leak. */
#if defined(Q_OS_WIN)
    typedef QRecursiveMutex ChannelsLock;
#else
    typedef std::recursive_mutex ChannelsLock;
#endif

    /** A FadeChannel of this fader, with the fader's channels locked for as
     *  long as it is held. The universe this fader belongs to writes it on
     *  its own thread -- stepping its channels, and dropping the ones that are
     *  done -- and a QHash moves its entries when one is added or removed: a
     *  bare FadeChannel pointer, kept past the lock, wrote into whatever had
     *  moved there (heap corruption under load). Keep it only as long as the
     *  channel is being set up; don't hold two from different faders, nor
     *  ask a Universe for a fader, while holding one. */
    class ChannelRef
    {
    public:
        ChannelRef(std::unique_lock<ChannelsLock> &&lock, FadeChannel *fc)
            : m_lock(std::move(lock)), m_fc(fc) {}
        ChannelRef() = default;
        ChannelRef(ChannelRef &&) = default;
        ChannelRef &operator=(ChannelRef &&) = default;
        ChannelRef(const ChannelRef &) = delete;
        ChannelRef &operator=(const ChannelRef &) = delete;

        FadeChannel *get() const { return m_fc; }
        FadeChannel *operator->() const { return m_fc; }
        FadeChannel &operator*() const { return *m_fc; }
        explicit operator bool() const { return m_fc != nullptr; }

        /** Let go of the channel (and the lock) before the end of the scope */
        void release() { m_fc = nullptr; m_lock = std::unique_lock<ChannelsLock>(); }

    private:
        std::unique_lock<ChannelsLock> m_lock;
        FadeChannel *m_fc = nullptr;
    };

    /** Returns the FadeChannel for the provided $fixtureID and $channel,
     *  locked (see ChannelRef). If no FadeChannel is found, a new one is
     *  created and added to m_channels. Also, new channels will have a start
     *  value set depending on their type */
    ChannelRef getChannelFader(const Doc *doc, Universe *universe, quint32 fixtureID, quint32 channel);

    /** Get all channels in a non-modifiable hashmap */
    QHash <quint32,FadeChannel> channels() const;

    /** Return the number of channel added to this fader */
    int channelsCount() const;

    /**
     * Run the channels forward by one step and write their current values to
     * the given Universe
     *
     * @param universe The universe that receives channel data.
     */
    void write(Universe *universe);

    /** Get/Set the intensities of all channels in a 0.0 - 1.0 range */
    qreal intensity() const;
    void adjustIntensity(qreal fraction);

    /** Get/Set an optional intensity for the fader parent Function */
    qreal parentIntensity() const;
    void setParentIntensity(qreal fraction);

    /** Get/Set the pause state of this fader */
    bool isPaused() const;
    void setPaused(bool paused);

    /** Get/Set the enable state of this fader */
    bool isEnabled() const;
    void setEnabled(bool enable);

    /** Get the fade out status of this fader */
    bool isFadingOut() const;

    /** Set this fader to fade out. If $fadeTime is non-zero,
      * all the intensity channels will be updated.
      * Enabling fade out also resumes a paused fader */
    void setFadeOut(bool enable, uint fadeTime);

    /** As setFadeOut(enable, fadeTime), but honouring a per-channel fade-time
     *  override (keyed by channelHash(fixtureID, channel), in ms). Channels
     *  absent from @a channelFadeTimes use @a fadeTime; those present use their
     *  own time (0 = snap). Unlike the 2-arg overload this ALWAYS targets the
     *  channels, so a 0 default still drives them to 0 — needed when a scene's
     *  own fade-out is 0 but some looks carry a non-zero fade-out override
     *  (a "pulse": fast in, slow out). */
    void setFadeOut(bool enable, uint fadeTime, const QHash<quint32, uint> &channelFadeTimes);

    /** Re-time channels already fading, each by @p timing (given the channel,
     *  set its fade and its delay; return false to leave it as it is) -- a
     *  transition reaching each fixture at its own moment. */
    void setChannelTimings(const std::function<bool(const FadeChannel &, uint &fade, uint &delay)> &timing);

    /**
     * Set the blend mode to be applied in the write method
     *
     * @param mode the blend mode as listed in Universe::BlendMode
     */
    void setBlendMode(Universe::BlendMode mode);

    /** Enable/disable universe monitoring before writing new data */
    void setMonitoring(bool enable);

    /** Remove the Crossfade flag from every fader handled by this class */
    void resetCrossfade();

    /** When true, write() zeros all 512 channels in the universe (forceLTP)
     *  before writing this fader's channels.  Use for Override-priority faders
     *  that must blackout everything except their own channels (e.g. fixture
     *  test-mode isolation). */
    bool zeroAll() const { return m_zeroAll; }
    void setZeroAll(bool z) { m_zeroAll = z; }

    /** An override hit's fader (Universe::HitOverride, Scene): every channel
     *  it holds is written over what the faders below wrote this frame,
     *  intensity too (Universe::writeMix), mixed in from that over
     *  @a fadeInMs (0: at once). Its channels themselves don't fade. */
    void startOverrideMix(uint fadeInMs);
    /** Let go of an override hit: from where its mix is, back to what the
     *  faders below write -- each frame's, not a snapshot -- over
     *  @a fadeOutMs; then the fader goes. */
    void releaseOverrideMix(uint fadeOutMs);

signals:
    /** Signal emitted when monitoring is enabled.
     *  Data is preGM and includes the whole universe */
    void preWriteData(quint32 index, const QByteArray& universeData);

private:
    QString m_name;
    quint32 m_fid;
    int m_priority;
    bool m_handleSecondary;
    QHash <quint32,FadeChannel> m_channels;
    /** Guards m_channels and the FadeChannels in it. Recursive: a caller
     *  holding a ChannelRef may ask for another channel of the same fader. */
    mutable ChannelsLock m_channelsLock;
    qreal m_intensity;
    qreal m_parentIntensity;
    bool m_paused;
    /** For an unpatched fixture's channels: its values go to the fixture, for
     *  display, instead of to a universe (Fixture::setPreviewValue()). */
    const Doc *m_doc = nullptr;
    void showOnFixture(const FadeChannel &fc, quint32 value) const;
    bool m_enabled;
    bool m_fadeOut;
    bool m_deleteRequest;
    Universe::BlendMode m_blendMode;
    bool m_monitoring;
    bool m_zeroAll = false;

    /** An override hit's mix (startOverrideMix): off, coming in (and held
     *  at 1 when in), or going. Worked out once a frame in write(). */
    enum MixMode { MixOff, MixIn, MixOut };
    MixMode m_mixMode = MixOff;
    qreal m_mix = 1.0;
    qreal m_mixFrom = 1.0;
    uint m_mixTime = 0;
    uint m_mixElapsed = 0;
};

/** @} */

#endif
