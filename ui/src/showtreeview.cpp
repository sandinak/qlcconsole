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
#include <QSlider>
#include <QDialog>
#include <QApplication>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDragEnterEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTreeWidget>
#include <QInputDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QToolButton>
#include <QToolBar>
#include <QGridLayout>
#include <QFormLayout>
#include <QDataStream>
#include <QScrollArea>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpression>
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
#include <QtMath>
#include <QActionGroup>
#include <QMenu>
#include <QCursor>
#include <QDrag>
#include <QPainter>
#include <algorithm>

#include "mainbar.h"
#include "moveinblackbutton.h"
#include "fadetimespin.h"
#include "programmercontroller.h"
#include "showtreecontroller.h"
#include "showtreecommand.h"
#include "showcontrollistener.h"
#include "fixturegroupsource.h"
#include "fixturemanager.h"
#include "functionstreewidget.h"
#include "linespreadpreview.h"
#include "lineplanview.h"
#include "monitorproperties.h"
#include "outputrulesdialog.h"
#include "stagetarget.h"
#include "linespread.h"
#include "effectdesigner.h"
#include "focuscallcue.h"
#include "refinepanel.h"
#include "focuscallsdialog.h"
#include "focusmenu.h"
#include "showtreeview.h"
#include "effectrunsmenu.h"
#include "controllearndialog.h"
#include "itemicons.h"
#include "workbench.h"
#include "blindstyle.h"
#include "noteedit.h"
#include "importsheet.h"
#include "showconverter.h"
#include "programmingmanager.h"
#include "inputoutputmap.h"
#include "paletteops.h"
#include "usedby.h"
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

/* Is a saved QSplitter state one for @p panes panes, with pane @p mustShow
   wider than nothing? (QSplitter::saveState(): a marker, a version, then the
   sizes.) */
