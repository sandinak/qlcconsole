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
#include <QTabWidget>
#include <QFrame>
#include <QDialog>
#include <QApplication>
#include <QRadioButton>
#include <QDialogButtonBox>
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
#include <QTextBrowser>
#include <QTextDocument>
#include <QListWidget>
#include <QKeyEvent>
#include <QSettings>
#include <QMimeData>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QDrag>
#include <QPainter>
#include <algorithm>

#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "fixturegroupsource.h"
#include "functionstreewidget.h"
#include "linespreadpreview.h"
#include "monitorproperties.h"
#include "stagetarget.h"
#include "linespread.h"
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

#include "virtualconsole.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcframe.h"

using namespace ShowTree;

#define SETTINGS_PINNED QStringLiteral("showtree/pinned")
// "2": the columns were reordered (Library | Tree | Editor, 2026-10-17), so
// sizes saved for the old order are not read back.
#define SETTINGS_SPLITTER QStringLiteral("showtree/splitter2")
#define SETTINGS_EDITOR_SPLITTER QStringLiteral("showtree/editorSplitter")

static const int kRecentMax = 10;

static ShowTreeView *s_instance = nullptr;

/* Selecting a row from code: that row alone, whatever keys are held (with
   ExtendedSelection a held ⌘ -- ⌘Z, say -- would otherwise toggle it). */
static const QItemSelectionModel::SelectionFlags kSelectOnly =
    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;

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
        // ⌘-click / ⇧-click select several (Merge layers, Delete); the
        // page, the preview and every single-item action follow the
        // current row, the one last clicked.
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    void setHint(QLabel *hint) { m_hint = hint; }

protected:
    void target(const QPoint &pos, quint32 *cue, quint32 *layer, ShowTree::Target *carried = nullptr) const
    {
        *cue = ShowTree::invalidId();
        *layer = ShowTree::invalidId();
        if (carried != nullptr)
            *carried = ShowTree::Target();
        QTreeWidgetItem *item = itemAt(pos);
        if (item == nullptr)
        {
            // Below the last row: the selected cue (or the selected layer's
            // cue) -- a new layer there.
            *cue = m_view->currentCueId();
            return;
        }
        const int kind = item->data(0, ShowTreeView::KindRole).toInt();
        if (kind == ShowTreeView::CueNode)
            *cue = item->data(0, ShowTreeView::IdRole).toUInt();
        else if (kind == ShowTreeView::LayerNode)
        {
            *cue = item->data(0, ShowTreeView::CueRole).toUInt();
            *layer = item->data(0, ShowTreeView::IdRole).toUInt();
        }
        else if (kind == ShowTreeView::TrackedLayerNode && carried != nullptr)
        {
            // A layer carried in: a drop sets it in this cue, as on its page.
            *cue = item->data(0, ShowTreeView::CueRole).toUInt();
            *carried = m_view->carriedTarget(*cue, item->data(0, ShowTreeView::TargetRole).toString());
            if (carried->isEmpty())
                *cue = ShowTree::invalidId();
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
            if (cues.count() > 1)
            {
                // Several cues have no one place to go: refused, and said.
                e->ignore();
                if (m_hint != nullptr)
                    m_hint->setText(ShowTreeView::tr("Drag one cue at a time to move or include it"));
                return;
            }
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
        ShowTree::Target carried;
        target(e->position().toPoint(), &cue, &layer, &carried);
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (cue != ShowTree::invalidId() && m_view->acceptsDrop(cue, layer, e->mimeData(), carried, child))
        {
            e->acceptProposedAction();
            if (m_hint != nullptr)
                m_hint->setText(itemAt(e->position().toPoint()) == nullptr
                                ? m_view->dropHintBelow(cue, e->mimeData(), e->modifiers() & Qt::AltModifier)
                                : m_view->dropHint(cue, layer, e->mimeData(),
                                                   e->modifiers() & Qt::AltModifier, carried, child));
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
        ShowTree::Target carried;
        target(e->position().toPoint(), &cue, &layer, &carried);
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
            if (cues.count() == 1 && layer == ShowTree::invalidId() && carried.isEmpty()
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
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (m_view->acceptsDrop(cue, layer, mime, carried, child)
            && m_view->dropMime(cue, layer, mime, e->modifiers() & Qt::AltModifier, carried, false, child))
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
    /** What a drag of @a items carries (Return / double-click use it too). */
    QMimeData *mimeFor(const QList<QTreeWidgetItem *> &items) const { return mimeData(items); }
    /** Return / Enter: told, with whether ⌥ was held. */
    std::function<void(bool)> onReturn;

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && onReturn)
        {
            onReturn(e->modifiers() & Qt::AltModifier);
            e->accept();
            return;
        }
        QTreeWidget::keyPressEvent(e);
    }
public:

protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        QByteArray grp, fx, pal, fn, set, tgt, reu;
        QDataStream gs(&grp, QIODevice::WriteOnly), xs(&fx, QIODevice::WriteOnly),
                    ps(&pal, QIODevice::WriteOnly), fs(&fn, QIODevice::WriteOnly),
                    ss(&set, QIODevice::WriteOnly), ts(&tgt, QIODevice::WriteOnly),
                    rs(&reu, QIODevice::WriteOnly);
        int g = 0, x = 0, p = 0, f = 0, t = 0, a = 0, u = 0;
        for (QTreeWidgetItem *item : items)
        {
            const quint32 id = item->data(0, ShowTreeView::IdRole).toUInt();
            switch (item->data(0, ShowTreeView::KindRole).toInt())
            {
                case ShowTreeView::LibGroup: gs << id; ++g; break;
                case ShowTreeView::LibFixture: xs << id; ++x; break;
                case ShowTreeView::LibPalette: ps << id; ++p; break;
                case ShowTreeView::LibFunction: fs << id; ++f; break;
                case ShowTreeView::LibSet: ss << id; ++t; break;
                case ShowTreeView::LibTarget: ts << id; ++a; break;
                case ShowTreeView::LibReusable: rs << id; ++u; break;
                default: break;
            }
        }
        if (g + x + p + f + t + a + u == 0)
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
        if (t > 0)
            mime->setData(QLatin1String(ShowTreeView::setMimeType()), set);
        if (a > 0)
            mime->setData(QLatin1String(ShowTreeView::targetMimeType()), tgt);
        if (u > 0)
            mime->setData(QLatin1String(ShowTreeView::reusableMimeType()), reu);
        return mime;
    }
};

/* A page that takes drops (2026-10-17): the layer page (palettes set its
   looks, groups join it -- a carried-in layer is brought into the cue) and
   the cue page (palettes are cue looks, groups a new layer, ⌥ one each,
   functions an opaque layer). While a drag is over it, it lights up and the
   header says what the drop would do, in the tree's words. */
class DropPage final : public QWidget
{
public:
    DropPage(ShowTreeView *view, QWidget *parent) : QWidget(parent), m_view(view)
    {
        setAcceptDrops(true);
    }
    void setHint(QLabel *hint) { m_hint = hint; }
    /** Told when the page lights up for a drop, or goes back. */
    std::function<void(bool)> onActive;

    quint32 cue = ShowTree::invalidId();
    quint32 layer = ShowTree::invalidId();
    ShowTree::Target target;            //!< a carried-in layer's
    /** A drop leaves the cue selected (the cue page). */
    bool stayOnCue = false;

    void setDropActive(bool on)
    {
        if (m_active == on)
            return;
        m_active = on;
        if (onActive)
            onActive(on);
        update();
    }
    bool isDropActive() const { return m_active; }
    void say(const QString &text)
    {
        if (m_hint != nullptr)
            m_hint->setText(text);
    }

protected:
    bool takes(const QMimeData *mime, bool child = false) const
    {
        return cue != ShowTree::invalidId() && m_view->acceptsDrop(cue, layer, mime, target, child);
    }
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (!takes(e->mimeData(), child))
            return;
        e->acceptProposedAction();
        setDropActive(true);
        say(m_view->dropHint(cue, layer, e->mimeData(), e->modifiers() & Qt::AltModifier, target, child));
    }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (!takes(e->mimeData(), child))
        {
            e->ignore();
            return;
        }
        e->acceptProposedAction();
        setDropActive(true);
        say(m_view->dropHint(cue, layer, e->mimeData(), e->modifiers() & Qt::AltModifier, target, child));
    }
    void dragLeaveEvent(QDragLeaveEvent *) override
    {
        setDropActive(false);
        say(QString());
    }
    void dropEvent(QDropEvent *e) override
    {
        setDropActive(false);
        say(QString());
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (takes(e->mimeData(), child)
            && m_view->dropMime(cue, layer, e->mimeData(), e->modifiers() & Qt::AltModifier, target, stayOnCue,
                                child))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void paintEvent(QPaintEvent *e) override
    {
        QWidget::paintEvent(e);
        if (!m_active)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor hl = palette().color(QPalette::Highlight);
        p.setPen(QPen(hl, 2));
        hl.setAlpha(28);
        p.setBrush(hl);
        p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
    }

private:
    ShowTreeView *m_view;
    QLabel *m_hint = nullptr;
    bool m_active = false;
};

/* The cue page's layers, one row each -- the cue's own and (grey) the ones
   carried in. A drop on a row goes to that layer, as on its row in the tree;
   anywhere else in the list, to the cue (a new layer). A click selects it. */
