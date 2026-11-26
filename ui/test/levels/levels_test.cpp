/*
  Q Light Controller Plus
  levels_test.cpp

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

#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include "levels_test.h"
#include "levelregistry.h"
#include "showlevel.h"
#include "showoperate.h"
#include "teachingsettings.h"
#include "qlcfixturedefcache.h"

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "doc.h"
#include "app.h"
#include "virtualconsole.h"
#include "showsettingsdialog.h"
#undef private
#undef protected

namespace
{
void settle()
{
    for (int i = 0; i < 4; ++i)
    {
        QCoreApplication::processEvents();
        QTest::qWait(10);
    }
}

/* The menu bar's top-level count: each menu's own items shown, a submenu
   as one, separators not counted (WORKSPACE_LEVELS §3). */
int menuCount(App *app, QStringList *detail = nullptr)
{
    int total = 0;
    for (QAction *top : app->menuBar()->actions())
    {
        QMenu *m = top->menu();
        if (m == nullptr || !top->isVisible())
            continue;
        int n = 0;
        for (QAction *a : m->actions())
            if (!a->isSeparator() && a->isVisible())
                ++n;
        total += n;
        if (detail != nullptr)
            *detail << QStringLiteral("%1 %2").arg(m->title().remove(QLatin1Char('&'))).arg(n);
    }
    return total;
}


/* Every leaf item, counted one by one: the menu bar's menus with their
   submenus opened out, plus the Virtual Console's Add menu (it is not on the
   bar, but it is a menu of the product). Separators and hidden items do not
   count; a submenu is not itself an item. The menu count is held per level
   on this, not only on the top-level entries, because the level's own items
   sit inside View > Workspace where menuCount() does not see them. */
int leafItems(const QMenu *menu)
{
    int n = 0;
    for (QAction *a : menu->actions())
    {
        if (a->isSeparator() || !a->isVisible())
            continue;
        n += a->menu() != nullptr ? leafItems(a->menu()) : 1;
    }
    return n;
}

int itemCount(App *app)
{
    int total = 0;
    for (QAction *top : app->menuBar()->actions())
        if (top->menu() != nullptr && top->isVisible())
            total += leafItems(top->menu());
    if (VirtualConsole::instance() != nullptr && VirtualConsole::instance()->addMenu() != nullptr)
        total += leafItems(VirtualConsole::instance()->addMenu());
    return total;
}

/* The <Workspace> attribute @p name in @p path, and whether it is there. */
QString workspaceAttr(const QString &path, const QString &name, bool *present = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QStringLiteral("<unreadable>");
    QXmlStreamReader xml(&f);
    while (!xml.atEnd())
    {
        if (xml.readNext() == QXmlStreamReader::StartElement && xml.name() == QLatin1String("Workspace"))
        {
            if (present != nullptr)
                *present = xml.attributes().hasAttribute(name);
            return xml.attributes().value(name).toString();
        }
    }
    return QStringLiteral("<no workspace>");
}

QStringList barTabs(App *app)
{
    QStringList out;
    for (int i = 0; i < app->m_tab->count(); ++i)
        out << app->m_tab->tabBar()->tabData(i).toString();
    return out;
}

QList<QMenu *> topMenus(App *app)
{
    QList<QMenu *> out;
    for (QAction *top : app->menuBar()->actions())
        if (top->menu() != nullptr)
            out << top->menu();
    return out;
}
}

void Levels_Test::initTestCase()
{
    static QTemporaryDir home;
    QVERIFY(home.isValid());
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("USERPROFILE", home.path().toLocal8Bit());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    ShowSettingsDialog::setTestMode(true);
    m_app = new App();
    m_app->init();
    m_app->resize(1280, 800);
    m_app->showNormal();
    settle();
    /* A fresh launch with no show (train 053n review): Standard ticked in
       View ▸ Workspace, the status bar's safety controls never hidden. */
    int ticked = 0;
    for (QAction *a : m_app->m_levelActions)
        if (a->isChecked())
        {
            ++ticked;
            QCOMPARE(a->data().toInt(), int(ShowLevel::Standard));
        }
    QCOMPARE(ticked, 1);
    QVERIFY(ShowLevel::instance()->isNeverHidden(m_app->m_statusGrandMasterBox));
    QVERIFY(ShowLevel::instance()->isNeverHidden(m_app->m_statusModeLabel));
}