static bool splitterStateUsable(const QByteArray &state, int panes, int mustShow)
{
    QDataStream in(state);
    in.setVersion(QDataStream::Qt_5_0);
    qint32 marker = 0, version = 0;
    QList<int> sizes;
    in >> marker >> version >> sizes;
    if (in.status() != QDataStream::Ok || marker != 0xff || sizes.size() != panes)
        return false;
    return mustShow < 0 || mustShow >= sizes.size() || sizes.at(mustShow) > 0;
}
#define SETTINGS_BADGE_COLUMN QStringLiteral("showtree/badgeColumn")
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
            // Looks onto a folder of their section (or the section): filed there.
            quint32 folderList = ShowTree::invalidId();
            QString folder;
            if (lookFolderAt(e->position().toPoint(), &folderList, &folder))
            {
                const QString hint = m_view->lookFolderDropHint(cues, folderList, folder);
                if (hint.isEmpty())
                    e->ignore();
                else
                {
                    e->setDropAction(Qt::MoveAction);
                    e->accept();
                }
                if (m_hint != nullptr)
                    m_hint->setText(hint);
                return;
            }
            if (cues.count() > 1 && include)
            {
                // Several cues have no one cue to include them: refused, and said.
                e->ignore();
                if (m_hint != nullptr)
                    m_hint->setText(ShowTreeView::tr("Alt-drag one cue at a time to include it"));
                return;
            }
            // Moved: before the cue in its upper half, after it in its lower.
            bool refused = false;
            const QString hint = cues.isEmpty() || layer != ShowTree::invalidId() ? QString()
                                 : include ? m_view->cueDropHint(cues.first(), cue, include)
                                 : m_view->cuesDropHint(cues, cue, dropsAfter(e->position().toPoint()), &refused);
            if (!hint.isEmpty() && !refused)
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
        quint32 fl = ShowTree::invalidId();
        QString fp;
        if (cue == ShowTree::invalidId()
            && !(e->mimeData()->hasFormat(QLatin1String(ShowTreeView::cueMimeType()))
                 && lookFolderAt(e->position().toPoint(), &fl, &fp)))
        {
            e->ignore();
            return;
        }
        const QMimeData *mime = e->mimeData();
        if (mime->hasFormat(QLatin1String(ShowTreeView::cueMimeType())))
        {
            const QList<quint32> cues = idsFromMime(mime, ShowTreeView::cueMimeType());
            quint32 folderList = ShowTree::invalidId();
            QString folder;
            if (lookFolderAt(e->position().toPoint(), &folderList, &folder))
            {
                if (m_view->dropLooksOnFolder(cues, folderList, folder))
                {
                    e->setDropAction(Qt::CopyAction);
                    e->accept();
                }
                else
                    e->ignore();
                return;
            }
            const bool include = e->modifiers() & Qt::AltModifier;
            // Handled here: never let the base class move the rows itself.
            const bool onCue = !cues.isEmpty() && layer == ShowTree::invalidId() && carried.isEmpty();
            if (onCue && (include ? cues.count() == 1 && m_view->dropCue(cues.first(), cue, true)
                                  : m_view->dropCues(cues, cue, dropsAfter(e->position().toPoint()))))
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
    /** A cue dropped at @a pos goes after the row there (its lower half). */
    bool dropsAfter(const QPoint &pos) const
    {
        QTreeWidgetItem *item = itemAt(pos);
        if (item == nullptr)
            return true;
        const QRect r = visualItemRect(item);
        return pos.y() >= r.center().y();
    }

    /** A Looks section's row or one of its folders at @a pos. */
    bool lookFolderAt(const QPoint &pos, quint32 *listId, QString *folder) const
    {
        QTreeWidgetItem *item = itemAt(pos);
        if (item == nullptr)
            return false;
        const int kind = item->data(0, ShowTreeView::KindRole).toInt();
        if (kind == ShowTreeView::LookFolderNode)
        {
            *listId = item->data(0, ShowTreeView::IdRole).toUInt();
            *folder = item->data(0, ShowTreeView::FolderRole).toString();
            return true;
        }
        if (kind == ShowTreeView::ListNode && item->data(0, ShowTreeView::FolderRole).toString() == QLatin1String("/"))
        {
            *listId = item->data(0, ShowTreeView::IdRole).toUInt();
            folder->clear();
            return true;
        }
        return false;
    }

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
        // Out to the tree and the pages; within: palettes onto a palette
        // folder (R2, Move to folder by drag).
        setDragDropMode(QAbstractItemView::DragDrop);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    /** What a drag of @a items carries (Return / double-click use it too). */
    QMimeData *mimeFor(const QList<QTreeWidgetItem *> &items) const { return mimeData(items); }
    /** Return / Enter: told, with whether ⌥ was held. */
    std::function<void(bool)> onReturn;
    /** F2: rename in place; ⌫: delete. */
    std::function<void()> onRename;
    std::function<void()> onDelete;
    /** Palettes dropped on a palette folder ("": none); false: refused. */
    std::function<bool(const QList<quint32> &, const QString &)> onPaletteDrop;

protected:
    void keyPressEvent(QKeyEvent *e) override
    {
        if (state() == QAbstractItemView::EditingState)
        {
            QTreeWidget::keyPressEvent(e);
            return;
        }
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && onReturn)
        {
            onReturn(e->modifiers() & Qt::AltModifier);
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_F2 && onRename)
        {
            onRename();
            e->accept();
            return;
        }
        if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && onDelete)
        {
            onDelete();
            e->accept();
            return;
        }
        QTreeWidget::keyPressEvent(e);
    }
    /** The palette folder a drop at @a pos files into (false: none there). */
    bool folderAt(const QPoint &pos, QString *folder) const
    {
        QTreeWidgetItem *item = itemAt(pos);
        if (item == nullptr || item->data(0, ShowTreeView::KindRole).toInt() != ShowTreeView::LibPaletteFolder)
            return false;
        *folder = item->data(0, ShowTreeView::FolderRole).toString();
        return true;
    }
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (e->source() == this && e->mimeData()->hasFormat(QLatin1String(FunctionsTreeWidget::paletteDragMimeType())))
            e->acceptProposedAction();
        else
            e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override
    {
        QString folder;
        if (e->source() == this && folderAt(e->position().toPoint(), &folder)
            && e->mimeData()->hasFormat(QLatin1String(FunctionsTreeWidget::paletteDragMimeType())))
        {
            e->setDropAction(Qt::MoveAction);
            e->accept();
        }
        else
            e->ignore();
    }
    void dropEvent(QDropEvent *e) override
    {
        // Handled here: never let the base class move the rows itself.
        QString folder;
        const QList<quint32> ids = idsFromMime(e->mimeData(), FunctionsTreeWidget::paletteDragMimeType());
        if (e->source() == this && folderAt(e->position().toPoint(), &folder) && !ids.isEmpty() && onPaletteDrop
            && onPaletteDrop(ids, folder))
        {
            e->setDropAction(Qt::CopyAction);
            e->accept();
        }
        else
            e->ignore();
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

/* The drop hint at the top right: while dragging, the one place that says
   where a drop goes, and after a change, what it changed. The Show Tree's
   tools share the window's top bar with the show controls while the tab is
   on show (MainBar), and a toolbar folds its end into » first -- which, at
   any laptop's width, was the hint. So the hint stands outside the toolbar
   and keeps its room (up to kHintRoom); the buttons fold instead. */
const int kHintRoom = 420;

class DropHintLabel final : public QLabel
{
public:
    explicit DropHintLabel(QWidget *parent) : QLabel(parent) {}

    QSize minimumSizeHint() const override
    {
        const QSize s = QLabel::sizeHint();
        return QSize(qMin(s.width(), kHintRoom), s.height());
    }
};

/* What a venue focus cue does, said on its tick box and menu entry. */
QString venueFocusCueTip()
{
    return ShowTreeView::tr("Pre-show: GO onto this cue plays its looks as a preset and waits; the "
                            "voice says \"Venue focus ready\". Start (Menu) on the controller, or "
                            "\u2318\u21a9, begins the show's focus calls; Menu, or past the last "
                            "call, goes on to the next cue.");
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

    // Header: a toolbar, so the main window's top bar can borrow it while
    // this is the current tab (MainBar), and a narrow window folds its end
    // into the toolbar's » menu instead of cutting it off. The drop hint
    // stands beside it, never folded (DropHintLabel).
    QWidget *header = new QWidget(this);
    header->setObjectName(QStringLiteral("stHeader"));
    QHBoxLayout *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(0);
    QToolBar *bar = new QToolBar(header);
    bar->setObjectName(QStringLiteral("stToolBar"));
    bar->setMovable(false);
    bar->setFloatable(false);
    auto make = [this](const QString &text, const QString &tip, const char *name) {
        QToolButton *b = new QToolButton(this);
        b->setText(text);
        b->setToolTip(tip);
        b->setObjectName(QLatin1String(name));
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return b;
    };
    auto button = [bar, &make](const QString &text, const QString &tip, const char *name) {
        QToolButton *b = make(text, tip, name);
        bar->addWidget(b);
        return b;
    };
    /* Q96: + Production, + Sub-cue and Merge live under More ▾. Each keeps
       its button, never shown, so its enabled state and action stay in one
       place; the menu entry clicks it. */
    auto offBar = [bar, &make](const QString &text, const QString &tip, const char *name) {
        QToolButton *b = make(text, tip, name);
        bar->addWidget(b)->setVisible(false);   // in the bar, never shown
        return b;
    };
    // On the bar only while the show has none: it's the first thing an empty
    // show needs (lesson 1.1). After that, under More ▾.
    m_newProduction = make(tr("+ Production"), tr("A new production: the top of a show's tree"), "stNewProduction");
    m_newProductionAction = bar->addWidget(m_newProduction);
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
    m_newSubCue = offBar(tr("+ Sub-cue"), tr("A sub-cue of the selected cue (4.1, 4.2…): Sub GO (⌥⌘↩) steps "
                                             "through them while GO goes on to the next cue. It starts from what "
                                             "is on stage before it; its changes are cue-only."), "stNewSubCue");
    connect(m_newSubCue, &QToolButton::clicked, this, [this]() { newSubCue(); });
    // W3: the selected cue / look / layer in the Workbench, docked here.
    {
        QToolButton *wb = button(tr("✎ Workbench"), tr("Open the selected cue, hit or layer in the Workbench, "
                                                      "here in place of the page (double-click, or wb): its "
                                                      "layers and what tracks in, each value's editor at full "
                                                      "height. ↩ Show Tree comes back."), "stOpenWorkbench");
        connect(wb, &QToolButton::clicked, this, [this]() { openSelectionInWorkbench(); });
    }
    m_merge = offBar(tr("Merge"), tr("Merge the layers selected in the tree (⌘- or ⇧-click two or more "
                                     "layers of one cue) into one layer on all their groups"), "stMerge");
    {
        QToolButton *more = button(tr("More"), tr("+ Production, + Sub-cue and Merge"), "stMore");
        more->setPopupMode(QToolButton::InstantPopup);
        QMenu *m = new QMenu(more);
        m->setToolTipsVisible(true);
        for (QToolButton *b : { m_newProduction, m_newSubCue, m_merge })
        {
            QAction *a = m->addAction(b->text(), b, &QToolButton::click);
            a->setToolTip(b->toolTip());
            a->setObjectName(b->objectName() + QStringLiteral("Action"));
        }
        connect(m, &QMenu::aboutToShow, this, [m, this]() {
            const QList<QAction *> acts = m->actions();
            const QList<QToolButton *> btns = { m_newProduction, m_newSubCue, m_merge };
            for (int i = 0; i < acts.size() && i < btns.size(); i++)
                acts.at(i)->setEnabled(btns.at(i)->isEnabled());
        });
        more->setMenu(m);
    }
    m_delete = button(tr("Delete"), tr("Delete the selected cues and layers, or a cue list or production"),
                      "stDelete");
    bar->addSeparator();
    m_undo = button(tr("Undo"), tr("Undo the last change to the tree (⌘Z)"), "stUndo");
    m_redo = button(tr("Redo"), tr("Redo (⇧⌘Z)"), "stRedo");
    bar->addSeparator();
    // The one door from old chasers to cue lists (Q76).
    m_import = button(tr("Upgrade This Show…"), tr("Make the show's cue-list chasers Show Tree cue lists, "
                                                   "checked channel for channel: tick the ones you want"),
                      "stUpgradeShow");
    m_importAction = bar->actions().last();     // a bar's widget is shown and hidden by its action
    connect(m_import, &QToolButton::clicked, this, [this]() { openUpgrade(); });
    // P5: the current production -- what GO, the footer, VC and show
    // control play.
    bar->addSeparator();
    m_productionLabelAction = bar->addWidget(new QLabel(tr("Production:"), this));
    m_production = new QComboBox(this);
    m_production->setObjectName("stProduction");
    m_production->setToolTip(tr("The current production: what the footer GO (⌘↩), show control (QLab, OSC, "
                                "MSC) and a Cue Runner following \"the current production\" play. Its main "
                                "list becomes the show's main cue list; its timecode source is followed. "
                                "A list already running keeps running."));
    m_production->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_productionAction = bar->addWidget(m_production);
    connect(m_production, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        m_st->setCurrentProduction(m_production->itemData(index).toUInt());
    });
    connect(m_st, &ShowTreeController::currentProductionChanged, this, [this]() {
        updateProductionSwitcher();
        if (!m_reloading)
        {
            buildTree();
            updateInspector();
        }
    });
    {
        QWidget *stretch = new QWidget(bar);
        stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        bar->addWidget(stretch);
    }
    // What the rig shows: the selected cue (Design), or nothing (Live).
    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("stPreview");
    bar->addWidget(m_previewLabel);
    m_previewPlay = new QToolButton(this);
    m_previewPlay->setObjectName("stPreviewPlay");
    m_previewPlay->setText(tr("Play"));
    m_previewPlay->setToolTip(tr("Preview the selected cue over the running show (the edit wins on the "
                                 "channels it plays), until the next GO"));
    connect(m_previewPlay, &QToolButton::clicked, this, [this]() {
        commitPendingCueEdits();   // a time typed and not yet left plays too
        m_st->resumePreview();
    });
    m_previewPlayAction = bar->addWidget(m_previewPlay);
    m_previewPlayAction->setVisible(false);
    headerRow->addWidget(bar, 1);
    // Auto move-in-black: the show's one setting (Programming's Move in
    // black menu and Settings show it too). Beside the bar, not in it, so a
    // narrow window never folds it into » -- in the top bar it sits by the
    // show controls, where an operator running cues looks.
    m_moveInBlack = new MoveInBlackButton(m_doc, header);
    headerRow->addSpacing(6);
    headerRow->addWidget(m_moveInBlack);
    headerRow->addSpacing(12);
    m_hint = new DropHintLabel(header);
    m_hint->setObjectName("stDropHint");
    headerRow->addWidget(m_hint);
    main->addWidget(header);
    MainBar::registerTabTools(this, header);

    // W5: an old show (chasers, no tree) says so once, never converting on
    // its own: Convert… opens the show converter; ✕ for this show.
    m_bridgeBanner = new QFrame(this);
    m_bridgeBanner->setObjectName(QStringLiteral("stBridgeBanner"));
    m_bridgeBanner->setFrameShape(QFrame::StyledPanel);
    {
        QHBoxLayout *h = new QHBoxLayout(m_bridgeBanner);
        h->setContentsMargins(8, 4, 4, 4);
        m_bridgeText = new QLabel(m_bridgeBanner);
        m_bridgeText->setObjectName(QStringLiteral("stBridgeText"));
        m_bridgeText->setWordWrap(true);
        h->addWidget(m_bridgeText, 1);
        QPushButton *convert = new QPushButton(tr("Upgrade this show…"), m_bridgeBanner);
        convert->setObjectName(QStringLiteral("stBridgeUpgrade"));
        convert->setToolTip(tr("Each chaser and collection with its plan, checked, and raw scenes into palettes; "
                               "nothing changes until you press Upgrade, and it is one undo step"));
        connect(convert, &QPushButton::clicked, this, [this]() { openUpgrade(); });
        h->addWidget(convert);
        QToolButton *dismiss = new QToolButton(m_bridgeBanner);
        dismiss->setObjectName(QStringLiteral("stBridgeDismiss"));
        dismiss->setText(QStringLiteral("✕"));
        dismiss->setAutoRaise(true);
        dismiss->setToolTip(tr("Not now: File ▸ Upgrade this show… is always there"));
        connect(dismiss, &QToolButton::clicked, this, [this]() {
            m_bridgeDismissed = true;
            updateBridgeBanner();
        });
        h->addWidget(dismiss);
    }
    m_bridgeBanner->hide();
    main->addWidget(m_bridgeBanner);

    // The command line (§6C): optional, hidden until asked for.
    m_commander = new ShowTreeCommand(m_doc, m_st);
    m_commander->setRecordChooser([this](quint32 cueId, const QList<RecordSource> &choices, RecordSource *chosen) {
        return chooseRecordSource(cueId, choices, chosen);
    });
    m_commander->setVcPageMaker([this](quint32 listId, const QString &folder, QString *message) {
        return makeVcPage(listId, folder, message);
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
        // Q59: the syntax lines, folded away until asked for: this button,
        // ? typed first in the line, or F1 there. Remembered.
        m_commandSyntaxButton = new QToolButton(m_commandBar);
        m_commandSyntaxButton->setObjectName("stCommandSyntaxButton");
        m_commandSyntaxButton->setText(QStringLiteral("?"));
        m_commandSyntaxButton->setCheckable(true);
        m_commandSyntaxButton->setAutoRaise(true);
        m_commandSyntaxButton->setToolTip(tr("Show or hide the command line's syntax (also ? typed first, "
                                             "or F1 in the line). Type help for every word, with examples."));
        h->addWidget(m_commandSyntaxButton);
        m_commandMsg = new QLabel(m_commandBar);
        m_commandMsg->setObjectName("stCommandMessage");
        h->addWidget(m_commandMsg);
        v->addLayout(h);
        m_commandSyntax = new QLabel(ShowTreeCommand::grammar(), m_commandBar);
        m_commandSyntax->setObjectName("stCommandHint");
        m_commandSyntax->setWordWrap(true);
        m_commandSyntax->setEnabled(false);
        v->addWidget(m_commandSyntax);
        const bool syntax = QSettings().value(QStringLiteral("showtree/commandSyntax"), false).toBool();
        m_commandSyntax->setVisible(syntax);
        m_commandSyntaxButton->setChecked(syntax);
        connect(m_commandSyntaxButton, &QToolButton::toggled, this, &ShowTreeView::setCommandSyntaxShown);
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
    {
        /* The docked picker (Q61): Level… and Colour… open here, in the
           window, above the command line. */
        m_picker = new QFrame(this);
        m_picker->setObjectName("stDockedPicker");
        qobject_cast<QFrame *>(m_picker)->setFrameShape(QFrame::StyledPanel);
        QHBoxLayout *row = new QHBoxLayout(m_picker);
        row->setContentsMargins(8, 6, 8, 6);
        m_pickerTitle = new QLabel(m_picker);
        m_pickerTitle->setObjectName("stPickerTitle");
        QFont bold = m_pickerTitle->font();
        bold.setBold(true);
        m_pickerTitle->setFont(bold);
        row->addWidget(m_pickerTitle, 0, Qt::AlignTop);
        m_pickerBody = new QHBoxLayout;
        row->addLayout(m_pickerBody, 1);
        QVBoxLayout *buttons = new QVBoxLayout;
        QPushButton *apply = new QPushButton(tr("Apply"), m_picker);
        apply->setObjectName("stPickerApply");
        apply->setDefault(true);
        QPushButton *cancel = new QPushButton(tr("Cancel"), m_picker);
        cancel->setObjectName("stPickerCancel");
        buttons->addWidget(apply);
        buttons->addWidget(cancel);
        buttons->addStretch(1);
        row->addLayout(buttons);
        connect(apply, &QPushButton::clicked, this, &ShowTreeView::applyDockedPicker);
        connect(cancel, &QPushButton::clicked, this, &ShowTreeView::closeDockedPicker);
        for (const QKeySequence &k : { QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter) })
        {
            QShortcut *sc = new QShortcut(k, m_picker);
            sc->setContext(Qt::WidgetWithChildrenShortcut);
            connect(sc, &QShortcut::activated, this, &ShowTreeView::applyDockedPicker);
        }
        QShortcut *esc = new QShortcut(QKeySequence(Qt::Key_Escape), m_picker);
        esc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(esc, &QShortcut::activated, this, &ShowTreeView::closeDockedPicker);
        m_picker->hide();
        main->addWidget(m_picker);
    }
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
    connect(m_lookEditor, &LookEditor::usedByLinkActivated, this, &ShowTreeView::usedByLinkActivated);
    // ↩ Back (Esc): the Look Editor closes, the page has the column again.
    m_lookEditor->setBackText(tr("↩ Back"));
    connect(m_lookEditor, &LookEditor::backRequested, this, [this]() {
        editPalette(QLCPalette::invalidId());
        m_tree->setFocus(Qt::OtherFocusReason);
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
    // A cue row's tick: unticked, the cue is skipped (GO passes over it).
    // On a selected row it acts on every selected cue. After the click is
    // done with the row: the edit rebuilds the tree.
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (m_reloading || m_renaming || column != 0 || item == nullptr
            || item->data(0, KindRole).toInt() != CueNode || !item->data(0, Qt::CheckStateRole).isValid())
            return;
        const quint32 cueId = item->data(0, IdRole).toUInt();
        const bool skip = item->checkState(0) == Qt::Unchecked;
        if (skip == m_st->isCueSkipped(cueId))
            return;
        QList<quint32> cues = item->isSelected() ? selectedCues() : QList<quint32>();
        if (!cues.contains(cueId))
            cues = { cueId };
        QTimer::singleShot(0, this, [this, cues, skip]() { setCuesSkipped(cues, skip); });
    });
    connect(m_tree->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this]() {
        if (!m_renaming)
            return;
        m_renaming = false;
        QTimer::singleShot(0, this, [this]() { buildTree(); });
    });
    m_tree->setObjectName("stTree");
    /* One letter each, the full name and meaning on hover ("the new
       columns are too wide .. S for sets .. T for tracks .. R for
       releases with mouseover on title"). */
    m_tree->setHeaderLabels({ tr("Show"), tr(""), tr("S"), tr("T"), tr("R"), tr("M"), tr("F") });
    {
        // What each column is, on its header (Q104).
        const QStringList tips = {
            QString(), QString(),
            tr("S \u2014 Sets. A cue: how many layers it sets here. A layer: \u25cf it sets values "
               "in this cue."),
            tr("T \u2014 Tracks in. A cue: how many layers track into it from earlier cues. A carried "
               "layer: \u25cf."),
            tr("R \u2014 Releases. A cue: how many layers it lets go. A layer: \u25cf it is released here."),
            tr("M \u2014 Mode. B: Block (starts from nothing). C: Cue-only (its changes don't pass on). "
               "Blank: it tracks."),
            tr("F \u2014 Flags. base: the list's base cue. skip: GO passes over it. no MIB: no move in "
               "black into it. focus: a venue focus cue (GO arms Venue focus). \u2295 includes, "
               "\u00d7 reusable uses, mix, VC buttons that fire it.") };
        for (int c = 0; c < tips.size(); ++c)
            if (!tips.at(c).isEmpty())
                m_tree->headerItem()->setToolTip(c, tips.at(c));
    }
    /* Right-click the header: show or hide each flag column, remembered. */
    m_tree->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree->header(), &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu menu(this);
        for (int c = SetsCol; c < TreeColumnCount; ++c)
        {
            // Its full name: the tooltip's "S — Sets." part.
            const QString tip = m_tree->headerItem()->toolTip(c);
            QAction *a = menu.addAction(tip.section(QLatin1Char('.'), 0, 0));
            a->setCheckable(true);
            a->setChecked(!m_hiddenColumns.contains(c));
            connect(a, &QAction::toggled, this, [this, c](bool on) {
                if (on)
                    m_hiddenColumns.remove(c);
                else
                    m_hiddenColumns.insert(c);
                QStringList hidden;
                for (int k : std::as_const(m_hiddenColumns))
                    hidden << QString::number(k);
                QSettings().setValue(QStringLiteral("showtree/hiddenColumns"), hidden);
                fitTreeColumns();
            });
        }
        menu.exec(m_tree->header()->mapToGlobal(pos));
    });
    for (const QString &c : QSettings().value(QStringLiteral("showtree/hiddenColumns")).toStringList())
        if (c.toInt() >= SetsCol && c.toInt() < TreeColumnCount)
            m_hiddenColumns.insert(c.toInt());
    // The names get the room (Branson, 2026-10-21: the "tracked from …"
    // column grew to its text and covered the layer names). The badge
    // column is narrow and fixed as the pane is resized; dragging the line
    // between the columns sets it, remembered. Text too long for either
    // column ends in "…", never over the other.
    m_tree->setTextElideMode(Qt::ElideRight);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setMinimumSectionSize(24);
    m_badgeWidth = QSettings().value(SETTINGS_BADGE_COLUMN,
                                     m_tree->fontMetrics().horizontalAdvance(QStringLiteral("Tracking · main"))
                                     + 12).toInt();
    m_tree->viewport()->installEventFilter(this);
    connect(m_tree->header(), &QHeaderView::sectionResized, this, [this](int section, int, int size) {
        if (m_fittingColumns)
            return;
        // Dragged by hand: the line between the first two columns or the
        // info column's own edge; the flag columns keep their width.
        if (section > InfoCol)
            return;
        int flags = 0;
        for (int c = SetsCol; c < TreeColumnCount; ++c)
            if (!m_tree->header()->isSectionHidden(c))
                flags += m_tree->header()->sectionSize(c);
        m_badgeWidth = qMax(24, section == 0 ? m_tree->viewport()->width() - flags - size : size);
        QSettings().setValue(SETTINGS_BADGE_COLUMN, m_badgeWidth);
        fitTreeColumns();
    });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &ShowTreeView::slotTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &ShowTreeView::slotTreeContextMenu);
    // W3: double-click a cue, a look or a layer: it opens in the Workbench,
    // docked here (a layer that plays a function as it is, W0's Open, too:
    // its editor is the Workbench's, and ▣ Open loads it there).
    m_tree->setExpandsOnDoubleClick(false);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        const int kind = item != nullptr ? item->data(0, KindRole).toInt() : NoNode;
        if (kind == CueNode || kind == LayerNode || kind == TrackedLayerNode)
        {
            m_tree->setCurrentItem(item, 0, kSelectOnly);
            openSelectionInWorkbench();
            return;
        }
        if (item != nullptr && item->childCount() > 0)
            item->setExpanded(!item->isExpanded());
    });
    split->addWidget(m_tree);

    // Right: the inspector (the page), the Look Editor under it
    m_pages = new QStackedWidget(vsplit);
    m_pages->setObjectName("stInspector");
    m_emptyPage = new QLabel(tr("Select a cue or a layer.\n\nStart with + Production, or turn the "
                                "chasers you already have into cue lists (Upgrade This Show…)."), m_pages);
    m_emptyPage->setAlignment(Qt::AlignCenter);
    m_emptyPage->setWordWrap(true);
    m_pages->addWidget(m_emptyPage);

    // A cue list
    m_listPage = new QWidget(m_pages);
    {
        QFormLayout *f = new QFormLayout(m_listPage);
        m_listForm = f;
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
        m_listColour = new QToolButton(m_listPage);
        m_listColour->setObjectName("stListColour");
        m_listColour->setPopupMode(QToolButton::InstantPopup);
        m_listColour->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_listColour->setToolTip(tr("The colour its cues' text is shown in -- here, in the Run tab and on the "
                                    "VC -- unless a cue has its own. Only how it reads: nothing plays "
                                    "differently."));
        {
            QMenu *m = new QMenu(m_listColour);
            connect(m, &QMenu::aboutToShow, this, [this, m]() {
                m->clear();
                if (currentKind() == ListNode)
                    colourMenu(m, {}, currentId());
            });
            m_listColour->setMenu(m);
        }
        f->addRow(tr("Text colour"), m_listColour);
        // Q38: the list's own move in black, else the show's.
        m_listMib = new QComboBox(m_listPage);
        m_listMib->setObjectName("stListMib");
        m_listMib->addItem(QString(), int(MoveInBlack::Show));
        m_listMib->addItem(tr("On"), int(MoveInBlack::On));
        m_listMib->addItem(tr("Off"), int(MoveInBlack::Off));
        m_listMib->setToolTip(tr("Automatic move in black for this list's cues: the show's setting (the MIB "
                                 "button), or On / Off for this list whatever the show says. A cue can still "
                                 "say \"Don't move in black into this cue\". The lead time is the show's."));
        f->addRow(tr("Move in black"), m_listMib);
        connect(m_listMib, QOverload<int>::of(&QComboBox::activated), this, &ShowTreeView::slotListMibChanged);
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
                                    "edit in Build ▸ Scenes; the list leaves the Show Tree. One undo step."));
        h->addWidget(m_listConvert);
        h->addWidget(m_listDetach);
        // W5: an import in place, undone.
        m_listRestore = new QPushButton(tr("Restore original…"), row);
        m_listRestore->setObjectName("stListRestore");
        m_listRestore->setToolTip(tr("Undo Import in place: the list leaves the Show Tree, the chaser and its "
                                     "originals go back to their folders, and what fires the list fires the "
                                     "chaser again. One undo step."));
        connect(m_listRestore, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ListNode)
                restoreOriginal(currentId());
        });
        m_listRestore->hide();
        h->addWidget(m_listRestore);
        // R3: a Looks section or folder -- a VC page of buttons for it.
        m_listVcPage = new QPushButton(tr("Make VC page"), row);
        m_listVcPage->setObjectName("stMakeVcPage");
        m_listVcPage->setToolTip(tr("A frame on the Virtual Console with one button per hit here, "
                                    "captioned with its name and coloured by its first colour"));
        h->addWidget(m_listVcPage);
        h->addStretch(1);
        f->addRow(row);
        connect(m_listVcPage, &QPushButton::clicked, this, [this]() {
            const quint32 listId = currentListId();
            QString msg;
            makeVcPage(listId, currentLookFolder(), &msg);
            m_hint->setText(msg);
        });
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

    // A production (P5): current or not, its main list, its timecode.
    m_prodPage = new QWidget(m_pages);
    m_prodPage->setObjectName("stProductionPage");
    {
        QFormLayout *f = new QFormLayout(m_prodPage);
        m_prodTitle = new QLabel(m_prodPage);
        QFont bold = m_prodTitle->font();
        bold.setBold(true);
        m_prodTitle->setFont(bold);
        f->addRow(m_prodTitle);
        m_prodCurrent = new QPushButton(m_prodPage);
        m_prodCurrent->setObjectName("stProdCurrent");
        m_prodCurrent->setToolTip(tr("GO, the footer, show control and the Cue Runner play the current "
                                     "production's main list"));
        f->addRow(tr("Current"), m_prodCurrent);
        connect(m_prodCurrent, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ProductionNode)
                m_st->setCurrentProduction(m_st->currentProduction() == currentId() ? invalidId() : currentId());
        });
        m_prodMainList = new QComboBox(m_prodPage);
        m_prodMainList->setObjectName("stProdMainList");
        m_prodMainList->setToolTip(tr("The list GO plays while this production is current"));
        f->addRow(tr("Main list"), m_prodMainList);
        connect(m_prodMainList, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            m_st->setMainList(m_prodMainList->itemData(index).toUInt());
        });
        m_prodTimecode = new QComboBox(m_prodPage);
        m_prodTimecode->setObjectName("stProdTimecode");
        m_prodTimecode->setToolTip(tr("Where this production's timecode comes from while it is current (MTC "
                                      "from QLab, a desk...). The show's setting is in Connections → Show "
                                      "control & sync."));
        f->addRow(tr("Timecode"), m_prodTimecode);
        connect(m_prodTimecode, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
            if (currentKind() == ProductionNode)
                m_st->setProductionTimecode(currentId(), m_prodTimecode->itemData(index).toInt());
        });
        m_prodInfo = new QLabel(m_prodPage);
        m_prodInfo->setWordWrap(true);
        f->addRow(m_prodInfo);
    }
    m_pages->addWidget(m_prodPage);
    // Q190: the numbering scheme, Merge lists into one…, a song section's page.
    buildSectionPages();

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
        m_cueForm = f;
        cueScroll->setWidget(inner);
        m_cueNumber = new QLineEdit(inner);
        m_cueNumber->setObjectName("stCueNumber");
        m_cueName = new QLineEdit(inner);
        m_cueName->setObjectName("stCueName");
        m_cueNote = new NoteEdit(inner);
        m_cueNote->setObjectName("stCueNote");
        m_cueNote->setHelpTip(tr("It opens up while you type: Return applies it, Shift+Return "
                                 "starts a new line, Esc puts it back."));
        m_cueMode = new QComboBox(inner);
        m_cueMode->setObjectName("stCueMode");
        for (CueMode m : { CueMode::ListDefault, CueMode::Track, CueMode::Block, CueMode::CueOnly })
            m_cueMode->addItem(cueModeLabel(m), int(m));
        m_cueMode->setToolTip(tr("Track: starts from the cue before. Block: starts from nothing. "
                                 "Cue-only: its changes don't pass on to the next cue."));
        auto seconds = [inner]() {
            QDoubleSpinBox *s = new SecondsSpinBox(inner);
            s->setDecimals(2);
            s->setRange(0, 3600);
            s->setSuffix(ShowTreeView::tr(" s"));
            return s;
        };
        m_cueFadeIn = seconds();
        m_cueFadeOut = seconds();
        m_cueHold = seconds();
        m_cueFadeIn->setObjectName("stCueFadeIn");
        m_cueFadeOut->setObjectName("stCueFadeOut");
        m_cueHold->setObjectName("stCueHold");
        m_cueHold->setRange(-1, 36000);
        m_cueHold->setSpecialValueText(tr("∞ — waits for GO"));
        /* Focus ▾ (board Q149): this cue from the stage, and making it the
           show's venue focus cue -- in the header, where it is seen. */
        {
            QToolButton *focus = new QToolButton(inner);
            focus->setObjectName("stCueFocusMenu");
            focus->setText(tr("Focus"));
            focus->setToolTip(tr("Focus this cue from the stage, or make it the show's venue focus cue"));
            focus->setPopupMode(QToolButton::InstantPopup);
            focus->setToolButtonStyle(Qt::ToolButtonTextOnly);
            QMenu *m = new QMenu(focus);
            m->setObjectName("stCueFocusMenuPopup");
            m->setToolTipsVisible(true);
            connect(m, &QMenu::aboutToShow, this, [this, m]() { fillCueFocusMenu(m); });
            focus->setMenu(m);
            QHBoxLayout *head = new QHBoxLayout();
            head->addStretch(1);
            head->addWidget(focus);
            f->addRow(head);
        }
        f->addRow(tr("Number"), m_cueNumber);
        f->addRow(tr("Name"), m_cueName);
        f->addRow(tr("Note"), m_cueNote);
        m_cueColour = new QToolButton(inner);
        m_cueColour->setObjectName("stCueColour");
        m_cueColour->setPopupMode(QToolButton::InstantPopup);
        m_cueColour->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_cueColour->setToolTip(tr("The colour its text is shown in -- here, in the Run tab and on the VC. "
                                   "None: its list's. Only how it reads: nothing plays differently."));
        {
            QMenu *m = new QMenu(m_cueColour);
            connect(m, &QMenu::aboutToShow, this, [this, m]() {
                m->clear();
                if (m_pageCue != invalidId())
                    colourMenu(m, { m_pageCue });
            });
            m_cueColour->setMenu(m);
        }
        f->addRow(tr("Text colour"), m_cueColour);
        // R3: a look is fired by VC buttons (P-D7): which, and Make VC page.
        m_lookFiredBy = new QLabel(inner);
        m_lookFiredBy->setObjectName("stLookFiredBy");
        m_lookFiredBy->setWordWrap(true);
        m_lookFiredBy->setTextFormat(Qt::RichText);
        connect(m_lookFiredBy, &QLabel::linkActivated, this, &ShowTreeView::lookLinkActivated);
        f->addRow(tr("Fired by"), m_lookFiredBy);
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
        m_cueMix->setToolTip(tr("Off (the one rule, as everywhere in the tree): where layers share fixtures, "
                                "the most specific source wins what both set -- a layer's own over a cue look, "
                                "a child over its parent, then the lower layer -- and the other isn't lit there "
                                "for it. On: they both play, highest intensity wins -- how an imported chaser "
                                "played. Convert to tracking turns it off wherever that plays the same."));
        f->addRow(QString(), m_cueMix);
        connect(m_cueMix, &QCheckBox::toggled, this, &ShowTreeView::slotCueMixToggled);
        m_cueSkip = new QCheckBox(tr("Skip: GO passes over it"), inner);
        m_cueSkip->setObjectName("stCueSkip");
        m_cueSkip->setToolTip(tr("Skipped: GO, auto-follow and Back pass over it, and it tracks as if it "
                                 "weren't in the list (the next cue starts from the cue before it). A jump "
                                 "straight to it still plays it. The tick on its row in the tree, or ⌘/, "
                                 "does the same."));
        f->addRow(QString(), m_cueSkip);
        connect(m_cueSkip, &QCheckBox::toggled, this, &ShowTreeView::slotCueSkipToggled);
        m_cueNoMib = new QCheckBox(tr("Don't move in black into this cue"), inner);
        m_cueNoMib->setObjectName("stCueNoMib");
        m_cueNoMib->setToolTip(tr("No head is pre-positioned in black for this cue, whatever its list and the "
                                  "show say -- for a cue whose heads must be seen moving, or that follows too "
                                  "closely to move them unseen. Right-click a cue, or nomib 0.0.3, does the "
                                  "same."));
        f->addRow(QString(), m_cueNoMib);
        /* A focus-call cue (docs/design/FOCUS_REFINE_DESIGN.md section 7): GO arms the
           show's focus calls; the controller's Start begins them. */
        m_cueFocusCall = new QCheckBox(tr("Make this a venue focus cue"), inner);
        m_cueFocusCall->setObjectName("stCueFocusCall");
        m_cueFocusCall->setToolTip(venueFocusCueTip());
        f->addRow(QString(), m_cueFocusCall);
        connect(m_cueFocusCall, &QCheckBox::toggled, this, [this](bool on) {
            if (m_inspectorUpdating || m_pageCue == invalidId())
                return;
            m_st->setCuesFocusCall({ m_pageCue }, on);
        });
        m_cueFocusState = new QLabel(inner);
        m_cueFocusState->setObjectName("stCueFocusState");
        m_cueFocusState->setStyleSheet(QStringLiteral("color: #d08a1c; font-weight: bold;"));
        m_cueFocusState->hide();
        f->addRow(QString(), m_cueFocusState);
        if (FocusCallCue *fcc = m_doc->programmer()->focusCallCue())
            connect(fcc, &FocusCallCue::stateChanged, this, &ShowTreeView::refreshFocusCallState);
        /* Focus this cue (docs/design/FOCUS_REFINE_DESIGN.md): this cue's own values
           from the stage, with the controller. */
        QPushButton *refine = new QPushButton(tr("Focus this cue\342\200\246"), inner);
        refine->setObjectName("stCueRefine");
        m_cueRefine = refine;
        refine->setToolTip(FocusMenu::focusCueTip(m_doc));
        connect(refine, &QPushButton::clicked, this, [this]() {
            if (currentCueId() != invalidId())
                RefinePanel::open(m_doc, window(), currentCueId());
        });
        f->addRow(QString(), refine);
        connect(m_cueNoMib, &QCheckBox::toggled, this, &ShowTreeView::slotCueNoMibToggled);
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
        {
            // P6: a sub-cue goes on by Sub GO, or by itself after a wait.
            m_cueAdvanceRow = new QWidget(inner);
            QHBoxLayout *h = new QHBoxLayout(m_cueAdvanceRow);
            h->setContentsMargins(0, 0, 0, 0);
            m_cueAdvance = new QComboBox(m_cueAdvanceRow);
            m_cueAdvance->setObjectName("stSubCueAdvance");
            m_cueAdvance->addItem(tr("Sub GO (⌥⌘↩)"), 0);
            m_cueAdvance->addItem(tr("Auto-follow after"), 1);
            m_cueAdvance->setToolTip(tr("Sub GO: it waits for Sub GO. Auto-follow: it goes on by itself the wait "
                                        "after the sub-cue before it has faded in. The first sub-cue plays with "
                                        "GO."));
            m_cueWait = seconds();
            m_cueWait->setObjectName("stSubCueWait");
            h->addWidget(m_cueAdvance);
            h->addWidget(m_cueWait);
            h->addStretch(1);
            f->addRow(tr("Advance"), m_cueAdvanceRow);
            auto commit = [this]() {
                if (m_inspectorUpdating || m_pageCue == invalidId())
                    return;
                const bool follow = m_cueAdvance->currentData().toInt() == 1;
                m_cueWait->setEnabled(follow);
                m_st->setSubCueFollow(m_pageCue, follow, uint(qRound(m_cueWait->value() * 1000)));
            };
            connect(m_cueAdvance, QOverload<int>::of(&QComboBox::activated), this, commit);
            // Q60: typed, then Return, focus out or a click elsewhere.
            connect(m_cueWait, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (!m_inspectorUpdating && m_pageCue != invalidId())
                    m_cueWaitDirty = true;
            });
            connect(m_cueWait, &QDoubleSpinBox::editingFinished, this, [this]() { commitPendingCueEdits(); });
            m_cueWait->installEventFilter(this);
            // A cue's sub-cues: which, and + Sub-cue.
            m_cueSubCues = new QLabel(inner);
            m_cueSubCues->setObjectName("stCueSubCues");
            m_cueSubCues->setWordWrap(true);
            m_cueSubCues->setTextFormat(Qt::RichText);
            connect(m_cueSubCues, &QLabel::linkActivated, this, [this](const QString &link) {
                if (link == QLatin1String("new"))
                    newSubCue();
                else
                    selectNode(link.toUInt());
            });
            f->addRow(tr("Sub-cues"), m_cueSubCues);
        }
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
            m_cueDropActive = false;
            styleCueDropZone();
            inner->onActive = [this](bool active) {
                m_cueDropActive = active;
                styleCueDropZone();
            };
            f->addRow(tr("Layers"), box);
            connect(m_cueLayers, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
                if (item->data(KindRole).toInt() == LayerNode)
                    selectNode(item->data(IdRole).toUInt(), m_pageCue);
                else
                    selectTracked(m_pageCue, item->data(TargetRole).toString());
            });
        }
        // What tracks in, value by value: the details the tree keeps to a
        // count (⤳ 3) and a tooltip.
        m_cueTracked = new QLabel(inner);
        m_cueTracked->setObjectName("stCueTracked");
        m_cueTracked->setWordWrap(true);
        m_cueTracked->setTextInteractionFlags(Qt::TextSelectableByMouse);
        f->addRow(tr("Tracked in"), m_cueTracked);
        m_cueNext = new QLabel(inner);
        m_cueNext->setWordWrap(true);
        f->addRow(tr("Next cue receives"), m_cueNext);
        for (QLineEdit *e : { m_cueNumber, m_cueName })
            connect(e, &QLineEdit::editingFinished, this, &ShowTreeView::slotCueEdited);
        connect(m_cueNote, &NoteEdit::editingFinished, this, &ShowTreeView::slotCueEdited);
        // Text fields don't swallow a group dragged over them: the page takes it.
        for (QWidget *w : inner->findChildren<QWidget *>())
            if (w != m_cueLayers && w->acceptDrops() && w->parentWidget() != m_cueLayers)
                w->setAcceptDrops(false);
        // Q60: a time typed (or stepped) is applied on Return, on focus out
        // and before the page shows anything else (a click on another cue:
        // commitPendingCueEdits()); Esc in the field puts it back.
        for (QDoubleSpinBox *s : { m_cueFadeIn, m_cueFadeOut, m_cueHold })
        {
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) {
                if (!m_inspectorUpdating && m_pageCue != invalidId())
                    m_cueTimesDirty = true;
            });
            connect(s, &QDoubleSpinBox::editingFinished, this, [this]() { commitPendingCueEdits(); });
            s->installEventFilter(this);
        }
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
        // R2: + Palette, by type, in the folder selected below.
        QHBoxLayout *fh = new QHBoxLayout();
        fh->setContentsMargins(0, 0, 0, 0);
        fh->addWidget(m_filter, 1);
        m_newPalette = new QToolButton(lib);
        m_newPalette->setObjectName("stNewPalette");
        m_newPalette->setText(tr("+ Palette"));
        m_newPalette->setToolTip(tr("A new palette, by type, in the folder selected in the Library; it opens in "
                                    "the Look Editor. Effect design, .qxfx and Bundles are in Build."));
        m_newPalette->setPopupMode(QToolButton::InstantPopup);
        {
            QMenu *m = new QMenu(m_newPalette);
            connect(m, &QMenu::aboutToShow, this, [this, m]() {
                m->clear();
                fillNewPaletteMenu(m, libraryPaletteFolder());
            });
            m_newPalette->setMenu(m);
        }
        fh->addWidget(m_newPalette);
        v->addLayout(fh);
        m_library = new LibraryWidget(lib);
        m_library->setObjectName("stLibrary");
        m_library->setHeaderHidden(true);
        m_library->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_library, &QTreeWidget::customContextMenuRequested, this, &ShowTreeView::slotLibraryContextMenu);
        // Return / double-click: what is selected acts on the tree's
        // selection, as a drop on it would -- no dragging across.
        static_cast<LibraryWidget *>(m_library)->onReturn = [this](bool alt) { applyLibrarySelection(alt); };
        static_cast<LibraryWidget *>(m_library)->onRename = [this]() { startLibraryRename(); };
        static_cast<LibraryWidget *>(m_library)->onDelete = [this]() { deleteLibrarySelection(); };
        static_cast<LibraryWidget *>(m_library)->onPaletteDrop = [this](const QList<quint32> &ids, const QString &f) {
            return dropPalettesOnFolder(ids, f);
        };
        connect(m_library, &QTreeWidget::itemChanged, this, &ShowTreeView::slotLibraryItemRenamed);
        connect(m_library->itemDelegate(), &QAbstractItemDelegate::closeEditor, this, [this]() {
            if (!m_libRenaming)
                return;
            m_libRenaming = false;
            m_libRenameItem = nullptr;
            QTimer::singleShot(0, this, [this]() { reloadLibrary(); });
        });
        connect(m_library, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
            const int kind = item->data(0, KindRole).toInt();
            if (kind == LibSection || kind == LibFolder || kind == LibPaletteFolder)
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
        /* Only a Library | Tree | page split, with the page showing. One
           saved while the Workbench was docked here is four panes, the
           hidden page's at 0 -- restored, the page came back at no width.
           Such a state is dropped and the default split used. */
        if (state.isValid() && splitterStateUsable(state.toByteArray(), 3, 2))
            split->restoreState(state.toByteArray());
        else if (state.isValid())
            settings.remove(SETTINGS_SPLITTER);
        connect(split, &QSplitter::splitterMoved, this, [this, split]() {
            // Docked, the panes are not the page's: closeWorkbench() puts the
            // split back as it was, and that is what is kept.
            if (isWorkbenchDocked())
                return;
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
        m_paletteByType = settings.value(QStringLiteral("showtree/libraryPalettesByType"), true).toBool();
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
    // ⇧⌘N: the next thing the selection calls for (showtreenewhere.cpp) --
    // here, the docked Workbench included. The Virtual Console's own ⇧⌘N
    // (animation) is on another tab, never active with this one.
    QShortcut *newHereSc = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    newHereSc->setObjectName(QStringLiteral("stNewHere"));
    newHereSc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newHereSc, &QShortcut::activated, this, [this]() { newHere(); });
    for (const QKeySequence &key : { QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace) })
    {
        QShortcut *sc = new QShortcut(key, m_tree);
        sc->setContext(Qt::WidgetShortcut);
        connect(sc, &QShortcut::activated, this, &ShowTreeView::slotDelete);
    }

    connect(m_st, &ShowTreeController::changed, this, &ShowTreeView::slotTreeChanged);
    // Q38: the badges and "Use show's (On)" follow the show's move in black.
    if (ProgrammerController *pc = m_doc->programmer())
        connect(pc, &ProgrammerController::autoMoveInBlackChanged, this, [this]() {
            buildTree();
            updateInspector();
        });
    connect(m_st, &ShowTreeController::previewChanged, this, &ShowTreeView::slotPreviewChanged);
    if (m_doc->inputOutputMap() != nullptr)
        connect(m_doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged,
                this, &ShowTreeView::slotPreviewChanged);
    // GO (from anywhere) plays what the page shows: a time typed or stepped
    // in the cue's fields and not yet left is committed (and baked) before
    // the run starts, not on the next visit to the cue.
    connect(m_st, &ShowTreeController::stageTakenForRun, this, [this](QStringList *) {
        commitPendingCueEdits();
    });
    // The Library follows the show's groups and palettes -- once per burst
    // (a show loading adds them one by one).
    m_libraryTimer = new QTimer(this);
    m_libraryTimer->setSingleShot(true);
    m_libraryTimer->setInterval(0);
    connect(m_libraryTimer, &QTimer::timeout, this, &ShowTreeView::reloadLibrary);
    // One connect() per signal: a braced list of &Doc::signal connects
    // nothing on Windows (it holds the DLL import thunks' addresses).
    const auto kickTimer = QOverload<>::of(&QTimer::start);
    connect(m_doc, &Doc::fixtureGroupAdded, m_libraryTimer, kickTimer);
    connect(m_doc, &Doc::fixtureGroupRemoved, m_libraryTimer, kickTimer);
    connect(m_doc, &Doc::fixtureGroupChanged, m_libraryTimer, kickTimer);
    connect(m_doc, &Doc::paletteAdded, m_libraryTimer, kickTimer);
    connect(m_doc, &Doc::paletteRemoved, m_libraryTimer, kickTimer);
    /* A fixture's look or output rules changed (or one came or went): the
       Library's fixture and group icons, and the layers' -- the same icon as
       in every other list. Once per burst. */
    QTimer *fixtureTimer = new QTimer(this);
    fixtureTimer->setSingleShot(true);
    fixtureTimer->setInterval(0);
    connect(fixtureTimer, &QTimer::timeout, this, [this]() {
        buildTree();
        reloadLibrary();
    });
    connect(m_doc, &Doc::fixtureAdded, fixtureTimer, kickTimer);
    connect(m_doc, &Doc::fixtureRemoved, fixtureTimer, kickTimer);
    connect(m_doc, &Doc::fixtureChanged, fixtureTimer, kickTimer);
    connect(m_doc, &Doc::loaded, this, &ShowTreeView::reload);
    // Auto fixtures shown as their closest look, or not: the Library's and
    // the layers' icons.
    connect(m_doc, &Doc::stageLooksChanged, this, &ShowTreeView::reload);
    connect(m_doc, &Doc::loaded, this, [this]() {
        m_bridgeDismissed = false;
        updateBridgeBanner();
    });
    // A chaser made or deleted elsewhere (Build, Functions): the header's
    // Upgrade This Show… follows it (Q90), not only the next tree change.
    // (Not while a show loads: Doc::loaded does it once at the end.)
    auto chasersChanged = [this]() {
        if (m_doc->loadStatus() != Doc::Loading)
            updateBridgeBanner();
    };
    connect(m_doc, &Doc::functionAdded, this, chasersChanged);
    connect(m_doc, &Doc::functionRemoved, this, chasersChanged);
    /* An EFX or RGB Matrix made in Build (+ New ▾, the Workbench's
       + Effect ▾), deleted or renamed: the Library's "Functions" at once,
       not on the next visit ("creating an effect in Build doesn't update
       the list in the Show Tree"). Once per burst; a load does it at the
       end. */
    auto functionsChanged = [this]() {
        if (m_doc->loadStatus() != Doc::Loading)
            m_libraryTimer->start();
    };
    connect(m_doc, &Doc::functionAdded, this, functionsChanged);
    connect(m_doc, &Doc::functionRemoved, this, functionsChanged);
    connect(m_doc, &Doc::functionNameChanged, this, functionsChanged);
    // Palettes made, renamed, filed, deleted (here, typed or in the
    // Programming tab) and the shared undo: the Library and the buttons.
    connect(m_doc->paletteOps(), &PaletteOps::changed, this, [this]() {
        m_libraryTimer->start();
        updateButtons();
        // A palette back (or gone): the rows name it.
        if (!m_inspectorUpdating)
            updateInspector();
        if (editedPalette() != QLCPalette::invalidId())
            updateUsedBy(editedPalette());
    });

    reload();
}

