/*
  Q Light Controller Plus - Test Unit
  effectfit_test.cpp

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
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#include "effectbrowser.h"
#include "lookeditor.h"
#undef private
#include "effectscriptrunner.h"
#include "effectscriptcache.h"
#include "effectorigin.h"
#include "effectfit.h"
#include "qlcfixturedef.h"
#include "qlcpalette.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"

#include "effectfit_test.h"

namespace
{
const QString kRoot = QString(QLC_TEST_RESOURCE_ROOT);

/** A fixture of the library in the show; "wled" puts it on the network. */
Fixture *add(Doc *doc, const QString &qxf, const QString &mode, bool wled = false)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    if (def->loadXML(kRoot + "fixtures/" + qxf) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, def->mode(mode));
    f->setAddress(doc->fixtures().size() * 60);     // each its own place
    if (wled)
    {
        f->setConnection(Fixture::Network);
        f->setIPAddress("172.18.2.60");
    }
    return doc->addFixture(f) ? f : nullptr;
}

Fixture *kind(Doc *doc, const QString &which)
{
    if (which == "wled")    return add(doc, "WLED/WLED-Lantern.qxf", "Effect (15ch)", true);
    if (which == "mover")   return add(doc, "Ayrton/Ayrton-Rivale-Profile.qxf", "Standard");
    if (which == "par")     return add(doc, "Generic/Generic-Generic-RGB.qxf", "RGB");
    if (which == "bar")     return add(doc, "AFX/AFX-BARLED200-FX.qxf", "48 Channel");
    if (which == "dimmer")
    {
        Fixture *d = new Fixture(doc);
        d->setChannels(1);
        d->setAddress(doc->fixtures().size() * 60);
        return doc->addFixture(d) ? d : nullptr;
    }
    return nullptr;
}

EffectFit::Need needOf(Doc *doc, const QString &effect)
{
    if (effect == "WLED device effect")
        return EffectFit::wledNeed();
    EffectScriptCache *cache = doc->effectScriptRunner()->cache();
    return EffectFit::scriptNeed(cache->scriptMeta(effect).fixtureTypes, cache->scriptPath(effect));
}
}

void EffectFit_Test::init()
{
    m_doc = new Doc(this);
    m_doc->effectScriptRunner()->cache()->addDirectory(QDir(kRoot + "effectscripts"));
}

void EffectFit_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void EffectFit_Test::whatFitsWhat_data()
{
    QTest::addColumn<QString>("fixture");
    QTest::addColumn<QString>("effect");
    QTest::addColumn<bool>("fits");

    // fixture kinds: wled lantern, moving head profile, rgb par, dimmer, pixel bar
    struct Row { const char *fixture; bool wled, rgb, position, gobo, iris, zoom, matrix, dimmerFx; };
    const Row rows[] = {
        //  fixture   WLED   RGB   Pos   Gobo  Iris  Zoom  Matrix Dimmer
        { "wled",     true,  true, false, false, false, false, false, true  },
        { "mover",    false, false, true, true,  true,  true,  false, false },
        { "par",      false, true, false, false, false, false, false, true  },
        { "dimmer",   false, false, false, false, false, false, false, true  },
        { "bar",      false, true, false, false, false, false, true,  true  },
    };
    for (const Row &r : rows)
    {
        const QString f = r.fixture;
        QTest::newRow(qPrintable(f + " / WLED device effect")) << f << "WLED device effect" << r.wled;
        QTest::newRow(qPrintable(f + " / Rainbow (colour)")) << f << "Rainbow" << r.rgb;
        QTest::newRow(qPrintable(f + " / Diamond (position)")) << f << "Diamond" << r.position;
        QTest::newRow(qPrintable(f + " / Gobo Chase")) << f << "Gobo Chase" << r.gobo;
        QTest::newRow(qPrintable(f + " / Iris Pulse")) << f << "Iris Pulse" << r.iris;
        QTest::newRow(qPrintable(f + " / Zoom Pulse")) << f << "Zoom Pulse" << r.zoom;
        QTest::newRow(qPrintable(f + " / Matrix Bars")) << f << "Matrix Bars" << r.matrix;
        QTest::newRow(qPrintable(f + " / Dimmer Phaser (intensity)")) << f << "Dimmer Phaser" << (r.dimmerFx || r.position);
    }
}

