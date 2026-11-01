/*
  Q Light Controller Plus - Unit test
  configimportdialog_test.cpp

  File ▸ Import ▸ Universes and I/O Setup: the form over the import_config
  operation (docs/design/AI_ASSIST.md section 5b), on a tiny show file in this
  directory. Offscreen; no AI.

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
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QJsonArray>

#include "configimportdialog_test.h"
#include "configimportdialog.h"
#include "operationregistry.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "inputoutputmap.h"
#include "networkroute.h"
#include "doc.h"

namespace
{
const QString kFile = QStringLiteral(CONFIGIMPORTDIALOG_SAMPLES "/evo26.qxw");

void offNetwork()
{
    NetworkRoute::setResolverForTest([]() { return QList<NetworkRoute::LocalAddress>(); },
                                     [](const QHostAddress &) { return QHostAddress(); });
}
}

void ConfigImportDialog_Test::importWaitsForTheChoices()
{
    offNetwork();
    Doc doc(this, 1);
    ConfigImportDialog d(nullptr, &doc, kFile);
    // A plugin this machine lacks: a choice is needed before Import.
    QVERIFY(!d.importButton()->isEnabled());
    QVERIFY2(d.preview()->toPlainText().contains("ENTTEC Gone"), qPrintable(d.preview()->toPlainText()));
    d.findChild<QComboBox *>("configMissing")->setCurrentIndex(2);   // keep
    QVERIFY2(d.importButton()->isEnabled(), qPrintable(d.preview()->toPlainText()));
    QVERIFY(d.preview()->toPlainText().contains("not currently routable"));
    QVERIFY(d.preview()->toPlainText().contains("Universe 1 \"Front\""));
    // Nothing is changed by looking.
    QCOMPARE(doc.inputOutputMap()->universesCount(), quint32(1));
    NetworkRoute::setResolverForTest(nullptr, nullptr);
}

void ConfigImportDialog_Test::formIsTheOperationsArguments()
{
    offNetwork();
    Doc doc(this, 1);
    ConfigImportDialog d(nullptr, &doc, kFile);
    QJsonObject a = d.arguments();
    QCOMPARE(a.value("file").toString(), kFile);
    QVERIFY(!a.contains("universes"));           // all of them
    QVERIFY(!a.contains("sections"));            // all of them

    qobject_cast<QCheckBox *>(d.findChild<QTableWidget *>("configUniverses")->cellWidget(1, 0))->setChecked(false);
    qobject_cast<QSpinBox *>(d.findChild<QTableWidget *>("configUniverses")->cellWidget(2, 2))->setValue(5);
    d.findChild<QPlainTextEdit *>("configAddresses")->setPlainText("2.0.0.50 = 2.0.0.60\n");
    d.findChild<QCheckBox *>("configSection_feedback")->setChecked(false);
    d.findChild<QComboBox *>("configCollision")->setCurrentIndex(3);
    d.findChild<QComboBox *>("configMissing")->setCurrentIndex(1);
    a = d.arguments();
    QCOMPARE(a.value("universes").toArray(), (QJsonArray { 1, 3 }));
    QCOMPARE(a.value("universeMap").toObject().value("3").toString(), QStringLiteral("5"));
    QCOMPARE(a.value("addressMap").toObject().value("2.0.0.50").toString(), QStringLiteral("2.0.0.60"));
    QCOMPARE(a.value("sections").toArray().count(), 3);
    QCOMPARE(a.value("onCollision").toString(), QStringLiteral("renumber"));
    QCOMPARE(a.value("onMissingPlugin").toString(), QStringLiteral("skip"));
    // The same text the command line would take.
    QVERIFY(OperationRegistry::operation("import_config")->cliSyntax(a).contains("map 3=5"));
    QVERIFY(d.importButton()->isEnabled());
    // Nothing picked: no import.
    d.findChild<QCheckBox *>("configSection_universes")->setChecked(false);
    d.findChild<QCheckBox *>("configSection_outputs")->setChecked(false);
    d.findChild<QCheckBox *>("configSection_inputs")->setChecked(false);
    QVERIFY(!d.importButton()->isEnabled());
    NetworkRoute::setResolverForTest(nullptr, nullptr);
}

void ConfigImportDialog_Test::importIsOneUndoStep()
{
    offNetwork();
    Doc doc(this, 1);
    ConfigImportDialog d(nullptr, &doc, kFile);
    d.findChild<QComboBox *>("configMissing")->setCurrentIndex(2);
    QVERIFY(d.importButton()->isEnabled());
    // What App::importConfigFile does on Import.
    ShowTreeController *st = doc.showTree();
    ShowTreeCommand cmd(&doc, st);
    OperationRegistry reg(&doc, &cmd);
    const OperationRegistry::Result r = reg.runPlan(
        { OperationRegistry::Call { QStringLiteral("import_config"), d.arguments() } },
        ShowTreeCommand::Context(), QStringLiteral("Import configuration from evo26.qxw"));
    QVERIFY2(r.ok, qPrintable(r.message));
    QVERIFY(r.text.contains("Universe 1"));
    QCOMPARE(doc.inputOutputMap()->universesCount(), quint32(3));
    QCOMPARE(st->sharedUndoText(), QStringLiteral("Import configuration from evo26.qxw"));
    QVERIFY(st->undoShared());
    QCOMPARE(doc.inputOutputMap()->universesCount(), quint32(1));
    QVERIFY(!doc.inputOutputMap()->hasOfflinePatches());
    NetworkRoute::setResolverForTest(nullptr, nullptr);
}

void ConfigImportDialog_Test::notAShowFileCannotImport()
{
    Doc doc(this, 1);
    ConfigImportDialog d(nullptr, &doc, QStringLiteral(CONFIGIMPORTDIALOG_SAMPLES "/nothing.qxw"));
    QVERIFY(!d.importButton()->isEnabled());
    QVERIFY(d.preview()->toPlainText().contains("can't be opened"));
}

QTEST_MAIN(ConfigImportDialog_Test)
