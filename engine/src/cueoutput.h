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

class Doc;

class CueOutput
{
public:
    /** Per-fixture summary of an upcoming cue. */
    struct FixtureCue
    {
        bool  lit = false;          //!< master intensity > 0 in this cue
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
     *  (what a mark holds). Fixtures with no master-intensity channel report
     *  lit=false — they're not move-in-black candidates. */
    static QHash<quint32, FixtureCue> computeCues(Doc *doc, quint32 functionId);
};

#endif // CUEOUTPUT_H
