/*
  Q Light Controller Plus
  autosave_test.cpp

  Copyright (C) Massimo Callegari

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

#include <QtTest>
#include <QSettings>

#include "autosave_test.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private

#include "qlcconfig.h"

void Autosave_Test::initTestCase()
{
    // Clear any existing settings
    QSettings settings;
    settings.remove("workspace/autosave");

    m_app = new App();
    m_app->init();
}

void Autosave_Test::cleanupTestCase()
{
    // Clean up settings
    QSettings settings;
    settings.remove("workspace/autosave");

    delete m_app;
    m_app = nullptr;
}

void Autosave_Test::defaultSettings()
{
    // Default autosave should be enabled with 5 minute interval
    QCOMPARE(m_app->isAutosaveEnabled(), true);
    QCOMPARE(m_app->autosaveInterval(), 5);
}

void Autosave_Test::enableDisable()
{
    // Disable autosave
    m_app->setAutosaveEnabled(false);
    QCOMPARE(m_app->isAutosaveEnabled(), false);

    // Verify it's saved in settings
    QSettings settings;
    QCOMPARE(settings.value("workspace/autosave/enabled").toBool(), false);

    // Re-enable autosave
    m_app->setAutosaveEnabled(true);
    QCOMPARE(m_app->isAutosaveEnabled(), true);
    QCOMPARE(settings.value("workspace/autosave/enabled").toBool(), true);
}

void Autosave_Test::intervalSettings()
{
    // Set a new interval
    m_app->setAutosaveInterval(10);
    QCOMPARE(m_app->autosaveInterval(), 10);

    // Verify it's saved in settings
    QSettings settings;
    QCOMPARE(settings.value("workspace/autosave/interval").toInt(), 10);

    // Test minimum interval (should be clamped to 1)
    m_app->setAutosaveInterval(0);
    QCOMPARE(m_app->autosaveInterval(), 1);

    // Restore default
    m_app->setAutosaveInterval(5);
}

void Autosave_Test::autosaveFilePath()
{
    // When no file is set, should use default location
    m_app->setFileName(QString());
    QString path = m_app->autosaveFilePath();

    // Should end with untitled.qxw.autosave
    QVERIFY(path.endsWith("untitled.qxw.autosave"));

    // Should contain the app's user data directory
    QVERIFY(path.contains(".qlcplus") || path.contains("QLC+") || path.contains("qlcconsole"));
}

void Autosave_Test::autosaveFilePathWithFileName()
{
    // When a file is set, autosave should be next to it
    QString testFile = "/tmp/test_workspace.qxw";
    m_app->setFileName(testFile);

    QString path = m_app->autosaveFilePath();
    QCOMPARE(path, QString("/tmp/test_workspace.qxw.autosave"));

    // Clean up
    m_app->setFileName(QString());
}

QTEST_MAIN(Autosave_Test)

