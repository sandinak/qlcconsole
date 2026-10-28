/*
  Q Light Controller Plus - Unit test
  stockroundtrip_test.h

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

#ifndef STOCKROUNDTRIP_TEST_H
#define STOCKROUNDTRIP_TEST_H

#include <QObject>

class App;

class StockRoundTrip_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /** A show saved here as .qxw, read by stock QLC+ 4.14.4's engine:
     *  every fixture, group, channel group, palette and function, as
     *  saved, and what qlcconsole adds is read past without harm. */
    void aQxwSavedHereOpensInStockQlcPlus4();
    /** The notice's list is what stock actually leaves out: the Show Tree
     *  block is in the file, and stock still reads the show whole. */
    void theNoticeNamesWhatStockLeavesOut();

private:
    App *m_app = nullptr;
};

#endif
