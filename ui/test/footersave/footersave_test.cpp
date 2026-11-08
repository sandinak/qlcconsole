/*
  Q Light Controller Plus
  footersave_test.cpp

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
#include <QStatusBar>
#include <QToolButton>
#include <QLabel>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "programmercontroller.h"
#undef protected
#undef private
#include "powerdistribution.h"
#include "programmingmanager.h"
#include "doc.h"
#include "footersave_test.h"

namespace
{

/* Where every footer chip is and whether the footer squeezed it: a save
   that changes any of it is the footer jumping. */
QStringList footer(App *app)
{
    QStringList out;
    QStatusBar *sb = app->statusBar();
    for (QWidget *w : sb->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    {
        if (!w->isVisibleTo(sb))
            continue;
        QString name = w->objectName();
        if (w == app->m_statusPowerLabel)
            name = QStringLiteral("power");
        else if (w == app->m_statusDirtyLabel)
            name = QStringLiteral("unsaved");
        else if (w == app->m_statusStandby)
            name = QStringLiteral("next cue");
        else if (w == app->m_statusProgrammerLabel)
            name = QStringLiteral("programmer");
        else if (name.isEmpty())
            name = QString::fromLatin1(w->metaObject()->className());
        out << QStringLiteral("%1 at %2 width %3%4").arg(name).arg(w->x()).arg(w->width())
               .arg(w->property("statusSqueezed").toBool() ? QStringLiteral(" (squeezed)") : QString());
    }
    return out;
}

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}

}

void FooterSave_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    QString src = qEnvironmentVariable("FOOTER_SHOW");
    if (src.isEmpty())
        src = QStringLiteral(FOOTERSAVE_SHOW);
    const QString show = m_dir.filePath(QStringLiteral("show.") + QFileInfo(src).suffix());
    QVERIFY(QFile::copy(src, show));
    QFile::setPermissions(show, QFile::ReadOwner | QFile::WriteOwner);
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    m_app->resize(1280, 800);
    m_app->show();
    settle(300);
}

void FooterSave_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

void FooterSave_Test::savingLeavesTheFooterWhereItWas()
{
    sweepSaves();
}

void FooterSave_Test::savingWithEditsInTheProgrammerLeavesTheFooterWhereItWas()
{
    /* A scene edited through the programmer: its pre-edit state survives the
       save, and the chip turns from the red line to the longer grey one. */
    ProgrammerController *pc = m_app->doc()->programmer();
    QVERIFY(pc != nullptr);
    pc->m_editedScenes.insert(1);
    auto tidy = qScopeGuard([&]() {
        pc->m_editedScenes.clear();
        m_app->slotProgrammerDirtyChanged(false);
    });
    m_app->slotProgrammerDirtyChanged(true);
    QVERIFY(m_app->m_statusProgrammerLabel->isVisibleTo(m_app->statusBar()));
    sweepSaves();
    QVERIFY(m_app->m_statusProgrammerLabel->text().contains(QStringLiteral("saved, revertable")));
}

void FooterSave_Test::sweepSaves()
{
    // The power chip on (a show with its power set up), a cue list's Next.
    PowerDistribution *power = m_app->doc()->powerDistribution();
    if (power->sources().isEmpty())
        power->sources().append(PowerSource());
    m_app->m_showFooterPower = true;
    emit ProgrammingManager::instance()->powerEstimateChanged(12.5, 1.4, false, QString());
    settle(600);        // the estimate's own first reading
    QVERIFY(m_app->m_statusPowerLabel->isVisibleTo(m_app->statusBar()));

    // Wide to narrow, through where the footer squeezes one chip after another.
    for (int width = 1500; width >= 850; width -= 25)
    {
        m_app->resize(width, 800);
        settle(30);
        m_app->doc()->setModified();
        settle(30);
        const QStringList unsaved = footer(m_app);
        // Settled: fitting the footer again changes nothing (a chip squeezed
        // at one fit and back at the next is the footer flickering).
        m_app->fitStatusBar();
        settle(30);
        QCOMPARE(footer(m_app), unsaved);
        QCOMPARE(m_app->m_statusDirtyLabel->text(), QString::fromUtf8("● Unsaved changes"));

        QCOMPARE(m_app->saveXML(m_app->fileName()), QFile::NoError);   // what File ▸ Save writes
        settle(30);
        QVERIFY(!m_app->doc()->isModified());
        QCOMPARE(m_app->m_statusDirtyLabel->text(), QString::fromUtf8("✓ Saved"));
        const QStringList saved = footer(m_app);
        if (saved != unsaved)
        {
            QStringList moved;
            for (int i = 0; i < qMax(saved.size(), unsaved.size()); ++i)
                if (saved.value(i) != unsaved.value(i))
                    moved << unsaved.value(i) + QStringLiteral(" -> ") + saved.value(i);
            QFAIL(qPrintable(QStringLiteral("window %1 wide, saving moved the footer: %2")
                             .arg(width).arg(moved.join(QStringLiteral("; ")))));
        }
    }
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FooterSave_Test test;
    return QTest::qExec(&test, argc, argv);
}
