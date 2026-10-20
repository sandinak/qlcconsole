/*
  Q Light Controller Plus
  updates_test.h

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

#ifndef UPDATES_TEST_H
#define UPDATES_TEST_H

#include <QObject>

class FakeGitHub;

/* Software Update (UpdateChecker / UpdateDownloader / UpdateInstaller /
 * UpdateManager) against a local HTTP server standing in for GitHub --
 * never the real one. */
class Updates_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void compareVersions_data();
    void compareVersions();
    void currentVersion();
    void parseReleases();
    void channels();
    void pickAsset_data();
    void pickAsset();
    void parseSums();

    void checkFindsUpdate();
    void checkStableIgnoresPrerelease();
    void checkSkipVersion();
    void checkUpToDate();
    void checkEtag();
    void checkRateLimited();
    void checkOffline();
    void checkTimeout();
    void checkDue();
    void policyDisablesEverything();

    void downloadVerified();
    void downloadShaMismatchRefused();
    void downloadWithoutSumsRefused();

    void nothingInstallsDuringShow();
    void cleanupAfterUpdate();
    void windowsScript();
    void dialogStates();

    void macHelperPrepare();
    void macHelperRefusals();
    void macHelperInstall();

private:
    QByteArray releasesJson(const QString &base) const;

    FakeGitHub *m_server = nullptr;
};

#endif