void Levels_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void Levels_Test::init()
{
    // A new show for each case: Standard, qlcconsole, no Teaching, no PIN.
    m_app->doc()->resetModified();
    m_app->clearDocument();
    m_app->doc()->teachingSettings()->setEnabled(false);
    m_app->doc()->operateSettings()->setPin(QString());
    ShowSettingsDialog::testAnswers().clear();
    m_app->hideViewNotice();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
}

void Levels_Test::theLevelRoundTripsAndQlc4IgnoresIt()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("levels.qxw"));

    // Standard writes nothing: an untouched show's file does not change.
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    bool present = true;
    workspaceAttr(path, QStringLiteral("Level"), &present);
    QVERIFY(!present);
    QCOMPARE(workspaceAttr(path, QStringLiteral("View")), QStringLiteral("qlcconsole"));

    // Student is saved, beside the view, and read back.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("Level")), QStringLiteral("student"));
    m_app->clearDocument();
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Standard);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);

    // Advanced too.
    QVERIFY(m_app->requestLevel(ShowLevel::Advanced));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("Level")), QStringLiteral("advanced"));

    /* A newer build's level: opens at Standard (logged), and is written
       back as it was (the FeaturesOff rule: never fail, never drop). */
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray xml = f.readAll();
    f.close();
    xml.replace("Level=\"advanced\"", "Level=\"expert\"");
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml);
    f.close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown level"));
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Standard);
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("Level")), QStringLiteral("expert"));
    // Picking Standard there means it: the unknown value is dropped (train
    // 053n review: setLevel() returned early and wrote "expert" back).
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    present = true;
    workspaceAttr(path, QStringLiteral("Level"), &present);
    QVERIFY(!present);
    m_app->doc()->resetModified();
}

void Levels_Test::anOldShowOpensAtStandard()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("old.qxw"));
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    // As QLC+ 4 (or a build before levels) writes it: no Level attribute.
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray xml = f.readAll();
    f.close();
    xml.replace(" Level=\"student\"", "");
    QVERIFY(!xml.contains("Level=\""));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml);
    f.close();

    QStringList standard;
    m_app->clearDocument();
    const int full = menuCount(m_app, &standard);
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QVERIFY(menuCount(m_app) < full);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Standard);
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    QStringList after;
    QCOMPARE(menuCount(m_app, &after), full);
    QCOMPARE(after, standard);
    QVERIFY(barTabs(m_app).contains(QStringLiteral("Shows")));
    QVERIFY(m_app->m_statusLevelLabel->isHidden());
    m_app->doc()->resetModified();
}

/* Train 053n review: turning Teaching off kept the PIN-unlocked session
   level on screen while Save wrote the old saved one. */
void Levels_Test::turningTeachingOffKeepsTheLevelOnScreen()
{
    m_app->doc()->teachingSettings()->setEnabled(true);
    QVERIFY(m_app->doc()->operateSettings()->setPin(QStringLiteral("2468")));
    ShowLevel::instance()->setLevel(ShowLevel::Student);
    ShowSettingsDialog::testAnswers() = { QStringLiteral("2468") };
    QVERIFY(m_app->requestLevel(ShowLevel::Advanced));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Advanced);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);

    ShowSettingsDialog::testAnswers() = { QStringLiteral("2468") };     // the PIN to turn it off
    {
        ShowSettingsDialog dlg(m_app->doc(), m_app);
        dlg.m_teaching->setChecked(false);
        dlg.accept();
    }
    QVERIFY(!m_app->doc()->teachingSettings()->enabled());
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Advanced);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("off.qxw"));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("Level")), QStringLiteral("advanced"));
    ShowSettingsDialog::testAnswers().clear();
    m_app->doc()->operateSettings()->setPin(QString());
    m_app->clearDocument();
    m_app->doc()->resetModified();
}

/* Train 053n review: a tab the level hides stayed live in a window of its
   own (the Window menu naming it "More Tabs at Standard…"). */
