/*
  Q Light Controller Plus
  showtreeview.cpp

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

#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTreeWidget>
#include <QInputDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QToolButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QDataStream>
#include <QScrollArea>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSplitter>
#include <QShortcut>
#include <QListWidget>
#include <QKeyEvent>
#include <QSettings>
#include <QMimeData>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QDrag>
#include <algorithm>

#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "showtreeview.h"
#include "effectscriptrunner.h"
#include "lookeditor.h"
#include "scene.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "function.h"
#include "fixture.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

#define SETTINGS_PINNED QStringLiteral("showtree/pinned")
#define SETTINGS_SPLITTER QStringLiteral("showtree/splitter")
#define SETTINGS_COMMANDLINE QStringLiteral("showtree/commandline")

static const int kRecentMax = 10;

/****************************************************************************
 * Mime helpers
 ****************************************************************************/

static QList<quint32> idsFromMime(const QMimeData *mime, const char *type)
{
    QList<quint32> ids;
    if (mime == nullptr || !mime->hasFormat(QLatin1String(type)))
        return ids;
    QByteArray data = mime->data(QLatin1String(type));
    QDataStream stream(&data, QIODevice::ReadOnly);
    while (!stream.atEnd())
    {
        quint32 id;
        stream >> id;
        if (stream.status() != QDataStream::Ok)
            break;
        ids << id;
    }
    return ids;
}

/****************************************************************************
 * The tree: drops onto cues and layers
 ****************************************************************************/

namespace
{

class ShowTreeWidget final : public QTreeWidget
{
public:
    ShowTreeWidget(ShowTreeView *view, QWidget *parent)
        : QTreeWidget(parent), m_view(view)
    {
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        // Cues drag within the tree: onto another cue to move there, with
        // Alt to include it instead.
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
    }

    void setHint(QLabel *hint) { m_hint = hint; }

protected:
    void target(const QPoint &pos, quint32 *cue, quint32 *layer) const
    {
        *cue = ShowTree::invalidId();
        *layer = ShowTree::invalidId();
        QTreeWidgetItem *item = itemAt(pos);
        if (item == nullptr)
            return;
        const int kind = item->data(0, ShowTreeView::KindRole).toInt();
        if (kind == ShowTreeView::CueNode)
            *cue = item->data(0, ShowTreeView::IdRole).toUInt();
        else if (kind == ShowTreeView::LayerNode)
        {
            *cue = item->data(0, ShowTreeView::CueRole).toUInt();
            *layer = item->data(0, ShowTreeView::IdRole).toUInt();
        }
    }

    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        int n = 0;
        for (QTreeWidgetItem *item : items)
        {
            if (item->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::CueNode)
                continue;
            stream << item->data(0, ShowTreeView::IdRole).toUInt();
            ++n;
        }
        if (n == 0)
            return nullptr;
        QMimeData *mime = new QMimeData();
        mime->setData(QLatin1String(ShowTreeView::cueMimeType()), data);
        return mime;
    }

    Qt::DropActions supportedDropActions() const override
    {
        return Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
    }

    void dragEnterEvent(QDragEnterEvent *e) override
    {
        e->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent *e) override
    {
        quint32 cue, layer;
        target(e->position().toPoint(), &cue, &layer);
        if (e->mimeData()->hasFormat(QLatin1String(ShowTreeView::cueMimeType())))
        {
            const QList<quint32> cues = idsFromMime(e->mimeData(), ShowTreeView::cueMimeType());
            const bool include = e->modifiers() & Qt::AltModifier;
            const QString hint = cues.isEmpty() || layer != ShowTree::invalidId() ? QString()
                                 : m_view->cueDropHint(cues.first(), cue, include);
            if (!hint.isEmpty())
            {
                e->setDropAction(Qt::MoveAction);
                e->accept();
            }
            else
            {
                e->ignore();
            }
            if (m_hint != nullptr)
                m_hint->setText(hint);
            return;
        }
        if (cue != ShowTree::invalidId() && m_view->acceptsDrop(cue, layer, e->mimeData()))
        {
            e->acceptProposedAction();
            if (m_hint != nullptr)
                m_hint->setText(m_view->dropHint(cue, layer, e->mimeData(),
                                                 e->modifiers() & Qt::AltModifier));
        }
        else
        {
            e->ignore();
            if (m_hint != nullptr)
                m_hint->setText(QString());
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *e) override
    {
        if (m_hint != nullptr)
            m_hint->setText(QString());
        QTreeWidget::dragLeaveEvent(e);
    }

    void dropEvent(QDropEvent *e) override
    {
        quint32 cue, layer;
        target(e->position().toPoint(), &cue, &layer);
        if (m_hint != nullptr)
            m_hint->setText(QString());
        if (cue == ShowTree::invalidId())
        {
            e->ignore();
            return;
        }
        const QMimeData *mime = e->mimeData();
        if (mime->hasFormat(QLatin1String(ShowTreeView::cueMimeType())))
        {
            const QList<quint32> cues = idsFromMime(mime, ShowTreeView::cueMimeType());
            const bool include = e->modifiers() & Qt::AltModifier;
            // Handled here: never let the base class move the rows itself.
            if (!cues.isEmpty() && layer == ShowTree::invalidId()
                && m_view->dropCue(cues.first(), cue, include))
            {
                e->setDropAction(Qt::CopyAction);
                e->accept();
            }
            else
            {
                e->ignore();
            }
            return;
        }
        const QList<quint32> pals = idsFromMime(mime, FunctionsTreeWidget::paletteDragMimeType());
        const QList<quint32> groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
        const QList<quint32> fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
        const QList<quint32> funcs = idsFromMime(mime, FunctionsTreeWidget::functionDragMimeType());
        bool done = false;
        if (!pals.isEmpty())
            done = m_view->dropPalettes(cue, layer, pals);
        else if ((!groups.isEmpty() || !fixtures.isEmpty()) && layer != ShowTree::invalidId())
            done = m_view->dropTargetsOnLayer(cue, layer, groups, fixtures);
        else if (!groups.isEmpty() || !fixtures.isEmpty())
            done = m_view->dropTargets(cue, groups, fixtures, e->modifiers() & Qt::AltModifier);
        else if (!funcs.isEmpty())
            done = m_view->dropFunctions(cue, funcs);
        if (done)
            e->acceptProposedAction();
        else
            e->ignore();
    }

private:
    ShowTreeView *m_view;
    QLabel *m_hint = nullptr;
};

/* The Library: drags out what it holds in the same formats the Programming
   tab's trees use, so the Look Editor and scenes take them too. */
class LibraryWidget final : public QTreeWidget
{
public:
    explicit LibraryWidget(QWidget *parent) : QTreeWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        QByteArray grp, fx, pal, fn;
        QDataStream gs(&grp, QIODevice::WriteOnly), xs(&fx, QIODevice::WriteOnly),
                    ps(&pal, QIODevice::WriteOnly), fs(&fn, QIODevice::WriteOnly);
        int g = 0, x = 0, p = 0, f = 0;
        for (QTreeWidgetItem *item : items)
        {
            const quint32 id = item->data(0, ShowTreeView::IdRole).toUInt();
            switch (item->data(0, ShowTreeView::KindRole).toInt())
            {
                case ShowTreeView::LibGroup: gs << id; ++g; break;
                case ShowTreeView::LibFixture: xs << id; ++x; break;
                case ShowTreeView::LibPalette: ps << id; ++p; break;
                case ShowTreeView::LibFunction: fs << id; ++f; break;
                default: break;
            }
        }
        if (g + x + p + f == 0)
            return nullptr;
        QMimeData *mime = new QMimeData();
        if (g > 0)
            mime->setData(QLatin1String(FixtureGroupSource::fixtureGroupMimeType()), grp);
        if (x > 0)
            mime->setData(QLatin1String(FixtureGroupSource::fixtureMimeType()), fx);
        if (p > 0)
            mime->setData(QLatin1String(FunctionsTreeWidget::paletteDragMimeType()), pal);
        if (f > 0)
            mime->setData(QLatin1String(FunctionsTreeWidget::functionDragMimeType()), fn);
        return mime;
    }
};

/* The layer page takes palettes dropped anywhere on it. */
class DropPage final : public QWidget
{
public:
    DropPage(ShowTreeView *view, QWidget *parent) : QWidget(parent), m_view(view)
    {
        setAcceptDrops(true);
    }
    quint32 cue = ShowTree::invalidId();
    quint32 layer = ShowTree::invalidId();
    ShowTree::Target target;            //!< a carried-in layer's

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        // Palettes anywhere; groups only onto a layer of the cue's own.
        const bool palettes = e->mimeData()->hasFormat(QLatin1String(FunctionsTreeWidget::paletteDragMimeType()));
        if (cue != ShowTree::invalidId() && (palettes || layer != ShowTree::invalidId())
            && m_view->acceptsDrop(cue, layer, e->mimeData()))
            e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override
    {
        const QList<quint32> pals = idsFromMime(e->mimeData(), FunctionsTreeWidget::paletteDragMimeType());
        const QList<quint32> groups = idsFromMime(e->mimeData(), FixtureGroupSource::fixtureGroupMimeType());
        const QList<quint32> fixtures = idsFromMime(e->mimeData(), FixtureGroupSource::fixtureMimeType());
        if (!pals.isEmpty() && m_view->dropPalettes(cue, layer, pals, target))
            e->acceptProposedAction();
        else if (pals.isEmpty() && (!groups.isEmpty() || !fixtures.isEmpty())
                 && m_view->dropTargetsOnLayer(cue, layer, groups, fixtures))
            e->acceptProposedAction();
    }

private:
    ShowTreeView *m_view;
};

const QStringList &attributeKeys()
{
    static const QStringList keys = { KeyColour, KeyDimmer, KeyPosition, KeyGobo, KeyZoom,
                                      KeyBeam, KeyShutter };
    return keys;
}

QString cueModeLabel(CueMode mode)
{
    switch (mode)
    {
        case CueMode::Track: return ShowTreeView::tr("Track");
        case CueMode::Block: return ShowTreeView::tr("Block");
        case CueMode::CueOnly: return ShowTreeView::tr("Cue-only");
        case CueMode::ListDefault: break;
    }
    return ShowTreeView::tr("List default");
}

} // namespace

/****************************************************************************
 * Construction
 ****************************************************************************/

