/*
  Q Light Controller Plus - Unit test
  editliveui_test.cpp

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
#include <QMainWindow>
#include <QToolButton>
#include <QStatusBar>
#include <QSettings>
#include <QPainter>
#include <QAction>
#include <QMenu>
#include <QDir>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private

#include "editliveui_test.h"
#include "../../../engine/test/common/resource_paths.h"
#include "simmididevice.h"
#include "showtreecontroller.h"
#include "qlcfixturedefcache.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "ioplugincache.h"
#include "editlivechip.h"
#include "surfacepages.h"
#include "fixturegroup.h"
#include "showoperate.h"
#include "showcontrol.h"
#include "deskrecord.h"
#include "controlmap.h"
#include "cuerunner.h"
#include "qlcpalette.h"
#include "editlive.h"
#include "qlcfile.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

namespace
{
const QString kApc = QStringLiteral("Akai APC40 mkII");

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

Target groupTarget(quint32 g)
{
    Target t;
    t.groups << g;
    return t;
}
}

void EditLiveUi_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
}

void EditLiveUi_Test::init()
{
    QSettings().remove(DeskRecord::settingsGroup());
    QSettings().remove(SurfacePages::settingsGroup());
    m_doc = new Doc(this);
    QDir fx(INTERNAL_FIXTUREDIR);
    fx.setFilter(QDir::Files);
    fx.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    QVERIFY(m_doc->fixtureDefCache()->loadMap(fx));
    QDir dir(QStringLiteral("../../../engine/test/iopluginstub"));
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtPlugin));
    m_doc->ioPluginCache()->load(dir);
    QVERIFY2(!m_doc->ioPluginCache()->plugins().isEmpty(), "no I/O plugin stub");
    m_stub = static_cast<IOPluginStub *>(m_doc->ioPluginCache()->plugins().at(0));
    m_stub->m_feedback.clear();
    m_stub->m_feedbackParams.clear();
    m_stub->m_feedbackLines.clear();
    m_stub->m_lineNames.clear();
    m_apc = new SimMidiDevice(m_doc, m_stub, QStringLiteral("APC40 mkII"), 0, kApc,
                              QStringLiteral(QLC_TEST_RESOURCE_ROOT) + QStringLiteral("inputprofiles/Akai-APC40-mkII.qxi"));
    QVERIFY2(m_apc->plug(), qPrintable(m_apc->lastError()));
    QVERIFY(show());
    m_doc->controlMap()->editLive()->setSelectionProbe([this]() { return m_pars; });
}

void EditLiveUi_Test::cleanup()
{
    EditLiveChip::setPinAsker(nullptr);
    for (Function *f : m_doc->functions())
        f->stop(FunctionParent::master());
    tick(3);
    delete m_apc;
    m_apc = nullptr;
    delete m_doc;
    m_doc = nullptr;
    m_chaser = nullptr;
    m_pars.clear();
}

/* Four RGB pars: cue 1 red at full, cue 2 blue. */
bool EditLiveUi_Test::show()
{
    QLCFixtureDef *par = m_doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    if (par == nullptr)
        return false;
    FixtureGroup *g = new FixtureGroup(m_doc);
    g->setName(QStringLiteral("Front wash"));
    m_doc->addFixtureGroup(g);
    for (int i = 0; i < 4; ++i)
    {
        Fixture *f = new Fixture(m_doc);
        f->setName(QStringLiteral("Front %1").arg(i + 1));
        f->setFixtureDefinition(par, par->mode("RGB Dimmer"));
        f->setAddress(quint32(i * 4));
        m_doc->addFixture(f);
        g->assignFixture(f->id());
        m_pars << f->id();
    }
    auto palette = [&](QLCPalette::PaletteType type, const QString &name, const QVariantList &values) {
        QLCPalette *p = new QLCPalette(type, m_doc);
        p->setName(name);
        p->setValues(values);
        m_doc->addPalette(p);
        return p->id();
    };
    const quint32 red = palette(QLCPalette::Color, "Red", { QLCPalette::colorToString(Qt::red, Qt::black) });
    const quint32 blue = palette(QLCPalette::Color, "Blue", { QLCPalette::colorToString(Qt::blue, Qt::black) });
    const quint32 full = palette(QLCPalette::Dimmer, "Full", { 255 });
    ShowTreeController *st = m_doc->showTree();
    const quint32 prod = st->addProduction("Edit Live");
    const quint32 list = st->addCueList(prod, "Main");
    m_c1 = st->addCue(list, -1, "Open");
    st->setTargetLook(m_c1, groupTarget(g->id()), st->lookForPalette(red));
    st->setTargetLook(m_c1, groupTarget(g->id()), st->lookForPalette(full));
    m_c2 = st->addCue(list, 0, "Blue");
    st->setTargetLook(m_c2, groupTarget(g->id()), st->lookForPalette(blue));
    st->setCurrentProduction(prod);
    m_chaser = qobject_cast<Chaser *>(m_doc->function(st->cueList(list)->chaser));
    return m_chaser != nullptr;
}

void EditLiveUi_Test::tick(int n)
{
    for (int i = 0; i < n; ++i)
    {
        m_doc->masterTimer()->timerTick();
        QList<Universe *> ua = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : ua)
            u->processFaders();
        m_doc->inputOutputMap()->releaseUniverses(false);
    }
}

void EditLiveUi_Test::go()
{
    ShowControl *sc = m_doc->showControl();
    sc->setMainCueList(m_chaser->id());
    ShowControl::Request r;
    r.command = ShowControl::Go;
    r.chaser = m_chaser->id();
    sc->execute(r);
    tick(10);
    settle();
    m_doc->controlMap()->editLive()->checkCue();
    settle();
}

void EditLiveUi_Test::trimRed()
{
    // Fader 1 is red: up at the red on stage (picked up), then half.
    QVERIFY(m_apc->set(QStringLiteral("#7"), 127));
    QVERIFY(m_apc->set(QStringLiteral("#7"), 64));
    settle();
}

void EditLiveUi_Test::thePinIsAsked()
{
    m_doc->setMode(Doc::Operate);
    EditLive *el = m_doc->controlMap()->editLive();
    int asked = 0;
    bool give = false;
    EditLiveChip::setPinAsker([&](QWidget *, const QString &) {
        ++asked;
        return give;
    });
    // No PIN: anyone at this computer.
    QVERIFY(EditLiveChip::turnOn(m_doc, nullptr));
    QCOMPARE(asked, 0);
    QVERIFY(el->isOn());
    el->setOn(false);

    // A PIN: asked, and refused without it.
    QVERIFY(m_doc->operateSettings()->setPin(QStringLiteral("1234")));
    QString why;
    QVERIFY(!EditLiveChip::turnOn(m_doc, nullptr, &why));
    QCOMPARE(asked, 1);
    QVERIFY(!el->isOn());
    QVERIFY(!why.isEmpty());
    // The chip's click asks too.
    EditLiveChip chip(m_doc);
    chip.chip()->click();
    QCOMPARE(asked, 2);
    QVERIFY(!el->isOn());
    give = true;
    chip.chip()->click();
    QCOMPARE(asked, 3);
    QVERIFY(el->isOn());
}

void EditLiveUi_Test::theRunTabSwitchTurnsItOn()
{
    CueRunner run(m_doc, CueRunner::Full);
    QToolButton *b = run.editLiveButton();
    QVERIFY(b != nullptr);
    QCOMPARE(b->text(), QStringLiteral("Edit Live"));
    // Design: Operate only, and the Design header keeps its room.
    QVERIFY(b->isHidden());
    m_doc->setMode(Doc::Operate);
    settle();
    QVERIFY(!b->isHidden());
    b->click();
    EditLive *el = m_doc->controlMap()->editLive();
    QVERIFY(el->isOn());
    QVERIFY(b->isChecked());
    b->click();
    QVERIFY(!el->isOn());
    // Turned off elsewhere (the footer), the switch follows.
    QVERIFY(el->setOn(true));
    QVERIFY(b->isChecked());
    m_doc->setMode(Doc::Design);
    QVERIFY(!el->isOn());
    QVERIFY(!b->isChecked());
    QVERIFY(b->isHidden());
}

void EditLiveUi_Test::theChipIsRedAndListsTheTrims()
{
    EditLiveChip chip(m_doc);
    // Design: hidden.
    QVERIFY(chip.isHidden());
    m_doc->setMode(Doc::Operate);
    go();
    chip.refresh();
    QVERIFY(!chip.isHidden());
    QCOMPARE(chip.chip()->text(), QStringLiteral("Edit Live"));
    QVERIFY(!chip.chip()->styleSheet().contains(QStringLiteral("#c62828")));
    chip.chip()->click();
    EditLive *el = m_doc->controlMap()->editLive();
    QVERIFY(el->isOn());
    QCOMPARE(chip.chip()->text(), QStringLiteral("EDIT LIVE"));
    QVERIFY(chip.chip()->styleSheet().contains(QStringLiteral("#c62828")));

    trimRed();
    QVERIFY(el->hasTrims());
    QCOMPARE(chip.chip()->text(), QStringLiteral("EDIT LIVE · 4 Trims"));
    QScopedPointer<QMenu> menu(chip.buildMenu());
    QStringList lines;
    QAction *store = nullptr, *drop = nullptr;
    for (QAction *a : menu->actions())
    {
        lines << a->text();
        if (a->objectName() == QLatin1String("editLiveStore"))
            store = a;
        if (a->objectName() == QLatin1String("editLiveDrop"))
            drop = a;
    }
    QVERIFY2(lines.contains(QStringLiteral("Front 1 · Red · 100% → 50%")), qPrintable(lines.join(" | ")));
    QVERIFY(store != nullptr && store->isEnabled());
    QVERIFY(store->text().startsWith(QStringLiteral("Store to Cue ")));
    QVERIFY(drop != nullptr && drop->isEnabled());
    store->trigger();
    QVERIFY(!el->hasTrims());
    QCOMPARE(chip.chip()->text(), QStringLiteral("EDIT LIVE"));
    QVERIFY(el->lastNote().contains(QStringLiteral("Colour")));
}

void EditLiveUi_Test::afterGoTheChipOffersStoreToCue()
{
    EditLiveChip chip(m_doc);
    m_doc->setMode(Doc::Operate);
    go();
    EditLive *el = m_doc->controlMap()->editLive();
    QVERIFY(el->setOn(true));
    trimRed();
    QVERIFY(el->hasTrims());
    QVERIFY(chip.storeLeftButton()->isHidden());
    go();
    QVERIFY(!el->hasTrims());
    QVERIFY(!chip.storeLeftButton()->isHidden());
    QVERIFY(chip.storeLeftButton()->text().startsWith(QStringLiteral("Store to Cue ")));
    QVERIFY(chip.storeLeftButton()->text().contains(QStringLiteral("Open")));
    chip.storeLeftButton()->click();
    QVERIFY(chip.storeLeftButton()->isHidden());
    QVERIFY(!el->hasLeftBehind());
}

void EditLiveUi_Test::shots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        QSKIP("QLC_SHOT_DIR not set");
    QDir().mkpath(dir);
    const QString prefix = qEnvironmentVariable("QLC_SHOT_PREFIX", QStringLiteral("after"));
    m_doc->setMode(Doc::Operate);
    go();
    EditLive *el = m_doc->controlMap()->editLive();

    const QList<QSize> sizes = { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) };
    for (const QSize &size : sizes)
    {
        QMainWindow win;
        win.setCentralWidget(new QWidget(&win));
        EditLiveChip *chip = new EditLiveChip(m_doc, &win);
        const int h = win.fontMetrics().height() + 4;
        chip->setFixedHeight(h);
        chip->setChipHeight(h);
        win.statusBar()->addPermanentWidget(chip);
        QToolButton *mode = new QToolButton(&win);
        mode->setText(QStringLiteral("OPERATE"));
        mode->setAutoRaise(true);
        win.statusBar()->addPermanentWidget(mode);
        win.resize(size);
        win.show();
        settle();
        const QString base = QStringLiteral("%1/%2-editlive-%3-%4x%5.png").arg(dir, prefix);
        // Off, then on with trims and its popup, then Store to Cue after a GO.
        QVERIFY(win.grab().save(base.arg(QStringLiteral("off")).arg(size.width()).arg(size.height())));
        QVERIFY(el->setOn(true));
        trimRed();
        settle();
        QMenu *menu = chip->buildMenu();
        menu->popup(chip->mapToGlobal(QPoint(0, 0)) - QPoint(0, menu->sizeHint().height()));
        settle();
        QImage img = win.grab().toImage();
        {
            QPainter p(&img);
            const QPoint at = win.mapFromGlobal(menu->pos());
            p.drawPixmap(at, menu->grab());
        }
        QVERIFY(img.save(base.arg(QStringLiteral("popup")).arg(size.width()).arg(size.height())));
        menu->close();
        delete menu;
        go();
        settle();
        QVERIFY(win.grab().save(base.arg(QStringLiteral("storetocue")).arg(size.width()).arg(size.height())));
        el->setOn(false);
        // Back to cue 1 for the next size.
        for (Function *f : m_doc->functions())
            f->stop(FunctionParent::master());
        tick(5);
        go();
    }
}

QTEST_MAIN(EditLiveUi_Test)