ShowTreeView::~ShowTreeView()
{
    // A docked Workbench goes back to Build (or with the tree's own).
    destroyWorkbenchDock();
    // A field with the focus (a cue's name, just made) loses it as the
    // widgets go: its editingFinished must not reach a half-gone view.
    for (QObject *child : findChildren<QObject *>())
        disconnect(child, nullptr, this, nullptr);
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
    // An original Import in place kept: the cue that stands for it.
    if (!m_st->generatedNode(functionId, &listId, &cueId, &layerId)
        && (listId = m_st->originalFor(functionId, &cueId)) == invalidId())
        return false;
    const bool ok = layerId != invalidId() ? selectNode(layerId, cueId)
                    : cueId != invalidId() ? selectNode(cueId) : selectNode(listId);
    bringForward();
    return ok;
}

void ShowTreeView::bringForward()
{
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
    QString body;
    if (st->isGenerated(functionId))
    {
        const QString what = st->describeGenerated(functionId);
        body = QStringLiteral("<b>%1</b>").arg(ShowTreeController::lockedText().toHtmlEscaped());
        if (!what.isEmpty())
            body += QStringLiteral("<br>") + tr("It is %1.").arg(what.toHtmlEscaped());
        if (qobject_cast<Chaser *>(doc->function(functionId)) != nullptr)
            body += QStringLiteral("<br>") + tr("Its run order, direction and speed modes are yours to set "
                                                "here; its steps and name are the tree's.");
    }
    else
    {
        // An original Import in place kept (Q141): what plays is the list.
        body = QStringLiteral("<b>%1</b>").arg(ShowTreeController::originalText().toHtmlEscaped());
        const QString where = st->originalWhere(functionId);
        if (!where.isEmpty())
            body += QStringLiteral("<br>") + where.toHtmlEscaped();
    }
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
        if (l == nullptr || l->looks)
            return false;                   // the Looks section keeps its name
        text = l->name;
    }
    else if (kind == LookFolderNode)
    {
        text = item->text(0);
    }
    else if (kind == SectionNode)
    {
        // A song section (Q190): its name, not the key it shows.
        Section section;
        if (!currentSection(&section))
            return false;
        text = section.name;
    }
    else if (kind == CueNode)
    {
        // Its number and name, as the row reads ("2.5 Fog"): a new number
        // moves the cue to its place. A look's number is only the tree's.
        const Cue *c = m_st->cue(id);
        const CueList *l = m_st->listOfCue(id);
        if (c == nullptr || l == nullptr)
            text = QString();
        else if (l->looks)
            text = c->name;
        else
        {
            const QString number = c->number.isEmpty() ? QString::number(l->indexOf(id) + 1) : c->number;
            text = c->name.isEmpty() ? number : QStringLiteral("%1 %2").arg(number, c->name);
        }
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
    // The words selected, not the number: "Chorus" in "1.2.0 Chorus" (or a
    // "01-02.03-Chorus" style name). Clicking into the number edits it too.
    // No number: all of it.
    const QString numbered = text;
    auto selectWords = [this, numbered]() {
        const QList<QLineEdit *> eds = m_tree->viewport()->findChildren<QLineEdit *>();
        QLineEdit *ed = nullptr;
        for (QLineEdit *e : eds)
            if (e->isVisible() || ed == nullptr)
                ed = e;
        if (ed == nullptr || ed->text() != numbered)
            return;
        const int start = wordsStart(numbered);
        if (start > 0 && start < numbered.length())
            ed->setSelection(start, int(numbered.length()) - start);
        else
            ed->selectAll();
    };
    selectWords();
    QTimer::singleShot(0, this, selectWords);
    return true;
}

int ShowTreeView::wordsStart(const QString &name)
{
    // A leading number ("1.2.0 ", "2.5 ", "01-02.03-"): digits and the
    // dots, dashes and spaces between them, then the words.
    static const QRegularExpression re(QStringLiteral("^((?>\\d+(?:[.\\-]\\d+)*)[ .\\-]+)(?=\\S)"));
    const QRegularExpressionMatch m = re.match(name);
    return m.hasMatch() ? int(m.capturedLength(1)) : 0;
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
    if (kind == LookFolderNode)
    {
        const QString path = item->data(0, FolderRole).toString();
        QTimer::singleShot(0, this, [this, id, name, path]() {
            if (!m_st->renameLookFolder(id, path, name))
                buildTree();
            m_tree->setFocus(Qt::OtherFocusReason);
        });
        return;
    }
    if (kind == SectionNode)
    {
        const QString role = item->data(0, SectionRole).toString();
        QTimer::singleShot(0, this, [this, id, name, role]() {
            bool done = false;
            for (const Section &sec : m_st->sections(id))
                if (sectionRole(sec) == role)
                    done = m_st->setSectionName(id, sec, name);
            if (!done)
                buildTree();
            m_tree->setFocus(Qt::OtherFocusReason);
        });
        return;
    }
    if (kind == CueNode)
    {
        const CueList *l = m_st->listOfCue(id);
        QString number;
        QString rest;
        if (l != nullptr && !l->looks && splitNumberedName(name, &number, &rest))
        {
            // "2.5 Fog": number and name in one step; a number taken is
            // refused and said.
            QTimer::singleShot(0, this, [this, id, number, rest]() {
                renameCue(id, number, rest);
            });
            return;
        }
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

bool ShowTreeView::renameCue(quint32 cueId, const QString &number, const QString &name)
{
    const Cue *c = m_st->cue(cueId);
    const CueList *l = m_st->listOfCue(cueId);
    if (c == nullptr || l == nullptr)
        return false;
    Cue cue = *c;
    // An unnumbered cue left at its position's number stays unnumbered.
    if (!(c->number.isEmpty() && number == QString::number(l->indexOf(cueId) + 1)))
        cue.number = number;
    cue.name = name;
    QString why;
    const bool same = cue.number == c->number && cue.name == c->name;
    const bool ok = same || m_st->setCue(cue, &why);
    if (!ok)
    {
        buildTree();                    // as it was
        if (!why.isEmpty())
            QMessageBox::information(this, tr("Cue number"), why);
    }
    selectNode(cueId);
    m_tree->setFocus(Qt::OtherFocusReason);
    return ok;
}

void ShowTreeView::slotTreeChanged()
{
    if (m_reloading)
        return;
    buildTree();
    updateInspector();
    updateButtons();
    updateProductionSwitcher();
    updateBridgeBanner();
    // "In this cue list" counts what the list uses.
    m_libraryTimer->start();
    if (editedPalette() != QLCPalette::invalidId())
        updateUsedBy(editedPalette());

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
    updateProductionSwitcher();
    buildTree();
    reloadLibrary();
    updateInspector();
    updateButtons();
    updateBridgeBanner();
}

void ShowTreeView::saveExpanded()
{
    m_expanded.clear();
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        const int savedKind = (*it)->data(0, KindRole).toInt();
        if (savedKind == LookFolderNode || savedKind == SectionNode)
        {
            // A look folder by its path, a song section by its key: open
            // unless closed by hand.
            const QString key = QStringLiteral("%1:%2").arg((*it)->data(0, IdRole).toUInt())
                                .arg((*it)->data(0, savedKind == SectionNode ? SectionRole : FolderRole)
                                     .toString());
            if ((*it)->isExpanded())
                m_collapsedFolders.remove(key);
            else
                m_collapsedFolders.insert(key);
            continue;
        }
        if ((*it)->isExpanded())
            m_expanded << (*it)->data(0, IdRole).toUInt();
    }
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
        if (kind == TrackedLayerNode || kind == LookFolderNode || kind == SectionNode)
            continue;
        return item;
    }
    return nullptr;
}

/* A skipped cue's grey: between the theme's text and its background, as
   far toward the background as still reads (3:1, WCAG's floor for greyed
   text) -- the Disabled text colour all but vanishes on some themes. */
static QColor readableGrey(const QPalette &pal)
{
    auto lum = [](const QColor &c) {
        auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
    };
    const QColor text = pal.color(QPalette::Active, QPalette::Text);
    const QColor base = pal.color(QPalette::Active, QPalette::Base);
    const double lb = lum(base);
    for (double t = 0.45; t < 1.0; t += 0.05)
    {
        const QColor c = QColor::fromRgbF(text.redF() * t + base.redF() * (1 - t),
                                          text.greenF() * t + base.greenF() * (1 - t),
                                          text.blueF() * t + base.blueF() * (1 - t));
        const double l = lum(c);
        if ((qMax(l, lb) + 0.05) / (qMin(l, lb) + 0.05) >= 3.3)
            return c;
    }
    return text;
}

void ShowTreeView::buildTree()
{
    // A production added or removed shows or hides the switcher (Q75).
    updateProductionSwitcher();
    m_reloading = true;
    const quint32 selId = currentId();
    const quint32 selCue = currentCueId();
    // A carried-in layer is found again by its cue and target (it has no
    // id of its own there); made the cue's own, by its layer there.
    const bool selTracked = currentKind() == TrackedLayerNode;
    const QString selTarget = selTracked ? m_tree->currentItem()->data(0, TargetRole).toString() : QString();
    const bool selFolder = currentKind() == LookFolderNode;
    const QString selFolderPath = selFolder ? m_tree->currentItem()->data(0, FolderRole).toString() : QString();
    const bool selSection = currentKind() == SectionNode;
    const QString selSectionKey = selSection ? m_tree->currentItem()->data(0, SectionRole).toString() : QString();
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
    const QBrush skipGrey(readableGrey(m_tree->palette()));

    for (const Production &prod : m_st->productions())
    {
        QTreeWidgetItem *pi = new QTreeWidgetItem(m_tree);
        pi->setText(0, prod.name);
        pi->setFont(0, boldFont);
        pi->setData(0, KindRole, ProductionNode);
        pi->setData(0, IdRole, prod.id);
        const bool currentProd = m_st->currentProduction() == prod.id;
        const quint32 mainListId = m_st->mainList(prod.id);
        if (currentProd && !singleProduction())
        {
            pi->setText(1, tr("▶ current"));
            pi->setToolTip(0, tr("The current production: GO, the footer and show control play its main list"));
        }

        // The cue lists, then the Looks section (R3).
        QList<const CueList *> lists;
        for (const CueList &list : prod.lists)
            if (!list.looks && !list.isSubCues())
                lists << &list;
        if (const CueList *looks = prod.looks())
            lists << looks;
        for (const CueList *lp : lists)
        {
            const CueList &list = *lp;
            QTreeWidgetItem *li = new QTreeWidgetItem(pi);
            li->setText(0, list.name);
            li->setText(1, list.mode == ListMode::Tracking ? tr("Tracking") : tr("Non-tracking"));
            li->setData(0, KindRole, ListNode);
            li->setData(0, IdRole, list.id);
            if (!list.colour.isEmpty())
                li->setForeground(0, treeTextColour(list.colour));
            // One production (Q75): "main" only says something with two lists.
            if (!list.looks && list.id == mainListId
                && (singleProduction() ? m_st->playableLists(prod.id).count() > 1
                                       : (currentProd || m_st->playableLists(prod.id).count() > 1)))
            {
                li->setText(1, li->text(1) + tr(" · main"));
                li->setToolTip(1, currentProd ? tr("GO plays this list (the current production's main list)")
                                              : tr("GO plays this list when the production is current"));
            }
            // Q38: a list whose move in black isn't the show's.
            if (!list.looks && list.moveInBlack != MoveInBlack::Show
                && m_st->listMovesInBlack(list.id, m_st->showMoveInBlack()) != m_st->showMoveInBlack())
            {
                const bool on = list.moveInBlack == MoveInBlack::On;
                li->setText(1, li->text(1) + (on ? tr("  ▸ MIB") : tr("  ⊘ MIB")));
                const QString tip = on ? tr("Move in black On for this list, although the show's is off: dark "
                                            "heads are pre-positioned for its next cue.")
                                       : tr("Move in black Off for this list, although the show's is on: no head "
                                            "is pre-positioned for its cues.");
                li->setToolTip(1, QStringList({ li->toolTip(1), tip }).join(QStringLiteral("\n\n")).trimmed());
            }
            const QList<ResolvedCue> resolved = m_st->resolved(list);

            // A Looks section: its folders (nested, first) and the VC
            // buttons firing each look.
            QHash<QString, QTreeWidgetItem *> folderItems;
            QHash<quint32, int> firedBy;
            std::function<QTreeWidgetItem *(const QString &)> folderItem = [&](const QString &path) {
                if (path.isEmpty())
                    return li;
                if (QTreeWidgetItem *have = folderItems.value(path))
                    return have;
                const int slash = path.lastIndexOf(QLatin1Char('/'));
                QTreeWidgetItem *f = new QTreeWidgetItem(folderItem(slash < 0 ? QString() : path.left(slash)));
                f->setText(0, slash < 0 ? path : path.mid(slash + 1));
                f->setIcon(0, QIcon(":/folder.png"));
                f->setData(0, KindRole, LookFolderNode);
                f->setData(0, IdRole, list.id);
                f->setData(0, FolderRole, path);
                f->setToolTip(0, tr("A folder of hits: drag hits onto it; Make VC page makes a button for each"));
                folderItems.insert(path, f);
                return f;
            };
            if (list.looks)
            {
                li->setText(0, tr("Hits"));
                li->setText(1, tr("busking"));
                li->setData(0, FolderRole, QStringLiteral("/"));
                li->setIcon(0, QIcon(":/collection.png"));
                li->setToolTip(0, tr("Hits, for busking: built with layers like cues, never tracking; each plays from a "
                                     "VC button (Make VC page), over whatever runs."));
                for (const QString &path : m_st->lookFolders(list.id))
                    folderItem(path);
                QSet<quint32> fids;
                for (const Cue &c : list.cues)
                    fids.insert(c.collection);
                for (const VCButton *b : UsedBy::vcButtons(fids))
                    firedBy[b->function()]++;
            }

            // One cue's row, its layers, what else is on stage in it -- and
            // (P6) its sub-cues under it, each a cue row of its own.
            std::function<void(const CueList &, int, const QList<ResolvedCue> &, QTreeWidgetItem *)> addCue;
            addCue = [&](const CueList &list, int i, const QList<ResolvedCue> &resolved,
                         QTreeWidgetItem *parentItem) {
                const Cue &cue = list.cues.at(i);
                QTreeWidgetItem *ci = new QTreeWidgetItem(parentItem != nullptr ? parentItem
                                                          : list.looks ? folderItem(cue.folder) : li);
                ci->setText(0, m_st->cueLabel(list, i));
                /* Its flags, a column each (Q104): a dot where it has one,
                   counts for what it sets, tracks and releases; the rest
                   under Also. */
                const QString dot = QStringLiteral("\u25cf");
                QStringList badges;     // Also
                if (list.baseCue == cue.id)
                    badges << tr("base");
                const CueMode mode = effectiveMode(list, cue);
                if (cue.mode != CueMode::ListDefault || list.mode == ListMode::Tracking)
                {
                    if (mode == CueMode::Block && list.mode == ListMode::Tracking)
                        ci->setText(ModeCol, tr("B"));
                    else if (mode == CueMode::CueOnly)
                        ci->setText(ModeCol, tr("C"));
                }
                if (!cue.includes.isEmpty())
                    badges << tr("⊕ %1").arg(cue.includes.count());
                if (cue.reusable != invalidId())
                {
                    // A use of a reusable cue (P4): ⟲, and how many there are.
                    ci->setText(0, QStringLiteral("⟲ ") + ci->text(0));
                    badges << tr("reusable ×%1").arg(m_st->reusableUses(cue.reusable).count());
                }
                if (list.looks && firedBy.value(cue.collection) > 0)
                    badges << tr("VC ×%1").arg(firedBy.value(cue.collection));
                // A cue that still mixes (open 13): the one exception to
                // the one rule, kept only where the rule would change it.
                QString badgeTip;
                if (cue.mixOverlap)
                {
                    badges << tr("mix");
                    badgeTip = (tr("Overlapping layers mix here as the imported chaser did (the highest "
                                         "intensity wins, the last started sets the rest) instead of the one "
                                         "rule (the most specific source, then the deeper, then the lower "
                                         "layer wins). Convert to tracking keeps a cue mixing only where the "
                                         "rule would change what it plays; untick Overlapping layers mix on "
                                         "the cue page to put it on the rule anyway."));
                }
                // A focus-call cue (docs/design/FOCUS_REFINE_DESIGN.md section 7).
                if (!list.looks && cue.focusCall)
                {
                    badges << tr("focus");
                    badgeTip = QStringList({ badgeTip, tr("Venue focus cue: GO arms the show's focus calls; "
                                                          "Start on the controller begins them.") })
                               .join(QStringLiteral("\n\n")).trimmed();
                }
                // Q38: a cue that says no move in black where its list
                // would (a list's own setting is badged on the list).
                if (!list.looks && cue.noMoveInBlack)
                {
                    const quint32 owner = list.isSubCues() && m_st->listOfCue(list.subOf) != nullptr
                                          ? m_st->listOfCue(list.subOf)->id : list.id;
                    if (m_st->listMovesInBlack(owner, m_st->showMoveInBlack()))
                    {
                        badges << tr("no MIB");
                        badgeTip = QStringList({ badgeTip, tr("Don't move in black into this cue: no head is "
                                                              "pre-positioned for it, although its list and the "
                                                              "show would.") }).join(QStringLiteral("\n\n")).trimmed();
                    }
                }
                // Tracking at a glance (Branson, 2026-10-21): how many
                // layers it sets (S), releases (R) and how many track in
                // (T); what they
                // are is in the tooltip and on the cue page.
                if (list.mode == ListMode::Tracking && !list.looks && i < resolved.count())
                {
                    int own = 0, released = 0, carried = 0;
                    for (const ResolvedLayer &rl : resolved.at(i).layers)
                        (rl.own ? (rl.release ? released : own) : carried)++;
                    if (own > 0)
                        ci->setText(SetsCol, QString::number(own));
                    if (released > 0)
                        ci->setText(ReleasesCol, QString::number(released));
                    if (carried > 0)
                        ci->setText(TracksCol, QString::number(carried));
                }
                if (!list.looks && cue.skip)
                    badges.prepend(tr("skip"));
                ci->setText(FlagsCol, badges.join(QStringLiteral(", ")));
                const QString trackingTip = list.looks ? QString() : cueTrackingTip(cue.id);
                for (int c = InfoCol; c < TreeColumnCount; ++c)
                {
                    ci->setForeground(c, grey);
                    if (c >= SetsCol && c < FlagsCol)
                        ci->setTextAlignment(c, Qt::AlignCenter);
                    ci->setToolTip(c, QStringList({ trackingTip, badgeTip }).join(QStringLiteral("\n\n")).trimmed());
                }
                const QString nameTip = cue.reusable == invalidId()
                               ? (cue.unlinkedFrom.isEmpty() ? cue.note
                                  : tr("Unlinked from %1. %2").arg(cue.unlinkedFrom, cue.note).trimmed())
                               : tr("A use of the reusable cue %1: an edit here is an edit to every use. %2")
                                 .arg(m_st->reusableName(cue.reusable), cue.note).trimmed();
                ci->setToolTip(0, QStringList({ nameTip, trackingTip }).join(QStringLiteral("\n\n")).trimmed());
                ci->setData(0, KindRole, CueNode);
                ci->setData(0, IdRole, cue.id);
                // Its text colour: its own, else its list's (skipped: grey wins).
                {
                    const QString colour = m_st->cueColour(cue.id);
                    if (!colour.isEmpty())
                        ci->setForeground(0, treeTextColour(colour));
                }

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
                    // What it lights, at a glance: one kind of fixture, or a group.
                    if (const Function *fn = layer.target.isOpaque() ? m_doc->function(layer.target.function) : nullptr)
                        yi->setIcon(0, fn->getIcon());
                    else if (!layer.target.isOpaque())
                    {
                        QList<quint32> groups = layer.target.groups, fixtures = layer.target.fixtures;
                        for (quint32 set : layer.target.sets)
                            if (const GroupSet *gs = m_st->groupSet(set))
                            {
                                groups += gs->groups;
                                fixtures += gs->fixtures;
                            }
                        yi->setIcon(0, ItemIcons::targetsIcon(m_doc, groups, fixtures));
                    }
                    QStringList looks;
                    for (const Look &l : layer.looks)
                        looks << lookText(l);
                    // Sets or Releases (Q104): a dot in its column.
                    if (layer.release)
                        yi->setText(ReleasesCol, dot);
                    else if (list.mode == ListMode::Tracking && !list.looks)
                        yi->setText(SetsCol, dot);
                    if (layer.reusable != invalidId())
                        yi->setText(FlagsCol, tr("×%1").arg(m_st->reusableUses(layer.reusable).count()));
                    for (int c = SetsCol; c < FlagsCol; ++c)
                        yi->setTextAlignment(c, Qt::AlignCenter);
                    QString tip = QStringLiteral("%1\n%2").arg(m_st->targetContents(layer.target),
                                                               looks.join(QStringLiteral(", "))).trimmed();
                    if (layer.reusable != invalidId())
                        tip = tr("Reusable layer: an edit here is an edit to every use (%1).\n")
                              .arg(m_st->reusableUses(layer.reusable).count()) + tip;
                    else if (!layer.unlinkedFrom.isEmpty())
                        tip = tr("Unlinked from %1.\n").arg(layer.unlinkedFrom) + tip;
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
                        // From a cue of this list, or (P6) a sub-cue, or a
                        // sub-cue's own cue: by its number only (⤳ tracked,
                        // ⊕ included, ↳ a sub-cue, ■ the base); the rest,
                        // value by value, in the tooltip.
                        const CueList *fromList = m_st->listOfCue(rl.fromCue);
                        const int from = fromList == nullptr ? -1 : fromList->indexOf(rl.fromCue);
                        const QString num = from < 0 ? QString() : m_st->cueNumber(*fromList, from);
                        ti->setText(1, rl.origin == Origin::Included ? tr("⊕ %1").arg(num)
                                       : rl.origin == Origin::Base ? tr("■ base")
                                       : fromList != nullptr && fromList->isSubCues()
                                         ? tr("↳ %1").arg(num)
                                       : num);
                        const QString tip = carriedDetail(rl) + QStringLiteral("\n")
                                            + tr("Select it and set a value to make it this cue's own.");
                        ti->setToolTip(0, tip);
                        ti->setToolTip(1, tip);
                        ti->setText(TracksCol, dot);
                        ti->setTextAlignment(TracksCol, Qt::AlignCenter);
                        for (int c = NameCol; c < TreeColumnCount; ++c)
                            ti->setForeground(c, grey);
                        ti->setData(0, KindRole, TrackedLayerNode);
                        ti->setData(0, IdRole, rl.layerId);
                        ti->setData(0, CueRole, cue.id);
                        ti->setData(0, TargetRole, rl.target.signature());
                    }
                }

                // Its sub-cues (§3C): 4.1, 4.2... with Sub GO / auto badges.
                if (!list.isSubCues() && !list.looks)
                    if (const CueList *subs = m_st->cueList(m_st->subCueList(cue.id)))
                    {
                        const QList<ResolvedCue> subResolved = m_st->resolved(*subs);
                        for (int k = 0; k < subs->cues.count(); ++k)
                            addCue(*subs, k, subResolved, ci);
                    }
                if (list.isSubCues())
                {
                    // ↳ .1 Hit · Sub GO / auto 0.5s, fade 1.5s.
                    QStringList sb;
                    if (i == 0)
                        sb << tr("with GO");
                    else if (cue.follow)
                        sb << tr("auto %1s").arg(cue.wait / 1000.0);
                    else
                        sb << tr("Sub GO");
                    if (cue.fadeIn > 0)
                        sb << tr("fade %1s").arg(cue.fadeIn / 1000.0);
                    ci->setText(0, QStringLiteral("↳ ") + ci->text(0));
                    ci->setText(1, sb.join(QStringLiteral(" · ")));
                    ci->setToolTip(0, tr("Sub-cue: it starts from what is on stage before it; its changes are "
                                         "cue-only -- the next main cue starts from %1's own state. %2")
                                      .arg(m_st->cueLabel(*m_st->listOfCue(list.subOf),
                                                          m_st->listOfCue(list.subOf)->indexOf(list.subOf)),
                                           cue.note).trimmed());
                }
                // Skip (a cue of a list, or a sub-cue): the tick, unticked
                // when skipped -- then greyed and struck through, "skip" in
                // the F column, and everything under it greyed (its layers, what
                // tracks in, its sub-cues: none of it plays from GO).
                if (!list.looks)
                {
                    ci->setFlags(ci->flags() | Qt::ItemIsUserCheckable);
                    ci->setCheckState(0, cue.skip ? Qt::Unchecked : Qt::Checked);
                }
                if (!list.looks && cue.skip)
                {
                    const QString why = tr("Skipped: GO passes over it. It tracks as if it weren't in the list; "
                                           "a jump straight to it still plays it. Tick it (or ⌘/) to put it back.");
                    for (int c = 0; c < TreeColumnCount; ++c)
                    {
                        ci->setToolTip(c, (why + QStringLiteral("\n\n") + ci->toolTip(c)).trimmed());
                        if (c == 0)
                        {
                            // The name struck through; the flags plain.
                            QFont f = ci->font(c);
                            f.setStrikeOut(true);
                            ci->setFont(c, f);
                        }
                        ci->setForeground(c, skipGrey);
                    }
                    QList<QTreeWidgetItem *> todo;
                    for (int k = 0; k < ci->childCount(); ++k)
                        todo << ci->child(k);
                    while (!todo.isEmpty())
                    {
                        QTreeWidgetItem *under = todo.takeFirst();
                        for (int c = 0; c < TreeColumnCount; ++c)
                            under->setForeground(c, skipGrey);
                        for (int k = 0; k < under->childCount(); ++k)
                            todo << under->child(k);
                    }
                }
            };
            // Q190: a list's song sections, each a row its cues fold under.
            const QList<Section> sections = list.looks ? QList<Section>() : m_st->sections(list.id);
            QTreeWidgetItem *sectionItem = nullptr;
            for (int i = 0; i < list.cues.count(); ++i)
            {
                const int si = sectionOfIndex(sections, i);
                if (si >= 0 && sections.at(si).first == i)
                {
                    const Section &sec = sections.at(si);
                    sectionItem = new QTreeWidgetItem(li);
                    sectionItem->setText(0, sectionTitle(sec));
                    sectionItem->setFont(0, boldFont);
                    sectionItem->setFirstColumnSpanned(true);
                    sectionItem->setData(0, KindRole, SectionNode);
                    sectionItem->setData(0, IdRole, list.id);
                    sectionItem->setData(0, SectionRole, sectionRole(sec));
                    sectionItem->setToolTip(0, tr("A song section: its cues fold under it. Select it to name it "
                                                  "or start it with Block; + Cue adds a cue at its end."));
                }
                else if (si < 0)
                    sectionItem = nullptr;
                addCue(list, i, resolved, sectionItem);
            }
        }
    }

    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
    {
        QTreeWidgetItem *item = *it;
        const int kind = item->data(0, KindRole).toInt();
        if (kind == LookFolderNode || kind == SectionNode)
            item->setExpanded(!m_collapsedFolders.contains(QStringLiteral("%1:%2")
                                                           .arg(item->data(0, IdRole).toUInt())
                                                           .arg(item->data(0, kind == SectionNode ? SectionRole
                                                                                                  : FolderRole)
                                                                .toString())));
        else if (first)
            item->setExpanded(kind == ProductionNode || kind == ListNode);
        else
            item->setExpanded(m_expanded.contains(item->data(0, IdRole).toUInt())
                              && kind != TrackedLayerNode);
    }

    if (selSection)
    {
        // A song section by its key (Q190); gone (renumbered away): its list.
        QTreeWidgetItem *found = findItem(selId, invalidId());
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, KindRole).toInt() == SectionNode && (*it)->data(0, IdRole).toUInt() == selId
                && (*it)->data(0, SectionRole).toString() == selSectionKey)
            {
                found = *it;
                break;
            }
        if (found != nullptr)
            m_tree->setCurrentItem(found, 0, kSelectOnly);
    }
    else if (selFolder)
    {
        for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
            if ((*it)->data(0, KindRole).toInt() == LookFolderNode && (*it)->data(0, IdRole).toUInt() == selId
                && (*it)->data(0, FolderRole).toString() == selFolderPath)
            {
                m_tree->setCurrentItem(*it, 0, kSelectOnly);
                break;
            }
    }
    else if (selTracked)
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
    // Another cue selected is a pick: the latest action wins (a hold by a
    // GO lets go). The same one again -- a rebuild, a page put back -- is
    // not.
    const quint32 picked = currentCueId();
    const bool isPick = isVisible() && picked != invalidId() && picked != m_lastPickedCue;
    m_lastPickedCue = picked;
    if (isPick && !isWorkbenchDocked())
        m_st->pickCue(picked);
    syncPreview();
    if (currentListId() != m_libraryList)
        m_libraryTimer->start();
    workbenchFollowTree();
    if (isPick && isWorkbenchDocked())
        if (Workbench *bench = dockedWorkbench())
            bench->playPicked();
}

