// The on-screen memory soak, offscreen: the real App with a show loaded, the
// Lighting Studio tab in front and the Rig Overview open, in Operate with
// looks cycling (a new set of up to 12 collections / chasers / RGB matrices /
// scenes every 30 s), for N minutes. Prints RSS (phys_footprint on macOS)
// every minute. Run it under MallocStackLogging=1 and take `heap -s`,
// `vmmap --summary` and `leaks` of its pid from outside (studiosoak.sh).
//
// usage: studiosoak <minutes> <show>
// Not built by default: see CMakeLists.txt here.
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#include "../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#undef protected
#undef private
#include "doc.h"
#include "function.h"
#include "scene.h"
#include "collection.h"
#include "chaser.h"
#include "rgbmatrix.h"
#include "mastertimer.h"

static void closeModal()
{
    QWidget *w = QApplication::activeModalWidget();
    if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
    {
        if (QDialog *d = qobject_cast<QDialog *>(w))
            d->reject();
        else
            w->close();
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
    const int minutes = args.at(1).toInt();

    QTimer modals;
    modals.setInterval(50);
    QObject::connect(&modals, &QTimer::timeout, closeModal);
    modals.start();

    App *app = new App();
    app->init();
    app->resize(1920, 1080);
    app->show();
    const int e = int(app->loadXML(args.at(2)));
    Doc *doc = app->doc();
    app->showContext(QObject::tr("Lighting Studio"));
    if (Monitor::instance() != nullptr)
        Monitor::instance()->stageOverviewView(true);
    doc->setMode(Doc::Operate);

    QList<Function *> pool;
    for (Function *f : doc->functions())
        if (qobject_cast<Collection *>(f) || qobject_cast<Chaser *>(f)
            || qobject_cast<RGBMatrix *>(f) || qobject_cast<Scene *>(f))
            pool << f;
    fprintf(stderr, "[soak] pid %d load=%d functions=%d playable=%d rss %.1f MB\n",
            int(getpid()), e, int(doc->functions().size()), int(pool.size()), rssMB());

    int next = 0;
    auto cycle = [&]() {
        doc->masterTimer()->stopAllFunctions();
        for (int k = 0; k < 12 && !pool.isEmpty(); ++k)
        {
            Function *f = pool.at(next++ % pool.size());
            f->start(doc->masterTimer(), FunctionParent(FunctionParent::Master, 0));
        }
    };
    QTimer looks;
    looks.setInterval(30000);
    QObject::connect(&looks, &QTimer::timeout, cycle);
    looks.start();
    QTimer::singleShot(2000, cycle);

    QElapsedTimer clock;
    clock.start();
    QTimer sample;
    sample.setInterval(60000);
    QObject::connect(&sample, &QTimer::timeout, [&]() {
        const qint64 m = (clock.elapsed() + 30000) / 60000;
        fprintf(stderr, "[soak] minute %lld rss %.1f MB running=%d\n",
                (long long)m, rssMB(), int(doc->masterTimer()->runningFunctions()));
        if (m >= minutes)
        {
            // Not quit(): a modal's nested loop would only leave itself.
            fflush(stderr);
            _exit(0);
        }
    });
    sample.start();
    qapp.exec();
    fflush(stderr);
    _exit(0);
}