ShowTreeView::ShowTreeView(QWidget *parent, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_st(doc->showTree())
{
    setObjectName("ShowTreeView");
    setProperty("helpTopic", QStringLiteral("show-tree.md"));

    QVBoxLayout *main = new QVBoxLayout(this);
    main->setContentsMargins(4, 4, 4, 4);

    // Header
    QHBoxLayout *bar = new QHBoxLayout();
    auto button = [this, bar](const QString &text, const QString &tip, const char *name) {
        QToolButton *b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setObjectName(QLatin1String(name));
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        bar->addWidget(b);
        return b;
    };
    m_newProduction = button(tr("+ Production"), tr("A new production: the top of a show's tree"), "stNewProduction");
    m_newList = button(tr("+ Cue list"), tr("A new cue list in the selected production"), "stNewList");
    m_newCue = button(tr("+ Cue"), tr("A new cue after the selected one, numbered for you (never renumbers)"), "stNewCue");
    m_addLayer = button(tr("+ Layer"), tr("A layer on the groups or fixtures selected in the Library "
                                          "(or drag them onto a cue)"), "stAddLayer");
    m_record = button(tr("Record"), tr("Record as new cue: what the selected cue puts on stage (tracked, "
                                       "included and its own) frozen into a new Block cue after it"), "stRecord");
    m_delete = button(tr("Delete"), tr("Delete the selected cue, layer, cue list or production"), "stDelete");
    bar->addSpacing(12);
    m_undo = button(tr("Undo"), tr("Undo the last change to the tree (⌘Z)"), "stUndo");
    m_redo = button(tr("Redo"), tr("Redo (⇧⌘Z)"), "stRedo");
    bar->addSpacing(12);
    m_import = button(tr("Import chaser…"), tr("Import a chaser as a cue list in a new production. "
                                               "The chaser stays as it is."), "stImport");
    m_importMenu = new QMenu(m_import);
    m_import->setMenu(m_importMenu);
    m_import->setPopupMode(QToolButton::InstantPopup);
    connect(m_importMenu, &QMenu::aboutToShow, this, &ShowTreeView::slotImportMenuAboutToShow);
    m_commandToggle = button(tr("Command"), tr("Show or hide the command line: type what you'd say "
                                               "(Towers colour Blue dimmer 45). Optional; the mouse does it all too."),
                             "stCommandToggle");
    m_commandToggle->setCheckable(true);
    bar->addStretch(1);
    // What the rig shows: the selected cue (Design), or nothing (Live).
    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("stPreview");
    bar->addWidget(m_previewLabel);
    bar->addSpacing(12);
    m_hint = new QLabel(this);
    m_hint->setObjectName("stDropHint");
    bar->addWidget(m_hint);
    main->addLayout(bar);

    // The command line (§6C): optional, hidden until asked for.
    m_commander = new ShowTreeCommand(m_doc, m_st);
    m_commandBar = new QWidget(this);
    {
        QVBoxLayout *v = new QVBoxLayout(m_commandBar);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        QHBoxLayout *h = new QHBoxLayout();
        QLabel *lbl = new QLabel(tr("Command"), m_commandBar);
        h->addWidget(lbl);
        m_command = new QLineEdit(m_commandBar);
        m_command->setObjectName("stCommand");
        m_command->setPlaceholderText(tr("e.g. Towers colour Blue dimmer 45"));
        m_command->setToolTip(tr("Type what you'd say. Tab completes names from the Library, Enter runs, "
                                 "↑ recalls. Every command is one undo step."));
        QFont mono(QStringLiteral("Menlo"));
        mono.setStyleHint(QFont::Monospace);
        m_command->setFont(mono);
        m_command->installEventFilter(this);
        h->addWidget(m_command, 1);
        m_commandMsg = new QLabel(m_commandBar);
        m_commandMsg->setObjectName("stCommandMessage");
        h->addWidget(m_commandMsg);
        v->addLayout(h);
        QLabel *hint = new QLabel(ShowTreeCommand::grammar(), m_commandBar);
        hint->setObjectName("stCommandHint");
        hint->setWordWrap(true);
        hint->setEnabled(false);
        v->addWidget(hint);
    }
    main->addWidget(m_commandBar);
    connect(m_command, &QLineEdit::returnPressed, this, &ShowTreeView::slotCommandEntered);
    connect(m_commandToggle, &QToolButton::toggled, this, &ShowTreeView::setCommandLineVisible);
    {
        QSettings settings;
        const bool on = settings.value(SETTINGS_COMMANDLINE, false).toBool();
        m_commandBar->setVisible(on);
        QSignalBlocker block(m_commandToggle);
        m_commandToggle->setChecked(on);
    }

    connect(m_newProduction, &QToolButton::clicked, this, &ShowTreeView::slotNewProduction);
    connect(m_newList, &QToolButton::clicked, this, &ShowTreeView::slotNewCueList);
    connect(m_newCue, &QToolButton::clicked, this, &ShowTreeView::slotNewCue);
    connect(m_addLayer, &QToolButton::clicked, this, &ShowTreeView::slotAddLayer);
    connect(m_record, &QToolButton::clicked, this, &ShowTreeView::slotRecordCue);
    connect(m_delete, &QToolButton::clicked, this, &ShowTreeView::slotDelete);
    connect(m_undo, &QToolButton::clicked, this, &ShowTreeView::slotUndo);
    connect(m_redo, &QToolButton::clicked, this, &ShowTreeView::slotRedo);

    // The tree, inspector and Library above; the Look Editor (Programming's)
    // under them, for the palette of the row being edited (§6, "Bottom").
    QSplitter *vsplit = new QSplitter(Qt::Vertical, this);
    vsplit->setObjectName("stVSplitter");
    main->addWidget(vsplit, 1);
    QSplitter *split = new QSplitter(Qt::Horizontal, vsplit);
    split->setObjectName("stSplitter");
    vsplit->addWidget(split);
    m_lookEditor = new LookEditor(m_doc, vsplit);
    m_lookEditor->setObjectName("stLookEditor");
    vsplit->addWidget(m_lookEditor);
    vsplit->setStretchFactor(0, 3);
    vsplit->setStretchFactor(1, 2);
    m_lookEditor->hide();
    connect(m_lookEditor, &LookEditor::paletteChanged, this, [this](quint32 pid) {
        paletteEdited(pid, true);
    });
    connect(m_lookEditor, &LookEditor::paletteValueChanged, this, [this](quint32 pid) {
        paletteEdited(pid, false);
    });

    // Left: the tree
    ShowTreeWidget *tree = new ShowTreeWidget(this, split);
    tree->setHint(m_hint);
    m_tree = tree;
    m_tree->setObjectName("stTree");
    m_tree->setHeaderLabels({ tr("Show"), tr("") });
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowTreeView::slotTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ShowTreeView::slotTreeContextMenu);
    split->addWidget(m_tree);

    // Centre: the inspector
    m_pages = new QStackedWidget(split);
    m_pages->setObjectName("stInspector");
    m_emptyPage = new QLabel(tr("Select a cue or a layer.\n\nStart with + Production, or import a "
                                "chaser you already have (Import chaser…)."), m_pages);
    m_emptyPage->setAlignment(Qt::AlignCenter);
    m_emptyPage->setWordWrap(true);
    m_pages->addWidget(m_emptyPage);

    // A cue list
    m_listPage = new QWidget(m_pages);
    {
        QFormLayout *f = new QFormLayout(m_listPage);
        m_listTitle = new QLabel(m_listPage);
        QFont bold = m_listTitle->font();
        bold.setBold(true);
        m_listTitle->setFont(bold);
        f->addRow(m_listTitle);
        m_listMode = new QComboBox(m_listPage);
        m_listMode->setObjectName("stListMode");
        m_listMode->addItem(tr("Non-tracking: each cue plays on its own"));
        m_listMode->addItem(tr("Tracking: a cue carries on what it doesn't change"));
        f->addRow(tr("Mode"), m_listMode);
        m_listBase = new QComboBox(m_listPage);
        m_listBase->setObjectName("stListBase");
        m_listBase->setToolTip(tr("The base cue: what a released layer goes back to (else dark)"));
        f->addRow(tr("Base cue"), m_listBase);
        connect(m_listBase, QOverload<int>::of(&QComboBox::activated), this, &ShowTreeView::slotListBaseChanged);
        m_listInfo = new QLabel(m_listPage);
        m_listInfo->setWordWrap(true);
        f->addRow(m_listInfo);
        connect(m_listMode, QOverload<int>::of(&QComboBox::activated), this, &ShowTreeView::slotListModeChanged);
    }
    m_pages->addWidget(m_listPage);

    // A cue (scrolls: its layers and what passes on can be long)
    QScrollArea *cueScroll = new QScrollArea(m_pages);
    cueScroll->setWidgetResizable(true);
    cueScroll->setFrameShape(QFrame::NoFrame);
    m_cuePage = cueScroll;
    {
        QWidget *inner = new QWidget(cueScroll);
        QFormLayout *f = new QFormLayout(inner);
        cueScroll->setWidget(inner);
        m_cueNumber = new QLineEdit(inner);
        m_cueNumber->setObjectName("stCueNumber");
        m_cueName = new QLineEdit(inner);
        m_cueName->setObjectName("stCueName");
        m_cueNote = new QLineEdit(inner);
        m_cueMode = new QComboBox(inner);
        m_cueMode->setObjectName("stCueMode");
        for (CueMode m : { CueMode::ListDefault, CueMode::Track, CueMode::Block, CueMode::CueOnly })
            m_cueMode->addItem(cueModeLabel(m), int(m));
        m_cueMode->setToolTip(tr("Track: starts from the cue before. Block: starts from nothing. "
                                 "Cue-only: its changes don't pass on to the next cue."));
        auto seconds = [inner]() {
            QDoubleSpinBox *s = new QDoubleSpinBox(inner);
            s->setDecimals(2);
            s->setRange(0, 3600);
            s->setSuffix(ShowTreeView::tr(" s"));
            return s;
        };
        m_cueFadeIn = seconds();
        m_cueFadeOut = seconds();
        m_cueHold = seconds();
        m_cueHold->setRange(-1, 36000);
        m_cueHold->setSpecialValueText(tr("∞ — waits for GO"));
        f->addRow(tr("Number"), m_cueNumber);
        f->addRow(tr("Name"), m_cueName);
        f->addRow(tr("Note"), m_cueNote);
        f->addRow(tr("Mode"), m_cueMode);
        m_cueBase = new QCheckBox(tr("The list's base cue (released layers go back to it)"), inner);
        m_cueBase->setObjectName("stCueBase");
        f->addRow(QString(), m_cueBase);
        connect(m_cueBase, &QCheckBox::toggled, this, &ShowTreeView::slotCueBaseToggled);
        {
            // Includes (D8): this cue plays their output under its own,
            // in order -- the later wins.
            QWidget *inc = new QWidget(inner);
            QHBoxLayout *h = new QHBoxLayout(inc);
            h->setContentsMargins(0, 0, 0, 0);
            m_cueIncludes = new QListWidget(inc);
            m_cueIncludes->setObjectName("stCueIncludes");
            m_cueIncludes->setMaximumHeight(60);
            m_cueIncludes->setToolTip(tr("Cues this cue includes: their output, as if you stood in them, "
                                         "under this cue's own. The later in the list wins."));
            h->addWidget(m_cueIncludes, 1);
            QVBoxLayout *b = new QVBoxLayout();
            b->setSpacing(0);
            auto small = [inc, b](const QString &text, const QString &tip, const char *name) {
                QToolButton *t = new QToolButton(inc);
                t->setText(text);
                t->setToolTip(tip);
                t->setObjectName(QLatin1String(name));
                b->addWidget(t);
                return t;
            };
            m_includeAdd = small(tr("+"), tr("Include another cue of this list"), "stIncludeAdd");
            m_includeAdd->setPopupMode(QToolButton::InstantPopup);
            QMenu *addMenu = new QMenu(m_includeAdd);
            m_includeAdd->setMenu(addMenu);
            connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu]() {
                addMenu->clear();
                includesMenu(addMenu, m_pageCue);
            });
            m_includeRemove = small(tr("−"), tr("Stop including the selected cue"), "stIncludeRemove");
            m_includeUp = small(tr("↑"), tr("Earlier: the cues after it win over it"), "stIncludeUp");
            m_includeDown = small(tr("↓"), tr("Later: it wins over the cues before it"), "stIncludeDown");
            b->addStretch(1);
            h->addLayout(b);
            f->addRow(tr("Includes"), inc);
            auto selected = [this]() {
                QListWidgetItem *it = m_cueIncludes->currentItem();
                return it == nullptr ? invalidId() : it->data(Qt::UserRole).toUInt();
            };
            connect(m_includeRemove, &QToolButton::clicked, this, [this, selected]() {
                if (selected() != invalidId())
                    m_st->removeInclude(m_pageCue, selected());
            });
            connect(m_includeUp, &QToolButton::clicked, this, [this, selected]() {
                const quint32 inc = selected();
                if (inc != invalidId())
                    m_st->moveInclude(m_pageCue, inc, m_cueIncludes->currentRow() - 1);
            });
            connect(m_includeDown, &QToolButton::clicked, this, [this, selected]() {
                const quint32 inc = selected();
                if (inc != invalidId())
                    m_st->moveInclude(m_pageCue, inc, m_cueIncludes->currentRow() + 1);
            });
            connect(m_cueIncludes, &QListWidget::currentRowChanged, this, &ShowTreeView::updateIncludeButtons);
        }
        f->addRow(tr("Fade in"), m_cueFadeIn);
        f->addRow(tr("Fade out"), m_cueFadeOut);
        f->addRow(tr("Hold"), m_cueHold);
        m_cueLooks = new QLabel(inner);
        m_cueLooks->setWordWrap(true);
        m_cueLooks->setObjectName("stCueLooks");
        f->addRow(tr("Cue looks"), m_cueLooks);
        m_cueLayers = new QLabel(inner);
        m_cueLayers->setWordWrap(true);
        m_cueLayers->setTextFormat(Qt::RichText);
        f->addRow(tr("Layers"), m_cueLayers);
        m_cueNext = new QLabel(inner);
        m_cueNext->setWordWrap(true);
        f->addRow(tr("Next cue receives"), m_cueNext);
        for (QLineEdit *e : { m_cueNumber, m_cueName, m_cueNote })
            connect(e, &QLineEdit::editingFinished, this, &ShowTreeView::slotCueEdited);
        for (QDoubleSpinBox *s : { m_cueFadeIn, m_cueFadeOut, m_cueHold })
            connect(s, &QDoubleSpinBox::editingFinished, this, &ShowTreeView::slotCueEdited);
        connect(m_cueMode, QOverload<int>::of(&QComboBox::activated), this, &ShowTreeView::slotCueEdited);
    }
    m_pages->addWidget(m_cuePage);

    // A layer: one row per attribute
    DropPage *layerPage = new DropPage(this, m_pages);
    m_layerPage = layerPage;
    m_layerPage->setObjectName("stLayerPage");
    {
        QVBoxLayout *v = new QVBoxLayout(m_layerPage);
        m_layerTitle = new QLabel(m_layerPage);
        QFont bold = m_layerTitle->font();
        bold.setBold(true);
        m_layerTitle->setFont(bold);
        m_layerTitle->setWordWrap(true);
        v->addWidget(m_layerTitle);
        m_layerRelease = new QCheckBox(tr("Release when the cue is left (back to the base cue, else dark)"),
                                       m_layerPage);
        m_layerRelease->setObjectName("stLayerRelease");
        connect(m_layerRelease, &QCheckBox::toggled, this, &ShowTreeView::slotLayerReleaseToggled);
        v->addWidget(m_layerRelease);
        QScrollArea *scroll = new QScrollArea(m_layerPage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        m_rowsHost = new QWidget(scroll);
        m_rows = new QGridLayout(m_rowsHost);
        m_rows->setColumnStretch(1, 1);
        scroll->setWidget(m_rowsHost);
        v->addWidget(scroll, 1);
        QLabel *tip = new QLabel(tr("Drop a palette here (or on the layer in the tree) to set it, a group to add it to the layer. "
                                    "Grey values come from the cue or an earlier cue; ⋯ sets one here."),
                                 m_layerPage);
        tip->setWordWrap(true);
        tip->setEnabled(false);
        v->addWidget(tip);
    }
    m_pages->addWidget(m_layerPage);
    split->addWidget(m_pages);

    // Right: the Library
    QWidget *lib = new QWidget(split);
    {
        QVBoxLayout *v = new QVBoxLayout(lib);
        v->setContentsMargins(0, 0, 0, 0);
        m_filter = new QLineEdit(lib);
        m_filter->setObjectName("stLibraryFilter");
        m_filter->setPlaceholderText(tr("Filter the Library"));
        m_filter->setClearButtonEnabled(true);
        connect(m_filter, &QLineEdit::textChanged, this, &ShowTreeView::slotFilterChanged);
        v->addWidget(m_filter);
        m_library = new LibraryWidget(lib);
        m_library->setObjectName("stLibrary");
        m_library->setHeaderHidden(true);
        m_library->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_library, &QTreeWidget::customContextMenuRequested, this, &ShowTreeView::slotLibraryContextMenu);
        v->addWidget(m_library, 1);
    }
    split->addWidget(lib);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    split->setStretchFactor(2, 2);
    {
        QSettings settings;
        const QVariant state = settings.value(SETTINGS_SPLITTER);
        if (state.isValid())
            split->restoreState(state.toByteArray());
        connect(split, &QSplitter::splitterMoved, this, [split]() {
            QSettings s;
            s.setValue(SETTINGS_SPLITTER, split->saveState());
        });
        for (const QString &entry : settings.value(SETTINGS_PINNED).toStringList())
        {
            const QStringList parts = entry.split(QLatin1Char(':'));
            if (parts.count() == 2)
                m_pinned << qMakePair(parts.at(0).toInt(), parts.at(1).toUInt());
        }
    }

    // Keys: undo/redo and delete, for this view only.
    QShortcut *undo = new QShortcut(QKeySequence::Undo, this);
    undo->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undo, &QShortcut::activated, this, &ShowTreeView::slotUndo);
    QShortcut *redo = new QShortcut(QKeySequence::Redo, this);
    redo->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redo, &QShortcut::activated, this, &ShowTreeView::slotRedo);
    for (const QKeySequence &key : { QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) })
    {
        QShortcut *sc = new QShortcut(key, m_tree);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, &ShowTreeView::slotDelete);
    }

    connect(m_st, &ShowTreeController::changed, this, &ShowTreeView::slotTreeChanged);
    connect(m_st, &ShowTreeController::previewChanged, this, &ShowTreeView::slotPreviewChanged);
    // The Library follows the show's groups and palettes -- once per burst
    // (a show loading adds them one by one).
    m_libraryTimer = new QTimer(this);
    m_libraryTimer->setSingleShot(true);
    m_libraryTimer->setInterval(0);
    connect(m_libraryTimer, &QTimer::timeout, this, &ShowTreeView::reloadLibrary);
    for (auto signal : { &Doc::fixtureGroupAdded, &Doc::fixtureGroupRemoved, &Doc::fixtureGroupChanged,
                         &Doc::paletteAdded, &Doc::paletteRemoved })
        connect(m_doc, signal, m_libraryTimer, QOverload<>::of(&QTimer::start));
    connect(m_doc, &Doc::loaded, this, &ShowTreeView::reload);

    reload();
}

ShowTreeView::~ShowTreeView()
{
    delete m_commander;
}

/****************************************************************************
 * The tree
 ****************************************************************************/

void ShowTreeView::slotTreeChanged()
{
    if (m_reloading)
        return;
    buildTree();
    updateInspector();
    updateButtons();
    // "In this cue list" counts what the list uses.
    m_libraryTimer->start();

    // The re-bake chain as it came out: which cues play something else now.
    QStringList cues;
    for (quint32 id : m_st->lastBakeChanged())
        if (const CueList *l = m_st->listOfCue(id))
            cues << m_st->cueLabel(*l, l->indexOf(id));
    if (cues.count() > 6)
        cues = cues.mid(0, 6) << tr("+%1 more").arg(cues.count() - 6);
    m_hint->setText(cues.isEmpty() ? QString() : tr("Changed: %1").arg(cues.join(QStringLiteral(", "))));
    m_hint->setToolTip(cues.isEmpty() ? QString()
                       : tr("The cues the last change re-baked: that cue, the ones after it that track "
                            "from it (up to a Block cue), and every cue that includes one of them."));
}

void ShowTreeView::reload()
{
    buildTree();
    reloadLibrary();
    updateInspector();
    updateButtons();
}

void ShowTreeView::saveExpanded()
{
    m_expanded.clear();
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->isExpanded())
            m_expanded << (*it)->data(0, IdRole).toUInt();
}

QTreeWidgetItem *ShowTreeView::findItem(quint32 id, quint32 cueId) const
{
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        if (item->data(0, IdRole).toUInt() != id)
            continue;
        const int kind = item->data(0, KindRole).toInt();
        if (kind == LayerNode && cueId != invalidId() && item->data(0, CueRole).toUInt() != cueId)
            continue;
        if (kind == TrackedLayerNode)
            continue;
        return item;
    }
    return nullptr;
}

void ShowTreeView::buildTree()
{
    m_reloading = true;
    const quint32 selId = currentId();
    const quint32 selCue = currentCueId();
    // A carried-in layer is found again by its cue and target (it has no
    // id of its own there); made the cue's own, by its layer there.
    const bool selTracked = currentKind() == TrackedLayerNode;
    const QString selTarget = selTracked ? m_tree->currentItem()->data(0, TargetRole).toString() : QString();
    const bool first = m_tree->topLevelItemCount() == 0;
    if (!first)
        saveExpanded();

    m_tree->clear();
    QFont boldFont = m_tree->font();
    boldFont.setBold(true);
    const QBrush grey = palette().brush(QPalette::Disabled, QPalette::Text);

    for (const Production &prod : m_st->productions())
    {
        QTreeWidgetItem *pi = new QTreeWidgetItem(m_tree);
        pi->setText(0, prod.name);
        pi->setFont(0, boldFont);
        pi->setData(0, KindRole, ProductionNode);
        pi->setData(0, IdRole, prod.id);

        for (const CueList &list : prod.lists)
        {
            QTreeWidgetItem *li = new QTreeWidgetItem(pi);
            li->setText(0, list.name);
            li->setText(1, list.mode == ListMode::Tracking ? tr("Tracking") : tr("Non-tracking"));
            li->setData(0, KindRole, ListNode);
            li->setData(0, IdRole, list.id);
            const QList<ResolvedCue> resolved = resolve(list);

            for (int i = 0; i < list.cues.count(); ++i)
            {
                const Cue &cue = list.cues.at(i);
                QTreeWidgetItem *ci = new QTreeWidgetItem(li);
                ci->setText(0, m_st->cueLabel(list, i));
                QStringList badges;
                if (list.baseCue == cue.id)
                    badges << tr("■ base");
                const CueMode mode = effectiveMode(list, cue);
                if (cue.mode != CueMode::ListDefault || list.mode == ListMode::Tracking)
                {
                    if (mode == CueMode::Block && list.mode == ListMode::Tracking)
                        badges << tr("⊘ block");
                    else if (mode == CueMode::CueOnly)
                        badges << tr("◐ cue-only");
                }
                if (!cue.includes.isEmpty())
                    badges << tr("includes %1").arg(cue.includes.count());
                ci->setText(1, badges.join(QStringLiteral(" ")));
                ci->setToolTip(0, cue.note);
                ci->setData(0, KindRole, CueNode);
                ci->setData(0, IdRole, cue.id);

                for (const Layer &layer : cue.layers)
                {
                    QTreeWidgetItem *yi = new QTreeWidgetItem(ci);
                    QString name = layer.name.isEmpty() ? m_st->targetLabel(layer.target) : layer.name;
                    if (layer.target.isOpaque())
                        name = tr("▣ %1").arg(name);
                    yi->setText(0, name);
                    QStringList looks;
                    for (const Look &l : layer.looks)
                        looks << lookText(l);
                    yi->setText(1, layer.release ? tr("release") : QString());
                    yi->setToolTip(0, looks.join(QStringLiteral(", ")));
                    yi->setData(0, KindRole, LayerNode);
                    yi->setData(0, IdRole, layer.id);
                    yi->setData(0, CueRole, cue.id);
                }
                // What else is on stage in this cue: tracked, included, base.
                if (i < resolved.count())
                {
                    for (const ResolvedLayer &rl : resolved.at(i).layers)
                    {
                        if (rl.own)
                            continue;
                        QTreeWidgetItem *ti = new QTreeWidgetItem(ci);
                        ti->setText(0, QStringLiteral("⤳ %1").arg(rl.name.isEmpty()
                                                                   ? m_st->targetLabel(rl.target) : rl.name));
                        const int from = list.indexOf(rl.fromCue);
                        const QString src = from < 0 ? QString() : m_st->cueLabel(list, from);
                        ti->setText(1, rl.origin == Origin::Included ? tr("included from %1").arg(src)
                                       : rl.origin == Origin::Base ? tr("base")
                                       : tr("tracked from %1").arg(src));
                        ti->setForeground(0, grey);
                        ti->setForeground(1, grey);
                        ti->setData(0, KindRole, TrackedLayerNode);
                        ti->setData(0, IdRole, rl.layerId);
                        ti->setData(0, CueRole, cue.id);
                        ti->setData(0, TargetRole, rl.target.signature());
                    }
                }
            }
        }
    }

    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        const int kind = item->data(0, KindRole).toInt();
        if (first)
            item->setExpanded(kind == ProductionNode || kind == ListNode);
        else
            item->setExpanded(m_expanded.contains(item->data(0, IdRole).toUInt())
                              && kind != TrackedLayerNode);
    }

    if (selTracked)
    {
        QTreeWidgetItem *found = nullptr;
        for (QTreeWidgetItemIterator it(m_tree); *it && found == nullptr; ++it)
        {
            const int kind = (*it)->data(0, KindRole).toInt();
            if ((kind != TrackedLayerNode && kind != LayerNode) || (*it)->data(0, CueRole).toUInt() != selCue)
                continue;
            if (kind == TrackedLayerNode && (*it)->data(0, TargetRole).toString() == selTarget)
                found = *it;
            if (kind == LayerNode)
                if (const Cue *c = m_st->cue(selCue))
                    if (const Layer *l = c->layer((*it)->data(0, IdRole).toUInt()))
                        if (l->target.signature() == selTarget)
                            found = *it;
        }
        if (found == nullptr)
            found = findItem(selCue, invalidId());
        if (found != nullptr)
            m_tree->setCurrentItem(found);
    }
    else if (selId != invalidId())
    {
        if (QTreeWidgetItem *item = findItem(selId, selCue))
            m_tree->setCurrentItem(item);
    }
    m_reloading = false;
}

bool ShowTreeView::selectNode(quint32 id, quint32 cueId)
{
    QTreeWidgetItem *item = findItem(id, cueId);
    if (item == nullptr)
        return false;
    for (QTreeWidgetItem *p = item->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    m_tree->setCurrentItem(item);
    return true;
}

ShowTreeView::NodeKind ShowTreeView::currentKind() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item == nullptr ? NoNode : NodeKind(item->data(0, KindRole).toInt());
}

quint32 ShowTreeView::currentId() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item == nullptr ? invalidId() : item->data(0, IdRole).toUInt();
}

quint32 ShowTreeView::currentCueId() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (item == nullptr)
        return invalidId();
    const int kind = item->data(0, KindRole).toInt();
    if (kind == CueNode)
        return item->data(0, IdRole).toUInt();
    if (kind == LayerNode || kind == TrackedLayerNode)
        return item->data(0, CueRole).toUInt();
    return invalidId();
}

void ShowTreeView::slotTreeSelectionChanged()
{
    if (m_reloading)
        return;
    updateInspector();
    updateButtons();
    syncPreview();
    if (currentListId() != m_libraryList)
        m_libraryTimer->start();
}

quint32 ShowTreeView::currentListId() const
{
    if (currentKind() == ListNode)
        return currentId();
    const quint32 cueId = currentCueId();
    const CueList *list = cueId == invalidId() ? nullptr : m_st->listOfCue(cueId);
    return list == nullptr ? invalidId() : list->id;
}

/****************************************************************************
 * Preview
 ****************************************************************************/

void ShowTreeView::showEvent(QShowEvent *ev)
{
    QWidget::showEvent(ev);
    syncPreview();
}

void ShowTreeView::hideEvent(QHideEvent *ev)
{
    // Don't leave a cue on the rig when the tab is left.
    m_st->stopPreview();
    QWidget::hideEvent(ev);
}

void ShowTreeView::syncPreview()
{
    // A cue, or a layer in one, plays that cue; the controller decides
    // whether it may (Design mode, its list not running for real).
    const quint32 cueId = currentCueId();
    if (!isVisible() || cueId == invalidId())
        m_st->stopPreview();
    else
        m_st->previewCue(cueId);
    slotPreviewChanged();
}

QString ShowTreeView::previewText() const
{
    const quint32 cueId = m_st->previewedCue();
    const CueList *list = cueId == invalidId() ? nullptr : m_st->listOfCue(cueId);
    const QString cue = list == nullptr ? QString() : m_st->cueLabel(*list, list->indexOf(cueId));
    switch (m_st->previewState())
    {
        case ShowTreeController::PreviewState::Playing:
        {
            // Design: the edit wins over a list running for real, which
            // keeps going underneath and comes back when the preview stops.
            const QStringList running = m_st->listsRunning();
            if (!running.isEmpty())
                return tr("▶ On the rig: %1, over %2 running").arg(cue, running.join(", "));
            return tr("▶ On the rig: %1").arg(cue);
        }
        case ShowTreeController::PreviewState::Dark:
            return tr("▶ On the rig: %1 (plays nothing yet)").arg(cue);
        case ShowTreeController::PreviewState::NotDesign:
            return tr("Live: editing is blind, changes reach the stage on GO");
        case ShowTreeController::PreviewState::Off:
        break;
    }
    return QString();
}

void ShowTreeView::slotPreviewChanged()
{
    m_previewLabel->setText(previewText());
    m_previewLabel->setToolTip(m_st->previewState() == ShowTreeController::PreviewState::Playing
                               ? tr("Design mode: the selected cue plays on the rig, and changes show at "
                                    "once, also over a cue list running for real (it keeps going "
                                    "underneath). Leaving the tab or selecting no cue stops it.")
                               : QString());
}

