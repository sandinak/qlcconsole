/*
  Q Light Controller Plus - qlcconsole
  webaccess_test.h

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

#ifndef WEBACCESS_TEST_H
#define WEBACCESS_TEST_H

#include <QObject>
#include <QTemporaryDir>
#include <QByteArray>
#include <QString>

class Doc;
class VirtualConsole;
class SimpleDesk;
class WebAccess;

/* Web access (-w, -wa) as a client on the network sees it: HTTP and
   WebSocket over loopback, against a WebAccess built in the test. */
class WebAccess_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /* Who can reach it, and who it lets in */
    void itListensOnlyOnLoopbackByDefault();
    void withoutAuthAProjectCannotBeLoaded();
    void withoutAuthAFixtureCannotBeStored();
    void authWithNoUsersIsNotOpenToEveryone();
    void theFirstVisitorCannotMakeThemselvesAdmin();
    void aLoggedInGuestCannotBlackOutTheShow();
    void aPageOnAnotherSiteCannotDriveTheShow();

    /* What it serves */
    void filesOutsideTheWebFolderAreNotServed();
    void anUploadedFixtureStaysInTheFixtureFolder();
    void namesFromTheShowAreEscapedInThePage();

    /* Malformed input */
    void shortCommandsDoNotCrash();
    void unknownWidgetIdsDoNotCrash();
    void aMalformedUrlDoesNotAbort();
    /** The Pi's network page: an SSID or a password typed there is one
     *  nmcli argument, never split into several (red team, RC8 leftover). */
    void networkSettingsAreSingleArguments();
    void aHugeUploadIsRefusedEarly();
    void shutdownWithALiveClientIsClean();

    /* RC8 */
    void withoutAuthThisComputerStillHasFullAccess();
    void withoutAuthTheNetworkCanLookButNotTouch();
    void aRebindingNameIsNotThisComputer();
    void aFormOnAnotherSiteCannotLoadAProject();
    void theFirstAdminComesFromThisComputer();
    void itListensWhereItIsTold();
    void systemCommandsNeedAnAdmin();
    void uploadNamesAreSanitised();

    /* The password file and Basic authentication, WebAccessAuth on its own
       (coverage campaign): no server, no network. */
    void passwordFileFormats();
    void passwordFileRoundTrip();
    void basicAuthChecksTheCredentials();

    /* The show's own web access (ShowWebAccess, WebAccessController): saved
       with the show, started and stopped as shows open. */
    void theShowSettingRoundTrips();
    void anOldShowLoadsWithItOff();
    void aBadPortInTheFileFallsBack();
    void openingAShowStartsAndStopsIt();
    void aNewPortRestartsIt();
    void aPortInUseIsAStatusNotACrash();
    void theCommandLineWins();
    void aShowLoadedFromTheWebKeepsItRunning();

private:
    void startWeb(bool auth, const QString &passwdFile = QString());
    QByteArray http(const QByteArray &request, int timeoutMs = 5000);
    QString wsRoundTrip(const QStringList &commands, const QByteArray &origin = QByteArray());

private:
    QTemporaryDir m_home;
    Doc *m_doc = nullptr;
    VirtualConsole *m_vc = nullptr;
    SimpleDesk *m_sd = nullptr;
    WebAccess *m_web = nullptr;
    quint16 m_port = 0;
};

#endif
