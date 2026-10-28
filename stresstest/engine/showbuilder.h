/*
  QLC+ stress test - parametric show builder

  Builds a large Doc (universes, RGB-heavy fixtures, and a mix of function
  types) from a ShowSpec. Shared by the engine torture harness and the
  workspace emitter so both stress the exact same content.
*/

#ifndef STRESS_SHOWBUILDER_H
#define STRESS_SHOWBUILDER_H

#include <QString>
#include <QList>

class Doc;
class Function;

struct ShowSpec
{
    int universes        = 16;   // number of DMX universes
    int fixturesPerUni   = 100;  // RGB fixtures per universe (100*3ch = 300/512)
    int scenes           = 200;  // static scenes touching random fixtures
    int chasers          = 40;   // chasers stepping through the scenes
    int matrices         = 16;   // RGBMatrix functions (CPU heavy per tick)
    int efx              = 16;    // EFX functions
    int collections      = 8;    // collections grouping other functions
    int scripts          = 0;    // Script functions (JS engine per tick)
    int sequences        = 0;    // Sequence functions (scene-bound chasers)
    int shows            = 0;    // Show functions (timeline of tracks)
    int sceneFixtures    = 0;    // fixtures each scene touches (0 = all fixtures / 8)
    // Show Tree-style cue lists: what a Show Tree cue list bakes to (a Chaser
    // of Collections, one per cue, each a Collection of layer Scenes), with
    // per-cue fade in / hold / fade out, auto-following and looping -- the
    // heaviest way a cue list runs (always mid-crossfade).
    int cuelists         = 0;
    int cuesPerList      = 8;
    int layersPerCue     = 4;
    int cueFadeMs        = 2000; // fade in and out per cue (hold 2-6 s)
    // One EFX per universe over all of its fixtures (RGB mode): every channel
    // of every universe changes every tick, so a healthy output path sends
    // every universe every frame (what the on-the-wire test needs).
    bool efxPerUniverse  = false;
    // Chasers in Common duration mode with the chaser's own duration (0) step
    // EVERY TICK (a 50 Hz chase): the builder's historical default, a worst
    // case. chaserPerStep: per-step fade in / hold / fade out (0-500 / 200-1000
    // / 0-500 ms), as the steps were always built -- a realistic chase.
    bool chaserPerStep   = false;
    unsigned int seed    = 1;     // RNG seed for reproducible shows

    // Every fixture is a Generic RGB (3 channels) so RGBMatrix/EFX have real
    // RGB heads to drive. 170 RGB fixtures = 510ch, the per-universe ceiling.
    static const int channelsPerFix = 3;

    // derived/reporting
    int totalFixtures() const { return universes * fixturesPerUni; }
    int totalChannels() const { return totalFixtures() * channelsPerFix; }
    int totalFunctions() const { return scenes + chasers + matrices + efx + collections + scripts + sequences + shows
                                       + cuelists * (1 + cuesPerList * (1 + layersPerCue)); }
};

// Populate an already-constructed Doc according to spec.
// The Doc must be constructed with at least spec.universes universes.
// Returns the list of top-level functions (scenes/chasers/matrices/efx/collections)
// that the caller may start. fixtureDir is the resources/fixtures path.
QList<Function*> buildShow(Doc *doc, const ShowSpec &spec,
                           const QString &fixtureDir, const QString &scriptsDir);

#endif
