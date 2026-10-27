/*
  Q Light Controller Plus
  layoutdiagnostics.cpp

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

#include <QGuiApplication>
#include <QApplication>
#include <QGraphicsView>
#include <QMainWindow>
#include <QStatusBar>
#include <QLayout>
#include <QTabWidget>
#include <QDateTime>
#include <QSplitter>
#include <QMenuBar>
#include <QSysInfo>
#include <QScreen>
#include <QWindow>
#include <QTimer>
#include <QEvent>
#include <QDebug>

#include "layoutdiagnostics.h"

namespace
{
    QStringList &healLog()
    {
        static QStringList log;
        return log;
    }

    QString sz(const QSize &s)
    {
        return QStringLiteral("%1x%2").arg(s.width()).arg(s.height());
    }

    QString rect(const QRect &r)
    {
        return QStringLiteral("%1x%2 at %3,%4").arg(r.width()).arg(r.height()).arg(r.x()).arg(r.y());
    }

    QString windowName(const QWidget *w)
    {
        if (w == nullptr)
            return QStringLiteral("(none)");
        const QString title = w->windowTitle();
        return title.isEmpty() ? QString::fromLatin1(w->metaObject()->className()) : title;
    }
}

/*****************************************************************************
 * WindowSizeGuard
 *****************************************************************************/

WindowSizeGuard::WindowSizeGuard(QWidget *topLevel)
    : QObject(topLevel)
    , m_widget(topLevel)
{
    setObjectName(QStringLiteral("windowSizeGuard"));
    topLevel->installEventFilter(this);
    attachToWindow();
}

WindowSizeGuard *WindowSizeGuard::watch(QWidget *topLevel)
{
    if (topLevel == nullptr)
        return nullptr;
    if (WindowSizeGuard *g = topLevel->findChild<WindowSizeGuard *>(QString(), Qt::FindDirectChildrenOnly))
        return g;
    return new WindowSizeGuard(topLevel);
}

void WindowSizeGuard::attachToWindow()
{
    if (m_widget == nullptr || m_window != nullptr)
        return;
    // The platform window exists once the widget is shown (or its id asked for).
    if (QWindow *w = m_widget->windowHandle())
    {
        m_window = w;
        w->installEventFilter(this);
    }
}

bool WindowSizeGuard::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::Show:
        case QEvent::WinIdChange:
            if (watched == m_widget)
                attachToWindow();
            scheduleCheck();
            break;
        case QEvent::Resize:
        case QEvent::Expose:
        case QEvent::WindowStateChange:
            scheduleCheck();
            break;
        default:
            break;
    }
    return QObject::eventFilter(watched, event);
}

void WindowSizeGuard::scheduleCheck()
{
    // After the events of this resize have run: mid-resize the two can differ.
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(0, this, [this]() {
        m_pending = false;
        checkNow();
    });
}

bool WindowSizeGuard::checkNow()
{
    attachToWindow();
    if (m_widget == nullptr || m_window == nullptr || !m_widget->isVisible()
        || (m_widget->windowState() & Qt::WindowMinimized))
        return false;
    const QSize handle = m_window->size();
    const QSize widget = m_widget->size();
    if (handle == widget || handle.isEmpty())
        return false;

    const QString line = QStringLiteral("%1 %2: the window is %3 but its contents were %4; re-synced and repainted")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate), windowName(m_widget), sz(handle), sz(widget));
    qWarning().noquote() << "Layout self-heal:" << line;
    QStringList &log = healLog();
    log << line;
    while (log.size() > 50)
        log.removeFirst();

    /* The widget takes the platform window's size, lays out again and
       repaints all of it: the stale picture was old content top-left and
       nothing beyond. */
    m_widget->resize(handle);
    if (m_widget->layout() != nullptr)
        m_widget->layout()->activate();
    m_widget->update();
    foreach (QWidget *child, m_widget->findChildren<QWidget *>())
        if (child->isVisible())
            child->update();
    m_widget->repaint();
    return true;
}

QStringList WindowSizeGuard::log()
{
    return healLog();
}

void WindowSizeGuard::clearLog()
{
    healLog().clear();
}

/*****************************************************************************
 * The layout report
 *****************************************************************************/