void ShowTreeView::pickCurrentCue()
{
    const quint32 cueId = currentCueId();
    if (cueId == invalidId() || m_doc->mode() != Doc::Design)
        return;
    m_lastPickedCue = cueId;
    if (Workbench *bench = isWorkbenchDocked() ? dockedWorkbench() : nullptr)
        bench->playPicked();
    else
        m_st->pickCue(cueId);
}

quint32 ShowTreeView::currentListId() const
{
    if (currentKind() == ListNode || currentKind() == LookFolderNode || currentKind() == SectionNode)
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
    // W3: docked when the tab was left: docked again.
    if (m_wb != nullptr)
        QTimer::singleShot(0, this, [this]() {
            if (isVisible())
                resumeWorkbench();
        });
}

void ShowTreeView::hideEvent(QHideEvent *ev)
{
    // Don't leave a cue on the rig when the tab is left (unless the Doc,
    // and the preview with it, is already gone).
    if (!m_st.isNull())
        m_st->stopPreview();
    // W3: Build gets its Workbench back while the tab is away.
    suspendWorkbench();
    QWidget::hideEvent(ev);
}

void ShowTreeView::changeEvent(QEvent *ev)
{
    QWidget::changeEvent(ev);
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange)
    {
        styleCueDropZone();
        // The tree's greys (skipped cues, carried-in layers) are the
        // theme's: taken again, once the change is through.
        QTimer::singleShot(0, this, [this]() {
            if (m_st != nullptr && m_tree != nullptr && !m_renaming && !m_reloading)
                buildTree();
        });
    }
}

void ShowTreeView::styleCueDropZone()
{
    if (m_cueDropZone == nullptr)
        return;
    if (m_cueDropActive)
    {
        m_cueDropZone->setStyleSheet(QStringLiteral(
            "QLabel { border: 2px solid palette(highlight); border-radius: 6px;"
            " padding: 12px; color: palette(highlight); }"));
        return;
    }
    // Two thirds of the way from the background to the text: quieter than
    // a label, never close to the background. palette(mid) is a shade of
    // the button colour -- on the dark themes about the window's own.
    const QPalette pal = palette();
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor bg = pal.color(QPalette::Window);
    const auto mix = [](int a, int b) { return (a * 2 + b) / 3; };
    const QColor quiet(mix(fg.red(), bg.red()), mix(fg.green(), bg.green()), mix(fg.blue(), bg.blue()));
    m_cueDropZone->setStyleSheet(QStringLiteral(
        "QLabel { border: 2px dashed %1; border-radius: 6px; padding: 12px; color: %1; }")
        .arg(quiet.name()));
}

void ShowTreeView::syncPreview()
{
    // A cue, or a layer in one, plays that cue; the controller decides
    // whether it may (Design mode, its list not running for real).
    // W3: docked, the Workbench plays the cue (in context, or Solo).
    if (isWorkbenchDocked())
    {
        slotPreviewChanged();
        return;
    }
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
    QString cue = list == nullptr ? QString() : m_st->cueLabel(*list, list->indexOf(cueId));
    // A skipped cue plays when picked, as a jump to it does (GO passes over it).
    if (m_st->isCueSkipped(cueId))
        cue = tr("%1 (skipped: GO passes over it; played as picked)").arg(cue);
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
        case ShowTreeController::PreviewState::Held:
        {
            // RC8: GO took the stage; selecting stays off it until Play.
            const QStringList running = m_st->listsRunning();
            if (running.isEmpty())
                return tr("■ Held — GO took the stage. Play to preview %1.").arg(cue);
            return tr("■ Held — %1 running. Play to preview %2 over it.").arg(running.join(", "), cue);
        }
        case ShowTreeController::PreviewState::Off:
        break;
    }
    return QString();
}

void ShowTreeView::slotPreviewChanged()
{
    m_previewLabel->setText(previewText());
    // Blind is blue (docs/design/LIVE_BLIND.md): an edit that doesn't reach
    // the stage -- Operate, or Design with the rig muted. Live: the theme.
    const bool blind = m_st->previewState() == ShowTreeController::PreviewState::NotDesign
                    || (m_doc->inputOutputMap() != nullptr && m_doc->inputOutputMap()->outputInhibited()
                        && !m_previewLabel->text().isEmpty());
    m_previewLabel->setProperty("blind", blind);
    m_previewLabel->setStyleSheet(blind ? BlindStyle::labelSheet() : QString());
    // Play, beside the line, while a run holds the preview -- the docked
    // Workbench has its own.
    if (m_previewPlayAction != nullptr)
        m_previewPlayAction->setVisible(m_st->previewState() == ShowTreeController::PreviewState::Held
                                    && !isWorkbenchDocked());
    m_previewLabel->setToolTip(m_st->previewState() == ShowTreeController::PreviewState::Playing
                               ? tr("Design mode: the selected cue plays on the rig, and changes show at "
                                    "once. GO holds it so the show plays alone; Play previews over a "
                                    "running list (it keeps going underneath). Leaving the tab or "
                                    "selecting no cue stops it.")
                               : QString());
}

