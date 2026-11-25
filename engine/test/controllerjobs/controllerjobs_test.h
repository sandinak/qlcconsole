/*
  Q Light Controller Plus
  controllerjobs_test.h

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

#ifndef CONTROLLERJOBS_TEST_H
#define CONTROLLERJOBS_TEST_H

#include <QObject>

class IOPluginStub;
class Doc;

/* Controller jobs' store and identity (docs/design/CONTROLLER_JOBS.md,
   invariants 1-3): who a pad is, two of one model told apart, every job
   by default, kept with this computer and never with the show. */
class ControllerJobs_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void anAddressInTheName_data();
    void anAddressInTheName();
    void theAddressThenThePortThenTheName();
    void twoOfAModelAreToldApart();
    void everyJobByDefault();
    void keptWithThisComputer();
    void aPortWithSlashesIsOneKey();
    void neverInTheShow();
    void theDescriptionsAPersonReads();
    /** Review 0.4.10: a universe's identity is worked out once per device
     *  (has() asks on every event; Linux reads /sys for the port), and
     *  again after a patch or a hot-plug. */
    void anIdentityIsWorkedOutOncePerDevice();
    /** This computer's desk (Q322): a MIDI port's identity on macOS (its
     *  name), Windows (a repeat's "2- " unit) and ALSA (client:port). */
    void midiLocationIdentity_data();
    void midiLocationIdentity();

private:
    Doc *m_doc = nullptr;
    IOPluginStub *m_stub = nullptr;
};

#endif
