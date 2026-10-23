/*
  Q Light Controller Plus
  importmenu_test.h

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

#ifndef IMPORTMENU_TEST_H
#define IMPORTMENU_TEST_H

#include <QObject>
#include <QStringList>

class App;
class QAction;

class ImportMenu_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    /** File ▸ Import is a submenu: the sections, one entry per source,
     *  then "Any Supported File…" last. */
    void submenuHasTheSources();

    /** Each entry opens its file dialog with its own filters and hands the
     *  picked file to its own importer. */
    void eachEntryRoutesToItsImporter();
    void eachEntryRoutesToItsImporter_data();

    /** Cancel in the file dialog imports nothing. */
    void cancelImportsNothing();

    /** "Any Supported File…" offers every type and picks the importer by
     *  extension; an unknown one is read as a workspace, as before. */
    void anySupportedFileDispatchesByExtension();
    void anySupportedFileDispatchesByExtension_data();

    /** The entries are in the File menu whichever workspace view is on. */
    void entriesShowInEveryView();

    /** The Fixtures List entry, with its real importer, adds the list's
     *  fixtures to the show. */
    void fixtureListImportAddsFixtures();

private:
    QAction *entry(const QString &key) const;

    App *m_app = nullptr;
    QString m_pickTitle;
    QStringList m_pickFilters;
    QString m_pickResult;
    int m_picks = 0;
    QStringList m_runs;   ///< "key|path"
};

#endif