void ShowTreeView::updateButtons()
{
    const NodeKind kind = currentKind();
    m_newList->setEnabled(kind != NoNode);
    m_newCue->setEnabled(kind == ListNode || kind == CueNode || kind == LayerNode
                         || kind == TrackedLayerNode);
    m_addLayer->setEnabled(currentCueId() != invalidId());
    m_record->setEnabled(currentCueId() != invalidId());
    m_delete->setEnabled(kind != NoNode && kind != TrackedLayerNode);
    m_undo->setEnabled(m_st->canUndo());
    m_redo->setEnabled(m_st->canRedo());
    m_undo->setToolTip(m_st->canUndo() ? tr("Undo %1 (⌘Z)").arg(m_st->undoText()) : tr("Nothing to undo"));
    m_redo->setToolTip(m_st->canRedo() ? tr("Redo %1 (⇧⌘Z)").arg(m_st->redoText()) : tr("Nothing to redo"));
}

/****************************************************************************
 * Text
 ****************************************************************************/

QString ShowTreeView::paletteName(quint32 id) const
{
    const QLCPalette *p = m_doc->palette(id);
    if (p == nullptr)
        return tr("(missing)");
    return p->name().isEmpty() ? tr("(unnamed)") : p->name();
}

QString ShowTreeView::lookText(const Look &look) const
{
    if (look.none)
        return tr("%1 none").arg(keyLabel(look.key));
    QStringList names;
    for (quint32 p : look.palettes)
        names << paletteName(p);
    QString text = names.join(QStringLiteral(" + "));
    if (!look.own.isEmpty())
    {
        QStringList own;
        for (quint32 p : look.own)
            own << paletteName(p);
        text += tr(" in %1").arg(own.join(QStringLiteral(" + ")));
    }
    return text;
}

QString ShowTreeView::originText(const CueList &list, const ResolvedValue &value) const
{
    const int from = list.indexOf(value.fromCue);
    const QString cue = from < 0 ? QString() : m_st->cueLabel(list, from);
    switch (value.origin)
    {
        case Origin::Own: return QString();
        case Origin::CueLook: return tr("from cue %1").arg(cue);
        case Origin::Tracked: return tr("tracked from %1").arg(cue);
        case Origin::Included: return tr("included from %1").arg(cue);
        case Origin::Base: return tr("base, from %1").arg(cue);
    }
    return QString();
}

/****************************************************************************
 * Inspector
 ****************************************************************************/

void ShowTreeView::updateInspector()
{
    m_inspectorUpdating = true;
    m_pageCue = invalidId();
    const NodeKind kind = currentKind();
    const quint32 id = currentId();

    if (kind == ListNode)
    {
        const CueList *list = m_st->cueList(id);
        if (list != nullptr)
        {
            m_listTitle->setText(list->name);
            m_listMode->setCurrentIndex(list->mode == ListMode::Tracking ? 1 : 0);
            m_listBase->clear();
            m_listBase->addItem(tr("None: released layers go dark"), invalidId());
            for (int i = 0; i < list->cues.count(); ++i)
                m_listBase->addItem(m_st->cueLabel(*list, i), list->cues.at(i).id);
            m_listBase->setCurrentIndex(qMax(0, m_listBase->findData(list->baseCue)));
            const Chaser *c = qobject_cast<const Chaser *>(m_doc->function(list->chaser));
            m_listInfo->setText(tr("%1 cues. Plays as the chaser \"%2\": pick it in a VC Cue List, "
                                   "or GO it from the footer.").arg(list->cues.count())
                                .arg(c == nullptr ? QString() : c->name())
                                + QStringLiteral("\n\n")
                                + (list->mode == ListMode::Tracking
                                   ? tr("Tracking: a cue starts from the one before and changes only what it "
                                        "sets; values carried on show grey with where they came from. Block "
                                        "starts a cue from nothing, Cue-only keeps its changes to itself.")
                                   : tr("Non-tracking: every cue plays only its own layers and includes, as "
                                        "a chaser does. Make it Tracking to carry values on.")));
            m_pages->setCurrentWidget(m_listPage);
            m_inspectorUpdating = false;
            return;
        }
    }

    const quint32 cueId = currentCueId();
    const CueList *list = cueId == invalidId() ? nullptr : m_st->listOfCue(cueId);
    if (list == nullptr)
    {
        m_pages->setCurrentWidget(m_emptyPage);
        m_inspectorUpdating = false;
        return;
    }
    const int index = list->indexOf(cueId);

    if (kind == CueNode)
    {
        showCuePage(*list, index);
    }
    else
    {
        const QList<ResolvedCue> resolved = resolve(*list);
        const Cue &cue = list->cues.at(index);
        const Layer *own = kind == LayerNode ? cue.layer(id) : nullptr;
        const ResolvedLayer *rl = nullptr;
        if (own != nullptr)
            rl = resolved.at(index).layer(own->target);
        else
        {
            const QString sig = m_tree->currentItem()->data(0, TargetRole).toString();
            for (const ResolvedLayer &l : resolved.at(index).layers)
                if (l.target.signature() == sig)
                    rl = &l;
        }
        showLayerPage(*list, index, own, rl);
    }
    m_inspectorUpdating = false;
}

void ShowTreeView::showCuePage(const CueList &list, int index)
{
    const Cue &cue = list.cues.at(index);
    m_pageCue = cue.id;
    m_cueNumber->setText(cue.number);
    m_cueNumber->setPlaceholderText(QString::number(index + 1));
    m_cueName->setText(cue.name);
    m_cueNote->setText(cue.note);
    m_cueMode->setCurrentIndex(m_cueMode->findData(int(cue.mode)));
    m_cueMode->setItemText(0, tr("List default (%1)").arg(list.mode == ListMode::Tracking ? tr("Track")
                                                                                           : tr("Block")));
    m_cueFadeIn->setValue(cue.fadeIn / 1000.0);
    m_cueFadeOut->setValue(cue.fadeOut / 1000.0);
    m_cueHold->setValue(int(cue.hold) < 0 ? -1 : cue.hold / 1000.0);
    {
        QSignalBlocker block(m_cueBase);
        m_cueBase->setChecked(list.baseCue == cue.id);
    }

    // Includes, in order (the later wins); the selected one kept.
    {
        QListWidgetItem *was = m_cueIncludes->currentItem();
        const quint32 keep = was == nullptr ? invalidId() : was->data(Qt::UserRole).toUInt();
        QSignalBlocker block(m_cueIncludes);
        m_cueIncludes->clear();
        for (quint32 inc : cue.includes)
        {
            const int at = list.indexOf(inc);
            if (at < 0)
                continue;
            QListWidgetItem *it = new QListWidgetItem(m_st->cueLabel(list, at), m_cueIncludes);
            it->setData(Qt::UserRole, inc);
            if (inc == keep)
                m_cueIncludes->setCurrentItem(it);
        }
    }
    updateIncludeButtons();

    QStringList looks;
    for (const Look &l : cue.looks)
        looks << QStringLiteral("%1: %2").arg(keyLabel(l.key), lookText(l));
    m_cueLooks->setText(looks.isEmpty() ? tr("— (drop a palette on the cue)") : looks.join(QStringLiteral("\n")));

    const QList<ResolvedCue> resolved = resolve(list);
    QStringList layers;
    for (const ResolvedLayer &rl : resolved.at(index).layers)
    {
        const QString name = (rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name).toHtmlEscaped();
        QStringList vals;
        for (const Look &l : rl.playedLooks())
            vals << lookText(l).toHtmlEscaped();
        const QString line = QStringLiteral("%1 &nbsp;%2").arg(name, vals.join(QStringLiteral(", ")));
        if (rl.own)
            layers << line + (rl.release ? tr(" · <i>release</i>") : QString());
        else
        {
            const int from = list.indexOf(rl.fromCue);
            layers << QStringLiteral("<span style='color:gray'>%1 · %2</span>")
                      .arg(line, (rl.origin == Origin::Included ? tr("included from %1")
                                  : rl.origin == Origin::Base ? tr("base, from %1")
                                  : tr("tracked from %1"))
                                 .arg(from < 0 ? QString() : m_st->cueLabel(list, from)).toHtmlEscaped());
        }
    }
    m_cueLayers->setText(layers.isEmpty() ? tr("— (drag groups onto the cue)") : layers.join(QStringLiteral("<br>")));
    m_cueNext->setText(nextCueText());
    m_pages->setCurrentWidget(m_cuePage);
}

QString ShowTreeView::nextCueText() const
{
    const CueList *list = m_pageCue == invalidId() ? nullptr : m_st->listOfCue(m_pageCue);
    if (list == nullptr)
        return QString();
    const int index = list->indexOf(m_pageCue);
    if (index + 1 >= list->cues.count())
        return tr("— (the last cue)");
    const Cue &next = list->cues.at(index + 1);
    const QString nextLabel = m_st->cueLabel(*list, index + 1);
    if (effectiveMode(*list, next) == CueMode::Block)
        return tr("nothing: %1 is a Block cue, it starts from nothing").arg(nextLabel);

    const QList<ResolvedCue> resolved = resolve(*list);
    QStringList parts;
    for (const ResolvedLayer &rl : resolved.at(index).passOn)
    {
        const QString name = rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name;
        QStringList vals;
        for (const Look &l : rl.playedLooks())
            vals << lookText(l);
        QString part = vals.isEmpty() ? name : tr("%1 (%2)").arg(name, vals.join(QStringLiteral(", ")));
        if (rl.origin == Origin::Base)
            part = tr("%1 — back to the base").arg(part);
        parts << part;
    }
    // Released here and not covered by the base: dark.
    const Cue &cue = list->cues.at(index);
    if (effectiveMode(*list, cue) != CueMode::CueOnly)
        for (const Layer &l : cue.layers)
        {
            if (!l.release)
                continue;
            bool back = false;
            for (const ResolvedLayer &rl : resolved.at(index).passOn)
                if (rl.target == l.target)
                    back = true;
            if (!back)
                parts << tr("%1 — released, dark").arg(l.name.isEmpty() ? m_st->targetLabel(l.target) : l.name);
        }
    QString text = parts.isEmpty() ? tr("nothing") : parts.join(QStringLiteral("; "));
    if (effectiveMode(*list, cue) == CueMode::CueOnly)
        text = tr("what this cue started from (it is cue-only): %1").arg(text);
    return text;
}

QStringList ShowTreeView::includeItems() const
{
    QStringList out;
    for (int i = 0; i < m_cueIncludes->count(); ++i)
        out << m_cueIncludes->item(i)->text();
    return out;
}

void ShowTreeView::updateIncludeButtons()
{
    const int row = m_cueIncludes->currentRow();
    m_includeRemove->setEnabled(row >= 0);
    m_includeUp->setEnabled(row > 0);
    m_includeDown->setEnabled(row >= 0 && row + 1 < m_cueIncludes->count());
}

void ShowTreeView::includesMenu(QMenu *menu, quint32 cueId)
{
    // Every other cue of the list: ticked if included; one that would loop
    // is there but greyed, and says why.
    const CueList *list = m_st->listOfCue(cueId);
    const Cue *cue = list == nullptr ? nullptr : list->cue(cueId);
    if (cue == nullptr)
        return;
    for (int i = 0; i < list->cues.count(); ++i)
    {
        const quint32 other = list->cues.at(i).id;
        if (other == cueId)
            continue;
        QAction *a = menu->addAction(m_st->cueLabel(*list, i));
        a->setCheckable(true);
        const bool on = cue->includes.contains(other);
        a->setChecked(on);
        QString why;
        if (!on && !canInclude(*list, cueId, other, &why))
        {
            a->setEnabled(false);
            a->setToolTip(why);
            a->setText(tr("%1 — would loop").arg(a->text()));
        }
        connect(a, &QAction::toggled, this, [this, cueId, other](bool checked) {
            if (checked)
                m_st->addInclude(cueId, other);
            else
                m_st->removeInclude(cueId, other);
        });
    }
    menu->setToolTipsVisible(true);
}