void Levels_Test::aLevelHiddenTabLeavesItsWindow()
{
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    settle();
    const int idx = barTabs(m_app).indexOf(QStringLiteral("Shows"));
    QVERIFY2(idx >= 0, qPrintable(barTabs(m_app).join(", ")));
    QWidget *shows = m_app->m_tab->widget(idx);
    m_app->slotDetachContext(idx);
    settle();
    auto showsWindow = [this, shows]() -> QWidget * {
        for (DetachedContext *dw : m_app->findChildren<DetachedContext *>())
            if (dw->centralWidget() == shows && dw->isVisible())
                return dw;
        return nullptr;
    };
    QVERIFY(showsWindow() != nullptr);
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Shows")));

    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    settle();
    QVERIFY2(showsWindow() == nullptr, "Shows (the Timeline) still open in its own window at Student");
    QVERIFY(!shows->isVisible());
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Shows")));

    // Back at Standard it is a tab on the bar again.
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    settle();
    QVERIFY(showsWindow() == nullptr);
    QVERIFY(barTabs(m_app).contains(QStringLiteral("Shows")));
    m_app->doc()->resetModified();
}

void Levels_Test::aTeachingShowsSavedLevelSurvivesGoingDown()
{
    // A Level 3 handout: Teaching, saved at Standard, with a PIN.
    m_app->doc()->teachingSettings()->setEnabled(true);
    QVERIFY(m_app->doc()->operateSettings()->setPin(QStringLiteral("2468")));
    ShowLevel::instance()->setLevel(ShowLevel::Standard);
    m_app->doc()->resetModified();

    // The student steps down: for this session only, nothing to save.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Standard);
    QVERIFY(!m_app->doc()->isModified());
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Shows")));

    // And back up to the saved level without the PIN (no answer queued:
    // a prompt would read as Cancel).
    ShowSettingsDialog::testAnswers().clear();
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    QVERIFY(!ShowLevel::instance()->hasSessionLevel());
    QVERIFY(!ShowLevel::instance()->unlocked());

    // Above it still asks.
    QVERIFY(!m_app->requestLevel(ShowLevel::Advanced));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);

    // Saved while down, the copy reopens at Standard.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("handout.qxw"));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    bool present = true;
    workspaceAttr(path, QStringLiteral("Level"), &present);
    QVERIFY(!present);
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(m_app->doc()->teachingSettings()->enabled());
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    m_app->doc()->resetModified();
}

void Levels_Test::teachingSetsStudentAndThePinUnlocksForTheSession()
{
    TeachingSettings *teach = m_app->doc()->teachingSettings();
    ShowOperate *op = m_app->doc()->operateSettings();

    // Turning Teaching on (Show Settings) sets the Student level (Q341).
    {
        ShowSettingsDialog dlg(m_app->doc(), m_app);
        dlg.m_teaching->setChecked(true);
        dlg.accept();
    }
    QVERIFY(teach->enabled());
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);
    QVERIFY(m_app->m_statusLevelLabel->isVisibleTo(m_app->statusBar()));
    QCOMPARE(m_app->m_statusLevelLabel->text(), QStringLiteral("STUDENT"));
    QVERIFY(op->setPin(QStringLiteral("2468")));

    // Going up asks the PIN: a wrong one changes nothing.
    ShowSettingsDialog::testAnswers() = { QStringLiteral("1111") };
    QVERIFY(!m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Shows")));
    // Cancel neither.
    ShowSettingsDialog::testAnswers().clear();
    QVERIFY(!m_app->requestLevel(ShowLevel::Advanced));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);

    // The PIN: Standard for this session; the saved level stays Student.
    ShowSettingsDialog::testAnswers() = { QStringLiteral("2468") };
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);
    QVERIFY(ShowLevel::instance()->unlocked());
    QVERIFY(barTabs(m_app).contains(QStringLiteral("Shows")));
    QVERIFY(ShowSettingsDialog::testAnswers().isEmpty());

    // Asked once: Advanced, down and up again without asking.
    QVERIFY(m_app->requestLevel(ShowLevel::Advanced));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Advanced);
    QVERIFY(m_app->requestLevel(ShowLevel::Student));     // going down never asks
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);

    // Never saved: the file says Student, and it reopens locked (Q342).
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("class.qxw"));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("Level")), QStringLiteral("student"));
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QVERIFY(m_app->doc()->teachingSettings()->enabled());
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    QVERIFY(!ShowLevel::instance()->unlocked());
    ShowSettingsDialog::testAnswers().clear();
    QVERIFY(!m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);

    // The Teaching chip offers the unlock, rather than a new menu item.
    ShowSettingsDialog::testAnswers() = { QStringLiteral("2468") };
    QTimer::singleShot(0, m_app, []() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (QMenu *m = qobject_cast<QMenu *>(w))
                if (m->objectName() == QLatin1String("teachingMenu"))
                {
                    QAction *unlock = m->findChild<QAction *>(QStringLiteral("teachingUnlockStandard"));
                    QVERIFY(unlock != nullptr);
                    unlock->trigger();
                    m->close();
                }
    });
    m_app->showTeachingMenu();
    settle();
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);

    // With no PIN, Teaching still resets to Student but asks nothing.
    m_app->clearDocument();
    {
        ShowSettingsDialog dlg(m_app->doc(), m_app);
        dlg.m_teaching->setChecked(true);
        dlg.accept();
    }
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    ShowSettingsDialog::testAnswers().clear();
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);
    m_app->doc()->resetModified();
}

