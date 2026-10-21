/*
  Q Light Controller Plus - Unit tests
  measurementunits_test.h

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

#ifndef MEASUREMENTUNITS_TEST_H
#define MEASUREMENTUNITS_TEST_H

#include <QObject>
#include <QTemporaryDir>

class MeasurementUnits_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void parseLength_data();
    void parseLength();
    void parseLengthInvalid_data();
    void parseLengthInvalid();
    void parseWeight_data();
    void parseWeight();
    void parseWeightInvalid_data();
    void parseWeightInvalid();

    void formatLength_data();
    void formatLength();
    void formatWeight_data();
    void formatWeight();

    /** What a file holds (whole mm) shows and parses back to the same mm,
     *  in every unit */
    void roundTripStoredMm();
    void roundTripWeight();

    void storedMm();
    void preference();
    /** The first show ever opened, if in feet, seeds an unset preference */
    void seedFromFirstShow();

private:
    QTemporaryDir m_settings;
};

#endif
