/*
  Q Light Controller Plus
  officeshow_test.h

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

#ifndef OFFICESHOW_TEST_H
#define OFFICESHOW_TEST_H

#include <QObject>
#include <QString>

class App;
class ShowTreeView;

/* Office.qlcc, the show for testing qlcconsole on Branson's office rig
   (tools/office-show/derive-office-rig.py says how to build it). Not part
   of the regular test run: each slot is skipped unless its environment
   variable is set. */
class OfficeShow_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** OFFICE_RIG=<rig.qxw from derive-office-rig.py>, OFFICE_OUT=<.qlcc>,
     *  OFFICE_USER_FIXTURES=<the user fixture folder>: opens the rig,
     *  builds the office stage, the groups, the palettes and the Show Tree
     *  production with the command line, and saves it. */
    void buildTheShow();

    /** OFFICE_SHOW=<.qlcc>: opens it on a computer with only the app's
     *  own definitions (no user folder) and checks it is complete, sends
     *  nowhere, and that each cue lights what it says. */
    void verifyTheShow();

private:
    bool open(const QString &path);
    ShowTreeView *showTree() const;

private:
    App *m_app = nullptr;
};

#endif