void EffectFit_Test::whatFitsWhat()
{
    QFETCH(QString, fixture);
    QFETCH(QString, effect);
    QFETCH(bool, fits);

    Fixture *f = kind(m_doc, fixture);
    QVERIFY(f != nullptr);
    const EffectFit::Targets t = EffectFit::targetsOf(m_doc, { f->id() });
    QVERIFY(t.known());
    const EffectFit::Need need = needOf(m_doc, effect);
    const EffectFit::Result r = EffectFit::fit(need, t);
    QVERIFY2(r.fits == fits, qPrintable(QString("%1 on %2: %3").arg(effect, fixture, r.why)));
    QCOMPARE(r.fits, fits);
    QCOMPARE(r.runsOn, fits ? 1 : 0);
    if (!fits)
        QVERIFY(!r.why.isEmpty());          // it says why
    else
        QVERIFY(r.why.isEmpty());           // all of one fixture: nothing to qualify
}

void EffectFit_Test::aMixedSetRunsOnSome()
{
    Fixture *a = kind(m_doc, "par"), *b = kind(m_doc, "par");
    Fixture *mover = kind(m_doc, "mover");
    Fixture *wled = kind(m_doc, "wled");
    QVERIFY(a && b && mover && wled);
    const QList<quint32> ids = { a->id(), b->id(), mover->id(), wled->id(), a->id() /* twice: counted once */ };
    const EffectFit::Targets t = EffectFit::targetsOf(m_doc, ids);
    QCOMPARE(t.fixtures.size(), 4);

    EffectFit::Result r = EffectFit::fit(needOf(m_doc, "Diamond"), t);
    QVERIFY(r.fits);
    QCOMPARE(r.runsOn, 1);
    QCOMPARE(r.of, 4);
    QVERIFY2(r.why.contains("1 of 4"), qPrintable(r.why));

    r = EffectFit::fit(EffectFit::wledNeed(), t);
    QVERIFY(r.fits);
    QCOMPARE(r.runsOn, 1);

    r = EffectFit::fit(needOf(m_doc, "Gobo Chase"), t);
    QCOMPARE(r.runsOn, 1);                  // the mover's wheel only
}

void EffectFit_Test::noTargetsListsEverything()
{
    const EffectFit::Targets none = EffectFit::targetsOf(m_doc, QList<quint32>());
    QVERIFY(!none.known());
    for (const QString &n : { QString("Diamond"), QString("Gobo Chase"), QString("Matrix Bars") })
        QVERIFY(EffectFit::fit(needOf(m_doc, n), none).fits);
    QVERIFY(EffectFit::fit(EffectFit::wledNeed(), none).fits);
}

void EffectFit_Test::searchHidesWhatDoesNotFit()
{
    Fixture *lantern = kind(m_doc, "wled");     // a WLED in the show, not in this look
    QVERIFY(lantern != nullptr);
    Fixture *par = kind(m_doc, "par");
    QVERIFY(par != nullptr);
    Scene *scene = new Scene(m_doc);
    scene->addFixture(par->id());
    m_doc->addFunction(scene);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setName("Look");
    QVERIFY(m_doc->addPalette(look));

    LookEditor ed(m_doc);
    ed.setContextScene(scene);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    QVERIFY(browser != nullptr);
    QVERIFY(browser->targets().known());
    QVERIFY(!browser->wledTargets());

    auto find = [browser](const QString &text, const QString &category) {
        browser->search()->setText(text);
        QStringList out;
        for (int i = 0; i < browser->list()->count(); ++i)
        {
            const int e = browser->list()->item(i)->data(Qt::UserRole + 10).toInt();
            if (browser->m_entries.at(e).category == category)
                out << browser->list()->item(i)->text();
        }
        return out;
    };

    QVERIFY(browser->tabNames().indexOf("WLED") < 0);      // the whole tab is gone
    QVERIFY(find("chase", "WLED").isEmpty());
    // A position effect isn't for a par either.
    QVERIFY(find("diamond", "Position").isEmpty());

    // Show all: they appear, greyed, with why on hover.
    QVERIFY(!browser->showAllButton()->isHidden());
    browser->showAllButton()->setChecked(true);
    const QStringList wled = find("chase", "WLED");
    QVERIFY(!wled.isEmpty());
    QStringList tips;
    for (int i = 0; i < browser->list()->count(); ++i)
        tips << browser->list()->item(i)->toolTip();
    QVERIFY(tips.join("\n").contains("Needs a WLED device"));
    QVERIFY(!find("diamond", "Position").isEmpty());

    // Back off: gone again.
    browser->showAllButton()->setChecked(false);
    QVERIFY(find("chase", "WLED").isEmpty());

    // A WLED among the targets: its effects are listed, no toggle needed.
    scene->addFixture(lantern->id());
    ed.setContextScene(scene);
    QVERIFY(browser->wledTargets());
    QVERIFY(!find("chase", "WLED").isEmpty());
}

