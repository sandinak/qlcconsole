/*
  Q Light Controller Plus - Test Unit
  windowsanity_test.cpp

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

#include <QtTest>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <functional>

// The application's own event filters are counted: a window must not leave
// one behind (the rig-view lock is one, for as long as its editor is open).
#include <QtCore/private/qobject_p.h>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "monitorgraphicsview.h"
#include "venuecorrections.h"
#undef protected
#undef private

#include "doc.h"
#include "fixture.h"
#include "programmercontroller.h"
#include "monitorproperties.h"
#include "truss.h"
#include "tower.h"
#include "stageplatform.h"
#include "pipe.h"
#include "qlcfixturedefcache.h"
#include "qlcfile.h"
#include "effectscriptcache.h"

#include "blockingwindowbanner.h"
#include "windowspace.h"
#include "focuscallsdialog.h"
#include "venuerigdialog.h"
#include "venuefromshowdialog.h"
#include "preshowcheck.h"
#include "showreviewpanel.h"
#include "pmjlamplogdialog.h"

#include "windowsanity_test.h"

namespace
{

/****************************************************************************
 * The screens: three side by side, a laptop, a 15" and a desktop
 ****************************************************************************/

const char *const kScreensJson =
    "{ \"screens\": ["
    "  { \"name\": \"laptop\",  \"x\": 0,    \"y\": 0, \"width\": 1024, \"height\": 640,  \"logicalDpi\": 96 },"
    "  { \"name\": \"medium\",  \"x\": 1024, \"y\": 0, \"width\": 1440, \"height\": 900,  \"logicalDpi\": 96 },"
    "  { \"name\": \"desktop\", \"x\": 2464, \"y\": 0, \"width\": 1920, \"height\": 1080, \"logicalDpi\": 96 }"
    "] }";

// How long a window has to show up, once asked for.
const int kOpenTimeout = 4000;

QList<QScreen *> sizeScreens()
{
    QList<QScreen *> list = QGuiApplication::screens();
    std::sort(list.begin(), list.end(), [](QScreen *a, QScreen *b) {
        return a->geometry().width() < b->geometry().width();
    });
    return list;
}

QString sizeName(const QScreen *s)
{
    return QStringLiteral("%1x%2").arg(s->geometry().width()).arg(s->geometry().height());
}

void settle(int rounds = 3)
{
    for (int i = 0; i < rounds; ++i)
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
}

int appFilterCount()
{
    QObjectPrivate *d = QObjectPrivate::get(qApp);
    if (d->extraData == nullptr)
        return 0;
    int n = 0;
    for (const QPointer<QObject> &f : d->extraData->eventFilters)
        if (!f.isNull())
            ++n;
    return n;
}

/** @p w is the window in front with the keyboard. The offscreen platform
 *  can leave QApplication's active window unset when a hidden window is
 *  shown again while it still is the focus window: either counts. */
bool isActive(const QWidget *w)
{
    return QApplication::activeWindow() == w
        || (w->windowHandle() != nullptr && QGuiApplication::focusWindow() == w->windowHandle());
}

QString describe(const QWidget *w)
{
    if (w == nullptr)
        return QStringLiteral("(none)");
    const QString title = w->windowTitle().isEmpty() ? QStringLiteral("untitled") : w->windowTitle();
    return QStringLiteral("%1 \"%2\"").arg(QString::fromLatin1(w->metaObject()->className()), title);
}

/** Puts the main window on @p s, filling its room, as a person working on
 *  that screen has it. */
bool placeMain(App *app, QScreen *s)
{
    app->showNormal();
    if (app->windowHandle() != nullptr && app->windowHandle()->screen() != s)
        app->windowHandle()->setScreen(s);
    app->setGeometry(s->availableGeometry());
    settle();
    app->activateWindow();
    settle();
    return app->screen() == s;
}

/** The Lighting Studio tab, current, and its Monitor. */
Monitor *studio(App *app)
{
    for (int i = 0; i < app->m_tab->count(); ++i)
        if (app->m_tab->tabBar()->tabData(i).toString() == QLatin1String("Lighting Studio"))
        {
            app->m_tab->setCurrentIndex(i);
            break;
        }
    settle();
    return Monitor::instance();
}

QList<quint32> someFixtures(Doc *doc, int n)
{
    QList<quint32> ids;
    for (Fixture *f : doc->fixtures())
    {
        ids << f->id();
        if (ids.size() == n)
            break;
    }
    return ids;
}

/** A menu or toolbar item under @p root by its text ("&" and "…" aside),
 *  triggered as a click on it would. */
bool trigger(QObject *root, const QString &text)
{
    auto plain = [](QString t) {
        t.remove(QLatin1Char('&'));
        t.remove(QChar(0x2026));
        t.remove(QStringLiteral("..."));
        return t.trimmed();
    };
    QList<QAction *> actions = root->findChildren<QAction *>();
    // A menu made for a tool button need not be the root's child.
    for (QWidget *w : QApplication::allWidgets())
        if (QMenu *m = qobject_cast<QMenu *>(w))
            for (QWidget *p = m->parentWidget(); p != nullptr; p = p->parentWidget())
                if (p == root)
                {
                    actions << m->actions();
                    break;
                }
    if (QWidget *rw = qobject_cast<QWidget *>(root))
        actions << rw->actions();
    if (QMenu *rm = qobject_cast<QMenu *>(root))
        for (QAction *a : rm->actions())
            if (a->menu() != nullptr)
                actions << a->menu()->actions();
    for (QAction *a : actions)
        if (plain(a->text()) == plain(text) && a->isEnabled())
        {
            a->trigger();
            return true;
        }
    return false;
}

/****************************************************************************
 * The rows
 ****************************************************************************/

enum RowFlag
{
    /** Not a dialog: closed as the window system closes a window, not by Esc. */
    ClosesByWindow = 1 << 0,
    /** A free window (not a dialog) that reopens where it was put, on any
     *  screen: it must fit there, not on the main window's screen. */
    FreeWindow = 1 << 1,
    /** Puts itself back where the person last had it (saved geometry): on
     *  that screen if it is still there, the person's choice. It must be
     *  wholly on a screen; the main window's only the first time. */
    RemembersPlace = 1 << 2,
};

struct Row
{
    /** The class (a QDialog subclass in ui/src), or "<file under ui/src>#<what>"
     *  for a QDialog built in place: what platforms/linux/check-window-rows.sh
     *  reads to know a dialog has a row. */
    const char *key;
    QString name;
    std::function<void(App *)> open;
    int flags = 0;
    /** What it holds still while it is open (the rig view), or null. */
    std::function<QWidget *(App *)> held = nullptr;
};

Row row(const char *key, const QString &name, std::function<void(App *)> open, int flags = 0,
        std::function<QWidget *(App *)> held = nullptr)
{
    Row r;
    r.key = key;
    r.name = name;
    r.open = std::move(open);
    r.flags = flags;
    r.held = std::move(held);
    return r;
}