void Levels_Test::theHandoutCarriesItsLevel()
{
    QTemporaryDir dir;
    HandoutOptions options;
    QCOMPARE(options.level, int(ShowLevel::Student));       // Student by default
    options.path = dir.filePath(QStringLiteral("Lesson.qlcc"));
    QString why;
    QVERIFY2(m_app->writeStudentHandout(options, &why), qPrintable(why));
    QCOMPARE(workspaceAttr(options.path, QStringLiteral("Level")), QStringLiteral("student"));
    // The open show is left as it is.
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Standard);
    QCOMPARE(m_app->m_levelSaveOverride, -1);

    // A Level 3 handout at Standard: nothing written, so it opens at Standard.
    options.path = dir.filePath(QStringLiteral("Lesson3.qlcc"));
    options.level = ShowLevel::Standard;
    QVERIFY2(m_app->writeStudentHandout(options, &why), qPrintable(why));
    bool present = true;
    workspaceAttr(options.path, QStringLiteral("Level"), &present);
    QVERIFY(!present);

    // Opened, it is a Teaching show at its level.
    options.path = dir.filePath(QStringLiteral("Lesson1.qlcc"));
    options.level = ShowLevel::Student;
    QVERIFY2(m_app->writeStudentHandout(options, &why), qPrintable(why));
    QCOMPARE(m_app->loadXML(options.path), QFile::NoError);
    QVERIFY(m_app->doc()->teachingSettings()->enabled());
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    m_app->doc()->resetModified();
}

void Levels_Test::theMenuCountFallsOrStaysFlat()
{
    /* Origin/main 63dafbfa4 had 70 top-level entries in the qlcconsole view.
       Standard pays for the levels (Clear Hold at End and Shift-Click a Tab
       left their menus); Student falls by a quarter. Raising either number
       means changing this test, with the reason in the commit. */
    QStringList standard, student, advanced;
    /* Item by item (submenus opened, the VC's Add menu in): the same three
       levels. kBefore is origin/main 63dafbfa4's count; the level adds its
       three radio items inside View > Workspace, and Standard pays them
       back (Clear Hold at End, Shift-Click a Tab, the VC's Programmer Frame
       wizard), so it holds flat. */
    const int kBefore = 9999;
    const int itemsAtStandard = itemCount(m_app);
    QVERIFY2(itemsAtStandard <= kBefore, qPrintable(QString::number(itemsAtStandard)));
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    const int itemsAtStudent = itemCount(m_app);
    QVERIFY2(itemsAtStudent < itemsAtStandard, qPrintable(QString::number(itemsAtStudent)));
    QVERIFY(m_app->requestLevel(ShowLevel::Advanced));
    QFAIL(qPrintable(QStringLiteral("ITEMS %1 %2 %3").arg(itemsAtStandard).arg(itemsAtStudent).arg(itemCount(m_app))));
    QCOMPARE(itemCount(m_app), itemsAtStandard);
    QVERIFY(m_app->requestLevel(ShowLevel::Standard));

    const int atStandard = menuCount(m_app, &standard);
    QVERIFY2(atStandard <= 68, qPrintable(standard.join(QStringLiteral(", "))));
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    const int atStudent = menuCount(m_app, &student);
    QVERIFY2(atStudent <= 52, qPrintable(student.join(QStringLiteral(", "))));
    QVERIFY(m_app->requestLevel(ShowLevel::Advanced));
    // Advanced adds only inside Settings and Help ▸ Troubleshooting.
    QCOMPARE(menuCount(m_app, &advanced), atStandard);
    QCOMPARE(advanced, standard);
    // The level items themselves are inside View ▸ Workspace, not on top.
    QMenu *workspace = m_app->menuBar()->findChild<QMenu *>(QStringLiteral("viewWorkspaceMenu"));
    QVERIFY(workspace != nullptr);
    for (const char *name : { "viewLevelStudent", "viewLevelStandard", "viewLevelAdvanced" })
    {
        QAction *a = workspace->findChild<QAction *>(QString::fromLatin1(name));
        QVERIFY2(a != nullptr && workspace->actions().contains(a), name);
    }
    m_app->doc()->resetModified();
}

