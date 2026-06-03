/*
  Q Light Controller Plus
  programmingmanager.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QShowEvent>
#include <QHideEvent>
#include <QTreeWidgetItem>

#include "programmingmanager.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "scenegrouplooks.h"
#include "functionparent.h"
#include "function.h"
#include "scene.h"
#include "doc.h"

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_currentScene(Function::invalidId())
    , m_previewScene(Function::invalidId())
{
    QHBoxLayout *root = new QHBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    root->addWidget(splitter);

    // --- Left: function tree (folders) of what you build ---
    QWidget *navPanel = new QWidget(this);
    QVBoxLayout *navCol = new QVBoxLayout(navPanel);
    navCol->setContentsMargins(0, 0, 0, 0);
    navCol->addWidget(new QLabel(tr("Scenes / functions"), this));
    m_funcTree = new FunctionsTreeWidget(m_doc, this);
    m_funcTree->setDisplayFilter(FunctionsTreeWidget::FunctionsOnly);
    m_funcTree->setHeaderHidden(true);
    m_funcTree->setColumnHidden(1, true);
    m_funcTree->updateTree();
    navCol->addWidget(m_funcTree, 1);
    QPushButton *newScene = new QPushButton(tr("New scene"), this);
    navCol->addWidget(newScene);
    splitter->addWidget(navPanel);

    // --- Center: the selected scene's canvas ---
    QWidget *canvasPanel = new QWidget(this);
    m_canvasLayout = new QVBoxLayout(canvasPanel);
    m_canvasPlaceholder = new QLabel(
        tr("Select or create a scene on the left, then drag palettes, "
           "fixture groups and fixtures from the right onto it."), this);
    m_canvasPlaceholder->setWordWrap(true);
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasLayout->addWidget(m_canvasPlaceholder);
    m_canvasLayout->addStretch(1);
    splitter->addWidget(canvasPanel);

    // --- Right: drag sources (palette tree + fixtures & groups tree) ---
    QWidget *sourcePanel = new QWidget(this);
    QVBoxLayout *srcCol = new QVBoxLayout(sourcePanel);
    srcCol->setContentsMargins(0, 0, 0, 0);

    srcCol->addWidget(new QLabel(tr("Palettes (looks)"), this));
    m_paletteTree = new FunctionsTreeWidget(m_doc, this);
    m_paletteTree->setDisplayFilter(FunctionsTreeWidget::PalettesOnly);
    m_paletteTree->setHeaderHidden(true);
    m_paletteTree->setColumnHidden(1, true);
    m_paletteTree->setExternalDragMode(true); // emit palette MIME on drag
    m_paletteTree->updateTree();
    srcCol->addWidget(m_paletteTree, 1);

    srcCol->addWidget(new QLabel(tr("Fixtures & groups"), this));
    m_fixGroupSource = new FixtureGroupSource(m_doc, this);
    srcCol->addWidget(m_fixGroupSource, 1);

    splitter->addWidget(sourcePanel);

    splitter->setStretchFactor(0, 2); // nav
    splitter->setStretchFactor(1, 3); // canvas
    splitter->setStretchFactor(2, 2); // sources

    connect(newScene, SIGNAL(clicked()), this, SLOT(slotNewScene()));
    connect(m_funcTree, SIGNAL(itemSelectionChanged()),
            this, SLOT(slotFunctionSelected()));

    // Keep the trees in sync with the Doc (functor connects so we can call
    // the trees' non-slot helpers directly).
    connect(m_doc, &Doc::functionAdded, m_funcTree, &FunctionsTreeWidget::addFunction);
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32) { m_funcTree->updateTree(); });
    connect(m_doc, &Doc::functionChanged, m_funcTree, &FunctionsTreeWidget::functionNameChanged);
    connect(m_doc, &Doc::loaded, m_funcTree, &FunctionsTreeWidget::updateTree);
    connect(m_doc, &Doc::cleared, m_funcTree, &FunctionsTreeWidget::updateTree);

    connect(m_doc, &Doc::paletteAdded, m_paletteTree, &FunctionsTreeWidget::addPalette);
    connect(m_doc, &Doc::paletteRemoved, this, [this](quint32) { m_paletteTree->updateTree(); });
    connect(m_doc, &Doc::loaded, m_paletteTree, &FunctionsTreeWidget::updateTree);
    connect(m_doc, &Doc::cleared, m_paletteTree, &FunctionsTreeWidget::updateTree);

    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged()));
}

ProgrammingManager::~ProgrammingManager()
{
    stopPreview();
}

void ProgrammingManager::slotFunctionSelected()
{
    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    if (sel.isEmpty())
        return;

    const quint32 fid = m_funcTree->itemFunctionId(sel.first());
    Function *f = m_doc->function(fid);
    if (f != NULL && f->type() == Function::SceneType)
        loadCanvas(fid);
    else
        loadCanvas(Function::invalidId()); // folders / non-scene -> placeholder
}

void ProgrammingManager::loadCanvas(quint32 sceneId)
{
    if (sceneId == m_currentScene && m_canvas != NULL)
        return;

    stopPreview();
    m_currentScene = sceneId;

    if (m_canvas != NULL)
    {
        m_canvas->deleteLater();
        m_canvas = NULL;
    }

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
    {
        m_canvasPlaceholder->show();
        return;
    }

    m_canvasPlaceholder->hide();
    m_canvas = new SceneGroupLooks(scene, m_doc, this, /*includeFixtureTargets*/ true);
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    // Insert above the trailing stretch.
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, m_canvas, 1);

    startPreview();
}

