/*
  Q Light Controller Plus
  wledfixture.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDFIXTURE_H
#define WLEDFIXTURE_H

#include <QStringList>

class Fixture;

/** @addtogroup engine Engine
 * @{
 */

/** A WLED fixture in WLED's own DMX "Effect" mode (the WLED Effect and WLED
 *  Lantern definitions): the console sends the effect, its speed, intensity,
 *  palette and three colours, and the device works out every LED. Its
 *  channels are found by the definitions' names. */
namespace WledFixture
{
    /** Whether @p fx is a WLED whose mode has an Effect channel -- one an
     *  effect is RUN ON rather than computed for. */
    bool runsEffects(const Fixture *fx);

    /** The index of @p fx's channel called @p name, -1 when its mode has
     *  none. */
    int channel(const Fixture *fx, const QString &name);

    /** The Effect channel value that selects WLED effect @p effect (by name,
     *  any case), -1 when @p fx does not have it. */
    int effectValue(const Fixture *fx, const QString &effect);

    /** The effects @p fx's Effect channel offers, in its order. */
    QStringList effectNames(const Fixture *fx);
}

/** @} */

#endif
