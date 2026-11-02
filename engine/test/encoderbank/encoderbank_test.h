/*
  Q Light Controller Plus - Unit test
  encoderbank_test.h

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

#ifndef ENCODERBANK_TEST_H
#define ENCODERBANK_TEST_H

#include <QObject>

class EncoderBank;
class IOPluginStub;
class Fixture;
class Doc;

class EncoderBank_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // The controller side
    void decodeTwosComplement();
    void decodeSignedBit();
    void decodeBinaryOffset();
    void decodeAgreesWithTheInputSource();
    void absoluteKnobsReadTheirChange();
    void invertTurnsItAround();
    void turnsAreGatheredIntoOne();
    void theInputPathDoesNotAllocate();
    void turnsComeThroughTheInputPatch();
    void aBankButtonStepsAndWraps();
    void directButtonsPickABank();
    void pushIsFineWhenTurnedAndHomeWhenTapped();
    void operateRefusesTurnsButStepsBanks();
    void offIsNotListening();
    void ringsAreSentOnlyWhenTheyChange();
    void bankLampsFollowTheBank();
    void everyPresetIsAWholeMapping();
    void savedWithTheControlMapAndLoadedBack();
    void aShowWithoutOneSavesAsBefore();

    // What a slot means on fixtures
    void intensityClampsAtBothEnds();
    void coarseAndFineSteps();
    void hueWrapsAndSaturationClamps();
    void colourChannelsMoveEachFixtureFromWhereItIs();
    void panTiltAreSixteenBitAndHomeIsCentre();
    void goboStepsThroughItsRanges();
    void aDimmerlessLightTakesIntensityFromItsColour();
    void aLookDrivingAChannelIsLeftAlone();
    void readGivesTheRingsValue();

private:
    /** A value byte as the MIDI plugin scales a 0-127 data byte. */
    static uchar dmx(int midi) { return midi == 127 ? 255 : uchar(midi << 1); }
    void send(quint32 channel, uchar value);
    EncoderBank *bank() const;
    /** A moving head with dimmer, 16-bit pan and tilt, RGBW, zoom, a gobo
     *  wheel and its rotation; a plain RGB par; a plain dimmer. */
    Fixture *mover(const QString &name, quint32 address);
    Fixture *par(const QString &name, quint32 address);

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif
