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
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#undef protected
#undef private
#include "deskchip.h"
#include "doc.h"
#include "footericons.h"
#include "showlevel.h"
#include "footericons_test.h"

namespace
{

void settle(int ms = 60)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}

QStringList hiddenSetting()
{
    QSettings s;
    s.sync();
    return s.value(QStringLiteral("footer/hidden")).toStringList();
}

}

void FooterIcons_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    m_app->resize(1440, 900);
    m_app->show();
    settle(200);
}

void FooterIcons_Test::cleanup()
{
    m_app->resetFooter();
    m_app->doc()->resetModified();
    m_app->resize(1440, 900);
    settle(30);
}

void FooterIcons_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

void FooterIcons_Test::everyChipHasAnIconAndAName()
{
    QVERIFY(FooterIcons::entries().size() >= 25);
    QStringList bad;
    for (const FooterIcons::Entry &e : FooterIcons::entries())
    {
        QWidget *chip = m_app->footerChip(e.id);
        if (chip == nullptr)
        {
            // Only the venue chip waits on the programmer; the rest are always built.
            bad << QStringLiteral("%1: no chip in the footer").arg(e.id);
            continue;
        }
        QVERIFY(!e.label.isEmpty());
        if (e.id == QLatin1String("surfaces"))
        {
            // The board chips carry the controller glyph themselves.
            QVERIFY(!FooterIcons::icon(e.glyph, Qt::black).isNull());
            continue;
        }
        FooterBadge *badge = m_app->footerBadge(e.id);
        if (badge == nullptr)
        {
            bad << QStringLiteral("%1: no icon").arg(e.id);
            continue;
        }
        const QImage img = badge->pixmap().toImage();
        bool ink = false;
        for (int y = 0; y < img.height() && !ink; ++y)
            for (int x = 0; x < img.width(); ++x)
                if (qAlpha(img.pixel(x, y)) > 0)
                {
                    ink = true;
                    break;
                }
        if (!ink)
            bad << QStringLiteral("%1: the icon is blank").arg(e.id);
        if (badge->accessibleName().isEmpty() || badge->toolTip().isEmpty())
            bad << QStringLiteral("%1: no accessible name / tooltip").arg(e.id);
        if (m_app->statusBar() != badge->parentWidget())
            bad << QStringLiteral("%1: the icon is not in the footer").arg(e.id);
    }
    QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QStringLiteral("; "))));
    // A glyph for each: no two kinds of chip share a drawing by accident.
    QSet<QByteArray> drawn;
    for (int g = FooterIcons::Ready; g <= FooterIcons::Disk; ++g)
    {
        const QImage img = FooterIcons::icon(FooterIcons::Glyph(g), Qt::black).pixmap(QSize(14, 14)).toImage();
        QByteArray bytes(reinterpret_cast<const char *>(img.constBits()), img.sizeInBytes());
        QVERIFY2(!drawn.contains(bytes), qPrintable(QStringLiteral("glyph %1 repeats another").arg(g)));
        drawn.insert(bytes);
    }
}

void FooterIcons_Test::viewFooterHidesAndShowsAChip()
{
    QMenu *menu = m_app->footerMenu();
    QVERIFY(menu != nullptr);
    QCOMPARE(menu->title(), QStringLiteral("Footer"));
    // Its items are made as it opens.
    QVERIFY(menu->actions().isEmpty());
    emit menu->aboutToShow();
    int checkable = 0;
    for (QAction *a : menu->actions())
        if (a->isCheckable())
        {
            ++checkable;
            QVERIFY2(!a->icon().isNull(), qPrintable(a->text()));
        }
    QCOMPARE(checkable, int(FooterIcons::entries().size()));
    QVERIFY(menu->findChild<QAction *>(QStringLiteral("footerReset")) != nullptr);

    // The Saved chip: always on show, so a hide is plain to see.
    QWidget *saved = m_app->footerChip(QStringLiteral("saved"));
    FooterBadge *badge = m_app->footerBadge(QStringLiteral("saved"));
    QVERIFY(saved != nullptr && badge != nullptr);
    m_app->fitFooterNow();
    QVERIFY(saved->width() > 0);
    QVERIFY(badge->isVisibleTo(m_app->statusBar()));
    QAction *act = menu->findChild<QAction *>(QStringLiteral("footerShow_saved"));
    QVERIFY(act != nullptr);
    QVERIFY(act->isChecked() && act->isEnabled());

    act->setChecked(false);           // as a click on the item does
    settle();
    m_app->fitFooterNow();
    QVERIFY(!m_app->footerShown(QStringLiteral("saved")));
    QCOMPARE(saved->width(), 0);
    QVERIFY(!badge->isVisibleTo(m_app->statusBar()));
    QVERIFY(hiddenSetting().contains(QStringLiteral("saved")));
    // The menu says so the next time it opens.
    emit menu->aboutToShow();
    QVERIFY(!menu->findChild<QAction *>(QStringLiteral("footerShow_saved"))->isChecked());

    m_app->setFooterShown(QStringLiteral("saved"), true);
    settle();
    m_app->fitFooterNow();
    QVERIFY(saved->width() > 0);
    QVERIFY(badge->isVisibleTo(m_app->statusBar()));
    QVERIFY(!hiddenSetting().contains(QStringLiteral("saved")));

    // One of the four optional chips keeps its own setting, and the menu is one with it.
    m_app->setFooterShown(QStringLiteral("load"), true);
    QVERIFY(m_app->footerChipShown(App::FooterLoad));
    m_app->setFooterShown(QStringLiteral("load"), false);
    QVERIFY(!m_app->footerChipShown(App::FooterLoad));
    QVERIFY(m_app->m_statusLoadLabel->isHidden());
    QVERIFY(!m_app->footerBadge(QStringLiteral("load"))->isVisibleTo(m_app->statusBar()));
}

void FooterIcons_Test::resetFooterShowsEverything()
{
    m_app->setFooterShown(QStringLiteral("saved"), false);
    m_app->setFooterShown(QStringLiteral("stage"), false);
    m_app->setFooterShown(QStringLiteral("gm"), false);
    m_app->setFooterShown(QStringLiteral("load"), true);
    QVERIFY(!hiddenSetting().isEmpty());
    QMenu *menu = m_app->footerMenu();
    emit menu->aboutToShow();
    menu->findChild<QAction *>(QStringLiteral("footerReset"))->trigger();
    settle();
    m_app->fitFooterNow();
    QVERIFY(hiddenSetting().isEmpty());
    QVERIFY(m_app->footerShown(QStringLiteral("saved")));
    QVERIFY(m_app->footerShown(QStringLiteral("stage")));
    QVERIFY(m_app->footerShown(QStringLiteral("gm")));
    QVERIFY(!m_app->footerShown(QStringLiteral("load")));       // as it ships: off
    QVERIFY(m_app->footerChip(QStringLiteral("saved"))->width() > 0);
}

void FooterIcons_Test::safetyChipsCantBeHidden()
{
    QStringList locked;
    for (const FooterIcons::Entry &e : FooterIcons::entries())
        if (e.locked)
            locked << e.id;
    for (const char *must : { "readiness", "hazard", "editlive", "pad", "overrides", "test", "desk", "student" })
        QVERIFY2(locked.contains(QLatin1String(must)), must);
    QMenu *menu = m_app->footerMenu();
    emit menu->aboutToShow();
    for (const QString &id : locked)
    {
        QVERIFY2(!m_app->setFooterShown(id, false), qPrintable(id));
        QVERIFY2(m_app->footerShown(id), qPrintable(id));
        QAction *a = menu->findChild<QAction *>(QStringLiteral("footerShow_%1").arg(id));
        QVERIFY2(a != nullptr, qPrintable(id));
        QVERIFY2(a->isChecked() && !a->isEnabled(), qPrintable(id));
        QVERIFY2(a->toolTip().contains(QStringLiteral("Always shown")), qPrintable(id));
        QVERIFY2(!hiddenSetting().contains(id), qPrintable(id));
    }
    // The readiness chip is on screen however it is asked.
    m_app->setFooterShown(QStringLiteral("readiness"), false);
    m_app->fitFooterNow();
    QVERIFY(m_app->m_statusModeLabel->width() > 0);
}

void FooterIcons_Test::theRemovedChipsAreGone()
{
    QVERIFY(m_app->m_statusModeChipLabel == nullptr);
    QVERIFY(m_app->statusBar()->findChild<QWidget *>(QStringLiteral("footerMode")) == nullptr);
    // The level: only Student is said.
    for (const ShowLevel::Level l : { ShowLevel::Standard, ShowLevel::Advanced, ShowLevel::Student })
    {
        ShowLevel::instance()->setLevel(l);
        m_app->updateLevelChip();
        if (l == ShowLevel::Student)
        {
            QVERIFY(m_app->m_statusLevelLabel->isVisibleTo(m_app->statusBar()));
            QCOMPARE(m_app->m_statusLevelLabel->text(), QStringLiteral("STUDENT"));
        }
        else
            QVERIFY2(m_app->m_statusLevelLabel->isHidden(), qPrintable(ShowLevel::name(l)));
    }
    ShowLevel::instance()->setLevel(ShowLevel::Standard);
    m_app->updateLevelChip();
    // The desk: hidden while it is fine, red and Patch It when it is not patched.
    DeskChip *desk = m_app->deskChip();
    QVERIFY(desk != nullptr);
    QVERIFY(desk->isHidden() || desk->colour() != DeskChip::Green);
    QVERIFY(m_app->footerBadge(QStringLiteral("desk")) != nullptr);
    QVERIFY(!m_app->footerBadge(QStringLiteral("desk"))->isVisibleTo(m_app->statusBar())
            || !desk->isHidden());
}

void FooterIcons_Test::aSqueezedChipKeepsItsIconBeforeItGoes()
{
    m_app->doc()->setModified();
    m_app->m_statusSelectionLabel->setText(QStringLiteral("Selected: Lanterns, Washes, Movers, Specials (24)"));
    m_app->m_statusSelectionLabel->show();
    m_app->m_statusFollowLabel->setText(QStringLiteral("Follow spot: Lanterns from SL tower"));
    m_app->m_statusFollowLabel->show();
    int iconOnly = 0;
    for (const int w : { 1920, 1280, 1024, m_app->minimumSizeHint().width() })
    {
        m_app->resize(w, 700);
        settle(100);
        m_app->fitFooterNow();
        settle(60);
        QStatusBar *sb = m_app->statusBar();
        for (FooterBadge *b : m_app->m_footerBadges)
        {
            QWidget *chip = b->chip();
            if (chip->isHidden())
            {
                QVERIFY2(!b->isVisibleTo(sb), qPrintable(b->id() + ": an icon with no chip"));
                continue;
            }
            if (chip->width() > 0)
                QVERIFY2(b->isVisibleTo(sb), qPrintable(QStringLiteral("%1 at %2: a chip without its icon").arg(b->id()).arg(w)));
            else if (b->isVisibleTo(sb))
                ++iconOnly;                    // squeezed to its icon
            if (b->isVisibleTo(sb))
                QVERIFY2(QRect(QPoint(0, 0), sb->size()).contains(b->geometry()),
                         qPrintable(QStringLiteral("%1's icon is outside the footer at %2").arg(b->id()).arg(w)));
        }
    }
    qInfo() << "icon-only chips seen:" << iconOnly;
    m_app->resize(1440, 900);
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FooterIcons_Test test;
    return QTest::qExec(&test, argc, argv);
}
