/*
  Q Light Controller Plus - Test Unit
  patchlistdialog_test.h

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

#ifndef PATCHLISTDIALOG_TEST_H
#define PATCHLISTDIALOG_TEST_H

#include <QTemporaryDir>
#include <QObject>

class Doc;

class PatchListDialog_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void theTableIsInPatchOrder();
    void copyPutsTheCsvOnTheClipboard();
    void saveCsvWritesTheSpreadsheet();
    void savePdfAsksWhereAndWrites();
    void f1OpensThePatchListHelp();

    void selectedRowsAreInListOrder();
    void labelsOpensTheLabelsDialog();
    void labelsAreForAllOrTheSelectedRows();
    void theLabelStockIsRemembered();
    void saveLabelsAsksWhereAndWrites();
    void aNetworkFixturesIpIsOnMouseover();

private:
    Doc *m_doc = nullptr;
    QTemporaryDir m_settingsDir;           //!< QSettings, not the user's
};

#endif
