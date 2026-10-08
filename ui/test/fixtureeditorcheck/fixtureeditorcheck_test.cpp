/*
  Q Light Controller Plus
  fixtureeditorcheck_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QPlainTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTreeWidget>
#include <QSignalSpy>
#include <QLabel>
#include <QtTest>

#define private public
#include "repairdialog.h"
#include "problemspanel.h"
#undef private

#include "fixtureeditorcheck_test.h"
#include "qlcfixturemode.h"
#include "qlcfixturedef.h"

/* The PHS Crew step file from the user library, as the fixture editor now
   opens it: where it breaks, a repair, then the next problem's repair,
   then it reads and can go to the editor. */
void FixtureEditorCheck_Test::aBrokenFileIsRepairedStepByStep()
{
    QFile f(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf");
    QVERIFY(f.open(QIODevice::ReadOnly));
    RepairDialog dlg(f.fileName(), f.readAll());

    // "\>" for "/>" cannot be anything else: already fixed, and said so.
    QVERIFY(!dlg.m_autoBar->isHidden());
    QVERIFY2(dlg.m_autoNote->text().contains("192"), qPrintable(dlg.m_autoNote->text()));
    QVERIFY(!dlg.xml().contains("\\>"));
    QVERIFY2(dlg.m_problem->text().contains("no modes"), qPrintable(dlg.m_problem->text()));
    QVERIFY(!dlg.m_open->isEnabled());
    QVERIFY(dlg.m_fixes->item(0)->text().contains("64 heads"));

    dlg.applyRepair(0);
    QVERIFY2(dlg.m_problem->text().contains("It reads now"), qPrintable(dlg.m_problem->text()));
    QVERIFY(dlg.m_open->isEnabled());

    QLCFixtureDef def;
    QVERIFY(def.loadXMLData(dlg.xml()));
    QCOMPARE(def.modes().first()->heads().size(), 64);
}

void FixtureEditorCheck_Test::anEditByHandIsCheckedAgain()
{
    QFile f(QString(FIXTUREDEFCHECK_DATA) + "backslash-step.qxf");
    QVERIFY(f.open(QIODevice::ReadOnly));
    RepairDialog dlg(f.fileName(), f.readAll());
    // Undo the automatic fix: back to the file as it was.
    dlg.m_autoBar->findChild<QPushButton *>()->click();
    QVERIFY(dlg.m_autoBar->isHidden());
    QVERIFY(dlg.m_problem->text().contains("Line 12"));
    // ...and fixed by hand instead: checked again as typed.
    dlg.m_text->setPlainText(dlg.m_text->toPlainText().replace("\\>", "/>"));
    QTRY_VERIFY(dlg.m_problem->text().contains("no modes"));
}

void FixtureEditorCheck_Test::theProblemsPanelCountsAndGoesToAnIssue()
{
    ProblemsPanel panel;
    FixtureDefCheck::Issue err;
    err.level = FixtureDefCheck::Issue::Error;
    err.section = FixtureDefCheck::Issue::Modes;
    err.target = "8ch";
    err.message = "Mode \"8ch\": head 2 uses channel 9, which the mode does not have.";
    FixtureDefCheck::Issue warn;
    warn.section = FixtureDefCheck::Issue::Physical;
    warn.message = "No beam angle (lens degrees): its beam is drawn at a guess.";
    panel.setIssues({ err, warn });
    QCOMPARE(panel.errorCount(), 1);
    QVERIFY(panel.m_summary->text().contains("1 error"));
    QCOMPARE(panel.m_list->topLevelItemCount(), 2);
    QVERIFY(panel.m_list->topLevelItem(0)->text(0).contains("8ch"));

    QSignalSpy spy(&panel, &ProblemsPanel::activated);
    emit panel.m_list->itemActivated(panel.m_list->topLevelItem(0), 1);
    QCOMPARE(spy.count(), 1);

    panel.setIssues({});
    QVERIFY(panel.m_summary->text().contains("No problems"));
}

/* Loads, but its mode names a channel that is not there: amber, a repair
   that guesses the name, and "Open Anyway" for opening it as it is. */
void FixtureEditorCheck_Test::aFileThatLosesPartsCanBeOpenedAnyway()
{
    const QByteArray xml(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE FixtureDefinition>\n"
        "<FixtureDefinition xmlns=\"http://www.qlcplus.org/FixtureDefinition\">\n"
        " <Manufacturer>Test</Manufacturer>\n"
        " <Model>Par</Model>\n"
        " <Type>Color Changer</Type>\n"
        " <Channel Name=\"Dimmer\" Preset=\"IntensityDimmer\"/>\n"
        " <Mode Name=\"1ch\">\n"
        "  <Channel Number=\"0\">Dimer</Channel>\n"
        " </Mode>\n"
        "</FixtureDefinition>\n");
    RepairDialog dlg("par.qxf", xml);
    QVERIFY(dlg.m_problem->text().contains("#d08a00"));      // amber: it loads
    QVERIFY(dlg.m_open->isEnabled());
    QCOMPARE(dlg.m_open->text(), QString("Open Anyway"));
    QVERIFY2(dlg.m_fixes->item(0)->text().contains("as \"Dimmer\""), qPrintable(dlg.m_fixes->item(0)->text()));
    dlg.applyRepair(0);
    QCOMPARE(dlg.m_open->text(), QString("Open in Editor"));
    QVERIFY(dlg.m_problem->text().contains("It reads now"));
}

QTEST_MAIN(FixtureEditorCheck_Test)