QWidget *heldPlot(App *)
{
    Monitor *m = Monitor::instance();
    return m != nullptr ? static_cast<QWidget *>(m->m_graphicsView) : nullptr;
}

/* The structures the Lighting Studio editors open on, and a venue. */
quint32 g_truss = 0, g_tower = 0, g_deck = 0, g_pipe = 0;
QString g_venue;

/* One row per window. A new dialog: one row here, opened the way the app
   opens it (its slot, its menu item, its static open()), key = its class
   (or "<file>#<what>" for one built in place). */
QList<Row> windows()
{
    QList<Row> list;

    // ---- The main window: menus and the status bar ----
    list << row("AboutBox", QStringLiteral("About"), [](App *a) { a->slotHelpAbout(); })
         << row("AppSettings", QStringLiteral("Settings"), [](App *a) { a->slotAppSettings(); })
         << row("AppSettings", QStringLiteral("Settings, rig network"), [](App *a) { a->showRigNetworkSettings(); })
         << row("ShowSettingsDialog", QStringLiteral("Show Settings"), [](App *a) { a->openShowSettings(); })
         << row("UnsavedChangesDialog", QStringLiteral("Unsaved changes"),
                [](App *a) { a->slotShowUnsavedChanges(); })
         << row("AddressTool", QStringLiteral("Address tool"), [](App *a) { a->slotAddressTool(); }, RemembersPlace)
         << row("DmxMonitorWindow", QStringLiteral("DMX monitor"), [](App *a) { a->slotDmxMonitor(); },
                ClosesByWindow | FreeWindow)
         << row("BugReportDialog", QStringLiteral("Report a Bug"), [](App *a) { a->reportBug(); })
         << row("TimecodeCalibrationDialog", QStringLiteral("Timecode calibration"),
                [](App *a) { a->openTimecodeCalibration(); })
         << row("ShowStatusDialog", QStringLiteral("Show status"), [](App *a) { a->showStatusDetails(); })
         << row("DmxDumpFactory", QStringLiteral("Dump DMX"), [](App *a) { a->slotDumpDmxIntoFunction(); }, RemembersPlace)
         << row("FunctionSelection", QStringLiteral("Live edit, pick a function"),
                [](App *a) { a->slotFunctionLiveEdit(); }, RemembersPlace)
         << row("app.cpp#Tabs & Features", QStringLiteral("Tabs and Features"),
                [](App *a) { a->slotChooseTabs(); })
         << row("DefinitionUpdatesDialog", QStringLiteral("Definition Updates"),
                [](App *a) { QVERIFY(trigger(a->menuBar(), QStringLiteral("Definition Updates…"))); })
         << row("PreshowCheck", QStringLiteral("Preshow check"),
                [](App *a) { PreshowCheck::openFor(a->doc(), a); })
         << row("ShowReviewPanel", QStringLiteral("Show Review"), [](App *a) {
                ShowReviewPanel::setShared(a->doc(), a);
                ShowReviewPanel::openShared();
            });

    // ---- The rig view (Lighting Studio) ----
    list << row("monitor/monitor.cpp#Fixture Properties", QStringLiteral("Fixture Properties"),
                [](App *a) {
                    const QList<quint32> ids = someFixtures(a->doc(), 1);
                    if (!ids.isEmpty())
                        studio(a)->showFixtureItemEditor(ids.first());
                }, 0, heldPlot)
         << row("monitor/monitor.cpp#Fixture Properties", QStringLiteral("Fixture Properties, several"),
                [](App *a) { studio(a)->showFixtureItemEditor(someFixtures(a->doc(), 3)); }, 0, heldPlot)
         << row("monitor/monitor.cpp#Tower", QStringLiteral("Tower"),
                [](App *a) { studio(a)->slotEditTower(g_tower); })
         << row("monitor/monitor.cpp#Truss", QStringLiteral("Truss"),
                [](App *a) { studio(a)->slotEditTruss(g_truss); })
         << row("monitor/monitor.cpp#Platform", QStringLiteral("Platform"),
                [](App *a) { studio(a)->slotEditPlatform(g_deck); })
         << row("monitor/monitor.cpp#Pipe", QStringLiteral("Pipe"),
                [](App *a) { studio(a)->slotEditPipe(g_pipe); })
         << row("monitor/monitor.cpp#Manage Trusses", QStringLiteral("Manage Trusses"),
                [](App *a) { studio(a)->slotManageTrusses(); })
         << row("NetworkReportDialog", QStringLiteral("Network Report"),
                [](App *a) { studio(a)->slotNetworkReport(); })
         << row("NetworkTopologyDialog", QStringLiteral("Network Topology"),
                [](App *a) { studio(a)->slotEditNetworkNode(0); })
         << row("PatchListDialog", QStringLiteral("Patch List"),
                [](App *a) {
                    // The plot's More menu (a tool button's menu).
                    Monitor *m = studio(a);
                    QVERIFY(m->m_moreBtn != nullptr && m->m_moreBtn->menu() != nullptr);
                    QVERIFY(trigger(m->m_moreBtn->menu(), QStringLiteral("Patch List…")));
                })
         << row("PowerDistributionDialog", QStringLiteral("Power distribution"),
                [](App *a) { emit studio(a)->m_graphicsView->powerDoubleClicked(0); }, RemembersPlace)
         << row("MonitorBackgroundSelection", QStringLiteral("Background"),
                [](App *a) { studio(a)->slotSetBackground(); }, RemembersPlace)
         << row("FixtureSelection", QStringLiteral("Add fixture to the plot"),
                [](App *a) { studio(a)->slotAddFixture(); }, RemembersPlace);

    // ---- Focus calls and venues ----
    list << row("FocusCallsDialog", QStringLiteral("Focus calls"),
                [](App *a) { FocusCallsDialog::run(a->doc(), a); })
         << row("monitor/focuscallsdialog.cpp#Venue", QStringLiteral("Venue, still here"), [](App *a) {
                // Applied, last changed two days ago, not yet confirmed.
                VenueCorrections *v = a->doc()->programmer()->venueCorrections();
                v->setActiveVenue(g_venue);
                for (VenueCorrections::Venue &venue : v->m_venues)
                    if (venue.name == g_venue)
                        venue.created = venue.modified = QDateTime::currentDateTime().addDays(-2);
                v->m_confirmed = false;
                FocusCallsDialog::confirmVenue(a->doc(), a);
            })
         << row("VenueRigDialog", QStringLiteral("Venue rig"),
                [](App *a) { VenueRigDialog::open(a->doc(), a, g_venue); })
         << row("VenueFromShowDialog", QStringLiteral("New venue from a show"),
                [](App *a) { VenueFromShowDialog::open(a->doc(), a); })
         // Connections ▸ Control surfaces ▸ PMJ Black 1 ▸ Lamp Log… (ControlSurfacesPanel::openPmjLampLog()).
         << row("PmjLampLogDialog", QStringLiteral("PMJ lamp log"), [](App *a) {
                PmjLampLogDialog *d = new PmjLampLogDialog(a->m_pmjOverlay, a);
                d->setAttribute(Qt::WA_DeleteOnClose);
                d->show();
            });

    return list;
}

