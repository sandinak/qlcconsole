// TEST MATRIX 2 item 2 (leaks): a long offscreen session of the real App --
// open a show, Build edits on the Workbench (open collections and scenes,
// add / reorder / remove layers, undo / redo, the three sections), Run (Operate
// mode, functions started and stopped), close -- N cycles over the shows given.
// Prints RSS after every cycle. Under macOS `leaks` (MallocStackLogging) or an
// ASan/LSan build, what is never freed shows up at exit.
//
// usage: m2leak <cycles> <show> [<show>...]
//   QLC_LEAK_SNAPSHOT=<cmd>: run "<cmd> <pid> <cycle>" after cycle 2 and at the end.
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QListWidget>
#include <QMenu>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#define private public
#define protected public
#include "app.h"
#include "programmingmanager.h"
#include "workbench.h"
#include "workbenchstack.h"
#undef protected
#undef private
#include "doc.h"
#include "function.h"
#include "scene.h"
#include "collection.h"
#include "chaser.h"
#include "rgbmatrix.h"
#include "mastertimer.h"
#include "showtreecontroller.h"

static int g_modals = 0;

static void closeModal()
{
    QWidget *w = QApplication::activeModalWidget();
    if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
    {
        ++g_modals;
        if (QDialog *d = qobject_cast<QDialog *>(w))
            d->reject();
        else
            w->close();
    }
}

static void pump(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        closeModal();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        // No exec() here: deleteLater() would otherwise never run, and the
        // app's own event loop does run them.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

static double rssMB()
{
#if defined(__APPLE__)
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        return info.phys_footprint / 1048576.0;
    return -1;
#else
    long pages = 0, resident = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) { if (fscanf(f, "%ld %ld", &pages, &resident) != 2) resident = 0; fclose(f); }
    return resident * (sysconf(_SC_PAGESIZE) / 1048576.0);
#endif
}

static void snapshot(int cycle)
{
    const QByteArray cmd = qgetenv("QLC_LEAK_SNAPSHOT");
    if (cmd.isEmpty())
        return;
    const QByteArray line = cmd + " " + QByteArray::number(getpid()) + " " + QByteArray::number(cycle);
    fprintf(stderr, "[m2leak] snapshot: %s\n", line.constData());
    if (system(line.constData()) != 0)
        fprintf(stderr, "[m2leak] snapshot command failed\n");
}

static QString uiCounts()
{
    QStringList out;
    for (QWidget *w : QApplication::allWidgets())
    {
        QListWidget *lw = qobject_cast<QListWidget *>(w);
        if (lw == nullptr)
            continue;
        for (QObject *p = lw->parent(); p != nullptr; p = p->parent())
            if (QByteArray(p->metaObject()->className()) == "InputOutputManager")
            {
                out << QStringLiteral("iolist=%1").arg(lw->count());
                break;
            }
    }
    int actions = 0;
    for (QWidget *w : QApplication::allWidgets())
        if (QMenu *m = qobject_cast<QMenu *>(w))
            actions += m->findChildren<QAction *>(Qt::FindDirectChildrenOnly).size();
    out << QStringLiteral("menuChildActions=%1").arg(actions);
    return out.join(' ');
}

static int buildEdits(App *app, Doc *doc)
{
    ProgrammingManager *pm = ProgrammingManager::instance();
    if (pm == nullptr)
        return -1;
    int edits = 0;
    pm->setSection(ProgrammingManager::ScenesSection);
    pump(30);
    QList<Collection *> cols;
    QList<Scene *> scenes;
    for (Function *f : doc->functions())
    {
        if (Collection *c = qobject_cast<Collection *>(f))
            if (c->isVisible() && (doc->showTree() == nullptr || !doc->showTree()->isGenerated(c->id())))
                cols << c;
        if (Scene *s = qobject_cast<Scene *>(f))
            if (s->isVisible())
                scenes << s;
    }
    int n = 0;
    for (Collection *c : cols)
    {
        if (++n > 6)
            break;
        pm->showFunction(c->id());
        pump(30);
        Workbench *wb = pm->workbench();
        if (wb == nullptr || wb->stack() == nullptr)
            continue;
        WorkbenchStack *st = wb->stack();
        if (st->canAddLayers() && !scenes.isEmpty())
        {
            if (st->addLayers({ scenes.at(n % scenes.size())->id() }, 0))
                ++edits;
            pump(20);
            if (st->canReorderLayers() && st->layers().size() > 1 && st->moveLayer(0, st->layers().size() - 1))
                ++edits;
            pump(20);
            pm->slotUndo(); pump(20);
            pm->slotUndo(); pump(20);
            pm->slotRedo(); pump(20);
            pm->slotUndo(); pump(20);
        }
    }
    n = 0;
    for (Scene *s : scenes)
    {
        if (++n > 8)
            break;
        pm->showFunction(s->id());     // the Look Editor on the scene
        pump(30);
    }
    pm->setSection(ProgrammingManager::PalettesSection); pump(30);
    pm->setSection(ProgrammingManager::EffectsSection); pump(30);
    pm->setSection(ProgrammingManager::ScenesSection); pump(30);
    Q_UNUSED(app);
    return edits;
}

static int run(Doc *doc)
{
    doc->setMode(Doc::Operate);
    pump(50);
    int started = 0;
    for (Function *f : doc->functions())
    {
        if (started >= 12)
            break;
        if (qobject_cast<Collection *>(f) || qobject_cast<Chaser *>(f) || qobject_cast<RGBMatrix *>(f))
        {
            f->start(doc->masterTimer(), FunctionParent(FunctionParent::Master, 0));
            ++started;
        }
    }
    pump(2000);
    doc->masterTimer()->stopAllFunctions();
    pump(100);
    doc->setMode(Doc::Design);
    pump(50);
    return started;
}

int main(int argc, char **argv)
{
    QApplication qapp(argc, argv);
    // The run's own settings (INI in a temporary folder): never the user's.
    static QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    const QStringList args = qapp.arguments();
    if (args.size() < 3)
        return 2;
    const int cycles = args.at(1).toInt();
    const QStringList shows = args.mid(2);

    App *app = new App();
    app->init();
    pump(200);
    fprintf(stderr, "[m2leak] start rss %.1f MB\n", rssMB());

    QTimer t;
    t.setInterval(50);
    QObject::connect(&t, &QTimer::timeout, closeModal);
    t.start();
    for (int i = 1; i <= cycles; ++i)
    {
        const QString show = shows.at((i - 1) % shows.size());
        app->clearDocument();
        pump(30);
        const int e = int(app->loadXML(show));
        pump(300);
        Doc *doc = app->doc();
        const int funcs = doc->functions().size();
        const int edits = qEnvironmentVariableIsSet("QLC_LEAK_NOEDIT") ? 0 : buildEdits(app, doc);
        const int started = qEnvironmentVariableIsSet("QLC_LEAK_NORUN") ? 0 : run(doc);
        app->clearDocument();
        pump(100);
        fprintf(stderr, "[m2leak] ui after load+close: %s\n", qPrintable(uiCounts()));
        fprintf(stderr, "[m2leak] cycle %d %s load=%d functions=%d edits=%d started=%d modals=%d rss %.1f MB\n",
                i, qPrintable(show.section('/', -1)), e, funcs, edits, started, g_modals, rssMB());
        if (i == 2)
            snapshot(i);
    }
    snapshot(cycles);
    if (qEnvironmentVariableIsSet("QLC_LEAK_TEARDOWN"))
    {
        // Full teardown, so LSan's report at exit is only what was really lost.
        delete app;
        return 0;
    }
    fflush(stderr);
    _exit(0);
}
