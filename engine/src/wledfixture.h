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
#include <QVector>

class QLCFixtureDef;
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

    /** Whether @p fx is a string of LEDs run by a WLED -- driven by WLED's
     *  own API, or a WLED definition -- rather than a light that is hung and
     *  aimed. Fixture Properties shows such a string what a string has
     *  (connection, LEDs, string length, pixel path) and hides the mounted
     *  light's rows (aim, tilt, beam, masking, orientation), keeping their
     *  stored values. A WLED driving something that pans or tilts is a
     *  mounted light again: that is the way back in for one that drives one. */
    bool isPixelString(const Fixture *fx);

    /** The index of @p fx's channel called @p name, -1 when its mode has
     *  none. */
    int channel(const Fixture *fx, const QString &name);

    /** The Effect channel value that selects WLED effect @p effect (by name,
     *  any case), -1 when @p fx does not have it. */
    int effectValue(const Fixture *fx, const QString &effect);

    /** The effects @p fx's Effect channel offers, in its order. */
    QStringList effectNames(const Fixture *fx);

    /*********************************************************************
     * Pixels: one RGB (or RGBW) head per LED
     *********************************************************************/

    /** WLED's own DMX modes (WLED const.h, DMX_MODE_*): what its
     *  "DMX mode" setting (if.live.dmx.mode in /json/cfg) holds. */
    enum DmxMode
    {
        DmxDisabled = 0,
        DmxSingleRgb = 1,
        DmxSingleDrgb = 2,
        DmxEffect = 3,
        DmxMultiRgb = 4,
        DmxMultiDrgb = 5,
        DmxMultiRgbw = 6
    };

    /** The most LEDs a pixel mode is made for. */
    const int MaxPixels = 2048;

    /** "Pixels (Multi RGB)" or "Pixels (Multi RGBW)": the mode a WLED is
     *  patched in as its LEDs, every LED a head of its own. */
    QString pixelModeName(bool rgbw);
    /** Whether @p name is one of those (and which). */
    bool isPixelModeName(const QString &name, bool *rgbw = nullptr);
    /** Whether @p fx is a WLED in a pixel mode. */
    bool isPixelMode(const Fixture *fx);
    /** Whether its pixel mode has a white channel per LED. */
    bool isRgbw(const Fixture *fx);

    /** The WLED DMX mode @p fx's mode needs (DmxMode), -1 when it is none
     *  of the ones a definition here drives. */
    int dmxModeFor(const Fixture *fx);

    /** How WLED lays LEDs over consecutive universes in Multi RGB / RGBW
     *  (WLED e131.cpp): the first universe from the start address, as many
     *  whole LEDs as fit up to channel 512; every universe after from
     *  channel 1, 170 RGB or 128 RGBW LEDs each. Only a transport with
     *  512-channel universes (Art-Net, sACN) has this: a DDP output would
     *  send the LEDs as one run and never asks. */
    struct UniverseSpan
    {
        QVector<int> leds;      ///< LEDs in each universe, the first first
        int channels = 0;       ///< the fixture's footprint, from its address
        /** LEDs that fit in the first universe; 0 when it is one run. */
        int firstUniverseLeds = 0;
    };
    UniverseSpan universeSpan(int leds, bool rgbw, quint32 address);
    /** The LEDs a full universe after the first carries: 170 or 128. */
    int ledsPerUniverse(bool rgbw);

    /** Whether @p fx's transport sends 512-channel universes (Art-Net,
     *  sACN, DMX): where the universe span applies. A DDP transport would
     *  answer false here and get one run of LEDs. */
    bool sendsUniverses(const Fixture *fx);

    /** How many universes @p fx's channels reach into, from its own (1 for
     *  any fixture that fits in one). */
    int universeCount(const Fixture *fx);

    /** A definition for a WLED of @p leds LEDs in a pixel mode, its one mode
     *  pixelModeName(@p rgbw): one head per LED, Red/Green/Blue(/White)
     *  each. Named (manufacturer, model) and bodied (its physical; the body's
     *  own LEDs none, since its heads are its LEDs) like @p like, a WLED definition from a file,
     *  so it saves and finds its way back as that model in a pixel mode.
     *  @p universeAddress >= 0: laid over universes from that address
     *  (0-based, universeSpan()) -- where a universe ends between LEDs, the
     *  channels left to its end are gaps (no head), so each universe starts
     *  on an LED as WLED expects. -1: one run, no gaps (a transport without
     *  universes). It belongs to the fixture given it (Fixture owns such a
     *  definition). */
    QLCFixtureDef *pixelDefinition(const QLCFixtureDef *like, int leds, bool rgbw,
                                   int universeAddress = -1);
    /** Whether @p def is one pixelDefinition() made (not from a file). */
    bool isPixelDefinition(const QLCFixtureDef *def);
    /** A fresh copy of such a definition, built again in a pass (a
     *  definition's own copy matches every channel by name: slow for
     *  thousands). */
    QLCFixtureDef *copyPixelDefinition(const QLCFixtureDef *def);
    /** The LEDs a pixel definition of @p channels channels from @p address
     *  has (the inverse of universeSpan()), for a show that saved the
     *  footprint only. */
    int ledsForChannels(int channels, bool rgbw, quint32 address, bool universes);

    /** Make @p fx a WLED of @p leds LEDs in a pixel mode, named and bodied
     *  like @p like (its definition now, when null), laid over universes
     *  from its address when it sendsUniverses(); several universes marks it
     *  cross-universe. False when there is nothing to make it from. */
    bool applyPixels(Fixture *fx, const QLCFixtureDef *like, int leds, bool rgbw);
    /** The same again for where @p fx is now (its address moved): its
     *  gaps follow. Nothing for a fixture not in a pixel mode. */
    void refitPixels(Fixture *fx);
}

/** @} */

#endif
