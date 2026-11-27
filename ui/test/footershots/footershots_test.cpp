/*
  Q Light Controller Plus
  footericons_test.cpp

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

#include <QApplication>
#include <QElapsedTimer>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "doc.h"
#include "footershots_test.h"

namespace
{
void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}
}

void FooterShots_Test::theFooterAtThreeSizesInDesignAndOperate()
{
    App app;
    app.init();
    app.doc()->setModified();       // the Unsaved chip, as a working show has
    app.show();
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!dir.isEmpty())
        QDir().mkpath(dir);
    for (const Doc::Mode mode : { Doc::Design, Doc::Operate })
        for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
        {
            app.doc()->setMode(mode);
            app.resize(sz);
            settle(250);
            QStatusBar *sb = app.statusBar();
            QVERIFY(sb->isVisible());
            QVERIFY(sb->width() > 0 && sb->height() > 0);
            if (!dir.isEmpty())
            {
                const QString tag = QStringLiteral("%1-%2x%3").arg(mode == Doc::Design ? "design" : "operate")
                                        .arg(sz.width()).arg(sz.height());
                QVERIFY(sb->grab().save(QStringLiteral("%1/footer-%2.png").arg(dir, tag)));
                QVERIFY(app.grab().save(QStringLiteral("%1/window-%2.png").arg(dir, tag)));
            }
        }
    app.doc()->setMode(Doc::Design);
    app.doc()->resetModified();
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FooterShots_Test test;
    return QTest::qExec(&test, argc, argv);
}
