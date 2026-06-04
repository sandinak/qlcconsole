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
#include "fixtureconsole.h"
#include "functionparent.h"
#include "fixture.h"
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
#include "scenevalue.h"
#include "doc.h"

ProgrammingManager::ProgrammingManager(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_canvas(nullptr)
    , m_funcEditor(nullptr)
    , m_currentScene(Function::invalidId())
    , m_canvasFunction(Function::invalidId())
    , m_previewFunction(Function::invalidId())
    , m_clipboardFunction(Function::invalidId())
    , m_memberContainer(Function::invalidId())
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
    m_lookScroll = new QScrollArea(this);
    m_lookScroll->setWidgetResizable(true);
    m_lookScroll->setFrameShape(QFrame::NoFrame);
    m_lookScroll->setWidget(m_lookEditor);
    m_canvasLayout->addWidget(m_lookScroll);

    // Per-fixture DMX channel editor, shown at the bottom when a single
    // fixed-fixture target is selected (instead of the look editor).
    m_fixtureConsole = new FixtureConsole(this, m_doc);
    m_fixtureScroll = new QScrollArea(this);
    m_fixtureScroll->setWidgetResizable(true);
    m_fixtureScroll->setFrameShape(QFrame::NoFrame);
    m_fixtureScroll->setWidget(m_fixtureConsole);
    m_fixtureScroll->hide();
    m_canvasLayout->addWidget(m_fixtureScroll);
    connect(m_fixtureConsole, SIGNAL(valueChanged(quint32,quint32,uchar)),
            this, SLOT(slotFixtureValueChanged(quint32,quint32,uchar)));
    connect(m_fixtureConsole, SIGNAL(checked(quint32,quint32,bool)),
            this, SLOT(slotFixtureChecked(quint32,quint32,bool)));
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
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32) {
        const quint32 c = m_memberContainer;
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
        if (c != Function::invalidId())
            syncMemberNodes(c);   // re-nest members (node is fresh after rebuild)
    });
    connect(m_doc, &Doc::functionChanged, this, [this](quint32 id) {
        m_funcTree->functionNameChanged(id);
        if (id == m_memberContainer)
            syncMemberNodes(m_memberContainer); // membership may have changed
    });
    connect(m_doc, &Doc::loaded, this, [this]() {
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
    });
    connect(m_doc, &Doc::cleared, this, [this]() {
        m_funcTree->updateTree();
        m_memberContainer = Function::invalidId();
    });

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
    m_canvasFunction = sceneId;
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_funcTree->setExternalDragMode(false); // only collections/chasers need it
    // Keep the collection/chaser context (its nested subtree) when drilling
    // into one of ITS member scenes, so you can go back and forth; clear it
    // only when opening an unrelated scene.
    if (isContainerMember(m_memberContainer, sceneId) == false)
        syncMemberNodes(Function::invalidId());
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
    m_fixtureScroll->hide(); // default to the look editor at the bottom
    m_lookScroll->show();
    m_lookEditor->setContextScene(scene);
    m_canvas = new SceneGroupLooks(scene, m_doc, this, /*includeFixtureTargets*/ true);
    connect(m_canvas, SIGNAL(sceneModified()), this, SLOT(slotCanvasModified()));
    connect(m_canvas, SIGNAL(lookSelected(quint32)),
            m_lookEditor, SLOT(setPalette(quint32)));
    connect(m_canvas, SIGNAL(fixtureSelected(quint32)),
            this, SLOT(slotFixtureSelected(quint32)));
    // Selecting a look switches the bottom back to the look editor.
    connect(m_canvas, &SceneGroupLooks::lookSelected, this, [this](quint32 pid) {
        if (pid != QLCPalette::invalidId()) { m_fixtureScroll->hide(); m_lookScroll->show(); }
    });
    // Insert above the bottom editors.
    m_canvasLayout->insertWidget(m_canvasLayout->indexOf(m_lookScroll), m_canvas, 1);

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
    m_canvasFunction = f->id();
    m_lookEditor->setPalette(QLCPalette::invalidId());
    m_lookEditor->setContextScene(NULL);
    clearEditors();
    m_canvasPlaceholder->hide();
    m_fixtureScroll->hide(); // non-scene: no per-fixture editor
    m_lookScroll->show();

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
    // Nest the container's members under its node for quick navigation
    // (add/remove still happens in the hosted editor).
    syncMemberNodes(dragIn ? f->id() : Function::invalidId());

    if (ed == NULL)
    {
        m_canvasPlaceholder->setText(
            tr("\"%1\" (%2) — open it in the Functions tab to edit.")
            .arg(f->name()).arg(Function::typeToString(f->type())));
        m_canvasPlaceholder->show();
        m_canvasFunction = Function::invalidId(); // nothing previewable shown
        updateTitle();
        return;
    }

    m_funcEditor = ed;
    m_canvasLayout->insertWidget(m_canvasLayout->indexOf(m_lookScroll), m_funcEditor, 1);
    m_funcEditor->show();
    startPreview();   // run it live so the view reflects it (Design mode)
    updateTitle();
}