void ProgrammingManager::startPreview()
{
    // Live-preview only in Design mode (don't hijack a running show).
    if (m_doc->mode() != Doc::Design)
        return;
    if (isVisible() == false)
        return;
    if (m_previewScene == m_currentScene)
        return;

    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene == NULL)
        return;

    scene->start(m_doc->masterTimer(), FunctionParent::master());
    m_previewScene = m_currentScene;
}

void ProgrammingManager::stopPreview()
{
    if (m_previewScene == Function::invalidId())
        return;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_previewScene));
    if (scene != NULL)
        scene->stop(FunctionParent::master());
    m_previewScene = Function::invalidId();
}

void ProgrammingManager::slotCanvasModified()
{
    // The running preview scene re-expands its palettes/groups on the next
    // write tick, so the DMX/2D view reflects the new look immediately.
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_previewScene));
    if (scene != NULL)
        scene->resetRuntime();
    else
        startPreview(); // a look/target was just added to a not-yet-running scene
}

void ProgrammingManager::slotModeChanged()
{
    if (m_doc->mode() == Doc::Design)
        startPreview();
    else
        stopPreview();
}

void ProgrammingManager::showEvent(QShowEvent *ev)
{
    // Pick up folder/path changes made in the Functions tab while away,
    // preserving the current scene selection.
    m_paletteTree->updateTree();
    m_funcTree->updateTree();
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (scene != NULL)
    {
        QTreeWidgetItem *it = m_funcTree->functionItem(scene);
        if (it != NULL)
            m_funcTree->setCurrentItem(it);
    }
    startPreview();
    QWidget::showEvent(ev);
}

void ProgrammingManager::hideEvent(QHideEvent *ev)
{
    stopPreview(); // don't leave a scene outputting when you leave the tab
    QWidget::hideEvent(ev);
}

void ProgrammingManager::slotNewScene()
{
    Scene *scene = new Scene(m_doc);
    if (m_doc->addFunction(scene) == false)
    {
        delete scene;
        return;
    }
    scene->setName(tr("New Scene %1").arg(scene->id()));

    // Select it in the tree (which loads the canvas).
    QTreeWidgetItem *it = m_funcTree->functionItem(scene);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
}
