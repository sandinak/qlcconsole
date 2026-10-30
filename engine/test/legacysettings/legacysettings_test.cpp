/*
  qlcconsole - Unit tests
  legacysettings_test.cpp

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

#include "legacysettings_test.h"
#include "legacysettings.h"

/* Both stores are .ini files in a temporary folder: the real QLC+ and
   qlcconsole settings of whoever runs the tests are never opened. */

void LegacySettings_Test::init()
{
    delete m_dir;
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
    m_from = m_dir->filePath("qlcplus.ini");
    m_to = m_dir->filePath("qlcconsole.ini");
}

QString LegacySettings_Test::file(const QString &name)
{
    const QString path = m_dir->filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write("<Workspace/>");
    return path;
}

void LegacySettings_Test::theStoreIsQlcPlus4s()
{
    const QPair<QString, QString> scope = LegacySettings::qlcPlusScope();
    QCOMPARE(scope.second, QString("Q Light Controller Plus"));
#if defined(Q_OS_MACOS)
    QCOMPARE(scope.first, QString("qlcplus.org"));      // org.qlcplus.Q Light Controller Plus
#else
    QCOMPARE(scope.first, QString("qlcplus"));
#endif

    /* The tests' own file in its place: the real one is never opened */
    {
        QSettings ini(m_from, QSettings::IniFormat);
        ini.setValue("workspace/recent0", "x");
    }
    qputenv("QLC_QLCPLUS_SETTINGS", m_from.toLocal8Bit());
    std::unique_ptr<QSettings> store = LegacySettings::openQlcPlus();
    qunsetenv("QLC_QLCPLUS_SETTINGS");
    QCOMPARE(store->fileName(), m_from);
    QCOMPARE(store->value("workspace/recent0").toString(), QString("x"));
}

void LegacySettings_Test::recentShowsComeAfterQlcconsolesOwn()
{
    const QString mine = file("Mine.qlcc");
    const QString both = file("Both.qxw");
    const QString theirs = file("Theirs.qxw");
    {
        QSettings from(m_from, QSettings::IniFormat);
        from.setValue("workspace/recent0", theirs);
        from.setValue("workspace/recent1", both);
        from.setValue("workspace/recent2", m_dir->filePath("Gone.qxw"));   // no longer there
        from.setValue("workspace/workingpath", m_dir->path());
        QSettings to(m_to, QSettings::IniFormat);
        to.setValue("workspace/recent0", mine);
        to.setValue("workspace/recent1", both);
    }
    QSettings from(m_from, QSettings::IniFormat);
    QSettings to(m_to, QSettings::IniFormat);
    const LegacySettings::Plan plan = LegacySettings::plan(from, to);
    QCOMPARE(plan.recentFiles, QStringList() << theirs);
    QVERIFY(plan.uiKeys.contains("workspace/workingpath"));

    QCOMPARE(LegacySettings::apply(from, to, plan), 2);
    QCOMPARE(to.value("workspace/recent0").toString(), mine);
    QCOMPARE(to.value("workspace/recent1").toString(), both);
    QCOMPARE(to.value("workspace/recent2").toString(), theirs);
    QVERIFY(!to.contains("workspace/recent3"));
    QCOMPARE(to.value("workspace/workingpath").toString(), m_dir->path());
}

void LegacySettings_Test::aFullRecentListTakesNoMore()
{
    QSettings from(m_from, QSettings::IniFormat);
    QSettings to(m_to, QSettings::IniFormat);
    for (int i = 0; i < LegacySettings::kMaxRecent; i++)
    {
        to.setValue(QString("workspace/recent%1").arg(i), file(QString("Mine%1.qlcc").arg(i)));
        from.setValue(QString("workspace/recent%1").arg(i), file(QString("Theirs%1.qxw").arg(i)));
    }
    QVERIFY(LegacySettings::plan(from, to).recentFiles.isEmpty());

    to.remove("workspace/recent9");
    QCOMPARE(LegacySettings::plan(from, to).recentFiles.size(), 1);
}

