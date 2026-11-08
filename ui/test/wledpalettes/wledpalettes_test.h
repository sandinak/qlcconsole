/*
  Q Light Controller Plus
  wledpalettes_test.h

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

#ifndef WLEDPALETTES_TEST_H
#define WLEDPALETTES_TEST_H

#include <QObject>

class WledPalettes_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();

    /** A real device's palx (the office WLED, 10 pages) is read whole. */
    void aCapturedPalxIsReadWhole();
    /** Pages fold into one document, the later page adding its palettes. */
    void pagesMergeIntoOne();
    /** Stops sample as a gradient; positions clamp at the ends. */
    void stopsSampleAsAGradient();
    /** "c1".."c3" take the effect's colours 1-3 as they are now. */
    void colourSlotsTakeTheEffectsColours();
    /** "r" is drawn as a soft rainbow, not one colour. */
    void randomIsDrawnAsARainbow();
    /** The bundled built-ins: 72 names, a strip each, none empty. */
    void theBundledBuiltInsAreComplete();
    /** Kept palettes win; without them the bundled ones, offline. */
    void aDeviceUsesItsKeptPalettesElseTheBundledOnes();
    /** The strip is a gradient of the palette's colours, slots included. */
    void theStripIsDrawnFromTheColours();
    /** The dropdown: a strip and a name each, the number as data. */
    void theDropdownHasAStripAndANameEach();
};

#endif
