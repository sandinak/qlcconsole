/*
  Q Light Controller Plus
  monitorgraphicsview_freestring.cpp

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

/* The plot's side of a Free string pixel path (PixelPath::FreeString,
   docs/features/lighting-studio.md "Free string (anchors)"): the selected
   string's anchors as handles to drag, a right-click menu on each, and
   "click to add an anchor". Every change is one undo step (pushRigUndo). */

#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QMouseEvent>
#include <QMenu>
#include <QtMath>

#include "monitorgraphicsview.h"
#include "monitorfixtureitem.h"
#include "monitorproperties.h"
#include "freestringhandles.h"
#include "pixelpathdecor.h"
#include "contextmenus.h"
#include "pixelpath.h"
#include "doc.h"

FreeStringHandles *MonitorGraphicsView::stringHandles() const
{
    return m_stringHandles != nullptr && m_stringHandles->isVisible() ? m_stringHandles : nullptr;
}

QVector<StringAnchor> MonitorGraphicsView::editableAnchors(quint32 fid) const
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.stringAnchors.size() >= 2)
        return rp.stringAnchors;
    return PixelPath::freeStringAnchors(rp, props->fixtureRigPosition(fid),
                                        PixelPathDecor::ledCount(m_doc, fid));
}

void MonitorGraphicsView::putAnchors(quint32 fid, const QVector<StringAnchor> &anchors,
                                     const QByteArray &before)
{
    MonitorProperties *props = m_doc->monitorProperties();
    FixtureRigProps rp = props->fixtureRigProps(fid);
    if (rp.stringAnchors == anchors)
        return;
    rp.stringAnchors = anchors;
    props->setFixtureRigProps(fid, rp);
    if (!before.isEmpty())
        pushRigUndo(before);
    m_doc->setModified();
    updateFixture(fid);         // its LEDs, and the handles with them
}

bool MonitorGraphicsView::stringEditable(quint32 fid) const
{
    if (m_layoutLocked)
        return false;
    MonitorFixtureItem *item = m_fixtures.value(fid, nullptr);
    return item != nullptr && (item->flags() & QGraphicsItem::ItemIsMovable);
}

void MonitorGraphicsView::refreshStringHandles()
{
    MonitorProperties *props = m_doc != nullptr ? m_doc->monitorProperties() : nullptr;
    if (props == nullptr || m_scene == nullptr)
        return;
    // The one being laid, else the one free string selected on its own.
    quint32 fid = m_addAnchorFid;
    if (fid == UINT_MAX)
    {
        QList<quint32> picked;
        foreach (QGraphicsItem *gi, m_scene->selectedItems())
            if (MonitorFixtureItem *mfi = dynamic_cast<MonitorFixtureItem *>(gi))
                picked << mfi->fixtureID();
        if (picked.size() == 1)
            fid = picked.first();
    }
    const bool show = fid != UINT_MAX && m_fixtures.contains(fid)
                      && props->fixtureRigProps(fid).pixelPath == PixelPath::FreeString
                      && m_fixtures.value(fid)->isVisible();
    if (!show)
    {
        if (m_stringHandles != nullptr)
            m_stringHandles->hide();
        return;
    }
    if (m_stringHandles != nullptr && m_stringHandles->fixtureID() != fid)
    {
        m_scene->removeItem(m_stringHandles);
        delete m_stringHandles;
        m_stringHandles = nullptr;
    }
    if (m_stringHandles == nullptr)
    {
        m_stringHandles = new FreeStringHandles(fid);
        m_scene->addItem(m_stringHandles);
    }
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    const QVector<StringAnchor> anchors = editableAnchors(fid);
    QVector<QPointF> pts;
    QVector<int> pins;
    QVector<bool> rev;
    for (const StringAnchor &a : anchors)
    {
        pts << projectMm(double(a.x) * 1000.0, double(a.y) * 1000.0, double(a.z) * 1000.0);
        pins << a.pin;
        rev << a.reverse;
    }
    const double scale = viewScale() > 0.0 ? viewScale() : 1.0;
    m_stringHandles->setAnchors(pts, pins, rev, rp.stringAnchors.size() >= 2, 7.0 / scale);
    m_stringHandles->show();
}