void EffectFit_Test::theEffectInUseIsNeverDropped()
{
    Fixture *par = kind(m_doc, "par");
    Scene *scene = new Scene(m_doc);
    scene->addFixture(par->id());
    m_doc->addFunction(scene);
    QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
    look->setName("Look");
    look->setScriptPath(m_doc->effectScriptRunner()->cache()->scriptPath("Diamond"));
    QVERIFY(m_doc->addPalette(look));

    LookEditor ed(m_doc);
    ed.setContextScene(scene);
    ed.setPalette(look->id());
    EffectBrowser *browser = ed.m_effectBrowser;
    browser->search()->setText("diamond");
    QCOMPARE(browser->list()->count(), 1);                   // the one it runs, no others like it
    QCOMPARE(ed.m_effectScriptCombo->currentText().trimmed(), QString("Diamond"));
    QVERIFY(!browser->fitNote()->isHidden());
    QVERIFY2(browser->fitNote()->text().contains("pan/tilt"), qPrintable(browser->fitNote()->text()));
    QCOMPARE(look->scriptPath(), m_doc->effectScriptRunner()->cache()->scriptPath("Diamond"));
}

void EffectFit_Test::shots()
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty())
        return;                             // pictures only on request
    QDir().mkpath(dir);
    const QSize sizes[] = { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) };
    struct Group { const char *name; QStringList fixtures; };
    const Group groups[] = {
        { "movinghead", { "mover", "mover", "mover", "mover" } },
        { "par", { "par", "par", "par", "par" } },
        { "wled", { "wled", "wled" } },
    };
    for (const Group &g : groups)
    {
        cleanup();
        init();                             // a show of its own for each
        Scene *scene = new Scene(m_doc);
        for (const QString &k : g.fixtures)
        {
            Fixture *f = kind(m_doc, k);
            QVERIFY(f != nullptr);
            scene->addFixture(f->id());
        }
        m_doc->addFunction(scene);
        QLCPalette *look = new QLCPalette(QLCPalette::Effect, m_doc);
        look->setName("Look");
        QVERIFY(m_doc->addPalette(look));
        LookEditor ed(m_doc);
        ed.setContextScene(scene);
        ed.setPalette(look->id());
        EffectBrowser *browser = ed.m_effectBrowser;
        for (const QSize &sz : sizes)
            for (int all = 1; all >= 0; --all)
            {
                browser->showAllButton()->setChecked(all == 1);
                ed.openEffectPicker();
                ed.m_effectPicker->resize(sz);
                QApplication::processEvents();
                QTest::qWait(250);
                const QString file = QString("%1/%2-%3-%4x%5.png").arg(dir, g.name, all ? "before" : "after")
                                         .arg(sz.width()).arg(sz.height());
                QVERIFY(ed.m_effectPicker->grab().save(file));
                ed.closeEffectPicker(true);
            }
    }
}

QTEST_MAIN(EffectFit_Test)
