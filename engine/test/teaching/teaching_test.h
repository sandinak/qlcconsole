/*
  Q Light Controller Plus
  teaching_test.h

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

#ifndef TEACHING_TEST_H
#define TEACHING_TEST_H

#include <QObject>

class Doc;

class Teaching_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Off by default; nothing is written; on marks the show modified */
    void offWritesNothing();
    /** On, allow output, lesson, link, hand-out time: saved and read back */
    void roundTrip();
    /** Attributes a newer build wrote are kept and written back */
    void unknownAttributesAreKept();
    /** A new show is off again; loading does not modify the show */
    void clearContentsResets();
    /** A whole show file carries it, and a file without it loads as off */
    void aShowFileCarriesIt();
    /** The copy's own settings are written; the open show is untouched */
    void saveOverrideLeavesTheShowAlone();
    /** The Operate lock can be for Teaching only (LockDesign) */
    void thePinCanSkipTheDesignLock();

private:
    Doc *m_doc = nullptr;
};

#endif