class CueLayerList final : public QListWidget
{
public:
    CueLayerList(ShowTreeView *view, DropPage *page, QWidget *parent)
        : QListWidget(parent), m_view(view), m_page(page)
    {
        setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setDropIndicatorShown(false);
        setSelectionMode(QAbstractItemView::NoSelection);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    /** Per row: a carried-in layer's target (empty: the cue's own layer). */
    QList<ShowTree::Target> targets;

    /** Where a drop at @a pos goes: the row's layer if it takes it, else the
     *  cue. False: neither takes it. */
    bool where(const QPoint &pos, const QMimeData *mime, quint32 *layer, ShowTree::Target *target, int *row,
               bool child = false) const
    {
        *layer = ShowTree::invalidId();
        *target = ShowTree::Target();
        *row = -1;
        const quint32 cue = m_page->cue;
        if (cue == ShowTree::invalidId())
            return false;
        QListWidgetItem *item = itemAt(pos);
        if (item != nullptr)
        {
            const int r = QListWidget::row(item);
            quint32 l = ShowTree::invalidId();
            ShowTree::Target t;
            if (item->data(ShowTreeView::KindRole).toInt() == ShowTreeView::LayerNode)
                l = item->data(ShowTreeView::IdRole).toUInt();
            else if (r < targets.count())
                t = targets.at(r);
            if (m_view->acceptsDrop(cue, l, mime, t, child))
            {
                *layer = l;
                *target = t;
                *row = r;
                return true;
            }
        }
        return !child && m_view->acceptsDrop(cue, ShowTree::invalidId(), mime);
    }
    void clearDropRow()
    {
        if (m_dropRow >= 0 && m_dropRow < count())
            item(m_dropRow)->setData(Qt::BackgroundRole, QVariant());
        m_dropRow = -1;
    }

protected:
    void showRow(int row)
    {
        if (row == m_dropRow)
            return;
        clearDropRow();
        m_dropRow = row;
        if (row >= 0 && row < count())
        {
            QColor hl = palette().color(QPalette::Highlight);
            hl.setAlpha(90);
            item(row)->setBackground(hl);
        }
    }
    void hover(QDropEvent *e)
    {
        quint32 l;
        ShowTree::Target t;
        int r;
        const bool alt = e->modifiers() & Qt::AltModifier;
        const bool child = e->modifiers() & Qt::ShiftModifier;
        if (!where(e->position().toPoint(), e->mimeData(), &l, &t, &r, child))
        {
            e->ignore();
            showRow(-1);
            m_page->setDropActive(false);
            m_page->say(QString());
            return;
        }
        e->acceptProposedAction();
        showRow(r);
        // Onto a row: that row lights up; else the whole page (a new layer).
        m_page->setDropActive(r < 0);
        m_page->say(m_view->dropHint(m_page->cue, l, e->mimeData(), alt, t, child));
    }
    void dragEnterEvent(QDragEnterEvent *e) override { hover(e); }
    void dragMoveEvent(QDragMoveEvent *e) override { hover(e); }
    void dragLeaveEvent(QDragLeaveEvent *) override
    {
        showRow(-1);
        m_page->setDropActive(false);
        m_page->say(QString());
    }
    void dropEvent(QDropEvent *e) override
    {
        quint32 l;
        ShowTree::Target t;
        int r;
        const bool child = e->modifiers() & Qt::ShiftModifier;
        const bool ok = where(e->position().toPoint(), e->mimeData(), &l, &t, &r, child);
        showRow(-1);
        m_page->setDropActive(false);
        m_page->say(QString());
        if (ok && m_view->dropMime(m_page->cue, l, e->mimeData(), e->modifiers() & Qt::AltModifier, t, true, child))
            e->acceptProposedAction();
        else
            e->ignore();
    }

private:
    ShowTreeView *m_view;
    DropPage *m_page;
    int m_dropRow = -1;
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
    s_instance = this;

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
    m_newList = button(tr("+ Cue list"), tr("A new Tracking cue list in the selected production (an imported chaser starts "
                                         "Non-tracking)"), "stNewList");
    m_newCue = button(tr("+ Cue"), tr("A new cue after the selected one, numbered for you (never renumbers)"), "stNewCue");
    m_addLayer = button(tr("+ Layer"), tr("A layer on the groups or fixtures selected in the Library "
                                          "(or drag them onto a cue). Several selected: ONE layer on all "
                                          "of them; ⌥-click (or the arrow) for a layer each — as a drop "
                                          "and an ⌥-drop do."), "stAddLayer");
    {
        QMenu *m = new QMenu(m_addLayer);
        m->addAction(tr("One layer on all selected"), this, [this]() { addLayersFromLibrary(false); });
        m->addAction(tr("A layer for each (⌥)"), this, [this]() { addLayersFromLibrary(true); });
        m_addLayer->setMenu(m);
        m_addLayer->setPopupMode(QToolButton::MenuButtonPopup);
    }
    m_record = button(tr("Record"), tr("Record as new cue: what is on stage frozen into a new Block cue "
                                       "after the selected one — the selected cue, the list's running cue "
                                       "or everything lit. When they differ it asks, or does what "
                                       "Settings → Show Tree says."), "stRecord");
    m_merge = button(tr("Merge"), tr("Merge the layers selected in the tree (⌘- or ⇧-click two or more "
                                     "layers of one cue) into one layer on all their groups"), "stMerge");
    m_delete = button(tr("Delete"), tr("Delete the selected cues and layers, or a cue list or production"),
                      "stDelete");
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
    m_commander->setRecordChooser([this](quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen) {
        return chooseRecordSource(cueId, choices, chosen);
    });
    m_commandBar = new QWidget(this);
    {
        QVBoxLayout *v = new QVBoxLayout(m_commandBar);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        // help / ? : the language, with examples (a click puts one in the
        // line). Scrolls; Esc in the line closes it. Never a dialog.
        m_commandHelp = new QTextBrowser(m_commandBar);
        m_commandHelp->setObjectName("stCommandHelp");
        m_commandHelp->setOpenLinks(false);
        m_commandHelp->setMaximumHeight(170);
        m_commandHelp->hide();
        connect(m_commandHelp, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            const QString s = url.toString();
            if (!s.startsWith(QLatin1String("ex:")))
                return;
            const QString cmd = QUrl::fromPercentEncoding(s.mid(3).toUtf8());
            if (cmd.startsWith(QLatin1String("help")))
            {
                runCommand(cmd);                // another word's help
                return;
            }
            m_command->setText(cmd);
            m_command->setFocus(Qt::OtherFocusReason);
        });
        v->addWidget(m_commandHelp);
        QHBoxLayout *h = new QHBoxLayout();
        QLabel *lbl = new QLabel(tr("Command"), m_commandBar);
        h->addWidget(lbl);
        m_command = new QLineEdit(m_commandBar);
        m_command->setObjectName("stCommand");
        m_command->setPlaceholderText(tr("Type a command (: from the tree) — e.g. Towers colour Blue dimmer 45"));
        m_command->setToolTip(tr("Type what you'd say. Tab completes every word (again: the next match), "
                                 "Enter runs, ↑ recalls, Esc goes back to the tree. Every command is one "
                                 "undo step. From the tree, : jumps here."));
        // Never takes the keyboard on its own: a click, or : in the tree.
        m_command->setFocusPolicy(Qt::ClickFocus);
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
    connect(m_command, &QLineEdit::returnPressed, this, &ShowTreeView::slotCommandEntered);

    connect(m_newProduction, &QToolButton::clicked, this, &ShowTreeView::slotNewProduction);
    connect(m_newList, &QToolButton::clicked, this, &ShowTreeView::slotNewCueList);
    connect(m_newCue, &QToolButton::clicked, this, &ShowTreeView::slotNewCue);
    connect(m_addLayer, &QToolButton::clicked, this, &ShowTreeView::slotAddLayer);
    connect(m_record, &QToolButton::clicked, this, &ShowTreeView::slotRecordCue);
    connect(m_merge, &QToolButton::clicked, this, &ShowTreeView::slotMergeSelected);
    connect(m_delete, &QToolButton::clicked, this, &ShowTreeView::slotDelete);
    connect(m_undo, &QToolButton::clicked, this, &ShowTreeView::slotUndo);
    connect(m_redo, &QToolButton::clicked, this, &ShowTreeView::slotRedo);

    // Library | Tree | Editor, left to right (Branson, 2026-10-17: drags go
    // from the Library straight into the tree beside it). The Look Editor
    // docks under the editor column, for the palette of the row being
    // edited; the command line stays full width at the bottom.
    QSplitter *split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName("stSplitter");
    main->addWidget(split, 1);
    main->addWidget(m_commandBar);
    QSplitter *vsplit = new QSplitter(Qt::Vertical, split);
    vsplit->setObjectName("stVSplitter");
    m_lookEditor = new LookEditor(m_doc, vsplit);
    m_lookEditor->setObjectName("stLookEditor");
    vsplit->addWidget(m_lookEditor);
    m_lookEditor->hide();
    connect(m_lookEditor, &LookEditor::paletteChanged, this, [this](quint32 pid) {
        paletteEdited(pid, true);
    });
    connect(m_lookEditor, &LookEditor::paletteValueChanged, this, [this](quint32 pid) {
        paletteEdited(pid, false);
    });

    // Middle: the tree
    ShowTreeWidget *tree = new ShowTreeWidget(this, split);
    tree->setHint(m_hint);
    m_tree = tree;
    m_tree->installEventFilter(this);
    // ⌘C / ⌘V / ⌘D: cues and layers, in the tree only -- the command line
    // and the page's fields keep their own copy and paste.
    {
        QShortcut *copySc = new QShortcut(QKeySequence::Copy, m_tree);
        copySc->setContext(Qt::WidgetShortcut);
        connect(copySc, &QShortcut::activated, this, [this]() { copySelection(); });
        QShortcut *pasteSc = new QShortcut(QKeySequence::Paste, m_tree);
        pasteSc->setContext(Qt::WidgetShortcut);
        connect(pasteSc, &QShortcut::activated, this, [this]() { pasteClipboard(); });
        QShortcut *dupSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_D), m_tree);
        dupSc->setContext(Qt::WidgetShortcut);
        connect(dupSc, &QShortcut::activated, this, [this]() { duplicateSelection(); });
    }
    // ⌘F (or / in the tree): to the Library's filter -- type, Return adds.
    {
        QShortcut *findSc = new QShortcut(QKeySequence::Find, this);
        findSc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(findSc, &QShortcut::activated, this, [this]() { focusLibraryFilter(); });
    }
    // In-place rename (Return / F2): committed, or Esc and back as it was.
    connect(m_tree, &QTreeWidget::itemChanged, this, &ShowTreeView::slotTreeItemRenamed);
    connect(m_tree->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this]() {
        if (!m_renaming)
            return;
        m_renaming = false;
        QTimer::singleShot(0, this, [this]() { buildTree(); });
    });
    m_tree->setObjectName("stTree");
    m_tree->setHeaderLabels({ tr("Show"), tr("") });
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowTreeView::slotTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ShowTreeView::slotTreeContextMenu);
    split->addWidget(m_tree);

    // Right: the inspector (the page), the Look Editor under it
    m_pages = new QStackedWidget(vsplit);
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
        // P4: an imported list made tracking; the list out of the tree.
        QWidget *row = new QWidget(m_listPage);
        QHBoxLayout *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        m_listConvert = new QPushButton(tr("Convert to tracking…"), row);
        m_listConvert->setObjectName("stListConvert");
        m_listConvert->setToolTip(tr("Makes the list Tracking without changing what any cue plays: values a "
                                     "cue only repeats go, \"includes the cue before\" becomes tracking. Shows "
                                     "what changes first; one undo step."));
        m_listDetach = new QPushButton(tr("Detach to plain chaser…"), row);
        m_listDetach->setObjectName("stListDetach");
        m_listDetach->setToolTip(tr("The list's chaser and its cues' collections become ordinary functions you "
                                    "edit in the Functions window; the list leaves the Show Tree. One undo step."));
        h->addWidget(m_listConvert);
        h->addWidget(m_listDetach);
        h->addStretch(1);
        f->addRow(row);
        connect(m_listConvert, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ListNode)
                convertListToTracking(currentId());
        });
        connect(m_listDetach, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ListNode)
                detachListToChaser(currentId());
        });
    }
    m_pages->addWidget(m_listPage);

    // A cue (scrolls: its layers and what passes on can be long)
    QScrollArea *cueScroll = new QScrollArea(m_pages);
    cueScroll->setWidgetResizable(true);
    cueScroll->setFrameShape(QFrame::NoFrame);
    m_cuePage = cueScroll;
    {
        // The whole page takes drops: groups a new layer, palettes cue looks.
        DropPage *inner = new DropPage(this, cueScroll);
        inner->setObjectName("stCuePage");
        inner->setHint(m_hint);
        inner->stayOnCue = true;
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
        // P4: a use of a reusable cue (Unlink here, Used by), or Make reusable.
        m_cueReuse = new QLabel(inner);
        m_cueReuse->setObjectName("stCueReuse");
        m_cueReuse->setWordWrap(true);
        m_cueReuse->setTextFormat(Qt::RichText);
        connect(m_cueReuse, &QLabel::linkActivated, this, &ShowTreeView::reuseLinkActivated);
        f->addRow(tr("Reuse"), m_cueReuse);
        f->addRow(tr("Mode"), m_cueMode);
        m_cueBase = new QCheckBox(tr("The list's base cue (released layers go back to it)"), inner);
        m_cueBase->setObjectName("stCueBase");
        f->addRow(QString(), m_cueBase);
        connect(m_cueBase, &QCheckBox::toggled, this, &ShowTreeView::slotCueBaseToggled);
        m_cueMix = new QCheckBox(tr("Overlapping layers mix (as a chaser)"), inner);
        m_cueMix->setObjectName("stCueMix");
        m_cueMix->setToolTip(tr("Off (the tree's rule): where two layers share fixtures, the lower one wins "
                                "what both set, and the upper isn't lit there for it. On: they both play, "
                                "highest intensity wins -- how an imported chaser played, which an import "
                                "keeps. Child layers always win."));
        f->addRow(QString(), m_cueMix);
        connect(m_cueMix, &QCheckBox::toggled, this, &ShowTreeView::slotCueMixToggled);
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
            inc->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
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
        {
            QWidget *row = new QWidget(inner);
            QHBoxLayout *h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->addWidget(m_cueLooks, 1);
            // A line's Spread / Grouping / Cross for the whole cue (D18:
            // they reach the layers aiming at a line).
            QToolButton *line = new QToolButton(row);
            line->setObjectName("stCueLineParams");
            line->setText(tr("Line ⋯"));
            line->setToolTip(tr("Spread, Grouping and Cross for the cue: they reach the layers whose "
                                "Position aims at a line, as one formation"));
            connect(line, &QToolButton::clicked, this, [this, line]() {
                if (m_pageCue == invalidId())
                    return;
                QMenu menu(this);
                lineParamMenu(&menu, m_pageCue, invalidId(), Target());
                menu.exec(line->mapToGlobal(QPoint(0, line->height())));
            });
            h->addWidget(line, 0, Qt::AlignTop);
            f->addRow(tr("Cue looks"), row);
        }
        {
            // One row per layer (onto a row: added to that layer), and under
            // them the drop area for a new one.
            QWidget *box = new QWidget(inner);
            QVBoxLayout *v = new QVBoxLayout(box);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(4);
            CueLayerList *rows = new CueLayerList(this, inner, box);
            rows->setObjectName("stCueLayers");
            rows->setToolTip(tr("Drop groups or fixtures on a layer to add them to it, a palette to set "
                                "it there. Click a layer to open it."));
            m_cueLayers = rows;
            v->addWidget(m_cueLayers);
            m_cueDropZone = new QLabel(tr("Drag groups or fixtures here to add a layer — onto a layer to add to it"),
                                       box);
            m_cueDropZone->setObjectName("stCueDropZone");
            m_cueDropZone->setAlignment(Qt::AlignCenter);
            m_cueDropZone->setWordWrap(true);
            // The spare height of the page is the drop area.
            m_cueDropZone->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            m_cueDropZone->setToolTip(tr("Several groups dropped together make ONE layer on all of them; "
                                         "hold ⌥ for a layer each. A palette dropped here is a cue look; "
                                         "a function plays as it is."));
            v->addWidget(m_cueDropZone);
            auto zoneStyle = [this](bool active) {
                m_cueDropZone->setStyleSheet(active
                    ? QStringLiteral("QLabel { border: 2px solid palette(highlight); border-radius: 6px;"
                                     " padding: 12px; color: palette(highlight); }")
                    : QStringLiteral("QLabel { border: 2px dashed palette(mid); border-radius: 6px;"
                                     " padding: 12px; color: palette(mid); }"));
            };
            zoneStyle(false);
            inner->onActive = zoneStyle;
            f->addRow(tr("Layers"), box);
            connect(m_cueLayers, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
                if (item->data(KindRole).toInt() == LayerNode)
                    selectNode(item->data(IdRole).toUInt(), m_pageCue);
                else
                    selectTracked(m_pageCue, item->data(TargetRole).toString());
            });
        }
        m_cueNext = new QLabel(inner);
        m_cueNext->setWordWrap(true);
        f->addRow(tr("Next cue receives"), m_cueNext);
        for (QLineEdit *e : { m_cueNumber, m_cueName, m_cueNote })
            connect(e, &QLineEdit::editingFinished, this, &ShowTreeView::slotCueEdited);
        // Text fields don't swallow a group dragged over them: the page takes it.
        for (QWidget *w : inner->findChildren<QWidget *>())
            if (w != m_cueLayers && w->acceptDrops() && w->parentWidget() != m_cueLayers)
                w->setAcceptDrops(false);
        for (QDoubleSpinBox *s : { m_cueFadeIn, m_cueFadeOut, m_cueHold })
            connect(s, &QDoubleSpinBox::editingFinished, this, &ShowTreeView::slotCueEdited);
        connect(m_cueMode, QOverload<int>::of(&QComboBox::activated), this, &ShowTreeView::slotCueEdited);
    }
    m_pages->addWidget(m_cuePage);

    // A layer: one row per attribute
    DropPage *layerPage = new DropPage(this, m_pages);
    layerPage->setHint(m_hint);
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
        // What it holds: each group a link (to it in the Library), with ✕ to
        // take it out of the layer (never its last).
        m_layerContents = new QLabel(m_layerPage);
        m_layerContents->setObjectName("stLayerContents");
        m_layerContents->setWordWrap(true);
        m_layerContents->setTextFormat(Qt::RichText);
        connect(m_layerContents, &QLabel::linkActivated, this, [this](const QString &link) {
            const QStringList parts = link.split(QLatin1Char(':'));
            if (parts.count() != 3)
                return;
            const quint32 id = parts.at(2).toUInt();
            if (parts.at(0) == QLatin1String("rm"))
            {
                const quint32 cueId = currentCueId(), layerId = currentId();
                if (currentKind() != LayerNode)
                    return;
                if (parts.at(1) == QLatin1String("g"))
                    m_st->removeGroupFromLayer(cueId, layerId, id);
                else if (parts.at(1) == QLatin1String("f"))
                    m_st->removeFixtureFromLayer(cueId, layerId, id);
                else if (parts.at(1) == QLatin1String("s"))
                {
                    const Layer *l = m_st->cue(cueId) == nullptr ? nullptr : m_st->cue(cueId)->layer(layerId);
                    if (l != nullptr)
                    {
                        Target t = l->target;
                        t.sets.removeAll(id);
                        m_st->setLayerTarget(cueId, layerId, t, tr("Remove a set from layer"));
                    }
                }
                return;
            }
            // Show it in the Library (All).
            const int kind = parts.at(1) == QLatin1String("g") ? LibGroup
                             : parts.at(1) == QLatin1String("s") ? LibSet : LibFixture;
            for (QTreeWidgetItemIterator it(m_library); *it; ++it)
            {
                if ((*it)->data(0, KindRole).toInt() != kind || (*it)->data(0, IdRole).toUInt() != id)
                    continue;
                QTreeWidgetItem *top = *it;
                while (top->parent() != nullptr)
                    top = top->parent();
                if (top->data(0, IdRole).toUInt() != 2)
                    continue;                           // All's, not Recent's
                for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
                    p->setExpanded(true);
                m_library->clearSelection();
                m_library->setCurrentItem(*it);
                (*it)->setSelected(true);
                m_library->scrollToItem(*it);
                break;
            }
        });
        v->addWidget(m_layerContents);
        m_layerReuse = new QLabel(m_layerPage);
        m_layerReuse->setObjectName("stLayerReuse");
        m_layerReuse->setWordWrap(true);
        m_layerReuse->setTextFormat(Qt::RichText);
        connect(m_layerReuse, &QLabel::linkActivated, this, &ShowTreeView::reuseLinkActivated);
        v->addWidget(m_layerReuse);
        m_layerRelease = new QCheckBox(tr("Release when the cue is left (back to the base cue, else dark)"),
                                       m_layerPage);
        m_layerRelease->setObjectName("stLayerRelease");
        connect(m_layerRelease, &QCheckBox::toggled, this, &ShowTreeView::slotLayerReleaseToggled);
        // Several layers: a click releases them all -- unless all already
        // are, then it carries them all (a part-ticked box goes to ticked).
        connect(m_layerRelease, &QCheckBox::clicked, this, [this]() {
            if (m_inspectorUpdating || m_multi.count() < 2)
                return;
            bool all = true;
            for (const QPair<quint32, quint32> &ref : m_multi)
                if (const Cue *c = m_st->cue(ref.first))
                    if (const Layer *l = c->layer(ref.second))
                        all &= l->release;
            setReleaseOnSelected(!all);
        });
        v->addWidget(m_layerRelease);
        QScrollArea *scroll = new QScrollArea(m_layerPage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        m_rowsHost = new QWidget(scroll);
        m_rows = new QGridLayout(m_rowsHost);
        m_rows->setColumnStretch(1, 1);
        scroll->setWidget(m_rowsHost);
        v->addWidget(scroll, 1);
        QLabel *tip = new QLabel(tr("Drop a palette here (or on the layer in the tree) to set it, groups or fixtures to add them to the layer. "
                                    "Grey values come from the cue or an earlier cue; ⋯ sets one here."),
                                 m_layerPage);
        tip->setWordWrap(true);
        tip->setEnabled(false);
        v->addWidget(tip);
    }
    m_pages->addWidget(m_layerPage);
    vsplit->insertWidget(0, m_pages);
    vsplit->setStretchFactor(0, 3);
    vsplit->setStretchFactor(1, 2);

    // Left: the Library
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
        // Return / double-click: what is selected acts on the tree's
        // selection, as a drop on it would -- no dragging across.
        static_cast<LibraryWidget *>(m_library)->onReturn = [this](bool alt) { applyLibrarySelection(alt); };
        connect(m_library, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
            const int kind = item->data(0, KindRole).toInt();
            if (kind == LibSection || kind == LibFolder)
                return;
            applyLibrarySelection(QApplication::keyboardModifiers() & Qt::AltModifier);
        });
        connect(m_library, &QTreeWidget::itemSelectionChanged, this, [this]() {
            if (m_library->hasFocus() || m_filter->hasFocus())
                m_hint->setText(libraryReturnHint());
        });
        // The filter: Return takes the first match there is (or what is
        // selected) and adds it; ↓ goes into the list.
        m_filter->installEventFilter(this);
        v->addWidget(m_library, 1);
    }
    split->insertWidget(0, lib);
    split->insertWidget(1, m_tree);
    split->insertWidget(2, vsplit);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 3);
    {
        QSettings settings;
        const QVariant state = settings.value(SETTINGS_SPLITTER);
        if (state.isValid())
            split->restoreState(state.toByteArray());
        connect(split, &QSplitter::splitterMoved, this, [split]() {
            QSettings s;
            s.setValue(SETTINGS_SPLITTER, split->saveState());
        });
        const QVariant editorState = settings.value(SETTINGS_EDITOR_SPLITTER);
        if (editorState.isValid())
            vsplit->restoreState(editorState.toByteArray());
        connect(vsplit, &QSplitter::splitterMoved, this, [vsplit]() {
            QSettings s;
            s.setValue(SETTINGS_EDITOR_SPLITTER, vsplit->saveState());
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
    if (s_instance == this)
        s_instance = nullptr;
    delete m_commander;
}

/****************************************************************************
 * D13: what the tree made is changed in the tree
 ****************************************************************************/

ShowTreeView *ShowTreeView::instance()
{
    return s_instance;
}

bool ShowTreeView::reveal(quint32 functionId)
{
    quint32 listId = invalidId(), cueId = invalidId(), layerId = invalidId();
    if (!m_st->generatedNode(functionId, &listId, &cueId, &layerId))
        return false;
    const bool ok = layerId != invalidId() ? selectNode(layerId, cueId)
                    : cueId != invalidId() ? selectNode(cueId) : selectNode(listId);
    // Forward: its tab, and the window it is in.
    for (QWidget *w = parentWidget(); w != nullptr; w = w->parentWidget())
        if (QTabWidget *tabs = qobject_cast<QTabWidget *>(w))
        {
            for (int i = 0; i < tabs->count(); ++i)
                if (tabs->widget(i) == this || tabs->widget(i)->isAncestorOf(this))
                    tabs->setCurrentIndex(i);
            break;
        }
    if (window() != nullptr && window()->isVisible())
    {
        window()->raise();
        window()->activateWindow();
    }
    m_tree->setFocus(Qt::OtherFocusReason);
    return ok;
}

quint32 ShowTreeView::detachGenerated(Doc *doc, quint32 sceneId)
{
    ShowTreeController *st = doc->showTree();
    if (qobject_cast<Scene *>(doc->function(sceneId)) == nullptr)
        return Function::invalidId();
    quint32 listId = invalidId(), cueId = invalidId(), layerId = invalidId();
    if (!st->generatedNode(sceneId, &listId, &cueId, &layerId) || layerId == invalidId())
        return Function::invalidId();
    return st->detachLayer(cueId, layerId);
}

QWidget *ShowTreeView::lockedBanner(QWidget *parent, Doc *doc, quint32 functionId,
                                    const std::function<void(quint32)> &detached)
{
    ShowTreeController *st = doc->showTree();
    QFrame *bar = new QFrame(parent);
    bar->setObjectName(QStringLiteral("stLockedBanner"));
    bar->setFrameShape(QFrame::StyledPanel);
    QHBoxLayout *h = new QHBoxLayout(bar);
    QLabel *text = new QLabel(bar);
    text->setObjectName(QStringLiteral("stLockedText"));
    text->setWordWrap(true);
    const QString what = st->describeGenerated(functionId);
    QString body = QStringLiteral("<b>%1</b>").arg(ShowTreeController::lockedText().toHtmlEscaped());
    if (!what.isEmpty())
        body += QStringLiteral("<br>") + tr("It is %1.").arg(what.toHtmlEscaped());
    if (qobject_cast<Chaser *>(doc->function(functionId)) != nullptr)
        body += QStringLiteral("<br>") + tr("Its run order, direction and speed modes are yours to set "
                                            "here; its steps and name are the tree's.");
    text->setText(body);
    h->addWidget(text, 1);

    QPushButton *show = new QPushButton(tr("Show in Show Tree"), bar);
    show->setObjectName(QStringLiteral("stLockedReveal"));
    show->setEnabled(instance() != nullptr);
    QObject::connect(show, &QPushButton::clicked, bar, [functionId]() {
        if (ShowTreeView *v = instance())
            v->reveal(functionId);
    });
    h->addWidget(show);

    // A layer's scene can be detached (D13): a plain copy the tree then
    // plays as it is, which is edited like any scene.
    quint32 listId = invalidId(), cueId = invalidId(), layerId = invalidId();
    if (detached && qobject_cast<Scene *>(doc->function(functionId)) != nullptr
        && st->generatedNode(functionId, &listId, &cueId, &layerId) && layerId != invalidId())
    {
        QPushButton *detach = new QPushButton(tr("Detach to plain scene"), bar);
        detach->setObjectName(QStringLiteral("stLockedDetach"));
        const CueList *list = st->cueList(listId);
        if (list != nullptr)
            detach->setToolTip(tr("A plain copy of this layer's scene, which cue %1 then plays as it is: "
                                  "edit that one. The tree no longer sets this layer's looks. (Undo in "
                                  "the Show Tree puts it back.)")
                               .arg(st->cueLabel(*list, list->indexOf(cueId))));
        QObject::connect(detach, &QPushButton::clicked, bar, [doc, functionId, detached]() {
            const quint32 sid = detachGenerated(doc, functionId);
            if (sid != Function::invalidId())
                detached(sid);
        });
        h->addWidget(detach);
    }
    return bar;
}

bool ShowTreeView::refuseGenerated(QWidget *parent, Doc *doc, const QList<quint32> &functionIds,
                                   const QString &title)
{
    ShowTreeController *st = doc->showTree();
    QList<quint32> made;
    for (quint32 fid : functionIds)
        if (st->isGenerated(fid) && !made.contains(fid))
            made << fid;
    if (made.isEmpty())
        return false;
    QStringList names;
    for (quint32 fid : made)
    {
        const Function *f = doc->function(fid);
        const QString what = st->describeGenerated(fid);
        names << (what.isEmpty() ? (f == nullptr ? QString::number(fid) : f->name())
                                 : QStringLiteral("%1 (%2)").arg(f == nullptr ? QString::number(fid) : f->name(),
                                                                 what));
    }
    QMessageBox box(QMessageBox::Information, title, ShowTreeController::lockedText(), QMessageBox::Close, parent);
    box.setObjectName(QStringLiteral("stLockedMessage"));
    box.setInformativeText(names.join(QLatin1Char('\n')));
    QPushButton *show = nullptr;
    if (instance() != nullptr)
    {
        show = box.addButton(tr("Show in Show Tree"), QMessageBox::AcceptRole);
        show->setObjectName(QStringLiteral("stLockedReveal"));
    }
    box.exec();
    if (show != nullptr && box.clickedButton() == show && instance() != nullptr)
        instance()->reveal(made.first());
    return true;
}

/****************************************************************************
 * The tree
 ****************************************************************************/

bool ShowTreeView::startRename()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (item == nullptr || m_tree->selectedItems().count() > 1)
        return false;
    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    QString text;
    if (kind == ProductionNode)
    {
        const Production *p = m_st->production(id);
        text = p == nullptr ? QString() : p->name;
    }
    else if (kind == ListNode)
    {
        const CueList *l = m_st->cueList(id);
        text = l == nullptr ? QString() : l->name;
    }
    else if (kind == CueNode)
    {
        const Cue *c = m_st->cue(id);
        text = c == nullptr ? QString() : c->name;     // its name; the number stays
    }
    else if (kind == LayerNode)
    {
        const Cue *c = m_st->cue(item->data(0, CueRole).toUInt());
        const Layer *l = c == nullptr ? nullptr : c->layer(id);
        if (l == nullptr)
            return false;
        text = l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name;
    }
    else
        return false;
    // The bare name to edit (a cue's row shows its number, a layer's ▣).
    m_reloading = true;
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setText(0, text);
    m_reloading = false;
    m_renaming = true;
    m_tree->editItem(item, 0);
    return true;
}

void ShowTreeView::slotTreeItemRenamed(QTreeWidgetItem *item, int column)
{
    if (!m_renaming || m_reloading || column != 0 || item == nullptr)
        return;
    m_renaming = false;
    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    QString name = item->text(0).trimmed();
    if (kind == LayerNode)
    {
        // Its groups' name again (or nothing): the automatic name.
        const Cue *c = m_st->cue(item->data(0, CueRole).toUInt());
        const Layer *l = c == nullptr ? nullptr : c->layer(id);
        if (l != nullptr && name == m_st->targetLabel(l->target))
            name.clear();
    }
    // After the editor is done with the row: the rename rebuilds the tree.
    const quint32 cueId = item->data(0, CueRole).toUInt();
    QTimer::singleShot(0, this, [this, id, name, cueId, kind]() {
        if (!m_st->renameNode(id, name))
            buildTree();                                    // as it was
        if (kind == LayerNode)
            selectNode(id, cueId);
        m_tree->setFocus(Qt::OtherFocusReason);
    });
}

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
    // The rest of a multi-selection (own rows: a carried-in row has no id).
    QList<QPair<quint32, quint32>> alsoSelected;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
        if (item != m_tree->currentItem() && item->data(0, KindRole).toInt() != TrackedLayerNode)
            alsoSelected << qMakePair(item->data(0, IdRole).toUInt(), item->data(0, CueRole).toUInt());
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
            const QList<ResolvedCue> resolved = m_st->resolved(list);

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
                if (cue.reusable != invalidId())
                {
                    // A use of a reusable cue (P4): ⟲, and how many there are.
                    ci->setText(0, QStringLiteral("⟲ ") + ci->text(0));
                    badges << tr("reusable ×%1").arg(m_st->reusableUses(cue.reusable).count());
                }
                ci->setText(1, badges.join(QStringLiteral(" ")));
                ci->setToolTip(0, cue.reusable == invalidId() ? cue.note
                               : tr("A use of the reusable cue %1: an edit here is an edit to every use. %2")
                                 .arg(m_st->reusableName(cue.reusable), cue.note).trimmed());
                ci->setData(0, KindRole, CueNode);
                ci->setData(0, IdRole, cue.id);

                // A child layer sits under its parent's row (§3.5).
                QHash<quint32, QTreeWidgetItem *> layerItems;
                for (const Layer &layer : cue.layers)
                {
                    QTreeWidgetItem *under = layer.parent == invalidId() ? nullptr : layerItems.value(layer.parent);
                    QTreeWidgetItem *yi = new QTreeWidgetItem(under != nullptr ? under : ci);
                    layerItems.insert(layer.id, yi);
                    // Its name (Branson: the short name only); what it holds
                    // is in the tooltip, and on its page when selected.
                    QString name = layer.name.isEmpty() ? m_st->targetLabel(layer.target) : layer.name;
                    if (layer.target.isOpaque())
                        name = tr("▣ %1").arg(name);
                    if (layer.reusable != invalidId())
                        name = QStringLiteral("⟲ ") + name;
                    yi->setText(0, name);
                    QStringList looks;
                    for (const Look &l : layer.looks)
                        looks << lookText(l);
                    QStringList col1;
                    if (layer.release)
                        col1 << tr("release");
                    if (layer.reusable != invalidId())
                        col1 << tr("×%1").arg(m_st->reusableUses(layer.reusable).count());
                    yi->setText(1, col1.join(QStringLiteral(" ")));
                    QString tip = QStringLiteral("%1\n%2").arg(m_st->targetContents(layer.target),
                                                               looks.join(QStringLiteral(", "))).trimmed();
                    if (layer.reusable != invalidId())
                        tip = tr("Reusable layer: an edit here is an edit to every use (%1).\n")
                              .arg(m_st->reusableUses(layer.reusable).count()) + tip;
                    yi->setToolTip(0, tip);
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
                        QString tname = rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name;
                        if (rl.depth > 0)
                            tname = tr("%1 (in %2)").arg(tname, m_st->targetLabel(rl.parent));
                        ti->setText(0, QStringLiteral("⤳ %1").arg(tname));
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
            m_tree->setCurrentItem(found, 0, kSelectOnly);
    }
    else if (selId != invalidId())
    {
        if (QTreeWidgetItem *item = findItem(selId, selCue))
            m_tree->setCurrentItem(item, 0, kSelectOnly);
    }
    for (const QPair<quint32, quint32> &sel : alsoSelected)
        if (QTreeWidgetItem *item = findItem(sel.first, sel.second))
            item->setSelected(true);
    m_reloading = false;
}

bool ShowTreeView::selectNode(quint32 id, quint32 cueId)
{
    QTreeWidgetItem *item = findItem(id, cueId);
    if (item == nullptr)
        return false;
    for (QTreeWidgetItem *p = item->parent(); p != nullptr; p = p->parent())
        p->setExpanded(true);
    m_tree->setCurrentItem(item, 0, kSelectOnly);
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
    // The command line says what the selected layer holds.
    {
        QString holds;
        if (currentKind() == LayerNode)
            if (const Cue *c = m_st->cue(currentCueId()))
                if (const Layer *l = c->layer(currentId()))
                    holds = tr("Layer %1: %2 — e.g. colour Blue, or name \"…\"")
                            .arg(l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name,
                                 m_st->targetContents(l->target));
        m_command->setPlaceholderText(holds.isEmpty()
                                      ? tr("Type a command (: from the tree) — e.g. Towers colour Blue dimmer 45, or help")
                                      : holds);
    }
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
    m_merge->setEnabled(selectedLayers().count() > 1);
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
    if (!look.value.isEmpty())
        return isLineParamKey(look.key) ? tr("%1 %2").arg(keyLabel(look.key),
                                                         ShowTreeController::lineParamText(look.key, look.value))
                                        : look.value;
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

/* Whether Convert to tracking… has anything to try (cheap: the plan
   itself resolves the list many times over). */
static bool mayConvert(const CueList *list)
{
    if (list == nullptr || list->cues.count() < 2)
        return false;
    if (list->mode == ListMode::NonTracking)
        return true;
    for (const Cue &c : list->cues)
        if (c.mode == CueMode::Block || !c.includes.isEmpty())
            return true;
    return false;
}

void ShowTreeView::updateInspector()
{
    m_inspectorUpdating = true;
    m_pageCue = invalidId();
    m_multi.clear();
    m_layerRelease->setTristate(false);
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
            m_listConvert->setEnabled(mayConvert(list));
            m_listDetach->setEnabled(true);
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
    else if (kind == LayerNode && selectedLayerRefs().count() > 1
             && selectedLayerRefs().contains(qMakePair(cueId, id)))
    {
        // Several layers selected: one page for all of them.
        showMultiLayerPage(selectedLayerRefs());
    }
    else
    {
        const QList<ResolvedCue> resolved = m_st->resolved(*list);
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
    {
        QSignalBlocker block(m_cueMix);
        m_cueMix->setChecked(cue.mixOverlap);
    }
    m_cueReuse->setText(reuseLine(cue.id, invalidId()));

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
    m_cueLooks->setText(looks.isEmpty() ? tr("— (drop a palette here or on the cue)") : looks.join(QStringLiteral("\n")));

    // The layers, one row each: the cue's own, then (grey) the carried-in.
    // Each takes drops; the page's cue is where the rest go.
    static_cast<DropPage *>(static_cast<QScrollArea *>(m_cuePage)->widget())->cue = cue.id;
    CueLayerList *rows = static_cast<CueLayerList *>(m_cueLayers);
    rows->clearDropRow();
    rows->clear();
    rows->targets.clear();
    const QList<ResolvedCue> resolved = m_st->resolved(list);
    const QColor grey = palette().color(QPalette::Disabled, QPalette::Text);
    for (const ResolvedLayer &rl : resolved.at(index).layers)
    {
        QString name = rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name;
        if (rl.depth > 0)
            name = QString(rl.depth * 3, QLatin1Char(' ')) + QStringLiteral("↳ ") + name;
        QStringList vals;
        for (const Look &l : rl.playedLooks())
            vals << lookText(l);
        QString line = vals.isEmpty() ? name : QStringLiteral("%1   %2").arg(name, vals.join(QStringLiteral(", ")));
        QListWidgetItem *it = new QListWidgetItem(rows);
        if (rl.own)
        {
            if (rl.release)
                line += tr(" · release");
            it->setData(KindRole, LayerNode);
            it->setData(IdRole, rl.layerId);
            rows->targets << Target();
        }
        else
        {
            const int from = list.indexOf(rl.fromCue);
            const QString src = from < 0 ? QString() : m_st->cueLabel(list, from);
            line = QStringLiteral("⤳ %1 · %2").arg(line, rl.origin == Origin::Included ? tr("included from %1").arg(src)
                                                        : rl.origin == Origin::Base ? tr("base, from %1").arg(src)
                                                        : tr("tracked from %1").arg(src));
            it->setForeground(grey);
            it->setData(KindRole, TrackedLayerNode);
            it->setData(IdRole, rl.layerId);
            rows->targets << rl.target;
        }
        it->setData(TargetRole, rl.target.signature());
        // A named layer: what it holds, compact, on a line under it.
        if (!rl.name.isEmpty() && !rl.target.isOpaque())
            line += QStringLiteral("\n") + QString(rl.depth * 3 + 4, QLatin1Char(' '))
                    + m_st->targetContents(rl.target);
        it->setText(line);
        it->setToolTip(rl.own ? tr("Drop groups here to add them to %1, a palette to set it").arg(name)
                              : tr("Carried into this cue: a drop here sets it in this cue"));
    }
    if (rows->count() == 0)
    {
        rows->hide();
        m_cueDropZone->setMinimumHeight(90);
    }
    else
    {
        int h = 0;
        for (int r = 0; r < rows->count(); ++r)
            h += qMax(rows->sizeHintForRow(r),
                      (rows->item(r)->text().count(QLatin1Char('\n')) + 1) * fontMetrics().height() + 6);
        rows->setFixedHeight(h + 2 * rows->frameWidth() + 2);
        rows->show();
        m_cueDropZone->setMinimumHeight(0);
    }
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

    const QList<ResolvedCue> resolved = m_st->resolved(*list);
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
    // Nothing to drop on when the layer isn't on stage here.
    page->cue = rl == nullptr ? invalidId() : cue.id;
    page->layer = own == nullptr ? invalidId() : own->id;
    page->target = rl == nullptr || own != nullptr ? Target() : rl->target;

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

    m_layerReuse->setVisible(own != nullptr);
    if (own != nullptr)
        m_layerReuse->setText(reuseLine(cue.id, own->id));
    if (rl == nullptr)
    {
        m_layerContents->setVisible(false);
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
    // What it holds: groups (links to them in the Library) with counts, ✕
    // to take one out (an own layer on more than one).
    {
        const Target &t = rl->target;
        QStringList parts;
        const int units = int(t.groups.count() + t.fixtures.count() + t.sets.count());
        const bool removable = own != nullptr && units > 1;
        auto part = [&](const QString &kind, quint32 id, const QString &text) {
            QString out = QStringLiteral("<a href=\"show:%1:%2\">%3</a>").arg(kind).arg(id).arg(text.toHtmlEscaped());
            if (removable)
                out += QStringLiteral(" <a href=\"rm:%1:%2\" title=\"%3\">✕</a>").arg(kind).arg(id)
                       .arg(tr("Take it out of this layer").toHtmlEscaped());
            return out;
        };
        if (t.isOpaque())
            parts << m_st->targetContents(t).toHtmlEscaped();
        for (quint32 set : t.sets)
        {
            Target one;
            one.sets << set;
            parts << part(QStringLiteral("s"), set, m_st->targetContents(one).section(QStringLiteral(" — "), 0, 0));
        }
        for (quint32 g : t.groups)
        {
            const FixtureGroup *grp = m_doc->fixtureGroup(g);
            parts << part(QStringLiteral("g"), g, grp == nullptr ? tr("(missing group)")
                          : QStringLiteral("%1 (%2)").arg(grp->name()).arg(grp->fixtureList().count()));
        }
        for (quint32 f : t.fixtures)
        {
            const Fixture *fxi = m_doc->fixture(f);
            parts << part(QStringLiteral("f"), f, fxi == nullptr ? tr("(missing fixture)") : fxi->name());
        }
        QString text = tr("Contains: %1").arg(parts.join(QStringLiteral(" · ")));
        if (!t.isOpaque())
            text += tr(" — %1 fixture(s)").arg(m_st->targetFixtures(t).count());
        m_layerContents->setText(text);
        m_layerContents->setToolTip(m_st->targetContents(t));
        m_layerContents->setVisible(true);
    }
    // A child layer (§3.5): what it doesn't set, its parent lights.
    QHash<QString, QPair<ResolvedValue, QString>> inherited;
    if (rl->depth > 0)
    {
        const QList<ResolvedCue> rcs = m_st->resolved(list);
        Target up = rl->parent;
        for (int guard = 0; guard < 16 && !up.isEmpty() && index < rcs.count(); ++guard)
        {
            const ResolvedLayer *p = rcs.at(index).layer(up);
            if (p == nullptr)
                break;
            const QString pname = p->name.isEmpty() ? m_st->targetLabel(p->target) : p->name;
            for (auto it = p->values.constBegin(); it != p->values.constEnd(); ++it)
                if (!inherited.contains(it.key()))
                    inherited.insert(it.key(), qMakePair(it.value(), pname));
            up = p->parent;
        }
        m_layerTitle->setText(m_layerTitle->text()
                              + tr(" — a child layer of %1: it sets only what it sets").arg(m_st->targetLabel(rl->parent)));
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
    // A line aim's Spread / Grouping / Cross (§3B, D16): rows of their own
    // under Position, when it aims at a line (or they are set anyway).
    const bool lineAim = rl->values.contains(KeyPosition) && m_st->isLineAim(rl->values.value(KeyPosition).look);
    if (lineAim)
        keys << KeySpread << KeyGrouping << KeyCross;
    for (const QString &k : rl->values.keys())
        if (!keys.contains(k))
            keys << k;
    for (const QString &k : inherited.keys())
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
        if (key.startsWith(QLatin1String("Beam.")) || isLineParamKey(key))
            label->setText(QStringLiteral("   ") + label->text());     // Beam's / a line aim's children
        QLabel *value = new QLabel(m_rowsHost);
        value->setObjectName(QStringLiteral("stRow:%1").arg(key));
        QString text, origin;
        if (!addEffect && rl->values.contains(key))
        {
            const ResolvedValue &v = rl->values.value(key);
            text = isLineParamKey(key) && !v.look.none ? ShowTreeController::lineParamText(key, v.look.value)
                                                       : lookText(v.look);
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
        else if (isLineParamKey(key) && lineAim)
        {
            // Not set: the Aim look's own (its default +5, each light, swap).
            const QLCPalette *aimPal = m_doc->palette(rl->values.value(KeyPosition).look.palettes.value(0));
            const LineSpread::Settings ls = aimPal == nullptr ? LineSpread::Settings()
                                                              : LineSpread::fromParams(aimPal->effectParamValues());
            const QString v = key == KeySpread ? QString::number(ls.spread)
                              : key == KeyCross ? (ls.cross == LineSpread::Mirror ? QStringLiteral("mirror")
                                                                                   : QStringLiteral("swap"))
                              : ls.group == LineSpread::Halves ? QStringLiteral("halves")
                              : ls.group == 2 ? QStringLiteral("pairs") : ls.group == 3 ? QStringLiteral("threes")
                              : ls.group == 4 ? QStringLiteral("fours") : QStringLiteral("each");
            text = ShowTreeController::lineParamText(key, v);
            origin = tr("the Aim look's");
            QPalette pal = value->palette();
            pal.setBrush(QPalette::WindowText, grey);
            value->setPalette(pal);
        }
        else if (!addEffect && inherited.contains(key))
        {
            // Lit by the parent (a child layer): grey, and says so.
            text = lookText(inherited.value(key).first.look);
            origin = tr("from %1 (parent)").arg(inherited.value(key).second);
            QPalette pal = value->palette();
            pal.setBrush(QPalette::WindowText, grey);
            value->setPalette(pal);
            QFont f = value->font();
            f.setItalic(true);
            value->setFont(f);
        }
        else
        {
            text = QStringLiteral("—");
        }
        // ONE effect / ONE formation with other layers (D10, D17).
        if (!addEffect && (key == KeyPosition || isEffectKey(key)))
        {
            QStringList others;
            for (const Target &t : m_st->spanMembers(cue.id, rl->target, key))
                if (!(t == rl->target))
                    others << m_st->targetLabel(t);
            if (!others.isEmpty())
            {
                const QString with = key == KeyPosition ? tr("one formation with %1").arg(others.join(QStringLiteral(" + ")))
                                                        : tr("one effect with %1").arg(others.join(QStringLiteral(" + ")));
                origin = origin.isEmpty() ? with : QStringLiteral("%1 · %2").arg(origin, with);
            }
        }
        value->setText(origin.isEmpty() ? text : QStringLiteral("%1 — %2").arg(text, origin));
        value->setWordWrap(true);
        // A one-off value set here: "Make reusable" right by it, one click
        // (named and filed as the show's palettes of that type are).
        QWidget *valueCell = value;
        if (!addEffect && rl->values.contains(key) && rl->values.value(key).origin == Origin::Own)
        {
            QList<quint32> oneOffs;
            for (quint32 pid : rl->values.value(key).look.palettes)
                if (m_st->isInlinePalette(pid))
                    oneOffs << pid;
            if (!oneOffs.isEmpty())
            {
                valueCell = new QWidget(m_rowsHost);
                QHBoxLayout *h = new QHBoxLayout(valueCell);
                h->setContentsMargins(0, 0, 0, 0);
                value->setParent(valueCell);
                h->addWidget(value, 1);
                QLabel *reuse = new QLabel(QStringLiteral("<a href=\"reuse\">%1</a>").arg(tr("Make reusable")),
                                           valueCell);
                reuse->setObjectName(QStringLiteral("stReuse:%1").arg(key));
                reuse->setTextFormat(Qt::RichText);
                reuse->setToolTip(tr("Keep this value as a palette (named as the show's are, e.g. \"%1\"), "
                                     "so it can be reused and adjusted everywhere at once")
                                  .arg(m_doc->palette(oneOffs.first()) == nullptr ? QString()
                                       : m_st->suggestPaletteName(m_doc->palette(oneOffs.first())->type(),
                                                                  m_doc->palette(oneOffs.first())->values())));
                connect(reuse, &QLabel::linkActivated, this, [this, oneOffs]() {
                    m_st->makeReusable(oneOffs);    // one undo step, however many
                    reloadLibrary();
                });
                h->addWidget(reuse);
            }
        }

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
            if (isLineParamKey(key))
            {
                QMenu menu(this);
                lineParamMenu(&menu, cueId, layerId, layerId == invalidId() ? target : Target(), key);
                menu.exec(at);
                return;
            }
            attributeMenu(cueId, layerId, layerId == invalidId() ? target : Target(), target, key, at);
        });

        m_rows->addWidget(label, row, 0);
        m_rows->addWidget(valueCell, row, 1);
        m_rows->addWidget(reach, row, 2);
        m_rows->addWidget(editBtn, row, 3);
        m_rows->addWidget(more, row, 4);
        m_rowText << QStringLiteral("%1 | %2 | %3").arg(label->text().trimmed(), text, origin);
        ++row;
        // Under a line aim's Position: where each light lands, numbered by
        // where they hang along the line (the formation's, if one).
        if (key == KeyPosition && lineAim)
        {
            const Scene *sc = qobject_cast<const Scene *>(m_doc->function(m_st->layerScene(cue.id, rl->target)));
            const QLCPalette *aimPal = nullptr;
            if (sc != nullptr)
                for (quint32 pid : sc->palettes())
                    if (const QLCPalette *p = m_doc->palette(pid))
                        if (p->type() == QLCPalette::Aim)
                            aimPal = p;
            if (aimPal == nullptr)
                aimPal = m_doc->palette(rl->values.value(KeyPosition).look.palettes.value(0));
            LineSpreadPreview *strip = new LineSpreadPreview(m_doc, m_rowsHost);
            strip->setObjectName("stLineStrip");
            strip->setLook(sc, aimPal);
            m_rows->addWidget(strip, row, 1, 1, 4);
            ++row;
        }
    }
    m_rows->setRowStretch(row, 1);
    m_pages->setCurrentWidget(m_layerPage);
}

QStringList ShowTreeView::attributeRows() const
{
    return m_rowText;
}

QString ShowTreeView::layerContentsText() const
{
    QTextDocument doc;
    doc.setHtml(m_layerContents->text());
    return doc.toPlainText();
}

QWidget *ShowTreeView::lineStrip() const
{
    return m_rowsHost == nullptr ? nullptr : m_rowsHost->findChild<QWidget *>("stLineStrip");
}

bool ShowTreeView::setLineParam(quint32 cueId, quint32 layerId, const QString &key, const QString &value,
                                const Target &target)
{
    Look l;
    l.key = key;
    if (!value.isEmpty())
    {
        l = ShowTreeController::lineParamLook(key, value);
        if (l.value.isEmpty())
            return false;
    }
    if (value.isEmpty() && layerId == invalidId() && !target.isEmpty())
        return false;                       // nothing to clear on a carried-in layer
    return applyLook(cueId, layerId, target, l);
}

void ShowTreeView::lineParamMenu(QMenu *menu, quint32 cueId, quint32 layerId, const Target &target,
                                 const QString &onlyKey)
{
    auto add = [&](QMenu *m, const QString &key, const QStringList &values) {
        for (const QString &v : values)
            m->addAction(ShowTreeController::lineParamText(key, v), this, [this, cueId, layerId, key, v, target]() {
                setLineParam(cueId, layerId, key, v, target);
            });
        if (key == KeySpread)
            m->addAction(tr("Spread…"), this, [this, cueId, layerId, target]() {
                bool ok = false;
                const double d = QInputDialog::getDouble(this, tr("Spread"),
                                                         tr("-5 crossed … 0 all on the centre … +5 end to end"),
                                                         5.0, -5.0, 5.0, 1, &ok, Qt::WindowFlags(), 0.1);
                if (ok)
                    setLineParam(cueId, layerId, KeySpread, QString::number(d), target);
            });
        m->addSeparator();
        m->addAction(tr("Clear (inherit)"), this, [this, cueId, layerId, key, target]() {
            setLineParam(cueId, layerId, key, QString(), target);
        });
    };
    const QList<QPair<QString, QStringList>> all = {
        { KeySpread, { "5", "2.5", "0", "-2.5", "-5" } },
        { KeyGrouping, { "each", "pairs", "threes", "fours", "halves" } },
        { KeyCross, { "swap", "mirror" } } };
    for (const auto &k : all)
    {
        if (!onlyKey.isEmpty() && k.first != onlyKey)
            continue;
        add(onlyKey.isEmpty() ? menu->addMenu(keyLabel(k.first)) : menu, k.first, k.second);
    }
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
        menu.addAction(tr("Level…"), this, [this, cueId, layerId, target]() {
            bool ok = false;
            const int pc = QInputDialog::getInt(this, tr("Dimmer"), tr("Level (%)"), 100, 0, 100, 1, &ok);
            if (!ok)
                return;
            const quint32 pid = valuePalette(cueId, layerId, target, QLCPalette::Dimmer,
                                             { qBound(0, qRound(pc * 255.0 / 100.0), 255) },
                                             tr("%1%").arg(pc));
            if (pid != QLCPalette::invalidId())
                applyLook(cueId, layerId, target, m_st->lookForPalette(pid));
        });
    }
    else if (key == KeyColour)
    {
        menu.addAction(tr("Colour…"), this, [this, cueId, layerId, target]() {
            const QColor c = QColorDialog::getColor(Qt::white, this, tr("Colour"));
            if (!c.isValid())
                return;
            const quint32 pid = valuePalette(cueId, layerId, target, QLCPalette::Color,
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

bool ShowTreeView::isMulti(quint32 cueId, quint32 layerId) const
{
    return !m_applyingMulti && m_multi.count() > 1 && m_multi.contains(qMakePair(cueId, layerId));
}

bool ShowTreeView::forEachEdited(quint32 cueId, quint32 layerId, const QString &label,
                                 const std::function<bool(quint32, quint32)> &one)
{
    if (!isMulti(cueId, layerId))
        return one(cueId, layerId);
    // Every selected layer, one undo step.
    const QList<QPair<quint32, quint32>> all = m_multi;
    m_st->beginBatch(label);
    m_applyingMulti = true;
    bool any = false;
    for (const QPair<quint32, quint32> &ref : all)
        any |= one(ref.first, ref.second);
    m_applyingMulti = false;
    m_st->endBatch(any);
    return any;
}

bool ShowTreeView::applyLook(quint32 cueId, quint32 layerId, const Target &target, const Look &look)
{
    if (isMulti(cueId, layerId))
        return forEachEdited(cueId, layerId, tr("Set %1 on %2 layers").arg(keyLabel(look.key)).arg(m_multi.count()),
                             [this, &look](quint32 c, quint32 l) { return m_st->setLayerLook(c, l, look); });
    if (layerId != invalidId())
        return m_st->setLayerLook(cueId, layerId, look);
    if (!target.isEmpty())
        return m_st->setTargetLook(cueId, target, look);
    return m_st->setCueLook(cueId, look);
}

QList<quint32> ShowTreeView::recentPalettes() const
{
    QList<quint32> out;
    for (const QPair<int, quint32> &r : m_recent)
        if (r.first == LibPalette)
            out << r.second;
    return out;
}

quint32 ShowTreeView::valuePalette(quint32 cueId, quint32 layerId, const Target &target,
                                   QLCPalette::PaletteType type, const QVariantList &values, const QString &value)
{
    // A palette already playing exactly that is reused (In this cue list,
    // Recent, then any), so adjusting it adjusts every cue; else a one-off.
    const CueList *list = m_st->listOfCue(cueId);
    const quint32 have = m_st->findPalette(type, values, list == nullptr ? invalidId() : list->id,
                                           recentPalettes());
    if (have != QLCPalette::invalidId())
    {
        noteRecent(LibPalette, have);
        return have;
    }
    return inlinePalette(cueId, layerId, target, type, values, value);
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
    if (m_multi.count() > 1)
        return;             // several layers: on click (below), all at once
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

void ShowTreeView::slotCueMixToggled(bool on)
{
    if (m_inspectorUpdating || m_pageCue == invalidId())
        return;
    m_st->setCueMixOverlap(m_pageCue, on);
}

void ShowTreeView::slotRecordCue()
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
        return;
    recordFrom(cueId);
}

QString ShowTreeView::recordSettingKey(bool perform)
{
    return perform ? QStringLiteral("showtree/recordPerform") : QStringLiteral("showtree/recordDesign");
}

QString ShowTreeView::recordSetting(bool perform)
{
    const QString v = QSettings().value(recordSettingKey(perform), QStringLiteral("ask")).toString();
    return ShowTreeController::stringToRecordSource(v, nullptr) ? v : QStringLiteral("ask");
}

quint32 ShowTreeView::recordFrom(quint32 cueId)
{
    const QList<RecordSource> choices = m_st->recordChoices(cueId);
    RecordSource src = RecordSource::Selected;
    if (choices.count() == 1)
        src = choices.first();
    else if (choices.count() > 1 && !chooseRecordSource(cueId, choices, &src))
        return invalidId();
    const quint32 id = m_st->recordCue(cueId, src);
    if (id != invalidId())
        selectNode(id);
    return id;
}

bool ShowTreeView::chooseRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen)
{
    const bool perform = m_doc->mode() == Doc::Operate;
    RecordSource src;
    // The setting decides when what it names can be recorded now.
    if (ShowTreeController::stringToRecordSource(recordSetting(perform), &src)
        && (src == RecordSource::Selected || m_st->canRecord(cueId, src)))
    {
        *chosen = src;
        return true;
    }
    bool dontAsk = false;
    const bool ok = m_recordAsker ? m_recordAsker(choices, chosen, &dontAsk)
                                  : askRecordSource(cueId, choices, chosen, &dontAsk);
    if (!ok)
        return false;
    if (dontAsk)
        QSettings().setValue(recordSettingKey(perform), ShowTreeController::recordSourceToString(*chosen));
    return true;
}

bool ShowTreeView::askRecordSource(quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen,
                                   bool *dontAsk)
{
    const bool perform = m_doc->mode() == Doc::Operate;
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr)
        return false;
    QDialog dlg(this);
    dlg.setObjectName("stRecordDialog");
    dlg.setWindowTitle(tr("Record as new cue"));
    QVBoxLayout *v = new QVBoxLayout(&dlg);
    QLabel *what = new QLabel(tr("What is on stage differs. Record which, as a new Block cue after %1?")
                              .arg(m_st->cueLabel(*list, list->indexOf(cueId))), &dlg);
    what->setWordWrap(true);
    v->addWidget(what);
    QList<QPair<QRadioButton *, RecordSource>> radios;
    for (RecordSource src : choices)
    {
        const int layers = m_st->recordLayers(cueId, src).count();
        QString text;
        if (src == RecordSource::Selected)
            text = tr("The selected cue — %1 (%n layer(s))", "", layers)
                   .arg(m_st->cueLabel(*list, list->indexOf(cueId)));
        else if (src == RecordSource::Running)
        {
            const quint32 run = m_st->runningCue(list->id);
            text = tr("The running cue — %1, running in %2 (%n layer(s))", "", layers)
                   .arg(m_st->cueLabel(*list, list->indexOf(run)), list->name);
        }
        else
            text = tr("Everything lit — every cue list running, the Design preview and anything else "
                      "on (%n layer(s))", "", layers);
        QRadioButton *rb = new QRadioButton(text, &dlg);
        rb->setObjectName(QStringLiteral("stRecord:%1").arg(ShowTreeController::recordSourceToString(src)));
        v->addWidget(rb);
        radios << qMakePair(rb, src);
    }
    // Checked first: what the mode usually means -- Design, the cue in
    // edit; Perform, the running cue.
    RecordSource pre = perform ? RecordSource::Running : RecordSource::Selected;
    if (!choices.contains(pre))
        pre = choices.first();
    for (const auto &r : radios)
        r.first->setChecked(r.second == pre);
    QCheckBox *remember = new QCheckBox(tr("Don't ask again in %1 mode (Settings → Show Tree changes it)")
                                        .arg(perform ? tr("Perform (Operate)") : tr("Design")), &dlg);
    remember->setObjectName("stRecordDontAsk");
    v->addWidget(remember);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Record"));
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    v->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted)
        return false;
    for (const auto &r : radios)
        if (r.first->isChecked())
            *chosen = r.second;
    *dontAsk = remember->isChecked();
    return true;
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
    // A new list is Tracking, as `list "name"` makes it; only an import
    // starts Non-tracking (D12).
    const quint32 id = m_st->addCueList(item->data(0, IdRole).toUInt(), name.trimmed(), ListMode::Tracking);
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
    // ⌥-click: a layer each, as an ⌥-drop.
    addLayersFromLibrary(QApplication::keyboardModifiers() & Qt::AltModifier);
}

bool ShowTreeView::addLayersFromLibrary(bool separate)
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId())
        return false;
    QList<quint32> groups, fixtures, sets;
    for (QTreeWidgetItem *item : m_library->selectedItems())
    {
        // The same group may be selected in two sections (Recent and All).
        const int kind = item->data(0, KindRole).toInt();
        const quint32 id = item->data(0, IdRole).toUInt();
        if (kind == LibGroup && !groups.contains(id))
            groups << id;
        else if (kind == LibFixture && !fixtures.contains(id))
            fixtures << id;
        else if (kind == LibSet && !sets.contains(id))
            sets << id;
    }
    if (groups.isEmpty() && fixtures.isEmpty() && sets.isEmpty())
    {
        QMessageBox::information(this, tr("Add layer"),
                                 tr("Select one or more groups in the Library first (or drag them onto the cue)."));
        return false;
    }
    return dropTargets(cueId, groups, fixtures, separate, sets);
}

quint32 ShowTreeView::makeSetFromLibrary(const QString &name)
{
    QList<quint32> groups, fixtures;
    for (QTreeWidgetItem *item : m_library->selectedItems())
    {
        const int kind = item->data(0, KindRole).toInt();
        const quint32 id = item->data(0, IdRole).toUInt();
        if (kind == LibGroup && !groups.contains(id))
            groups << id;
        else if (kind == LibFixture && !fixtures.contains(id))
            fixtures << id;
        else if (kind == LibSet)
            if (const GroupSet *gs = m_st->groupSet(id))
            {
                for (quint32 g : gs->groups)
                    if (!groups.contains(g))
                        groups << g;
                for (quint32 f : gs->fixtures)
                    if (!fixtures.contains(f))
                        fixtures << f;
            }
    }
    if (groups.isEmpty() && fixtures.isEmpty())
        return invalidId();
    const quint32 id = m_st->addGroupSet(name, groups, fixtures);
    if (id != invalidId())
        noteRecent(LibSet, id);
    return id;
}

const char *ShowTreeView::targetMimeType()
{
    return "application/x-qlcconsole-showtree-stagetargets";
}

const char *ShowTreeView::setMimeType()
{
    return "application/x-qlcconsole-showtree-groupsets";
}

const char *ShowTreeView::reusableMimeType()
{
    return "application/x-qlcconsole-showtree-reusables";
}

bool ShowTreeView::splitLayer(quint32 cueId, quint32 layerId, QString *why)
{
    QList<quint32> made;
    if (!m_st->splitLayer(cueId, layerId, &made, why))
        return false;
    if (!made.isEmpty())
        selectNode(made.first(), cueId);
    return true;
}

quint32 ShowTreeView::mergeLayers(quint32 cueId, const QList<quint32> &layerIds, QString *why)
{
    const quint32 id = m_st->mergeLayers(cueId, layerIds, why);
    if (id != invalidId())
        selectNode(id, cueId);
    return id;
}

QList<quint32> ShowTreeView::selectedLayers(quint32 *cueId) const
{
    QList<quint32> out;
    quint32 cue = invalidId();
    for (QTreeWidgetItem *item : m_tree->selectedItems())
    {
        if (item->data(0, KindRole).toInt() != LayerNode)
            return QList<quint32>();
        const quint32 c = item->data(0, CueRole).toUInt();
        if (cue != invalidId() && c != cue)
            return QList<quint32>();
        cue = c;
        out << item->data(0, IdRole).toUInt();
    }
    if (cueId != nullptr)
        *cueId = cue;
    return out;
}

void ShowTreeView::slotMergeSelected()
{
    quint32 cueId = invalidId();
    const QList<quint32> sel = selectedLayers(&cueId);
    if (sel.count() < 2)
    {
        QMessageBox::information(this, tr("Merge layers"),
                                 tr("Select two or more layers of one cue in the tree (⌘- or ⇧-click) "
                                    "to merge them."));
        return;
    }
    QString why;
    if (mergeLayers(cueId, sel, &why) == invalidId())
        QMessageBox::information(this, tr("Merge layers"), why);
}

bool ShowTreeView::deleteSelected()
{
    // Several rows selected: the cues and own layers among them go in one
    // undo step (a layer of a cue going too goes with it). A cue list or a
    // production is deleted only on its own, as it asks first.
    QList<quint32> cues;
    QList<QPair<quint32, quint32>> layers;
    for (QTreeWidgetItem *item : m_tree->selectedItems())
    {
        const int kind = item->data(0, KindRole).toInt();
        if (kind == CueNode)
            cues << item->data(0, IdRole).toUInt();
        else if (kind == LayerNode)
            layers << qMakePair(item->data(0, CueRole).toUInt(), item->data(0, IdRole).toUInt());
        else if (kind == ProductionNode || kind == ListNode)
            return false;
    }
    if (cues.count() + layers.count() < 2)
        return false;
    m_st->beginBatch(tr("Delete %1 items").arg(cues.count() + layers.count()));
    bool ok = true;
    for (const QPair<quint32, quint32> &l : layers)
        if (!cues.contains(l.first))
            ok &= m_st->removeLayer(l.first, l.second);
    for (quint32 c : cues)
        ok &= m_st->removeCue(c);
    m_st->endBatch(ok);
    return ok;
}

void ShowTreeView::slotDelete()
{
    if (m_tree->selectedItems().count() > 1)
    {
        bool lists = false;
        for (QTreeWidgetItem *item : m_tree->selectedItems())
        {
            const int kind = item->data(0, KindRole).toInt();
            lists |= kind == ProductionNode || kind == ListNode;
        }
        if (lists)
        {
            QMessageBox::information(this, tr("Delete"),
                                     tr("A cue list or a production is deleted on its own: select just "
                                        "it. Several cues and layers can go together."));
            return;
        }
        if (deleteSelected())
            return;
    }
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
                               bool separate, const QList<quint32> &sets)
{
    quint32 last = invalidId();
    if (separate)
    {
        // ⌥: one layer per group -- a set's members each one (§1) -- and
        // one on the fixtures, one undo step.
        QList<quint32> each = groups;
        QList<quint32> loose = fixtures;
        for (quint32 set : sets)
            if (const GroupSet *gs = m_st->groupSet(set))
            {
                for (quint32 g : gs->groups)
                    if (!each.contains(g))
                        each << g;
                for (quint32 f : gs->fixtures)
                    if (!loose.contains(f))
                        loose << f;
            }
        m_st->beginBatch(tr("Add %1 layers").arg(each.count() + (loose.isEmpty() ? 0 : 1)));
        for (quint32 g : each)
        {
            Target t;
            t.groups << g;
            last = m_st->addLayer(cueId, t);
        }
        if (!loose.isEmpty())
        {
            Target t;
            t.fixtures = loose;
            last = m_st->addLayer(cueId, t);
        }
        m_st->endBatch(last != invalidId());
    }
    else if (!groups.isEmpty() || !fixtures.isEmpty() || !sets.isEmpty())
    {
        // One layer on everything dropped together.
        Target t;
        t.groups = groups;
        t.fixtures = fixtures;
        t.sets = sets;
        last = m_st->addLayer(cueId, t);
    }
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    for (quint32 set : sets)
        noteRecent(LibSet, set);
    if (last != invalidId())
        selectNode(last, cueId);
    return last != invalidId();
}

bool ShowTreeView::dropTargetsOnLayer(quint32 cueId, quint32 layerId, const QList<quint32> &groups,
                                      const QList<quint32> &fixtures, const QList<quint32> &sets)
{
    if (!m_st->addToLayer(cueId, layerId, groups, fixtures, sets))
        return false;
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    for (quint32 set : sets)
        noteRecent(LibSet, set);
    selectNode(layerId, cueId);
    return true;
}

bool ShowTreeView::dropChildLayers(quint32 cueId, quint32 parentId, const QList<quint32> &groups,
                                   const QList<quint32> &fixtures, const QList<quint32> &sets,
                                   bool separate, QString *why)
{
    QList<Target> children;
    if (separate)
    {
        for (quint32 g : groups)
        {
            Target t;
            t.groups << g;
            children << t;
        }
        for (quint32 set : sets)
        {
            Target t;
            t.sets << set;
            children << t;
        }
        if (!fixtures.isEmpty())
        {
            Target t;
            t.fixtures = fixtures;
            children << t;
        }
    }
    else
    {
        Target t;
        t.groups = groups;
        t.fixtures = fixtures;
        t.sets = sets;
        if (!t.isEmpty())
            children << t;
    }
    if (children.isEmpty())
        return false;
    // One undo step, all or none.
    m_st->beginBatch(children.count() == 1 ? tr("Add child layer %1").arg(m_st->targetLabel(children.first()))
                                           : tr("Add %1 child layers").arg(children.count()));
    quint32 last = invalidId();
    QString reason;
    for (const Target &t : children)
    {
        last = m_st->addChildLayer(cueId, parentId, t, &reason);
        if (last == invalidId())
            break;
    }
    m_st->endBatch(last != invalidId());
    if (why != nullptr)
        *why = reason;
    if (last == invalidId())
        return false;
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    selectNode(last, cueId);
    return true;
}

bool ShowTreeView::dropFunctions(quint32 cueId, const QList<quint32> &functions)
{
    QList<quint32> funcs;
    for (quint32 f : functions)
        if (!m_st->isGenerated(f) && m_doc->function(f) != nullptr && !funcs.contains(f))
            funcs << f;
    if (funcs.isEmpty())
        return false;
    // A layer each, in one undo step (D22).
    m_st->beginBatch(funcs.count() == 1 ? tr("Add %1").arg(m_doc->function(funcs.first())->name())
                                        : tr("Add %1 functions").arg(funcs.count()));
    quint32 last = invalidId();
    bool ok = true;
    for (quint32 f : funcs)
    {
        Target t;
        t.function = f;
        const quint32 id = m_st->addLayer(cueId, t);
        ok &= id != invalidId();
        last = id;
        noteRecent(LibFunction, f);
    }
    m_st->endBatch(ok);
    return ok && last != invalidId();
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
    // Several kinds at once (a colour and a dimmer): one undo step (D22).
    if (looks.count() > 1)
    {
        QStringList names;
        for (quint32 pid : palettes)
            names << paletteName(pid);
        m_st->beginBatch(tr("Set %1").arg(names.join(QStringLiteral(" + "))));
    }
    bool done = false;
    for (const Look &l : looks)
        done |= applyLook(cueId, layerId, target, l);
    if (looks.count() > 1)
        m_st->endBatch(done);
    return done;
}

bool ShowTreeView::acceptsDrop(quint32 cueId, quint32 layerId, const QMimeData *mime, const Target &target,
                               bool child) const
{
    if (cueId == invalidId() || mime == nullptr)
        return false;
    // Reusables: a use in the cue (a layer), or after it (a cue).
    if (mime->hasFormat(QLatin1String(reusableMimeType())))
        return !child;
    const bool targets = mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureGroupMimeType()))
                         || mime->hasFormat(QLatin1String(FixtureGroupSource::fixtureMimeType()))
                         || mime->hasFormat(QLatin1String(setMimeType()));
    const bool palettes = mime->hasFormat(QLatin1String(FunctionsTreeWidget::paletteDragMimeType()))
                          || mime->hasFormat(QLatin1String(targetMimeType()));
    if (child)
    {
        // ⇧: a child layer, under a layer of the cue's own.
        const Cue *cue = m_st->cue(cueId);
        const Layer *l = cue == nullptr || layerId == invalidId() ? nullptr : cue->layer(layerId);
        return l != nullptr && !l->target.isOpaque() && targets;
    }
    if (layerId == invalidId() && !target.isEmpty())
    {
        // A layer carried in: looks and groups make it the cue's own.
        return !target.isOpaque() && (palettes || targets);
    }
    if (palettes)
        return true;
    if (layerId != invalidId())
    {
        // Groups and fixtures join a layer's target; functions go on a cue.
        const Cue *cue = m_st->cue(cueId);
        const Layer *l = cue == nullptr ? nullptr : cue->layer(layerId);
        return l != nullptr && !l->target.isOpaque() && targets;
    }
    return targets || mime->hasFormat(QLatin1String(FunctionsTreeWidget::functionDragMimeType()));
}

