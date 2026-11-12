/*
  Q Light Controller Plus - qlcconsole
  pagescript_test.h

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

#ifndef PAGESCRIPT_TEST_H
#define PAGESCRIPT_TEST_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJSValue>
#include <QObject>
#include <QStringList>

class QJSEngine;

/* The web pages' own scripts -- the phone / Steam Deck Run page (/run),
   the focus remote (/focus) and the gamepad reader both share -- run as
   the console serves them (WebAccessRun::html(), WebAccessFocus::html())
   in a QJSEngine, on a stand-in browser (pagestubs.js): the page's markup
   as a DOM, a WebSocket that records what the page sends, timers and
   animation frames stepped by the test, and a fake controller. Each test
   feeds the page the console's state and checks the words it sends and
   what it shows. No network, no window, no real time. */
class PageScript_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /* Both pages */
    void bothPagesLoadAndSayHello();
    void theyReconnectAndGiveThePinAgain();

    /* The Run page */
    void runButtonsSendTheirWords();
    void stopNeedsASecondTapInTwoSeconds();
    void nothingIsSentWhileRemoteGoIsOff();
    void thePinIsAskedAndKeptForTheSession();
    void everyPadWordNeedsLBHeld();
    void theControllerTestSendsNothing();
    void acksSayWhyInPlainWords();
    void teachingShowsWhatItHolds();
    void blindAndSoloFollowTheState();
    void blindAndSoloButtonsSendTheirWords();
    void theConsolesOwnStateDrawsOnThePage();

    /* The focus remote */
    void focusButtonsAndKeysPressTheirRoles();
    void aTriggerPressesPastHalfAndLetsGoUnderAThird();
    void aStickSendsMovesAtMost40ASecond();
    void aStickInsideTheDeadZoneSendsNothing();
    void theDpadRepeatsWhileHeld();
    void arrowKeysNudgeAndTypingDoesNot();
    void focusShowsBlindAndTeaching();

private:
    void loadPage(const QString &html);
    /** Run @a code in the page; an error is kept and fails the test. */
    QJSValue js(const QString &code);
    /** The console says @a message to the page (its websocket). */
    void hear(const QString &message);
    void runState(const QJsonObject &state);
    void focusState(const QJsonObject &state);
    /** What the page sent since the last call. */
    QStringList sent();
    QString text(const QString &id);
    bool shown(const QString &id);
    bool enabled(const QString &id);
    bool click(const QString &id);
    /** Hold these controls down (role: value) for @a ms. */
    void pad(const QJsonObject &down, int ms = 50, const QJsonArray &axes = QJsonArray());

private:
    QJSEngine *m_js = nullptr;
    QStringList m_errors;
};

#endif
