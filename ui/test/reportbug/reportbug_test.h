/*
  Q Light Controller Plus
  reportbug_test.h

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

#ifndef REPORTBUG_TEST_H
#define REPORTBUG_TEST_H

#include <QObject>
#include <QList>
#include <QUrl>

class Doc;

class ReportBug_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    /** The URL names the issue form, prefixes the title with [Bug] and
     *  percent-encodes every field ('+', '&', '=', '%', '#', new lines). */
    void theUrlCarriesTheFormAndEncodedFields();

    /** The home folder becomes ~; the show's path and name, fixture and
     *  group names, the user name, IPv4/IPv6/MAC addresses and
     *  password values come out; versions and times stay. */
    void scrubbingTakesOutWhatIsPrivate();

    /** A part left unticked is not in the details nor the URL; the layout
     *  report and the log start unticked, and what they hold is scrubbed. */
    void untickedPartsAreAbsent();

    /** Too long for a link: the URL stays under the limit with the cut note,
     *  and the whole report goes to the clipboard. */
    void anOverLongReportIsCutAndCopied();

    /** Copy Report puts the whole Markdown report on the clipboard and
     *  opens nothing. */
    void copyReportOpensNothing();

    /** Help > Troubleshooting > Report a Bug... is there in every view, and
     *  opens the dialog naming that view. */
    void theMenuItemIsInEveryView();

private:
    Doc *m_doc = nullptr;
    QList<QUrl> m_opened;
};

#endif