ShowTree::Target ShowTreeView::carriedTarget(quint32 cueId, const QString &signature) const
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr)
        return Target();
    const int index = list->indexOf(cueId);
    const QList<ResolvedCue> resolved = m_st->resolved(*list);
    if (index < 0 || index >= resolved.count())
        return Target();
    for (const ResolvedLayer &rl : resolved.at(index).layers)
        if (!rl.own && rl.target.signature() == signature)
            return rl.target;
    return Target();
}

bool ShowTreeView::selectTracked(quint32 cueId, const QString &signature)
{
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == TrackedLayerNode
            && (*it)->data(0, CueRole).toUInt() == cueId
            && (*it)->data(0, TargetRole).toString() == signature)
        {
            for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
                p->setExpanded(true);
            m_tree->setCurrentItem(*it, 0, kSelectOnly);
            return true;
        }
    return false;
}

QStringList ShowTreeView::cueLayerItems() const
{
    QStringList out;
    for (int i = 0; i < m_cueLayers->count(); ++i)
        out << m_cueLayers->item(i)->text();
    return out;
}

bool ShowTreeView::dropMime(quint32 cueId, quint32 layerId, const QMimeData *mime, bool separate,
                            const Target &target, bool stayOnCue, bool child)
{
    if (!acceptsDrop(cueId, layerId, mime, target, child))
        return false;
    if (mime->hasFormat(QLatin1String(reusableMimeType())))
        return dropReusables(cueId, idsFromMime(mime, reusableMimeType()));
    QList<quint32> pals = idsFromMime(mime, FunctionsTreeWidget::paletteDragMimeType());
    // A target dropped: the Aim palette pointing at it (reused, or made).
    for (quint32 tid : idsFromMime(mime, targetMimeType()))
    {
        const quint32 pid = m_st->aimPaletteFor(tid);
        if (pid != QLCPalette::invalidId() && !pals.contains(pid))
            pals << pid;
    }
    const QList<quint32> groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
    const QList<quint32> fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
    const QList<quint32> sets = idsFromMime(mime, setMimeType());
    const QList<quint32> funcs = idsFromMime(mime, FunctionsTreeWidget::functionDragMimeType());
    const bool targets = !groups.isEmpty() || !fixtures.isEmpty() || !sets.isEmpty();
    bool done = false;
    if (child)
    {
        QString why;
        done = targets && dropChildLayers(cueId, layerId, groups, fixtures, sets, separate, &why);
        if (!done && !why.isEmpty())
            m_hint->setText(why);
    }
    else if (!pals.isEmpty())
        done = dropPalettes(cueId, layerId, pals, layerId == invalidId() ? target : Target());
    else if (targets && layerId != invalidId())
        done = dropTargetsOnLayer(cueId, layerId, groups, fixtures, sets);
    else if (targets && !target.isEmpty())
        done = dropTargetsOnTracked(cueId, target, groups, fixtures, sets);
    else if (targets)
        done = dropTargets(cueId, groups, fixtures, separate, sets);
    else if (!funcs.isEmpty() && layerId == invalidId() && target.isEmpty())
        done = dropFunctions(cueId, funcs);
    // From the cue page the cue stays up, ready for the next drop.
    if (done && stayOnCue && !(currentKind() == CueNode && currentId() == cueId))
        selectNode(cueId);
    return done;
}

