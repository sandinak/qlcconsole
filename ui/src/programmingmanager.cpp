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
#include <QShortcut>
#include <QScrollArea>
#include <QMenu>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>

#include "programmingmanager.h"
#include "functionstreewidget.h"
#include "fixturegroupsource.h"
#include "scenegrouplooks.h"
#include "lookeditor.h"
#include "paletteeditdialog.h"
#include "functionparent.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "efx.h"
#include "rgbmatrix.h"
#include "chasereditor.h"
#include "collectioneditor.h"
#include "efxeditor.h"
#include "rgbmatrixeditor.h"
#include "chaserstep.h"
#include "doc.h"

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_funcEditor(nullptr)
    , m_currentScene(Function::invalidId())
    , m_previewScene(Function::invalidId())
    , m_clipboardFunction(Function::invalidId())
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
    m_funcTree->setSortingEnabled(true);
    m_funcTree->sortByColumn(0, Qt::AscendingOrder);
    m_funcTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_funcTree->updateTree();
    navCol->addWidget(m_funcTree, 1);
    navCol->addWidget(new QLabel(tr("Right-click to add a scene/function or folder."), this));
    splitter->addWidget(navPanel);

    // --- Center: the selected scene's canvas ---
    QWidget *canvasPanel = new QWidget(this);
    m_canvasLayout = new QVBoxLayout(canvasPanel);
    m_canvasTitle = new QLabel(this);
    m_canvasTitle->setStyleSheet("font-weight: bold;");
    m_canvasTitle->setWordWrap(true);
    m_canvasLayout->addWidget(m_canvasTitle);
    m_canvasPlaceholder = new QLabel(
        tr("Select or create a scene on the left, then drag palettes, "
           "fixture groups and fixtures from the right onto it."), this);
    m_canvasPlaceholder->setWordWrap(true);
    m_canvasPlaceholder->setAlignment(Qt::AlignCenter);
    m_canvasLayout->addWidget(m_canvasPlaceholder, 1);
    // Inline look editor pinned to the bottom of the center panel, in a
    // scroll area so tall pages (color picker / XY pad) never get clipped.
    m_lookEditor = new LookEditor(m_doc, this);
    QScrollArea *lookScroll = new QScrollArea(this);
    lookScroll->setWidgetResizable(true);
    lookScroll->setFrameShape(QFrame::NoFrame);
    lookScroll->setWidget(m_lookEditor);
    m_canvasLayout->addWidget(lookScroll);
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
    m_paletteTree->setSortingEnabled(true);
    m_paletteTree->sortByColumn(0, Qt::AscendingOrder);
    m_paletteTree->setExternalDragMode(true); // emit palette MIME on drag
    m_paletteTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_paletteTree->updateTree();
    srcCol->addWidget(m_paletteTree, 1);
    srcCol->addWidget(new QLabel(tr("Right-click to add a palette."), this));

    srcCol->addWidget(new QLabel(tr("Fixtures & groups"), this));
    m_fixGroupSource = new FixtureGroupSource(m_doc, this);
    srcCol->addWidget(m_fixGroupSource, 1);

    splitter->addWidget(sourcePanel);

    splitter->setStretchFactor(0, 2); // nav
    splitter->setStretchFactor(1, 3); // canvas
    splitter->setStretchFactor(2, 2); // sources

    // Open on DOUBLE-click only; single-click just selects, so a function
    // can be click-dragged from the tree into a collection/chaser canvas
    // without the canvas switching out from under the drag.
    connect(m_funcTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotFunctionActivated(QTreeWidgetItem*)));
    connect(m_funcTree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotFuncTreeMenu(QPoint)));
    connect(m_paletteTree, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotPaletteTreeMenu(QPoint)));

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
    connect(m_lookEditor, SIGNAL(paletteChanged(quint32)),
            this, SLOT(slotLookEdited()));

    // Double-click a palette in the source tree to edit it inline too.
    connect(m_paletteTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            this, SLOT(slotPaletteDoubleClicked(QTreeWidgetItem*)));

    // Cmd/Ctrl-C then Cmd/Ctrl-V duplicates the selected function.
    QShortcut *copySc = new QShortcut(QKeySequence::Copy, this);
    copySc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copySc, SIGNAL(activated()), this, SLOT(slotCopy()));
    QShortcut *pasteSc = new QShortcut(QKeySequence::Paste, this);
    pasteSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteSc, SIGNAL(activated()), this, SLOT(slotPaste()));
}

