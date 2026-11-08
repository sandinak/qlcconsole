/*
  Q Light Controller Plus
  importmenu_test.cpp

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
#include <QTemporaryDir>
#include <QXmlStreamWriter>
#include <QFile>

#include "importmenu_test.h"
#include "doc.h"
// The importers are App's own (private) handlers: compared by address.
#define private public
#include "app.h"
#undef private
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "scene.h"
#include "sequence.h"
#include "chaserstep.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "importselectiondialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include "qlcfile.h"

#if defined(WIN32) || defined(Q_OS_WIN)
#define ALL_FILES "All Files (*.*)"
#else
#define ALL_FILES "All Files (*)"
#endif

void ImportMenu_Test::initTestCase()
{
    m_app = new App();
    m_app->init();

    m_app->setImportFilePicker([this](const QString &title, const QStringList &filters) {
        ++m_picks;
        m_pickTitle = title;
        m_pickFilters = filters;
        return m_pickResult;
    });
    m_app->setImportRunHook([this](const QString &key, const QString &path) {
        m_runs << key + QLatin1Char('|') + path;
    });
}

void ImportMenu_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void ImportMenu_Test::cleanup()
{
    m_pickTitle.clear();
    m_pickFilters.clear();
    m_pickResult.clear();
    m_picks = 0;
    m_runs.clear();
}

QAction *ImportMenu_Test::entry(const QString &key) const
{
    QMenu *menu = m_app->fileImportMenu();
    if (menu == nullptr)
        return nullptr;
    foreach (QAction *a, menu->actions())
        if (a->objectName() == QStringLiteral("fileImport_") + key)
            return a;
    return nullptr;
}

void ImportMenu_Test::submenuHasTheSources()
{
    QMenu *menu = m_app->fileImportMenu();
    QVERIFY(menu != nullptr);

    // In the File menu, as a submenu.
    QMenu *file = nullptr;
    foreach (QAction *a, m_app->menuBar()->actions())
        if (a->menu() != nullptr && a->menu()->actions().contains(menu->menuAction()))
            file = a->menu();
    QVERIFY2(file != nullptr, "Import is a submenu of File");

    QStringList seen;
    foreach (QAction *a, menu->actions())
    {
        if (a->isSeparator())
            seen << (a->text().isEmpty() ? QStringLiteral("--") : QStringLiteral("## ") + a->text());
        else
            seen << a->objectName() + QLatin1Char('=') + a->text();
    }
    const QStringList expected {
        QStringLiteral("## From a show file"),
        QStringLiteral("fileImport_workspace=QLC+ / qlcconsole &Workspace (.qxw, .qlcc)…"),
        QStringLiteral("fileImport_config=Universes and I/O &Setup (.qxw)…"),
        QStringLiteral("fileImport_eos=ETC &Eos Show (ASCII .asc)…"),
        QStringLiteral("## Fixtures && patch"),
        QStringLiteral("fileImport_fixturelist=QLC+ &Fixtures List (.qxfl)…"),
        QStringLiteral("--"),
        QStringLiteral("fileImport_any=&Any Supported File…"),
    };
    QCOMPARE(seen, expected);

    // Icons where there is one.
    QVERIFY(entry("workspace")->icon().isNull() == false);
    QVERIFY(entry("fixturelist")->icon().isNull() == false);
    QVERIFY(entry("any")->icon().isNull() == false);

    // The table and the menu agree: a row is an entry.
    QCOMPARE(m_app->importSources().size(), 4);
    foreach (const App::ImportSource &src, m_app->importSources())
    {
        QVERIFY2(entry(src.key) != nullptr, qPrintable(src.key));
        QVERIFY(src.handler != nullptr);
        // Every row is dispatched by extension, but the configuration
        // import: it is chosen by its entry (a .qxw is the Workspace's).
        QVERIFY(src.extensions.isEmpty() == (src.key == QLatin1String("config")));
    }
}

void ImportMenu_Test::eachEntryRoutesToItsImporter_data()
{
    QTest::addColumn<QString>("key");
    QTest::addColumn<QString>("path");
    QTest::addColumn<QStringList>("filters");

    QTest::newRow("workspace") << "workspace" << "/shows/other.qlcc"
        << QStringList{ "Workspaces (*.qlcc *.qxw)", "qlcconsole Workspaces (*.qlcc)",
                        "QLC+ Workspaces (*.qxw)", ALL_FILES };
    QTest::newRow("config") << "config" << "/shows/evo26.qxw"
        << QStringList{ "Workspaces (*.qlcc *.qxw)", ALL_FILES };
    QTest::newRow("eos") << "eos" << "/shows/venue.asc"
        << QStringList{ "ETC Eos shows, USITT ASCII export (*.asc *.ASC)", ALL_FILES };
    QTest::newRow("fixturelist") << "fixturelist" << "/shows/rig.qxfl"
        << QStringList{ "Fixtures List (*.qxfl)", ALL_FILES };
    // The entry says what the file is: its importer even for another name.
    QTest::newRow("eos, odd name") << "eos" << "/shows/venue.txt"
        << QStringList{ "ETC Eos shows, USITT ASCII export (*.asc *.ASC)", ALL_FILES };
}

void ImportMenu_Test::eachEntryRoutesToItsImporter()
{
    QFETCH(QString, key);
    QFETCH(QString, path);
    QFETCH(QStringList, filters);

    QAction *a = entry(key);
    QVERIFY(a != nullptr);
    m_pickResult = path;
    a->trigger();

    QCOMPARE(m_picks, 1);
    QCOMPARE(m_pickFilters, filters);
    QVERIFY(m_pickTitle.isEmpty() == false);
    QCOMPARE(m_runs, QStringList{ key + QLatin1Char('|') + path });

    // And the handler behind that key is that importer.
    void (App::*expected)(const QString &) = nullptr;
    if (key == "workspace")
        expected = &App::importWorkspaceFile;
    else if (key == "config")
        expected = &App::importConfigFile;
    else if (key == "eos")
        expected = &App::importEosShow;
    else if (key == "fixturelist")
        expected = &App::importFixtureListFile;
    bool found = false;
    foreach (const App::ImportSource &src, m_app->importSources())
        if (src.key == key)
        {
            found = true;
            QVERIFY(src.handler == expected);
        }
    QVERIFY(found);
}

void ImportMenu_Test::cancelImportsNothing()
{
    m_pickResult.clear();
    entry("workspace")->trigger();
    entry("config")->trigger();
    entry("eos")->trigger();
    entry("fixturelist")->trigger();
    entry("any")->trigger();
    QCOMPARE(m_picks, 5);
    QVERIFY(m_runs.isEmpty());
}

void ImportMenu_Test::anySupportedFileDispatchesByExtension_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("key");

    QTest::newRow(".qlcc") << "/s/a.qlcc" << "workspace";
    QTest::newRow(".qxw") << "/s/a.qxw" << "workspace";
    QTest::newRow(".QXW") << "/s/A.QXW" << "workspace";
    QTest::newRow(".asc") << "/s/b.asc" << "eos";
    QTest::newRow(".ASC") << "/s/B.ASC" << "eos";
    QTest::newRow(".qxfl") << "/s/c.qxfl" << "fixturelist";
    QTest::newRow("unknown") << "/s/d.xml" << "workspace";
}

void ImportMenu_Test::anySupportedFileDispatchesByExtension()
{
    QFETCH(QString, path);
    QFETCH(QString, key);

    m_pickResult = path;
    entry("any")->trigger();

    QCOMPARE(m_picks, 1);
    const QStringList expectedFilters {
        "All supported files (*.qlcc *.qxw *.asc *.qxfl)",
        "Workspaces (*.qlcc *.qxw)",
        "qlcconsole Workspaces (*.qlcc)",
        "QLC+ Workspaces (*.qxw)",
        "ETC Eos shows, USITT ASCII export (*.asc *.ASC)",
        "Fixtures List (*.qxfl)",
        ALL_FILES,
    };
    QCOMPARE(m_pickFilters, expectedFilters);
    QCOMPARE(m_runs, QStringList{ key + QLatin1Char('|') + path });

    // importFile() is the same dispatch without the dialog.
    m_runs.clear();
    m_app->importFile(path);
    QCOMPARE(m_runs, QStringList{ key + QLatin1Char('|') + path });
}

void ImportMenu_Test::entriesShowInEveryView()
{
    const App::WorkspaceView views[] = { App::ClassicView, App::ConsoleView, App::EverythingView };
    for (App::WorkspaceView view : views)
    {
        m_app->setWorkspaceView(view, false);
        QMenu *menu = m_app->fileImportMenu();
        QVERIFY(menu->menuAction()->isVisible());
        QVERIFY(menu->menuAction()->isEnabled());
        const QStringList keys { "workspace", "config", "eos", "fixturelist", "any" };
        foreach (const QString &key, keys)
        {
            QAction *a = entry(key);
            QVERIFY2(a != nullptr && a->isVisible() && a->isEnabled(),
                     qPrintable(App::workspaceViewKey(view) + QLatin1Char(' ') + key));
        }
    }
    m_app->setWorkspaceView(App::ConsoleView, false);
}

void ImportMenu_Test::fixtureListImportAddsFixtures()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("rig.qxfl"));

    // A Fixtures List as Export fixtures writes it: two generic dimmers.
    {
        Doc scratch(this);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QXmlStreamWriter xml(&file);
        QLCFile::writeXMLHeader(&xml, KXMLQLCFixturesList);
        for (int i = 0; i < 2; ++i)
        {
            Fixture *fxi = new Fixture(&scratch);
            fxi->setName(QStringLiteral("Imported dimmer %1").arg(i + 1));
            fxi->setUniverse(3);
            fxi->setAddress(i * 4);
            fxi->setChannels(4);
            QVERIFY(scratch.addFixture(fxi));
            fxi->saveXML(&xml);
        }
        xml.writeEndDocument();
        file.close();
    }

    Doc *doc = m_app->doc();
    const int before = doc->fixtures().count();

    m_app->setImportRunHook(App::ImportRunHook());   // the real importer
    m_pickResult = path;
    entry("fixturelist")->trigger();
    m_app->setImportRunHook([this](const QString &key, const QString &p) {
        m_runs << key + QLatin1Char('|') + p;
    });

    QCOMPARE(doc->fixtures().count(), before + 2);
    QStringList names;
    foreach (Fixture *fxi, doc->fixtures())
        names << fxi->name();
    QVERIFY(names.contains(QStringLiteral("Imported dimmer 1")));
    QVERIFY(names.contains(QStringLiteral("Imported dimmer 2")));
}

void ImportMenu_Test::importedCopiesPointAtTheImportedObjects()
{
    Doc *doc = m_app->doc();

    // Here first, so the source's ids are taken: a fixture, a group, a
    // palette, a scene and two stage targets ("Lead" not the first).
    Fixture *house = new Fixture(doc);
    house->setName("House");
    house->setUniverse(0);
    house->setAddress(100);
    house->setChannels(1);
    QVERIFY(doc->addFixture(house));
    FixtureGroup *houseGroup = new FixtureGroup(doc);
    houseGroup->setName("House lights");
    QVERIFY(doc->addFixtureGroup(houseGroup));
    houseGroup->assignFixture(house->id());
    QLCPalette *amber = new QLCPalette(QLCPalette::Color, doc);
    amber->setName("Amber");
    amber->setValue(QString("#ffbf00"));
    QVERIFY(doc->addPalette(amber));
    QLCPalette *amber2 = new QLCPalette(QLCPalette::Color, doc);
    amber2->setName("Amber 2");
    amber2->setValue(QString("#ffbf00"));
    QVERIFY(doc->addPalette(amber2));
    for (int i = 0; i < 3; ++i)
    {
        Scene *walkIn = new Scene(doc);
        walkIn->setName(QString("Walk-in %1").arg(i));
        QVERIFY(doc->addFunction(walkIn));
    }
    doc->monitorProperties()->addStageTarget()->setName("Other");
    StageTarget *lead = doc->monitorProperties()->addStageTarget();
    lead->setName("Lead");

    // The source: a par in group Front; Red, an effect bound to Red, an aim
    // at its own "Lead"; a scene on Front playing Red and the effect, Red
    // leaving the par out; a Sequence on its own scene.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("other.qxw"));
    {
        Doc scratch(this);
        Fixture *par = new Fixture(&scratch);
        par->setName("Par");
        par->setChannels(3);
        QVERIFY(scratch.addFixture(par));
        FixtureGroup *front = new FixtureGroup(&scratch);
        front->setName("Front");
        QVERIFY(scratch.addFixtureGroup(front));
        front->assignFixture(par->id());
        StageTarget *theirLead = scratch.monitorProperties()->addStageTarget();
        theirLead->setName("Lead");
        QLCPalette *red = new QLCPalette(QLCPalette::Color, &scratch);
        red->setName("Red");
        red->setValue(QString("#ff0000"));
        QVERIFY(scratch.addPalette(red));
        QLCPalette *fx = new QLCPalette(QLCPalette::Effect, &scratch);
        fx->setName("Pulse");
        fx->setEffectPaletteBinding("colour", red->id());
        fx->setEffectTargetBinding("at", theirLead->id());
        QVERIFY(scratch.addPalette(fx));
        QLCPalette *aim = new QLCPalette(QLCPalette::Aim, &scratch);
        aim->setName("At lead");
        aim->setStageTargetId(theirLead->id());
        QVERIFY(scratch.addPalette(aim));
        Scene *look = new Scene(&scratch);
        look->setName("Front red");
        QVERIFY(scratch.addFunction(look));
        look->addFixtureGroup(front->id());
        look->addFixture(par->id());
        look->addPalette(red->id());
        look->addPalette(fx->id());
        look->addPalette(aim->id());
        look->setExcludedFixtures(red->id(), { par->id() });
        Scene *base = new Scene(&scratch);
        base->setName("Chase base");
        base->setValue(par->id(), 0, 0);
        QVERIFY(scratch.addFunction(base));
        Sequence *seq = new Sequence(&scratch);
        seq->setName("Chase");
        QVERIFY(scratch.addFunction(seq));
        seq->setBoundSceneID(base->id());
        ChaserStep step(base->id());
        step.values << SceneValue(par->id(), 0, 200);
        QVERIFY(seq->addStep(step));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QXmlStreamWriter xml(&file);
        QLCFile::writeXMLHeader(&xml, KXMLQLCWorkspace);   // opens <Workspace>
        scratch.saveXML(&xml);
        xml.writeEndElement();
        xml.writeEndDocument();
        file.close();
    }

    QList<quint32> functionsBefore;
    foreach (Function *f, doc->functions())
        functionsBefore << f->id();

    // File ▸ Import ▸ Workspace: take everything, close the summary.
    QTimer answer;
    QStringList seen;
    connect(&answer, &QTimer::timeout, this, [&seen]() {
        QWidget *w = QApplication::activeModalWidget();
        if (w != nullptr)
            seen << QString::fromLatin1(w->metaObject()->className())
                        + (qobject_cast<QMessageBox *>(w) ? "|" + qobject_cast<QMessageBox *>(w)->text() : QString());
        if (ImportSelectionDialog *picker = qobject_cast<ImportSelectionDialog *>(w))
        {
            picker->selectAll();
            picker->accept();
        }
        else if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
            box->accept();
    });
    answer.start(20);
    m_app->setImportRunHook(App::ImportRunHook());   // the real importer
    m_pickResult = path;
    entry("workspace")->trigger();
    m_app->setImportRunHook([this](const QString &key, const QString &p) {
        m_runs << key + QLatin1Char('|') + p;
    });
    answer.stop();

    Fixture *par = nullptr;
    foreach (Fixture *f, doc->fixtures())
        if (f->name() == "Par")
            par = f;
    FixtureGroup *front = nullptr;
    foreach (FixtureGroup *g, doc->fixtureGroups())
        if (g->name() == "Front")
            front = g;
    QLCPalette *red = nullptr, *fx = nullptr, *aim = nullptr;
    foreach (QLCPalette *p, doc->palettes())
    {
        if (p->name() == "Red")
            red = p;
        else if (p->name() == "Pulse")
            fx = p;
        else if (p->name() == "At lead")
            aim = p;
    }
    Scene *look = nullptr, *base = nullptr;
    Sequence *seq = nullptr;
    foreach (Function *f, doc->functions())
    {
        if (functionsBefore.contains(f->id()))
            continue;
        if (f->name() == "Front red")
            look = qobject_cast<Scene *>(f);
        else if (f->name() == "Chase base")
            base = qobject_cast<Scene *>(f);
        else if (f->name() == "Chase")
            seq = qobject_cast<Sequence *>(f);
    }
    QVERIFY2(par != nullptr && front != nullptr, qPrintable(seen.join(" / ")));
    QVERIFY(red != nullptr && fx != nullptr && aim != nullptr);
    QVERIFY(look != nullptr && base != nullptr && seq != nullptr);

    // The scene: the imported group, fixture and looks, Red leaving the
    // imported par out.
    QCOMPARE(look->fixtureGroups(), QList<quint32>({ front->id() }));
    QVERIFY(look->fixtures().contains(par->id()));
    QCOMPARE(look->palettes(), QList<quint32>({ red->id(), fx->id(), aim->id() }));
    QCOMPARE(look->excludedFixtures(red->id()), QList<quint32>({ par->id() }));
    QCOMPARE(look->excludedFixtures().count(), 1);
    // The effect plays the imported Red; both aim at this show's Lead.
    QCOMPARE(fx->effectPaletteBindings().value("colour"), red->id());
    QCOMPARE(fx->effectTargetBindings().value("at"), lead->id());
    QCOMPARE(aim->stageTargetId(), lead->id());
    // The Sequence plays its imported scene.
    QCOMPARE(seq->boundSceneID(), base->id());
    QCOMPARE(seq->steps().first().fid, base->id());

    // Editing the copy leaves the original alone.
    red->setValue(QString("#0000ff"));
    QCOMPARE(amber->value().toString(), QString("#ffbf00"));
    QCOMPARE(amber2->value().toString(), QString("#ffbf00"));
    base->setValue(par->id(), 1, 90);
    foreach (quint32 fid, functionsBefore)
        if (Scene *s = qobject_cast<Scene *>(doc->function(fid)))
            QVERIFY(s->values().isEmpty());
    QCOMPARE(houseGroup->fixtureList(), QList<quint32>({ house->id() }));
}

QTEST_MAIN(ImportMenu_Test)