int MonitorGraphicsView::stringHandleAt(const QPointF &scenePos) const
{
    const FreeStringHandles *h = stringHandles();
    return h != nullptr ? h->handleAt(scenePos) : -1;
}

void MonitorGraphicsView::beginAddAnchors(quint32 fid)
{
    if (m_doc->fixture(fid) == nullptr || !m_fixtures.contains(fid))
        return;
    m_addAnchorFid = fid;
    viewport()->setCursor(Qt::CrossCursor);
    refreshStringHandles();
    const int own = m_doc->monitorProperties()->fixtureRigProps(fid).stringAnchors.size();
    emit notice(own == 0 ? tr("Click on the plan where the string starts, then where it goes. "
                              "Esc or a right-click ends.")
                         : tr("Click on the plan for anchor %1. Esc or a right-click ends.").arg(own + 1));
}

void MonitorGraphicsView::endAddAnchors()
{
    if (m_addAnchorFid == UINT_MAX)
        return;
    m_addAnchorFid = UINT_MAX;
    viewport()->unsetCursor();
    refreshStringHandles();
}

void MonitorGraphicsView::addStringAnchorAt(quint32 fid, const QPointF &scenePos)
{
    MonitorProperties *props = m_doc->monitorProperties();
    const FixtureRigProps rp = props->fixtureRigProps(fid);
    QVector<StringAnchor> anchors = rp.stringAnchors;
    // Its own height (the fixture's) from above; the last one's depth or across in an elevation.
    const QVector3D at = props->fixtureRigPosition(fid);
    QVector3D cur = anchors.isEmpty() ? at : anchors.last().pos();
    if (m_pov == PovTop)
        cur.setZ(at.z());
    const QVector3D w = unprojectMm(snapScenePos(scenePos), cur * 1000.0f) / 1000.0f;
    StringAnchor a;
    a.x = w.x();
    a.y = w.y();
    a.z = w.z();
    anchors << a;
    putAnchors(fid, anchors, m_rigSnapshotter ? m_rigSnapshotter() : QByteArray());
    emit notice(tr("Anchor %1 placed. Click for the next, Esc or a right-click ends.").arg(anchors.size()));
}

void MonitorGraphicsView::insertStringAnchor(quint32 fid, int before)
{
    QVector<StringAnchor> anchors = editableAnchors(fid);
    if (before < 0 || before >= anchors.size())
        return;
    // Half way back to the one before it; the first gets one a foot before it.
    StringAnchor a = anchors.at(before);
    a.pin = 0;
    a.reverse = before > 0 ? anchors.at(before - 1).reverse : false;
    if (before > 0)
    {
        const QVector3D mid = (anchors.at(before - 1).pos() + anchors.at(before).pos()) / 2.0f;
        a.x = mid.x(); a.y = mid.y(); a.z = mid.z();
    }
    else if (anchors.size() > 1)
    {
        QVector3D d = anchors.at(0).pos() - anchors.at(1).pos();
        d = d.length() > 1e-4f ? d.normalized() : QVector3D(-1, 0, 0);
        const QVector3D p = anchors.at(0).pos() + d * 0.3048f;
        a.x = p.x(); a.y = p.y(); a.z = p.z();
    }
    anchors.insert(before, a);
    putAnchors(fid, anchors, m_rigSnapshotter ? m_rigSnapshotter() : QByteArray());
}

void MonitorGraphicsView::deleteStringAnchor(quint32 fid, int index)
{
    QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index < 0 || index >= anchors.size())
        return;
    anchors.remove(index);
    putAnchors(fid, anchors, m_rigSnapshotter ? m_rigSnapshotter() : QByteArray());
}

