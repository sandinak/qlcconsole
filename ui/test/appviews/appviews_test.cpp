/*
  Q Light Controller Plus
  appviews_test.cpp

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
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTabBar>
#include <QDialog>

#include "appviews_test.h"

#define private public
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "doc.h"
#include "monitor.h"
#define protected public
#include "app.h"
#undef protected
#undef private

void AppViews_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
}

void AppViews_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

/* Put a detached view back where it came from, the same way closing its
   window does in the app. */
static void reattach(App *app, const QString &label)
{
    foreach (DetachedContext *dw, app->findChildren<DetachedContext *>())
    {
        QWidget *w = dw->centralWidget();
        if (w != NULL && w->property("tabLabel").toString() == label)
        {
            dw->close();
            return;
        }
    }
}

void AppViews_Test::jumpingToADockedViewSurvivesAnEarlierDetach()
{
    QTabWidget *tabs = m_app->m_tab;
    QVERIFY2(tabs->count() > 3, "need several tabs to shift one past another");

    const QString first = tabs->tabBar()->tabData(0).toString();
    const QString third = tabs->tabBar()->tabData(2).toString();
    QVERIFY(first.isEmpty() == false);
    QVERIFY(third.isEmpty() == false);
    QVERIFY(first != third);

    /* Detaching the FIRST tab pulls it out of the tab bar, so every view after
       it now sits one index lower than the order it was built in. A jump that
       remembers construction order lands on the wrong page from here on. */
    m_app->slotDetachContext(0);
    QCOMPARE(tabs->tabBar()->tabData(1).toString(), third);

    m_app->showContext(third);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), third);

    reattach(m_app, first);
}

void AppViews_Test::jumpingToADetachedViewRaisesItsWindow()
{
    QTabWidget *tabs = m_app->m_tab;
    QVERIFY(tabs->count() > 2);

    const QString label = tabs->tabBar()->tabData(1).toString();
    QVERIFY(label.isEmpty() == false);

    m_app->slotDetachContext(1);

    DetachedContext *dw = NULL;
    foreach (DetachedContext *w, m_app->findChildren<DetachedContext *>())
    {
        QWidget *c = w->centralWidget();
        if (c != NULL && c->property("tabLabel").toString() == label)
            dw = w;
    }
    QVERIFY2(dw != NULL, "detaching produced no window for that view");

    /* Buried behind something else. Picking the view from the menu has to
       bring it back -- with an index-based jump there was no tab left to
       select, so nothing happened at all. */
    dw->hide();
    QVERIFY(dw->isVisible() == false);

    m_app->showContext(label);
    QVERIFY2(dw->isVisible(),
             "picking an already-detached view did not bring its window forward");

    reattach(m_app, label);
}

void AppViews_Test::theWindowMenuFindsAndPutsBackBrokenOutViews()
{
    /* "the menubar needs a windows thing .. if I break out a tab it's hard to
       find": every view in a Window menu, marked when it is in its own window,
       and put back from there. */
    QMenu *windowMenu = m_app->menuBar()->findChild<QMenu *>("windowMenu");
    QVERIFY(windowMenu != nullptr);
    QTabWidget *tabs = m_app->m_tab;
    const int views = tabs->count();
    QCOMPARE(m_app->m_windowJumps.size(), views);

    const QString label = tabs->tabBar()->tabData(2).toString();
    m_app->slotDetachContext(2);
    QCOMPARE(m_app->detachedContexts(), QStringList() << label);
    m_app->updateWindowMenu();
    QAction *jump = nullptr;
    foreach (QAction *a, m_app->m_windowJumps)
        if (a->data().toString() == label)
            jump = a;
    QVERIFY(jump != nullptr);
    QVERIFY2(jump->text().contains("in its own window"), qPrintable(jump->text()));
    QVERIFY(m_app->m_windowDockMenu->isEnabled());
    QCOMPARE(m_app->m_windowDockMenu->actions().size(), 1);
    QCOMPARE(m_app->m_windowDockMenu->actions().first()->text(), label);

    // Put back from the menu: a tab again, and shown.
    m_app->m_windowDockMenu->actions().first()->trigger();
    QCOMPARE(tabs->count(), views);
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), label);
    m_app->updateWindowMenu();
    QVERIFY(!jump->text().contains("in its own window"));
    QVERIFY(!m_app->m_windowDockAll->isEnabled());

    // Two out, all put back at once.
    m_app->slotDetachContext(1);
    m_app->slotDetachContext(1);
    QCOMPARE(m_app->detachedContexts().size(), 2);
    m_app->reattachAllContexts();
    QVERIFY(m_app->detachedContexts().isEmpty());
    QCOMPARE(tabs->count(), views);
}

void AppViews_Test::theRigOverviewOpensWithoutTakingYouToTheStudio()
{
    QTabWidget *tabs = m_app->m_tab;
    // Somewhere that is not the Lighting Studio.
    int other = -1;
    for (int i = 0; i < tabs->count(); ++i)
        if (!tabs->tabBar()->tabData(i).toString().contains("Studio"))
            other = i;
    QVERIFY(other >= 0);
    tabs->setCurrentIndex(other);
    const QString was = tabs->tabBar()->tabData(other).toString();

    m_app->slotRigOverview();
    QCOMPARE(tabs->tabBar()->tabData(tabs->currentIndex()).toString(), was);
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr && mon->m_overviewDlg != nullptr);
    QVERIFY(mon->m_overviewDlg->isVisible());
    mon->m_overviewDlg->close();
}

void AppViews_Test::aShowsDefinitionsComeFromTheShowCache()
{
    QTemporaryDir home;
    const QByteArray realHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());

    // A definition that is only in the show cache.
    QLCFixtureDefCache *defs = m_app->m_doc->fixtureDefCache();
    QFile src(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/Futurelight/Futurelight-DJScan250.qxf");
    QVERIFY(src.open(QIODevice::ReadOnly));
    QString text = QString::fromUtf8(src.readAll());
    text.replace(QRegularExpression("<Manufacturer>[^<]*</Manufacturer>"), "<Manufacturer>Branson</Manufacturer>");
    text.replace(QRegularExpression("<Model>[^<]*</Model>"), "<Model>Cached Head</Model>");
    QFile cached(QLCFixtureDefCache::showCacheFile("Branson", "Cached Head"));
    QVERIFY(cached.open(QIODevice::WriteOnly));
    cached.write(text.toUtf8());
    cached.close();
    QVERIFY(defs->fixtureDef("Branson", "Cached Head") == nullptr);

    // A show that uses it.
    QTemporaryDir shows;
    const QString show = shows.filePath("cached.qlcc");
    QFile f(show);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("<?xml version=\"1.0\"?><!DOCTYPE Workspace><Workspace><Engine>"
            "<Fixture><Manufacturer>Branson</Manufacturer><Model>Cached Head</Model>"
            "<Mode>Mode 1</Mode><ID>0</ID><Name>Head</Name><Universe>0</Universe>"
            "<Address>0</Address><Channels>6</Channels></Fixture></Engine></Workspace>");
    f.close();

    m_app->prepareShowDefinitions(show);
    QVERIFY2(defs->fixtureDef("Branson", "Cached Head") != nullptr,
             "the show's definition did not come from the show cache");

    qputenv("HOME", realHome);
}

QTEST_MAIN(AppViews_Test)