namespace
{
    void describeTopLevel(QStringList &out, QWidget *w, const QString &indent)
    {
        QWindow *h = w->windowHandle();
        out << indent + QStringLiteral("widget: %1  (min %2, max %3)")
                   .arg(rect(w->geometry()), sz(w->minimumSize()), sz(w->maximumSize()));
        if (h != nullptr)
        {
            const bool same = h->size() == w->size();
            out << indent + QStringLiteral("platform window: %1  %2")
                       .arg(rect(h->geometry()), same ? QStringLiteral("(matches)")
                                                      : QStringLiteral("(DIFFERS from the widget)"));
            out << indent + QStringLiteral("device pixel ratio %1, screen %2, state 0x%3, %4")
                       .arg(h->devicePixelRatio())
                       .arg(h->screen() != nullptr ? h->screen()->name() : QStringLiteral("?"))
                       .arg(int(w->windowState()), 0, 16)
                       .arg(w->isVisible() ? QStringLiteral("visible") : QStringLiteral("hidden"));
        }
        else
        {
            out << indent + QStringLiteral("platform window: none yet");
        }

        QMainWindow *mw = qobject_cast<QMainWindow *>(w);
        QWidget *central = mw != nullptr ? mw->centralWidget() : nullptr;
        if (mw != nullptr)
        {
            if (mw->menuWidget() != nullptr && mw->menuWidget()->isVisible())
                out << indent + QStringLiteral("menu bar: %1").arg(rect(mw->menuWidget()->geometry()));
            if (mw->statusBar() != nullptr && mw->statusBar()->isVisible())
                out << indent + QStringLiteral("status bar: %1").arg(rect(mw->statusBar()->geometry()));
        }
        if (central == nullptr)
            return;
        out << indent + QStringLiteral("central (%1): %2")
                   .arg(central->objectName().isEmpty() ? QString::fromLatin1(central->metaObject()->className())
                                                        : central->objectName(),
                        rect(central->geometry()));

        QWidget *page = central;
        if (QTabWidget *tabs = central->findChild<QTabWidget *>(QStringLiteral("MainTabs")))
        {
            out << indent + QStringLiteral("tabs: %1, %2 tabs, current %3 \"%4\"")
                       .arg(rect(tabs->geometry())).arg(tabs->count()).arg(tabs->currentIndex())
                       .arg(tabs->tabText(tabs->currentIndex()));
            for (int i = 0; i < tabs->count(); i++)
            {
                QWidget *p = tabs->widget(i);
                out << indent + QStringLiteral("  tab %1 \"%2\": page %3%4")
                           .arg(i).arg(tabs->tabText(i))
                           .arg(p != nullptr ? rect(p->geometry()) : QStringLiteral("(none)"))
                           .arg(p != nullptr && p->isVisible() ? QStringLiteral(" (shown)") : QString());
            }
            page = tabs->currentWidget();
        }
        if (page == nullptr)
            return;
        out << indent + QStringLiteral("page (%1): %2, size hint %3, minimum %4")
                   .arg(QString::fromLatin1(page->metaObject()->className()), rect(page->geometry()),
                        sz(page->sizeHint()), sz(page->minimumSizeHint()));
        foreach (QSplitter *sp, page->findChildren<QSplitter *>())
        {
            if (!sp->isVisible())
                continue;
            QStringList sizes;
            foreach (int s, sp->sizes())
                sizes << QString::number(s);
            out << indent + QStringLiteral("  splitter %1: %2, panes %3")
                       .arg(sp->objectName().isEmpty() ? QStringLiteral("(unnamed)") : sp->objectName(),
                            rect(sp->geometry()), sizes.join(QStringLiteral(" | ")));
        }
        foreach (QGraphicsView *gv, page->findChildren<QGraphicsView *>())
        {
            if (!gv->isVisible())
                continue;
            out << indent + QStringLiteral("  %1 view: %2, viewport %3")
                       .arg(QString::fromLatin1(gv->metaObject()->className()), rect(gv->geometry()),
                            sz(gv->viewport()->size()));
        }
    }
}

QString LayoutDiagnostics::report(QWidget *mainWindow)
{
    QStringList out;
    out << QStringLiteral("qlcconsole layout report, %1")
               .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    out << QStringLiteral("Qt %1 (built with %2), platform %3, %4")
               .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR),
                    QGuiApplication::platformName(), QSysInfo::prettyProductName());
    out << QString();
    out << QStringLiteral("Screens:");
    foreach (QScreen *s, QGuiApplication::screens())
        out << QStringLiteral("  %1: %2, available %3, device pixel ratio %4%5")
                   .arg(s->name(), rect(s->geometry()), rect(s->availableGeometry()))
                   .arg(s->devicePixelRatio())
                   .arg(s == QGuiApplication::primaryScreen() ? QStringLiteral(" (primary)") : QString());

    if (mainWindow != nullptr)
    {
        out << QString();
        out << QStringLiteral("Main window \"%1\":").arg(windowName(mainWindow));
        describeTopLevel(out, mainWindow, QStringLiteral("  "));
    }

    // Tabs moved into their own windows, and any other window that is up.
    bool any = false;
    foreach (QWidget *w, QApplication::topLevelWidgets())
    {
        if (w == mainWindow || !w->isVisible() || qobject_cast<QMainWindow *>(w) == nullptr)
            continue;
        if (!any)
        {
            out << QString();
            out << QStringLiteral("Other windows:");
            any = true;
        }
        out << QStringLiteral("  \"%1\":").arg(windowName(w));
        describeTopLevel(out, w, QStringLiteral("    "));
    }
    if (!any)
    {
        out << QString();
        out << QStringLiteral("Other windows: none");
    }

    out << QString();
    const QStringList healed = WindowSizeGuard::log();
    if (healed.isEmpty())
        out << QStringLiteral("Self-heal: nothing healed this session");
    else
    {
        out << QStringLiteral("Self-heal, %1 time(s):").arg(healed.size());
        foreach (const QString &l, healed)
            out << QStringLiteral("  ") + l;
    }
    return out.join(QLatin1Char('\n')) + QLatin1Char('\n');
}