void Levels_Test::aStudentShowSavedOnFunctionsOpensQuietly()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("student.qxw"));
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QString xml = QString::fromUtf8(f.readAll());
    f.close();
    // The show was last saved with the Functions tab in front (a Standard session).
    xml.replace(QRegularExpression(QStringLiteral(" CurrentWindow=\"[^\"]*\"")), QString());
    xml.replace(QStringLiteral("<Workspace "),
                QStringLiteral("<Workspace CurrentWindow=\"FunctionManager\" "));
    QVERIFY(xml.contains(QStringLiteral("CurrentWindow=\"FunctionManager\"")));
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(xml.toUtf8());
    f.close();

    m_app->clearDocument();
    m_app->hideViewNotice();
    m_app->statusBar()->clearMessage();
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);
    settle();

    // Quiet: no notice line, no Switch button; the Functions tab is not on screen.
    QVERIFY(!m_app->m_viewNotice->isVisibleTo(m_app));
    QVERIFY(!m_app->m_viewNoticeButton->isVisibleTo(m_app));
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Functions")));
    // The footer says why, in one line.
    const QString said = m_app->statusBar()->currentMessage();
    QVERIFY2(said.contains(QStringLiteral("Functions")) && said.contains(QStringLiteral("Standard")),
             qPrintable(said));
    m_app->doc()->resetModified();
}

void Levels_Test::aHiddenTabIsInTheWindowMenuSwitchLine()
{
    QAction *more = m_app->m_windowMoreTabs;
    QVERIFY(more != nullptr);
    QVERIFY(!more->isVisible());
    QVERIFY(barTabs(m_app).contains(QStringLiteral("Shows")));

    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    const QStringList tabs = barTabs(m_app);
    for (const QString &t : { QStringLiteral("Build"), QStringLiteral("Functions"),
                              QStringLiteral("Simple Desk"), QStringLiteral("Shows") })
        QVERIFY2(!tabs.contains(t), qPrintable(t));
    for (const QString &t : { QStringLiteral("Connections"), QStringLiteral("Fixtures"),
                              QStringLiteral("Lighting Studio"), QStringLiteral("Show Tree"),
                              QStringLiteral("Virtual Console"), QStringLiteral("Run") })
        QVERIFY2(tabs.contains(t), qPrintable(t));

    // One line in the Window menu, naming them; their jumps are gone.
    QVERIFY(more->isVisible());
    QCOMPARE(more->text(), QString::fromUtf8("More Tabs at Standard…"));
    for (QAction *jump : m_app->m_windowJumps)
    {
        const QString label = jump->data().toString();
        QCOMPARE(jump->isVisible(), !m_app->levelHiddenTabs().contains(label));
    }
    for (const QString &t : { QStringLiteral("Build"), QStringLiteral("Simple Desk"), QStringLiteral("Timeline") })
        QVERIFY2(more->toolTip().contains(t), qPrintable(more->toolTip()));

    // It says so in the notice line, with the switch, and the switch works.
    more->trigger();
    QVERIFY(m_app->m_viewNotice->isVisibleTo(m_app));
    QVERIFY(m_app->m_viewNoticeText->text().contains(QStringLiteral("Simple Desk")));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Switch to Standard"));
    m_app->m_viewNoticeButton->click();
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    QVERIFY(barTabs(m_app).contains(QStringLiteral("Shows")));
    QVERIFY(!more->isVisible());

    // A path to a hidden tab (a link, a button) is the same notice, never a
    // silent raise.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    m_app->hideViewNotice();
    m_app->showContext(QStringLiteral("Simple Desk"));
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Simple Desk")));
    QVERIFY(m_app->m_viewNotice->isVisibleTo(m_app));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Switch to Standard"));
    m_app->showFunctions();
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QVERIFY(!barTabs(m_app).contains(QStringLiteral("Functions")));
    m_app->doc()->resetModified();
}

