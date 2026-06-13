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
    unsigned int seed    = 1;     // RNG seed for reproducible shows

    // Every fixture is a Generic RGB (3 channels) so RGBMatrix/EFX have real
    // RGB heads to drive. 170 RGB fixtures = 510ch, the per-universe ceiling.
    static const int channelsPerFix = 3;

    // derived/reporting
    int totalFixtures() const { return universes * fixturesPerUni; }
    int totalChannels() const { return totalFixtures() * channelsPerFix; }
    int totalFunctions() const { return scenes + chasers + matrices + efx + collections + scripts + sequences + shows; }
};

// Populate an already-constructed Doc according to spec.
// The Doc must be constructed with at least spec.universes universes.
// Returns the list of top-level functions (scenes/chasers/matrices/efx/collections)
// that the caller may start. fixtureDir is the resources/fixtures path.
QList<Function*> buildShow(Doc *doc, const ShowSpec &spec,
                           const QString &fixtureDir, const QString &scriptsDir);

#endif
