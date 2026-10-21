/*
  Q Light Controller Plus - Unit test
  fixturedefupdates_test.h

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

#ifndef FIXTUREDEFUPDATES_TEST_H
#define FIXTUREDEFUPDATES_TEST_H

#include <QObject>
#include <QTemporaryDir>

/* Every "computer" is a temporary HOME and the settings a temporary folder:
   the real user's definitions and shows are never read or written. */
class FixtureDefUpdates_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void versions();
    void olderIsNotJustDifferent();
    void theDiffSaysWhatChanges();
    void scanFindsOldCopiesUsedOrNot();
    void updateKeepsABackupAndUndoes();
    void keepMineAndIgnoreThisVersion();
    void aLinkUpdatesItsOriginal();
    void aLinkCanBeReplacedInstead();
    void neverWritesAShippedFile();
    void aStaleCopyInTheQlcPlusFolderIsOffered();
    void theShowFollowsAnUpdateModeByMode();
    void aChangedLayoutNeedsConfirming();
    void aMissingModeIsRefused();
    void anOlderEmbeddedCopySwitchesAndUndoes();
    void anEmbeddedCopyWithAChangedLayoutNamesWhatItTouches();
    void aNewerEmbeddedCopyCanBeInstalled();
    void theUserFolderUpdateReachesAnOpenShow();
    void realDefinitions();

private:
    QByteArray m_realHome;
    QByteArray m_realProfile;
    QTemporaryDir *m_root = nullptr;
    QTemporaryDir m_settings;
};

#endif