void ShowTreeView::updateButtons()
{
    const NodeKind kind = currentKind();
    m_newList->setEnabled(kind != NoNode);
    // In a Looks section, + Cue makes a look (R3).
    const bool looks = selectionInLooks();
    m_newCue->setText(looks ? tr("+ Hit") : tr("+ Cue"));
    m_newCue->setToolTip(looks ? tr("A new hit after the selected one, in its folder")
                               : tr("A new cue after the selected one, numbered for you (never renumbers)"));
    m_newCue->setEnabled(kind == ListNode || kind == CueNode || kind == LayerNode
                         || kind == TrackedLayerNode || kind == LookFolderNode);
    m_addLayer->setEnabled(currentCueId() != invalidId());
    m_newSubCue->setEnabled(currentCueId() != invalidId() && !looks);
    m_newProductionAction->setVisible(m_st->productions().isEmpty());
    m_record->setEnabled(currentCueId() != invalidId() && !looks);
    m_delete->setEnabled(kind != NoNode && kind != TrackedLayerNode);
    m_merge->setEnabled(selectedLayers().count() > 1);
    // One history: the tree's steps and the palette steps (P-D14).
    m_undo->setEnabled(m_st->canUndoShared());
    m_redo->setEnabled(m_st->canRedoShared());
    m_undo->setToolTip(m_st->canUndoShared() ? tr("Undo %1 (⌘Z)").arg(m_st->sharedUndoText()) : tr("Nothing to undo"));
    m_redo->setToolTip(m_st->canRedoShared() ? tr("Redo %1 (⇧⌘Z)").arg(m_st->sharedRedoText())
                                             : tr("Nothing to redo"));
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

bool ShowTreeView::commitPendingCueEdits()
{
    if (m_committingTimes || m_inspectorUpdating || m_pageCue == invalidId())
        return false;
    // What is typed and not yet read as a value counts too.
    for (QDoubleSpinBox *s : { m_cueFadeIn, m_cueFadeOut, m_cueHold, m_cueWait })
        if (s != nullptr && s->hasFocus())
            s->interpretText();
    if (!m_cueTimesDirty && !m_cueWaitDirty)
        return false;
    const Cue *c = m_st->cue(m_pageCue);
    if (c == nullptr)
    {
        m_cueTimesDirty = m_cueWaitDirty = false;
        return false;
    }
    m_committingTimes = true;
    bool applied = false;
    const quint32 id = m_pageCue;
    const bool times = m_cueTimesDirty, wait = m_cueWaitDirty;
    m_cueTimesDirty = m_cueWaitDirty = false;
    if (times)
    {
        Cue cue = *c;
        cue.fadeIn = uint(qRound(m_cueFadeIn->value() * 1000));
        cue.fadeOut = uint(qRound(m_cueFadeOut->value() * 1000));
        cue.hold = m_cueHold->value() < 0 ? Function::infiniteSpeed() : uint(qRound(m_cueHold->value() * 1000));
        if (cue.fadeIn != c->fadeIn || cue.fadeOut != c->fadeOut || cue.hold != c->hold)
            applied = m_st->setCue(cue);
    }
    if (wait)
        if (const Cue *sc = m_st->cue(id))
        {
            const uint ms = uint(qRound(m_cueWait->value() * 1000));
            const bool follow = m_cueAdvance->currentData().toInt() == 1;
            if (sc->wait != ms || sc->follow != follow)
                applied |= m_st->setSubCueFollow(id, follow, ms);
        }
    m_committingTimes = false;
    return applied;
}

void ShowTreeView::revertCueTimes()
{
    m_cueTimesDirty = m_cueWaitDirty = false;
    const Cue *c = m_st->cue(m_pageCue);
    if (c == nullptr)
        return;
    const bool was = m_inspectorUpdating;
    m_inspectorUpdating = true;
    m_cueFadeIn->setValue(c->fadeIn / 1000.0);
    m_cueFadeOut->setValue(c->fadeOut / 1000.0);
    m_cueHold->setValue(int(c->hold) < 0 ? -1 : c->hold / 1000.0);
    if (m_cueWait != nullptr)
        m_cueWait->setValue(c->wait / 1000.0);
    m_inspectorUpdating = was;
}

void ShowTreeView::updateInspector()
{
    // Q60: a time typed on the page goes to the cue it was typed for, not
    // lost to the page showing another.
    commitPendingCueEdits();
    m_inspectorUpdating = true;
    m_pageCue = invalidId();
    m_multi.clear();
    m_layerRelease->setTristate(false);
    const NodeKind kind = currentKind();
    const quint32 id = currentId();

    if (kind == LookFolderNode || (kind == ListNode && m_st->isLooks(id)))
    {
        showLooksPage(id, currentLookFolder());
        m_inspectorUpdating = false;
        return;
    }
    if (kind == ProductionNode && m_st->production(id) != nullptr)
    {
        showProductionPage(id);
        m_inspectorUpdating = false;
        return;
    }
    Section section;
    if (kind == SectionNode && currentSection(&section))
    {
        showSectionPage(id, section);
        m_inspectorUpdating = false;
        return;
    }
    if (kind == ListNode)
    {
        const CueList *list = m_st->cueList(id);
        if (list != nullptr)
        {
            for (QWidget *w : { static_cast<QWidget *>(m_listMode), static_cast<QWidget *>(m_listBase) })
                m_listForm->setRowVisible(w, true);
            m_listConvert->show();
            m_listDetach->show();
            m_listVcPage->hide();
            m_listTitle->setText(list->name);
            m_listForm->setRowVisible(m_listColour, true);
            showColourButton(m_listColour, list->colour, QString());
            m_listForm->setRowVisible(m_listMib, true);
            if (m_listAddSection != nullptr)
                m_listForm->setRowVisible(m_listAddSection, true);
            m_listMib->setItemText(0, showMibLabel());
            m_listMib->setCurrentIndex(qMax(0, m_listMib->findData(int(list->moveInBlack))));
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
            // W5: where it came from.
            m_listRestore->setVisible(list->inPlace);
            if (const Function *src = m_doc->function(list->source))
            {
                QString from = list->inPlace
                        ? tr("Imported in place from the chaser “%1”: it and what only it played are filed under "
                             "%2 (editing them there doesn't change the list). Restore original puts them back.")
                          .arg(src->name(), ShowTreeController::originalsFolder(src->name()))
                        : tr("Imported from the chaser “%1”, which is still where it was.").arg(src->name());
                m_listInfo->setText(m_listInfo->text() + QStringLiteral("\n\n") + from);
            }
            m_pages->setCurrentWidget(m_listPage);
            m_inspectorUpdating = false;
            return;
        }
    }

    const quint32 cueId = currentCueId();
    const CueList *list = cueId == invalidId() ? nullptr : m_st->listOfCue(cueId);
    if (list == nullptr)
    {
        // An empty show: only the way in, not "select" with nothing to select.
        m_emptyPage->setText(m_st->productions().isEmpty()
                             ? tr("Start with + Production, or turn the chasers you already have "
                                  "into cue lists (Upgrade This Show…).")
                             : tr("Select a cue or a layer."));
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

QList<QPair<QString, qint32>> ShowTreeView::timecodeChoices() const
{
    QList<QPair<QString, qint32>> out;
    out << qMakePair(tr("The show's setting"), -2) << qMakePair(tr("Any source"), -1);
    InputOutputMap *iom = m_doc->inputOutputMap();
    for (quint32 i = 0; iom != nullptr && i < iom->universesCount(); ++i)
        out << qMakePair(tr("Universe %1: %2").arg(i + 1).arg(iom->getUniverseNameByIndex(int(i))),
                         qint32(iom->getUniverseID(int(i))));
    return out;
}

void ShowTreeView::showProductionPage(quint32 productionId)
{
    const Production *p = m_st->production(productionId);
    const bool current = m_st->currentProduction() == productionId;
    m_prodTitle->setText(p->name);
    m_prodCurrent->setText(current ? tr("▶ Current — click to unset") : tr("Make current"));
    // One production (Q75): it plays on GO; nothing to make current, and a
    // main list only to pick with two lists.
    const bool single = singleProduction();
    m_prodCurrent->setVisible(!single);
    if (QFormLayout *form = qobject_cast<QFormLayout *>(m_prodPage->layout()))
        if (QWidget *l = form->labelForField(m_prodCurrent))
            l->setVisible(!single);
    {
        QSignalBlocker block(m_prodMainList);
        m_prodMainList->clear();
        for (quint32 lid : m_st->playableLists(productionId))
            m_prodMainList->addItem(m_st->cueList(lid)->name, lid);
        m_prodMainList->setCurrentIndex(qMax(0, m_prodMainList->findData(m_st->mainList(productionId))));
        m_prodMainList->setEnabled(m_prodMainList->count() > 0);
        const bool pick = !single || m_prodMainList->count() > 1;
        m_prodMainList->setVisible(pick);
        if (QFormLayout *form = qobject_cast<QFormLayout *>(m_prodPage->layout()))
            if (QWidget *l = form->labelForField(m_prodMainList))
                l->setVisible(pick);
    }
    {
        QSignalBlocker block(m_prodTimecode);
        m_prodTimecode->clear();
        for (const auto &c : timecodeChoices())
            m_prodTimecode->addItem(c.first, c.second);
        int at = m_prodTimecode->findData(p->timecode);
        if (at < 0)
        {
            m_prodTimecode->addItem(tr("Universe %1 (not in this show)").arg(p->timecode + 1), p->timecode);
            at = m_prodTimecode->count() - 1;
        }
        m_prodTimecode->setCurrentIndex(at);
    }
    updateNumberingRows(productionId);
    const quint32 looks = m_st->looksSection(productionId);
    m_prodInfo->setText(tr("%n cue list(s)", "", int(m_st->playableLists(productionId).count()))
                        + (looks == invalidId() ? QString()
                           : tr(", %n hit(s)", "", int(m_st->looksIn(looks, QString()).count())))
                        + QStringLiteral(".\n\n")
                        + (single ? tr("GO, the footer, show control and the Cue Runner play its main list. "
                                       "With a second production you choose which one plays.")
                           : current ? tr("Current: GO, the footer, show control and every Cue Runner following "
                                        "the current production play its main list.")
                                   : tr("Not current: its lists play only when picked (a VC Cue List, a Cue "
                                        "Runner set to one of them) or when it is made current. Undo is one "
                                        "history for every production.")));
    m_pages->setCurrentWidget(m_prodPage);
}

bool ShowTreeView::singleProduction() const
{
    return m_st->productions().count() == 1;
}

void ShowTreeView::updateProductionSwitcher()
{
    // One production (Q75): nothing to switch between.
    const bool show = m_st->productions().count() > 1;
    if (m_productionAction != nullptr)
        m_productionAction->setVisible(show);
    if (m_productionLabelAction != nullptr)
        m_productionLabelAction->setVisible(show);
    QSignalBlocker block(m_production);
    m_production->clear();
    m_production->addItem(tr("None (the main cue list)"), invalidId());
    for (const Production &p : m_st->productions())
        m_production->addItem(p.name, p.id);
    m_production->setCurrentIndex(qMax(0, m_production->findData(m_st->currentProduction())));
}

void ShowTreeView::showCuePage(const CueList &list, int index)
{
    const Cue &cue = list.cues.at(index);
    m_pageCue = cue.id;
    // A look (R3): no number, mode, base, includes, timing or next cue --
    // what it has is all it plays, from a VC button.
    {
        const bool cueRows = !list.looks;
        for (QWidget *w : { static_cast<QWidget *>(m_cueNumber), static_cast<QWidget *>(m_cueMode),
                            static_cast<QWidget *>(m_cueBase), m_cueIncludes->parentWidget(),
                            static_cast<QWidget *>(m_cueFadeIn), static_cast<QWidget *>(m_cueFadeOut),
                            static_cast<QWidget *>(m_cueHold), static_cast<QWidget *>(m_cueNext),
                            static_cast<QWidget *>(m_cueTracked), static_cast<QWidget *>(m_cueSkip),
                            static_cast<QWidget *>(m_cueNoMib), static_cast<QWidget *>(m_cueFocusCall) })
            m_cueForm->setRowVisible(w, cueRows);
        m_cueForm->setRowVisible(m_lookFiredBy, list.looks);
        // A sub-cue (P6): its advance, not a mode, base, includes or hold
        // (it holds until Sub GO or auto-follows); a cue: its sub-cues.
        const bool sub = list.isSubCues();
        for (QWidget *w : { static_cast<QWidget *>(m_cueMode), static_cast<QWidget *>(m_cueBase),
                            m_cueIncludes->parentWidget(), static_cast<QWidget *>(m_cueHold) })
            if (sub)
                m_cueForm->setRowVisible(w, false);
        m_cueForm->setRowVisible(m_cueAdvanceRow, sub);
        m_cueForm->setRowVisible(m_cueSubCues, !list.looks && !sub);
        if (list.looks)
        {
            const QStringList fired = lookFiredBy(cue.id);
            m_lookFiredBy->setText((fired.isEmpty() ? tr("no VC button yet") : fired.join(QStringLiteral(", ")).toHtmlEscaped())
                                   + QStringLiteral(" · <a href=\"vcpage\">%1</a>").arg(tr("Make VC page"))
                                   + QStringLiteral(" · <a href=\"folder\">%1</a>").arg(tr("Move to folder…")));
            m_lookFiredBy->setToolTip(tr("A hit plays from a VC button like any function: Toggle or Flash. "
                                         "Over a running cue list it mixes as any function does -- the "
                                         "highest intensity wins, the last started sets the rest."));
        }
    }
    m_cueNumber->setText(cue.number);
    m_cueNumber->setPlaceholderText(QString::number(index + 1));
    m_cueNumber->setToolTip(list.isSubCues() ? tr("Its number under its cue: %1").arg(m_st->cueNumber(list, index))
                                             : QString());
    if (list.isSubCues())
    {
        m_cueAdvance->setCurrentIndex(cue.follow ? 1 : 0);
        m_cueWait->setValue(cue.wait / 1000.0);
        m_cueWait->setEnabled(cue.follow);
        m_cueAdvance->setEnabled(index > 0);
        m_cueAdvance->setToolTip(index == 0 ? tr("The first sub-cue plays with GO.") : m_cueAdvance->toolTip());
    }
    else if (!list.looks)
    {
        QStringList subs;
        if (const CueList *sl = m_st->cueList(m_st->subCueList(cue.id)))
            for (int k = 0; k < sl->cues.count(); ++k)
                subs << QStringLiteral("<a href=\"%1\">%2</a>").arg(sl->cues.at(k).id)
                        .arg(m_st->cueLabel(*sl, k).toHtmlEscaped());
        m_cueSubCues->setText((subs.isEmpty() ? tr("none") : subs.join(QStringLiteral(" · ")))
                              + QStringLiteral(" · <a href=\"new\">%1</a>").arg(tr("+ Sub-cue")));
        m_cueSubCues->setToolTip(subs.isEmpty() ? QString()
                                 : tr("GO plays the first; Sub GO the next. Cue-only: the next cue starts from "
                                      "this cue's own state, however far Sub GO got."));
    }
    m_cueName->setText(cue.name);
    m_cueNote->setText(cue.note);
    showColourButton(m_cueColour, cue.colour, cue.colour.isEmpty() ? m_st->cueColour(cue.id) : QString());
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
    {
        QSignalBlocker block(m_cueSkip);
        m_cueSkip->setChecked(cue.skip);
    }
    {
        QSignalBlocker block(m_cueNoMib);
        m_cueNoMib->setChecked(cue.noMoveInBlack);
    }
    {
        QSignalBlocker block(m_cueFocusCall);
        m_cueFocusCall->setChecked(cue.focusCall);
        // No calls to run: greyed with why -- but one ticked can always be unticked.
        FocusMenu::setAvailable(m_cueFocusCall, cue.focusCall ? QString() : FocusMenu::venueFocusCueBlocked(m_doc),
                                venueFocusCueTip());
    }
    FocusMenu::setAvailable(m_cueRefine, FocusMenu::focusCueBlocked(m_doc), FocusMenu::focusCueTip(m_doc));
    refreshFocusCallState();
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
    {
        QString tracked = cueTrackedText();
        if (m_st->lastBakeChanged().contains(cue.id))
            tracked = (tracked.isEmpty() ? QString() : tracked + QStringLiteral("\n"))
                      + tr("Changed by the last edit (re-baked).");
        m_cueTracked->setText(tracked.isEmpty() ? tr("— (nothing: every layer is set here)") : tracked);
        if (!list.looks)
            m_cueForm->setRowVisible(m_cueTracked, list.mode == ListMode::Tracking || !tracked.isEmpty());
    }
    m_pages->setCurrentWidget(m_cuePage);
}

QString ShowTreeView::nextCueText() const
{
    return nextCueText(m_pageCue);
}

QString ShowTreeView::nextCueText(quint32 cueId) const
{
    const CueList *list = cueId == invalidId() ? nullptr : m_st->listOfCue(cueId);
    if (list == nullptr)
        return QString();
    const int index = list->indexOf(cueId);
    // The next cue GO plays: skipped ones are passed over.
    const int nextAt = playedAfter(*list, index);
    if (nextAt < 0)
        return index + 1 >= list->cues.count() ? tr("— (the last cue)")
                                               : tr("— (every cue after it is skipped)");
    const Cue &next = list->cues.at(nextAt);
    const QString nextLabel = m_st->cueLabel(*list, nextAt);
    const QString via = nextAt == index + 1 ? QString() : tr("(%1, past the skipped) ").arg(nextLabel);
    if (effectiveMode(*list, next) == CueMode::Block)
        return tr("nothing: %1%2 is a Block cue, it starts from nothing")
               .arg(nextLabel, via.isEmpty() ? QString() : tr(" (past the skipped)"));
    if (list->cues.at(index).skip)
    {
        // Skipped: it passes on nothing of its own -- the next cue starts
        // from the cue played before it.
        const int before = playedBefore(*list, index);
        if (before < 0)
            return tr("%1nothing of this cue (skipped): it starts as the first cue played").arg(via);
        return tr("%1nothing of this cue (skipped): what %2 passes on").arg(via, m_st->cueLabel(*list, before));
    }

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
    return via + text;
}

QString ShowTreeView::carriedOrigin(Origin origin, quint32 fromCue) const
{
    const CueList *list = m_st->listOfCue(fromCue);
    const int from = list == nullptr ? -1 : list->indexOf(fromCue);
    const QString cue = from < 0 ? QString() : m_st->cueLabel(*list, from);
    switch (origin)
    {
        case Origin::Own: return QString();
        case Origin::CueLook: return tr("from cue %1").arg(cue);
        case Origin::Tracked:
            return list != nullptr && list->isSubCues() ? tr("from sub-cue %1").arg(cue)
                                                        : tr("tracked from %1").arg(cue);
        case Origin::Included: return tr("included from %1").arg(cue);
        case Origin::Base: return tr("base, from %1").arg(cue);
    }
    return QString();
}

QString ShowTreeView::carriedDetail(const ResolvedLayer &rl) const
{
    QString name = rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name;
    if (rl.depth > 0)
        name = tr("%1 (in %2)").arg(name, m_st->targetLabel(rl.parent));
    QStringList lines;
    lines << tr("⤳ %1 — %2").arg(name, carriedOrigin(rl.origin, rl.fromCue));
    for (auto it = rl.values.cbegin(); it != rl.values.cend(); ++it)
    {
        const ResolvedValue &v = it.value();
        QString line = QStringLiteral("    %1: %2").arg(keyLabel(it.key()),
                                                      v.look.none ? tr("none") : lookText(v.look));
        if (v.fromCue != rl.fromCue || v.origin != rl.origin)
            line += QStringLiteral(" — ") + carriedOrigin(v.origin, v.fromCue);
        lines << line;
    }
    return lines.join(QStringLiteral("\n"));
}

QString ShowTreeView::cueTrackedText() const
{
    const CueList *list = m_pageCue == invalidId() ? nullptr : m_st->listOfCue(m_pageCue);
    if (list == nullptr)
        return QString();
    const int index = list->indexOf(m_pageCue);
    const QList<ResolvedCue> resolved = m_st->resolved(*list);
    if (index < 0 || index >= resolved.count())
        return QString();
    QStringList out;
    for (const ResolvedLayer &rl : resolved.at(index).layers)
        if (!rl.own)
            out << carriedDetail(rl);
    return out.join(QStringLiteral("\n"));
}

QString ShowTreeView::cueTrackingTip(quint32 cueId) const
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr || list->looks)
        return QString();
    const int index = list->indexOf(cueId);
    const QList<ResolvedCue> resolved = m_st->resolved(*list);
    if (index < 0 || index >= resolved.count())
        return QString();
    const Cue &cue = list->cues.at(index);
    QStringList out;
    if (effectiveMode(*list, cue) == CueMode::Block && list->mode == ListMode::Tracking)
        out << tr("Mode B (Block): it starts from nothing; nothing tracks in.");
    if (cue.skip)
        out << tr("Skipped: below is what a jump to it plays; nothing of it passes on.");

    // What tracks in, at most a dozen layers (the cue page has them all).
    QStringList carried, sets, releases;
    for (const ResolvedLayer &rl : resolved.at(index).layers)
    {
        const QString name = rl.name.isEmpty() ? m_st->targetLabel(rl.target) : rl.name;
        if (rl.own)
            (rl.release ? releases : sets) << name;
        else
            carried << carriedDetail(rl);
    }
    const int kShown = 12;
    if (!carried.isEmpty())
    {
        out << tr("Tracked in (%1):").arg(carried.count());
        out << carried.mid(0, kShown);
        if (carried.count() > kShown)
            out << tr("    … and %1 more: see the cue page").arg(carried.count() - kShown);
    }
    // Named as the S and R columns are (Q104).
    if (!sets.isEmpty() && list->mode == ListMode::Tracking)
        out << tr("Sets (%1): %2").arg(sets.count()).arg(sets.join(QStringLiteral(", ")));
    if (!releases.isEmpty() && list->mode == ListMode::Tracking)
        out << tr("Releases (%1): %2").arg(releases.count()).arg(releases.join(QStringLiteral(", ")));
    if (!list->isSubCues())
    {
        // One layer a line: a tooltip doesn't wrap.
        const QString next = nextCueText(cueId);
        if (!next.isEmpty())
            out << tr("Next cue receives:") + QStringLiteral("\n    ")
                   + next.split(QStringLiteral("; ")).join(QStringLiteral("\n    "));
    }
    if (m_st->lastBakeChanged().contains(cueId))
        out << tr("Changed by the last edit (re-baked).");
    return out.join(QStringLiteral("\n"));
}

void ShowTreeView::fitTreeColumns()
{
    if (m_tree == nullptr)
        return;
    int room = m_tree->viewport()->width();
    m_fittingColumns = true;
    /* The flag columns: as wide as their headers -- but the names and the
       info column keep 220 px, so in a narrow pane they shrink (their
       headers end in "…") or fold away; a cue's tooltip still says it all. */
    const QFontMetrics fm = m_tree->header()->fontMetrics();
    QHash<int, int> want;
    int total = 0;
    for (int c = SetsCol; c < TreeColumnCount; ++c)
        if (m_hiddenColumns.contains(c))
            m_tree->header()->hideSection(c);
        else
        {
            const int w = (c == FlagsCol) ? 80 : qMax(24, fm.horizontalAdvance(QStringLiteral("99")) + 12);
            want.insert(c, w);
            total += w;
        }
    const int avail = qMax(0, room - 220);          // the names and info keep 220 px
    const double scale = (total > avail && total > 0) ? double(avail) / total : 1.0;
    for (auto it = want.constBegin(); it != want.constEnd(); ++it)
    {
        const int w = int(it.value() * scale);
        if (w < m_tree->header()->minimumSectionSize())
        {
            m_tree->header()->hideSection(it.key());   // too narrow to read: folded away
            continue;
        }
        m_tree->header()->showSection(it.key());
        m_tree->header()->resizeSection(it.key(), w);
        room -= w;
    }
    // The names keep at least 60% of what is left, however wide InfoCol was set.
    const int badges = qBound(24, m_badgeWidth, qMax(24, room * 2 / 5));
    m_tree->header()->resizeSection(NameCol, qMax(40, room - badges));
    m_tree->header()->resizeSection(InfoCol, badges);
    m_fittingColumns = false;
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
    m_layerRelease->setVisible(!rl->target.isOpaque() && !list.looks);
    m_layerReleaseTarget = own == nullptr ? rl->target : Target();
    {
        QSignalBlocker block(m_layerRelease);
        m_layerRelease->setChecked(own != nullptr && own->release);
    }

    if (rl->target.isOpaque())
    {
        // One block at the top: what it plays, and (W0) Open, which edits
        // it in its own editor -- what changes there is what plays.
        QWidget *box = new QWidget(m_rowsHost);
        QVBoxLayout *v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        QLabel *l = new QLabel(tr("Plays the function \"%1\" as it is.").arg(m_st->targetLabel(rl->target)), box);
        l->setWordWrap(true);
        v->addWidget(l);
        m_rowText << l->text();
        const quint32 fid = rl->target.function;
        if (const Function *f = m_doc->function(fid))
        {
            QPushButton *open = new QPushButton(tr("Open “%1”").arg(f->name()), box);
            open->setObjectName(QStringLiteral("stLayerOpen"));
            open->setToolTip(tr("Open it in the Workbench, in its own editor (↩ comes back). Double-clicking the layer does "
                                "the same."));
            connect(open, &QPushButton::clicked, this, [this, fid]() { openFunction(fid); });
            const Scene *raw = qobject_cast<const Scene *>(f);
            if (raw != nullptr && !raw->values().isEmpty())
            {
                // W5: a raw scene, as layers of palettes on request.
                QHBoxLayout *buttons = new QHBoxLayout();
                buttons->setContentsMargins(0, 0, 0, 0);
                buttons->addWidget(open);
                const ShowTreeController::CapturePlan plan = m_st->planCapture(fid);
                QPushButton *cap = new QPushButton(tr("Capture into palettes…"), box);
                cap->setObjectName(QStringLiteral("stLayerCapture"));
                cap->setEnabled(plan.ok);
                cap->setToolTip(plan.ok ? tr("Its values become layers of groups / fixtures and palettes "
                                             "(reused where the show has them), checked channel for channel in "
                                             "every cue it plays in. The scene is kept. One undo step.")
                                        : plan.why);
                connect(cap, &QPushButton::clicked, this, [this, fid]() { captureIntoPalettes(fid); });
                buttons->addWidget(cap);
                buttons->addStretch(1);
                v->addLayout(buttons);
            }
            else
            {
                v->addWidget(open, 0, Qt::AlignLeft);
            }
            const QStringList also = alsoIn(fid, cue.id);
            QString what = tr("Edited there, it changes what this cue plays.");
            if (!also.isEmpty())
                what += QStringLiteral(" ") + tr("Also in: %1 — they change too.").arg(also.join(QStringLiteral(", ")));
            QLabel *note = new QLabel(what, box);
            note->setObjectName(QStringLiteral("stLayerOpenNote"));
            note->setWordWrap(true);
            v->addWidget(note);
            m_rowText << note->text();
        }
        m_rows->addWidget(box, 0, 0, 1, 5, Qt::AlignTop);
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
                // An effect made here, for here: said so, beside Make reusable.
                if (isEffectKey(key))
                {
                    value->setTextFormat(Qt::RichText);
                    value->setText(value->text().toHtmlEscaped()
                                   + QStringLiteral(" · <i>%1</i>").arg(tr("one-off")));
                }
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
        more->setObjectName(QStringLiteral("stMore:%1").arg(key));
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
            // The plan view (D19): from above, the beams now and, as rings,
            // where the cue before put the same lights.
            const Scene *prevSc = nullptr;
            const QLCPalette *prevPal = nullptr;
            if (index > 0)
            {
                prevSc = qobject_cast<const Scene *>(
                    m_doc->function(m_st->layerScene(list.cues.at(index - 1).id, rl->target)));
                if (prevSc != nullptr)
                    for (quint32 pid : prevSc->palettes())
                        if (const QLCPalette *pp = m_doc->palette(pid))
                            if (pp->type() == QLCPalette::Aim)
                                prevPal = pp;
            }
            LinePlanView *planView = new LinePlanView(m_doc, m_rowsHost);
            planView->setObjectName("stLinePlan");
            planView->setFormation(sc, aimPal, prevSc, prevPal);
            m_rows->addWidget(planView, row, 1, 1, 4);
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

QWidget *ShowTreeView::linePlan() const
{
    // The newest: the page's old rows go later (deleteLater).
    const QList<QWidget *> all = m_rowsHost == nullptr ? QList<QWidget *>()
                                 : m_rowsHost->findChildren<QWidget *>("stLinePlan");
    return all.isEmpty() ? nullptr : all.last();
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

    if (isEffectKey(key) && !rowTarget.isEmpty())
    {
        // In detail, on just this layer's heads (W4).
        // On the Effect row: a new one, beside the effects there (a movement
        // and an intensity effect together); on an effect's row: that one.
        menu.addAction(key == QLatin1String("Effect.") ? tr("Design a new effect…") : tr("Design effect…"),
                       this, [this, cueId, layerId, rowTarget, key]() {
            EffectLayerDialog::design(m_doc, this, cueId, layerId, rowTarget, key);
        })->setObjectName("stDesignEffect");
        // How that effect runs: Loop | Once | N times, Hold | Release.
        if (key != QLatin1String("Effect."))
            if (const CueList *list = m_st->listOfCue(cueId))
            {
                const QList<ResolvedCue> r = m_st->resolved(*list);
                const int at = list->indexOf(cueId);
                const ResolvedLayer *rl = at >= 0 && at < r.size() ? r.at(at).layer(rowTarget) : nullptr;
                if (rl != nullptr && rl->values.contains(key))
                    EffectRunsMenu::add(&menu, m_doc, rl->values.value(key).look.palettes, this);
            }
        menu.addSeparator();
    }
    if (key == KeyDimmer)
    {
        menu.addAction(tr("Level…"), this, [this, cueId, layerId, target]() {
            QWidget *box = new QWidget;
            QHBoxLayout *h = new QHBoxLayout(box);
            h->setContentsMargins(0, 0, 0, 0);
            QSlider *slider = new QSlider(Qt::Horizontal, box);
            slider->setObjectName("stPickerLevelSlider");
            slider->setRange(0, 100);
            QSpinBox *spin = new QSpinBox(box);
            spin->setObjectName("stPickerLevel");
            spin->setRange(0, 100);
            spin->setSuffix(QStringLiteral("%"));
            connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
            spin->setValue(100);
            h->addWidget(slider, 1);
            h->addWidget(spin);
            openDockedPicker(tr("Dimmer level"), box, spin, [this, cueId, layerId, target, spin]() {
                const int pc = spin->value();
                const quint32 pid = valuePalette(cueId, layerId, target, QLCPalette::Dimmer,
                                                 { qBound(0, qRound(pc * 255.0 / 100.0), 255) },
                                                 tr("%1%").arg(pc));
                if (pid != QLCPalette::invalidId())
                    applyLook(cueId, layerId, target, m_st->lookForPalette(pid));
            });
        });
    }
    else if (key == KeyColour)
    {
        menu.addAction(tr("Colour…"), this, [this, cueId, layerId, target]() {
            QColorDialog *picker = new QColorDialog(Qt::white);
            picker->setObjectName("stPickerColour");
            picker->setWindowFlags(Qt::Widget);
            picker->setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
            openDockedPicker(tr("Colour"), picker, picker, [this, cueId, layerId, target, picker]() {
                const QColor c = picker->currentColor();
                if (!c.isValid())
                    return;
                const quint32 pid = valuePalette(cueId, layerId, target, QLCPalette::Color,
                                                 { QLCPalette::colorToString(c, QColor(Qt::black)) },
                                                 tr("%1-ish").arg(c.name()));
                if (pid != QLCPalette::invalidId())
                    applyLook(cueId, layerId, target, m_st->lookForPalette(pid));
            });
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
    // A new number moves the cue to its place in the list; one taken is
    // refused, and the field shows the number it has.
    const quint32 id = m_pageCue;
    const bool renumber = cue.number != c->number;
    QString why;
    if (!m_st->setCue(cue, &why))
    {
        if (!why.isEmpty())
        {
            QSignalBlocker block(m_cueNumber);
            m_cueNumber->setText(c->number);
            QMessageBox::information(this, tr("Cue number"), why);
        }
        return;
    }
    if (renumber)
        selectNode(id);
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

/* A small square of @a colour for the colour menus and buttons. */
static QIcon colourSwatch(const QColor &colour)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(0, 0, 0, 110));
    p.setBrush(colour);
    p.drawRoundedRect(QRectF(0.5, 0.5, 13, 13), 2.5, 2.5);
    return QIcon(pm);
}

QColor ShowTreeView::treeTextColour(const QString &hex) const
{
    if (hex.isEmpty())
        return QColor();
    // On the rows and the alternate rows alike.
    const QPalette pal = m_tree->palette();
    return readableOn(readableOn(QColor(hex), pal.color(QPalette::Base)), pal.color(QPalette::AlternateBase));
}

void ShowTreeView::colourMenu(QMenu *menu, const QList<quint32> &cueIds, quint32 listId)
{
    const bool forList = listId != invalidId();
    if (!forList && cueIds.isEmpty())
        return;
    // What they have now: one colour, or (mixed) none ticked.
    QString current;
    bool same = true;
    if (forList)
        current = m_st->listColour(listId);
    for (int i = 0; !forList && i < cueIds.count(); ++i)
    {
        const Cue *c = m_st->cue(cueIds.at(i));
        const QString have = c == nullptr ? QString() : c->colour;
        if (i == 0)
            current = have;
        else if (have != current)
            same = false;
    }
    auto apply = [this, cueIds, listId, forList](const QString &hex) {
        if (forList ? m_st->setListColour(listId, hex) : m_st->setCuesColour(cueIds, hex))
            m_hint->setText(hex.isEmpty() ? (forList ? tr("Text colour: none (the theme's)")
                                                     : tr("Text colour: none (the list's)"))
                                          : tr("Text colour: %1").arg(textColourName(hex)));
    };
    bool known = current.isEmpty();
    for (const TextColour &tc : textColours())
    {
        QAction *a = menu->addAction(colourSwatch(QColor(tc.hex)), tc.name);
        a->setObjectName(QStringLiteral("stColour%1").arg(tc.name));
        a->setCheckable(true);
        a->setChecked(same && current == tc.hex);
        known = known || current == tc.hex;
        const QString hex = tc.hex;
        connect(a, &QAction::triggered, this, [apply, hex]() { apply(hex); });
    }
    menu->addSeparator();
    QAction *custom = menu->addAction(same && !known ? colourSwatch(QColor(current)) : QIcon(), tr("Custom…"));
    custom->setObjectName("stColourCustom");
    custom->setCheckable(true);
    custom->setChecked(same && !known);
    connect(custom, &QAction::triggered, this, [this, apply, current]() {
        const QColor c = QColorDialog::getColor(current.isEmpty() ? QColor(Qt::white) : QColor(current), this,
                                                tr("Text colour"));
        if (c.isValid())
            apply(c.name());
    });
    QString noneText = tr("None");
    if (!forList)
    {
        const CueList *l = m_st->listOfCue(cueIds.first());
        const QString inherited = l == nullptr ? QString() : m_st->listColour(l->id);
        noneText = inherited.isEmpty() ? tr("None") : tr("None (the list's: %1)").arg(textColourName(inherited));
    }
    QAction *none = menu->addAction(noneText);
    none->setObjectName("stColourNone");
    none->setCheckable(true);
    none->setChecked(same && current.isEmpty());
    connect(none, &QAction::triggered, this, [apply]() { apply(QString()); });
}

void ShowTreeView::showColourButton(QToolButton *button, const QString &own, const QString &inherited)
{
    if (button == nullptr)
        return;
    if (!own.isEmpty())
    {
        button->setIcon(colourSwatch(QColor(own)));
        button->setText(textColourName(own));
    }
    else if (!inherited.isEmpty())
    {
        button->setIcon(colourSwatch(QColor(inherited)));
        button->setText(tr("None (the list's: %1)").arg(textColourName(inherited)));
    }
    else
    {
        button->setIcon(QIcon());
        button->setText(tr("None"));
    }
}

void ShowTreeView::slotCueSkipToggled(bool on)
{
    if (m_inspectorUpdating || m_pageCue == invalidId())
        return;
    setCuesSkipped({ m_pageCue }, on);
}

void ShowTreeView::fillCueFocusMenu(QMenu *m)
{
    m->clear();
    const quint32 cueId = m_pageCue;
    const ShowTree::Cue *cue = cueId != invalidId() ? m_st->cue(cueId) : nullptr;
    const ShowTree::CueList *list = cue != nullptr ? m_st->listOfCue(cueId) : nullptr;
    QAction *self = m->addAction(tr("Focus this cue\342\200\246"), this, [this, cueId]() {
        if (cueId != invalidId())
            RefinePanel::open(m_doc, window(), cueId);
    });
    self->setObjectName("stCueFocusThis");
    FocusMenu::setAvailable(self, cue == nullptr ? tr("Choose a cue first.") : FocusMenu::focusCueBlocked(m_doc),
                            FocusMenu::focusCueTip(m_doc));
    // A look list's looks are fired from the VC, never GO'd onto: no venue focus cue.
    if (list != nullptr && !list->looks)
    {
        QAction *make = m->addAction(tr("Make this a venue focus cue"));
        make->setObjectName("stCueFocusVenueCue");
        make->setCheckable(true);
        make->setChecked(cue->focusCall);
        FocusMenu::setAvailable(make, cue->focusCall ? QString() : FocusMenu::venueFocusCueBlocked(m_doc),
                                venueFocusCueTip());
        connect(make, &QAction::toggled, this, [this, cueId](bool on) {
            m_st->setCuesFocusCall({ cueId }, on);
        });
    }
    m->addSeparator();
    QAction *venue = m->addAction(tr("Venue focus\342\200\246"), this, [this]() {
        FocusCallsDialog::startVenueFocus(m_doc, window());
    });
    venue->setObjectName("stCueVenueFocus");
    FocusMenu::setAvailable(venue, FocusMenu::venueFocusBlocked(m_doc),
                            tr("Run the show's focus calls now, for the venue applied"));
}

void ShowTreeView::refreshFocusCallState()
{
    if (m_cueFocusState == nullptr)
        return;
    FocusCallCue *fcc = m_doc->programmer()->focusCallCue();
    const bool mine = fcc != nullptr && m_pageCue != invalidId() && fcc->cueId() == m_pageCue;
    if (mine && fcc->state() == FocusCallCue::Armed)
        m_cueFocusState->setText(tr("Venue focus: waiting for the controller (Start, or \u2318\u21a9)"));
    else if (mine && fcc->state() == FocusCallCue::NamingVenue)
        m_cueFocusState->setText(tr("Venue focus: name the venue on the laptop"));
    else if (mine && fcc->state() == FocusCallCue::Running)
        m_cueFocusState->setText(m_doc->programmer()->padText(tr("Venue focus running: {Menu} ends it and goes on")));
    m_cueFocusState->setVisible(mine && fcc->state() != FocusCallCue::Idle);
}

void ShowTreeView::slotCueNoMibToggled(bool on)
{
    if (m_inspectorUpdating || m_pageCue == invalidId())
        return;
    m_st->setCuesNoMoveInBlack({ m_pageCue }, on);
}

void ShowTreeView::slotListMibChanged(int index)
{
    if (m_inspectorUpdating || currentKind() != ListNode || index < 0)
        return;
    m_st->setListMoveInBlack(currentId(), MoveInBlack(m_listMib->itemData(index).toInt()));
}

QString ShowTreeView::showMibLabel() const
{
    return m_st->showMoveInBlack() ? tr("Use show's (On)") : tr("Use show's (Off)");
}

void ShowTreeView::listMibMenu(QMenu *menu, quint32 listId)
{
    const MoveInBlack now = m_st->listMoveInBlack(listId);
    const QList<QPair<MoveInBlack, QString>> items = {
        { MoveInBlack::Show, showMibLabel() }, { MoveInBlack::On, tr("On") }, { MoveInBlack::Off, tr("Off") } };
    QActionGroup *group = new QActionGroup(menu);
    for (const auto &it : items)
    {
        const MoveInBlack m = it.first;
        QAction *a = menu->addAction(it.second, this, [this, listId, m]() { m_st->setListMoveInBlack(listId, m); });
        a->setCheckable(true);
        a->setChecked(now == m);
        a->setActionGroup(group);
        a->setObjectName(m == MoveInBlack::On ? "stListMibOn" : m == MoveInBlack::Off ? "stListMibOff"
                                                                                    : "stListMibShow");
    }
}

QList<quint32> ShowTreeView::selectedCues() const
{
    QList<quint32> out;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
        if ((*it)->isSelected() && (*it)->data(0, KindRole).toInt() == CueNode)
        {
            const quint32 id = (*it)->data(0, IdRole).toUInt();
            const CueList *l = m_st->listOfCue(id);
            if (l != nullptr && !l->looks)
                out << id;
        }
    return out;
}

bool ShowTreeView::setCuesSkipped(const QList<quint32> &cueIds, bool skip)
{
    if (cueIds.isEmpty() || !m_st->setCuesSkipped(cueIds, skip))
        return false;
    m_hint->setText(skip ? tr("Skipped %1: GO passes over it (⌘/ or the tick puts it back; a jump still plays it)")
                           .arg(cueIds.count() == 1 ? tr("1 cue") : tr("%1 cues").arg(cueIds.count()))
                         : tr("Unskipped %1: GO plays it again")
                           .arg(cueIds.count() == 1 ? tr("1 cue") : tr("%1 cues").arg(cueIds.count())));
    return true;
}

bool ShowTreeView::toggleSkipSelected()
{
    const QList<quint32> cues = selectedCues();
    if (cues.isEmpty())
        return false;
    bool all = true;
    for (quint32 id : cues)
        all = all && m_st->isCueSkipped(id);
    return setCuesSkipped(cues, !all);
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
                                        .arg(perform ? tr("Operate") : tr("Design")), &dlg);
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
    if (selectionInLooks())
    {
        newLook();
        return;
    }
    quint32 listId = invalidId();
    int after = -1;
    Section section;
    if (currentKind() == ListNode)
    {
        listId = currentId();
        if (const CueList *l = m_st->cueList(listId))
            after = int(l->cues.count()) - 1;
    }
    else if (currentKind() == SectionNode && currentSection(&section))
    {
        // Q190: at the section's end, numbered to the scheme.
        listId = currentId();
        after = section.last;
    }
    else if (currentCueId() != invalidId())
    {
        // A sub-cue selected: the next main cue, after its cue (+ Sub-cue
        // makes another sub-cue).
        quint32 cueId = currentCueId();
        m_st->isSubCue(cueId, &cueId);
        const CueList *l = m_st->listOfCue(cueId);
        if (l == nullptr)
            return;
        listId = l->id;
        after = l->indexOf(cueId);
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

QList<quint32> ShowTreeView::selectedLibraryGroups() const
{
    QList<quint32> groups;
    for (QTreeWidgetItem *item : m_library->selectedItems())
        if (item->data(0, KindRole).toInt() == LibGroup && !groups.contains(item->data(0, IdRole).toUInt()))
            groups << item->data(0, IdRole).toUInt();
    return groups;
}

int ShowTreeView::selectLibraryGroups(const QList<quint32> &groupIds)
{
    QTreeWidgetItem *first = nullptr;
    QSet<quint32> found;
    m_library->clearSelection();
    for (QTreeWidgetItemIterator it(m_library); *it; ++it)
    {
        QTreeWidgetItem *row = *it;
        if (row->data(0, KindRole).toInt() != LibGroup || !groupIds.contains(row->data(0, IdRole).toUInt()))
            continue;
        row->setSelected(true);
        found.insert(row->data(0, IdRole).toUInt());
        for (QTreeWidgetItem *p = row->parent(); p != nullptr; p = p->parent())
            p->setExpanded(true);
        if (first == nullptr)
            first = row;
    }
    if (first != nullptr)
    {
        m_library->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
        m_library->scrollToItem(first);
    }
    return int(found.count());
}

int ShowTreeView::selectSimilarLibraryGroups()
{
    const QList<quint32> like = selectedLibraryGroups();
    if (like.isEmpty())
        return 0;
    const int n = selectLibraryGroups(FixtureManager::similarGroups(m_doc, like));
    // What to do with them: a group set (Make set), offered next.
    m_hint->setText(n > 1 ? tr("%1 similar groups selected: right-click → Make set… to keep them as a group set")
                                .arg(n)
                          : tr("No other group like it (same folder, same fixtures, as many heads)"));
    return n;
}

int ShowTreeView::moveLibraryGroupsToFolder(const QString &path)
{
    const QList<quint32> groups = selectedLibraryGroups();
    const int moved = FixtureManager::moveGroupsToFolder(m_doc, groups, path);
    // Reloaded now, not on the next turn, so they stay selected where they went.
    m_libraryTimer->stop();
    reloadLibrary();
    selectLibraryGroups(groups);
    return moved;
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
            if (m_st->isLooks(id))
            {
                if (confirm(tr("Delete the Hits section"),
                            tr("Delete the Hits section and its %n hit(s)? VC buttons firing them stop "
                               "doing anything. (Undo brings it back.)", "",
                               int(m_st->looksIn(id, QString()).count()))))
                    m_st->removeCueList(id);
            }
            else if (QMessageBox::question(this, tr("Delete cue list"),
                                      tr("Delete this cue list and its cues? (Undo brings it back.)"))
                == QMessageBox::Yes)
                m_st->removeCueList(id);
        break;
        case LookFolderNode:
            // What is in it moves up; nothing is deleted.
            m_st->removeLookFolder(id, currentLookFolder());
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
    // The tree's steps and the palette steps are one history (P-D14).
    m_st->undoShared();
    updateButtons();
}

void ShowTreeView::slotRedo()
{
    m_st->redoShared();
    updateButtons();
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
    // W3: docked, the Workbench's Look Editor (the page's own is away).
    if (Workbench *bench = dockedWorkbench())
        if (m_doc->palette(paletteId) != nullptr)
        {
            bench->editPalette(paletteId);
            return;
        }
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
    updateUsedBy(paletteId);
    m_lookEditor->show();
}

void ShowTreeView::keyPressEvent(QKeyEvent *ev)
{
    /* Esc that nothing took (a rename, the command line and the filter take
       their own): back out one level. The docked Workbench first -- a
       palette opened over its page, then ↩ Show Tree -- else the Look
       Editor under the page closes. Edits stay: they are made already. */
    if (ev->key() == Qt::Key_Escape && ev->modifiers() == Qt::NoModifier)
    {
        bool done = false;
        if (Workbench *bench = dockedWorkbench())
            done = bench->backOut();
        else if (m_lookEditor->isVisible())
        {
            editPalette(QLCPalette::invalidId());
            m_tree->setFocus(Qt::OtherFocusReason);
            done = true;
        }
        if (done)
        {
            ev->accept();
            return;
        }
    }
    QWidget::keyPressEvent(ev);
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
    /* Another effect picked (script, preset or WLED effect): the running
       engine is made again -- new params alone left the old script running
       until the cue was played again ("when selecting an effect .. sometimes
       it doesn't apply on selection"). */
    bool newEffect = false;
    if (const QLCPalette *p = m_doc->palette(paletteId))
        if (p->type() == QLCPalette::Effect)
        {
            const QString sig = p->scriptPath() + QLatin1Char('|') + p->effectPreset()
                                + QLatin1Char('|') + p->wledEffect();
            auto it = m_effectSignatures.find(paletteId);
            newEffect = it == m_effectSignatures.end() ? full : it.value() != sig;
            m_effectSignatures.insert(paletteId, sig);
        }
    for (Function *f : m_doc->functions())
    {
        Scene *sc = qobject_cast<Scene *>(f);
        if (sc == nullptr || !sc->palettes().contains(paletteId))
            continue;
        if (sc->isRunning() && newEffect && m_doc->effectScriptRunner() != nullptr)
            m_doc->effectScriptRunner()->syncScene(sc->id());
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
    // Onto a cue: its place -- after it moving down, before it moving up.
    return cuesDropHint({ cueId }, onCueId, list->indexOf(cueId) < list->indexOf(onCueId));
}

bool ShowTreeView::dropCue(quint32 cueId, quint32 onCueId, bool include)
{
    if (cueDropHint(cueId, onCueId, include).isEmpty())
        return false;
    if (include)
        return m_st->addInclude(onCueId, cueId);
    const CueList *list = m_st->listOfCue(cueId);
    return dropCues({ cueId }, onCueId, list->indexOf(cueId) < list->indexOf(onCueId));
}

QString ShowTreeView::cuesDropHint(const QList<quint32> &cueIds, quint32 onCueId, bool after, bool *refused) const
{
    if (refused != nullptr)
        *refused = false;
    const CueList *list = m_st->listOfCue(onCueId);
    if (list == nullptr || cueIds.isEmpty() || cueIds.contains(onCueId))
        return QString();
    QString why;
    const QStringList numbers = m_st->moveNumbers(cueIds, onCueId, after, &why);
    if (numbers.isEmpty())
    {
        if (refused != nullptr)
            *refused = !why.isEmpty();
        return why;
    }
    const QString what = cueIds.count() == 1 ? m_st->cueLabel(*list, list->indexOf(cueIds.first()))
                                             : tr("%1 cues").arg(cueIds.count());
    const QString onto = m_st->cueLabel(*list, list->indexOf(onCueId));
    const QString where = after ? tr("Move %1 after %2").arg(what, onto) : tr("Move %1 before %2").arg(what, onto);
    if (list->looks)
        return where;
    return tr("%1, as %2").arg(where, numbers.join(QStringLiteral(", ")));
}

bool ShowTreeView::dropCues(const QList<quint32> &cueIds, quint32 onCueId, bool after, QString *why)
{
    QString err;
    const bool ok = m_st->moveCues(cueIds, onCueId, after, &err);
    if (ok)
    {
        // The moved cues stay selected, the first one current.
        selectNode(cueIds.first());
        for (quint32 id : cueIds)
            for (QTreeWidgetItemIterator it(m_tree); *it; ++it)
                if ((*it)->data(0, KindRole).toInt() == CueNode && (*it)->data(0, IdRole).toUInt() == id)
                    (*it)->setSelected(true);
    }
    else if (!err.isEmpty())
        m_hint->setText(err);
    if (why != nullptr)
        *why = err;
    return ok;
}

bool ShowTreeView::renumberLevel(quint32 cueId, int start, int step, bool ask)
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr || list->looks)
        return false;
    if (ask)
    {
        QDialog dlg(this);
        dlg.setWindowTitle(tr("Renumber this level"));
        QFormLayout *f = new QFormLayout(&dlg);
        QLabel *intro = new QLabel(tr("Numbers the cues at %1's level 1, 2, 3… in list order (or from Start, "
                                      "in steps of Step). A point cue keeps its point under its cue's new "
                                      "number; sub-cues follow their cue.")
                                   .arg(m_st->cueLabel(*list, list->indexOf(cueId))), &dlg);
        intro->setWordWrap(true);
        f->addRow(intro);
        QSpinBox *from = new QSpinBox(&dlg);
        from->setObjectName("stRenumberStart");
        from->setRange(0, 99999);
        from->setValue(start);
        QSpinBox *by = new QSpinBox(&dlg);
        by->setObjectName("stRenumberStep");
        by->setRange(1, 1000);
        by->setValue(step);
        f->addRow(tr("Start"), from);
        f->addRow(tr("Step"), by);
        QLabel *preview = new QLabel(&dlg);
        preview->setWordWrap(true);
        preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
        f->addRow(preview);
        QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        f->addRow(box);
        connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        auto update = [this, cueId, from, by, preview, box]() {
            QString why;
            const QList<ShowTreeController::Renumbered> plan = m_st->renumberLevelPlan(cueId, from->value(),
                                                                                       by->value(), &why);
            QStringList lines;
            for (const ShowTreeController::Renumbered &r : plan.mid(0, 12))
                lines << tr("%1 → %2").arg(r.from, r.to);
            if (plan.count() > 12)
                lines << tr("… and %1 more").arg(plan.count() - 12);
            preview->setText(plan.isEmpty() ? why : lines.join(QStringLiteral("\n")));
            box->button(QDialogButtonBox::Ok)->setEnabled(!plan.isEmpty());
        };
        connect(from, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, update);
        connect(by, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, update);
        update();
        if (dlg.exec() != QDialog::Accepted)
            return false;
        start = from->value();
        step = by->value();

        // QLab, a desk or a VC page elsewhere fires these cues by number:
        // what it knows by the old numbers now plays other cues.
        if (ShowControlListener::oscEnabled() || ShowControlListener::mscEnabled())
        {
            const QList<ShowTreeController::Renumbered> plan = m_st->renumberLevelPlan(cueId, start, step);
            QStringList lines;
            for (const ShowTreeController::Renumbered &r : plan.mid(0, 20))
                lines << tr("%1 → %2").arg(r.from, r.to);
            if (plan.count() > 20)
                lines << tr("… and %1 more").arg(plan.count() - 20);
            if (QMessageBox::question(this, tr("Renumber this level"),
                                      tr("Show control (OSC / MSC) is listening, and fires cues by number. "
                                         "Anything sending these numbers -- QLab, a desk, a MIDI or OSC "
                                         "trigger -- will fire other cues until it is changed too:\n\n%1\n\n"
                                         "Renumber?").arg(lines.join(QStringLiteral("\n"))))
                != QMessageBox::Yes)
                return false;
        }
    }
    QString why;
    const bool ok = m_st->renumberLevel(cueId, start, step, &why);
    if (ok)
        selectNode(cueId);
    else if (ask && !why.isEmpty())
        QMessageBox::information(this, tr("Renumber this level"), why);
    else if (!why.isEmpty())
        m_hint->setText(why);
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

void ShowTreeView::openDockedPicker(const QString &title, QWidget *editor, QWidget *focus,
                                   const std::function<void()> &apply)
{
    closeDockedPicker();
    m_pickerTitle->setText(title);
    editor->setParent(m_picker);
    m_pickerBody->addWidget(editor, 1);
    m_pickerApply = apply;
    m_picker->show();
    editor->show();
    if (focus != nullptr)
        focus->setFocus(Qt::OtherFocusReason);
}

void ShowTreeView::applyDockedPicker()
{
    if (!dockedPickerOpen())
        return;
    const std::function<void()> apply = m_pickerApply;
    closeDockedPicker();
    if (apply)
        apply();
}

void ShowTreeView::closeDockedPicker()
{
    m_pickerApply = nullptr;
    while (QLayoutItem *item = m_pickerBody->takeAt(0))
    {
        if (QWidget *w = item->widget())
        {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    if (m_picker->isVisible() && m_tree != nullptr)
        m_tree->setFocus(Qt::OtherFocusReason);
    m_picker->hide();
}

bool ShowTreeView::dockedPickerOpen() const
{
    return m_picker != nullptr && !m_picker->isHidden();
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
    // + Production where it applies: on a production, or on empty space (Q93).
    if (item == nullptr || item->data(0, KindRole).toInt() == ProductionNode)
        menu.addAction(tr("+ Production…"), this, &ShowTreeView::slotNewProduction);
    if (item != nullptr)
    {
        // A right-click in a selection keeps it (Merge works on it).
        if (item->isSelected())
            m_tree->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
        else
            m_tree->setCurrentItem(item, 0, kSelectOnly);
        const int kind = item->data(0, KindRole).toInt();
        const bool inLooks = selectionInLooks();
        menu.addAction(tr("+ Cue list…"), this, &ShowTreeView::slotNewCueList);
        // R3: busking looks, beside the cue lists.
        menu.addAction(tr("+ Hit"), this, [this]() { newLook(); })->setObjectName("stNewLook");
        if (kind != ProductionNode && !inLooks)
            menu.addAction(tr("+ Cue"), this, &ShowTreeView::slotNewCue);
        if ((kind == CueNode || kind == LayerNode || kind == TrackedLayerNode) && !inLooks)
            menu.addAction(tr("+ Sub-cue"), this, [this]() { newSubCue(); })->setObjectName("stNewSubCueMenu");
        if (kind == CueNode || kind == LayerNode || kind == TrackedLayerNode)
            menu.addAction(tr("Open in Workbench"), this, [this]() { openSelectionInWorkbench(); })
                ->setObjectName("stOpenWorkbenchMenu");
        if (inLooks && kind != CueNode && kind != LayerNode)
        {
            const quint32 listId = currentListId();
            const QString folder = currentLookFolder();
            menu.addAction(tr("New folder…"), this, [this, listId, folder]() {
                bool ok = false;
                const QString n = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal,
                                                        QString(), &ok);
                if (ok && !n.trimmed().isEmpty())
                    m_st->addLookFolder(listId, folder.isEmpty() ? n.trimmed() : folder + QLatin1Char('/') + n.trimmed());
            })->setObjectName("stNewLookFolder");
            if (kind == LookFolderNode)
            {
                menu.addAction(tr("Rename folder"), this, [this]() { startRename(); });
                menu.addAction(tr("Delete folder (its hits move up)"), this, [this, listId, folder]() {
                    m_st->removeLookFolder(listId, folder);
                });
            }
            menu.addAction(tr("Make VC page"), this, [this, listId, folder]() {
                QString msg;
                makeVcPage(listId, folder, &msg);
                m_hint->setText(msg);
            })->setObjectName("stMakeVcPageMenu");
        }
        if (kind == CueNode && inLooks)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            const CueList *list = m_st->listOfCue(cueId);
            QList<quint32> looks;
            for (QTreeWidgetItem *sel : m_tree->selectedItems())
                if (sel->data(0, KindRole).toInt() == CueNode && m_st->isLook(sel->data(0, IdRole).toUInt()))
                    looks << sel->data(0, IdRole).toUInt();
            if (!looks.contains(cueId))
                looks = { cueId };
            if (list != nullptr)
                lookFolderMenu(menu.addMenu(tr("Move to folder")), list->id, looks);
            const QString folder = m_st->cue(cueId) == nullptr ? QString() : m_st->cue(cueId)->folder;
            menu.addAction(tr("Make VC page"), this, [this, list, folder]() {
                QString msg;
                if (list != nullptr)
                    makeVcPage(list->id, folder, &msg);
                m_hint->setText(msg);
            });
        }
        // Fire it from a MIDI or OSC control (the Control Map): GO on a
        // list, a jump to a cue, a hit. Not on a sub-cue: its cue's GO plays it.
        if (kind == ListNode || kind == CueNode)
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            const CueList *l = kind == ListNode ? m_st->cueList(id) : m_st->listOfCue(id);
            ControlMap::Target target;
            if (l != nullptr && !l->isSubCues() && kind == ListNode && !l->looks)
                target = ControlMap::cueList(id);
            else if (l != nullptr && !l->isSubCues() && kind == CueNode)
                target = l->looks ? ControlMap::hit(id) : ControlMap::cue(id);
            ControlLearnDialog::addTo(&menu, m_doc, target, this);
        }
        if (kind == CueNode && !inLooks)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            menu.addAction(tr("Copy to a hit"), this, [this, cueId]() {
                const quint32 id = m_st->copyCueToLook(cueId);
                if (id != invalidId())
                    selectNode(id);
            })->setObjectName("stCopyToLook");
        }
        if (kind == CueNode && !inLooks)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            const CueList *list = m_st->listOfCue(cueId);
            if (list != nullptr)
            {
                /* Base cue, Mode and Includes are set on the cue's page, not
                   here (Q93): the menu keeps what acts on a selection. */
                // Skip cue / Unskip cue: the selected cues, when this is one.
                {
                    QList<quint32> cues = item->isSelected() ? selectedCues() : QList<quint32>();
                    if (!cues.contains(cueId))
                        cues = { cueId };
                    const bool skipped = list->cue(cueId)->skip;
                    const QString what = cues.count() == 1 ? QString() : tr(" (%1 cues)").arg(cues.count());
                    QAction *sk = menu.addAction((skipped ? tr("Unskip cue") : tr("Skip cue")) + what, this,
                                                 [this, cues, skipped]() { setCuesSkipped(cues, !skipped); });
                    sk->setObjectName("stSkipCue");
                    sk->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
                    sk->setToolTip(skipped ? tr("GO plays it again")
                                           : tr("GO, auto-follow and Back pass over it; a jump still plays it"));
                }
                // Q38: Don't move in black into this cue (the selected cues).
                {
                    QList<quint32> cues = item->isSelected() ? selectedCues() : QList<quint32>();
                    if (!cues.contains(cueId))
                        cues = { cueId };
                    const bool no = list->cue(cueId)->noMoveInBlack;
                    const QString what = cues.count() == 1 ? QString() : tr(" (%1 cues)").arg(cues.count());
                    QAction *nm = menu.addAction(tr("Don't move in black into this cue") + what, this,
                                                 [this, cues, no]() { m_st->setCuesNoMoveInBlack(cues, !no); });
                    nm->setObjectName("stCueNoMib");
                    nm->setCheckable(true);
                    nm->setChecked(no);
                }
                menu.addSeparator();
                menu.addAction(tr("Record as new cue"), this, [this, cueId]() {
                    recordFrom(cueId);
                });
                // Duplicate (⌘D) is further down, for any selection (Q93).
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
            // Colour ▸: the selected cues' text, when this is one.
            const quint32 cueId = item->data(0, IdRole).toUInt();
            QList<quint32> cues = item->isSelected() ? selectedCues() : QList<quint32>();
            if (!cues.contains(cueId))
                cues = { cueId };
            QMenu *colour = menu.addMenu(cues.count() == 1 ? tr("Colour") : tr("Colour (%1 cues)").arg(cues.count()));
            colour->setObjectName("stColourMenu");
            colourMenu(colour, cues);
        }
        if (kind == CueNode)
        {
            const quint32 cueId = item->data(0, IdRole).toUInt();
            const CueList *list = m_st->listOfCue(cueId);
            const int at = list == nullptr ? -1 : list->indexOf(cueId);
            // Up / down: renumbered for the new place, as a drag is.
            QAction *up = menu.addAction(tr("Move up"), this, [this, list, cueId, at]() {
                dropCues({ cueId }, list->cues.at(at - 1).id, false);
            });
            up->setEnabled(at > 0);
            QAction *down = menu.addAction(tr("Move down"), this, [this, list, cueId, at]() {
                dropCues({ cueId }, list->cues.at(at + 1).id, true);
            });
            down->setEnabled(list != nullptr && at + 1 < list->cues.count());
            if (list != nullptr && !list->looks)
                menu.addAction(tr("Renumber this level…"), this, [this, cueId]() { renumberLevel(cueId); })
                    ->setObjectName("stRenumberLevel");
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
            if (const Cue *dc = m_st->cue(cueId))
                if (const Layer *dl = dc->layer(layerId))
                    if (!dl->target.isOpaque())
                    {
                        const Target dt = dl->target;
                        menu.addAction(tr("Design effect…"), this, [this, cueId, layerId, dt]() {
                            EffectLayerDialog::design(m_doc, this, cueId, layerId, dt);
                        })->setObjectName("stDesignEffectLayer");
                    }
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
                /* Focus this cue (docs/design/FOCUS_REFINE_DESIGN.md): this cue's own values
                   from the stage, on the controller, starting on this layer. */
                QAction *refine = menu.addAction(tr("Focus this cue\342\200\246"), this,
                                                 [this, cueId, layerId]() {
                    RefinePanel::open(m_doc, window(), cueId, layerId);
                });
                refine->setObjectName("stRefineLayer");
                FocusMenu::setAvailable(refine, FocusMenu::focusCueBlocked(m_doc),
                                        tr("Move, zoom, focus and recolour this cue from the stage with the "
                                           "controller: the layer as a whole, or one fixture as an exception"));
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
        if (kind == ListNode && !inLooks)
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            QAction *conv = menu.addAction(tr("Convert to tracking…"), this, [this, id]() { convertListToTracking(id); });
            conv->setObjectName("stConvertList");
            conv->setEnabled(mayConvert(m_st->cueList(id)));
            QMenu *colour = menu.addMenu(tr("Colour"));
            colour->setObjectName("stColourMenu");
            colourMenu(colour, {}, id);
            QMenu *mib = menu.addMenu(tr("Move in black"));
            mib->setObjectName("stListMibMenu");
            listMibMenu(mib, id);
            menu.addAction(tr("Detach to plain chaser…"), this, [this, id]() { detachListToChaser(id); })
                ->setObjectName("stDetachList");
            if (m_st->canRestoreOriginal(id))
                menu.addAction(tr("Restore original…"), this, [this, id]() { restoreOriginal(id); })
                    ->setObjectName("stRestoreOriginal");
        }
        if (kind == ProductionNode && !singleProduction())
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            const bool cur = m_st->currentProduction() == id;
            menu.addAction(cur ? tr("Unset current production") : tr("Make current production"), this,
                           [this, id, cur]() { m_st->setCurrentProduction(cur ? invalidId() : id); })
                ->setObjectName("stMakeCurrent");
        }
        if (kind == ListNode && !inLooks)
        {
            const quint32 id = item->data(0, IdRole).toUInt();
            quint32 pid = invalidId();
            m_st->cueList(id, &pid);
            if (m_st->mainList(pid) != id)
                menu.addAction(tr("Make main list"), this, [this, id]() { m_st->setMainList(id); })
                    ->setObjectName("stMakeMainList");
            if ((m_st->currentProduction() != pid && !singleProduction()) || m_st->mainList(pid) != id)
                menu.addAction(tr("Play on GO (make current, main list)"), this, [this, id, pid]() {
                    m_st->setMainList(id);
                    m_st->setCurrentProduction(pid);
                })->setObjectName("stPlayOnGo");
        }
        if (kind == ProductionNode || (kind == ListNode && !inLooks))
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
    // W0: a layer that plays a function as it is opens it, first.
    {
        const quint32 fid = openableFunction(item);
        if (const Function *f = m_doc->function(fid))
        {
            QAction *open = new QAction(tr("Open “%1”…").arg(f->name()), &menu);
            open->setObjectName(QStringLiteral("stOpenFunction"));
            open->setToolTip(tr("In the Workbench, in its own editor (↩ comes back)"));
            connect(open, &QAction::triggered, this, [this, fid]() { openFunction(fid); });
            QAction *first = menu.actions().value(0);
            menu.insertAction(first, open);
            // W5: a raw scene can become palettes.
            const Scene *raw = qobject_cast<const Scene *>(f);
            if (raw != nullptr && !raw->values().isEmpty())
            {
                const ShowTreeController::CapturePlan plan = m_st->planCapture(fid);
                QAction *cap = new QAction(tr("Capture into palettes…"), &menu);
                cap->setObjectName(QStringLiteral("stCaptureLayer"));
                cap->setEnabled(plan.ok);
                cap->setToolTip(plan.ok ? tr("Its values as layers of palettes, checked channel for channel; the "
                                             "scene is kept")
                                        : plan.why);
                connect(cap, &QAction::triggered, this, [this, fid]() { captureIntoPalettes(fid); });
                menu.insertAction(first, cap);
            }
            menu.insertSeparator(first);
        }
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void ShowTreeView::importChaser(quint32 chaserId, bool showReport)
{
    const ShowTreeController::ImportReport rep = m_st->importChaser(chaserId);
    if (rep.list != invalidId())
        selectNode(rep.list);
    if (!showReport)
        return;
    // The report as a sheet to read (W0), with the offer to fire the list
    // from what fires the chaser.
    ImportSheet sheet(m_doc, rep, chaserId, this);
    if (m_sheetRunner)
        m_sheetRunner(&sheet);
    else
        sheet.exec();
    if (rep.list == invalidId())
        return;
    if (sheet.undoAsked())
    {
        m_st->undo();
        m_hint->setText(tr("Import undone: the show is as it was"));
        return;
    }
    // W5: Import in place -- the originals filed, with the import's step.
    if (sheet.fileOriginals())
        m_st->fileOriginals(rep.list);
    if (sheet.repoint())
        repointToList(chaserId, rep.list);
}

bool ShowTreeView::sendToShowTree(quint32 chaserId, QWidget *parent)
{
    // Build's Send to Show Tree…: the one door (Q76), Upgrade This Show…
    // with just this chaser ticked.
    Q_UNUSED(parent)
    const QString why = m_st->whyNotSend(chaserId);
    if (!why.isEmpty())
    {
        m_hint->setText(why);
        return false;
    }
    const QList<quint32> made = openUpgrade(chaserId);
    if (made.isEmpty())
        return false;
    const CueList *list = m_st->cueList(made.first());
    m_hint->setText(tr("%1 is a Show Tree cue list now").arg(list == nullptr ? QString() : list->name));
    selectNode(made.first());
    bringForward();
    return true;
}

int ShowTreeView::repointToList(quint32 chaserId, quint32 listId)
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr)
        return 0;
    const ImportSheet::Repoint r = ImportSheet::planRepoint(m_doc, chaserId, list->chaser);
    if (r.count() == 0)
        return 0;
    r.apply(m_doc, true);
    // Part of the import's step: Undo puts them back on the chaser.
    Doc *doc = m_doc;
    m_st->attachToLastStep([doc, r](bool undone) { r.apply(doc, !undone); });
    m_hint->setText(tr("%1 now fire(s) the cue list %2").arg(r.lines().join(QStringLiteral(", ")), list->name));
    return r.count();
}

/****************************************************************************
 * W5: the bridge -- the show converter, Import in place, Restore original,
 * Capture into palettes, the open banner
 ****************************************************************************/

QWidget *ShowTreeView::bridgeBanner() const
{
    return m_bridgeBanner;
}

void ShowTreeView::updateBridgeBanner()
{
    /* The header's Upgrade This Show… only while there's a chaser to upgrade
       (Q90): a show made here never needs it. File ▸ Upgrade This Show…
       stays. */
    /* Deferred: this runs as a function is added, and a cue list's own
       chaser is only marked the tree's (generated) once that's done. */
    QTimer::singleShot(0, this, [this]() {
        if (m_importAction != nullptr)
            m_importAction->setVisible(!m_st->convertibleChasers().isEmpty());
    });
    if (m_bridgeBanner == nullptr)
        return;
    // Only a show with no tree yet, and only until dismissed: it never
    // nags a show that already has one, and never converts on its own.
    QStringList lists;
    if (!m_bridgeDismissed && m_st->productions().isEmpty())
        lists = ShowConverter::cueListChasers(m_doc);
    if (lists.isEmpty())
    {
        m_bridgeBanner->hide();
        return;
    }
    QStringList shown = lists.mid(0, 3);
    if (lists.count() > 3)
        shown << tr("…");
    m_bridgeText->setText(tr("This show has %n chaser(s) that work like cue lists (%1). They play as they are; "
                             "Upgrade this show… makes them Show Tree cue lists, checked, when you want.", "",
                             int(lists.count())).arg(shown.join(QStringLiteral(", "))));
    m_bridgeBanner->show();
}

void ShowTreeView::openConverter()
{
    ShowConverter dialog(m_doc, this);
    dialog.setConverter([this](quint32 chaserId, bool inPlace) {
        return inPlace ? importInPlace(chaserId) : importBeside(chaserId);
    });
    if (m_converterRunner)
        m_converterRunner(&dialog);
    else
        dialog.exec();
}

QStringList ShowTreeView::importBeside(quint32 chaserId)
{
    const Function *f = m_doc->function(chaserId);
    const QString name = f == nullptr ? QString() : f->name();
    const ShowTreeController::ImportReport rep = m_st->importChaser(chaserId);
    if (rep.list == invalidId())
        return QStringList(tr("%1: not imported (%2)").arg(name, rep.notes.join(QStringLiteral("; "))));
    selectNode(rep.list);
    return QStringList(tr("%1: a cue list of %n cue(s) beside the chaser; %2.", "", rep.cues)
                       .arg(name, rep.differChannels == 0 ? tr("plays the same (0 channels differ)")
                                                          : tr("%n channel(s) differ", "", rep.differChannels)));
}

QList<quint32> ShowTreeView::openUpgrade(quint32 onlyChaser)
{
    auto allLists = [this]() {
        QSet<quint32> ids;
        for (const Production &p : m_st->productions())
            for (const CueList &l : p.lists)
                ids.insert(l.id);
        return ids;
    };
    const QSet<quint32> before = allLists();
    ShowConverter dialog(m_doc, this, ShowConverter::Upgrade);
    dialog.setUpgrader([this](const QList<quint32> &chasers, bool inPlace, const QList<quint32> &scenes) {
        return upgradeShow(chasers, inPlace, scenes);
    });
    dialog.setRestorer([this](quint32 listId) { return restoreOriginal(listId); });
    if (onlyChaser != invalidId())
    {
        // Sent from Build: this chaser, nothing else, no capture.
        for (const ShowConverter::Row &r : dialog.rows())
            dialog.setChosen(r.chaser, r.chaser == onlyChaser);
        for (const ShowConverter::CaptureRow &r : dialog.captureRows())
            dialog.setCaptureChosen(r.scene, false);
        dialog.select(onlyChaser);
    }
    if (m_converterRunner)
        m_converterRunner(&dialog);
    else
        dialog.exec();
    // The lists this upgrade made (not ones converted before).
    QList<quint32> made;
    for (quint32 id : allLists())
        if (!before.contains(id) && m_st->cueList(id) != nullptr && !m_st->cueList(id)->looks)
            made << id;
    return made;
}

QStringList ShowTreeView::upgradeShow(const QList<quint32> &chasers, bool inPlace, const QList<quint32> &scenes)
{
    QStringList out;
    const quint64 since = m_st->undoStamp();
    for (quint32 id : chasers)
        out << (inPlace ? importInPlace(id) : importBeside(id));
    int captured = 0;
    for (quint32 id : scenes)
    {
        const Function *f = m_doc->function(id);
        const QString name = f == nullptr ? QString() : f->name();
        ShowTreeController::CapturePlan done;
        if (!m_st->captureIntoPalettes(id, &done))
        {
            out << tr("%1: not captured (%2)").arg(name, done.why);
            continue;
        }
        ++captured;
        out << tr("%1: captured into palettes in %n cue(s), 0 channels differ.", "", done.captured).arg(name);
        for (const QString &k : done.kept)
            out << QStringLiteral("%1: %2").arg(name, k);
    }
    // All of it one undo step.
    if (m_st->groupStepsSince(since, tr("Upgrade this show")))
        m_hint->setText(tr("Show upgraded: %n chaser(s) converted, %1 scene(s) captured. Undo takes it all back.",
                           "", int(chasers.count())).arg(captured));
    updateBridgeBanner();
    return out;
}

QStringList ShowTreeView::importInPlace(quint32 chaserId)
{
    QStringList out;
    const Function *f = m_doc->function(chaserId);
    const QString name = f == nullptr ? QString() : f->name();
    const ShowTreeController::ImportReport rep = m_st->importInPlace(chaserId);
    if (rep.list == invalidId())
    {
        out << tr("%1: not imported (%2)").arg(name, rep.notes.join(QStringLiteral("; ")));
        return out;
    }
    const CueList *list = m_st->cueList(rep.list);
    const ImportSheet::Repoint r = ImportSheet::planRepoint(m_doc, chaserId, list->chaser);
    repointToList(chaserId, rep.list);
    out << tr("%1: a cue list of %n cue(s); %2.", "", rep.cues)
           .arg(name, rep.differChannels == 0 ? tr("plays the same (0 channels differ)")
                                              : tr("%n channel(s) differ", "", rep.differChannels));
    if (rep.dropped + rep.flattened > 0)
        out << tr("%1: %n nested collection(s) read into the cues.", "", rep.dropped + rep.flattened).arg(name);
    if (rep.trackingCues > 0)
        out << tr("%1: %n cue(s) track what the steps repeated (%2 layer(s) set once).", "", rep.trackingCues)
               .arg(name).arg(rep.trackedLayers + rep.trackedIncludes);
    if (rep.filed > 0)
        out << tr("%1: %n original(s) filed under %2.", "", rep.filed)
               .arg(name, ShowTreeController::originalsFolder(name));
    for (const QString &t : rep.timelines)
        out << tr("%1: the Show timeline “%2” plays the list now.").arg(name, t);
    if (r.count() > 0)
        out << tr("%1: %2 fire the list now.").arg(name, r.lines().join(QStringLiteral(", ")));
    selectNode(rep.list);
    m_hint->setText(tr("%1 imported in place").arg(name));
    return out;
}

bool ShowTreeView::restoreOriginal(quint32 listId)
{
    QString why;
    if (!m_st->canRestoreOriginal(listId, &why))
    {
        m_hint->setText(why);
        return false;
    }
    const CueList *list = m_st->cueList(listId);
    const quint32 source = list->source, listChaser = list->chaser;
    const QString name = m_doc->function(source)->name();
    const ImportSheet::Repoint back = ImportSheet::planRepoint(m_doc, listChaser, source);
    QString text = tr("Restore “%1”? The cue list %2 leaves the Show Tree -- with any change made to it since the "
                      "import -- and “%1” and its originals go back to their folders.")
                   .arg(name, list->name);
    if (back.count() > 0)
        text += QStringLiteral(" ") + tr("%1 fire the chaser again.").arg(back.lines().join(QStringLiteral(", ")));
    text += QStringLiteral(" ") + tr("One undo step.");
    if (!confirm(tr("Restore original"), text))
        return false;
    // What fires the list fires the chaser first: the list's chaser goes
    // with the list.
    back.apply(m_doc, true);
    if (!m_st->restoreOriginal(listId, &why))
    {
        back.apply(m_doc, false);
        m_hint->setText(why);
        return false;
    }
    Doc *doc = m_doc;
    if (back.count() > 0)
        m_st->attachToLastStep([doc, back](bool undone) { back.apply(doc, !undone); }, true);
    m_hint->setText(tr("“%1” is back as it was").arg(name));
    return true;
}

bool ShowTreeView::captureIntoPalettes(quint32 sceneId)
{
    const Function *f = m_doc->function(sceneId);
    if (f == nullptr)
        return false;
    const QString name = f->name();
    const ShowTreeController::CapturePlan plan = m_st->planCapture(sceneId);
    if (!plan.ok)
    {
        m_hint->setText(tr("Can't capture “%1”: %2").arg(name, plan.why));
        return false;
    }
    QStringList details = ShowConverter::captureLines(m_doc, plan);
    QStringList cues;
    for (const auto &u : plan.uses)
        if (const CueList *l = m_st->cueList(u.first))
            cues << QStringLiteral("%1 %2").arg(l->name, m_st->cueLabel(*l, l->indexOf(u.second)));
    details << tr("Where: %1").arg(cues.join(QStringLiteral(", ")));
    const QString text = tr("Capture “%1” into palettes? %2 of its %3 channel values become %n layer(s) of palettes "
                            "in %4 cue(s), checked channel for channel: a cue that would change keeps the scene as "
                            "it is. The scene itself is kept. One undo step.", "", int(plan.layers.count()))
                         .arg(name).arg(plan.capturedChannels)
                         .arg(plan.capturedChannels + int(plan.leftovers.count())).arg(plan.uses.count());
    if (!confirm(tr("Capture into palettes"), text, details.join(QStringLiteral("\n"))))
        return false;
    ShowTreeController::CapturePlan done;
    if (!m_st->captureIntoPalettes(sceneId, &done))
    {
        m_hint->setText(tr("“%1” not captured: %2").arg(name, done.why));
        return false;
    }
    QString hint = tr("“%1” captured into palettes in %n cue(s): 0 channels differ", "", done.captured).arg(name);
    if (!done.kept.isEmpty())
        hint += QStringLiteral(" · ") + done.kept.join(QStringLiteral(" · "));
    m_hint->setText(hint);
    return true;
}

quint32 ShowTreeView::openableFunction(QTreeWidgetItem *item) const
{
    if (item == nullptr)
        return Function::invalidId();
    const int kind = item->data(0, KindRole).toInt();
    const quint32 cueId = item->data(0, CueRole).toUInt();
    Target t;
    if (kind == LayerNode)
    {
        const Cue *c = m_st->cue(cueId);
        const Layer *l = c == nullptr ? nullptr : c->layer(item->data(0, IdRole).toUInt());
        if (l != nullptr)
            t = l->target;
    }
    else if (kind == TrackedLayerNode)
    {
        t = carriedTarget(cueId, item->data(0, TargetRole).toString());
    }
    if (!t.isOpaque() || m_doc->function(t.function) == nullptr)
        return Function::invalidId();
    return t.function;
}

bool ShowTreeView::openFunction(quint32 functionId)
{
    const Function *f = m_doc->function(functionId);
    if (f == nullptr)
        return false;
    // W3: the Workbench docked here: it loads there (↩ comes back).
    if (Workbench *bench = dockedWorkbench())
    {
        bench->open(functionId);
        m_hint->setText(tr("“%1” is open in the Workbench: ↩ comes back").arg(f->name()));
        return bench->loadedFunction() == functionId;
    }
    // Otherwise Build's Workbench (the Functions window is hidden).
    const bool ok = m_functionOpener ? m_functionOpener(functionId) : ProgrammingManager::openFunction(functionId);
    m_hint->setText(ok ? tr("“%1” is open in Build's Workbench").arg(f->name())
                       : tr("Couldn't open “%1”: no Build tab").arg(f->name()));
    return ok;
}

QStringList ShowTreeView::alsoIn(quint32 functionId, quint32 cueId) const
{
    QStringList out;
    for (const Production &p : m_st->productions())
        for (const CueList &l : p.lists)
            for (int i = 0; i < l.cues.count(); ++i)
            {
                const Cue &c = l.cues.at(i);
                if (c.id == cueId)
                    continue;
                for (const Layer &layer : c.layers)
                    if (layer.target.function == functionId)
                    {
                        out << (l.looks ? tr("hit %1").arg(m_st->cueLabel(l, i))
                                        : tr("cue %1 of %2").arg(m_st->cueLabel(l, i), l.name));
                        break;
                    }
            }
    // The show's own functions that play it (not the tree's).
    const QList<quint32> usage = m_doc->getUsage(functionId);
    for (int i = 0; i + 1 < usage.count(); i += 2)
    {
        const Function *f = m_doc->function(usage.at(i));
        if (f == nullptr || m_st->isGenerated(f->id()))
            continue;
        const QString line = tr("%1 “%2”").arg(Function::typeToString(f->type()).toLower(), f->name());
        if (!out.contains(line))
            out << line;
    }
    return out;
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
    // VC buttons that play one of the uses' cue or layer (the shared
    // Used by, P-D13).
    out << UsedBy::vcLines(functions);
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
                            "%5 include(s) of the cue before become tracking, %6 layer(s) are released, "
                            "%7 cue(s) become Cue-only, %8 mixing cue(s) follow the one rule. "
                            "One undo step.")
                         .arg(name).arg(plan.tracking).arg(plan.looksRemoved).arg(plan.layersRemoved)
                         .arg(plan.includesRemoved).arg(plan.releases).arg(plan.cueOnly).arg(plan.mixCleared)
                         + (plan.mixKept.isEmpty() ? QString()
                            : tr(" %n cue(s) keep mixing (mix, under Flags in the tree): the one rule would change "
                                 "what they play.", "", int(plan.mixKept.count())));
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
                 tr("Detach %1? Its chaser and its cues' collections become ordinary functions you edit in "
                    "Build (Scenes, Productions folder), and the list leaves the Show Tree. VC widgets and "
                    "show control that play the chaser keep playing it. One undo step.").arg(name)))
        return false;
    const quint32 chaser = m_st->detachList(listId);
    if (chaser == Function::invalidId())
        return false;
    m_hint->setText(tr("%1 is a plain chaser now (Build ▸ Scenes ▸ Productions)").arg(name));
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
            icon = ItemIcons::groupIcon(m_doc, g);
        }
        else if (kind == LibFixture)
        {
            const Fixture *f = m_doc->fixture(id);
            if (f == nullptr)
                return nullptr;
            text = f->name();
            icon = ItemIcons::fixtureIcon(f);
        }
        else if (kind == LibPalette)
        {
            QLCPalette *p = m_doc->palette(id);
            if (p == nullptr)
                return nullptr;
            text = p->name().isEmpty() ? tr("(unnamed)") : p->name();
            icon = ItemIcons::paletteIcon(m_doc, p);
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
            icon = ItemIcons::targetIcon(t);
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
            icon = ItemIcons::targetsIcon(m_doc, gs->groups, gs->fixtures);
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

    // Palettes (R2): by type with the folders under each, or folders first.
    buildLibraryPalettes(all, add);

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
        const bool folder = (kind == LibFolder || kind == LibPaletteFolder) && !item->data(0, PathRole).toString().isEmpty()
                            && item->data(0, PathRole).toString() != QLatin1String("Groups")
                            && !item->data(0, PathRole).toString().endsWith(QLatin1String(":/"));
        const bool leaf = kind != LibSection && kind != LibFolder && kind != LibPaletteFolder;
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
            if (item->data(0, KindRole).toInt() != LibFolder && item->data(0, KindRole).toInt() != LibPaletteFolder)
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
    const bool paletteHeading = kind == LibFolder && item->data(0, PathRole).toString().startsWith(QLatin1String("Palettes"));
    if (kind == LibPaletteFolder || paletteHeading)
    {
        // R2: a palette folder (or the Palettes heading, or a type under it).
        if (!item->isSelected())
            m_library->setCurrentItem(item);
        const QString folder = kind == LibPaletteFolder ? item->data(0, FolderRole).toString() : QString();
        QMenu menu(this);
        fillNewPaletteMenu(menu.addMenu(tr("New palette")), folder);
        menu.addAction(tr("New folder…"), this, [this]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal,
                                                    QString(), &ok);
            if (ok && !n.trimmed().isEmpty())
                newPaletteFolder(n.trimmed());
        })->setObjectName("stNewPaletteFolder");
        if (kind == LibPaletteFolder && !folder.isEmpty())
        {
            menu.addAction(tr("Rename folder"), this, [this]() { startLibraryRename(); });
            menu.addAction(tr("Delete folder (its palettes move up)"), this, [this]() { deleteLibrarySelection(); });
        }
        menu.addSeparator();
        QAction *byType = menu.addAction(tr("Group by type"), this, [this](bool on) { setPaletteGroupByType(on); });
        byType->setCheckable(true);
        byType->setChecked(m_paletteByType);
        menu.exec(m_library->viewport()->mapToGlobal(pos));
        return;
    }
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
    if (kind == LibGroup)
    {
        // Select All Similar (as in the Fixture Manager and Build): the
        // groups like these, selected -- for Make set, right below.
        menu.addSeparator();
        QAction *similar = menu.addAction(tr("Select all similar"), this, [this]() {
            selectSimilarLibraryGroups();
        });
        similar->setObjectName("stSelectSimilarGroups");
        similar->setToolTip(tr("Every group in the same folder made of the same fixtures (as many of each "
                               "kind) and as many heads -- then Make set… for a group set of them"));
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
    if (kind == LibGroup && !selGroups.isEmpty())
    {
        // Move to folder (the Fixture Manager's folders): each folder in
        // use, none, or New Folder… -- the groups stay selected.
        QMenu *move = menu.addMenu(QIcon(":/folder.png"), selGroups.count() > 1
                                       ? tr("Move %1 groups to folder").arg(selGroups.count())
                                       : tr("Move to folder"));
        move->setObjectName("stMoveGroupsToFolder");
        FixtureManager::fillMoveToFolderMenu(move, m_doc, selGroups);
        connect(move, &QMenu::triggered, this, [this, selGroups](QAction *chosen) {
            if (!FixtureManager::applyMoveToFolder(this, m_doc, selGroups, chosen))
                return;
            m_libraryTimer->stop();
            reloadLibrary();
            selectLibraryGroups(selGroups);
        });
        menu.addSeparator();
    }
    if ((kind == LibGroup || kind == LibFixture) && (!selGroups.isEmpty() || !selFixtures.isEmpty()))
    {
        // Relays, house and work lights: for the fixtures or groups' members.
        menu.addAction(OutputRulesDialog::actionText(), this, [this, selGroups, selFixtures]() {
            if (OutputRulesDialog::edit(this, m_doc, selGroups, selFixtures))
                reloadLibrary();    // the badges
        })->setObjectName("stOutputRules");
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
    if (kind == LibPalette && m_doc->paletteOps()->isManaged(id))
    {
        // R2 (P-D8): the everyday operations, as in Programming.
        menu.addSeparator();
        menu.addAction(tr("Edit"), this, [this, id]() { editPalette(id); })->setObjectName("stEditPalette");
        menu.addAction(tr("Rename"), this, [this]() { startLibraryRename(); });
        menu.addAction(tr("Duplicate"), this, [this, id]() {
            const quint32 copy = m_doc->paletteOps()->duplicate(id);
            if (copy != QLCPalette::invalidId())
            {
                reloadLibrary();
                selectLibraryPalette(copy);
                editPalette(copy);
            }
        });
        QList<quint32> pids;
        for (QTreeWidgetItem *sel : m_library->selectedItems())
            if (sel->data(0, KindRole).toInt() == LibPalette && !pids.contains(sel->data(0, IdRole).toUInt()))
                pids << sel->data(0, IdRole).toUInt();
        if (!pids.contains(id))
            pids = { id };
        QMenu *moveMenu = menu.addMenu(pids.count() > 1 ? tr("Move %1 to folder").arg(pids.count()) : tr("Move to folder"));
        moveMenu->addAction(tr("(no folder)"), this, [this, pids]() { dropPalettesOnFolder(pids, QString()); });
        for (const QString &f : m_doc->paletteOps()->folders())
            moveMenu->addAction(f, this, [this, pids, f]() { dropPalettesOnFolder(pids, f); });
        moveMenu->addSeparator();
        moveMenu->addAction(tr("New folder…"), this, [this, pids]() {
            bool ok = false;
            const QString n = QInputDialog::getText(this, tr("Move to a new folder"), tr("Folder name"),
                                                    QLineEdit::Normal, QString(), &ok);
            if (ok && !n.trimmed().isEmpty())
                dropPalettesOnFolder(pids, n.trimmed());
        });
        QMenu *used = menu.addMenu(tr("Used by"));
        used->setObjectName("stPaletteUsedBy");
        for (const PaletteOps::Use &u : m_doc->paletteOps()->usage(id))
        {
            QAction *a = used->addAction(u.label);
            if (u.kind == PaletteOps::Use::Cue || u.kind == PaletteOps::Use::Look)
                connect(a, &QAction::triggered, this, [this, u]() {
                    if (u.layer != invalidId())
                        selectNode(u.layer, u.owner);
                    else
                        selectNode(u.owner);
                });
            else
                a->setEnabled(false);
        }
        QSet<quint32> fns;
        for (const PaletteOps::Use &u : m_doc->paletteOps()->usage(id))
            if (u.function != invalidId())
                fns.insert(u.function);
        for (const QString &vc : UsedBy::vcLines(fns))
            used->addAction(vc)->setEnabled(false);
        if (used->isEmpty())
            used->addAction(tr("(not used)"))->setEnabled(false);
        menu.addSeparator();
        menu.addAction(pids.count() > 1 ? tr("Delete %1 palettes…").arg(pids.count()) : tr("Delete…"), this,
                       [this]() { deleteLibrarySelection(); })->setObjectName("stDeletePalette");
    }
    if (kind == LibFunction)
    {
        const Function *f = m_doc->function(id);
        if (f != nullptr && f->type() == Function::SceneType)
            menu.addAction(tr("Adopt as a hit"), this, [this, id]() { adoptAsLook(id); })
                ->setObjectName("stAdoptAsLook");
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
 * Palettes in the Library (R2, docs/design/PROGRAMMING_ROLE_DESIGN.md §4.2, P-D8)
 ****************************************************************************/

void ShowTreeView::buildLibraryPalettes(QTreeWidgetItem *all,
                                        const std::function<QTreeWidgetItem *(QTreeWidgetItem *, int, quint32)> &add)
{
    PaletteOps *ops = m_doc->paletteOps();
    // The heading is the folder "none": a palette dropped on it is unfiled.
    QTreeWidgetItem *root = new QTreeWidgetItem(all);
    root->setText(0, tr("Palettes"));
    root->setData(0, KindRole, LibPaletteFolder);
    root->setData(0, FolderRole, QString());
    root->setData(0, PathRole, QStringLiteral("Palettes:/"));
    root->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
    root->setToolTip(0, tr("+ Palette (or right-click) makes one; F2 renames; drag onto a folder to file it "
                           "(folders hold any type); ⌫ deletes, saying where it is used. Effect design, .qxfx "
                           "and Bundles are in Build."));

    QList<QLCPalette *> pl;
    for (QLCPalette *p : m_doc->palettes())
        if (!m_st->isInlinePalette(p->id()))
            pl << p;                    // one-off values: Recent only (D21)
    std::sort(pl.begin(), pl.end(), [](const QLCPalette *a, const QLCPalette *b) {
        return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
    });

    // Nested folder rows under @a parent for @a paths (their parents too),
    // first: folders before palettes.
    auto makeFolders = [this](QTreeWidgetItem *parent, const QString &scope, const QStringList &paths) {
        QStringList withParents;
        for (const QString &path : paths)
        {
            const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            for (int i = 1; i <= parts.count(); ++i)
            {
                const QString pfx = parts.mid(0, i).join(QLatin1Char('/'));
                if (!withParents.contains(pfx))
                    withParents << pfx;
            }
        }
        std::sort(withParents.begin(), withParents.end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
        QHash<QString, QTreeWidgetItem *> items;
        for (const QString &path : withParents)
        {
            const int slash = path.lastIndexOf(QLatin1Char('/'));
            QTreeWidgetItem *up = slash < 0 ? parent : items.value(path.left(slash), parent);
            QTreeWidgetItem *f = new QTreeWidgetItem(up);
            f->setText(0, slash < 0 ? path : path.mid(slash + 1));
            f->setIcon(0, QIcon(":/folder.png"));
            f->setData(0, KindRole, LibPaletteFolder);
            f->setData(0, FolderRole, path);
            f->setData(0, PathRole, QStringLiteral("Palettes:%1/%2").arg(scope, path));
            f->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
            f->setToolTip(0, tr("A palette folder: any type goes in it. Drop palettes here to file them."));
            items.insert(path, f);
        }
        return items;
    };

    if (!m_paletteByType)
    {
        // Folders first, any type in each; palettes in no folder below.
        const QHash<QString, QTreeWidgetItem *> items = makeFolders(root, QStringLiteral("*"), ops->folders());
        for (const QLCPalette *p : pl)
            add(items.value(PaletteOps::folderOf(p), root), LibPalette, p->id());
        return;
    }

    // By type (the default), each type's folders under it. A folder with
    // nothing in it yet has no type: it shows at the top.
    QStringList used;
    for (const QLCPalette *p : pl)
        used << PaletteOps::folderOf(p);
    QStringList empty;
    for (const QString &f : ops->folders())
    {
        bool hasAny = false;
        for (const QString &u : used)
            hasAny |= u == f || u.startsWith(f + QLatin1Char('/'));
        if (!hasAny)
            empty << f;
    }
    makeFolders(root, QStringLiteral("*"), empty);
    const QList<QPair<QString, QString>> kinds = {
        { KeyColour, tr("Colour") }, { KeyDimmer, tr("Dimmer") }, { KeyPosition, tr("Position") },
        { KeyGobo, tr("Gobo") }, { KeyZoom, tr("Zoom") }, { KeyBeam, tr("Focus / Frost / Iris") },
        { KeyShutter, tr("Shutter") }, { KeyEffectPrefix, tr("Effects") } };
    QHash<QString, QList<const QLCPalette *>> byKind;
    for (const QLCPalette *p : pl)
    {
        QString key = m_st->keyForPalette(p->id());
        if (isEffectKey(key) || !attributeKeys().contains(key))
            key = isEffectKey(key) || key.isEmpty() ? KeyEffectPrefix : key;
        byKind[key] << p;
    }
    for (const auto &k : kinds)
    {
        const QList<const QLCPalette *> these = byKind.value(k.first);
        if (these.isEmpty())
            continue;
        QTreeWidgetItem *heading = new QTreeWidgetItem(root);
        heading->setText(0, k.second);
        heading->setData(0, KindRole, LibFolder);
        heading->setData(0, PathRole, QStringLiteral("Palettes/%1").arg(k.second));
        heading->setFlags(Qt::ItemIsEnabled);
        QStringList paths;
        for (const QLCPalette *p : these)
            if (!PaletteOps::folderOf(p).isEmpty())
                paths << PaletteOps::folderOf(p);
        const QHash<QString, QTreeWidgetItem *> items = makeFolders(heading, k.second, paths);
        for (const QLCPalette *p : these)
            add(items.value(PaletteOps::folderOf(p), heading), LibPalette, p->id());
    }
}

void ShowTreeView::fillNewPaletteMenu(QMenu *menu, const QString &folder)
{
    // What Programming offers, the same words.
    const QList<QPair<QString, QLCPalette::PaletteType>> types = {
        { tr("Colour"), QLCPalette::Color }, { tr("Dimmer"), QLCPalette::Dimmer },
        { tr("Pan/Tilt"), QLCPalette::PanTilt }, { tr("Aim"), QLCPalette::Aim },
        { tr("Beam (focus, frost, iris)"), QLCPalette::Beam }, { tr("Gobo"), QLCPalette::Gobo },
        { tr("Zoom"), QLCPalette::Zoom }, { tr("Shutter"), QLCPalette::Shutter },
        { tr("Strobe"), QLCPalette::Strobe }, { tr("Effect"), QLCPalette::Effect } };
    if (!folder.isEmpty())
        menu->addSection(tr("In %1").arg(folder));
    for (const auto &t : types)
    {
        const QLCPalette::PaletteType type = t.second;
        menu->addAction(t.first, this, [this, type]() { newPaletteFromLibrary(type); })
            ->setObjectName(QStringLiteral("stNewPalette%1").arg(QLCPalette::typeToString(type)));
    }
}

QString ShowTreeView::libraryPaletteFolder() const
{
    QTreeWidgetItem *item = m_library->currentItem();
    if (item == nullptr)
        return QString();
    const int kind = item->data(0, KindRole).toInt();
    if (kind == LibPaletteFolder)
        return item->data(0, FolderRole).toString();
    if (kind == LibPalette)
        return PaletteOps::folderOf(m_doc->palette(item->data(0, IdRole).toUInt()));
    return QString();
}

void ShowTreeView::selectLibraryPalette(quint32 paletteId)
{
    for (QTreeWidgetItemIterator it(m_library); *it; ++it)
    {
        if ((*it)->data(0, KindRole).toInt() != LibPalette || (*it)->data(0, IdRole).toUInt() != paletteId)
            continue;
        QTreeWidgetItem *top = *it;
        while (top->parent() != nullptr)
            top = top->parent();
        if (top->data(0, IdRole).toUInt() != 2)
            continue;                               // All's, not Recent's
        for (QTreeWidgetItem *p = (*it)->parent(); p != nullptr; p = p->parent())
            p->setExpanded(true);
        m_library->clearSelection();
        m_library->setCurrentItem(*it);
        (*it)->setSelected(true);
        m_library->scrollToItem(*it);
        return;
    }
}

quint32 ShowTreeView::newPaletteFromLibrary(QLCPalette::PaletteType type)
{
    // In the folder selected in the Library, else where the show keeps
    // that type (as a typed value's palette is filed, §10.11).
    const QString folder = libraryPaletteFolder();
    const QString path = folder.isEmpty() ? m_st->suggestPaletteFolder(type) : PaletteOps::pathForFolder(folder);
    const quint32 id = m_doc->paletteOps()->create(type, path);
    if (id == QLCPalette::invalidId())
        return id;
    reloadLibrary();
    selectLibraryPalette(id);
    editPalette(id);
    m_hint->setText(tr("New palette “%1”: set it in the Look Editor; F2 in the Library renames it")
                    .arg(m_doc->palette(id)->name()));
    return id;
}

bool ShowTreeView::newPaletteFolder(const QString &name)
{
    const QString parent = libraryPaletteFolder();
    const QString folder = PaletteOps::cleanFolder(parent.isEmpty() ? name : parent + QLatin1Char('/') + name);
    if (!m_doc->paletteOps()->addFolder(folder))
    {
        m_hint->setText(tr("There is a folder “%1” already").arg(folder));
        return false;
    }
    reloadLibrary();
    return true;
}

bool ShowTreeView::startLibraryRename()
{
    // The row last clicked (the current one), whatever else is selected.
    QTreeWidgetItem *item = m_library->currentItem();
    if (item == nullptr)
        return false;
    const int kind = item->data(0, KindRole).toInt();
    QString text;
    if (kind == LibPalette)
    {
        const quint32 id = item->data(0, IdRole).toUInt();
        if (!m_doc->paletteOps()->isManaged(id))
            return false;
        text = m_doc->palette(id)->name();
    }
    else if (kind == LibPaletteFolder && !item->data(0, FolderRole).toString().isEmpty())
        text = item->text(0);
    else
        return false;
    item->setText(0, text);         // the bare name (no ★)
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_libRenaming = true;
    m_libRenameItem = item;
    m_library->editItem(item, 0);
    return true;
}

void ShowTreeView::slotLibraryItemRenamed(QTreeWidgetItem *item, int column)
{
    if (!m_libRenaming || item != m_libRenameItem || column != 0)
        return;
    m_libRenaming = false;
    m_libRenameItem = nullptr;
    const QString name = item->text(0).trimmed();
    const int kind = item->data(0, KindRole).toInt();
    const quint32 id = item->data(0, IdRole).toUInt();
    const QString folder = item->data(0, FolderRole).toString();
    // After the editor is done with the row: the rename rebuilds the Library.
    QTimer::singleShot(0, this, [this, kind, id, folder, name]() {
        if (kind == LibPalette)
            m_doc->paletteOps()->rename(id, name);
        else
            m_doc->paletteOps()->renameFolder(folder, name);
        reloadLibrary();
        if (kind == LibPalette)
        {
            selectLibraryPalette(id);
            if (editedPalette() == id)
                m_lookEditor->setPalette(id);
        }
    });
}

bool ShowTreeView::deleteLibrarySelection()
{
    QList<quint32> pids;
    for (QTreeWidgetItem *sel : m_library->selectedItems())
        if (sel->data(0, KindRole).toInt() == LibPalette && m_doc->paletteOps()->isManaged(sel->data(0, IdRole).toUInt())
            && !pids.contains(sel->data(0, IdRole).toUInt()))
            pids << sel->data(0, IdRole).toUInt();
    QTreeWidgetItem *cur = m_library->currentItem();
    if (pids.isEmpty() && cur != nullptr && cur->data(0, KindRole).toInt() == LibReusable)
        return deleteReusable(cur->data(0, IdRole).toUInt());
    if (pids.isEmpty() && cur != nullptr && cur->data(0, KindRole).toInt() == LibPaletteFolder)
    {
        const QString folder = cur->data(0, FolderRole).toString();
        if (folder.isEmpty())
            return false;
        const int n = m_doc->paletteOps()->folderCount(folder);
        if (n > 0 && !confirm(tr("Delete folder"),
                              tr("Delete the folder “%1”? The %n palette(s) in it move up a level; nothing is "
                                 "deleted.", "", n).arg(folder)))
            return false;
        return m_doc->paletteOps()->removeFolder(folder);
    }
    if (pids.isEmpty())
        return false;
    // Asks, naming every cue, look, scene and VC button that uses each.
    if (!confirm(pids.count() > 1 ? tr("Delete palettes") : tr("Delete palette"),
                 UsedBy::deletePalettesQuestion(m_doc, pids)))
        return false;
    const bool editing = pids.contains(editedPalette());
    if (!m_doc->paletteOps()->remove(pids))
        return false;
    if (editing)
        editPalette(QLCPalette::invalidId());
    // Their uses went with them (open 26); Undo brings both back.
    updateInspector();
    return true;
}

bool ShowTreeView::dropPalettesOnFolder(const QList<quint32> &paletteIds, const QString &folder)
{
    QList<quint32> ids;
    for (quint32 id : paletteIds)
        if (m_doc->paletteOps()->isManaged(id))
            ids << id;
    if (ids.isEmpty() || !m_doc->paletteOps()->move(ids, folder))
        return false;
    reloadLibrary();
    selectLibraryPalette(ids.first());
    m_hint->setText(folder.isEmpty() ? tr("%n palette(s) in no folder", "", int(ids.count()))
                                     : tr("%n palette(s) → %1", "", int(ids.count())).arg(folder));
    return true;
}

void ShowTreeView::setPaletteGroupByType(bool byType)
{
    if (m_paletteByType == byType)
        return;
    m_paletteByType = byType;
    QSettings().setValue(QStringLiteral("showtree/libraryPalettesByType"), byType);
    reloadLibrary();
}

QStringList ShowTreeView::libraryPaletteRows() const
{
    QStringList out;
    QTreeWidgetItem *root = nullptr;
    for (QTreeWidgetItemIterator it(m_library); *it && root == nullptr; ++it)
        if ((*it)->data(0, PathRole).toString() == QLatin1String("Palettes:/"))
            root = *it;
    if (root == nullptr)
        return out;
    std::function<void(QTreeWidgetItem *, int)> walk = [&](QTreeWidgetItem *item, int depth) {
        for (int i = 0; i < item->childCount(); ++i)
        {
            QTreeWidgetItem *c = item->child(i);
            QString text = c->text(0);
            if (text.startsWith(QStringLiteral("★ ")))
                text = text.mid(2);
            if (c->data(0, KindRole).toInt() == LibPaletteFolder)
                text += QLatin1Char('/');
            out << QString(depth * 2, QLatin1Char(' ')) + text;
            walk(c, depth + 1);
        }
    };
    walk(root, 0);
    return out;
}

QString ShowTreeView::usedByText() const
{
    return m_lookEditor->usedByText();
}

void ShowTreeView::updateUsedBy(quint32 paletteId)
{
    // P-D13: where it is used, a click for the list (and a jump there).
    if (m_doc->palette(paletteId) == nullptr || m_st->isInlinePalette(paletteId))
    {
        m_lookEditor->setUsedBy(QString());
        return;
    }
    const QString summary = UsedBy::paletteSummary(m_doc, paletteId);
    m_lookEditor->setUsedBy(summary.isEmpty()
                            ? tr("Used by: nothing yet")
                            : tr("Used by: %1 — <a href=\"usedby\">which</a>").arg(summary.toHtmlEscaped()));
}

void ShowTreeView::usedByLinkActivated(const QString &link)
{
    const quint32 pid = m_lookEditor->paletteId();
    if (link != QLatin1String("usedby") || m_doc->palette(pid) == nullptr)
        return;
    QMenu menu(this);
    for (const PaletteOps::Use &u : m_doc->paletteOps()->usage(pid))
    {
        QAction *a = menu.addAction(u.label);
        if (u.kind == PaletteOps::Use::Cue || u.kind == PaletteOps::Use::Look)
            connect(a, &QAction::triggered, this, [this, u]() {
                if (u.layer != invalidId())
                    selectNode(u.layer, u.owner);
                else
                    selectNode(u.owner);
            });
        else
            a->setEnabled(false);
    }
    QSet<quint32> fns;
    for (const PaletteOps::Use &u : m_doc->paletteOps()->usage(pid))
        if (u.function != invalidId())
            fns.insert(u.function);
    for (const QString &vc : UsedBy::vcLines(fns))
        menu.addAction(vc)->setEnabled(false);
    menu.exec(QCursor::pos());
}

/****************************************************************************
 * The Looks section (R3, docs/design/PROGRAMMING_ROLE_DESIGN.md §4.1)
 ****************************************************************************/

bool ShowTreeView::selectionInLooks() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (item == nullptr)
        return false;
    switch (item->data(0, KindRole).toInt())
    {
        case LookFolderNode: return true;
        case ListNode: return m_st->isLooks(item->data(0, IdRole).toUInt());
        case CueNode: return m_st->isLook(item->data(0, IdRole).toUInt());
        case LayerNode:
        case TrackedLayerNode: return m_st->isLook(item->data(0, CueRole).toUInt());
        default: break;
    }
    return false;
}

quint32 ShowTreeView::currentProductionId() const
{
    for (QTreeWidgetItem *item = m_tree->currentItem(); item != nullptr; item = item->parent())
        if (item->data(0, KindRole).toInt() == ProductionNode)
            return item->data(0, IdRole).toUInt();
    return m_st->productions().count() == 1 ? m_st->productions().first().id : invalidId();
}

QString ShowTreeView::currentLookFolder() const
{
    QTreeWidgetItem *item = m_tree->currentItem();
    return item != nullptr && item->data(0, KindRole).toInt() == LookFolderNode
           ? item->data(0, FolderRole).toString() : QString();
}

quint32 ShowTreeView::newSubCue()
{
    quint32 cueId = currentCueId();
    if (cueId == invalidId())
    {
        m_hint->setText(tr("Select a cue first: its sub-cues go under it"));
        return invalidId();
    }
    quint32 parent = cueId, after = invalidId();
    if (m_st->isSubCue(cueId, &parent))
        after = cueId;
    QString why;
    const quint32 id = m_st->addSubCue(parent, QString(), after, &why);
    if (id == invalidId())
    {
        m_hint->setText(why);
        return id;
    }
    selectNode(id);
    if (QTreeWidgetItem *item = m_tree->currentItem())
        m_tree->scrollToItem(item);
    return id;
}

quint32 ShowTreeView::newLook()
{
    const quint32 prod = currentProductionId();
    if (prod == invalidId())
    {
        m_hint->setText(tr("Select a production first (+ Production)"));
        return invalidId();
    }
    // After the selected look, in its folder; else in the selected folder.
    quint32 after = invalidId();
    QString folder = currentLookFolder();
    const quint32 cueId = currentCueId();
    if (cueId != invalidId() && m_st->isLook(cueId))
    {
        after = cueId;
        folder = m_st->cue(cueId)->folder;
    }
    const quint32 id = m_st->addLook(prod, QString(), folder, after);
    if (id != invalidId())
    {
        selectNode(id);
        m_cueName->setFocus();
    }
    return id;
}

QString ShowTreeView::lookFolderDropHint(const QList<quint32> &lookIds, quint32 listId, const QString &folder) const
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr || !list->looks || lookIds.isEmpty())
        return QString();
    bool any = false;
    for (quint32 id : lookIds)
    {
        const Cue *c = list->cue(id);
        if (c == nullptr)
            return QString();               // a cue of a list, or another section's look
        any |= c->folder != folder;
    }
    if (!any)
        return QString();
    const QString what = lookIds.count() == 1 ? m_st->cueLabel(*list, list->indexOf(lookIds.first()))
                                              : tr("%1 hits").arg(lookIds.count());
    return folder.isEmpty() ? tr("%1 → out of its folder").arg(what) : tr("%1 → %2").arg(what, folder);
}

bool ShowTreeView::dropLooksOnFolder(const QList<quint32> &lookIds, quint32 listId, const QString &folder)
{
    if (lookFolderDropHint(lookIds, listId, folder).isEmpty())
        return false;
    return m_st->setLookFolder(lookIds, folder);
}

QStringList ShowTreeView::lookFiredBy(quint32 lookId) const
{
    const quint32 fid = m_st->lookFunction(lookId);
    return fid == invalidId() ? QStringList() : UsedBy::vcLines({ fid });
}

bool ShowTreeView::makeVcPage(quint32 listId, const QString &folder, QString *message)
{
    auto say = [message](const QString &text) {
        if (message != nullptr)
            *message = text;
    };
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc == nullptr || vc->contents() == nullptr)
    {
        say(tr("No Virtual Console to put it on."));
        return false;
    }
    const CueList *list = m_st->cueList(listId);
    quint32 prodId = invalidId();
    m_st->cueList(listId, &prodId);
    const QList<quint32> looks = m_st->looksIn(listId, folder);
    if (list == nullptr || !list->looks || looks.isEmpty())
    {
        say(tr("No hits there yet."));
        return false;
    }
    const Production *prod = m_st->production(prodId);
    const QList<ResolvedCue> resolved = m_st->resolved(*list);

    const int size = 80, gap = 6, header = 36;
    auto colourOf = [&](VCButton *b, int at) {
        // Its colour: the look's first Colour (as it plays).
        for (const ResolvedLayer &rl : resolved.value(at).layers)
        {
            const ResolvedValue v = rl.values.value(KeyColour);
            const QLCPalette *p = v.look.palettes.isEmpty() ? nullptr : m_doc->palette(v.look.palettes.first());
            if (p == nullptr)
                continue;
            const QColor c = p->rgbValue();
            b->setBackgroundColor(c);
            b->setForegroundColor(qGray(c.rgb()) > 140 ? QColor(Qt::black) : QColor(Qt::white));
            break;
        }
    };
    auto makeButton = [&](VCFrame *frame, quint32 lookId, const QPoint &at) -> VCButton * {
        const int i = list->indexOf(lookId);
        const quint32 fid = m_st->lookFunction(lookId);
        if (i < 0 || fid == invalidId())
            return nullptr;
        VCButton *b = new VCButton(frame, m_doc);
        vc->placeWidget(b, frame);
        b->setHit(lookId);
        b->setCaption(m_st->cueLabel(*list, i));
        b->setAction(VCButton::Toggle);
        b->move(at);
        b->resize(QSize(size, size));
        colourOf(b, i);
        b->show();
        return b;
    };

    /* Open 22: the frame it made before is updated, not added again --
       a button for each new look, the buttons it made for looks now gone
       taken away; every other widget in it (the user's own, moved or
       resized buttons) left as it is. Gone (deleted on the console): a new
       one. */
    VCFrame *frame = qobject_cast<VCFrame *>(vc->widget(m_st->lookVcFrame(listId, folder)));
    if (frame != nullptr)
    {
        QList<quint32> mine = m_st->lookVcButtons(listId, folder);
        QSet<quint32> lookFids;
        for (quint32 lookId : looks)
            lookFids.insert(m_st->lookFunction(lookId));
        QSet<quint32> have;
        int removed = 0;
        for (VCButton *b : frame->findChildren<VCButton *>(QString(), Qt::FindDirectChildrenOnly))
        {
            if (lookFids.contains(b->function()))
            {
                have.insert(b->function());
                continue;
            }
            if (mine.contains(b->id()))
            {
                // One it made, for a look gone (deleted, or filed elsewhere).
                mine.removeAll(b->id());
                b->setProperty("stGoing", true);
                b->hide();
                vc->removeWidget(b);
                ++removed;
            }
        }
        // New looks: in the first free places of the grid, after what is there.
        int added = 0, slot = 0;
        const int cols = 6;
        auto taken = [&](const QRect &r) {
            for (QObject *o : frame->children())
                if (VCWidget *w = qobject_cast<VCWidget *>(o))
                    if (!w->property("stGoing").toBool() && w->geometry().intersects(r))
                        return true;
            return false;
        };
        for (quint32 lookId : looks)
        {
            if (have.contains(m_st->lookFunction(lookId)))
                continue;
            QRect r;
            do
            {
                r = QRect(5 + (slot % cols) * (size + gap), header + (slot / cols) * (size + gap), size, size);
                ++slot;
            } while (taken(r) && slot < 10000);
            if (VCButton *b = makeButton(frame, lookId, r.topLeft()))
            {
                mine << b->id();
                ++added;
                // The frame grows to hold it.
                const QSize want(qMax(frame->width(), r.right() + gap + 5), qMax(frame->height(), r.bottom() + gap));
                if (want != frame->size())
                    frame->resize(want);
            }
        }
        m_st->setLookVcFrame(listId, folder, frame->id(), mine);
        m_doc->setModified();
        buildTree();
        updateInspector();
        say(added == 0 && removed == 0
            ? tr("VC page “%1” is up to date").arg(frame->caption())
            : tr("VC page “%1” updated: %2 button(s) added, %3 taken away").arg(frame->caption()).arg(added).arg(removed));
        return true;
    }

    // Below what the console already holds, at its left.
    VCFrame *contents = vc->contents();
    int y = 10;
    for (QObject *o : contents->children())
        if (VCWidget *w = qobject_cast<VCWidget *>(o))
            y = qMax(y, w->geometry().bottom() + 10);
    const int cols = qMin(6, int(looks.count()));
    const int rows = (int(looks.count()) + cols - 1) / cols;

    frame = new VCFrame(contents, m_doc, true);
    vc->placeWidget(frame, contents);
    frame->setCaption(folder.isEmpty() ? tr("Hits — %1").arg(prod == nullptr ? QString() : prod->name)
                                       : tr("Hits — %1").arg(folder));
    frame->setHeaderVisible(true);
    frame->move(QPoint(10, y));
    frame->resize(QSize(10 + cols * (size + gap), header + rows * (size + gap) + 4));

    int n = 0;
    QList<quint32> made;
    for (quint32 lookId : looks)
        if (VCButton *b = makeButton(frame, lookId, QPoint(5 + (n % cols) * (size + gap),
                                                            header + (n / cols) * (size + gap))))
        {
            made << b->id();
            ++n;
        }
    frame->show();
    m_st->setLookVcFrame(listId, folder, frame->id(), made);
    m_doc->setModified();
    buildTree();
    updateInspector();
    say(tr("VC page “%1”: %n button(s), one per hit (Virtual Console)", "", n).arg(frame->caption()));
    return n > 0;
}