void ShowTreeView::showLayerPage(const CueList &list, int index, const Layer *own, const ResolvedLayer *rl)
{
    const Cue &cue = list.cues.at(index);
    DropPage *page = static_cast<DropPage *>(m_layerPage);
    page->cue = cue.id;
    page->layer = own == nullptr ? invalidId() : own->id;
    page->target = rl == nullptr ? Target() : rl->target;

    // Clear the rows. Later, not now: a row's own ⋯ may be what is being
    // clicked (its menu's action edits the tree, which rebuilds this page).
    while (QLayoutItem *item = m_rows->takeAt(0))
    {
        if (QWidget *w = item->widget())
        {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    m_rowText.clear();

    if (rl == nullptr)
    {
        m_layerTitle->setText(tr("(not on stage in this cue)"));
        m_layerRelease->setVisible(false);
        m_pages->setCurrentWidget(m_layerPage);
        return;
    }

    const QString name = rl->name.isEmpty() ? m_st->targetLabel(rl->target) : rl->name;
    if (own == nullptr)
    {
        const int from = list.indexOf(rl->fromCue);
        m_layerTitle->setText(tr("%1 in cue %2 — %3. Drop a palette here, or use ⋯, to set it in this cue.")
                              .arg(name, m_st->cueLabel(list, index),
                                   rl->origin == Origin::Included ? tr("included from %1").arg(m_st->cueLabel(list, from))
                                   : rl->origin == Origin::Base ? tr("back to the base")
                                   : tr("tracked from %1").arg(m_st->cueLabel(list, from))));
    }
    else
    {
        m_layerTitle->setText(tr("%1 in cue %2").arg(name, m_st->cueLabel(list, index)));
    }
    // Release: on the cue's own layer, or on a carried-in one (which then
    // becomes the cue's own, in the same undo step).
    m_layerRelease->setVisible(!rl->target.isOpaque());
    m_layerReleaseTarget = own == nullptr ? rl->target : Target();
    {
        QSignalBlocker block(m_layerRelease);
        m_layerRelease->setChecked(own != nullptr && own->release);
    }

    if (rl->target.isOpaque())
    {
        QLabel *l = new QLabel(tr("Plays the function \"%1\" as it is.").arg(m_st->targetLabel(rl->target)),
                               m_rowsHost);
        l->setWordWrap(true);
        m_rows->addWidget(l, 0, 0, 1, 5);
        m_rowText << l->text();
        m_pages->setCurrentWidget(m_layerPage);
        return;
    }

    QStringList keys = attributeKeys();
    for (const QString &k : rl->values.keys())
        if (!keys.contains(k))
            keys << k;
    if (own != nullptr)
        for (const Look &l : own->looks)
            if (!keys.contains(l.key))
                keys << l.key;
    std::sort(keys.begin(), keys.end(), keyLessThan);
    if (!keys.contains(effectKey(QStringLiteral("Other"))))
        keys << QStringLiteral("Effect.");          // "an effect…" row to add one

    const QBrush grey = palette().brush(QPalette::Disabled, QPalette::Text);
    int row = 0;
    for (const QString &key : keys)
    {
        const bool addEffect = key == QLatin1String("Effect.");
        QLabel *label = new QLabel(addEffect ? tr("Effect") : keyLabel(key), m_rowsHost);
        if (key.startsWith(QLatin1String("Beam.")))
            label->setText(QStringLiteral("   ") + label->text());     // Beam's children
        QLabel *value = new QLabel(m_rowsHost);
        value->setObjectName(QStringLiteral("stRow:%1").arg(key));
        QString text, origin;
        if (!addEffect && rl->values.contains(key))
        {
            const ResolvedValue &v = rl->values.value(key);
            text = lookText(v.look);
            origin = originText(list, v);
            if (v.origin != Origin::Own)
            {
                QPalette pal = value->palette();
                pal.setBrush(QPalette::WindowText, grey);
                value->setPalette(pal);
                QFont f = value->font();
                f.setItalic(true);
                value->setFont(f);
            }
        }
        else
        {
            text = QStringLiteral("—");
        }
        value->setText(origin.isEmpty() ? text : QStringLiteral("%1 — %2").arg(text, origin));
        value->setWordWrap(true);

        QLabel *reach = new QLabel(m_rowsHost);
        reach->setObjectName(QStringLiteral("stReach:%1").arg(key));
        if (!addEffect && !isEffectKey(key))
        {
            // What it applies to: the value itself where there is one (a
            // Gobo by name reaches only the wheels that have that gobo).
            Look probe;
            probe.key = key;
            if (rl->values.contains(key))
                probe = rl->values.value(key).look;
            int n = 0, total = 0;
            m_st->lookReach(rl->target, probe, &n, &total);
            if (total > 0 && n < total)
            {
                QStringList names;
                for (quint32 fid : m_st->lookMisses(rl->target, probe))
                    if (const Fixture *f = m_doc->fixture(fid))
                        names << f->name();
                if (names.count() > 12)
                    names = names.mid(0, 12) << tr("…and %1 more").arg(names.count() - 12);
                QString tip = tr("Without it (they ignore this look):\n%1").arg(names.join(QLatin1Char('\n')));
                // A gobo name that fits several slots on a type isn't
                // guessed: say so, and offer the per-type pick.
                const QStringList ambiguous = m_st->lookAmbiguities(rl->target, probe);
                const quint32 pickPid = probe.palettes.value(0, QLCPalette::invalidId());
                if (!ambiguous.isEmpty() && pickPid != QLCPalette::invalidId())
                {
                    reach->setText(tr("applies to %1 of %2 — ambiguous: <a href=\"pick\">pick per type…</a>")
                                   .arg(n).arg(total));
                    reach->setTextFormat(Qt::RichText);
                    connect(reach, &QLabel::linkActivated, this, [this, pickPid]() { editPalette(pickPid); });
                    tip += tr("\n\nThe name fits more than one gobo on:\n%1\nPick the one it means for "
                              "that type in the Look Editor (\"Wheel of\").")
                           .arg(ambiguous.join(QLatin1Char('\n')));
                }
                else
                {
                    reach->setText(tr("applies to %1 of %2").arg(n).arg(total));
                }
                reach->setToolTip(tip);
            }
        }

        // ✎ opens the value's palette in the Look Editor below.
        QToolButton *editBtn = new QToolButton(m_rowsHost);
        editBtn->setText(QStringLiteral("✎"));
        quint32 editPid = QLCPalette::invalidId();
        if (!addEffect && rl->values.contains(key) && !rl->values.value(key).look.palettes.isEmpty())
            editPid = rl->values.value(key).look.palettes.first();
        editBtn->setEnabled(editPid != QLCPalette::invalidId());
        editBtn->setToolTip(tr("Edit this value in the Look Editor"));
        connect(editBtn, &QToolButton::clicked, this, [this, editPid]() { editPalette(editPid); });

        QToolButton *more = new QToolButton(m_rowsHost);
        more->setText(QStringLiteral("⋯"));
        more->setToolTip(tr("Set, clear or turn off %1 here").arg(label->text().trimmed()));
        const quint32 cueId = cue.id;
        const quint32 layerId = own == nullptr ? invalidId() : own->id;
        const Target target = rl->target;
        connect(more, &QToolButton::clicked, this, [this, cueId, layerId, key, more, target]() {
            // A carried-in layer (no layer id) becomes the cue's own when a
            // value is set on it, in the same undo step.
            const QPoint at = more->mapToGlobal(QPoint(0, more->height()));
            attributeMenu(cueId, layerId, layerId == invalidId() ? target : Target(), target, key, at);
        });

        m_rows->addWidget(label, row, 0);
        m_rows->addWidget(value, row, 1);
        m_rows->addWidget(reach, row, 2);
        m_rows->addWidget(editBtn, row, 3);
        m_rows->addWidget(more, row, 4);
        m_rowText << QStringLiteral("%1 | %2 | %3").arg(label->text().trimmed(), text, origin);
        ++row;
    }
    m_rows->setRowStretch(row, 1);
    m_pages->setCurrentWidget(m_layerPage);
}

QStringList ShowTreeView::attributeRows() const
{
    return m_rowText;
}

QString ShowTreeView::updateSourceLabel(quint32 cueId, const Target &target, const QString &key) const
{
    quint32 src = invalidId();
    if (!m_st->valueSource(cueId, target, key, &src))
        return QString();
    const CueList *list = m_st->listOfCue(src);
    return list == nullptr ? QString() : m_st->cueLabel(*list, list->indexOf(src));
}

void ShowTreeView::attributeMenu(quint32 cueId, quint32 layerId, const Target &target,
                                 const Target &rowTarget, const QString &key, const QPoint &globalPos)
{
    QMenu menu(this);
    // A value set in an earlier cue (tracked, included, the base): D22 --
    // set it here, or update it where it was set so the change tracks on.
    const QString source = (key == QLatin1String("Effect.") || rowTarget.isEmpty())
                           ? QString() : updateSourceLabel(cueId, rowTarget, key);
    // Set to: the palettes of this kind, recent ones first.
    QMenu *set = menu.addMenu(source.isEmpty() ? tr("Set to") : tr("Set in this cue"));
    QList<QLCPalette *> pals;
    for (QLCPalette *p : m_doc->palettes())
    {
        if (m_st->isInlinePalette(p->id()))
            continue;
        const QString pk = m_st->keyForPalette(p->id());
        if (key == QLatin1String("Effect.") ? isEffectKey(pk) : pk == key)
            pals << p;
    }
    std::sort(pals.begin(), pals.end(), [this](const QLCPalette *a, const QLCPalette *b) {
        const int ra = m_recent.indexOf(qMakePair(int(LibPalette), a->id()));
        const int rb = m_recent.indexOf(qMakePair(int(LibPalette), b->id()));
        if ((ra < 0) != (rb < 0))
            return ra >= 0;
        if (ra != rb)
            return ra < rb;
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (QLCPalette *p : pals)
    {
        const quint32 pid = p->id();
        set->addAction(p->name(), this, [this, cueId, layerId, target, pid]() {
            dropPalettes(cueId, layerId, { pid }, target);
        });
    }
    set->setEnabled(!pals.isEmpty());
    if (!source.isEmpty())
    {
        QMenu *upd = menu.addMenu(tr("Update cue %1").arg(source));
        upd->setObjectName("stUpdateSource");
        upd->setToolTipsVisible(true);
        for (QLCPalette *p : pals)
        {
            const quint32 pid = p->id();
            upd->addAction(p->name(), this, [this, cueId, rowTarget, pid]() {
                noteRecent(LibPalette, pid);
                m_st->updateSource(cueId, rowTarget, m_st->lookForPalette(pid));
            });
        }
        upd->addSeparator();
        upd->addAction(tr("None (off from there on)"), this, [this, cueId, rowTarget, key]() {
            Look l;
            l.key = key;
            l.none = true;
            m_st->updateSource(cueId, rowTarget, l);
        });
        upd->setEnabled(true);
    }

    if (key == KeyDimmer)
    {
        menu.addAction(tr("One-off level…"), this, [this, cueId, layerId, target]() {
            bool ok = false;
            const int pc = QInputDialog::getInt(this, tr("Dimmer"), tr("Level (%)"), 100, 0, 100, 1, &ok);
            if (!ok)
                return;
            const quint32 pid = inlinePalette(cueId, layerId, target, QLCPalette::Dimmer,
                                              { qBound(0, qRound(pc * 255.0 / 100.0), 255) },
                                              tr("%1%").arg(pc));
            if (pid != QLCPalette::invalidId())
                applyLook(cueId, layerId, target, m_st->lookForPalette(pid));
        });
    }
    else if (key == KeyColour)
    {
        menu.addAction(tr("One-off colour…"), this, [this, cueId, layerId, target]() {
            const QColor c = QColorDialog::getColor(Qt::white, this, tr("Colour"));
            if (!c.isValid())
                return;
            const quint32 pid = inlinePalette(cueId, layerId, target, QLCPalette::Color,
                                              { QLCPalette::colorToString(c, QColor(Qt::black)) },
                                              tr("%1-ish").arg(c.name()));
            if (pid != QLCPalette::invalidId())
                applyLook(cueId, layerId, target, m_st->lookForPalette(pid));
        });
    }
    if (isEffectKey(key) && key != QLatin1String("Effect."))
    {
        // The effect's own Colour/Dimmer (inside that effect only, §2).
        QMenu *own = menu.addMenu(tr("Its own colour / dimmer"));
        QList<QLCPalette *> owners;
        for (QLCPalette *p : m_doc->palettes())
            if ((p->type() == QLCPalette::Color || p->type() == QLCPalette::Dimmer)
                && !m_st->isInlinePalette(p->id()))
                owners << p;
        std::sort(owners.begin(), owners.end(), [](const QLCPalette *a, const QLCPalette *b) {
            if (a->type() != b->type())
                return a->type() == QLCPalette::Color;
            return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
        });
        for (QLCPalette *p : owners)
        {
            const quint32 pid = p->id();
            own->addAction(p->name(), this, [this, cueId, layerId, key, pid, target]() {
                setEffectOwn(cueId, layerId, key, { pid }, target);
            });
        }
        own->addSeparator();
        own->addAction(tr("None of its own"), this, [this, cueId, layerId, key, target]() {
            setEffectOwn(cueId, layerId, key, {}, target);
        });
    }
    if (key != QLatin1String("Effect."))
    {
        menu.addSeparator();
        menu.addAction(tr("None (off here)"), this, [this, cueId, layerId, key, target]() {
            Look l;
            l.key = key;
            l.none = true;
            applyLook(cueId, layerId, target, l);
        });
        // Clearing only means something where it is set.
        if (layerId != invalidId() || target.isEmpty())
            menu.addAction(tr("Clear (inherit)"), this, [this, cueId, layerId, key]() {
                clearAttribute(cueId, layerId, key);
            });
    }
    menu.exec(globalPos);
}

bool ShowTreeView::applyLook(quint32 cueId, quint32 layerId, const Target &target, const Look &look)
{
    if (layerId != invalidId())
        return m_st->setLayerLook(cueId, layerId, look);
    if (!target.isEmpty())
        return m_st->setTargetLook(cueId, target, look);
    return m_st->setCueLook(cueId, look);
}

quint32 ShowTreeView::inlinePalette(quint32 cueId, quint32 layerId, const Target &target,
                                    QLCPalette::PaletteType type, const QVariantList &values,
                                    const QString &value)
{
    // Named for where it was made: "30% · 1.0.1 Towers" (D21).
    const CueList *list = m_st->listOfCue(cueId);
    const Cue *cue = m_st->cue(cueId);
    if (list == nullptr || cue == nullptr)
        return QLCPalette::invalidId();
    QString where;
    if (layerId != invalidId() && cue->layer(layerId) != nullptr)
    {
        const Layer *l = cue->layer(layerId);
        where = l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name;
    }
    else if (!target.isEmpty())
        where = m_st->targetLabel(target);
    else
        where = tr("cue");
    const quint32 pid = m_st->createInlinePalette(type, values,
                                                  QStringLiteral("%1 \u00b7 %2 %3")
                                                  .arg(value, m_st->cueLabel(*list, list->indexOf(cueId)), where));
    if (pid != QLCPalette::invalidId())
        noteRecent(LibPalette, pid);
    return pid;
}

void ShowTreeView::slotCueEdited()
{
    // The cue the page was showing -- focus leaving a field can come after
    // the tree has moved on to another cue.
    if (m_inspectorUpdating || m_pageCue == invalidId())
        return;
    const Cue *c = m_st->cue(m_pageCue);
    if (c == nullptr)
        return;
    Cue cue = *c;
    cue.number = m_cueNumber->text().trimmed();
    cue.name = m_cueName->text();
    cue.note = m_cueNote->text();
    cue.mode = CueMode(m_cueMode->currentData().toInt());
    cue.fadeIn = uint(qRound(m_cueFadeIn->value() * 1000));
    cue.fadeOut = uint(qRound(m_cueFadeOut->value() * 1000));
    cue.hold = m_cueHold->value() < 0 ? Function::infiniteSpeed() : uint(qRound(m_cueHold->value() * 1000));
    if (cue.number == c->number && cue.name == c->name && cue.note == c->note && cue.mode == c->mode
        && cue.fadeIn == c->fadeIn && cue.fadeOut == c->fadeOut && cue.hold == c->hold)
        return;
    m_st->setCue(cue);
}

void ShowTreeView::slotListModeChanged(int index)
{
    if (m_inspectorUpdating || currentKind() != ListNode)
        return;
    m_st->setListMode(currentId(), index == 1 ? ListMode::Tracking : ListMode::NonTracking);
}

void ShowTreeView::slotLayerReleaseToggled(bool on)
{
    if (m_inspectorUpdating)
        return;
    const quint32 cueId = currentCueId();
    if (currentKind() == LayerNode)
    {
        m_st->setLayerRelease(cueId, currentId(), on);
        return;
    }
    if (currentKind() != TrackedLayerNode || m_layerReleaseTarget.isEmpty())
        return;
    // A carried-in layer becomes the cue's own: select it there.
    const Target t = m_layerReleaseTarget;
    if (m_st->setTargetRelease(cueId, t, on))
        if (const Cue *c = m_st->cue(cueId))
            for (const Layer &l : c->layers)
                if (l.target == t)
                    selectNode(l.id, cueId);
}

void ShowTreeView::slotListBaseChanged(int index)
{
    if (m_inspectorUpdating || currentKind() != ListNode)
        return;
    m_st->setBaseCue(currentId(), m_listBase->itemData(index).toUInt());
}

void ShowTreeView::slotCueBaseToggled(bool on)
{
    if (m_inspectorUpdating || m_pageCue == invalidId())
        return;
    const CueList *list = m_st->listOfCue(m_pageCue);
    if (list != nullptr)
        m_st->setBaseCue(list->id, on ? m_pageCue : invalidId());
}

void ShowTreeView::slotRecordCue()
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
        return;
    const quint32 id = m_st->recordCue(cueId);
    if (id != invalidId())
        selectNode(id);
}

/****************************************************************************
 * Editing
 ****************************************************************************/

void ShowTreeView::slotNewProduction()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New production"), tr("Name"),
                                               QLineEdit::Normal, tr("Show"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    const quint32 id = m_st->addProduction(name.trimmed());
    selectNode(id);
}

void ShowTreeView::slotNewCueList()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    while (item != nullptr && item->data(0, KindRole).toInt() != ProductionNode)
        item = item->parent();
    if (item == nullptr)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New cue list"), tr("Name (a leading number numbers its cues: "
                                                                           "\"01-Opener\" starts at 1.0.0)"),
                                               QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    const quint32 id = m_st->addCueList(item->data(0, IdRole).toUInt(), name.trimmed());
    selectNode(id);
}

void ShowTreeView::slotNewCue()
{
    quint32 listId = invalidId();
    int after = -1;
    if (currentKind() == ListNode)
    {
        listId = currentId();
        if (const CueList *l = m_st->cueList(listId))
            after = int(l->cues.count()) - 1;
    }
    else if (currentCueId() != invalidId())
    {
        const CueList *l = m_st->listOfCue(currentCueId());
        if (l == nullptr)
            return;
        listId = l->id;
        after = l->indexOf(currentCueId());
    }
    if (listId == invalidId())
        return;
    const quint32 id = m_st->addCue(listId, after, QString());
    if (id != invalidId())
    {
        selectNode(id);
        m_cueName->setFocus();
    }
}

void ShowTreeView::slotAddLayer()
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
        return;
    QList<quint32> groups, fixtures;
    for (QTreeWidgetItem *item : m_library->selectedItems())
    {
        const int kind = item->data(0, KindRole).toInt();
        if (kind == LibGroup)
            groups << item->data(0, IdRole).toUInt();
        else if (kind == LibFixture)
            fixtures << item->data(0, IdRole).toUInt();
    }
    if (groups.isEmpty() && fixtures.isEmpty())
    {
        QMessageBox::information(this, tr("Add layer"),
                                 tr("Select one or more groups in the Library first (or drag them onto the cue)."));
        return;
    }
    dropTargets(cueId, groups, fixtures);
}

void ShowTreeView::slotDelete()
{
    const NodeKind kind = currentKind();
    const quint32 id = currentId();
    switch (kind)
    {
        case ProductionNode:
            if (QMessageBox::question(this, tr("Delete production"),
                                      tr("Delete this production and everything it plays? "
                                         "(Undo brings it back.)")) == QMessageBox::Yes)
                m_st->removeProduction(id);
        break;
        case ListNode:
            if (QMessageBox::question(this, tr("Delete cue list"),
                                      tr("Delete this cue list and its cues? (Undo brings it back.)"))
                == QMessageBox::Yes)
                m_st->removeCueList(id);
        break;
        case CueNode:
            m_st->removeCue(id);
        break;
        case LayerNode:
            m_st->removeLayer(currentCueId(), id);
        break;
        default:
        break;
    }
}

void ShowTreeView::slotUndo()
{
    m_st->undo();
}

void ShowTreeView::slotRedo()
{
    m_st->redo();
}

bool ShowTreeView::dropTargets(quint32 cueId, const QList<quint32> &groups, const QList<quint32> &fixtures,
                               bool separate)
{
    quint32 last = invalidId();
    if (separate)
    {
        // ⌥: one layer per group, as before (and one on the fixtures).
        for (quint32 g : groups)
        {
            Target t;
            t.groups << g;
            last = m_st->addLayer(cueId, t);
        }
        if (!fixtures.isEmpty())
        {
            Target t;
            t.fixtures = fixtures;
            last = m_st->addLayer(cueId, t);
        }
    }
    else if (!groups.isEmpty() || !fixtures.isEmpty())
    {
        // One layer on everything dropped together.
        Target t;
        t.groups = groups;
        t.fixtures = fixtures;
        last = m_st->addLayer(cueId, t);
    }
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    if (last != invalidId())
        selectNode(last, cueId);
    return last != invalidId();
}

bool ShowTreeView::dropTargetsOnLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                                      const QList<quint32> &fixtures)
{
    if (!m_st->addToLayer(cueId, layerId, groups, fixtures))
        return false;
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    selectNode(layerId, cueId);
    return true;
}

