/*
  Q Light Controller Plus - Unit test
  showreviewpanel_test.h

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

#ifndef SHOWREVIEWPANEL_TEST_H
#define SHOWREVIEWPANEL_TEST_H

#include <QObject>

class Doc;

class ShowReviewPanel_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Findings by severity; the summary counts them */
    void listsBySeverity();
    /** Accept stores the note and moves it to Accepted; Reopen puts it back */
    void acceptAndReopen();
    /** Open / Accepted / All and the search */
    void filters();
    /** Fix: a footer fix runs the footer's action, others go to the app's jumper */
    void fixGoesToThePlace();
    /** A footer design entry no check reads is listed under its id */
    void footerEntriesFoldIn();
    /** The Show readiness dialog shows design entries' ids and opens the panel */
    void readinessDialogLeadsHere();
    /** PWR-12: I Meant It and Not Meant from the row; Show Me goes to the layer */
    void iMeantItFromTheRow();
    /** PWR-12: each breaching cue is marked in the Show Tree; a click opens its row */
    void showTreeMarksBreachingCues();

private:
    Doc *m_doc = nullptr;
};

#endif