quint32 ShowTreeView::adoptAsLook(quint32 functionId)
{
    const Function *f = m_doc->function(functionId);
    const quint32 prod = currentProductionId();
    if (f == nullptr || prod == invalidId())
    {
        m_hint->setText(tr("Select a production first (+ Production)"));
        return invalidId();
    }
    QString why;
    const quint32 id = m_st->adoptAsLook(prod, functionId, &why);
    if (id == invalidId())
    {
        m_hint->setText(why);
        return invalidId();
    }
    // The VC buttons firing the scene may fire the look instead (P-D16);
    // the scene stays either way.
    const QList<VCButton *> buttons = UsedBy::vcButtons({ functionId });
    if (!buttons.isEmpty()
        && confirm(tr("Adopt as a hit"),
                   tr("%n VC button(s) fire “%1”. Fire the hit instead?", "", int(buttons.count())).arg(f->name()),
                   UsedBy::vcLines({ functionId }).join(QStringLiteral("\n"))))
    {
        const quint32 fid = m_st->lookFunction(id);
        for (VCButton *b : buttons)
        {
            b->setFunction(fid);
            b->setHit(id);
        }
        m_doc->setModified();
    }
    selectNode(id);
    m_hint->setText(why.isEmpty() ? tr("“%1” is a hit: its groups and palettes are a layer").arg(f->name())
                                  : tr("“%1” is a hit, played as it is: %2").arg(f->name(), why));
    return id;
}