void Levels_Test::safetyControlsAreNeverHidden()
{
    const QList<QAction *> safety = {
        m_app->m_controlPanicAction, m_app->m_controlBlackoutAction, m_app->m_controlBlindAction,
        m_app->m_cueGoAction, m_app->m_cueBackAction, m_app->m_cueSubGoAction,
        m_app->m_cueSubBackAction, m_app->m_modeToggleAction };
    for (QAction *a : safety)
        QVERIFY(a != nullptr);
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    // bind() refuses them, whatever key it is asked for.
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("never hidden"));
    ShowLevel::instance()->bind(m_app->m_controlPanicAction, QStringLiteral("file.import"));
    ShowLevel::instance()->apply();
    for (QAction *a : safety)
    {
        QVERIFY2(a->isVisible(), qPrintable(a->text()));
        QVERIFY2(!a->property("levelKey").isValid(), qPrintable(a->text()));
        QVERIFY2(ShowLevel::instance()->isNeverHidden(a), qPrintable(a->text()));
    }
    QVERIFY(m_app->m_controlPanicAction->isEnabled());
    // The Grand Master and the readiness footer: never bound, never hidden.
    QVERIFY(ShowLevel::instance()->isNeverHidden(m_app->m_statusGrandMasterBox));
    QVERIFY(ShowLevel::instance()->isNeverHidden(m_app->m_statusModeLabel));
    QVERIFY(!m_app->m_statusModeLabel->property("levelFolded").toBool());
    // In the Control menu at Student: the mode, Blackout, Stop All and the GO keys.
    QMenu *control = m_app->menuBar()->findChild<QMenu *>(QStringLiteral("controlMenu"));
    QVERIFY(control != nullptr);
    for (QAction *a : safety)
        if (a != m_app->m_controlBlindAction)
            QVERIFY2(control->actions().contains(a), qPrintable(a->text()));
    m_app->doc()->resetModified();
}

void Levels_Test::everyLevelHiddenItemHasASwitchPath()
{
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    int folded = 0;
    QStringList menusWithFolds;
    for (QMenu *menu : topMenus(m_app))
    {
        QList<QAction *> mine;
        for (QAction *a : menu->actions())
            if (a->property("levelFolded").toBool())
                mine << a;
        if (mine.isEmpty())
            continue;
        folded += mine.size();
        const QString title = menu->title().remove(QLatin1Char('&'));
        menusWithFolds << title;
        // Its More line, last, in its own band.
        QAction *more = ShowLevel::instance()->moreLine(menu);
        QVERIFY2(more != nullptr && more->isVisible(), qPrintable(title));
        QCOMPARE(more->text(), QString::fromUtf8("More at Standard…"));
        QCOMPARE(menu->actions().last(), more);
        const int at = menu->actions().indexOf(more);
        // The band above it ends with a separator (or the menu is the line).
        bool bandOk = true;
        for (int i = at - 1; i >= 0; --i)
        {
            QAction *a = menu->actions().at(i);
            if (!a->isVisible())
                continue;
            bandOk = a->isSeparator();
            break;
        }
        QVERIFY2(bandOk, qPrintable(title + ": the More line is not in its own band"));
        // It names each folded item, and offers the switch.
        m_app->hideViewNotice();
        more->trigger();
        QVERIFY2(m_app->m_viewNotice->isVisibleTo(m_app), qPrintable(title));
        for (QAction *a : mine)
        {
            const QString t = a->text().remove(QLatin1Char('&'));
            QVERIFY2(m_app->m_viewNoticeText->text().contains(t),
                     qPrintable(title + ": " + t + " not named in: " + m_app->m_viewNoticeText->text()));
        }
        QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Switch to Standard"));
        // Switch, and they are back.
        m_app->m_viewNoticeButton->click();
        QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
        for (QAction *a : mine)
            QVERIFY2(a->isVisible(), qPrintable(title + ": " + a->text()));
        QVERIFY(!more->isVisible());
        QVERIFY(m_app->requestLevel(ShowLevel::Student));
    }
    QVERIFY2(folded >= 12, qPrintable(QString::number(folded)));
    for (const QString &t : { QStringLiteral("File"), QStringLiteral("View"), QStringLiteral("Fixtures"),
                              QStringLiteral("Control") })
        QVERIFY2(menusWithFolds.contains(t), qPrintable(menusWithFolds.join(QStringLiteral(", "))));

    // And a Teaching show's Switch asks the PIN first.
    m_app->doc()->teachingSettings()->setEnabled(true);
    QVERIFY(m_app->doc()->operateSettings()->setPin(QStringLiteral("1357")));
    m_app->offerLevelSwitch(ShowLevel::Standard, QStringLiteral("test"));
    ShowSettingsDialog::testAnswers() = { QStringLiteral("0000") };
    m_app->m_viewNoticeButton->click();
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Student);
    m_app->offerLevelSwitch(ShowLevel::Standard, QStringLiteral("test"));
    ShowSettingsDialog::testAnswers() = { QStringLiteral("1357") };
    m_app->m_viewNoticeButton->click();
    QCOMPARE(ShowLevel::instance()->chosenLevel(), ShowLevel::Standard);
    m_app->doc()->resetModified();
}