void ProgrammingManager::updateTitle()
{
    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
    {
        m_canvasTitle->setText(tr("Programming"));
        return;
    }
    const QString kind = (f->type() == Function::SceneType)
                         ? tr("scene") : Function::typeToString(f->type());
    const bool live = (m_previewFunction == m_canvasFunction);
    const int uses = functionUsageCount(m_canvasFunction);
    m_canvasTitle->setText(
        tr("Editing %1: %2   —   used in %n place(s)   [ live preview: %3 ]",
           "", uses)
        .arg(kind)
        .arg(f->name())
        .arg(live ? tr("ON") : tr("OFF — Design mode only")));
}

bool ProgrammingManager::isContainerMember(quint32 containerId, quint32 fid) const
{
    if (containerId == Function::invalidId())
        return false;
    Function *c = m_doc->function(containerId);
    if (Collection *col = qobject_cast<Collection*>(c))
        return col->functions().contains(fid);
    if (Chaser *ch = qobject_cast<Chaser*>(c))
    {
        foreach (const ChaserStep &s, ch->steps())
            if (s.fid == fid)
                return true;
    }
    return false;
}

void ProgrammingManager::slotLookEdited()
{
    // A look's value changed in the editor: refresh the preview output.
    refreshPreview();
}

void ProgrammingManager::createPalette(int paletteType)
{
    const QLCPalette::PaletteType type =
        static_cast<QLCPalette::PaletteType>(paletteType);
    QLCPalette *p = new QLCPalette(type);
    p->setName(tr("New %1").arg(QLCPalette::typeToString(type)));

    // Sensible starting value so the palette is valid/saveable.
    switch (type)
    {
    case QLCPalette::Color:   p->setValue(QColor(Qt::white).name()); break;
    case QLCPalette::Dimmer:  p->setValue(255); break;
    case QLCPalette::PanTilt: p->setValue(270, 135); break;
    default:                  p->setValue(0); break; // Gobo / Shutter / …
    }

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
    m_lookEditor->setPalette(p->id()); // edit it inline immediately
}

void ProgrammingManager::slotPaletteDoubleClicked(QTreeWidgetItem *item)
{
    const quint32 pid = m_paletteTree->itemPaletteId(item);
    if (pid != QLCPalette::invalidId())
        m_lookEditor->setPalette(pid);
}

void ProgrammingManager::slotFixtureSelected(quint32 fid)
{
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (fid == Fixture::invalidId() || s == NULL || m_doc->fixture(fid) == NULL)
    {
        m_fixtureScroll->hide();
        m_lookScroll->show();
        return;
    }

    // Show this fixture's channel editor, prefilled with the scene's values.
    m_fixtureConsole->blockSignals(true);
    m_fixtureConsole->setFixture(fid);
    foreach (const SceneValue &scv, s->values())
        if (scv.fxi == fid)
            m_fixtureConsole->setSceneValue(scv);
    m_fixtureConsole->blockSignals(false);

    m_lookScroll->hide();
    m_fixtureScroll->show();
}

void ProgrammingManager::slotFixtureValueChanged(quint32 fxi, quint32 ch, uchar value)
{
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (s == NULL)
        return;
    s->setValue(fxi, ch, value);
    m_doc->setModified();
    refreshPreview();
}

void ProgrammingManager::slotFixtureChecked(quint32 fxi, quint32 ch, bool state)
{
    if (state) // enabling emits a value via valueChanged separately
        return;
    Scene *s = qobject_cast<Scene*>(m_doc->function(m_currentScene));
    if (s == NULL)
        return;
    s->unsetValue(fxi, ch);
    m_doc->setModified();
    refreshPreview();
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
    // Live-preview the canvas function (scene, collection, …) by running it.
    // Design mode only, so we don't hijack a running show.
    if (m_doc->mode() != Doc::Design)
        return;
    if (isVisible() == false)
        return;
    if (m_previewFunction == m_canvasFunction)
        return;

    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
        return;

    f->start(m_doc->masterTimer(), FunctionParent::master());
    m_previewFunction = m_canvasFunction;
    updateTitle();
}