/** Known window problems, each with its TODO line: reported, not failed.
 *  A row no window hits any more is called out at the end, to be removed.
 *  match: row name prefix, size ("" = any) and a substring of the finding. */
struct Known
{
    const char *row;
    const char *size;
    const char *finding;
    const char *todo;
};

const QList<Known> &knownIssues()
{
    /* Empty: Fixture Properties' overflow (1046 px tall at 1440x900) was
       the last, fixed on fix/053-fixture-props-fit (its rows scroll). */
    static const QList<Known> list = {};
    return list;
}

QStringList g_seenKnown;

/****************************************************************************
 * The checker
 ****************************************************************************/

/** A click target in @p w that the window really hands a click to: a
 *  button (Cancel first), else a field. */
QWidget *clickTarget(QWidget *w, bool *isButton)
{
    QList<QWidget *> order;
    for (QDialogButtonBox *bb : w->findChildren<QDialogButtonBox *>())
        for (QAbstractButton *b : bb->buttons())
        {
            if (bb->buttonRole(b) == QDialogButtonBox::RejectRole)
                order.prepend(b);
            else
                order << b;
        }
    for (QPushButton *b : w->findChildren<QPushButton *>())
        if (!order.contains(b))
            order << b;
    for (QAbstractButton *b : w->findChildren<QAbstractButton *>())
        if (!order.contains(b))
            order << b;
    const int buttons = int(order.size());
    for (QLineEdit *e : w->findChildren<QLineEdit *>())
        if (qobject_cast<QAbstractSpinBox *>(e->parentWidget()) == nullptr)
            order << e;
    for (QAbstractSpinBox *s : w->findChildren<QAbstractSpinBox *>())
        order << s;
    for (int i = 0; i < order.size(); ++i)
    {
        QWidget *c = order.at(i);
        if (!c->isVisible() || !c->isEnabled() || c->window() != w)
            continue;
        const QRect vis = c->visibleRegion().boundingRect();
        if (vis.isEmpty())
            continue;
        const QPoint at = c->mapTo(w, vis.center());
        QWidget *hit = w->childAt(at);
        if (hit != c && !c->isAncestorOf(hit))
            continue;
        *isButton = i < buttons;
        return c;
    }
    return nullptr;
}

/** Counts the presses an object gets. */
class PressCounter final : public QObject
{
public:
    int presses = 0;

protected:
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::MouseButtonPress)
            ++presses;
        return false;
    }
};

/** The window's input, the way a real screen hands it over: to the window
 *  first. A press on a button through the window holds it down (let go off
 *  it: nothing is clicked); a click on a field gives it the keyboard. */