void Levels_Test::theRegistryIsWhole()
{
    // Every key bound has a row.
    for (const QString &key : ShowLevel::instance()->boundKeys())
        QVERIFY2(LevelRegistry::row(key) != nullptr, qPrintable(key));

    // Every row is bound somewhere in ui/src (a tab row names a real tab),
    // but the follow-ups (Q343's settings still to be built).
    QString all;
    QDirIterator it(QStringLiteral(UI_SRC_DIR), { QStringLiteral("*.cpp") }, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();
        if (path.endsWith(QLatin1String("levelregistry.cpp")))
            continue;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            all += QString::fromUtf8(f.readAll());
    }
    QVERIFY(all.size() > 100000);
    QStringList keys;
    for (const LevelRegistry::Row &r : LevelRegistry::rows())
    {
        QVERIFY2(!keys.contains(r.key), qPrintable(QStringLiteral("two rows for ") + r.key));
        keys << r.key;
        QVERIFY2(!r.words.isEmpty(), qPrintable(r.key));
        if (r.followUp)
            continue;
        if (r.treatment == ShowLevel::Tab)
        {
            QVERIFY2(App::tabKeys().contains(r.tab), qPrintable(r.key + ": " + r.tab));
            continue;
        }
        QVERIFY2(all.contains(QStringLiteral("\"%1\"").arg(r.key)),
                 qPrintable(r.key + " has a row but nothing binds it"));
    }
    // Nothing Student-level in the table: Student is everything Levels 0-2
    // use (Q340), and the table only lists what is above it.
    for (const LevelRegistry::Row &r : LevelRegistry::rows())
        QVERIFY2(r.level > ShowLevel::Student, qPrintable(r.key));
    // Standard stays today's surface but for the script editor (Q343).
    for (const LevelRegistry::Row &r : LevelRegistry::rows())
        if (r.level == ShowLevel::Advanced && !r.followUp)
            QCOMPARE(r.key, QStringLiteral("effects.script"));
}