bool ShowTreeView::dropTargetsOnTracked(quint32 cueId, const Target &target, const QList<quint32> &groups,
                                        const QList<quint32> &fixtures, const QList<quint32> &sets)
{
    const Cue *cue = m_st->cue(cueId);
    if (cue == nullptr || target.isEmpty() || target.isOpaque())
        return false;
    // The cue's own already (a row a moment old): add to that.
    for (const Layer &l : cue->layers)
        if (l.target == target)
            return dropTargetsOnLayer(cueId, l.id, groups, fixtures, sets);
    bool adds = false;
    for (quint32 g : groups)
        adds |= !target.groups.contains(g);
    for (quint32 f : fixtures)
        adds |= !target.fixtures.contains(f);
    for (quint32 set : sets)
        adds |= !target.sets.contains(set);
    if (!adds)
        return false;

    // Brought into the cue (it takes the carried one's place and values),
    // then the groups added -- one undo step.
    Target added;
    added.groups = groups;
    added.fixtures = fixtures;
    added.sets = sets;
    m_st->beginBatch(tr("Add %1 to layer %2").arg(m_st->targetLabel(added), m_st->targetLabel(target)));
    const quint32 lid = m_st->addLayer(cueId, target);
    const bool ok = lid != invalidId() && m_st->addToLayer(cueId, lid, groups, fixtures, sets);
    m_st->endBatch(ok);
    if (!ok)
        return false;
    for (quint32 g : groups)
        noteRecent(LibGroup, g);
    selectNode(lid, cueId);
    return true;
}