ProgrammingManager::~ProgrammingManager()
{
    stopPreview();
}

void ProgrammingManager::slotFunctionActivated(QTreeWidgetItem *item)
{
    if (item == NULL)
        return;

    const quint32 fid = m_funcTree->itemFunctionId(item);
    Function *f = m_doc->function(fid);
    if (f == NULL)
        loadCanvas(Function::invalidId()); // folder / category -> placeholder
    else if (f->type() == Function::SceneType)
        loadCanvas(fid);
    else
        loadFunctionEditor(f);
}

void ProgrammingManager::loadCanvas(quint32 sceneId)
{
    if (sceneId == m_currentScene && m_canvas != NULL)
        return;

    stopPreview();
    m_currentScene = sceneId;
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_funcTree->setExternalDragMode(false); // only collections/chasers need it
    clearEditors();

    Scene *scene = qobject_cast<Scene*>(m_doc->function(sceneId));
    if (scene == NULL)
    {
        m_canvasPlaceholder->setText(
            tr("Select or create a scene on the left, then drag palettes, "
               "fixture groups and fixtures from the right onto it."));
        m_canvasPlaceholder->show();
        updateTitle();
        return;
    }

    m_canvasPlaceholder->hide();
    m_lookEditor->setContextScene(scene);
    m_canvas = new SceneGroupLooks(scene, m_doc, this, /*includeFixtureTargets*/ true);
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    connect(m_canvas, SIGNAL(lookSelected(quint32)),
            m_lookEditor, SLOT(setPalette(quint32)));
    // Insert above the look editor (which is the last item).
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, m_canvas, 1);

    startPreview();
    updateTitle();
}

void ProgrammingManager::clearEditors()
{
    if (m_canvas != NULL)
    {
        m_canvas->deleteLater();
        m_canvas = NULL;
    }
    if (m_funcEditor != NULL)
    {
        m_funcEditor->deleteLater();
        m_funcEditor = NULL;
    }
}

void ProgrammingManager::loadFunctionEditor(Function *f)
{
    if (f == NULL)
        return;

    stopPreview();
    m_currentScene = Function::invalidId();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    clearEditors();
    m_canvasPlaceholder->hide();

    // Host the stock editor for the function type; collections and chasers
    // accept functions dragged from the left tree (external drag mode).
    QWidget *ed = NULL;
    bool dragIn = false;
    switch (f->type())
    {
    case Function::CollectionType:
        ed = new CollectionEditor(this, qobject_cast<Collection*>(f), m_doc);
        dragIn = true;
        break;
    case Function::ChaserType:
        ed = new ChaserEditor(this, qobject_cast<Chaser*>(f), m_doc);
        dragIn = true;
        break;
    case Function::EFXType:
        ed = new EFXEditor(this, qobject_cast<EFX*>(f), m_doc);
        break;
    case Function::RGBMatrixType:
        ed = new RGBMatrixEditor(this, qobject_cast<RGBMatrix*>(f), m_doc);
        break;
    default:
        break;
    }

    m_funcTree->setExternalDragMode(dragIn);

    if (ed == NULL)
    {
        m_canvasPlaceholder->setText(
            tr("\"%1\" (%2) — open it in the Functions tab to edit.")
            .arg(f->name()).arg(Function::typeToString(f->type())));
        m_canvasPlaceholder->show();
        m_canvasTitle->setText(tr("Programming"));
        return;
    }

    m_funcEditor = ed;
    m_canvasLayout->insertWidget(m_canvasLayout->count() - 1, m_funcEditor, 1);
    m_funcEditor->show();
    m_canvasTitle->setText(tr("Editing %1: %2")
                           .arg(Function::typeToString(f->type()))
                           .arg(f->name()));
}

