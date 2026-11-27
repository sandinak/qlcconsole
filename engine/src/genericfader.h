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
#include <QMutex>
#include <mutex>
#include <memory>
#include <atomic>
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

/** A gate on a fader (board Q352/Q353: the hazards' fire layers): the
 *  fader writes only while @a flag is set (the spotter's Hold, or the
 *  arming) and the grant it was laid for is still the current one (a new
 *  step, a Kill or a stop moves @a epoch on: closed at once, from any
 *  thread, without the main thread). Read once a frame per gated fader. */
struct FaderGate
{
    const std::atomic<bool> *flag = nullptr;
    const std::atomic<quint32> *grant = nullptr;
    const std::atomic<quint32> *epoch = nullptr;
    bool open() const
    {
        return flag->load(std::memory_order_acquire)
               && grant->load(std::memory_order_acquire) == epoch->load(std::memory_order_acquire);
    }
};

/** A gate that never opens: a keeper going away closes its layers with it */
inline const FaderGate *closedFaderGate()
{
    static std::atomic<bool> never { false };
    static std::atomic<quint32> a { 0 }, b { 1 };
    static const FaderGate gate { &never, &a, &b };
    return &gate;
}

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

    /** Who this fader is in its universe ("Why is this on?" v2): set once
     *  by Universe::requestFader(), never 0 there */
    quint32 token() const { return m_token; }
    void setToken(quint32 token) { m_token = token; }
    /** Tell @p universe every channel this fader holds lets go of it now
     *  (Universe::noteLetGo()): when it leaves the universe's list */
    void noteLetGo(Universe *universe) const;

    /** Get/Set a priority for this fader.
     *  The Universe class is in charge of sorting faders by priority */
    int priority() const;
    void setPriority(int priority);

    /** On top of its priority: the Universe keeps it after every other
     *  fader of the same priority, whenever they arrive -- the effect
     *  scripts' fader, which layers on the scenes it plays over (a Show
     *  Tree preview's are Override too). */
    bool onTopOfPriority() const { return m_onTopOfPriority; }
    void setOnTopOfPriority(bool on) { m_onTopOfPriority = on; }

    /** Write only while @p gate is open (FaderGate); nullptr: always. */
    void setGate(const FaderGate *gate) { m_gate = gate; }
    const FaderGate *gate() const { return m_gate; }

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

    /** The current value of channel @p channel of @p fixtureID, in @p out;
     *  false when this fader has no such channel. No allocation. */
    bool currentValue(quint32 fixtureID, quint32 channel, quint32 *out) const;
    /** Where channel @p channel of @p fixtureID is in its fade: its value
     *  now, where it started, how long it still waits (a transition's
     *  delay) and then fades (ms; both 0 at its target), and whether a
     *  manual crossfade morphs it. False when this fader has no such
     *  channel. No allocation. */
    struct RunningFade
    {
        quint32 current = 0;
        quint32 start = 0;
        uint delayLeft = 0;
        uint fadeLeft = 0;
        bool crossFade = false;
    };
    bool runningFade(quint32 fixtureID, quint32 channel, RunningFade &out) const;

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

    /** "Why is this on?" (WhyOn), on demand from the UI -- never per frame:
     *  a copy of the channel that covers @a address (in this fader's
     *  universe; a 16-bit pair's either byte), taken under the channels'
     *  lock. False: this fader doesn't write @a address. */
    bool peekAddress(quint32 address, FadeChannel *out) const;

    /** The blend mode write() uses (setBlendMode()) */
    Universe::BlendMode blendMode() const { return m_blendMode; }

    /** An override hit's mix as of the last frame (startOverrideMix()):
     *  0..1, and whether this fader is a hit's at all. For WhyOn. */
    qreal overrideMix(bool *isHit = nullptr) const;

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
    /** Let go over @p fadeTime: every channel, intensity too, travels from
     *  where it is to what the faders under this one write, frame by frame
     *  (releaseOverrideMix()), then the fader goes. Stage machinery a layer held
     *  (OutputRuleKeeper::releaseMachineryForEdit()): never a jump. */
    void glideOut(uint fadeTime);

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

    /*********************************************************************
     * Show only (Fixture::ShowOnly)
     *********************************************************************/
    /** A function's "not the show" flag, shared with the faders it makes
     *  (Function::previewGate()); true: held */
    typedef std::shared_ptr<std::atomic<bool> > PreviewGate;
    /** A function's stamp as the show (Function::showStamp()), shared with
     *  the faders it makes; 0 while held */
    typedef std::shared_ptr<std::atomic<quint32> > StampGate;

    /** Show only (OutputRuleKeeper, rules S0-S3, E1-E7). Every operator
     *  action takes a stamp (stampEpoch()): a number that only grows. A
     *  live session starts at a stamp of its own (liveEpoch()); each
     *  Show-only channel keeps the stamp it was made Show only at
     *  (Universe). A writer counts on a Show-only channel only while its
     *  stamp is of this session (>= liveEpoch()) and later than the
     *  channel's. What a fader's stamp is (showStamp()):
     *   - a function's (made in its CreationScope): its function's
     *     (0 while the function is held);
     *   - held (setHoldsShowOnly(): a preview, focus, Locate, a test): 0;
     *   - stamped (setLiveEpoch(): Park, Mark, a cue stack, a VC slider,
     *     XY pad, flash or audio DMX bar -- the stamp is its owner's, given
     *     again on every write, so a fader rebuilt from a control left up
     *     stays as old as the control);
     *   - stamped per channel (setChannelLiveEpoch(): the Simple Desk, the
     *     effect runner's own fader -- each Show-only write with its
     *     scene's stamp; channelStamp());
     *   - none of these: not the show -- 0.
     *  No writer bypasses a channel's mark (R8-5).
     *  Read once a frame on the universe thread. */
    quint32 showStamp() const
    {
        if (m_holdsShowOnly.load(std::memory_order_relaxed))
            return 0;
        if (m_previewGate)
        {
            if (m_previewGate->load(std::memory_order_acquire))
                return 0;
            return m_gateStamp ? m_gateStamp->load(std::memory_order_acquire) : 0;
        }
        if (m_liveBound.load(std::memory_order_relaxed))
            return m_liveEpoch.load(std::memory_order_relaxed);
        return 0;
    }
    /** True when the whole fader is held now (its stamp is not of this
     *  session). A per-channel one (the Simple Desk) never is as a whole. */
    bool holdsShowOnly() const
    {
        if (channelStamped() && !m_holdsShowOnly.load(std::memory_order_relaxed) && !m_previewGate
            && !m_liveBound.load(std::memory_order_relaxed))
            return false;
        return showStamp() < liveEpoch();
    }
    void setHoldsShowOnly(bool hold) { m_holdsShowOnly.store(hold, std::memory_order_relaxed); }

    /** Stamped by its owner now (an operator action; or, inside a
     *  StampScope, the automatic writer's own stamp) */
    void stampLiveEpoch() { setLiveEpoch(stampEpoch()); }
    /** Stamped with @p stamp: its owner's, whenever that was */
    void setLiveEpoch(quint32 stamp)
    {
        m_liveEpoch.store(stamp, std::memory_order_relaxed);
        m_liveBound.store(true, std::memory_order_relaxed);
    }

    /** Stamped channel by channel (the Simple Desk: each channel its own
     *  control): channel @p address of its universe stamped @p stamp. A
     *  channel never stamped is held. */
    void setChannelLiveEpoch(int address, quint32 stamp);
    bool channelStamped() const { return m_channelStamped.load(std::memory_order_relaxed); }
    /** Channel @p address's stamp (0: none) */
    quint32 channelStamp(int address) const;

    /** The stamp the current live session started at: bumped at each
     *  switch to Operate, All Off, a show loaded or cleared, the end of
     *  Blind (Function::startLiveSession()). A stamp below it is of an
     *  older session. */
    static quint32 liveEpoch();
    static void bumpLiveEpoch();
    /** A new stamp, later than every one before (a channel made Show only
     *  takes one: Universe) */
    static quint32 newStamp();

    /** The stamp an operator action happening now takes: a new one, or --
     *  inside a StampScope on this thread -- its automatic writer's. Every
     *  stamp (a source added, a control moved, a Park set) takes this. */
    static quint32 stampEpoch();
    /** True inside a StampScope on this thread: what happens now is an
     *  automatic writer's, not a person's (no GO is noted) */
    static bool stampOverridden();

    /** While one is in scope on this thread, every stamp made there is
     *  @p stamp (0: never the show): an audio bar pressing a VC button or
     *  moving a slider is the audio widget's (armed when it was), not a
     *  hand; a playback slider starting its function again is as old as
     *  its last move; a Show or a cue list started by timecode is as old
     *  as the timecode link's arming. Nests. */
    class StampScope
    {
    public:
        explicit StampScope(quint32 stamp);
        ~StampScope();
        StampScope(const StampScope &) = delete;
        StampScope &operator=(const StampScope &) = delete;
    private:
        quint32 m_previous;
        bool m_previousActive;
    };

    /** While one is in scope on a thread, the faders made there follow
     *  @p gate and @p stamp, those of function @p functionId (the
     *  MasterTimer, around each function's preRun(), write() and
     *  postRun()). Nests; both must outlive the scope. */
    class CreationScope
    {
    public:
        explicit CreationScope(const PreviewGate *gate, const StampGate *stamp = nullptr,
                               quint32 functionId = 0xFFFFFFFF);
        ~CreationScope();
        CreationScope(const CreationScope &) = delete;
        CreationScope &operator=(const CreationScope &) = delete;
    private:
        const PreviewGate *m_previousGate;
        const StampGate *m_previousStamp;
        quint32 m_previousFunction;
    };

    /** The gate and stamp of function @p functionId when it is the one
     *  writing on this thread now (its CreationScope is in force), else
     *  null: what a function it starts follows (Function::start()) */
    static PreviewGate creationGateOf(quint32 functionId);
    static StampGate creationStampOf(quint32 functionId);

