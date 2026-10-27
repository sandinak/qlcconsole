/*
  Q Light Controller Plus
  patchlist_test.h

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

#ifndef PATCHLIST_TEST_H
#define PATCHLIST_TEST_H

#include <QObject>

class Doc;

class PatchList_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void anEmptyShowHasNoRows();
    void rowsAreInPatchOrderTheInventoryLast();
    void addressesReadOneBased();
    void theMountIsNamed();
    void networkFixturesCarryTheirIp();
    void aClashIsListedWhereItAsked();
    void onlyTheIdsAskedFor();
    void theCsvIsSpreadsheetSafe();
    void theCsvFollowsThePlotUnits();
    void theHtmlHasATablePerUniverse();
    void thePdfIsWritten();
    void anUnwritablePdfSaysWhy();

    void theModeSaysItsChannelsOnce();
    void groupedByLocationThenAddress();
    void groupedByUniverseAndType();
    void pixelFixturesByTypeOrHeads();
    void theFilterPicksKindAndOwner();
    void theMultiUniverseFlagIsTheShows();
    void theCsvFollowsTheGrouping();
    void theXlsxIsAWorkbook();
    void aFormulaInANameIsNotRunBySpreadsheets();

private:
    Doc *m_doc = nullptr;
};

#endif
