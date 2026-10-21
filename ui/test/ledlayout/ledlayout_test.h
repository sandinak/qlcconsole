/*
  Q Light Controller Plus - Unit test
  ledlayout_test.h

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

#ifndef LEDLAYOUT_TEST_H
#define LEDLAYOUT_TEST_H

#include <QObject>
#include <QTemporaryDir>

class QLCFixtureDef;
class Fixture;
class Doc;

class LedLayout_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void rowPatternsAreReadAsTyped();
    void theXl450RowsGiveTheMapTheShowKeeps();
    void aPatternThatDoesNotFitSaysWhy();
    void clickingSwapsAndMovesHeads();
    void flipsMirrorTheGrid();
    void theGridCanBeResized();
    void aStripIsLaidOutInRows();
    void thePreviewWalksTheHeadsInOrder();
    void saveToShowKeepsItInTheShowOnly();
    void saveAsDefaultRoundTripsAndTheShowWins();
    void theFixtureManagerOffersItForMultiHeadFixtures();
    void thePlotAndStudioTreeOfferIt();
    void fixturePropertiesOffersIt();
    void theMatrixCanvasUsesTheLayout();
    void thePlotDrawsHeadsWhereTheLayoutSays();
    void theSplitWizardSharesTheEditor();

    /* With LEDLAYOUT_SHOW set to a copy of a show with an XL-450 (Dance
       Hysteria 0.6): its stored layout is what the editor makes from
       "RGB, W, RGB, W, RGB". With LEDLAYOUT_SHOTS, pictures of the editor. */
    void theRealShowsXl450();

private:
    Fixture *addXl450(const QString &name);

private:
    QTemporaryDir m_library;
    QLCFixtureDef *m_xl = nullptr;      ///< 165ch: 45 RGB heads, then 30 white
    QLCFixtureDef *m_strip = nullptr;   ///< 16 RGB then 16 white, no grid
    QLCFixtureDef *m_par = nullptr;     ///< one RGB head
    Doc *m_doc = nullptr;
};

#endif