void LegacySettings_Test::preferencesAreCopiedOnlyWhereMissing()
{
    QSettings from(m_from, QSettings::IniFormat);
    QSettings to(m_to, QSettings::IniFormat);
    from.setValue("midiplugin/Input/nanoKONTROL2/mode", 1);
    from.setValue("inputmap/universe0/plugin", "MIDI");
    from.setValue("outputmap/universe0/plugin", "ArtNet");
    from.setValue("inputmanager/hotplug", true);
    from.setValue("ArtNetPlugin/ifacewait", 5);
    from.setValue("virtualconsole/buttonsize", QSize(60, 60));
    from.setValue("simpledesk/channelsperpage", 48);
    from.setValue("workspace/sliderstyle", 1);
    to.setValue("workspace/sliderstyle", 0);              // qlcconsole's own choice
    to.setValue("outputmap/universe0/plugin", "E1.31");   // ... and patch

    const LegacySettings::Plan plan = LegacySettings::plan(from, to);
    QCOMPARE(plan.ioKeys, QStringList() << "ArtNetPlugin/ifacewait" << "inputmanager/hotplug"
                                        << "inputmap/universe0/plugin"
                                        << "midiplugin/Input/nanoKONTROL2/mode");
    QCOMPARE(plan.uiKeys, QStringList() << "simpledesk/channelsperpage" << "virtualconsole/buttonsize");

    QCOMPARE(LegacySettings::apply(from, to, plan), 6);
    QCOMPARE(to.value("midiplugin/Input/nanoKONTROL2/mode").toInt(), 1);
    QCOMPARE(to.value("virtualconsole/buttonsize").toSize(), QSize(60, 60));
    QCOMPARE(to.value("workspace/sliderstyle").toInt(), 0);
    QCOMPARE(to.value("outputmap/universe0/plugin").toString(), QString("E1.31"));

    /* Done once, there is nothing left to bring */
    QVERIFY(LegacySettings::plan(from, to).isEmpty());
}

void LegacySettings_Test::windowLayoutsAndOtherKeysAreLeftBehind()
{
    QSettings from(m_from, QSettings::IniFormat);
    QSettings to(m_to, QSettings::IniFormat);
    from.setValue("workspace/geometry", QByteArray("geo"));
    from.setValue("workspace/state", QByteArray("state"));
    from.setValue("workspace/size", QSize(800, 600));
    from.setValue("fixturemanager/splitterstate", QByteArray("s"));
    from.setValue("configuremidiplugin/geometry", QByteArray("g"));
    from.setValue("monitor/geometry", QByteArray("g"));
    from.setValue("ui/language", "it_IT");                // the QML UI's
    from.setValue("virtualconsole/size", QSize(1920, 1080)); // the VC's default size: kept

    const LegacySettings::Plan plan = LegacySettings::plan(from, to);
    QVERIFY(plan.ioKeys.isEmpty());
    QCOMPARE(plan.uiKeys, QStringList() << "virtualconsole/size");
}

void LegacySettings_Test::qlcPlusIsNeverWritten()
{
    {
        QSettings from(m_from, QSettings::IniFormat);
        from.setValue("workspace/recent0", file("Theirs.qxw"));
        from.setValue("midiplugin/Input/X/mode", 2);
    }
    QFile before(m_from);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray bytes = before.readAll();
    before.close();

    {
        QSettings from(m_from, QSettings::IniFormat);
        QSettings to(m_to, QSettings::IniFormat);
        LegacySettings::apply(from, to, LegacySettings::plan(from, to));
        LegacySettings::markOffered(to);
    }
    QFile after(m_from);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), bytes);
}

void LegacySettings_Test::theOfferIsMadeOnce()
{
    QSettings to(m_to, QSettings::IniFormat);
    QVERIFY(!LegacySettings::wasOffered(to));
    LegacySettings::markOffered(to);
    QVERIFY(LegacySettings::wasOffered(to));
    QSettings again(m_to, QSettings::IniFormat);
    QVERIFY(LegacySettings::wasOffered(again));
}

void LegacySettings_Test::thePlanIsDescribedInAFewLines()
{
    LegacySettings::Plan plan;
    QVERIFY(plan.isEmpty());
    QVERIFY(LegacySettings::describe(plan).isEmpty());
    plan.recentFiles << "a" << "b";
    plan.ioKeys << "midiplugin/x";
    const QString text = LegacySettings::describe(plan);
    QCOMPARE(text.count('\n'), 1);
    QVERIFY(text.contains("2 recent show"));
    QVERIFY(text.contains("1 setting"));
}

QTEST_GUILESS_MAIN(LegacySettings_Test)
