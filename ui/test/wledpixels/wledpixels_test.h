/*
  Q Light Controller Plus
  wledpixels_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDPIXELS_TEST_H
#define WLEDPIXELS_TEST_H

#include <QObject>

/* A WLED patched as its LEDs: one RGB (or RGBW) head per LED, over Art-Net
   or sACN, spanning universes as WLED takes them. */
class WledPixels_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aWledOfFiftyLedsIsFiftyRgbHeadsInOneUniverse();
    void aWledOfFourHundredLedsSpansThreeUniversesAsWledLaysThemOut();
    void aWledFromALaterAddressStartsItsNextUniverseOnAnLed();
    void rgbwLedsTakeFourChannelsAndFillAUniverseExactly();
    void aPixelWledIsSavedAndLoadsBackAsItsLeds();
    void anotherFixtureGivenItsDefinitionGetsACopyOfItsOwn();
    void aPixelWledIsAStringNotAnAimedLight();
    void eachLedHeadSitsWhereThePixelPathDrawsIt();
    void fixOnDeviceAsksForMultiRgbAndStreamsOn();
    void fixOnDeviceAsksForMultiRgbwForAnRgbwString();
    void oneDeviceDrivenBothWaysIsSaidAndNeverFlipped();
    void routingGivesASpanningWledItsOwnRunOfArtNetUniverses();
    void universesOutOfOrderOrThroughTheWledOutputAreFound();
    void addFixtureOffersPixelsAndReadsTheLedCountFromTheDevice();
    void aChaseGoesOutAsArtNetBytesPerLedAcrossTwoUniverses();
    void aRealOlaProbeSeesTheChase();
};

#endif
