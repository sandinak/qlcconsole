/*
  Q Light Controller Plus
  wledshow_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDSHOW_TEST_H
#define WLEDSHOW_TEST_H

#include <QObject>

/* A WLED from Add Fixture to the stage, on the real WLED output plugin and a
   stand-in device (ui/test/common/fakewled.h): patched, its settings found
   and fixed, its string a fountain's Sprinkle loops written as segments,
   and a scene's effect played on it. */
class WledShow_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** The effect a scene sets ends up on every span of the fountain, each
     *  turned by Reverse from its own way. */
    void anEffectFromAShowPlaysOnEverySpanOfAFountain();
};

#endif
