/*
  Q Light Controller Plus - Test Unit
  heldpropsui_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HELDPROPS_UI_TEST_H
#define HELDPROPS_UI_TEST_H

#include <QObject>

/* Props someone holds on the plot and in the rig view (docs/design/
 * BLOCKING.md, "Props someone holds"); the engine side is
 * engine/test/heldprops. */
class HeldPropsUi_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Give Each a Prop ▸ Candle on three selected people: three candles,
     *  one each, and one Undo takes them all away. */
    void giveEachIsOneUndoStep();
    /** Held By ▸ a person, in the blocking the plot shows; dropped on
     *  another person it is theirs; dropped on the deck it is put down. */
    void heldByAndHandOver();
    /** The item stands in the hand in every view; not on stage, hidden. */
    void drawnInTheHandOrNotAtAll();
    /** Each candle state draws differently, by shape, not colour alone. */
    void candleStatesDrawDifferently();
    /** The rig view draws a held staff, and nothing when it is off stage. */
    void drawnInTheRigView();
    /** QLC_HELDPROPS_SHOTS=dir: before/after pictures at three sizes. */
    void shots();
};

#endif