void checkInputThroughWindow(QWidget *w, QStringList &problems)
{
    QWindow *win = w->windowHandle();
    bool isButton = false;
    QWidget *c = clickTarget(w, &isButton);
    if (c == nullptr)
    {
        /* No button or field (a monitor, a view): whatever is under its
           middle must get the press. An object filter runs after every
           application one, so it sees only what got through. */
        const QPoint mid = w->rect().center();
        QWidget *under = w->childAt(mid) != nullptr ? w->childAt(mid) : w;
        PressCounter counter;
        under->installEventFilter(&counter);
        QTest::mousePress(win, Qt::LeftButton, Qt::NoModifier, mid);
        QTest::mouseRelease(win, Qt::LeftButton, Qt::NoModifier, mid);
        under->removeEventFilter(&counter);
        if (counter.presses == 0)
            problems << QStringLiteral("a press sent through its window did not reach what is under it (%1)")
                            .arg(QString::fromLatin1(under->metaObject()->className()));
        return;
    }
    const QRect vis = c->visibleRegion().boundingRect();
    const QPoint at = c->mapTo(w, vis.center());
    if (isButton)
    {
        QAbstractButton *b = static_cast<QAbstractButton *>(c);
        QTest::mousePress(win, Qt::LeftButton, Qt::NoModifier, at);
        const bool down = b->isDown();
        QTest::mouseRelease(win, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
        if (b->isDown())
            b->setDown(false);
        if (!down)
            problems << QStringLiteral("a press sent through its window did not reach \"%1\"")
                            .arg(b->text().remove(QLatin1Char('&')));
        return;
    }
    if (c->hasFocus())
        c->clearFocus();
    QTest::mouseClick(win, Qt::LeftButton, Qt::NoModifier, at);
    QWidget *f = QApplication::focusWidget();
    if (f != c && !c->isAncestorOf(f))
        problems << QStringLiteral("a click sent through its window did not reach its field %1")
                        .arg(c->objectName().isEmpty() ? QString::fromLatin1(c->metaObject()->className())
                                                       : c->objectName());
}

/** Out of sight while it holds something: the held area says so, and its
 *  Show It, clicked through the main window, brings it back in front. */
void checkHiddenBlocker(App *app, QWidget *w, QWidget *held, QStringList &problems)
{
    if (held == nullptr || !held->isVisible())
    {
        problems << QStringLiteral("what it holds is not on the screen to check");
        return;
    }
    w->hide();
    settle();
    BlockingWindowBanner *b = BlockingWindowBanner::bannerOn(held);
    if (b == nullptr || !b->isVisible())
    {
        problems << QStringLiteral("hidden while it holds a view, and nothing there says it is open");
        w->show();
        settle();
        return;
    }
    const QRect inHeld(b->mapTo(held, QPoint(0, 0)), b->size());
    if (!held->rect().contains(inHeld))
        problems << QStringLiteral("its banner runs off what it holds");
    QPushButton *show = b->showButton();
    QTest::mouseClick(app->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                      show->mapTo(app, show->rect().center()));
    settle();
    if (!w->isVisible())
    {
        problems << QStringLiteral("\"Show It\", clicked through the main window, did not bring it back");
        w->show();
        settle();
        return;
    }
    if (!isActive(w))
        problems << QStringLiteral("brought back, but not in front (active: %1)")
                        .arg(describe(QApplication::activeWindow()));
    if (w->screen() != app->screen())
        problems << QStringLiteral("brought back onto another screen than its main window's");
}

QStringList checkOpen(App *app, const Row &r, QWidget *w)
{
    QStringList problems;
    settle();
    if (w->windowHandle() == nullptr || !QTest::qWaitForWindowExposed(w->windowHandle(), 1000))
    {
        problems << QStringLiteral("never shown on the screen");
        return problems;
    }

    // It belongs to the main window: a parent widget, or a transient parent.
    bool owned = false;
    for (QWidget *p = w->parentWidget(); p != nullptr && !owned; p = p->parentWidget())
        owned = (p == app);
    for (QWindow *t = w->windowHandle()->transientParent(); t != nullptr && !owned; t = t->transientParent())
        owned = (t == app->windowHandle());
    if (!owned)
        problems << QStringLiteral("no parent: it does not belong to the main window (it can open "
                                   "behind it, on another screen or Space)");

    // On the main window's screen, and wholly on it.
    const bool ownPlace = (r.flags & (FreeWindow | RemembersPlace)) != 0;
    if (w->screen() != app->screen() && !ownPlace)
        problems << QStringLiteral("opened on screen %1, its main window is on %2")
                        .arg(w->screen()->name(), app->screen()->name());
    const QRect room = (ownPlace ? w->screen() : app->screen())->availableGeometry();
    const QRect frame = w->frameGeometry();
    if (!room.contains(frame))
        problems << QStringLiteral("runs off its screen: %1x%2 at (%3,%4), the screen has %5x%6 at (%7,%8)")
                        .arg(frame.width()).arg(frame.height()).arg(frame.x()).arg(frame.y())
                        .arg(room.width()).arg(room.height()).arg(room.x()).arg(room.y());
    for (QDialogButtonBox *bb : w->findChildren<QDialogButtonBox *>())
        if (bb->isVisible() && bb->window() == w)
        {
            const QRect g(bb->mapToGlobal(QPoint(0, 0)), bb->size());
            if (!room.contains(g) || !w->rect().contains(QRect(bb->mapTo(w, QPoint(0, 0)), bb->size())))
                problems << QStringLiteral("its OK / Cancel are out of view");
        }

    // In front while it holds anything.
    if (r.held && !isActive(w))
        problems << QStringLiteral("holds a view but is not in front (active: %1)")
                        .arg(describe(QApplication::activeWindow()));

    checkInputThroughWindow(w, problems);

    if (r.held)
        checkHiddenBlocker(app, w, r.held(app), problems);
    return problems;
}

/** Closes @p w as a person does: Esc to it through its window (a window that
 *  is not a dialog: the window system's close). */
void closeThroughWindow(const Row &r, QWidget *w, QStringList &problems)
{
    QPointer<QWidget> g(w);
    if (r.flags & ClosesByWindow)
    {
        w->windowHandle()->close();
    }
    else
    {
        w->activateWindow();
        settle(1);
        QTest::keyClick(w->windowHandle(), Qt::Key_Escape);
    }
    (void)QTest::qWaitFor([&]() { return g.isNull() || !g->isVisible(); }, 500);
    if (!g.isNull() && g->isVisible())
    {
        problems << ((r.flags & ClosesByWindow) ? QStringLiteral("a close through its window did not close it")
                                                : QStringLiteral("Esc, sent through its window, did not close it"));
        if (QDialog *d = qobject_cast<QDialog *>(g.data()))
            d->reject();
        else
            g->close();
    }
}

QList<QWidget *> newWindows(const QSet<QWidget *> &before)
{
    QList<QWidget *> out;
    for (QWidget *w : QApplication::topLevelWidgets())
    {
        if (!w->isVisible() || before.contains(w) || qobject_cast<QMenu *>(w) != nullptr)
            continue;
        const Qt::WindowType t = w->windowType();
        if (t == Qt::Popup || t == Qt::ToolTip || t == Qt::SplashScreen)
            continue;
        out << w;
    }
    return out;
}

void closeStrays(const QSet<QWidget *> &before)
{
    for (QWidget *w : newWindows(before))
    {
        if (QDialog *d = qobject_cast<QDialog *>(w))
            d->reject();
        else
            w->close();
    }
}

/** A widget at the main window's edge that counts the presses it gets: a
 *  click through the main window's window reaches it unless something
 *  (a filter left behind, a grab) keeps it. */
class ClickProbe final : public QWidget
{
public:
    explicit ClickProbe(QWidget *parent) : QWidget(parent)
    {
        setGeometry(parent->width() - 40, parent->height() / 2, 24, 24);
        show();
        raise();
    }
    int presses = 0;

protected:
    void mousePressEvent(QMouseEvent *) override { ++presses; }
};

QStringList checkAfterClose(App *app, int filtersBefore)
{
    QStringList problems;
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();
    if (QWidget *g = QWidget::mouseGrabber())
        problems << QStringLiteral("closed, and left the mouse grabbed by %1").arg(describe(g));
    if (QWidget *g = QWidget::keyboardGrabber())
        problems << QStringLiteral("closed, and left the keyboard grabbed by %1").arg(describe(g));
    if (QWidget *p = QApplication::activePopupWidget())
        problems << QStringLiteral("closed, and left a popup open: %1").arg(describe(p));
    if (QWidget *m = QApplication::activeModalWidget())
        problems << QStringLiteral("closed, and left a modal window open: %1").arg(describe(m));
    for (QWidget *w : QApplication::allWidgets())
        if (QGraphicsView *v = qobject_cast<QGraphicsView *>(w))
            if (v->scene() != nullptr && v->scene()->mouseGrabberItem() != nullptr)
                problems << QStringLiteral("closed, and an item in %1 still holds the mouse")
                                .arg(v->objectName().isEmpty() ? QStringLiteral("a plot") : v->objectName());
    const int filters = appFilterCount();
    if (filters > filtersBefore)
        problems << QStringLiteral("closed, and left %1 application event filter(s) installed")
                        .arg(filters - filtersBefore);
    {
        ClickProbe probe(app);
        settle(1);
        QTest::mouseClick(app->windowHandle(), Qt::LeftButton, Qt::NoModifier, probe.geometry().center());
        if (probe.presses == 0)
            problems << QStringLiteral("closed, and the main window takes no click");
    }
    return problems;
}

/** UX shots (QLC_SHOT_DIR): Fixture Properties as its screen shows it, over
 *  the main window; what runs off the screen is cut off. */
void shootOnScreen(App *app, const Row &r, QWidget *w)
{
    const QString dir = qEnvironmentVariable("QLC_SHOT_DIR");
    if (dir.isEmpty() || !r.name.startsWith(QStringLiteral("Fixture Properties")))
        return;
    settle();
    const QRect screen = app->screen()->geometry();
    QImage img(screen.size(), QImage::Format_RGB32);
    img.fill(QColor(40, 40, 46));
    QPainter p(&img);
    p.drawPixmap(app->mapToGlobal(QPoint(0, 0)) - screen.topLeft(), app->grab());
    p.drawPixmap(w->mapToGlobal(QPoint(0, 0)) - screen.topLeft(), w->grab());
    p.end();
    QString name = r.name.toLower();
    name.replace(QStringLiteral(", "), QStringLiteral("-")).replace(QLatin1Char(' '), QLatin1Char('-'));
    img.save(QDir(dir).filePath(QStringLiteral("%1-%2x%3.png").arg(name).arg(screen.width()).arg(screen.height())));
}

/** Opens @p r with the main window where it is, checks what opened, and
 *  closes it. @p what: the window that came up. */
QStringList runRow(App *app, const Row &r, QString *what = nullptr)
{
    QSet<QWidget *> before;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (w->isVisible())
            before << w;
    const int filters = appFilterCount();
    const int keptBefore = WindowSpace::keptCount();

    QStringList problems;
    bool found = false, done = false;
    QElapsedTimer clock;
    clock.start();
    QTimer poll;
    poll.setInterval(10);
    QObject::connect(&poll, &QTimer::timeout, [&]() {
        if (done)
        {
            // Anything the open path still shows after it (a note after a
            // cancel): out of the way, so a blocking open returns.
            closeStrays(before);
            return;
        }
        const QList<QWidget *> fresh = newWindows(before);
        if (fresh.isEmpty())
        {
            if (clock.elapsed() > kOpenTimeout)
                done = true;
            return;
        }
        QWidget *w = fresh.last();
        if (QWidget *m = QApplication::activeModalWidget())
            if (fresh.contains(m))
                w = m;
        found = true;
        done = true;              // before anything that spins the event loop
        if (what != nullptr)
            *what = describe(w);
        /* A dialog goes through WindowSpace when it is shown: it stays on
           its main window's macOS Space (a Cmd-Tab back took Fixture
           Properties to another one). One with no parent widget is not
           kept. */
        if (w->windowType() == Qt::Dialog && WindowSpace::keptCount() == keptBefore)
            problems << QStringLiteral("a dialog that does not stay on its main window's Space "
                                       "(WindowSpace keeps only a dialog with a parent widget)");
        shootOnScreen(app, r, w);
        problems << checkOpen(app, r, w);
        closeThroughWindow(r, w, problems);
    });
    poll.start();
    r.open(app);
    (void)QTest::qWaitFor([&]() { return done; }, kOpenTimeout + 2000);
    poll.stop();
    closeStrays(before);
    settle();
    if (!found)
    {
        problems << QStringLiteral("did not open");
        return problems;
    }
    problems << checkAfterClose(app, filters);
    return problems;
}

/** @p problems less the known issues (reported, not failed). */
QStringList unknownOnly(const QString &rowName, const QString &size, const QStringList &problems)
{
    QStringList out;
    for (const QString &p : problems)
    {
        const Known *hit = nullptr;
        for (const Known &k : knownIssues())
            if (rowName.startsWith(QLatin1String(k.row))
                && (k.size[0] == '\0' || size == QLatin1String(k.size))
                && p.contains(QLatin1String(k.finding)))
                hit = &k;
        if (hit == nullptr)
        {
            out << p;
            continue;
        }
        const QString key = QStringLiteral("%1|%2|%3").arg(hit->row, hit->size, hit->finding);
        if (!g_seenKnown.contains(key))
            g_seenKnown << key;
        qWarning().noquote() << "windowsanity: known issue:" << rowName << size << p << "--" << hit->todo;
    }
    return out;
}

/****************************************************************************
 * Windows made wrong on purpose, for the checker's own test
 ****************************************************************************/

/** As the rig-view lock was (builds 4365/4367): every click and key outside
 *  the dialog's own widgets is dropped -- at its window too. */
class WindowEater final : public QObject
{
public:
    explicit WindowEater(QWidget *dlg) : QObject(dlg), m_dlg(dlg) { qApp->installEventFilter(this); }

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        switch (e->type())
        {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            break;
        default:
            return false;
        }
        for (QObject *o = obj; o != nullptr; o = o->parent())
            if (o == m_dlg)
                return false;
        return true;
    }

private:
    QWidget *m_dlg;
};