void MonitorGraphicsView::setStringAnchorPin(quint32 fid, int index, int led)
{
    QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index < 0 || index >= anchors.size())
        return;
    anchors[index].pin = qMax(0, led);
    putAnchors(fid, anchors, m_rigSnapshotter ? m_rigSnapshotter() : QByteArray());
}

void MonitorGraphicsView::setStringAnchorReverse(quint32 fid, int index, bool on)
{
    QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index < 0 || index >= anchors.size())
        return;
    anchors[index].reverse = on;
    putAnchors(fid, anchors, m_rigSnapshotter ? m_rigSnapshotter() : QByteArray());
}

int MonitorGraphicsView::stringLedNearAnchor(quint32 fid, int index) const
{
    const QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index < 0 || index >= anchors.size())
        return 1;
    const QVector<QVector3D> leds = PixelPath::positions(m_doc, m_doc->monitorProperties(), fid,
                                                         PixelPathDecor::ledCount(m_doc, fid));
    int best = 0;
    float bestD = 1e9f;
    for (int i = 0; i < leds.size(); ++i)
    {
        const float d = (leds.at(i) - anchors.at(index).pos()).length();
        if (d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return best + 1;
}

void MonitorGraphicsView::stringDragTo(quint32 fid, int index, const QPointF &scenePos,
                                       Qt::KeyboardModifiers mods)
{
    QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index < 0 || index >= anchors.size())
        return;
    if (m_stringDragFid != fid || m_stringDragIndex != index)
    {
        m_stringDragFid = fid;
        m_stringDragIndex = index;
        m_stringDragMoved = false;
        m_stringDragBefore = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
        m_stringDragStartScene = projectMm(double(anchors.at(index).x) * 1000.0,
                                           double(anchors.at(index).y) * 1000.0,
                                           double(anchors.at(index).z) * 1000.0);
    }
    QPointF sp = scenePos;
    if (mods & Qt::ShiftModifier)
    {
        // Along the view's horizontal or vertical only, whichever is more.
        const QPointF d = sp - m_stringDragStartScene;
        if (qAbs(d.x()) >= qAbs(d.y()))
            sp.setY(m_stringDragStartScene.y());
        else
            sp.setX(m_stringDragStartScene.x());
    }
    sp = snapScenePos(sp);
    const QVector3D w = unprojectMm(sp, anchors.at(index).pos() * 1000.0f) / 1000.0f;
    anchors[index].x = w.x();
    anchors[index].y = w.y();
    anchors[index].z = w.z();
    m_stringDragMoved = true;
    putAnchors(fid, anchors, QByteArray());     // the undo step is the whole drag
}

void MonitorGraphicsView::stringDragEnd()
{
    const bool moved = m_stringDragMoved && m_stringDragFid != UINT_MAX;
    const QByteArray before = m_stringDragBefore;
    m_stringDragFid = UINT_MAX;
    m_stringDragIndex = -1;
    m_stringDragMoved = false;
    m_stringDragBefore.clear();
    if (moved && !before.isEmpty())
        pushRigUndo(before);
}

bool MonitorGraphicsView::stringPress(QMouseEvent *event)
{
    const QPointF sp = mapToScene(event->pos());
    if (m_addAnchorFid != UINT_MAX)
    {
        if (event->button() == Qt::LeftButton)
        {
            addStringAnchorAt(m_addAnchorFid, sp);
            m_lastAnchorAdd.start();
            m_lastAnchorAddPos = event->pos();
        }
        else
        {
            endAddAnchors();
            m_swallowContextMenu = event->button() == Qt::RightButton;
        }
        event->accept();
        return true;
    }
    const FreeStringHandles *h = stringHandles();
    if (h == nullptr)
        return false;
    const int index = h->handleAt(sp);
    if (index < 0)
        return false;
    if (event->button() == Qt::RightButton)
    {
        event->accept();        // its menu, from contextMenuEvent; the selection stays
        return true;
    }
    if (event->button() != Qt::LeftButton)
        return false;
    if (!stringEditable(h->fixtureID()))
    {
        emit dragRefused(m_layoutLocked ? tr("Plot locked — Edit Plot to move")
                                        : tr("Locked — unlock it to move"));
        event->accept();
        return true;
    }
    // Held: the next moves are this handle's (mouseMoveEvent), one undo step.
    m_stringDragMoved = false;
    m_stringDragFid = h->fixtureID();
    m_stringDragIndex = index;
    m_stringDragBefore = m_rigSnapshotter ? m_rigSnapshotter() : QByteArray();
    m_stringDragStartScene = h->handlePos(index);
    event->accept();
    return true;
}

