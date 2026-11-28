/*
  Q Light Controller Plus - qlcconsole
  footerbar_test.cpp

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

#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QtTest>

#include "footerbar.h"
#include "footerbar_test.h"

namespace
{
struct Window
{
    QMainWindow mw;
    FooterBar *bar = nullptr;
    QLabel *normal = nullptr;
    QLabel *permanent = nullptr;

    Window()
    {
        bar = new FooterBar(&mw);
        mw.setStatusBar(bar);
        mw.setCentralWidget(new QWidget(&mw));
        normal = new QLabel(QStringLiteral("Show Status: what time is it?"), bar);
        normal->setObjectName(QStringLiteral("normalChip"));
        bar->addWidget(normal);
        permanent = new QLabel(QStringLiteral("Operate chip words"), bar);
        permanent->setObjectName(QStringLiteral("permanentChip"));
        bar->addPermanentWidget(permanent);
        mw.resize(900, 300);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw));
    }

    QList<QLabel *> shownMessages() const
    {
        QList<QLabel *> out;
        for (QLabel *l : bar->findChildren<QLabel *>(QStringLiteral("footerMessage")))
            if (l->isVisible())
                out << l;
        return out;
    }
};

QRect inBar(QWidget *w, QWidget *bar)
{
    return QRect(w->mapTo(bar, QPoint(0, 0)), w->size());
}
}

void FooterBar_Test::twoMessagesShowOneAtATime()
{
    Window w;
    w.bar->showMessage(QStringLiteral("Design mode. The footer does not go live: use Control ▸ Operate (⌘F12)."), 6000);
    w.bar->showMessage(QStringLiteral("FaderPort: Shift+Stop stops."), 4000);
    QCoreApplication::processEvents();
    const QList<QLabel *> shown = w.shownMessages();
    QCOMPARE(shown.size(), 1);
    QCOMPARE(shown.first()->text(), QStringLiteral("FaderPort: Shift+Stop stops."));
    // No other label shows words where the message is.
    for (QLabel *l : w.bar->findChildren<QLabel *>())
        if (l != shown.first() && l->isVisible())
            QVERIFY2(!inBar(l, w.bar).intersects(inBar(shown.first(), w.bar)),
                     qPrintable(l->objectName() + QStringLiteral(" overlaps the message")));
    w.bar->clearMessage();
    QVERIFY(w.shownMessages().isEmpty());
    QVERIFY(w.normal->isVisible());
}

void FooterBar_Test::theMessageIsNeverUnderAPermanentChip()
{
    Window w;
    QCoreApplication::processEvents();
    const QRect chip = inBar(w.permanent, w.bar);
    const QImage before = w.bar->grab().toImage();
    w.bar->showMessage(QString(400, QLatin1Char('W')));
    QCoreApplication::processEvents();
    const QList<QLabel *> shown = w.shownMessages();
    QCOMPARE(shown.size(), 1);
    QVERIFY(!inBar(shown.first(), w.bar).intersects(chip));
    // The permanent chip's pixels are what they were: no message under its words.
    const QImage after = w.bar->grab().toImage();
    const qreal dpr = after.devicePixelRatio();
    const QRect px(QPoint(int(chip.left() * dpr), int(chip.top() * dpr)),
                   QPoint(int((chip.right() + 1) * dpr) - 1, int((chip.bottom() + 1) * dpr) - 1));
    QCOMPARE(after.copy(px), before.copy(px));
}

void FooterBar_Test::aChipShownDuringAMessageIsNotDrawnOverIt()
{
    Window w;
    w.bar->showMessage(QStringLiteral("FaderPort: Shift+Stop stops."));
    QCoreApplication::processEvents();
    QVERIFY(!w.normal->isVisible());        // QStatusBar hides it for the message
    w.normal->show();                       // a chip's own update shows it again
    QCoreApplication::processEvents();
    QLabel *msg = w.shownMessages().value(0);
    QVERIFY(msg != nullptr);
    const QPoint inChip = w.normal->mapTo(w.bar, w.normal->rect().center());
    QVERIFY(msg->geometry().contains(inChip));
    QCOMPARE(w.bar->childAt(inChip), static_cast<QWidget *>(msg));
}

void FooterBar_Test::aLongMessageIsElided()
{
    Window w;
    const QString longText = QStringLiteral("A very long footer message ") + QString(300, QLatin1Char('x'));
    w.bar->showMessage(longText);
    QCoreApplication::processEvents();
    QLabel *msg = w.shownMessages().value(0);
    QVERIFY(msg != nullptr);
    QVERIFY(msg->text() != longText);
    QVERIFY(msg->text().endsWith(QChar(0x2026)));
    QCOMPARE(msg->toolTip(), longText);
}

QTEST_MAIN(FooterBar_Test)
