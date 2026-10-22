/*
  Q Light Controller Plus - Unit test
  palettenaming_test.h

  Copyright (C) Branson Matheson

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

#ifndef PALETTENAMING_TEST_H
#define PALETTENAMING_TEST_H

#include <QObject>

class PaletteNaming_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** The nearest named colour, light and dark variants included. */
    void colourNames_data();
    void colourNames();
    /** "if we go from Red to Dark Red .. we should auto update the title":
     *  only the colour part, whatever is round it stays; a name of the
     *  user's own stays whole. */
    void followColour_data();
    void followColour();
    void followDimmer_data();
    void followDimmer();
    void followStrobe_data();
    void followStrobe();
    void followPart_data();
    void followPart();
    void factoryNames();
    /** A new palette is named the way its folder's siblings are. */
    void conventionFromSiblings();
    /** The value and the name that follows it: one undo step; a drag
     *  merges into one; Undo puts both back, Redo both again. */
    void aValueStepUndoesTheNameWithIt();
};

#endif
