/*
  Q Light Controller Plus - Unit test
  preshowcheck_test.h

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

#ifndef PRESHOWCHECK_TEST_H
#define PRESHOWCHECK_TEST_H

#include <QObject>

class Doc;

class PreshowCheck_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    /** Every universe reaching its node: no output (red, Open Connections),
     *  an output (green), a node not answering (red, the entry's own fix) */
    void outputsReachTheirNodes();
    /** Show Review: a blocker is red and Open Show Review goes to it; none
     *  is green */
    void showReviewBlockers();
    /** Local Network refused (macOS, every network output failing) is red
     *  and opens Settings ▸ Performance; sending is green; elsewhere not used */
    void localNetwork();
    /** A pad patched and away is red (Show in Connections); here is green;
     *  none patched is not used */
    void padsPresent();
    /** Autosave off is a check with Turn It On (which turns it on); failed
     *  is red; on is green */
    void autosave();
    /** Battery, the screen, Wi-Fi and the rest of Settings ▸ Performance */
    void thisComputer();
    /** The footer's other entries: Show Details… opens the readiness dialog */
    void readinessFooter();
    /** Each line says its state in a symbol and a word; the summary too;
     *  the channel check is by hand and offers Quick Focus */
    void neverColourAlone();
    /** Show Review's Before the House Opens… opens the page */
    void reviewPanelOpensIt();
    /** Fix ▸ on a finding whose cue was deleted says the cue is gone */
    void fixOnAGoneCueSaysSo();

    void screenshots();

private:
    Doc *m_doc = nullptr;
};

#endif