void ShowTreeView::showLooksPage(quint32 listId, const QString &folder)
{
    const CueList *list = m_st->cueList(listId);
    if (list == nullptr)
        return;
    for (QWidget *w : { static_cast<QWidget *>(m_listMode), static_cast<QWidget *>(m_listBase) })
        m_listForm->setRowVisible(w, false);
    m_listForm->setRowVisible(m_listColour, false);
    m_listForm->setRowVisible(m_listMib, false);
    if (m_listAddSection != nullptr)
        m_listForm->setRowVisible(m_listAddSection, false);
    m_listConvert->hide();
    m_listDetach->hide();
    m_listVcPage->show();
    const QList<quint32> looks = m_st->looksIn(listId, folder);
    m_listVcPage->setEnabled(!looks.isEmpty());
    m_listTitle->setText(folder.isEmpty() ? tr("Hits") : tr("Hits › %1").arg(folder));
    QStringList lines;
    for (quint32 id : looks)
    {
        const QStringList fired = lookFiredBy(id);
        lines << QStringLiteral("• %1%2").arg(m_st->cueLabel(*list, list->indexOf(id)),
                                             fired.isEmpty() ? QString() : QStringLiteral(" — ") + fired.join(QStringLiteral(", ")));
    }
    m_listInfo->setText(tr("%n hit(s) here.", "", int(looks.count()))
                        + (lines.isEmpty() ? QString() : QStringLiteral("\n") + lines.join(QLatin1Char('\n')))
                        + QStringLiteral("\n\n")
                        + tr("A hit is built like a cue -- layers, attribute rows, effects -- but never tracks: "
                             "it plays only what it has. It plays from a VC button (Toggle or Flash), over "
                             "whatever runs: the highest intensity wins, the last started sets the rest. Make "
                             "VC page puts a button for each on the Virtual Console; + Hit adds one; drag hits "
                             "onto a folder to file them."));
    m_pages->setCurrentWidget(m_listPage);
}

