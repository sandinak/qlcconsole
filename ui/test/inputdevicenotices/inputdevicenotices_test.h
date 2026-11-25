/*
  Q Light Controller Plus - Test Unit
  inputdevicenotices_test.h

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

#ifndef INPUTDEVICENOTICES_TEST_H
#define INPUTDEVICENOTICES_TEST_H

#include <QObject>
#include <QTemporaryDir>

class IOPluginStub;
class Doc;

class InputDeviceNotices_Test final : public QObject
{
    Q_OBJECT

private slots:
    void aPadOfTheSameNameIsNotTakenOverSilently_data();
    void aPadOfTheSameNameIsNotTakenOverSilently();
    /** Two universes wait for one name, one spare is free: only the first
     *  (where a replugged spare goes) is told to replug it. */
    void onlyTheFirstWaitingUniverseIsToldToReplug();
    void initTestCase();
    void init();
    void cleanup();

    void aDisconnectIsSaidOnceSettled();
    void aPadAsleepWhileIdleIsSaidAndRepatchedWhenWoken();
    void aReconnectIsSaidAndClearsTheFooter();
    void aFlapIsNotSaid();
    void absentWhenTheShowOpensIsSaid();
    void onlyDeviceInputsCount();
    void theDefaultSettleIsTwoAndAHalfSeconds();
    void aControllerSlotSaysWhichIsInUse();
    void anotherPadIsOfferedAndAdopted();
    // Phase R (CONTROL_SURFACES_EVAL §7)
    /** An unplug names what stops working with the device. */
    void namesLostJobs();
    /** A repatch says what moves first; Cancel leaves the patch. */
    void repatchWarnsAndLists();
    /** After a sleep: "Slept 40 s, finding devices…", once. */
    void wakeNoticeSaysSlept();
    /** A device missing before the sleep is said again, in amber, once the
     *  wake settles: the wake line never leaves the lost stop unsaid. */
    void aDeviceStillMissingAfterAWakeIsSaidAgain();
    /** The stop's first value after a replug, ignored as the desk's state
     *  dump, is said in the notices; a double tap after it is not ignored. */
    void anIgnoredStopDumpIsSaid();
    /** This computer's STOP on the unplugged device is named, though the
     *  unplug made it not live before the notice; again after a wake. */
    void namesThisComputersStopWhenItsDeviceGoes();
    /** A repatch says the desk roles stay with their device, not move. */
    void repatchSaysTheDeskStaysWithItsDevice();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
    QTemporaryDir m_settings;
};

#endif