bool ShowTreeView::dropFunctions(quint32 cueId, const QList<quint32> &functions)
{
    quint32 last = invalidId();
    for (quint32 f : functions)
    {
        if (m_st->isGenerated(f) || m_doc->function(f) == nullptr)
            continue;
        Target t;
        t.function = f;
        last = m_st->addLayer(cueId, t);
        noteRecent(LibFunction, f);
    }
    return last != invalidId();
}

bool ShowTreeView::dropPalettes(quint32 cueId, quint32 layerId, const QList<quint32> &palettes,
                                const Target &target)
{
    // Colours dropped together are colour 1, 2, 3; anything else, one look
    // per palette (the last of a kind wins).
    QList<Look> looks;
    for (quint32 pid : palettes)
    {
        const Look l = m_st->lookForPalette(pid);
        if (l.key.isEmpty())
            continue;
        bool merged = false;
        for (Look &have : looks)
        {
            if (have.key == l.key && l.key == KeyColour)
            {
                have.palettes << pid;
                merged = true;
            }
            else if (have.key == l.key)
            {
                have = l;
                merged = true;
            }
        }
        if (!merged)
            looks << l;
        noteRecent(LibPalette, pid);
    }
    if (looks.isEmpty())
        return false;

    // A colour or dimmer dropped on a layer with an effect of its own and no
    // base look of that kind goes to the effect? No: drops set the layer's
    // base; an effect's own looks come from its ⋯ (kept simple for P1).
    bool done = false;
    for (const Look &l : looks)
        done |= applyLook(cueId, layerId, target, l);
    return done;
}

bool ShowTreeView::acceptsDrop(quint32 cueId, quint32 layerId, const QMimeData *mime) const
{
    if (cueId == invalidId() || mime == nullptr)
        return false;
    if (mime->hasFormat(QLatin1String(FunctionsTreeWidget::paletteDragMimeType())))
        return true;
    if (layerId != invalidId())
    {
        // Groups and fixtures join a layer's target; functions go on a cue.
        const Cue *cue = m_st->cue(cueId);
        const Layer *l = cue == nullptr ? nullptr : cue->layer(layerId);
        return l != nullptr && !l->target.isOpaque()
               && (mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureGroupMimeType()))
                   || mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureMimeType())));
    }
    return mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureGroupMimeType()))
           || mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureMimeType()))
           || mime->hasFormat(QLatin1String(FunctionsTreeWidget::functionDragMimeType()));
}

QString ShowTreeView::dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime, bool separate) const
{
    const CueList *list = m_st->listOfCue(cueId);
    const Cue *cue = m_st->cue(cueId);
    if (list == nullptr || cue == nullptr)
        return QString();
    const QString cueText = m_st->cueLabel(*list, list->indexOf(cueId));
    const QList<quint32> pals = idsFromMime(mime, FunctionsTreeWidget::paletteDragMimeType());
    if (!pals.isEmpty())
    {
        QStringList names;
        for (quint32 p : pals)
            names << paletteName(p);
        const QString what = names.join(QStringLiteral(" + "));
        if (layerId != invalidId())
        {
            const Layer *l = cue->layer(layerId);
            return tr("%1 → layer %2 in %3").arg(what, l == nullptr ? QString()
                                                    : (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name),
                                                cueText);
        }
        // A cue look reaches the cue's own layers that don't set it themselves.
        const QString key = m_st->keyForPalette(pals.first());
        int reach = 0, total = 0;
        for (const Layer &l : cue->layers)
        {
            if (l.target.isOpaque())
                continue;
            ++total;
            if (l.look(key) == nullptr)
                ++reach;
        }
        return tr("%1 → cue look on %2: reaches %3 of %4 layers").arg(what, cueText).arg(reach).arg(total);
    }
    const QList<quint32> groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
    const QList<quint32> fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
    if (!groups.isEmpty() || !fixtures.isEmpty())
    {
        Target t;
        t.groups = groups;
        t.fixtures = fixtures;
        const QString what = groups.isEmpty() ? tr("these fixtures") : m_st->targetLabel(t);
        if (layerId != invalidId())
        {
            const Layer *l = cue->layer(layerId);
            return tr("add %1 to layer %2 in %3")
                   .arg(what, l == nullptr ? QString()
                              : (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name), cueText);
        }
        const int pieces = int(groups.count()) + (fixtures.isEmpty() ? 0 : 1);
        if (pieces < 2)
            return tr("a new layer on %1 in %2").arg(what, cueText);
        if (separate)
            return tr("%1 new layers in %2, one for each group").arg(pieces).arg(cueText);
        return tr("one layer on %1 in %2 (⌥: a layer for each group)").arg(what, cueText);
    }
    if (!idsFromMime(mime, FunctionsTreeWidget::functionDragMimeType()).isEmpty())
        return tr("played as it is in %1").arg(cueText);
    return QString();
}

void ShowTreeView::editPalette(quint32 paletteId)
{
    if (m_doc->palette(paletteId) == nullptr)
    {
        m_lookEditor->setPalette(QLCPalette::invalidId());
        m_lookEditor->hide();
        return;
    }
    m_lookEditor->setPalette(paletteId);
    // A named palette is shared: say so before it is changed everywhere.
    m_lookEditor->setTrialNote(m_st->isInlinePalette(paletteId)
                               ? tr("A one-off value of this cue")
                               : tr("A shared palette: changing it changes every cue and scene that uses it"));
    m_lookEditor->show();
}

quint32 ShowTreeView::editedPalette() const
{
    return m_lookEditor->isVisible() ? m_lookEditor->paletteId() : QLCPalette::invalidId();
}

void ShowTreeView::paletteEdited(quint32 paletteId, bool full)
{
    // Scenes reference palettes, so nothing re-bakes (§4): the running ones
    // re-read it, and anything showing it is told.
    if (m_doc->effectScriptRunner() != nullptr)
        m_doc->effectScriptRunner()->updateEffectParams(paletteId);
    for (Function *f : m_doc->functions())
    {
        Scene *sc = qobject_cast<Scene *>(f);
        if (sc == nullptr || !sc->palettes().contains(paletteId))
            continue;
        if (sc->isRunning())
            sc->requestPaletteRefresh();
        if (full)
            m_doc->notifyFunctionChanged(sc->id());
    }
    m_doc->setModified();
    if (full)
    {
        // Names may have changed: the rows and the Library say them.
        updateInspector();
        m_libraryTimer->start();
    }
}

const char *ShowTreeView::cueMimeType()
{
    return "application/x-qlcconsole-showtree-cues";
}

QString ShowTreeView::cueDropHint(quint32 cueId, quint32 onCueId, bool include) const
{
    if (cueId == onCueId || onCueId == invalidId())
        return QString();
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr || list->indexOf(onCueId) < 0)
        return QString();           // cues move within their own list
    const QString what = m_st->cueLabel(*list, list->indexOf(cueId));
    const QString onto = m_st->cueLabel(*list, list->indexOf(onCueId));
    if (include)
    {
        // Only an include that can be made: none twice, no loop (D8).
        if (!canInclude(*list, onCueId, cueId))
            return QString();
        return tr("%1 includes %2").arg(onto, what);
    }
    return tr("Move %1 to %2's place").arg(what, onto);
}

bool ShowTreeView::dropCue(quint32 cueId, quint32 onCueId, bool include)
{
    if (cueDropHint(cueId, onCueId, include).isEmpty())
        return false;
    if (include)
        return m_st->addInclude(onCueId, cueId);
    const CueList *list = m_st->listOfCue(cueId);
    const bool ok = m_st->moveCue(cueId, list->indexOf(onCueId));
    if (ok)
        selectNode(cueId);
    return ok;
}