void ProgrammingManager::updateTitle()
{
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (s == NULL)
    {
        m_canvasTitle->setText(tr("Programming"));
        return;
    }
    const bool live = (m_previewScene == m_currentScene);
    const int uses = functionUsageCount(m_currentScene);
    m_canvasTitle->setText(
        tr("Editing scene: %1   —   used in %n place(s)   [ live preview: %2 ]",
           "", uses)
        .arg(s->name())
        .arg(live ? tr("ON") : tr("OFF — Design mode only")));
}

void ProgrammingManager::slotLookEdited()
{
    // A look's value changed in the editor: refresh the preview output.
    stopPreview();
    startPreview();
}

void ProgrammingManager::slotNewPalette()
{
    PaletteEditDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || dlg.result() == NULL)
        return;
    QLCPalette *p = dlg.result();

    // Drop it into the selected palette folder, if any.
    const QList<QTreeWidgetItem*> sel = m_paletteTree->selectedItems();
    if (sel.isEmpty() == false)
    {
        QString cp = sel.first()->text(1); // folder COL_PATH ("Palettes/…")
        if (cp.isEmpty())
        {
            QLCPalette *sp = m_doc->palette(m_paletteTree->itemPaletteId(sel.first()));
            if (sp != NULL)
                cp = sp->path();
        }
        if (cp.isEmpty() == false)
            p->setPath(cp);
    }

    if (m_doc->addPalette(p) == false)
    {
        delete p;
        return;
    }
    m_doc->setModified();
    // Edit it immediately in the inline editor.
    m_lookEditor->setPalette(p->id());
}

void ProgrammingManager::slotPaletteDoubleClicked(QTreeWidgetItem *item)
{
    const quint32 pid = m_paletteTree->itemPaletteId(item);
    if (pid != QLCPalette::invalidId())
        m_lookEditor->setPalette(pid);
}

void ProgrammingManager::slotCopy()
{
    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    if (sel.isEmpty())
        return;
    const quint32 fid = m_funcTree->itemFunctionId(sel.first());
    if (fid != Function::invalidId())
        m_clipboardFunction = fid;
}

void ProgrammingManager::slotPaste()
{
    Function *src = m_doc->function(m_clipboardFunction);
    if (src == NULL)
        return;
    Function *copy = src->createCopy(m_doc);
    if (copy == NULL)
        return;
    copy->setName(m_doc->nextDuplicateName(src));

    QTreeWidgetItem *it = m_funcTree->functionItem(copy);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
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
    updateTitle();
}

void ProgrammingManager::stopPreview()
{
    if (m_previewScene == Function::invalidId())
        return;
    Scene *scene = qobject_cast<Scene*>(m_doc->function(m_previewScene));
    if (scene != NULL)
        scene->stop(FunctionParent::master());
    m_previewScene = Function::invalidId();
    updateTitle();
}