QString ShowTreeView::dropHint(quint32 cueId, quint32 layerId, const QMimeData *mime, bool separate,
                               const Target &target, bool child) const
{
    const CueList *list = m_st->listOfCue(cueId);
    const Cue *cue = m_st->cue(cueId);
    if (list == nullptr || cue == nullptr)
        return QString();
    const QString cueText = m_st->cueLabel(*list, list->indexOf(cueId));
    const QList<quint32> reusables = idsFromMime(mime, reusableMimeType());
    if (!reusables.isEmpty())
    {
        QStringList layers, cues;
        for (quint32 r : reusables)
        {
            if (m_st->reusableLayer(r) != nullptr)
                layers << m_st->reusableName(r);
            else if (m_st->reusableCue(r) != nullptr)
                cues << m_st->reusableName(r);
        }
        QStringList out;
        if (!layers.isEmpty())
            out << tr("⟲ %1 → a layer in %2, linked: an edit to it is an edit to every use")
                   .arg(layers.join(QStringLiteral(", ")), cueText);
        if (!cues.isEmpty())
            out << tr("⟲ %1 → a new cue after %2, linked").arg(cues.join(QStringLiteral(", ")), cueText);
        return out.join(QStringLiteral("; "));
    }
    const QList<quint32> pals = idsFromMime(mime, FunctionsTreeWidget::paletteDragMimeType());
    const QList<quint32> stageTargets = idsFromMime(mime, targetMimeType());
    if (!stageTargets.isEmpty() && !child)
    {
        const MonitorProperties *mp = m_doc->monitorProperties();
        QStringList names;
        bool line = false;
        for (quint32 tid : stageTargets)
            if (const StageTarget *t = mp == nullptr ? nullptr : mp->stageTarget(tid))
            {
                names << t->name();
                line |= t->isLine();
            }
        const QString what = tr("Aim at %1").arg(names.join(QStringLiteral(", ")));
        QString where;
        if (layerId != invalidId())
        {
            const Layer *l = cue->layer(layerId);
            where = tr("layer %1 in %2").arg(l == nullptr ? QString()
                                                : (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name), cueText);
        }
        else if (!target.isEmpty())
            where = tr("layer %1 in %2 (sets it in this cue)").arg(m_st->targetLabel(target), cueText);
        else
            where = line ? tr("cue look on %1: one formation over its layers").arg(cueText)
                         : tr("cue look on %1").arg(cueText);
        return tr("%1 → %2").arg(what, where);
    }
    if (child)
    {
        // ⇧: a child layer -- said only where it can go.
        const Layer *l = layerId == invalidId() ? nullptr : cue->layer(layerId);
        if (l == nullptr || l->target.isOpaque())
            return tr("⇧: a child layer goes under a layer of this cue's own");
        Target t;
        t.groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
        t.fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
        t.sets = idsFromMime(mime, setMimeType());
        const QString parent = l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name;
        if (!m_st->targetContains(l->target, t))
            return tr("%1 isn't part of %2: a child layer is on part of its parent").arg(m_st->targetLabel(t), parent);
        return separate ? tr("a child layer each under %1 in %2").arg(parent, cueText)
                        : tr("a child layer on %1 under %2 in %3: it sets only what it sets").arg(
                              m_st->targetLabel(t), parent, cueText);
    }
    if (layerId == invalidId() && !target.isEmpty())
    {
        // Onto a layer carried in: it becomes this cue's own.
        if (!acceptsDrop(cueId, layerId, mime, target))
            return QString();
        const QString layerName = m_st->targetLabel(target);
        if (!pals.isEmpty())
        {
            QStringList names;
            for (quint32 p : pals)
                names << paletteName(p);
            return tr("%1 → layer %2 in %3 (sets it in this cue)")
                   .arg(names.join(QStringLiteral(" + ")), layerName, cueText);
        }
        Target t;
        t.groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
        t.fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
        t.sets = idsFromMime(mime, setMimeType());
        const QString what = t.groups.isEmpty() && t.sets.isEmpty() ? tr("these fixtures") : m_st->targetLabel(t);
        return tr("add %1 to layer %2 in %3 (brings it into this cue)").arg(what, layerName, cueText);
    }
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
    const QList<quint32> sets = idsFromMime(mime, setMimeType());
    if (!groups.isEmpty() || !fixtures.isEmpty() || !sets.isEmpty())
    {
        Target t;
        t.groups = groups;
        t.fixtures = fixtures;
        t.sets = sets;
        const QString what = groups.isEmpty() && sets.isEmpty() ? tr("these fixtures") : m_st->targetLabel(t);
        if (layerId != invalidId())
        {
            const Layer *l = cue->layer(layerId);
            // Part of it already: ⇧ would make it a child layer instead.
            const bool inside = l != nullptr && m_st->targetContains(l->target, t);
            return (inside ? tr("add %1 to layer %2 in %3 (⇧: a child layer on it)")
                           : tr("add %1 to layer %2 in %3"))
                   .arg(what, l == nullptr ? QString()
                              : (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name), cueText);
        }
        // ⌥ makes a layer of each group, a set's members each one.
        int pieces = int(groups.count()) + (fixtures.isEmpty() ? 0 : 1);
        for (quint32 set : sets)
            if (const GroupSet *gs = m_st->groupSet(set))
                pieces += int(gs->groups.count());
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
    // An Aim look re-pointed or given other line settings: the inline Aim
    // palettes made from it for a layer's own Spread / Grouping / Cross
    // follow at the next bake (no tree change, so no undo step).
    if (full)
        if (const QLCPalette *p = m_doc->palette(paletteId))
            if (p->type() == QLCPalette::Aim && !m_st->isInlinePalette(paletteId))
                m_st->bake();
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
    const quint32 pid = valuePalette(cueId, layerId, Target(), QLCPalette::Dimmer,
                                      { qBound(0, qRound(percent * 255.0 / 100.0), 255) },
                                      tr("%1%").arg(percent));
    return pid != QLCPalette::invalidId() && applyLook(cueId, layerId, Target(), m_st->lookForPalette(pid));
}

bool ShowTreeView::setOneOffColour(quint32 cueId, quint32 layerId, const QColor &colour)
{
    const quint32 pid = valuePalette(cueId, layerId, Target(), QLCPalette::Color,
                                      { QLCPalette::colorToString(colour, QColor(Qt::black)) },
                                      tr("%1-ish").arg(colour.name()));
    return pid != QLCPalette::invalidId() && applyLook(cueId, layerId, Target(), m_st->lookForPalette(pid));
}

bool ShowTreeView::setEffectOwn(quint32 cueId, quint32 layerId, const QString &key,
                                const QList<quint32> &palettes, const Target &target)
{
    if (isMulti(cueId, layerId))
        return forEachEdited(cueId, layerId, tr("Set the effect's own looks"), [&](quint32 c, quint32 l) {
            return setEffectOwn(c, l, key, palettes);
        });
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
        const QList<ResolvedCue> r = m_st->resolved(*list);
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
    return applyLook(cueId, layerId, Target(), l);
}

bool ShowTreeView::setAttributeNone(quint32 cueId, quint32 layerId, const QString &key)
{
    Look l;
    l.key = key;
    l.none = true;
    return applyLook(cueId, layerId, Target(), l);
}

QList<QPair<quint32, quint32>> ShowTreeView::selectedLayerRefs() const
{
    QList<QPair<quint32, quint32>> out;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->isSelected() && (*it)->data(0, KindRole).toInt() == LayerNode)
            out << qMakePair((*it)->data(0, CueRole).toUInt(), (*it)->data(0, IdRole).toUInt());
    return out;
}

bool ShowTreeView::setReleaseOnSelected(bool release)
{
    const QList<QPair<quint32, quint32>> refs = selectedLayerRefs();
    if (refs.isEmpty())
        return false;
    m_st->beginBatch(release ? tr("Release %1 layers").arg(refs.count()) : tr("Carry %1 layers").arg(refs.count()));
    bool any = false;
    for (const QPair<quint32, quint32> &ref : refs)
        any |= m_st->setLayerRelease(ref.first, ref.second, release);
    m_st->endBatch(any);
    return any;
}

void ShowTreeView::showMultiLayerPage(const QList<QPair<quint32, quint32>> &layers)
{
    m_multi = layers;
    m_layerReuse->setVisible(false);
    DropPage *page = static_cast<DropPage *>(m_layerPage);
    const quint32 curCue = currentCueId(), curLayer = currentId();
    page->cue = curCue;                 // drops go to all of them (applyLook)
    page->layer = curLayer;
    page->target = Target();
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

    // Each layer as it resolves in its cue.
    QHash<quint32, QList<ResolvedCue>> byList;
    QList<const ResolvedLayer *> rls;
    QStringList names;
    QSet<quint32> cues, fixtures;
    int released = 0;
    for (const QPair<quint32, quint32> &ref : layers)
    {
        const CueList *list = m_st->listOfCue(ref.first);
        const Cue *c = m_st->cue(ref.first);
        const Layer *l = c == nullptr ? nullptr : c->layer(ref.second);
        if (list == nullptr || l == nullptr)
            continue;
        if (!byList.contains(list->id))
            byList.insert(list->id, m_st->resolved(*list));
        const ResolvedLayer *rl = byList[list->id].at(list->indexOf(ref.first)).layer(l->target);
        if (rl == nullptr)
            continue;
        rls << rl;
        cues.insert(ref.first);
        names << (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name);
        for (quint32 f : m_st->targetFixtures(l->target))
            fixtures.insert(f);
        if (l->release)
            ++released;
    }
    QString where;
    if (cues.count() == 1)
    {
        const CueList *list = m_st->listOfCue(*cues.begin());
        where = tr("in cue %1").arg(m_st->cueLabel(*list, list->indexOf(*cues.begin())));
    }
    else
        where = tr("in %1 cues").arg(cues.count());
    m_layerTitle->setText(tr("%1 layers %2 — edited together").arg(rls.count()).arg(where));
    m_layerContents->setText(tr("%1 — %2 fixture(s)").arg(names.join(QStringLiteral(" · ")).toHtmlEscaped())
                             .arg(fixtures.count()));
    m_layerContents->setToolTip(QString());
    m_layerContents->setVisible(true);
    // Release: all, none, or some (partly ticked); a click sets it on all.
    m_layerRelease->setVisible(true);
    {
        QSignalBlocker block(m_layerRelease);
        const bool mixed = released > 0 && released < rls.count();
        m_layerRelease->setTristate(mixed);
        m_layerRelease->setCheckState(mixed ? Qt::PartiallyChecked : released > 0 ? Qt::Checked : Qt::Unchecked);
    }

    QStringList keys = attributeKeys();
    for (const ResolvedLayer *rl : rls)
        for (const QString &k : rl->values.keys())
            if (!keys.contains(k))
                keys << k;
    std::sort(keys.begin(), keys.end(), keyLessThan);
    const QBrush grey = palette().brush(QPalette::Disabled, QPalette::Text);
    int row = 0;
    for (const QString &key : keys)
    {
        QLabel *label = new QLabel(keyLabel(key), m_rowsHost);
        if (key.startsWith(QLatin1String("Beam.")) || isLineParamKey(key))
            label->setText(QStringLiteral("   ") + label->text());
        QLabel *value = new QLabel(m_rowsHost);
        value->setObjectName(QStringLiteral("stRow:%1").arg(key));
        // The same on all (and from where), mixed, or none.
        int have = 0;
        bool same = true, allOwn = true;
        Look first;
        QSet<QString> origins;
        for (const ResolvedLayer *rl : rls)
        {
            if (!rl->values.contains(key))
                continue;
            const ResolvedValue &v = rl->values.value(key);
            if (have == 0)
                first = v.look;
            else if (v.look != first)
                same = false;
            ++have;
            allOwn &= v.origin == Origin::Own;
            const CueList *list = m_st->listOfCue(v.fromCue);
            origins.insert(list == nullptr ? QString() : originText(*list, v));
        }
        QString text, origin;
        if (have == 0)
            text = QStringLiteral("—");
        else if (!same || have < rls.count())
            text = tr("mixed");
        else
        {
            text = isLineParamKey(key) && !first.none ? ShowTreeController::lineParamText(key, first.value)
                                                      : lookText(first);
            origin = origins.count() == 1 ? *origins.begin() : tr("from several places");
        }
        if (have > 0 && (!allOwn || text == tr("mixed")))
        {
            QPalette pal = value->palette();
            pal.setBrush(QPalette::WindowText, grey);
            value->setPalette(pal);
            QFont f = value->font();
            f.setItalic(true);
            value->setFont(f);
        }
        value->setText(origin.isEmpty() ? text : QStringLiteral("%1 — %2").arg(text, origin));
        value->setWordWrap(true);
        // Applies to: summed over them.
        QLabel *reach = new QLabel(m_rowsHost);
        if (!isEffectKey(key) && !isLineParamKey(key))
        {
            int n = 0, total = 0;
            for (const ResolvedLayer *rl : rls)
            {
                Look probe;
                probe.key = key;
                if (rl->values.contains(key))
                    probe = rl->values.value(key).look;
                int a = 0, b = 0;
                m_st->lookReach(rl->target, probe, &a, &b);
                n += a;
                total += b;
            }
            if (total > 0 && n < total)
                reach->setText(tr("applies to %1 of %2").arg(n).arg(total));
        }
        QToolButton *more = new QToolButton(m_rowsHost);
        more->setText(QStringLiteral("⋯"));
        more->setToolTip(tr("Set, clear or turn off %1 on all %2").arg(label->text().trimmed()).arg(rls.count()));
        connect(more, &QToolButton::clicked, this, [this, curCue, curLayer, key, more]() {
            const QPoint at = more->mapToGlobal(QPoint(0, more->height()));
            if (isLineParamKey(key))
            {
                QMenu menu(this);
                lineParamMenu(&menu, curCue, curLayer, Target(), key);
                menu.exec(at);
                return;
            }
            attributeMenu(curCue, curLayer, Target(), Target(), key, at);
        });
        m_rows->addWidget(label, row, 0);
        m_rows->addWidget(value, row, 1);
        m_rows->addWidget(reach, row, 2);
        m_rows->addWidget(more, row, 4);
        m_rowText << QStringLiteral("%1 | %2 | %3").arg(label->text().trimmed(), text, origin);
        ++row;
    }
    m_rows->setRowStretch(row, 1);
    m_pages->setCurrentWidget(m_layerPage);
}

