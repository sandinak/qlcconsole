/*
  Q Light Controller Plus - Unit test
  configimport_test.h

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

#ifndef CONFIGIMPORT_TEST_H
#define CONFIGIMPORT_TEST_H

#include <QObject>
#include <QJsonObject>

class Doc;

class ConfigImport_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void readsTheSource();
    void notAShowFile();
    void offNetworkImportSucceedsWithTheNoRouteWarning();
    void routeRequirementsAreCopiedIntact();
    void defaultRouteIsAProminentWarning();
    void aRealRouteWarnsNothing();
    void mappingMovesUniversesAndAddresses();
    void sectionsAndUniverseSubset();
    void collisionNeedsAChoice();
    void collisionChoices_data();
    void collisionChoices();
    void missingPluginNeedsAChoice();
    void missingPluginChoices();
    void pluginMapRemaps();
    void profileMissingWarns();
    void nothingAppliedWhenAProblem();
    void operateRefused();
    void fixturesAreNotTouched();

    void oneUndoStepThroughAPlan();
    void failingPlanPutsEverythingBack();
    void commandLineFormPreviewsThenApplies();
    void roundTrip();
    void readAndPreviewAreQuestions();
    void worksWithoutAnyAi();

private:
    QString sample(const QString &name = QStringLiteral("evo26.qxw")) const;
    QJsonObject args(const QJsonObject &extra = QJsonObject()) const;
    QString universeXml(int index) const;
    void showRoute(const QString &kind);

    Doc *m_doc = nullptr;
};

#endif
