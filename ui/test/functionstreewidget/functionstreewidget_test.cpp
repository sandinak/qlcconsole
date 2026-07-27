/*
  Q Light Controller Plus - Test Unit
  functionstreewidget_test.cpp

  Copyright (c) Massimo Callegari

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

#include "functionstreewidget.h"
#include "collection.h"
#include "rgbmatrix.h"
#include "function.h"
#include "chaser.h"
#include "scene.h"
#include "show.h"
#include "doc.h"
#include "efx.h"

#include "functionstreewidget_test.h"

void FunctionsTreeWidget_Test::init()
{
    m_doc = new Doc(this);
}

void FunctionsTreeWidget_Test::cleanup()
{
    delete m_doc;
    m_doc = NULL;
}

// One function of each of the six visible categories, added out of role order.
static void addOnePerType(Doc *doc)
{
    doc->addFunction(new Scene(doc));
    doc->addFunction(new Chaser(doc));
    doc->addFunction(new Collection(doc));
    doc->addFunction(new EFX(doc));
    doc->addFunction(new RGBMatrix(doc));
    doc->addFunction(new Show(doc));
}

static QStringList topLevelTexts(const FunctionsTreeWidget &t)
{
    QStringList out;
    for (int i = 0; i < t.topLevelItemCount(); i++)
        out << t.topLevelItem(i)->text(0);
    return out;
}

void FunctionsTreeWidget_Test::typeSortRankTable()
{
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::ShowType)
            < FunctionsTreeWidget::typeSortRank(Function::ChaserType));
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::ChaserType)
            < FunctionsTreeWidget::typeSortRank(Function::CollectionType));
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::CollectionType)
            < FunctionsTreeWidget::typeSortRank(Function::RGBMatrixType));
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::RGBMatrixType)
            < FunctionsTreeWidget::typeSortRank(Function::EFXType));
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::EFXType)
            < FunctionsTreeWidget::typeSortRank(Function::SceneType));
    // Any listed type ranks below an unlisted one (audio/video/…).
    QVERIFY(FunctionsTreeWidget::typeSortRank(Function::SceneType)
            < FunctionsTreeWidget::typeSortRank(Function::AudioType));
}

void FunctionsTreeWidget_Test::categoryOrderByRole()
{
    addOnePerType(m_doc);

    FunctionsTreeWidget tree(m_doc);
    tree.setDisplayFilter(FunctionsTreeWidget::FunctionsOnly);
    tree.setSortingEnabled(true);
    tree.updateTree();
    tree.setTypeOrderByRole(true);

    QStringList want;
    want << Function::typeToString(Function::ShowType)
         << Function::typeToString(Function::ChaserType)
         << Function::typeToString(Function::CollectionType)
         << Function::typeToString(Function::RGBMatrixType)
         << Function::typeToString(Function::EFXType)
         << Function::typeToString(Function::SceneType);

    QCOMPARE(topLevelTexts(tree), want);
}

void FunctionsTreeWidget_Test::categoryOrderAlphabetical()
{
    addOnePerType(m_doc);

    FunctionsTreeWidget tree(m_doc);
    tree.setDisplayFilter(FunctionsTreeWidget::FunctionsOnly);
    tree.setSortingEnabled(true);
    tree.updateTree();
    tree.setTypeOrderByRole(false);

    const QStringList got = topLevelTexts(tree);
    QStringList sorted = got;
    sorted.sort(Qt::CaseInsensitive);
    QCOMPARE(got, sorted);
}

QTEST_MAIN(FunctionsTreeWidget_Test)
