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
#include <QTabWidget>
#include <QTabBar>

#include "appviews_test.h"

#define private public
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

QTEST_MAIN(AppViews_Test)