void ShowTreeView::slotTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    QMenu menu(this);
    menu.addAction(tr("+ Production…"), this, &ShowTreeView::slotNewProduction);
    if (item != nullptr)
    {
        // A right-click in a selection keeps it (Merge works on it).
        if (item->isSelected())
            m_tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
        else
            m_tree->setCurrentItem(item, 0, kSelectOnly);
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
                    recordFrom(cueId);
                });
                menu.addAction(tr("Duplicate cue"), this, [this, cueId]() {
                    const quint32 id = m_st->duplicateCue(cueId);
                    if (id != invalidId())
                        selectNode(id);
                });
                // P4: a reusable cue, or this use unlinked.
                if (list->cue(cueId)->reusable == invalidId())
                    menu.addAction(tr("Make cue reusable…"), this, [this]() { reuseLinkActivated(QStringLiteral("make")); })
                        ->setObjectName("stMakeCueReusable");
                else
                    menu.addAction(tr("Unlink here (%1 becomes a cue of its own)")
                                   .arg(m_st->reusableName(list->cue(cueId)->reusable)), this,
                                   [this, cueId]() { m_st->unlinkCue(cueId); })->setObjectName("stUnlinkCue");
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
        const QList<QPair<quint32, quint32>> sel = selectedLayerRefs();
        if (kind == LayerNode && sel.count() > 1 && item->isSelected())
        {
            // Several layers: the edits act on all of them, one step each.
            QMenu *several = menu.addMenu(tr("%1 selected layers").arg(sel.count()));
            several->setObjectName("stSeveralLayers");
            several->addAction(tr("Release on"), this, [this]() { setReleaseOnSelected(true); });
            several->addAction(tr("Release off"), this, [this]() { setReleaseOnSelected(false); });
            QMenu *set = several->addMenu(tr("Set look"));
            QMenu *clear = several->addMenu(tr("Clear look"));
            const QPair<quint32, quint32> cur(item->data(0, CueRole).toUInt(), item->data(0, IdRole).toUInt());
            QHash<QString, QMenu *> kinds;
            QList<QLCPalette *> pals = m_doc->palettes();
            std::sort(pals.begin(), pals.end(), [](const QLCPalette *a, const QLCPalette *b) {
                return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
            });
            const QStringList keys = attributeKeys();
            for (const QString &k : keys)
            {
                kinds.insert(k, set->addMenu(keyLabel(k)));
                clear->addAction(keyLabel(k), this, [this, cur, k]() {
                    m_multi = selectedLayerRefs();
                    clearAttribute(cur.first, cur.second, k);
                });
            }
            for (QLCPalette *p : pals)
            {
                if (m_st->isInlinePalette(p->id()))
                    continue;
                QMenu *m = kinds.value(m_st->keyForPalette(p->id()));
                if (m == nullptr)
                    continue;
                const quint32 pid = p->id();
                m->addAction(p->name(), this, [this, cur, pid]() {
                    m_multi = selectedLayerRefs();
                    dropPalettes(cur.first, cur.second, { pid });
                });
            }
            several->addAction(tr("Split each (one layer per group)"), this, [this, sel]() {
                m_st->beginBatch(tr("Split %1 layers").arg(sel.count()));
                bool any = false;
                for (const QPair<quint32, quint32> &ref : sel)
                    any |= m_st->splitLayer(ref.first, ref.second);
                m_st->endBatch(any);
            });
            menu.addSeparator();
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
            // P4: a reusable layer, or this use unlinked.
            if (const Cue *rc = m_st->cue(cueId))
                if (const Layer *rl = rc->layer(layerId))
                {
                    if (rl->reusable != invalidId())
                        menu.addAction(tr("Unlink here (a copy of its own)"), this, [this, cueId, layerId]() {
                            m_st->unlinkLayer(cueId, layerId);
                        })->setObjectName("stUnlinkLayer");
                    else if (rl->parent == invalidId() && rc->reusable == invalidId())
                        menu.addAction(tr("Make reusable…"), this, [this]() {
                            reuseLinkActivated(QStringLiteral("make"));
                        })->setObjectName("stMakeLayerReusable");
                }
            // A child layer (§3.5) on part of it: a member of its set, a
            // group inside it, or what the Library has selected.
            {
                const Cue *c = m_st->cue(cueId);
                const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
                if (l != nullptr && !l->target.isOpaque())
                {
                    QMenu *childMenu = menu.addMenu(tr("Add child layer"));
                    childMenu->setObjectName("stAddChild");
                    QList<FixtureGroup *> inside;
                    for (FixtureGroup *g : m_doc->fixtureGroups())
                    {
                        Target t;
                        t.groups << g->id();
                        if (!(t == l->target) && m_st->targetContains(l->target, t))
                            inside << g;
                    }
                    std::sort(inside.begin(), inside.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
                        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
                    });
                    for (FixtureGroup *g : inside)
                    {
                        const quint32 gid = g->id();
                        childMenu->addAction(g->name(), this, [this, cueId, layerId, gid]() {
                            QString why;
                            if (!dropChildLayers(cueId, layerId, { gid }, {}, {}, false, &why))
                                QMessageBox::information(this, tr("Add child layer"), why);
                        });
                    }
                    childMenu->addSeparator();
                    childMenu->addAction(tr("The Library's selection"), this, [this, cueId, layerId]() {
                        QList<quint32> groups, fixtures, sets;
                        for (QTreeWidgetItem *it : m_library->selectedItems())
                        {
                            const int k = it->data(0, KindRole).toInt();
                            const quint32 id = it->data(0, IdRole).toUInt();
                            if (k == LibGroup && !groups.contains(id))
                                groups << id;
                            else if (k == LibFixture && !fixtures.contains(id))
                                fixtures << id;
                            else if (k == LibSet && !sets.contains(id))
                                sets << id;
                        }
                        QString why = tr("Select groups in the Library first.");
                        if (!dropChildLayers(cueId, layerId, groups, fixtures, sets, false, &why))
                            QMessageBox::information(this, tr("Add child layer"), why);
                    });
                    childMenu->setEnabled(true);
                }
            }
            // Split (one per group) and Merge (the layers selected).
            {
                const Cue *c = m_st->cue(cueId);
                const Layer *l = c == nullptr ? nullptr : c->layer(layerId);
                QAction *split = menu.addAction(tr("Split layer (one per group)"), this, [this, cueId, layerId]() {
                    QString why;
                    if (!splitLayer(cueId, layerId, &why))
                        QMessageBox::information(this, tr("Split layer"), why);
                });
                split->setObjectName("stSplitLayer");
                split->setEnabled(l != nullptr && l->pieces().count() > 1);
                split->setToolTip(tr("One layer per group (and one on its fixtures), each with these looks; "
                                     "it keeps tracking as before"));
                quint32 selCue = invalidId();
                const QList<quint32> sel = selectedLayers(&selCue);
                QAction *merge = menu.addAction(sel.count() > 1 ? tr("Merge %1 layers").arg(sel.count())
                                                                : tr("Merge layers (select several)"),
                                                this, [this, selCue, sel]() {
                    QString why;
                    if (mergeLayers(selCue, sel, &why) == invalidId())
                        QMessageBox::information(this, tr("Merge layers"), why);
                });
                merge->setObjectName("stMergeLayers");
                merge->setEnabled(sel.count() > 1);
            }
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
        if (kind == ListNode)
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            QAction *conv = menu.addAction(tr("Convert to tracking…"), this, [this, id]() { convertListToTracking(id); });
            conv->setObjectName("stConvertList");
            conv->setEnabled(mayConvert(m_st->cueList(id)));
            menu.addAction(tr("Detach to plain chaser…"), this, [this, id]() { detachListToChaser(id); })
                ->setObjectName("stDetachList");
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
        if (kind == CueNode || kind == LayerNode)
        {
            menu.addSeparator();
            QAction *copy = menu.addAction(tr("Copy"), this, [this]() { copySelection(); });
            copy->setShortcut(QKeySequence::Copy);
            QAction *paste = menu.addAction(m_commander->clipboard.cues.isEmpty() ? tr("Paste into this cue")
                                                                                 : tr("Paste after this cue"),
                                            this, [this]() { pasteClipboard(); });
            paste->setShortcut(QKeySequence::Paste);
            paste->setEnabled(!m_commander->clipboard.isEmpty());
            QAction *dup = menu.addAction(tr("Duplicate"), this, [this]() { duplicateSelection(); });
            dup->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
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
 * Reusable layers and cues; Detach a list; Convert to tracking (P4)
 ****************************************************************************/

bool ShowTreeView::confirm(const QString &title, const QString &text, const QString &details)
{
    if (m_confirm)
        return m_confirm(title, text, details);
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Ok | QMessageBox::Cancel, this);
    if (!details.isEmpty())
        box.setInformativeText(details);
    return box.exec() == QMessageBox::Ok;
}

bool ShowTreeView::dropReusables(quint32 cueId, const QList<quint32> &reusableIds)
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr || reusableIds.isEmpty())
        return false;
    m_st->beginBatch(tr("Use %1").arg(reusableIds.count() == 1 ? m_st->reusableName(reusableIds.first())
                                                               : tr("%1 reusables").arg(reusableIds.count())));
    quint32 lastLayer = invalidId(), lastCue = invalidId();
    int after = list->indexOf(cueId);
    const quint32 listId = list->id;
    bool any = false;
    for (quint32 r : reusableIds)
    {
        if (m_st->reusableLayer(r) != nullptr)
        {
            const quint32 id = m_st->insertReusableLayer(cueId, r);
            any |= id != invalidId();
            if (id != invalidId())
                lastLayer = id;
        }
        else if (m_st->reusableCue(r) != nullptr)
        {
            // Each after the one before: a run after the cue.
            const quint32 id = m_st->insertReusableCue(listId, after, r);
            any |= id != invalidId();
            if (id != invalidId())
            {
                lastCue = id;
                after = m_st->cueList(listId)->indexOf(id);
            }
        }
    }
    m_st->endBatch(any);
    if (lastCue != invalidId())
        selectNode(lastCue);
    else if (lastLayer != invalidId())
        selectNode(lastLayer, cueId);
    return any;
}

QStringList ShowTreeView::reusableUsedBy(quint32 id) const
{
    QStringList out;
    QSet<quint32> functions;
    for (const ShowTreeController::ReusableUse &u : m_st->reusableUses(id))
    {
        QString text = u.label;
        if (u.layer == invalidId())
        {
            if (const Cue *c = m_st->cue(u.cue))
                functions.insert(c->collection);
        }
        else if (const Cue *c = m_st->cue(u.cue))
        {
            if (const Layer *l = c->layer(u.layer))
            {
                const quint32 sid = m_st->layerScene(u.cue, l->target);
                if (sid != invalidId())
                    functions.insert(sid);
            }
        }
        out << text;
    }
    for (const ReusableCue &rc : m_st->reusableCues())
        for (const Layer &l : rc.layers)
            if (l.reusable == id)
                out << tr("reusable cue %1").arg(rc.name);
    // VC widgets that play one of the uses' cue or layer (cheap: buttons
    // and cue lists by the function they hold).
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc != nullptr && vc->contents() != nullptr && !functions.isEmpty())
    {
        for (VCButton *b : vc->contents()->findChildren<VCButton *>())
            if (functions.contains(b->function()))
                out << tr("VC button “%1”").arg(b->caption());
    }
    return out;
}

QStringList ShowTreeView::reusableItems() const
{
    QStringList out;
    for (QTreeWidgetItemIterator it(m_library); *it; ++it)
        if ((*it)->data(0, KindRole).toInt() == LibReusable)
        {
            QTreeWidgetItem *top = *it;
            while (top->parent() != nullptr)
                top = top->parent();
            if (top->data(0, IdRole).toUInt() == 2)       // All's
                out << (*it)->text(0);
        }
    return out;
}

bool ShowTreeView::deleteReusable(quint32 id)
{
    const QString name = m_st->reusableName(id);
    if (name.isEmpty())
        return false;
    QString why;
    if (m_st->removeReusable(id, false, &why))
        return true;
    const QStringList used = reusableUsedBy(id);
    if (!confirm(tr("Delete %1").arg(name),
                 tr("%1 is used in %n place(s). Delete it anyway? Each use stays as it is, as a plain copy of "
                    "its own.", "", int(used.count())).arg(name),
                 used.join(QStringLiteral("\n"))))
        return false;
    return m_st->removeReusable(id, true);
}

bool ShowTreeView::convertListToTracking(quint32 listId)
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr)
        return false;
    const ShowTreeController::ConvertPlan plan = m_st->planConvertToTracking(listId);
    if (!plan.ok)
    {
        m_hint->setText(plan.why);
        if (!m_confirm)
            QMessageBox::information(this, tr("Convert to tracking"), plan.why);
        return false;
    }
    const QString name = list->name;
    const QString text = tr("Convert %1 to tracking? Every cue plays exactly as it does now: %2 cue(s) track "
                            "from the cue before, %3 value(s) and %4 layer(s) that only repeat what tracks in go, "
                            "%5 include(s) of the cue before become tracking, %6 layer(s) are released. "
                            "One undo step.")
                         .arg(name).arg(plan.tracking).arg(plan.looksRemoved).arg(plan.layersRemoved)
                         .arg(plan.includesRemoved).arg(plan.releases);
    const QString details = plan.changes.join(QStringLiteral("\n"));
    if (m_confirm)
    {
        if (!m_confirm(tr("Convert to tracking"), text, details))
            return false;
    }
    else
    {
        // The plan to read before it is applied.
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Convert to tracking"));
        QVBoxLayout *v = new QVBoxLayout(&dlg);
        QLabel *intro = new QLabel(text, &dlg);
        intro->setWordWrap(true);
        v->addWidget(intro);
        QListWidget *changes = new QListWidget(&dlg);
        changes->setObjectName("stConvertPlan");
        changes->addItems(plan.changes);
        v->addWidget(changes, 1);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, &dlg);
        bb->addButton(tr("Convert"), QDialogButtonBox::AcceptRole);
        connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        v->addWidget(bb);
        dlg.resize(560, 420);
        if (dlg.exec() != QDialog::Accepted)
            return false;
    }
    ShowTreeController::ConvertPlan done;
    if (!m_st->convertToTracking(listId, &done))
    {
        m_hint->setText(done.why);
        return false;
    }
    m_hint->setText(tr("%1 tracks now: %2 cue(s) track, %3 value(s) and %4 layer(s) taken out")
                    .arg(name).arg(done.tracking).arg(done.looksRemoved).arg(done.layersRemoved));
    selectNode(listId);
    return true;
}

bool ShowTreeView::detachListToChaser(quint32 listId)
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr)
        return false;
    const QString name = list->name;
    if (!confirm(tr("Detach to plain chaser"),
                 tr("Detach %1? Its chaser and its cues' collections become ordinary functions you edit in the "
                    "Functions window (Productions folder), and the list leaves the Show Tree. VC widgets and "
                    "show control that play the chaser keep playing it. One undo step.").arg(name)))
        return false;
    const quint32 chaser = m_st->detachList(listId);
    if (chaser == Function::invalidId())
        return false;
    m_hint->setText(tr("%1 is a plain chaser now (Functions → Productions)").arg(name));
    return true;
}

QString ShowTreeView::reuseLine(quint32 cueId, quint32 layerId) const
{
    const Cue *c = m_st->cue(cueId);
    if (c == nullptr)
        return QString();
    const quint32 rid = layerId == invalidId() ? c->reusable
                                               : (c->layer(layerId) == nullptr ? invalidId() : c->layer(layerId)->reusable);
    if (rid == invalidId())
    {
        if (layerId != invalidId())
        {
            const Layer *l = c->layer(layerId);
            if (l == nullptr || l->parent != invalidId() || c->reusable != invalidId())
                return QString();
            return tr("<a href=\"make\">Make reusable…</a> — a named layer for several cues and lists, "
                      "every use live-linked");
        }
        return tr("<a href=\"make\">Make reusable…</a> — a named cue (a Walk-in, a Blackout) for several lists, "
                  "every use live-linked");
    }
    const int uses = m_st->reusableUses(rid).count();
    return tr("⟲ Reusable %1 <b>%2</b> — used in %3 place(s): an edit here is an edit to all. "
              "<a href=\"uses\">Used by…</a> · <a href=\"unlink\">Unlink here</a>")
           .arg(layerId == invalidId() ? tr("cue") : tr("layer"), m_st->reusableName(rid).toHtmlEscaped())
           .arg(uses);
}

QString ShowTreeView::reuseLineText() const
{
    QLabel *l = m_pages->currentWidget() == m_cuePage ? m_cueReuse : m_layerReuse;
    QTextDocument doc;
    doc.setHtml(l->text());
    return doc.toPlainText();
}