void ShowTreeView::lookFolderMenu(QMenu *menu, quint32 listId, const QList<quint32> &lookIds)
{
    menu->addAction(tr("(no folder)"), this, [this, lookIds]() { m_st->setLookFolder(lookIds, QString()); });
    for (const QString &f : m_st->lookFolders(listId))
        menu->addAction(f, this, [this, lookIds, f]() { m_st->setLookFolder(lookIds, f); });
    menu->addSeparator();
    menu->addAction(tr("New folder…"), this, [this, lookIds]() {
        bool ok = false;
        const QString n = QInputDialog::getText(this, tr("Move to a new folder"), tr("Folder name"),
                                                QLineEdit::Normal, QString(), &ok);
        if (ok && !n.trimmed().isEmpty())
            m_st->setLookFolder(lookIds, n.trimmed());
    });
}

void ShowTreeView::lookLinkActivated(const QString &link)
{
    const quint32 lookId = m_pageCue;
    const CueList *list = m_st->listOfCue(lookId);
    if (list == nullptr || !list->looks)
        return;
    if (link == QLatin1String("vcpage"))
    {
        QString msg;
        makeVcPage(list->id, list->cue(lookId)->folder, &msg);
        m_hint->setText(msg);
    }
    else if (link == QLatin1String("folder"))
    {
        QMenu menu(this);
        lookFolderMenu(&menu, list->id, { lookId });
        menu.exec(QCursor::pos());
    }
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

bool ShowTreeView::isCommandSyntaxShown() const
{
    return !m_commandSyntax->isHidden();
}

void ShowTreeView::setCommandSyntaxShown(bool shown)
{
    m_commandSyntax->setVisible(shown);
    {
        QSignalBlocker block(m_commandSyntaxButton);
        m_commandSyntaxButton->setChecked(shown);
    }
    QSettings().setValue(QStringLiteral("showtree/commandSyntax"), shown);
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
    // A palette made or named for editing: in the Look Editor, and shown
    // in the Library (R2).
    if (r.ok && r.editPalette != QLCPalette::invalidId() && m_doc->palette(r.editPalette) != nullptr)
    {
        reloadLibrary();
        selectLibraryPalette(r.editPalette);
        editPalette(r.editPalette);
    }
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
    // W3: `wb`: the cue (and its layer) in the Workbench, docked here.
    if (r.openWorkbench)
        return openInWorkbench(r.context.cue, r.layer);
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
    if (m_tree != nullptr && obj == m_tree->viewport() && ev->type() == QEvent::Resize)
        fitTreeColumns();
    // Q60: a click in the tree applies a time typed on the cue page first,
    // to the cue it was typed for -- before the click selects another.
    if (m_tree != nullptr && obj == m_tree->viewport() && ev->type() == QEvent::MouseButtonPress)
        commitPendingCueEdits();
    // A click on a cue (or a layer in it) is a pick, also on the one already
    // selected: after GO it takes the stage back (LIVE_BLIND.md).
    if (m_tree != nullptr && obj == m_tree->viewport() && ev->type() == QEvent::MouseButtonRelease
        && static_cast<QMouseEvent *>(ev)->button() == Qt::LeftButton
        && m_tree->itemAt(static_cast<QMouseEvent *>(ev)->position().toPoint()) != nullptr)
        pickCurrentCue();
    // Esc in a time field: the cue's own value back, nothing applied.
    if (ev->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(ev)->key() == Qt::Key_Escape
        && (obj == m_cueFadeIn || obj == m_cueFadeOut || obj == m_cueHold || obj == m_cueWait)
        && (m_cueTimesDirty || m_cueWaitDirty || !static_cast<QDoubleSpinBox *>(obj)->hasAcceptableInput()))
    {
        revertCueTimes();
        static_cast<QDoubleSpinBox *>(obj)->selectAll();
        return true;
    }
    // F1 in the command line: its syntax, not the help window.
    if (obj == m_command && ev->type() == QEvent::ShortcutOverride
        && static_cast<QKeyEvent *>(ev)->key() == Qt::Key_F1)
    {
        ev->accept();
        return true;
    }
    if (obj == m_command && ev->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_F1 && ke->modifiers() == Qt::NoModifier)
        {
            setCommandSyntaxShown(!isCommandSyntaxShown());
            return true;
        }
        // ? typed first: the syntax shows (the ? stays: Return runs help).
        if (ke->text() == QLatin1String("?") && m_command->text().trimmed().isEmpty())
            setCommandSyntaxShown(true);
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
        && static_cast<QKeyEvent *>(ev)->key() == Qt::Key_Slash
        && (static_cast<QKeyEvent *>(ev)->modifiers() & Qt::ControlModifier))
    {
        // ⌘/: skip the selected cues, or put them back.
        toggleSkipSelected();
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
