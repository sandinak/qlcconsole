/*
  Q Light Controller Plus - Unit tests
  editphysical_test.h

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

#ifndef EDITPHYSICAL_TEST_H
#define EDITPHYSICAL_TEST_H

#include <QObject>

class EditPhysical_Test final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    /** 7.5" in the width with inches set: shows 7.5 in, saves 191 mm */
    void inchesTypedInInches();
    /** 7.5" with millimetres set: shows 190.5 mm, saves 191 mm */
    void inchesTypedInMillimetres();
    /** A bare number is in the unit shown */
    void bareNumber();
    /** Weight follows: lb with inches, kg with millimetres */
    void weight();
    /** Nonsense is marked, explained and changes nothing */
    void invalid();
    /** A file's mm shows in the unit, and comes back unchanged */
    void loadShowsPreferredUnit();
    /** Typed but not yet entered still counts when the page is read */
    void uncommittedText();
    /** The page follows the preference when it is shown again */
    void followsPreference();
};

#endif