bool ShowTreeView::setOneOffDimmer(quint32 cueId, quint32 layerId, int percent)
{
    const quint32 pid = inlinePalette(cueId, layerId, Target(), QLCPalette::Dimmer,
                                      { qBound(0, qRound(percent * 255.0 / 100.0), 255) },
                                      tr("%1%").arg(percent));
    return pid != QLCPalette::invalidId() && applyLook(cueId, layerId, Target(), m_st->lookForPalette(pid));
}

bool ShowTreeView::setOneOffColour(quint32 cueId, quint32 layerId, const QColor &colour)
{
    const quint32 pid = inlinePalette(cueId, layerId, Target(), QLCPalette::Color,
                                      { QLCPalette::colorToString(colour, QColor(Qt::black)) },
                                      tr("%1-ish").arg(colour.name()));
    return pid != QLCPalette::invalidId() && applyLook(cueId, layerId, Target(), m_st->lookForPalette(pid));
}

bool ShowTreeView::setEffectOwn(quint32 cueId, quint32 layerId, const QString &key,
                                const QList<quint32> &palettes, const Target &target)
{
    const Cue *cue = m_st->cue(cueId);
    const CueList *list = m_st->listOfCue(cueId);
    if (cue == nullptr || list == nullptr)
        return false;
    const Look *have = nullptr;
    if (layerId != invalidId())
    {
        if (const Layer *l = cue->layer(layerId))
            have = l->look(key);
    }
    else if (target.isEmpty())
    {
        have = cue->look(key);
    }
    Look look;
    if (have != nullptr && !have->none)
    {
        look = *have;
    }
    else
    {
        // Inherited: take the effect as it plays here, and set it here.
        const Target t = layerId != invalidId() && cue->layer(layerId) != nullptr
                         ? cue->layer(layerId)->target : target;
        if (t.isEmpty())
            return false;
        const QList<ResolvedCue> r = resolve(*list);
        const ResolvedLayer *rl = r.at(list->indexOf(cueId)).layer(t);
        if (rl == nullptr || !rl->values.contains(key))
            return false;
        look = rl->values.value(key).look;
    }
    look.own = palettes;
    for (quint32 p : palettes)
        noteRecent(LibPalette, p);
    return applyLook(cueId, layerId, target, look);
}

bool ShowTreeView::clearAttribute(quint32 cueId, quint32 layerId, const QString &key)
{
    Look l;
    l.key = key;
    return layerId == invalidId() ? m_st->setCueLook(cueId, l) : m_st->setLayerLook(cueId, layerId, l);
}

bool ShowTreeView::setAttributeNone(quint32 cueId, quint32 layerId, const QString &key)
{
    Look l;
    l.key = key;
    l.none = true;
    return layerId == invalidId() ? m_st->setCueLook(cueId, l) : m_st->setLayerLook(cueId, layerId, l);
}