void ShowTreeView::reuseLinkActivated(const QString &link)
{
    const quint32 cueId = currentCueId();
    const quint32 layerId = currentKind() == LayerNode ? currentId() : invalidId();
    const Cue *c = m_st->cue(cueId);
    if (c == nullptr)
        return;
    const quint32 rid = layerId == invalidId() ? c->reusable
                                               : (c->layer(layerId) == nullptr ? invalidId() : c->layer(layerId)->reusable);
    if (link == QLatin1String("make"))
    {
        const Layer *l = layerId == invalidId() ? nullptr : c->layer(layerId);
        const QString suggested = l != nullptr ? (l->name.isEmpty() ? m_st->targetLabel(l->target) : l->name)
                                               : c->name;
        bool ok = false;
        const QString n = QInputDialog::getText(this, tr("Make reusable"), tr("Name"), QLineEdit::Normal,
                                                suggested, &ok);
        if (!ok)
            return;
        QString why;
        const quint32 id = layerId != invalidId() ? m_st->makeLayerReusable(cueId, layerId, n.trimmed(), &why)
                                                  : m_st->makeCueReusable(cueId, n.trimmed(), &why);
        if (id == invalidId())
            QMessageBox::information(this, tr("Make reusable"), why);
    }
    else if (link == QLatin1String("unlink"))
    {
        if (layerId != invalidId())
            m_st->unlinkLayer(cueId, layerId);
        else
            m_st->unlinkCue(cueId);
    }
    else if (link == QLatin1String("uses") && rid != invalidId())
    {
        QMenu menu(this);
        for (const ShowTreeController::ReusableUse &u : m_st->reusableUses(rid))
            menu.addAction(u.label, this, [this, u]() {
                if (u.layer != invalidId())
                    selectNode(u.layer, u.cue);
                else
                    selectNode(u.cue);
            });
        const QStringList all = reusableUsedBy(rid);
        for (const QString &x : all)
            if (x.startsWith(tr("VC button")) || x.startsWith(tr("reusable cue")))
                menu.addAction(x)->setEnabled(false);
        menu.exec(QCursor::pos());
    }
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
        else if (kind == LibTarget)
        {
            const MonitorProperties *mp = m_doc->monitorProperties();
            const StageTarget *t = mp == nullptr ? nullptr : mp->stageTarget(id);
            if (t == nullptr)
                return nullptr;
            text = t->isLine() ? tr("%1 (line)").arg(t->name()) : t->name();
            icon = QIcon(":/target.png");
        }
        else if (kind == LibSet)
        {
            const GroupSet *gs = m_st->groupSet(id);
            if (gs == nullptr)
                return nullptr;
            Target t;
            t.sets << id;
            QStringList members;
            for (quint32 g : gs->groups)
                if (const FixtureGroup *fg = m_doc->fixtureGroup(g))
                    members << fg->name();
            if (!gs->fixtures.isEmpty())
                members << tr("%n fixture(s)", "", int(gs->fixtures.count()));
            text = tr("%1 (%2)").arg(gs->name, members.join(QStringLiteral(" + ")));
            icon = QIcon(":/group.png");
        }
        else if (kind == LibReusable)
        {
            // P4: a reusable layer or cue, and how many use it.
            const bool isLayer = m_st->reusableLayer(id) != nullptr;
            if (!isLayer && m_st->reusableCue(id) == nullptr)
                return nullptr;
            text = tr("⟲ %1 — %2, used in %3").arg(m_st->reusableName(id), isLayer ? tr("layer") : tr("cue"))
                   .arg(m_st->reusableUses(id).count());
            icon = QIcon(isLayer ? ":/group.png" : ":/collection.png");
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
        for (auto it = u.sets.constBegin(); it != u.sets.constEnd(); ++it)
            entries << Entry { LibSet, it.key(), it.value() };
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

    // Targets (§3B): points, areas and lines from the Lighting Studio;
    // dropped, a Position that aims at them.
    {
        const MonitorProperties *mp = m_doc->monitorProperties();
        QList<StageTarget *> tl = mp == nullptr ? QList<StageTarget *>() : mp->stageTargets();
        std::sort(tl.begin(), tl.end(), [](const StageTarget *a, const StageTarget *b) {
            return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
        });
        if (!tl.isEmpty())
        {
            QTreeWidgetItem *targetsFolder = folder(all, tr("Targets"));
            targetsFolder->setData(0, PathRole, QStringLiteral("Targets"));
            targetsFolder->setToolTip(0, tr("Drop on a layer or a cue: Position aims at it. A line "
                                            "spreads the lights along it (Spread, Grouping, Cross)."));
            for (const StageTarget *t : tl)
                add(targetsFolder, LibTarget, t->id());
        }
    }

    // Group sets (§1): named sets of groups, made here (Make set).
    QTreeWidgetItem *setsFolder = folder(all, tr("Group sets"));
    setsFolder->setData(0, PathRole, QStringLiteral("Group sets"));
    setsFolder->setToolTip(0, tr("Select groups below, right-click → Make set… (e.g. Towers = SL + SR); "
                                 "then drag the set onto a cue like a group"));
    {
        QList<GroupSet> gsl = m_st->groupSets();
        std::sort(gsl.begin(), gsl.end(), [](const GroupSet &a, const GroupSet &b) {
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
        for (const GroupSet &gs : gsl)
            add(setsFolder, LibSet, gs.id);
        if (gsl.isEmpty())
        {
            QTreeWidgetItem *none = new QTreeWidgetItem(setsFolder);
            none->setText(0, tr("(none yet: select groups, right-click → Make set…)"));
            none->setFlags(Qt::ItemIsEnabled);
            none->setData(0, KindRole, LibFolder);
        }
    }

    // Reusable layers and cues (P4): drop one on a cue for a use there.
    {
        QTreeWidgetItem *reuse = folder(all, tr("Reusable"));
        reuse->setData(0, PathRole, QStringLiteral("Reusable"));
        reuse->setToolTip(0, tr("Layers and cues made reusable (right-click a layer or cue in the tree → Make "
                                "reusable…). Drop one on a cue: a layer in it, or a cue after it -- every use "
                                "live-linked."));
        QList<QPair<QString, quint32>> items;
        for (const ReusableLayer &r : m_st->reusableLayers())
            items << qMakePair(r.name, r.id);
        for (const ReusableCue &r : m_st->reusableCues())
            items << qMakePair(r.name, r.id);
        std::sort(items.begin(), items.end(), [](const QPair<QString, quint32> &a, const QPair<QString, quint32> &b) {
            return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
        });
        for (const auto &r : items)
            if (QTreeWidgetItem *it = add(reuse, LibReusable, r.second))
                it->setToolTip(0, tr("Used by:\n%1").arg(reusableUsedBy(r.second).join(QStringLiteral("\n"))));
        if (items.isEmpty())
        {
            QTreeWidgetItem *none = new QTreeWidgetItem(reuse);
            none->setText(0, tr("(none yet: right-click a layer or a cue → Make reusable…)"));
            none->setFlags(Qt::ItemIsEnabled);
            none->setData(0, KindRole, LibFolder);
        }
    }

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
    if (kind == LibReusable)
    {
        // P4: rename, who uses it, put it in, delete.
        QMenu menu(this);
        const QString name = m_st->reusableName(id);
        menu.addAction(tr("Use in the selected cue"), this, [this, id]() {
            const quint32 cue = currentCueId();
            if (cue == invalidId())
                m_hint->setText(tr("Select a cue in the tree first"));
            else
                dropReusables(cue, { id });
        });
        menu.addAction(tr("Rename…"), this, [this, id, name]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("Rename"), tr("Name"), QLineEdit::Normal, name, &ok);
            if (ok)
                m_st->renameReusable(id, n);
        });
        QMenu *usedBy = menu.addMenu(tr("Used by"));
        usedBy->setObjectName("stReusableUsedBy");
        const QList<ShowTreeController::ReusableUse> uses = m_st->reusableUses(id);
        for (const ShowTreeController::ReusableUse &u : uses)
            usedBy->addAction(u.label, this, [this, u]() {
                if (u.layer != invalidId())
                    selectNode(u.layer, u.cue);
                else
                    selectNode(u.cue);
            });
        for (const QString &x : reusableUsedBy(id))
            if (x.startsWith(tr("VC button")) || x.startsWith(tr("reusable cue")))
                usedBy->addAction(x)->setEnabled(false);
        if (usedBy->isEmpty())
            usedBy->addAction(tr("(not used)"))->setEnabled(false);
        menu.addSeparator();
        menu.addAction(tr("Delete…"), this, [this, id]() { deleteReusable(id); });
        menu.exec(m_library->viewport()->mapToGlobal(pos));
        return;
    }
    if (kind != LibGroup && kind != LibFixture && kind != LibPalette && kind != LibFunction && kind != LibSet)
        return;
    QMenu menu(this);
    const bool pinned = isPinned(kind, id);
    menu.addAction(pinned ? tr("Unpin") : tr("Pin"), this, [this, kind, id, pinned]() {
        setPinned(kind, id, !pinned);
    });
    // Make set (§1): the groups (and fixtures) selected, as one named set.
    if (!item->isSelected())
        m_library->setCurrentItem(item);
    QList<quint32> selGroups, selFixtures;
    for (QTreeWidgetItem *sel : m_library->selectedItems())
    {
        const int k = sel->data(0, KindRole).toInt();
        const quint32 sid = sel->data(0, IdRole).toUInt();
        if (k == LibGroup && !selGroups.contains(sid))
            selGroups << sid;
        else if (k == LibFixture && !selFixtures.contains(sid))
            selFixtures << sid;
    }
    if (kind == LibGroup || kind == LibFixture || kind == LibSet)
    {
        QAction *make = menu.addAction(tr("Make set…"), this, [this]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("Make set"),
                                                    tr("Name of the set (e.g. Towers):"), QLineEdit::Normal,
                                                    QString(), &ok);
            if (ok && !n.trimmed().isEmpty())
                makeSetFromLibrary(n.trimmed());
        });
        make->setObjectName("stMakeSet");
        make->setToolTip(tr("A named set of the groups selected (⌘-click several), reusable by name "
                            "in any cue"));
    }
    if (kind == LibSet)
    {
        menu.addAction(tr("Rename set…"), this, [this, id]() {
            const GroupSet *gs = m_st->groupSet(id);
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("Rename set"), tr("Name"), QLineEdit::Normal,
                                                    gs == nullptr ? QString() : gs->name, &ok);
            if (ok)
                m_st->renameGroupSet(id, n);
        });
        if (!selGroups.isEmpty() || !selFixtures.isEmpty())
            menu.addAction(tr("Members: the %n selected group(s)", "", int(selGroups.count() + selFixtures.count())),
                           this, [this, id, selGroups, selFixtures]() {
                if (!m_st->setGroupSetMembers(id, selGroups, selFixtures))
                    QMessageBox::information(this, tr("Group set"),
                                             tr("Not changed: a child layer under a layer on this set "
                                                "would no longer be part of it."));
            });
        menu.addAction(tr("Delete set"), this, [this, id]() {
            QString why;
            if (!m_st->removeGroupSet(id, &why) && !why.isEmpty())
                QMessageBox::information(this, tr("Delete set"), why);
        });
    }
    if (kind == LibPalette && m_st->isInlinePalette(id))
    {
        menu.addAction(tr("Make reusable…"), this, [this, id]() {
            bool ok = false;
            const QLCPalette *p = m_doc->palette(id);
            if (p == nullptr)
                return;
            // Named as the show's palettes of that type are, to start with.
            const QString n = QInputDialog::getText(this, tr("Make reusable"), tr("Name"), QLineEdit::Normal,
                                                    m_st->suggestPaletteName(p->type(), p->values()), &ok);
            if (!ok || n.trimmed().isEmpty())
                return;
            const QString folder = m_st->suggestPaletteFolder(p->type());
            m_st->makeReusable(id, n.trimmed(), folder);
            reloadLibrary();
        });
    }
    menu.exec(m_library->viewport()->mapToGlobal(pos));
}

/****************************************************************************
 * Command line (§6C)
 ****************************************************************************/

void ShowTreeView::focusCommandLine()
{
    m_command->setFocus(Qt::ShortcutFocusReason);
}

QString ShowTreeView::helpOutput() const
{
    return m_commandHelp->toPlainText();
}

bool ShowTreeView::isHelpShown() const
{
    return !m_commandHelp->isHidden();
}

void ShowTreeView::focusLibraryFilter()
{
    m_filter->setFocus(Qt::ShortcutFocusReason);
    m_filter->selectAll();
    m_hint->setText(libraryReturnHint());
}

/* Where Return in the Library goes: the tree's current row -- a cue, a
   layer of its own, or a layer carried in (by its target). */
static void libraryTarget(const ShowTreeView *view, QTreeWidget *tree, quint32 *cue, quint32 *layer, Target *carried)
{
    *cue = view->currentCueId();
    *layer = invalidId();
    *carried = Target();
    QTreeWidgetItem *item = tree->currentItem();
    if (item == nullptr || *cue == invalidId())
        return;
    const int kind = item->data(0, ShowTreeView::KindRole).toInt();
    if (kind == ShowTreeView::LayerNode)
        *layer = item->data(0, ShowTreeView::IdRole).toUInt();
    else if (kind == ShowTreeView::TrackedLayerNode)
        *carried = view->carriedTarget(*cue, item->data(0, ShowTreeView::TargetRole).toString());
}

QString ShowTreeView::libraryReturnHint(bool separate) const
{
    const QList<QTreeWidgetItem *> sel = m_library->selectedItems();
    if (sel.isEmpty())
        return QString();
    quint32 cue, layer;
    Target carried;
    libraryTarget(this, m_tree, &cue, &layer, &carried);
    if (cue == invalidId())
        return tr("Select a cue or a layer in the tree: Return adds the Library's selection there");
    QScopedPointer<QMimeData> mime(static_cast<LibraryWidget *>(m_library)->mimeFor(sel));
    if (mime.isNull() || !acceptsDrop(cue, layer, mime.data(), carried))
        return QString();
    return tr("Return: %1").arg(dropHint(cue, layer, mime.data(), separate, carried));
}

bool ShowTreeView::applyLibrarySelection(bool separate)
{
    const QList<QTreeWidgetItem *> sel = m_library->selectedItems();
    quint32 cue, layer;
    Target carried;
    libraryTarget(this, m_tree, &cue, &layer, &carried);
    if (sel.isEmpty() || cue == invalidId())
    {
        if (!sel.isEmpty())
            m_hint->setText(tr("Select a cue or a layer in the tree first"));
        return false;
    }
    QScopedPointer<QMimeData> mime(static_cast<LibraryWidget *>(m_library)->mimeFor(sel));
    if (mime.isNull() || !acceptsDrop(cue, layer, mime.data(), carried))
        return false;
    // As a drop there: the cue stays (or the layer's cue page shows the
    // new layer) -- the same rules, one undo step.
    // On a cue the cue stays selected, so the next Return adds another.
    return dropMime(cue, layer, mime.data(), separate, carried, layer == invalidId() && carried.isEmpty());
}

QString ShowTreeView::dropHintBelow(quint32 cueId, const QMimeData *mime, bool separate) const
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr)
        return QString();
    Target t;
    t.groups = idsFromMime(mime, FixtureGroupSource::fixtureGroupMimeType());
    t.fixtures = idsFromMime(mime, FixtureGroupSource::fixtureMimeType());
    t.sets = idsFromMime(mime, setMimeType());
    if (t.isEmpty() || separate)
        return dropHint(cueId, invalidId(), mime, separate);
    return tr("New layer in %1: %2").arg(m_st->cueLabel(*list, list->indexOf(cueId)),
                                          t.groups.isEmpty() && t.sets.isEmpty() ? tr("these fixtures")
                                                                                 : m_st->targetLabel(t));
}

bool ShowTreeView::copySelection()
{
    // Cues in list order, else the cue's own layers selected.
    QList<quint32> cues;
    QList<QPair<quint32, quint32>> layers;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        if (!(*it)->isSelected())
            continue;
        const int kind = (*it)->data(0, KindRole).toInt();
        if (kind == CueNode)
            cues << (*it)->data(0, IdRole).toUInt();
        else if (kind == LayerNode)
            layers << qMakePair((*it)->data(0, CueRole).toUInt(), (*it)->data(0, IdRole).toUInt());
    }
    if (cues.isEmpty() && layers.isEmpty())
        return false;
    ShowTreeCommand::Clipboard clip;
    if (!cues.isEmpty())
        clip.cues = cues;
    else
        clip.layers = layers;
    m_commander->clipboard = clip;
    m_hint->setText(!cues.isEmpty() ? tr("Copied %1 cue(s): ⌘V pastes after the selected cue").arg(cues.count())
                                    : tr("Copied %1 layer(s): ⌘V pastes into the selected cue").arg(layers.count()));
    return true;
}

bool ShowTreeView::pasteClipboard()
{
    const ShowTreeCommand::Clipboard &clip = m_commander->clipboard;
    const quint32 cueId = currentCueId();
    if (clip.isEmpty() || cueId == invalidId())
        return false;
    if (!clip.cues.isEmpty())
    {
        const QList<quint32> made = m_st->pasteCues(clip.cues, cueId);
        if (made.isEmpty())
            return false;
        selectNode(made.last());
        return true;
    }
    const QList<quint32> made = m_st->pasteLayers(clip.layers, cueId);
    if (made.isEmpty())
        return false;
    selectNode(made.last(), cueId);
    return true;
}

bool ShowTreeView::duplicateSelection()
{
    const ShowTreeCommand::Clipboard keep = m_commander->clipboard;
    const bool ok = copySelection() && pasteClipboard();
    m_commander->clipboard = keep;          // ⌘D leaves what ⌘C took
    return ok;
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
    ctx.layer = currentKind() == LayerNode ? currentId() : invalidId();
    ctx.layers = selectedLayerRefs();
    ctx.recentPalettes = recentPalettes();

    const ShowTreeCommand::Result r = m_commander->run(line, ctx);
    m_commandMsg->setText(r.message);
    if (!r.help.isEmpty())
    {
        m_commandHelp->setHtml(r.help);
        m_commandHelp->show();
    }
    QPalette pal = m_commandMsg->palette();
    pal.setColor(QPalette::WindowText, r.ok ? palette().color(QPalette::WindowText) : QColor(Qt::red));
    m_commandMsg->setPalette(pal);
    if (!r.ok)
        return false;

    if (m_commandHistory.isEmpty() || m_commandHistory.last() != line)
        m_commandHistory << line;
    m_historyAt = -1;
    // Where the command leaves the selection: its layer, cue or list (or,
    // acting on the layers selected, as it was).
    if (r.keepSelection)
        return true;
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
            // Tab completes the word instead of moving focus; again, the
            // next of several matches.
            ShowTreeCommand::Context ctx;
            for (QTreeWidgetItem *item = m_tree->currentItem(); item != nullptr; item = item->parent())
                if (item->data(0, KindRole).toInt() == ProductionNode)
                    ctx.production = item->data(0, IdRole).toUInt();
            ctx.list = currentListId();
            ctx.cue = currentCueId();
            QStringList options;
            int current = -1;
            m_command->setText(m_commander->tab(m_command->text(), ctx, &options, &current));
            QStringList shown;
            const int from = current < 10 ? 0 : current - 9;
            for (int i = from; i < options.count() && shown.count() < 10; ++i)
                shown << (i == current ? QStringLiteral("[%1]").arg(options.at(i)) : options.at(i));
            if (options.count() > from + shown.count())
                shown << tr("+%1 more").arg(options.count() - from - shown.count());
            m_commandMsg->setText(options.count() > 1 ? shown.join(QStringLiteral(" · ")) : QString());
            QPalette pal = m_commandMsg->palette();
            pal.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
            m_commandMsg->setPalette(pal);
            return true;
        }
        if (ke->key() == Qt::Key_Escape)
        {
            // The help first, then back to the tree.
            if (m_commandHelp->isVisible())
                m_commandHelp->hide();
            else
                m_tree->setFocus(Qt::ShortcutFocusReason);
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
    if (obj == m_tree && ev->type() == QEvent::KeyPress && !m_renaming)
    {
        // Return / F2: rename in place, as in the Programming tab.
        const int key = static_cast<QKeyEvent *>(ev)->key();
        if ((key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_F2)
            && startRename())
            return true;
    }
    if (obj == m_tree && ev->type() == QEvent::KeyPress
        && static_cast<QKeyEvent *>(ev)->text() == QLatin1String("/"))
    {
        focusLibraryFilter();
        return true;
    }
    if (obj == m_filter && ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
        {
            // What is selected, else the first item the filter left showing.
            if (m_library->selectedItems().isEmpty())
                for (QTreeWidgetItemIterator it(m_library); *it; ++it)
                {
                    const int kind = (*it)->data(0, KindRole).toInt();
                    if (kind == LibSection || kind == LibFolder || (*it)->isHidden())
                        continue;
                    bool shown = true;
                    for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
                        shown &= !p->isHidden();
                    if (!shown)
                        continue;
                    m_library->setCurrentItem(*it);
                    (*it)->setSelected(true);
                    break;
                }
            applyLibrarySelection(ke->modifiers() & Qt::AltModifier);
            return true;
        }
        if (ke->key() == Qt::Key_Down)
        {
            m_library->setFocus(Qt::OtherFocusReason);
            return true;
        }
        if (ke->key() == Qt::Key_Escape)
        {
            m_tree->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
    }
    if (obj == m_tree && ev->type() == QEvent::KeyPress
        && static_cast<QKeyEvent *>(ev)->text() == QLatin1String(":"))
    {
        // : in the tree: to the command line (⌘L is Show Lock).
        focusCommandLine();
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}
