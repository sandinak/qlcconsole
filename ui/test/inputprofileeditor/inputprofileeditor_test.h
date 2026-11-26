/*
  Q Light Controller Plus - Unit test
  inputprofileeditor_test.h

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

#ifndef INPUTPROFILEEDITOR_TEST_H
#define INPUTPROFILEEDITOR_TEST_H

#include <QObject>

class Doc;

/* S2, surface types (docs/design/CONTROL_SCOPES.md section 2): the Input
   Profile editor sets a profile's surface type, each control's role and
   each fader's declared fact. */
class InputProfileEditor_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aSurfaceTypeShowsRoleAndFaderRows();
    void aRoleAndAFactAreSavedWithTheProfile();
    void aBrokenRuleIsSaidAndNothingCloses();
    void shots();

private:
    Doc *m_doc = nullptr;
    QString m_path;
};

#endif