QDialog *goodDialog(QWidget *parent)
{
    QDialog *d = new QDialog(parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(QStringLiteral("Checker"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    vl->addWidget(new QLineEdit(d));
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, d);
    vl->addWidget(bb);
    QObject::connect(bb, &QDialogButtonBox::rejected, d, &QDialog::reject);
    QObject::connect(bb, &QDialogButtonBox::accepted, d, &QDialog::accept);
    return d;
}

} // namespace

/****************************************************************************
 * Tests
 ****************************************************************************/

void WindowSanity_Test::initTestCase()
{
    QVERIFY2(QGuiApplication::screens().size() >= 3,
             "the offscreen platform did not take the three screens (QT_QPA_PLATFORM configfile)");

    // A HOME of our own: never the real user's folders or settings.
    static QTemporaryDir home;
    QVERIFY(home.isValid());
    qputenv("HOME", home.path().toLocal8Bit());
    qputenv("USERPROFILE", home.path().toLocal8Bit());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    EffectScriptCache::setSystemDirectoriesOverride(QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts",
                                                    QString(QLC_TEST_RESOURCE_ROOT) + "rgbscripts");

    m_app = new App();
    m_app->init();
    QVERIFY(m_app->doc()->fixtureDefCache()->loadMap(QDir(QString(QLC_TEST_RESOURCE_ROOT) + "fixtures")));
    QCOMPARE(m_app->loadXML(QStringLiteral(COURSE_DIR "/shows/lesson-3-9-finish.qlcc")), QFile::NoError);

    // Something for each Lighting Studio editor to open on, and a venue.
    MonitorProperties *props = m_app->doc()->monitorProperties();
    Truss *truss = props->addTruss();
    truss->setName(QStringLiteral("FOH"));
    truss->setLength(6.0f);
    g_truss = truss->id();
    Tower *tower = props->addTower();
    tower->setName(QStringLiteral("Tower A"));
    tower->setHeight(3.5f);
    g_tower = tower->id();
    StagePlatform *deck = props->addPlatform();
    deck->setName(QStringLiteral("Riser"));
    deck->setWidth(2.4f);
    deck->setDepth(1.2f);
    g_deck = deck->id();
    Pipe *pipe = props->addPipe();
    pipe->setName(QStringLiteral("Boom"));
    pipe->setHeight(3.0f);
    g_pipe = pipe->id();
    g_venue = m_app->doc()->programmer()->venueCorrections()->addVenue(QStringLiteral("Window Hall"));

    m_app->doc()->resetModified();
    QVERIFY(placeMain(m_app, sizeScreens().first()));
    QVERIFY(QTest::qWaitForWindowExposed(m_app));
}

void WindowSanity_Test::cleanupTestCase()
{
    for (const Known &k : knownIssues())
    {
        const QString key = QStringLiteral("%1|%2|%3").arg(k.row, k.size, k.finding);
        if (!g_seenKnown.contains(key))
            qWarning().noquote() << "windowsanity: known issue no longer seen, remove its row:"
                                 << k.row << k.size << k.finding << k.todo;
    }
    if (m_app != nullptr)
    {
        m_app->doc()->programmer()->venueCorrections()->setActiveVenue(QString());
        m_app->doc()->resetModified();
        delete m_app;
        m_app = nullptr;
    }
}

void WindowSanity_Test::theCheckerFindsWhatItShould()
{
    const QList<QScreen *> screens = sizeScreens();
    QVERIFY(placeMain(m_app, screens.first()));

    auto expect = [this](const QString &what, const Row &r, const QString &finding) {
        const QStringList found = runRow(m_app, r);
        const QString all = found.join(QStringLiteral("; "));
        if (finding.isEmpty())
            QVERIFY2(found.isEmpty(), qPrintable(what + QStringLiteral(": found ") + all));
        else
            QVERIFY2(all.contains(finding),
                     qPrintable(what + QStringLiteral(": wanted \"") + finding + QStringLiteral("\", found: ")
                                + (all.isEmpty() ? QStringLiteral("nothing") : all)));
    };

    // A good one: nothing, shown or run modal.
    expect(QStringLiteral("good"), row("", QStringLiteral("good"), [](App *a) { goodDialog(a)->show(); }),
           QString());
    expect(QStringLiteral("good, modal"),
           row("", QStringLiteral("good, modal"), [](App *a) { goodDialog(a)->exec(); }), QString());

    // (a) Its window's input dropped on the way, as the rig-view lock did.
    const Row eats = row("", QStringLiteral("eats"), [](App *a) {
        QDialog *d = goodDialog(a);
        new WindowEater(d);
        d->show();
    });
    expect(QStringLiteral("eats its window's clicks"), eats, QStringLiteral("through its window did not reach"));
    expect(QStringLiteral("eats its window's keys"), eats,
           QStringLiteral("Esc, sent through its window, did not close"));

    // (d) Taller than the laptop.
    expect(QStringLiteral("too tall"), row("", QStringLiteral("tall"), [](App *a) {
               QDialog *d = goodDialog(a);
               d->setMinimumHeight(1000);
               d->show();
           }), QStringLiteral("runs off its screen"));

    /* A dialog a little taller than the laptop, with room to shrink:
       WindowPlacement fits it on its main window's screen. */
    QVERIFY(placeMain(m_app, screens.first()));
    expect(QStringLiteral("taller than the laptop, can shrink"), row("", QStringLiteral("shrinks"), [](App *a) {
               QDialog *d = goodDialog(a);
               d->resize(500, 900);
               d->show();
           }), QString());

    // No parent, with the main window on the second screen.
    QVERIFY(placeMain(m_app, screens.at(1)));
    expect(QStringLiteral("no parent"), row("", QStringLiteral("orphan"), [](App *) {
               goodDialog(nullptr)->show();
           }), QStringLiteral("no parent"));
    QVERIFY(placeMain(m_app, screens.first()));

    // An application event filter left behind.
    QPointer<QObject> leftFilter;
    expect(QStringLiteral("leaves a filter"), row("", QStringLiteral("filter"), [&leftFilter](App *a) {
               leftFilter = new QObject(qApp);
               qApp->installEventFilter(leftFilter);
               goodDialog(a)->show();
           }), QStringLiteral("application event filter"));
    delete leftFilter.data();

    // The mouse left grabbed.
    QPointer<QWidget> grabber;
    expect(QStringLiteral("leaves a grab"), row("", QStringLiteral("grab"), [&grabber](App *a) {
               QDialog *d = goodDialog(a);
               grabber = new QWidget(a);
               grabber->setGeometry(10, 10, 20, 20);
               grabber->show();
               QWidget *g = grabber;
               QObject::connect(d, &QDialog::finished, g, [g]() { g->grabMouse(); });
               d->show();
           }), QStringLiteral("mouse grabbed"));
    if (grabber != nullptr)
    {
        grabber->releaseMouse();
        delete grabber.data();
    }

    // (b)/(c) Holds a view and goes out of sight, with nothing to find it by;
    // then the same with its banner.
    QWidget *held = new QWidget(m_app);
    held->setGeometry(100, 100, 400, 300);
    held->show();
    held->raise();
    auto heldFn = [held](App *) { return held; };
    expect(QStringLiteral("holds, no banner"), row("", QStringLiteral("holds"), [](App *a) {
               QDialog *d = goodDialog(a);
               d->show();
               d->activateWindow();
           }, 0, heldFn), QStringLiteral("nothing there says it is open"));
    expect(QStringLiteral("holds, with its banner"), row("", QStringLiteral("holds+banner"), [held](App *a) {
               QDialog *d = goodDialog(a);
               d->show();
               d->activateWindow();
               BlockingWindowBanner::whileOpen(held, d);
           }, 0, heldFn), QString());
    delete held;
}

void WindowSanity_Test::aHiddenEditorIsFoundFromTheBanner()
{
    const QList<quint32> ids = someFixtures(m_app->doc(), 1);
    QVERIFY(!ids.isEmpty());
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    for (QScreen *s : sizeScreens())
    {
        QVERIFY(placeMain(m_app, s));
        Monitor *mon = studio(m_app);
        QVERIFY(mon != nullptr);
        QWidget *plot = mon->m_graphicsView;
        bool ran = false;
        QStringList problems;
        QTimer::singleShot(0, this, [&]() {
            QDialog *dlg = nullptr;
            (void)QTest::qWaitFor([&]() {
                for (QWidget *w : QApplication::topLevelWidgets())
                    if (w->isVisible() && w->windowTitle().startsWith(QStringLiteral("Fixture Properties")))
                        dlg = qobject_cast<QDialog *>(w);
                return dlg != nullptr;
            }, kOpenTimeout);
            if (dlg == nullptr)
                return;
            ran = true;
            // On another screen and hidden, as on another macOS Space.
            QScreen *elsewhere = sizeScreens().first() == s ? sizeScreens().last() : sizeScreens().first();
            dlg->windowHandle()->setScreen(elsewhere);
            dlg->move(elsewhere->availableGeometry().topLeft() + QPoint(20, 20));
            dlg->hide();
            settle();
            BlockingWindowBanner *banner = BlockingWindowBanner::bannerOn(plot);
            if (banner == nullptr || !banner->isVisible())
            {
                problems << QStringLiteral("no banner on the plot");
            }
            else
            {
                if (!banner->text().contains(QStringLiteral("Fixture Properties")))
                    problems << QStringLiteral("the banner does not name the window: ") + banner->text();
                if (!shots.isEmpty())
                {
                    QDir().mkpath(shots);
                    m_app->grab().save(QDir(shots).filePath(
                        QStringLiteral("windowsanity-banner-%1.png").arg(sizeName(s))));
                }
                QPushButton *show = banner->showButton();
                QTest::mouseClick(m_app->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                                  show->mapTo(m_app, show->rect().center()));
                settle();
                if (!dlg->isVisible())
                    problems << QStringLiteral("Show It did not bring the editor back");
                if (!isActive(dlg))
                    problems << QStringLiteral("the editor is back but not active");
                if (dlg->screen() != m_app->screen())
                    problems << QStringLiteral("the editor came back on %1, not the main window's %2")
                                    .arg(dlg->screen()->name(), m_app->screen()->name());
            }
            dlg->reject();
        });
        mon->showFixtureItemEditor(ids.first());
        settle();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY2(ran, "Fixture Properties did not open");
        QVERIFY2(problems.isEmpty(),
                 qPrintable(sizeName(s) + QStringLiteral(": ") + problems.join(QStringLiteral("; "))));
        QVERIFY2(BlockingWindowBanner::bannerOn(plot) == nullptr, "the banner stayed after the editor closed");
    }
}

void WindowSanity_Test::aDialogPutOnAnotherScreenStaysThere()
{
    const QList<QScreen *> screens = sizeScreens();
    QVERIFY(placeMain(m_app, screens.first()));
    QScreen *other = screens.last();

    /* Kept and shown again, as the Show Tree's notes list is: the person
       dragged it onto the second screen. Hidden and shown, it is still
       there; the main window's screen does not pull it back. */
    QDialog *d = goodDialog(m_app);
    d->setAttribute(Qt::WA_DeleteOnClose, false);
    d->show();
    QVERIFY(QTest::qWaitForWindowExposed(d));
    d->windowHandle()->setScreen(other);
    const QPoint there = other->availableGeometry().topLeft() + QPoint(200, 150);
    d->move(there);
    settle();
    QCOMPARE(d->screen(), other);
    for (int round = 0; round < 2; ++round)
    {
        d->hide();
        settle();
        d->show();
        settle();
        QVERIFY2(d->screen() == other, qPrintable(QStringLiteral("round %1: pulled to %2")
                                                     .arg(round).arg(d->screen()->name())));
        QCOMPARE(d->frameGeometry().topLeft(), there);
    }

    /* Across the edge of two screens (wholly on neither): pulled onto the
       one under its middle, wholly. */
    d->hide();
    const QRect edge = screens.at(1)->availableGeometry();
    d->resize(300, 200);
    d->move(edge.left() - 40, edge.top() + 50);            // mostly on the second screen
    d->show();
    settle();
    bool wholly = false;
    for (QScreen *s : QGuiApplication::screens())
        wholly = wholly || s->availableGeometry().contains(d->frameGeometry());
    QVERIFY2(wholly, "a dialog across two screens was left across them");
    QCOMPARE(d->screen(), screens.at(1));
    delete d;
}

void WindowSanity_Test::theBannerAnswersAheadOfTheLock()
{
    /* The banner's own filter is installed after the rig-view lock's and so
       runs first: a click handed straight to Show It (not through the
       window, where the lock lets everything by) reaches it although the
       lock drops every other click on the plot. Swap the two lines in
       Monitor::execFixtureEditor and this fails. */
    const QList<quint32> ids = someFixtures(m_app->doc(), 1);
    QVERIFY(!ids.isEmpty());
    QVERIFY(placeMain(m_app, sizeScreens().first()));
    Monitor *mon = studio(m_app);
    QVERIFY(mon != nullptr);
    bool ran = false, back = false;
    QTimer::singleShot(0, this, [&]() {
        QDialog *dlg = nullptr;
        (void)QTest::qWaitFor([&]() {
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->isVisible() && w->windowTitle().startsWith(QStringLiteral("Fixture Properties")))
                    dlg = qobject_cast<QDialog *>(w);
            return dlg != nullptr;
        }, kOpenTimeout);
        if (dlg == nullptr)
            return;
        ran = true;
        dlg->hide();
        settle();
        if (BlockingWindowBanner *b = BlockingWindowBanner::bannerOn(mon->m_graphicsView))
        {
            QTest::mouseClick(b->showButton(), Qt::LeftButton);
            settle();
            back = dlg->isVisible();
        }
        dlg->reject();
    });
    mon->showFixtureItemEditor(ids.first());
    settle();
    QVERIFY2(ran, "Fixture Properties did not open");
    QVERIFY2(back, "a click on Show It did not get past the rig-view lock");
}

