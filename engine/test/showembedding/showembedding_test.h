/*
  Q Light Controller Plus - Unit test
  showembedding_test.h

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

#ifndef SHOWEMBEDDING_TEST_H
#define SHOWEMBEDDING_TEST_H

#include <QObject>
#include <QTemporaryDir>
#include <QByteArray>

#include <memory>

class Doc;

/* Each "computer" is a home folder of its own (HOME pointed at a temporary
   folder): none of these read or write the real one. */
class ShowEmbedding_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void onlyAQlccEmbeds();
    void savesExactBytesOfWhatIsNotShipped();
    void aCleanComputerOpensTheShowComplete();
    void installPutsTheShowsCopiesOnThisComputer();
    void identicalCopiesSayNothing();
    void aDifferentCopyKeepsTheShows();
    void whichSideIsNewer();
    void definitionCompatibility();
    void scriptCompatibility();
    void useThisComputersSwitchesAndIsEmbedded();
    void useThisComputersRefusesWhatWouldOverlap();
    void keepTheShowsIsRemembered();
    void installTheShowsReplacesThisComputersFile();
    void saveNeverSwapsVersions();
    void closingPutsThisComputersBack();
    void aFileThatIsNotAShowLeavesTheOpenOneAlone();
    void aCollectedFolderOpensCompleteAsQxw();
    /** transferPictures(): one in the show's folder keeps its place, one
     *  from elsewhere goes into images/, a different picture of the same
     *  name gets "-2", the same picture shares its copy, a missing file is
     *  left out; RGB matrix images count; the folder gets them all. */
    void transferPicturesHaveAPlaceEach();

private:
    /** A computer: HOME at a fresh folder, with these user files. */
    QString newComputer();
    Doc *startDoc();
    /** Computer A's show, saved as a .qlcc in m_shows. */
    QString buildShow();
    void save(Doc *doc, const QString &path, bool embed);
    void open(Doc *doc, const QString &path);

    QByteArray m_realHome;
    QByteArray m_realProfile;
    std::unique_ptr<QTemporaryDir> m_root;   ///< every computer and show of a test
    QString m_shipped;                       ///< the app's own scripts (override)
    int m_computers = 0;
};

#endif
