/*
  Q Light Controller Plus
  patchlock.cpp

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

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QBoxLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QEvent>
#include <QApplication>

#include "patchlock.h"

namespace
{
const char *kLockedProp = "patchLockSaved";       // the control's state before we greyed it
const char *kTipProp = "patchLockTip";
const char *kEditProp = "patchLockEdit";          // a view's edit triggers before
const char *kDragProp = "patchLockDrag";

/* Controls that only change what is shown, not the patch. */
bool isViewOnly(const QObject *o)
{
    static const QStringList names = {
        QStringLiteral("connectionsShowUnused"), QStringLiteral("connectionsByUniverse"),
        QStringLiteral("connectionsColumns"), QStringLiteral("connectionsTest"),
        QStringLiteral("patchLockKeep") };
    return names.contains(o->objectName()) || o->property("patchLockKeep").toBool();
}

/* A menu item that reads, not edits. */
bool menuItemIsReadOnly(const QAction *a)
{
    static const QStringList starts = {
        QStringLiteral("Collapse"), QStringLiteral("Expand"), QStringLiteral("Patch List"),
        QStringLiteral("Check Mapping"), QStringLiteral("Test Controller"),
        QStringLiteral("Select"), QStringLiteral("Find"), QStringLiteral("Locate"),
        QStringLiteral("Show"), QStringLiteral("Copy") };
    const QString t = QString(a->text()).remove(QLatin1Char('&')).trimmed();
    for (const QString &s : starts)
        if (t.startsWith(s))
            return true;
    return false;
}

/* Which page a widget belongs to, for the application-wide menu filter. */
QList<PatchLock *> &all()
{
    static QList<PatchLock *> l;
    return l;
}

class MenuGuard : public QObject
{
public:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() != QEvent::Show || !o->isWidgetType())
            return false;
        QWidget *w = static_cast<QWidget *>(o);
        const bool isMenu = qobject_cast<QMenu *>(w) != nullptr;
        for (PatchLock *l : all())
            if (l->isLocked())
            {
                if (isMenu)
                    l->lockMenu(w);
                else
                    l->lockLate(w);      // a control made after the page was locked
            }
        return false;
    }
};
}

PatchLock::PatchLock(QWidget *page, Doc *doc)
    : QObject(page)
    , m_page(page)
    , m_doc(doc)
    , m_bar(new QWidget(page))
    , m_design(nullptr)
    , m_text(nullptr)
    , m_locked(false)
{
    setObjectName(QStringLiteral("patchLock"));
    m_bar->setObjectName(QStringLiteral("patchLockBar"));
    m_bar->setProperty("patchLockKeep", true);
    m_bar->setAutoFillBackground(true);
    QPalette pal = m_bar->palette();
    pal.setColor(QPalette::Window, QColor(0x5a, 0x45, 0x10));
    pal.setColor(QPalette::WindowText, QColor(0xff, 0xf0, 0xc8));
    m_bar->setPalette(pal);
    QHBoxLayout *lay = new QHBoxLayout(m_bar);
    lay->setContentsMargins(8, 2, 8, 2);
    m_text = new QLabel(QString::fromUtf8("\xF0\x9F\x94\x92 ") + sentence(), m_bar);
    m_text->setObjectName(QStringLiteral("patchLockText"));
    m_text->setForegroundRole(QPalette::WindowText);
    lay->addWidget(m_text, 1);
    m_design = new QPushButton(tr("Design"), m_bar);
    m_design->setObjectName(QStringLiteral("patchLockDesign"));
    m_design->setToolTip(tr("Go to Design mode (asks for the show's PIN when it has one)."));
    connect(m_design, &QPushButton::clicked, this, &PatchLock::designRequested);
    lay->addWidget(m_design);
    m_bar->hide();

    if (QBoxLayout *box = qobject_cast<QBoxLayout *>(page->layout()))
        box->insertWidget(0, m_bar);

    page->setProperty("patchLock", QVariant::fromValue(static_cast<QObject *>(this)));
    all() << this;
    connect(this, &QObject::destroyed, this, [this]() { all().removeAll(this); });
    static MenuGuard *guard = nullptr;
    if (guard == nullptr)
    {
        guard = new MenuGuard;
        qApp->installEventFilter(guard);
    }
    if (doc != nullptr)
        connect(doc, &Doc::modeChanged, this, [this]() { refresh(); });
}

PatchLock *PatchLock::attach(QWidget *page, Doc *doc)
{
    if (page == nullptr)
        return nullptr;
    if (PatchLock *have = of(page))
        return have;
    PatchLock *l = new PatchLock(page, doc);
    l->refresh();
    return l;
}

PatchLock *PatchLock::of(const QWidget *page)
{
    return page == nullptr ? nullptr
                           : qobject_cast<PatchLock *>(page->property("patchLock").value<QObject *>());
}

QString PatchLock::sentence()
{
    return tr("Operate: the patch is locked. Design to change it");
}

QIcon PatchLock::lockedIcon(const QIcon &icon)
{
    QIcon out;
    for (const QSize &sz : { QSize(16, 16), QSize(24, 24), QSize(32, 32), QSize(48, 48) })
    {
        QPixmap pm = icon.pixmap(sz);
        if (pm.isNull())
            continue;
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const int w = pm.width() / 2;
        const QRectF body(pm.width() - w, pm.height() - w * 0.62, w * 0.9, w * 0.55);
        p.setPen(QPen(Qt::black, qMax(1.0, w / 8.0)));
        p.setBrush(QColor(0xf0, 0xb0, 0x20));
        p.drawRect(body);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(body.left() + body.width() * 0.2, body.top() - body.height() * 0.8,
                         body.width() * 0.6, body.height() * 1.2), 0, 180 * 16);
        out.addPixmap(pm);
    }
    return out.isNull() ? icon : out;
}