void ProgrammingManager::stopPreview()
{
    if (m_previewFunction == Function::invalidId())
        return;
    Function *f = m_doc->function(m_previewFunction);
    if (f != NULL)
        f->stop(FunctionParent::master());
    m_previewFunction = Function::invalidId();
    updateTitle();
}

void ProgrammingManager::refreshPreview()
{
    // Apply an edit to the live preview WITHOUT a stop/start cycle (rapid
    // start/stop on the MasterTimer thread races and crashes). If the
    // function is already running, just reset its runtime so the next write
    // re-expands values/palettes; otherwise start it (e.g. a scene that
    // auto-stopped while empty, now that it has content).
    if (m_doc->mode() != Doc::Design || isVisible() == false)
        return;
    Function *f = m_doc->function(m_canvasFunction);
    if (f == NULL)
        return;

    if (f->isRunning())
    {
        if (Scene *s = qobject_cast<Scene*>(f))
            s->resetRuntime();
        // collections/others reflect their members' own changes
    }
    else
    {
        f->start(m_doc->masterTimer(), FunctionParent::master());
        m_previewFunction = m_canvasFunction;
        updateTitle();
    }
}

void ProgrammingManager::slotCanvasModified()
{
    // Re-apply the scene's targets/looks to the live preview (resetRuntime
    // if running, else start) — no start/stop churn.
    refreshPreview();
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
    // Type list, like the function tree's New menu. Only the types the
    // engine resolves per-fixture are offered.
    struct { const char *label; int type; } types[] = {
        { QT_TR_NOOP("New Color"),    QLCPalette::Color },
        { QT_TR_NOOP("New Dimmer"),   QLCPalette::Dimmer },
        { QT_TR_NOOP("New Pan/Tilt"), QLCPalette::PanTilt },
        { QT_TR_NOOP("New Gobo"),     QLCPalette::Gobo },
        { QT_TR_NOOP("New Shutter"),  QLCPalette::Shutter },
    };
    QList<QAction*> newActions;
    for (uint i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        QAction *a = menu.addAction(tr(types[i].label));
        a->setData(types[i].type);
        newActions << a;
    }

    QAction *aMove = NULL;
    const quint32 pid =
        m_paletteTree->itemPaletteId(m_paletteTree->itemAt(pos));
    if (pid != QLCPalette::invalidId())
    {
        menu.addSeparator();
        aMove = menu.addAction(tr("Move to folder…"));
    }

    QAction *chosen = menu.exec(m_paletteTree->viewport()->mapToGlobal(pos));
    if (chosen == NULL)
        return;

    if (newActions.contains(chosen))
    {
        createPalette(chosen->data().toInt());
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

void ProgrammingManager::syncMemberNodes(quint32 containerId)
{
    // Remove members previously nested under the old container's node. After
    // a tree rebuild the node is fresh (no children), so this is a no-op then.
    if (m_memberContainer != Function::invalidId())
    {
        Function *old = m_doc->function(m_memberContainer);
        if (old != NULL)
        {
            QTreeWidgetItem *n = m_funcTree->functionItem(old);
            if (n != NULL)
                while (n->childCount() > 0)
                    delete n->takeChild(0);
        }
    }

    m_memberContainer = containerId;

    Function *c = m_doc->function(containerId);
    if (c == NULL)
        return;

    QList<quint32> members;
    if (Collection *col = qobject_cast<Collection*>(c))
        members = col->functions();
    else if (Chaser *ch = qobject_cast<Chaser*>(c))
        foreach (const ChaserStep &s, ch->steps())
            members << s.fid;
    else
        return;

    QTreeWidgetItem *node = m_funcTree->functionItem(c);
    if (node == NULL)
        return;

    foreach (quint32 mid, members)
    {
        Function *mf = m_doc->function(mid);
        if (mf == NULL)
            continue;
        QTreeWidgetItem *ci = new QTreeWidgetItem(node);
        ci->setText(0, mf->name());
        ci->setIcon(0, mf->getIcon());
        ci->setData(0, Qt::UserRole, mid); // so itemFunctionId() resolves it
        // Read-only nav: selectable + double-click to edit, not draggable.
        ci->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    node->setExpanded(true);
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
