/*
  Q Light Controller
  aboutbox_test.cpp

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

#include <QtTest>
#include <QLabel>

#include "qlcconfig.h"
#include "aboutbox_test.h"
#include "startupwindow.h"
#include "apputil.h"
#define private public
#include "aboutbox.h"
#undef protected

#define CONTRIBCOUNT 36

void AboutBox_Test::initial()
{
    QWidget w;
    AboutBox ab(&w);
    QCOMPARE(ab.parentWidget(), &w);

    QVERIFY(ab.m_titleLabel != NULL);
    QCOMPARE(ab.m_titleLabel->text(), QString(APPNAME));

    QVERIFY(ab.m_versionLabel != NULL);
    QCOMPARE(ab.m_versionLabel->text(), AboutBox::versionText());
    // The release first, then the build id and the commit it was built from.
    QVERIFY(ab.m_versionLabel->text().startsWith(QLCCONSOLE_VERSION));
    QVERIFY(ab.m_versionLabel->text().contains(APPVERSION));
    QVERIFY(ab.m_versionLabel->text().contains(QLCCONSOLE_GITREV));

    QVERIFY(ab.m_copyrightLabel != NULL);
    QVERIFY(ab.m_copyrightLabel->text().contains("by <B>Branson Matheson</B>"));

    QVERIFY(ab.m_websiteLabel != NULL);
    QVERIFY(ab.m_websiteLabel->text().contains("<A HREF=\"https://www.qlcplus.org/\">https://www.qlcplus.org/</A>"));

    QVERIFY(ab.m_contributors != NULL);
    QCOMPARE(ab.m_contributors->count(), CONTRIBCOUNT);

    QVERIFY(ab.m_timer != NULL);
    QCOMPARE(ab.m_row, -1);
    QCOMPARE(ab.m_increment, 1);
    QCOMPARE(ab.m_timer->interval(), 500);
    QCOMPARE(ab.m_timer->isActive(), true);
}

void AboutBox_Test::slotTimeout()
{
    AboutBox ab(NULL);
    ab.m_timer->stop();

    QCOMPARE(ab.m_row, -1);
    QCOMPARE(ab.m_increment, 1);

    for (int i = 0; i <= CONTRIBCOUNT; i++)
    {
        ab.slotTimeout();
        QCOMPARE(ab.m_row, i);
        QCOMPARE(ab.m_increment, 1);
    }

    for (int i = CONTRIBCOUNT - 1; i > 0; i--)
    {
        ab.slotTimeout();
        QCOMPARE(ab.m_row, i);
        QCOMPARE(ab.m_increment, -1);
    }
}

void AboutBox_Test::itemClick()
{
    AboutBox ab(NULL);
    QVERIFY(ab.m_timer != NULL);
    ab.slotItemClicked();
    QVERIFY(ab.m_timer == NULL);
    ab.slotItemClicked();
    QVERIFY(ab.m_timer == NULL);
}

/*
 * The window icon at every hicolor size, and the desktop entry name that lets
 * a Wayland compositor find the .desktop file (and so the dock icon) at all --
 * left to Qt, the app_id was "org.qlcplus.qlcconsole", which matched nothing.
 */
void AboutBox_Test::applicationIdentity()
{
    const QList<int> sizes = { 16, 22, 24, 32, 48, 64, 128, 256 };

    for (const QString &name : { QString("qlcconsole"), QString("qlcconsole-fixtureeditor") })
    {
        QIcon icon = AppUtil::applicationIcon(name);
        QVERIFY2(!icon.isNull(), qPrintable(name));
        QList<QSize> available = icon.availableSizes();
        QCOMPARE(available.size(), sizes.size());
        for (int size : sizes)
        {
            QVERIFY2(available.contains(QSize(size, size)),
                     qPrintable(QString("%1 has no %2px picture").arg(name).arg(size)));
            // Drawn at that size, not scaled from another one.
            QCOMPARE(icon.pixmap(QSize(size, size), 1.0).size(), QSize(size, size));
        }
    }
    QVERIFY(AppUtil::applicationIcon("no-such-app").isNull());

    AppUtil::setApplicationIdentity("qlcconsole");
    QCOMPARE(QGuiApplication::desktopFileName(), QString("qlcconsole"));
#if !defined(Q_OS_MACOS)
    // Every window inherits it, dialogs included, unless it sets its own.
    QWidget w;
    QCOMPARE(w.windowIcon().availableSizes().size(), sizes.size());
#endif
}

/* The boot window names the release, and below it the build and commit --
   the same as --version and the About box. */
void AboutBox_Test::startupWindowShowsTheVersion()
{
    StartupWindow sw(3);
    QLabel *release = sw.findChild<QLabel *>("startupVersion");
    QLabel *build = sw.findChild<QLabel *>("startupBuild");
    QVERIFY(release != nullptr && build != nullptr);
    QCOMPARE(release->text(), QString(APPNAME " " QLCCONSOLE_VERSION));
    QVERIFY2(build->text().contains(APPVERSION), qPrintable(build->text()));
    QVERIFY2(build->text().contains(QLCCONSOLE_GITREV), qPrintable(build->text()));
    QVERIFY(build->font().pointSizeF() < release->font().pointSizeF());

    sw.show();
    sw.beginStep("Loading fixtures");
    QVERIFY(release->isVisible() && build->isVisible());
    // Room for the whole line, not elided or clipped.
    QVERIFY(release->width() >= release->sizeHint().width());
    QVERIFY(build->width() >= build->sizeHint().width());

    const QString shot = qEnvironmentVariable("STARTUP_WINDOW_SHOT");
    if (!shot.isEmpty())
        QVERIFY(sw.grab().save(shot));
}

QTEST_MAIN(AboutBox_Test)
