/*
  Q Light Controller - Unit tests
  qlcfile_test.h

  Copyright (C) Heikki Junnila

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

#ifndef QLCFILE_TEST_H
#define QLCFILE_TEST_H

#include <QObject>

class QLCFile_Test final : public QObject
{
    Q_OBJECT

private slots:
    /** Cloud storage: a file whose contents are not on this machine is
     *  skipped by the loaders rather than waited on, and remembered. */
    void onlineOnlyFilesAreNotedNotWaitedOn();
    void aRemovedLegacyLinkStaysRemoved();
    /** Effect scripts, presets, bundles, head layouts, gobos and network
     *  gear live in the per-platform user folder, like fixture definitions
     *  (they were the Mac's path everywhere). */
    void userFoldersAreThePlatformsUserFolder();
    void XMLReader();
    void getXMLHeader();
    void errorString();
    void version();
    void windowManager();
};

#endif