void WindowSanity_Test::fixturePropertiesEveryTabFits()
{
    // As it opens the first time: on Fixture, Calibrate folded.
    QSettings().remove(QStringLiteral("fixtureProperties"));
    struct Case { QString tag; QList<quint32> ids; };
    QList<Case> cases;
    for (Fixture *f : m_app->doc()->fixtures())
    {
        if (f->name() == QLatin1String("SR Truss Spot 1"))
            cases << Case{ QStringLiteral("mover"), { f->id() } };
        else if (f->name() == QLatin1String("SR AT Spot"))
            cases << Case{ QStringLiteral("focus-spot"), { f->id() } };
    }
    QCOMPARE(cases.size(), 2);
    cases << Case{ QStringLiteral("several"), someFixtures(m_app->doc(), 3) };
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");

    QStringList problems;
    for (QScreen *s : sizeScreens())
    {
        QVERIFY(placeMain(m_app, s));
        Monitor *mon = studio(m_app);
        QVERIFY(mon != nullptr);
        for (const Case &c : cases)
        {
            // Each as it opens the first time: the tabs this goes through are not remembered.
            QSettings().remove(QStringLiteral("fixtureProperties"));
            const QString where = QStringLiteral("%1 @ %2").arg(c.tag, sizeName(s));
            bool ran = false;
            QTimer::singleShot(0, this, [&]() {
                QDialog *dlg = nullptr;
                (void)QTest::qWaitFor([&]() {
                    for (QWidget *w : QApplication::topLevelWidgets())
                        if (w->isVisible() && w->windowTitle().startsWith(QStringLiteral("Fixture Properties")))
                            dlg = qobject_cast<QDialog *>(w);
                    return dlg != nullptr;
                }, kOpenTimeout);
                if (dlg == nullptr)
                    return;
                ran = true;
                settle();
                /* Its pages: the tabs', or (no tabs) every scroller in it, one
                   page -- so the one tall scroller it had fails here. */
                QTabWidget *tabs = dlg->findChild<QTabWidget *>(QStringLiteral("fixturePropertiesTabs"));
                const int pages = tabs != nullptr ? tabs->count() : 1;
                if (tabs == nullptr)
                    problems << where + QStringLiteral(": no tabs");
                if (tabs != nullptr && tabs->currentIndex() != 0)
                    problems << where + QStringLiteral(": opened on tab %1, not Fixture").arg(tabs->currentIndex());
                if (QToolButton *more = dlg->findChild<QToolButton *>(QStringLiteral("fixtureAimMore")))
                    if (more->isChecked())
                        problems << where + QStringLiteral(": Calibrate is open");
                // Three tabs: Fixture | Place | Aim & Beam (DMX Controls went to Fixture's Find it row).
                if (tabs != nullptr && tabs->count() != 3)
                    problems << where + QStringLiteral(": %1 tabs, not 3").arg(tabs->count());
                const QRect room = dlg->screen()->availableGeometry();
                for (int i = 0; i < pages; ++i)
                {
                    QString page = QStringLiteral("whole");
                    if (tabs != nullptr)
                    {
                        tabs->setCurrentIndex(i);
                        page = tabs->tabText(i).remove(QLatin1Char('&'));
                    }
                    settle();
                    const QString at = QStringLiteral("%1, %2").arg(where, page);
                    QList<QScrollArea *> areas;
                    if (tabs != nullptr)
                    {
                        if (QScrollArea *a = qobject_cast<QScrollArea *>(tabs->widget(i)))
                            areas << a;
                    }
                    else
                        areas = dlg->findChildren<QScrollArea *>();
                    for (QScrollArea *a : areas)
                    {
                        if (!a->isVisible() || a->widget() == nullptr)
                            continue;
                        const int need = a->widget()->sizeHint().height(), has = a->viewport()->height();
                        if (a->verticalScrollBar()->isVisible() || need > has)
                            problems << at + QStringLiteral(": scrolls (%1 px of rows in %2 px; the dialog "
                                                            "is %3 px tall on a %4 px screen)")
                                                 .arg(need).arg(has).arg(dlg->frameGeometry().height())
                                                 .arg(room.height());
                    }
                    if (!room.contains(dlg->frameGeometry()))
                        problems << at + QStringLiteral(": runs off its screen");
                    QDialogButtonBox *bb = dlg->findChild<QDialogButtonBox *>();
                    if (bb == nullptr || !bb->isVisible()
                        || !dlg->rect().contains(QRect(bb->mapTo(dlg, QPoint(0, 0)), bb->size()))
                        || !room.contains(QRect(bb->mapToGlobal(QPoint(0, 0)), bb->size())))
                        problems << at + QStringLiteral(": OK / Cancel are out of view");
                    /* Aim & Beam with Calibrate unfolded, on a screen of 800 or
                       more: the dialog grows to show it all, no scroll. */
                    QToolButton *cal = dlg->findChild<QToolButton *>(QStringLiteral("fixtureAimMore"));
                    if (tabs != nullptr && tabs->widget(i)->objectName().endsWith(QLatin1String("-aim"))
                        && room.height() >= 800 && cal != nullptr && cal->isVisible() && !cal->isChecked())
                    {
                        QTest::mouseClick(cal, Qt::LeftButton);
                        settle();
                        for (QScrollArea *a : areas)
                            if (a->isVisible() && a->widget() != nullptr
                                && (a->verticalScrollBar()->isVisible()
                                    || a->widget()->sizeHint().height() > a->viewport()->height()))
                                problems << at + QStringLiteral(" (Calibrate open): scrolls (%1 px of rows in %2 px)")
                                                     .arg(a->widget()->sizeHint().height()).arg(a->viewport()->height());
                        if (!room.contains(dlg->frameGeometry()))
                            problems << at + QStringLiteral(" (Calibrate open): runs off its screen");
                        QTest::mouseClick(cal, Qt::LeftButton);
                        settle();
                        QSettings().remove(QStringLiteral("fixtureProperties"));
                    }
                    QStringList input;
                    checkInputThroughWindow(dlg, input);
                    for (const QString &p : input)
                        problems << at + QStringLiteral(": ") + p;
                    if (!shots.isEmpty())
                    {
                        QDir().mkpath(shots);
                        QString key = tabs != nullptr ? tabs->widget(i)->objectName().section(QLatin1Char('-'), 1)
                                                      : QStringLiteral("whole");
                        const QRect screen = s->geometry();
                        auto save = [&](const QString &name) {
                            settle();
                            QImage img(screen.size(), QImage::Format_RGB32);
                            img.fill(QColor(40, 40, 46));
                            QPainter p(&img);
                            p.drawPixmap(m_app->mapToGlobal(QPoint(0, 0)) - screen.topLeft(), m_app->grab());
                            p.drawPixmap(dlg->mapToGlobal(QPoint(0, 0)) - screen.topLeft(), dlg->grab());
                            p.end();
                            img.save(QDir(shots).filePath(QStringLiteral("fptabs-%1-%2-%3.png")
                                                              .arg(c.tag, name, sizeName(s))));
                        };
                        save(key);
                        // Aim & Beam with Calibrate unfolded, as it reads open.
                        QToolButton *more = dlg->findChild<QToolButton *>(QStringLiteral("fixtureAimMore"));
                        if (key == QLatin1String("aim") && more != nullptr && more->isVisible())
                        {
                            more->setChecked(true);
                            save(QStringLiteral("aim-more"));
                            more->setChecked(false);
                        }
                    }
                }
                QPointer<QDialog> g(dlg);
                dlg->activateWindow();
                settle(1);
                QTest::keyClick(dlg->windowHandle(), Qt::Key_Escape);
                (void)QTest::qWaitFor([&]() { return g.isNull() || !g->isVisible(); }, 500);
                if (!g.isNull() && g->isVisible())
                {
                    problems << where + QStringLiteral(": Esc did not close it");
                    g->reject();
                }
            });
            if (c.ids.size() == 1)
                mon->showFixtureItemEditor(c.ids.first());
            else
                mon->showFixtureItemEditor(c.ids);
            settle();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (!ran)
                problems << where + QStringLiteral(": did not open");
            m_app->doc()->resetModified();
        }
    }
    QSettings().remove(QStringLiteral("fixtureProperties"));
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("\n  "))));
}