void ShowTreeView::slotTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    QMenu menu(this);
    menu.addAction(tr("+ Production…"), this, &ShowTreeView::slotNewProduction);
    if (item != nullptr)
    {
        m_tree->setCurrentItem(item);
        const int kind = item->data(0, KindRole).toInt();
        menu.addAction(tr("+ Cue list…"), this, &ShowTreeView::slotNewCueList);
        if (kind != ProductionNode)
            menu.addAction(tr("+ Cue"), this, &ShowTreeView::slotNewCue);
        if (kind == CueNode)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            const CueList *list = m_st->listOfCue(cueId);
            if (list != nullptr)
            {
                const bool isBase = list->baseCue == cueId;
                const quint32 listId = list->id;
                menu.addAction(isBase ? tr("Not the base cue") : tr("Make the base cue"), this,
                               [this, listId, cueId, isBase]() {
                    m_st->setBaseCue(listId, isBase ? invalidId() : cueId);
                });
                QMenu *inc = menu.addMenu(tr("Include cue"));
                includesMenu(inc, cueId);
                // Mode: List default / Track / Block / Cue-only.
                QMenu *mode = menu.addMenu(tr("Mode"));
                const CueMode now = list->cue(cueId)->mode;
                for (CueMode m : { CueMode::ListDefault, CueMode::Track, CueMode::Block, CueMode::CueOnly })
                {
                    QString text = cueModeLabel(m);
                    if (m == CueMode::ListDefault)
                        text = tr("List default (%1)").arg(list->mode == ListMode::Tracking ? tr("Track")
                                                                                            : tr("Block"));
                    QAction *a = mode->addAction(text, this, [this, cueId, m]() {
                        m_st->setCueMode(cueId, m);
                    });
                    a->setCheckable(true);
                    a->setChecked(now == m);
                }
                menu.addSeparator();
                menu.addAction(tr("Record as new cue"), this, [this, cueId]() {
                    const quint32 id = m_st->recordCue(cueId);
                    if (id != invalidId())
                        selectNode(id);
                });
                menu.addAction(tr("Duplicate cue"), this, [this, cueId]() {
                    const quint32 id = m_st->duplicateCue(cueId);
                    if (id != invalidId())
                        selectNode(id);
                });
            }
        }
        if (kind == CueNode)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            const CueList *list = m_st->listOfCue(cueId);
            const int at = list == nullptr ? -1 : list->indexOf(cueId);
            QAction *up = menu.addAction(tr("Move up"), this, [this, cueId, at]() {
                m_st->moveCue(cueId, at - 1);
                selectNode(cueId);
            });
            up->setEnabled(at > 0);
            QAction *down = menu.addAction(tr("Move down"), this, [this, cueId, at]() {
                m_st->moveCue(cueId, at + 1);
                selectNode(cueId);
            });
            down->setEnabled(list != nullptr && at + 1 < list->cues.count());
        }
        if (kind == LayerNode)
        {
            const quint32 cueId = item->data(0, CueRole).toUInt();
            const quint32 layerId = item->data(0, IdRole).toUInt();
            menu.addAction(tr("Rename…"), this, [this, layerId]() {
                bool ok = false;
                const QString n = QInputDialog::getText(this, tr("Rename layer"), tr("Name"),
                                                        QLineEdit::Normal, QString(), &ok);
                if (ok)
                    m_st->renameNode(layerId, n.trimmed());
            });
            menu.addAction(tr("Detach to plain scene"), this, [this, cueId, layerId]() {
                m_st->detachLayer(cueId, layerId);
            });
            // A layer on several groups can lose one (never its last).
            const Cue *cue = m_st->cue(cueId);
            const Layer *layer = cue == nullptr ? nullptr : cue->layer(layerId);
            if (layer != nullptr && !layer->target.isOpaque()
                && layer->target.groups.count() + layer->target.fixtures.count() > 1)
            {
                menu.addSeparator();
                for (quint32 g : layer->target.groups)
                {
                    Target one;
                    one.groups << g;
                    menu.addAction(tr("Remove %1 from layer").arg(m_st->targetLabel(one)), this,
                                   [this, cueId, layerId, g]() {
                        m_st->removeGroupFromLayer(cueId, layerId, g);
                    });
                }
                for (quint32 f : layer->target.fixtures)
                {
                    Target one;
                    one.fixtures << f;
                    menu.addAction(tr("Remove %1 from layer").arg(m_st->targetLabel(one)), this,
                                   [this, cueId, layerId, f]() {
                        m_st->removeFixtureFromLayer(cueId, layerId, f);
                    });
                }
            }
        }
        if (kind == ProductionNode || kind == ListNode)
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            menu.addAction(tr("Rename…"), this, [this, id, item]() {
                bool ok = false;
                const QString n = QInputDialog::getText(this, tr("Rename"), tr("Name"),
                                                        QLineEdit::Normal, item->text(0), &ok);
                if (ok && !n.trimmed().isEmpty())
                    m_st->renameNode(id, n.trimmed());
            });
        }
        if (kind != TrackedLayerNode)
        {
            menu.addSeparator();
            menu.addAction(tr("Delete"), this, &ShowTreeView::slotDelete);
        }
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void ShowTreeView::slotImportMenuAboutToShow()
{
    m_importMenu->clear();
    QList<Function *> chasers;
    for (Function *f : m_doc->functions())
        if (f->type() == Function::ChaserType && f->isVisible() && !m_st->isGenerated(f->id()))
            chasers << f;
    std::sort(chasers.begin(), chasers.end(), [](const Function *a, const Function *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (Function *f : chasers)
    {
        const quint32 id = f->id();
        m_importMenu->addAction(f->name(), this, [this, id]() { importChaser(id); });
    }
    if (chasers.isEmpty())
        m_importMenu->addAction(tr("(no chasers in this show)"))->setEnabled(false);
}

void ShowTreeView::importChaser(quint32 chaserId, bool showReport)
{
    const ShowTreeController::ImportReport rep = m_st->importChaser(chaserId);
    if (rep.list != invalidId())
        selectNode(rep.list);
    if (!showReport)
        return;
    QString text = tr("%1 cues, %2 layers, %3 played as they are, %4 includes; %5 scenes generated.")
                   .arg(rep.cues).arg(rep.layers).arg(rep.opaqueLayers).arg(rep.includes).arg(rep.generatedScenes);
    QStringList details;
    for (const QString &s : rep.htpConflicts)
        details << s;
    if (!rep.opaqueReasons.isEmpty())
        details << tr("Played as they are:") << rep.opaqueReasons;
    details << rep.notes;
    QMessageBox box(QMessageBox::Information, tr("Import chaser"), text, QMessageBox::Ok, this);
    box.setInformativeText(tr("The list is non-tracking, so it plays as the chaser did. The chaser is unchanged."));
    if (!details.isEmpty())
        box.setDetailedText(details.join(QStringLiteral("\n")));
    box.exec();
}

/****************************************************************************
 * Library
 ****************************************************************************/

void ShowTreeView::noteRecent(int kind, quint32 id, const QString &label)
{
    Q_UNUSED(label)
    const QPair<int, quint32> key(kind, id);
    m_recent.removeAll(key);
    m_recent.prepend(key);
    while (m_recent.count() > kRecentMax)
        m_recent.removeLast();
    reloadLibrary();
}

QStringList ShowTreeView::recentItems() const
{
    QStringList out;
    for (int i = 0; i < m_library->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *section = m_library->topLevelItem(i);
        if (section->data(0, IdRole).toUInt() != 0)
            continue;
        for (int k = 0; k < section->childCount(); ++k)
            out << section->child(k)->text(0);
    }
    return out;
}

QStringList ShowTreeView::inListItems() const
{
    QStringList out;
    for (int i = 0; i < m_library->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *section = m_library->topLevelItem(i);
        if (section->data(0, IdRole).toUInt() != 3)
            continue;
        for (int k = 0; k < section->childCount(); ++k)
            if (section->child(k)->data(0, KindRole).toInt() != LibFolder)
                out << section->child(k)->text(0);
    }
    return out;
}

bool ShowTreeView::isPinned(int kind, quint32 id) const
{
    return m_pinned.contains(qMakePair(kind, id));
}

void ShowTreeView::setPinned(int kind, quint32 id, bool pinned)
{
    const QPair<int, quint32> key(kind, id);
    m_pinned.removeAll(key);
    if (pinned)
        m_pinned << key;
    QStringList list;
    for (const auto &p : m_pinned)
        list << QStringLiteral("%1:%2").arg(p.first).arg(p.second);
    QSettings settings;
    settings.setValue(SETTINGS_PINNED, list);
    reloadLibrary();
}

void ShowTreeView::reloadLibrary()
{
    // Remembered: which folders were open (a group folder by its path, so
    // two "Front" folders in different places are told apart).
    auto openKey = [](const QTreeWidgetItem *item) {
        const QString path = item->data(0, PathRole).toString();
        return path.isEmpty() ? item->text(0) + QLatin1Char('/') + QString::number(item->data(0, KindRole).toInt())
                              : path;
    };
    QSet<QString> open;
    for (QTreeWidgetItemIterator it(m_library); *it; ++it)
        if ((*it)->isExpanded())
            open.insert(openKey(*it));
    // A filter opens folders to show what matches: not "open" to remember.
    if (!m_filter->text().isEmpty())
        open = m_openWithoutFilter;
    const bool first = m_library->topLevelItemCount() == 0;

    m_library->clear();
    auto section = [this](const QString &name, int order) {
        QTreeWidgetItem *s = new QTreeWidgetItem(m_library);
        s->setText(0, name);
        QFont f = s->font(0);
        f.setBold(true);
        s->setFont(0, f);
        s->setData(0, KindRole, LibSection);
        s->setData(0, IdRole, order);
        s->setFlags(Qt::ItemIsEnabled);
        return s;
    };
    auto add = [this](QTreeWidgetItem *parent, int kind, quint32 id) -> QTreeWidgetItem * {
        QString text;
        QIcon icon;
        if (kind == LibGroup)
        {
            const FixtureGroup *g = m_doc->fixtureGroup(id);
            if (g == nullptr)
                return nullptr;
            text = tr("%1 (%2)").arg(g->name()).arg(g->fixtureList().count());
            icon = QIcon(":/group.png");
        }
        else if (kind == LibFixture)
        {
            const Fixture *f = m_doc->fixture(id);
            if (f == nullptr)
                return nullptr;
            text = f->name();
            icon = QIcon(":/fixture.png");
        }
        else if (kind == LibPalette)
        {
            QLCPalette *p = m_doc->palette(id);
            if (p == nullptr)
                return nullptr;
            text = p->name().isEmpty() ? tr("(unnamed)") : p->name();
            if (p->type() == QLCPalette::Color)
            {
                QPixmap px(14, 14);
                px.fill(p->rgbValue());
                icon = QIcon(px);
            }
            else
                icon = QIcon(p->iconResource());
        }
        else if (kind == LibFunction)
        {
            const Function *f = m_doc->function(id);
            if (f == nullptr)
                return nullptr;
            text = f->name();
            icon = f->getIcon();
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, text);
        item->setIcon(0, icon);
        item->setData(0, KindRole, kind);
        item->setData(0, IdRole, id);
        if (isPinned(kind, id))
            item->setText(0, QStringLiteral("★ ") + text);
        return item;
    };

    QTreeWidgetItem *recent = section(tr("Recent"), 0);
    for (const auto &r : m_recent)
        add(recent, r.first, r.second);

    // In this cue list (D20): what the selected list already uses, most
    // used first -- a song mostly reuses its own vocabulary.
    m_libraryList = currentListId();
    if (m_libraryList != invalidId())
    {
        QTreeWidgetItem *inList = section(tr("In this cue list"), 3);
        const ShowTreeController::Usage u = m_st->usageInList(m_libraryList);
        struct Entry { int kind; quint32 id; int count; };
        QList<Entry> entries;
        for (auto it = u.groups.constBegin(); it != u.groups.constEnd(); ++it)
            entries << Entry { LibGroup, it.key(), it.value() };
        for (auto it = u.fixtures.constBegin(); it != u.fixtures.constEnd(); ++it)
            entries << Entry { LibFixture, it.key(), it.value() };
        for (auto it = u.palettes.constBegin(); it != u.palettes.constEnd(); ++it)
            entries << Entry { LibPalette, it.key(), it.value() };
        for (auto it = u.functions.constBegin(); it != u.functions.constEnd(); ++it)
            entries << Entry { LibFunction, it.key(), it.value() };
        QList<QPair<Entry, QTreeWidgetItem *>> made;
        for (const Entry &e : entries)
            if (QTreeWidgetItem *item = add(inList, e.kind, e.id))
            {
                item->setText(0, tr("%1 ×%2").arg(item->text(0)).arg(e.count));
                made << qMakePair(e, item);
            }
        // Most used first, then by kind (groups, then values) and name.
        std::sort(made.begin(), made.end(), [](const QPair<Entry, QTreeWidgetItem *> &a,
                                               const QPair<Entry, QTreeWidgetItem *> &b) {
            if (a.first.count != b.first.count)
                return a.first.count > b.first.count;
            if (a.first.kind != b.first.kind)
                return a.first.kind < b.first.kind;
            return a.second->text(0).compare(b.second->text(0), Qt::CaseInsensitive) < 0;
        });
        for (const auto &m : made)
            inList->removeChild(m.second);
        for (const auto &m : made)
            inList->addChild(m.second);
        if (made.isEmpty())
        {
            QTreeWidgetItem *none = new QTreeWidgetItem(inList);
            none->setText(0, tr("(nothing yet)"));
            none->setFlags(Qt::ItemIsEnabled);
            none->setData(0, KindRole, LibFolder);
        }
    }
    QTreeWidgetItem *pinned = section(tr("Pinned"), 1);
    for (const auto &p : m_pinned)
        add(pinned, p.first, p.second);
    QTreeWidgetItem *all = section(tr("All"), 2);

    auto folder = [this](QTreeWidgetItem *parent, const QString &name) {
        QTreeWidgetItem *f = new QTreeWidgetItem(parent);
        f->setText(0, name);
        f->setData(0, KindRole, LibFolder);
        f->setFlags(Qt::ItemIsEnabled);
        return f;
    };

    // Groups in the Fixture Manager's folders (FixtureGroup::path), nested
    // the same way: folders first, then the groups, each by name.
    QTreeWidgetItem *groups = folder(all, tr("Groups"));
    groups->setData(0, PathRole, QStringLiteral("Groups"));
    QHash<QString, QTreeWidgetItem *> groupFolders;
    std::function<QTreeWidgetItem *(const QString &)> groupFolder = [&](const QString &path) {
        if (path.isEmpty())
            return groups;
        if (QTreeWidgetItem *have = groupFolders.value(path))
            return have;
        const int slash = path.lastIndexOf(QLatin1Char('/'));
        QTreeWidgetItem *f = folder(groupFolder(slash < 0 ? QString() : path.left(slash)),
                                    slash < 0 ? path : path.mid(slash + 1));
        f->setIcon(0, QIcon(":/folder.png"));
        f->setData(0, PathRole, QStringLiteral("Groups/") + path);
        groupFolders.insert(path, f);
        return f;
    };
    auto groupPath = [](const FixtureGroup *g) {
        QStringList parts = g->path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (QString &part : parts)
            part = part.trimmed();
        parts.removeAll(QString());
        return parts.join(QLatin1Char('/'));
    };
    QList<FixtureGroup *> gl = m_doc->fixtureGroups();
    QStringList paths;
    for (const FixtureGroup *g : gl)
        if (!groupPath(g).isEmpty() && !paths.contains(groupPath(g)))
            paths << groupPath(g);
    std::sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    for (const QString &path : paths)
        groupFolder(path);
    std::sort(gl.begin(), gl.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (const FixtureGroup *g : gl)
        add(groupFolder(groupPath(g)), LibGroup, g->id());

    const QList<QPair<QString, QString>> kinds = {
        { KeyColour, tr("Colour") }, { KeyDimmer, tr("Dimmer") }, { KeyPosition, tr("Position") },
        { KeyGobo, tr("Gobo") }, { KeyZoom, tr("Zoom") }, { KeyBeam, tr("Focus / Frost / Iris") },
        { KeyShutter, tr("Shutter") }, { KeyEffectPrefix, tr("Effects") } };
    QHash<QString, QTreeWidgetItem *> folders;
    QTreeWidgetItem *palettes = folder(all, tr("Palettes"));
    for (const auto &k : kinds)
        folders.insert(k.first, folder(palettes, k.second));
    QList<QLCPalette *> pl = m_doc->palettes();
    std::sort(pl.begin(), pl.end(), [](const QLCPalette *a, const QLCPalette *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (const QLCPalette *p : pl)
    {
        if (m_st->isInlinePalette(p->id()))
            continue;               // one-off values: Recent only (D21)
        QString key = m_st->keyForPalette(p->id());
        if (isEffectKey(key))
            key = KeyEffectPrefix;
        add(folders.value(key, folders.value(KeyEffectPrefix)), LibPalette, p->id());
    }
    for (QTreeWidgetItem *f : folders)
        if (f->childCount() == 0)
            delete f;

    QTreeWidgetItem *funcs = folder(all, tr("Functions (played as they are)"));
    QList<Function *> fl;
    for (Function *f : m_doc->functions())
        if (f->isVisible() && !m_st->isGenerated(f->id()) && f->type() != Function::ChaserType)
            fl << f;
    std::sort(fl.begin(), fl.end(), [](const Function *a, const Function *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });
    for (const Function *f : fl)
        add(funcs, LibFunction, f->id());

    for (QTreeWidgetItemIterator it(m_library); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        const int kind = item->data(0, KindRole).toInt();
        const QString key = openKey(item);
        if (first)
            item->setExpanded(kind == LibSection);
        else
            item->setExpanded(open.contains(key) || (kind == LibSection && item != all
                                                     ? true : open.contains(key)));
    }
    if (!m_filter->text().isEmpty())
        slotFilterChanged(m_filter->text());
}

void ShowTreeView::slotFilterChanged(const QString &text)
{
    // Starting to filter: remember which folders were open, to go back to
    // when the filter is cleared (filtering opens what it finds).
    if (!text.isEmpty() && !m_filtering)
    {
        m_openWithoutFilter.clear();
        for (QTreeWidgetItemIterator it(m_library); *it; ++it)
            if ((*it)->isExpanded())
            {
                const QString path = (*it)->data(0, PathRole).toString();
                m_openWithoutFilter.insert(path.isEmpty() ? (*it)->text(0) + QLatin1Char('/')
                                                            + QString::number((*it)->data(0, KindRole).toInt())
                                                          : path);
            }
    }
    const bool wasFiltering = m_filtering;
    m_filtering = !text.isEmpty();

    // A row stays when it matches or anything under it does; a folder
    // whose name matches ("Movers") shows everything in it, at any depth.
    std::function<bool(QTreeWidgetItem *, bool)> apply = [&](QTreeWidgetItem *item, bool inMatch) {
        const int kind = item->data(0, KindRole).toInt();
        const bool folder = kind == LibFolder && !item->data(0, PathRole).toString().isEmpty()
                            && item->data(0, PathRole).toString() != QLatin1String("Groups");
        const bool leaf = kind != LibSection && kind != LibFolder;
        const bool match = text.isEmpty() || inMatch
                           || ((leaf || folder) && item->text(0).contains(text, Qt::CaseInsensitive));
        bool any = false;
        for (int i = 0; i < item->childCount(); ++i)
            any |= apply(item->child(i), match && !text.isEmpty());
        const bool show = match || any || kind == LibSection;
        item->setHidden(!show);
        if (!text.isEmpty() && any)
            item->setExpanded(true);
        return match || any;
    };
    for (int i = 0; i < m_library->topLevelItemCount(); ++i)
        apply(m_library->topLevelItem(i), false);

    // Cleared: the folders the filter opened close again.
    if (text.isEmpty() && wasFiltering)
        for (QTreeWidgetItemIterator it(m_library); *it; ++it)
        {
            QTreeWidgetItem *item = *it;
            if (item->data(0, KindRole).toInt() != LibFolder)
                continue;
            const QString path = item->data(0, PathRole).toString();
            item->setExpanded(m_openWithoutFilter.contains(
                path.isEmpty() ? item->text(0) + QLatin1Char('/') + QString::number(LibFolder) : path));
        }
}

void ShowTreeView::slotLibraryContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_library->itemAt(pos);
    if (item == nullptr)
        return;
    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    if (kind != LibGroup && kind != LibFixture && kind != LibPalette && kind != LibFunction)
        return;
    QMenu menu(this);
    const bool pinned = isPinned(kind, id);
    menu.addAction(pinned ? tr("Unpin") : tr("Pin"), this, [this, kind, id, pinned]() {
        setPinned(kind, id, !pinned);
    });
    if (kind == LibPalette && m_st->isInlinePalette(id))
    {
        menu.addAction(tr("Make reusable…"), this, [this, id]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("Make reusable"), tr("Name"),
                                                    QLineEdit::Normal, QString(), &ok);
            if (!ok || n.trimmed().isEmpty())
                return;
            const QLCPalette *p = m_doc->palette(id);
            const QString folder = p == nullptr ? QStringLiteral("Palettes")
                                   : QStringLiteral("Palettes/%1").arg(QLCPalette::typeToString(p->type()));
            m_st->makeReusable(id, n.trimmed(), folder);
            reloadLibrary();
        });
    }
    menu.exec(m_library->viewport()->mapToGlobal(pos));
}

/****************************************************************************
 * Command line (§6C)
 ****************************************************************************/

void ShowTreeView::setCommandLineVisible(bool visible)
{
    m_commandBar->setVisible(visible);
    {
        QSignalBlocker block(m_commandToggle);
        m_commandToggle->setChecked(visible);
    }
    QSettings settings;
    settings.setValue(SETTINGS_COMMANDLINE, visible);
    if (visible)
        m_command->setFocus();
}

bool ShowTreeView::isCommandLineVisible() const
{
    return !m_commandBar->isHidden();
}

QString ShowTreeView::commandMessage() const
{
    return m_commandMsg->text();
}

bool ShowTreeView::runCommand(const QString &line)
{
    ShowTreeCommand::Context ctx;
    for (QTreeWidgetItem *item = m_tree->currentItem(); item != nullptr; item = item->parent())
        if (item->data(0, KindRole).toInt() == ProductionNode)
            ctx.production = item->data(0, IdRole).toUInt();
    if (ctx.production == invalidId() && m_st->productions().count() == 1)
        ctx.production = m_st->productions().first().id;
    ctx.list = currentListId();
    ctx.cue = currentCueId();

    const ShowTreeCommand::Result r = m_commander->run(line, ctx);
    m_commandMsg->setText(r.message);
    QPalette pal = m_commandMsg->palette();
    pal.setColor(QPalette::WindowText, r.ok ? palette().color(QPalette::WindowText) : QColor(Qt::red));
    m_commandMsg->setPalette(pal);
    if (!r.ok)
        return false;

    if (m_commandHistory.isEmpty() || m_commandHistory.last() != line)
        m_commandHistory << line;
    m_historyAt = -1;
    // Where the command leaves the selection: its layer, cue or list.
    if (r.layer != invalidId() && r.context.cue != invalidId())
        selectNode(r.layer, r.context.cue);
    else if (r.context.cue != invalidId())
        selectNode(r.context.cue);
    else if (r.context.list != invalidId())
        selectNode(r.context.list);
    return true;
}

void ShowTreeView::slotCommandEntered()
{
    if (runCommand(m_command->text()))
        m_command->clear();
}

bool ShowTreeView::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_command && ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Tab)
        {
            // Tab completes from the Library instead of moving focus.
            ShowTreeCommand::Context ctx;
            ctx.list = currentListId();
            ctx.cue = currentCueId();
            QStringList options;
            m_command->setText(m_commander->complete(m_command->text(), ctx, &options));
            m_commandMsg->setText(options.count() > 1 ? options.join(QStringLiteral(" · ")) : QString());
            return true;
        }
        if ((ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) && !m_commandHistory.isEmpty())
        {
            if (ke->key() == Qt::Key_Up)
                m_historyAt = m_historyAt < 0 ? int(m_commandHistory.count()) - 1 : qMax(0, m_historyAt - 1);
            else if (m_historyAt >= 0)
                m_historyAt = m_historyAt + 1 < m_commandHistory.count() ? m_historyAt + 1 : -1;
            m_command->setText(m_historyAt < 0 ? QString() : m_commandHistory.at(m_historyAt));
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}
