/*
  Q Light Controller Plus
  testsettings.cpp

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

/* Linked into every UI test program (ui/test/CMakeLists.txt).
 *
 * Settings of the test's own, never the user's. The tests ran on
 * QSettings() with no organization name, which:
 *  - on Linux and macOS read and wrote the person's real settings (a run
 *    left programming/guideSeen, filters and expanded folders behind), and
 *  - on Windows went to the registry, where Qt keeps nothing at all without
 *    an organization name: every "is it remembered?" test failed there.
 * An INI file in a temporary folder, set up as soon as the application
 * object exists -- before any test function runs. A test that sets its own
 * path in initTestCase (autosave, showcontrol, ...) still wins.
 *
 * And no modal on a real display: with fresh settings the Programming tab's
 * first-run guide would open (QDialog::exec) and wait for a click that never
 * comes. Offscreen and minimal skip it already; QLC_NO_RECOVERY_PROMPT makes
 * xcb, cocoa and windows skip it too, and the start-up recovery offer. */

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSettings>

namespace
{
    void isolateTestSettings()
    {
        static QTemporaryDir dir;
        if (QCoreApplication::organizationName().isEmpty())
            QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        if (!dir.isValid())
            qFatal("no temporary folder for the test's settings");
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    }
}

Q_COREAPP_STARTUP_FUNCTION(isolateTestSettings)