void Levels_Test::classicActsAsAdvancedAndStudentLeavesIt()
{
    QStringList standard;
    const int full = menuCount(m_app, &standard);
    m_app->chooseWorkspaceView(App::ClassicView);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    QVERIFY(!ShowLevel::instance()->applies());
    QCOMPARE(ShowLevel::instance()->effectiveLevel(), ShowLevel::Advanced);

    // Student, picked in Classic: the qlcconsole view, with the way back.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(ShowLevel::instance()->effectiveLevel(), ShowLevel::Student);
    QVERIFY(m_app->m_viewNotice->isVisibleTo(m_app));
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Back to Classic QLC+"));
    QVERIFY(menuCount(m_app) < full);
    m_app->m_viewNoticeButton->click();
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    // Classic shows QLC+'s tabs in full: nothing is folded there.
    for (QMenu *menu : topMenus(m_app))
        for (QAction *a : menu->actions())
            QVERIFY2(!a->property("levelFolded").toBool(), qPrintable(a->text()));
    QVERIFY(m_app->m_statusLevelLabel->isHidden());
    QCOMPARE(ShowLevel::instance()->level(), ShowLevel::Student);

    // Saved like that, it reopens in the qlcconsole view, at Student.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("classic.qxw"));
    QCOMPARE(m_app->saveXML(path), QFile::NoError);
    QCOMPARE(workspaceAttr(path, QStringLiteral("View")), QStringLiteral("classic"));
    QCOMPARE(m_app->loadXML(path), QFile::NoError);
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    QCOMPARE(ShowLevel::instance()->effectiveLevel(), ShowLevel::Student);
    QCOMPARE(m_app->m_viewNoticeButton->text(), QStringLiteral("Back to Classic QLC+"));

    // In a Teaching show, Back to Classic QLC+ asks the PIN.
    m_app->doc()->teachingSettings()->setEnabled(true);
    QVERIFY(m_app->doc()->operateSettings()->setPin(QStringLiteral("8642")));
    ShowSettingsDialog::testAnswers() = { QStringLiteral("0000") };
    m_app->chooseWorkspaceView(App::ClassicView);
    QCOMPARE(m_app->workspaceView(), App::ConsoleView);
    ShowSettingsDialog::testAnswers() = { QStringLiteral("8642") };
    m_app->chooseWorkspaceView(App::ClassicView);
    QCOMPARE(m_app->workspaceView(), App::ClassicView);
    m_app->doc()->resetModified();
}

void Levels_Test::levelShots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    QCOMPARE(m_app->loadXML(QStringLiteral(COURSE_DIR "/shows/lesson-2-5-start.qlcc")), QFile::NoError);
    m_app->doc()->resetModified();
    const QSize was = m_app->size();
    const struct { ShowLevel::Level level; const char *name; } levels[] = {
        { ShowLevel::Student, "student" }, { ShowLevel::Standard, "standard" }, { ShowLevel::Advanced, "advanced" } };
    for (const auto &l : levels)
    {
        QVERIFY(m_app->requestLevel(l.level));
        m_app->showContext(QStringLiteral("Show Tree"));
        for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            m_app->resize(size);
            settle();
            QTest::qWait(60);
            settle();
            QVERIFY(m_app->grab().save(QStringLiteral("%1/level-%2-%3x%4.png")
                                       .arg(dir, QString::fromLatin1(l.name)).arg(size.width()).arg(size.height())));
        }
        // The menus as each level has them.
        for (QMenu *menu : topMenus(m_app))
        {
            const QString title = menu->title().remove(QLatin1Char('&')).toLower();
            if (title != QLatin1String("file") && title != QLatin1String("control")
                && title != QLatin1String("window") && title != QLatin1String("fixtures")
                && title != QLatin1String("view"))
                continue;
            menu->adjustSize();
            menu->ensurePolished();
            QVERIFY(menu->grab().save(QStringLiteral("%1/level-%2-menu-%3.png")
                                      .arg(dir, QString::fromLatin1(l.name), title)));
        }
    }
    // The notice a More line raises, at Student, and a Teaching show's footer.
    QVERIFY(m_app->requestLevel(ShowLevel::Student));
    m_app->resize(1440, 900);
    settle();
    QMenu *file = m_app->menuBar()->findChild<QMenu *>(QStringLiteral("fileMenu"));
    QVERIFY(file != nullptr);
    ShowLevel::instance()->moreLine(file)->trigger();
    settle();
    QVERIFY(m_app->grab().save(QStringLiteral("%1/level-student-notice-1440x900.png").arg(dir)));
    m_app->doc()->teachingSettings()->setEnabled(true);
    for (const QSize &size : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
    {
        m_app->resize(size);
        settle();
        QTest::qWait(60);
        settle();
        QVERIFY(m_app->grab().save(QStringLiteral("%1/level-student-teaching-%2x%3.png")
                                   .arg(dir).arg(size.width()).arg(size.height())));
    }
    m_app->doc()->teachingSettings()->setEnabled(false);
    m_app->resize(was);
    m_app->doc()->resetModified();
}

QTEST_MAIN(Levels_Test)
