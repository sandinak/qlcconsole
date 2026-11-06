/*
  Q Light Controller Plus
  feetinchesspinbox.h

  A rig length field: a LengthSpinBox whose value() is in metres, or in
  feet for the older callers that keep their numbers in feet. What it shows
  and takes is always Settings > Units -- 5' 6" with feet, 167.6 cm with
  centimetres -- and a length typed with its own unit is converted (5' 6",
  5.5 ft, 66 in, 1.676 m). The value's unit is the caller's bookkeeping
  only, never what is displayed.

  Header-only and deliberately free of the Q_OBJECT macro (it adds no new
  signals/slots), so it needs no moc pass or CMake entry.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FEETINCHESSPINBOX_H
#define FEETINCHESSPINBOX_H

#include "lengthspinbox.h"
#include "measurementunits.h"

/** \addtogroup ui_mon DMX Monitor
 * @{
 */

class FeetInchesSpinBox : public LengthSpinBox
{
public:
    /** @a valueInFeet: value() is in feet, else in metres */
    explicit FeetInchesSpinBox(bool valueInFeet, QWidget *parent = nullptr)
        : LengthSpinBox(valueInFeet ? 304.8 : 1000.0, parent)
    {
        setDecimals(valueInFeet ? 3 : 4);
    }

    /** What the rig's fields keep their numbers in: feet when the person
     *  works in feet or inches, else metres (Settings > Units). Bookkeeping
     *  only -- every field and label shows the preferred unit itself. */
    static bool valuesInFeet()
    {
        return MeasurementUnits::isImperial(MeasurementUnits::preferredLength());
    }
};

/** @} */

#endif // FEETINCHESSPINBOX_H