void MonitorGraphicsView::stringHandleMenu(int index, const QPoint &globalPos)
{
    const FreeStringHandles *h = stringHandles();
    if (h == nullptr || index < 0 || index >= h->count())
        return;
    const quint32 fid = h->fixtureID();
    const QVector<StringAnchor> anchors = editableAnchors(fid);
    if (index >= anchors.size())
        return;
    const bool editable = stringEditable(fid);
    const int leds = PixelPathDecor::ledCount(m_doc, fid);
    QMenu menu(this);
    menu.setObjectName(QStringLiteral("stringAnchorMenu"));
    {
        ContextMenu cm(&menu);
        QAction *ins = cm.add(ContextMenu::Kind, tr("Insert Anchor Before"),
                              [this, fid, index]() { insertStringAnchor(fid, index); },
                              QStringLiteral("stringInsertAnchor"));
        ins->setEnabled(editable);
        QAction *pin = cm.add(ContextMenu::Kind, tr("Pin LED Here…"), [this, fid, index, leds]() {
            bool ok = false;
            const int now = editableAnchors(fid).value(index).pin;
            const int led = QInputDialog::getInt(this, tr("Pin LED Here"),
                tr("Which LED is at anchor %1? Counted from the start of the string, the first "
                   "is 1.").arg(index + 1),
                now > 0 ? now : stringLedNearAnchor(fid, index), 1, qMax(1, leds), 1, &ok);
            if (!ok)
                return;
            QVector<StringAnchor> t = editableAnchors(fid);
            t[index].pin = led;
            const QString bad = PixelPath::checkStringAnchors(t, leds);
            if (!bad.isEmpty())
            {
                emit notice(tr("Not pinned: %1").arg(bad));
                return;
            }
            setStringAnchorPin(fid, index, led);
        }, QStringLiteral("stringPinLed"));
        pin->setEnabled(editable);
        QAction *unpin = cm.add(ContextMenu::Kind,
                                anchors.at(index).pin > 0 ? tr("Unpin LED %1").arg(anchors.at(index).pin)
                                                          : tr("Unpin LED"),
                                [this, fid, index]() { setStringAnchorPin(fid, index, 0); },
                                QStringLiteral("stringUnpinLed"));
        unpin->setEnabled(editable && anchors.at(index).pin > 0);
        // A span starts at every anchor but the last.
        QAction *rev = cm.add(ContextMenu::Kind, tr("Reverse This Span"),
                              [this, fid, index]() {
            setStringAnchorReverse(fid, index, !editableAnchors(fid).value(index).reverse);
        }, QStringLiteral("stringReverseSpan"));
        rev->setCheckable(true);
        rev->setChecked(anchors.at(index).reverse);
        rev->setEnabled(editable && index < anchors.size() - 1);
        rev->setToolTip(tr("The span from this anchor to the next runs the other way when its "
                           "segments are written to the WLED"));
        QAction *del = cm.add(ContextMenu::Remove, tr("Delete Anchor"),
                              [this, fid, index]() { deleteStringAnchor(fid, index); },
                              QStringLiteral("stringDeleteAnchor"));
        del->setEnabled(editable);
    }
    menu.exec(globalPos);
}
