/*
  Q Light Controller Plus - qlcconsole
  monkey_test.cpp

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
#include <QApplication>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QPushButton>
#include <QToolButton>
#include <QAbstractButton>
#include <QLineEdit>
#include <QComboBox>
#include <QListView>
#include <QTreeView>
#include <QDialog>
#include <QTimer>
#include <QWidget>
#include <QTemporaryDir>

#include "monkey_test.h"
#define protected public
#define private public
#include "app.h"
#undef protected
#undef private
#include "doc.h"

/* Dismiss any modal dialog that pops up, so the monkey never deadlocks on an
   exec() waiting for a human. Installed as a repeating timer. */
namespace
{
    void dismissModals()
    {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *w : widgets)
        {
            QDialog *d = qobject_cast<QDialog *>(w);
            if (d != nullptr && d->isVisible())
                d->reject();
        }
    }

    QList<QWidget *> liveTargets(QWidget *root)
    {
        QList<QWidget *> out;
        if (root == nullptr)
            return out;
        const auto all = root->findChildren<QWidget *>();
        for (QWidget *w : all)
        {
            if (!w->isVisible() || !w->isEnabled())
                continue;
            if (qobject_cast<QAbstractButton *>(w) || qobject_cast<QLineEdit *>(w)
                || qobject_cast<QComboBox *>(w) || qobject_cast<QTabBar *>(w)
                || qobject_cast<QAbstractItemView *>(w))
                out << w;
        }
        return out;
    }
}

void Monkey_Test::initTestCase()
{
    /* A HOME of our own: the app reads user fixtures/profiles/scripts from it;
       never touch the real user's folders. */
    static QTemporaryDir home;
    QVERIFY(home.isValid());
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("USERPROFILE", home.path().toLocal8Bit());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    m_app = new App();
    m_app->init();
    m_app->resize(1280, 800);
}

void Monkey_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void Monkey_Test::aSeededMonkeyNeverCrashes()
{
    const quint32 seed = qEnvironmentVariableIsSet("QLC_MONKEY_SEED")
                             ? quint32(qEnvironmentVariableIntValue("QLC_MONKEY_SEED"))
                             : 20261027u;
    int budgetMs = qEnvironmentVariableIntValue("QLC_MONKEY_MS");
    if (budgetMs <= 0)
        budgetMs = 60000;

    QRandomGenerator rng(seed);

    /* A timer that keeps dismissing any modal the monkey opens. */
    QTimer modalKiller;
    connect(&modalKiller, &QTimer::timeout, [] { dismissModals(); });
    modalKiller.start(50);

    QTabWidget *tabs = m_app->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);

    QElapsedTimer clock;
    clock.start();
    int actions = 0;
    const QList<Qt::Key> keys = {
        Qt::Key_Space, Qt::Key_Return, Qt::Key_Escape, Qt::Key_Delete,
        Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down,
        Qt::Key_Tab, Qt::Key_A, Qt::Key_1, Qt::Key_Z,
    };

    while (clock.elapsed() < budgetMs)
    {
        const int choice = int(rng.bounded(100));

        if (choice < 25 && tabs->count() > 0)
        {
            tabs->setCurrentIndex(int(rng.bounded(tabs->count())));
        }
        else
        {
            QWidget *page = tabs->currentWidget();
            const QList<QWidget *> targets = liveTargets(page);
            if (!targets.isEmpty())
            {
                QWidget *w = targets.at(int(rng.bounded(targets.size())));
                if (choice < 60)
                {
                    const QPoint p(int(rng.bounded(qMax(1, w->width()))),
                                   int(rng.bounded(qMax(1, w->height()))));
                    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, p);
                }
                else if (choice < 80)
                {
                    QTest::keyClick(w, keys.at(int(rng.bounded(keys.size()))));
                }
                else if (auto *le = qobject_cast<QLineEdit *>(w))
                {
                    /* Hostile text: empty, very long, control and RTL chars. */
                    static const QStringList evil = {
                        QString(), QStringLiteral("=cmd|' /C calc'!A1"),
                        QString(4000, QChar('x')),
                        QString::fromUtf8("\xE2\x80\xAE""abc\xE2\x80\x8B\xF0\x9F\x98\x80"),
                        QStringLiteral("../../etc/passwd"),
                    };
                    le->setText(evil.at(int(rng.bounded(evil.size()))));
                }
                else if (auto *cb = qobject_cast<QComboBox *>(w))
                {
                    if (cb->count() > 0)
                        cb->setCurrentIndex(int(rng.bounded(cb->count())));
                }
            }
        }

        /* Occasionally fire Undo/Redo storms through the app's shortcuts. */
        if (choice >= 95)
        {
            QTest::keySequence(m_app, QKeySequence::Undo);
            QTest::keySequence(m_app, QKeySequence::Redo);
        }

        if ((actions++ % 32) == 0)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    modalKiller.stop();
    dismissModals();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    /* The point is that we reached here: no crash, assert or hang in
       budgetMs of random input. The document must still be usable. */
    QVERIFY(m_app->doc() != nullptr);
    qInfo() << "monkey: seed" << seed << "actions" << actions
            << "in" << clock.elapsed() << "ms";
}

QTEST_MAIN(Monkey_Test)