void WindowSanity_Test::everyWindowWorksThroughItsWindow_data()
{
    QTest::addColumn<int>("index");
    QTest::addColumn<int>("screen");
    const QList<Row> list = windows();
    const QList<QScreen *> screens = sizeScreens();
    for (int i = 0; i < list.size(); ++i)
        for (int s = 0; s < screens.size() && s < 3; ++s)
            QTest::newRow(qPrintable(QStringLiteral("%1 @ %2").arg(list.at(i).name, sizeName(screens.at(s)))))
                << i << s;
}

void WindowSanity_Test::everyWindowWorksThroughItsWindow()
{
    QFETCH(int, index);
    QFETCH(int, screen);
    const Row r = windows().at(index);
    QScreen *s = sizeScreens().at(screen);
    QVERIFY(placeMain(m_app, s));
    QString what;
    const QStringList problems = unknownOnly(r.name, sizeName(s), runRow(m_app, r, &what));
    m_app->doc()->resetModified();
    QVERIFY2(problems.isEmpty(),
             qPrintable(QStringLiteral("%1 (%2) @ %3:\n  ").arg(r.name, what, sizeName(s))
                        + problems.join(QStringLiteral("\n  "))));
}

/* Its own main(): the offscreen platform is given three screens of the three
   sizes before the application starts (still offscreen: the test guard and
   test.sh set that, and QLC_TEST_REAL_SCREEN=1 lifts it as for any test).
   The file is named relative to the working dir: a drive letter's ':' would
   end the platform's option. */
int main(int argc, char **argv)
{
    const QByteArray cfgName("windowsanity-screens.json");
    const bool offscreen = qgetenv("QLC_TEST_REAL_SCREEN") != "1";
    if (offscreen)
    {
        QFile cfg(QString::fromLatin1(cfgName));
        if (cfg.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            cfg.write(kScreensJson);
            cfg.close();
            qputenv("QT_QPA_PLATFORM", "offscreen:configfile=" + cfgName);
        }
    }
    int rc = 0;
    {
        QApplication app(argc, argv);
        app.setAttribute(Qt::AA_Use96Dpi, true);
        WindowSanity_Test t;
        rc = QTest::qExec(&t, argc, argv);
    }
    if (offscreen)
        QFile::remove(QString::fromLatin1(cfgName));
    return rc;
}
