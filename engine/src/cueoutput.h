/*
  Q Light Controller Plus
  cueoutput.h

  Offline computation of the DMX a Function (Scene / Collection) WOULD produce if
  it ran — without starting it. The move-in-black planner uses this to peek at an
  upcoming cue: which fixtures it lights, and where it aims/colours them, so dark
  movers can be pre-positioned before the reveal. Pure value math, GUI-thread safe.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CUEOUTPUT_H
#define CUEOUTPUT_H

#include <QHash>
#include <QPair>
#include <memory>

#include "qlcengine_export.h"

class Doc;
class Fixture;

/** What a WLED look (QLCPalette::wledEffect()) sends the WLED fixtures it
 *  plays on -- the effect, its speed, intensity, palette, option and three
 *  colours, and the Master Dimmer -- as the EffectInstance that plays it
 *  works them out (EffectInstance::wledDeviceWrites()), so a cue moved in
 *  black is the cue played. An instance loads its script, so they are kept
 *  from ask to ask, built again only when the look's effect or settings
 *  change, and let go of when a pass did not ask for them. Main thread. */
class QLC_ENGINE_EXPORT WledLookCache
{
public:
    WledLookCache();
    ~WledLookCache();
    WledLookCache(const WledLookCache &) = delete;
    WledLookCache &operator=(const WledLookCache &) = delete;

    /** fixture → (channel → value) look @p paletteId sends in scene
     *  @p sceneId; empty when it is not a WLED look or plays on none. */
    QHash<quint32, QHash<quint32, uchar>> settings(Doc *doc, quint32 sceneId, quint32 paletteId);

    /** A pass of asks starts / ends: what it did not ask for is let go. */
    void beginPass();
    void endPass();
    /** How many looks are kept (for the tests). */
    int size() const { return int(m_entries.size()); }

private:
    struct Entry;
    QHash<QPair<quint32, quint32>, std::shared_ptr<Entry>> m_entries;
};

class QLC_ENGINE_EXPORT CueOutput
{
public:
    /** Per-fixture summary of an upcoming cue. */
    struct FixtureCue
    {
        bool  lit = false;          //!< lit in this cue: see lightsFixture()
        uchar intensity = 0;        //!< master-intensity DMX value (0 if none)
        QHash<quint32, uchar> nonIntensity;  //!< relative channel → value, sans master intensity
    };

    /** The DMX each fixture would receive if @p functionId ran alone: a map of
     *  fixtureId → (relative channel index → value). A Scene expands its baked
     *  values + non-effect, non-effect-scoped palettes (effects are dynamic and
     *  skipped); a Collection unions its members in run order (later wins). Other
     *  function types (Chaser/EFX/RGBMatrix/Show) are skipped — their output
     *  isn't a static, predictable cue state. */
    static QHash<quint32, QHash<quint32, uchar>> compute(Doc *doc, quint32 functionId);

    /** As compute(), split per fixture into master intensity vs everything else
     *  (what a mark holds; a shutter is in it). Fixtures with neither a
     *  master-intensity channel nor a shutter that can close
     *  (ShutterAutoOpen::closingShutters()) report lit=false — they're not
     *  move-in-black candidates. */
    static QHash<quint32, FixtureCue> computeCues(Doc *doc, quint32 functionId);

    /** As computeCues(), with what its WLED looks send their WLED fixtures
     *  (@p wled): the effect settings and colours as non-intensity, the
     *  look's Master Dimmer as the intensity (highest wins with the
     *  scene's) -- and a WLED in a scene is at rest (Solid, the defaults:
     *  WledFixture::settingsAtRest()) under what the scene sets, as
     *  Scene::write lays it out. A dark WLED can then be moved in black. */
    static QHash<quint32, FixtureCue> computeCues(Doc *doc, quint32 functionId, WledLookCache *wled);

    /** Whether @p fc lights @p fxi: its master dimmer above 0 (when it has
     *  one) and its shutter not closed. A shutter the cue does not set is
     *  not known closed; a fixture with no dimmer is lit only by a cue that
     *  opens one of its shutters; one with neither is never lit. "Closed"
     *  is the definition's close capabilities (ShutterAutoOpen::closesAt()). */
    static bool lightsFixture(const Fixture *fxi, const FixtureCue &fc);
};

#endif // CUEOUTPUT_H
