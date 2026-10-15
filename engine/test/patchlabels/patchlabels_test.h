/*
  Q Light Controller Plus
  patchlabels_test.h

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

#ifndef PATCHLABELS_TEST_H
#define PATCHLABELS_TEST_H

#include <QObject>

class PatchLabels_Test final : public QObject
{
    Q_OBJECT

private slots:
    void theStocksAreThere();
    void anUnknownStockIsTheDefault();
    void avery5160PlacesThirtyAPage();
    void theRollIsOneAPage();
    void pageCounts();
    void aPatchedLabel();
    void unpatchedAndClashLabels();
    void aNetworkLabelCarriesItsIp();
    void aLabelFromTheShow();
    void thePdfHasAPagePerSheet();
    void theRollPdfIsALabelAPage();
    void nothingToLabelIsAnError();
    void anUnwritablePathIsAnError();
};

#endif