private:
    /** The gate of the CreationScope in force on this thread, or null */
    static const PreviewGate *creationGate();
    static const StampGate *creationStamp();

signals:
    /** Signal emitted when monitoring is enabled.
     *  Data is preGM and includes the whole universe */
    void preWriteData(quint32 index, const QByteArray& universeData);

private:
    QString m_name;
    quint32 m_fid;
    quint32 m_token = 0;
    int m_priority;
    bool m_onTopOfPriority = false;
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
    const FaderGate *m_gate = nullptr;
    std::atomic<bool> m_holdsShowOnly { false };
    std::atomic<bool> m_liveBound { false };
    std::atomic<quint32> m_liveEpoch { 0 };
    std::atomic<bool> m_channelStamped { false };
    mutable QMutex m_channelEpochMutex;
    QHash<int, quint32> m_channelEpochs;
    /** Set once, when made (CreationScope): never changes after */
    PreviewGate m_previewGate;

    /** An override hit's mix (startOverrideMix): off, coming in (and held
     *  at 1 when in), or going. Worked out once a frame in write(). */
    enum MixMode { MixOff, MixIn, MixOut };
    MixMode m_mixMode = MixOff;
    qreal m_mix = 1.0;
    qreal m_mixFrom = 1.0;
    uint m_mixTime = 0;
    uint m_mixElapsed = 0;

    /** Show only: its function's stamp (Function::showStamp()), set once,
     *  when made (CreationScope). Last: read once a frame, never in the
     *  per-channel loop. */
    StampGate m_gateStamp;
};

/** @} */

#endif