bool PatchLock::isLocked() const
{
    return m_locked;
}

void PatchLock::refresh()
{
    const bool locked = m_doc != nullptr && m_doc->mode() == Doc::Operate;
    m_locked = locked;
    m_bar->setVisible(locked);
    applyTo(locked);
}

void PatchLock::applyTo(bool locked)
{
    if (m_page == nullptr)
        return;
    const QList<QWidget *> widgets = m_page->findChildren<QWidget *>();
    for (QWidget *w : widgets)
        lockOne(w, locked);
    // The actions (toolbar buttons, menus and shortcuts follow them).
    const QList<QAction *> actions = m_page->findChildren<QAction *>();
    for (QAction *a : actions)
        lockAction(a, locked);
}

void PatchLock::lockAction(QAction *a, bool locked)
{
    if (a->isSeparator() || a->menu() != nullptr || isViewOnly(a) || menuItemIsReadOnly(a))
        return;
    if (locked)
    {
        if (!a->property(kLockedProp).isValid())
        {
            a->setProperty(kLockedProp, a->isEnabled());
            a->setProperty(kTipProp, a->toolTip());
        }
        a->setEnabled(false);
        a->setToolTip(sentence());
    }
    else if (a->property(kLockedProp).isValid())
    {
        const bool was = a->property(kLockedProp).toBool();
        const QString t = a->property(kTipProp).toString();
        a->setProperty(kLockedProp, QVariant());
        a->setProperty(kTipProp, QVariant());
        a->setToolTip(t);
        // Design's own state wins (the page set it): only a lock-time enabled
        // action that is still off comes back.
        if (was && !a->isEnabled())
            a->setEnabled(true);
    }
}

/* One control, idempotent: what it was is kept in a property and put back
   in Design. */
void PatchLock::lockOne(QWidget *w, bool locked)
{
    if (w == nullptr)
        return;
    const QString tip = sentence();
    auto lock = [&](QObject *o, bool wasEnabled, const QString &oldTip, auto setEnabled, auto setTip) {
        if (locked)
        {
            if (!o->property(kLockedProp).isValid())
            {
                o->setProperty(kLockedProp, wasEnabled);
                o->setProperty(kTipProp, oldTip);
            }
            setEnabled(false);
            setTip(tip);
        }
        else if (o->property(kLockedProp).isValid())
        {
            const bool was = o->property(kLockedProp).toBool();
            const QString t = o->property(kTipProp).toString();
            o->setProperty(kLockedProp, QVariant());
            o->setProperty(kTipProp, QVariant());
            setTip(t);
            setEnabled(was);
        }
    };

    for (QObject *p = w; p != nullptr; p = p->parent())
        if (p == m_bar)
            return;
    if (isViewOnly(w))
        return;
    QWidget *par = w->parentWidget();
    if (qobject_cast<QAbstractButton *>(w) != nullptr || qobject_cast<QAbstractSpinBox *>(w) != nullptr
        || qobject_cast<QComboBox *>(w) != nullptr || qobject_cast<QLineEdit *>(w) != nullptr)
    {
        // A tool button of a toolbar is its action's (below); a spin box's or
        // combo's own line edit goes with it.
        if (QToolButton *tbtn = qobject_cast<QToolButton *>(w))
            if (tbtn->defaultAction() != nullptr)
                return;
        if (qobject_cast<QLineEdit *>(w) != nullptr
            && (qobject_cast<QAbstractSpinBox *>(par) != nullptr || qobject_cast<QComboBox *>(par) != nullptr))
            return;
        lock(w, w->isEnabled(), w->toolTip(),
             [w](bool on) { w->setEnabled(on); }, [w](const QString &t) { w->setToolTip(t); });
    }
    else if (QAbstractItemView *v = qobject_cast<QAbstractItemView *>(w))
    {
        if (locked)
        {
            if (!v->property(kEditProp).isValid())
            {
                v->setProperty(kEditProp, int(v->editTriggers()));
                v->setProperty(kDragProp, int(v->dragDropMode()));
            }
            v->setEditTriggers(QAbstractItemView::NoEditTriggers);
            v->setDragDropMode(QAbstractItemView::NoDragDrop);
        }
        else if (v->property(kEditProp).isValid())
        {
            v->setEditTriggers(QAbstractItemView::EditTriggers(v->property(kEditProp).toInt()));
            v->setDragDropMode(QAbstractItemView::DragDropMode(v->property(kDragProp).toInt()));
            v->setProperty(kEditProp, QVariant());
            v->setProperty(kDragProp, QVariant());
        }
    }
}

/* A menu opened on the page: its editing items greyed, the sentence as their
   tooltip. Invoked by the application-wide filter as the menu shows. */
void PatchLock::lockMenu(QObject *o)
{
    QMenu *menu = qobject_cast<QMenu *>(o);
    if (menu == nullptr || m_page == nullptr || !m_locked)
        return;
    QWidget *pw = menu->parentWidget();
    if (pw == nullptr || (pw != m_page && !m_page->isAncestorOf(pw)))
        return;
    const QString tip = sentence();
    const QList<QAction *> actions = menu->actions();
    for (QAction *a : actions)
    {
        if (a->isSeparator() || menuItemIsReadOnly(a))
            continue;
        a->setEnabled(false);
        a->setToolTip(tip);
    }
}

void PatchLock::lockLate(QWidget *w)
{
    QWidget *pw = w->parentWidget();
    if (m_page == nullptr || !m_locked || pw == nullptr || (w != m_page && !m_page->isAncestorOf(w)))
        return;
    lockOne(w, true);
}
