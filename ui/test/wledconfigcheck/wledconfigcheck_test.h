/*
  Q Light Controller Plus
  wledconfigcheck_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef WLEDCONFIGCHECK_TEST_H
#define WLEDCONFIGCHECK_TEST_H

#include <QObject>

class Doc;

class WledConfigCheck_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Driven through WLED's API: nothing may take it over with a stream. */
    void aLanternDrivenByItsApiMustNotTakeStreams();
    /** Driven by Art-Net: the device's protocol, universe, address and DMX
     *  mode are the patch's and the definition's. */
    void anArtNetLanternMustListenWhereItIsPatched();
    /** Read from the device, in the footer, fixed on the device, read back. */
    void aMismatchIsFoundShownAndFixed();
    /** A device that does not answer: said once, never read; and back. */
    void anUnreachableDeviceIsSaidOnceAndNeverRead();
    /** The show's LED count against the device's: a finding with its own
     *  fix, all of hw.led sent back, only the length and total changed. */
    void aDifferentLedCountIsFoundWithItsOwnFix();
    /** Several outputs, or a 2D matrix: refused, said, nothing written. */
    void aMultiOutputWledIsRefused();
    /** A Teaching show holding real output writes nothing to a device. */
    void aHeldClassroomWritesNothing();
    /** Asked with the warning, written, waited for (rebooted when it has
     *  to be), read back. */
    void theLedCountIsSetAndReadBack();
    void theLedCountIsSetAndReadBack_data();
    /** Write Segments: read, asked, written, read back; cleared, refused. */
    void writeSegmentsAsksWritesAndReadsBack();
    /** The confirmations name the device, the count and that Undo can't. */
    void theConfirmationsSayWhatTheyDo();
    /** A real WLED, by hand (QLC_WLED_LIVE=ip); skipped otherwise. */
    void liveDevice();

private:
    Doc *m_doc = nullptr;
};

#endif