void ProgrammingManager::slotCanvasModified()
{
    // Restart the preview so the scene re-expands its palettes over the
    // current targets from scratch — reliable even when targets/looks were
    // added to a scene that started empty. The DMX/2D view then updates.
    stopPreview();
    startPreview();
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

QString ProgrammingManager::selectedFuncFolderPath() const
{
    const QList<QTreeWidgetItem*> sel = m_funcTree->selectedItems();
    if (sel.isEmpty())
        return QString();
    QTreeWidgetItem *it = sel.first();
    const QString colPath = it->text(1); // COL_PATH
    if (colPath.isEmpty() == false)
        return colPath; // a folder/category is selected
    Function *f = m_doc->function(m_funcTree->itemFunctionId(it));
    return f != NULL ? f->path() : QString();
}

void ProgrammingManager::slotFuncTreeMenu(const QPoint &pos)
{
    QMenu menu(this);
    QAction *aScene  = menu.addAction(tr("New Scene"));
    QAction *aChaser = menu.addAction(tr("New Chaser"));
    QAction *aColl   = menu.addAction(tr("New Collection"));
    QAction *aEFX    = menu.addAction(tr("New EFX"));
    QAction *aMatrix = menu.addAction(tr("New RGB Matrix"));
    menu.addSeparator();
    QAction *aFolder = menu.addAction(tr("New Folder"));

    QAction *chosen = menu.exec(m_funcTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;
    if (chosen == aFolder)
    {
        m_funcTree->addFolder();
        return;
    }

    Function *f = NULL;
    QString base;
    if (chosen == aScene)       { f = new Scene(m_doc);      base = tr("New Scene"); }
    else if (chosen == aChaser) { f = new Chaser(m_doc);     base = tr("New Chaser"); }
    else if (chosen == aColl)   { f = new Collection(m_doc); base = tr("New Collection"); }
    else if (chosen == aEFX)    { f = new EFX(m_doc);        base = tr("New EFX"); }
    else if (chosen == aMatrix) { f = new RGBMatrix(m_doc);  base = tr("New RGB Matrix"); }
    if (f == NULL)
        return;

    const QString folder = selectedFuncFolderPath();
    if (folder.isEmpty() == false)
        f->setPath(folder);

    if (m_doc->addFunction(f) == false)
    {
        delete f;
        return;
    }
    f->setName(QString("%1 %2").arg(base).arg(f->id()));

    QTreeWidgetItem *it = m_funcTree->functionItem(f);
    if (it != NULL)
    {
        m_funcTree->setCurrentItem(it);
        m_funcTree->scrollToItem(it);
    }
    // Selection no longer opens the canvas (double-click does), so open the
    // freshly created function explicitly.
    if (f->type() == Function::SceneType)
        loadCanvas(f->id());
    else
        loadFunctionEditor(f);
}

void ProgrammingManager::slotPaletteTreeMenu(const QPoint &pos)
{
    QMenu menu(this);
    QAction *aNew  = menu.addAction(tr("New palette…"));
    QAction *aMove = NULL;
    const quint32 pid =
        m_paletteTree->itemPaletteId(m_paletteTree->itemAt(pos));
    if (pid != QLCPalette::invalidId())
        aMove = menu.addAction(tr("Move to folder…"));

    QAction *chosen = menu.exec(m_paletteTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (chosen == aNew)
    {
        slotNewPalette();
    }
    else if (chosen == aMove)
    {
        QLCPalette *p = m_doc->palette(pid);
        if (p == NULL)
            return;
        bool ok = false;
        const QString folder = QInputDialog::getText(
            this, tr("Move palette to folder"),
            tr("Folder path (e.g. \"Shutters\"; empty for none):"),
            QLineEdit::Normal, p->path(), &ok);
        if (!ok)
            return;
        QString path = folder.trimmed();
        // Palette paths are stored with the internal "Palettes/" prefix.
        if (path.isEmpty())
            p->setPath(QString());
        else
            p->setPath(QStringLiteral("Palettes/") + path);
        m_doc->setModified();
        m_paletteTree->updateTree();
    }
}

int ProgrammingManager::functionUsageCount(quint32 fid) const
{
    int n = 0;
    foreach (Function *f, m_doc->functions())
    {
        if (f == NULL)
            continue;
        if (Collection *c = qobject_cast<Collection*>(f))
        {
            if (c->functions().contains(fid))
                n++;
        }
        else if (Chaser *ch = qobject_cast<Chaser*>(f))
        {
            foreach (const ChaserStep &s, ch->steps())
                if (s.fid == fid) { n++; break; }
        }
    }
    return n;
}
