/*
  QLC+ engine stress / torture harness

  Subcommands:
    engine   build a large show and drive the engine to find breaking points
    emit     build the same show and save it as a canonical .qxw workspace
             (for the full-app black-box driver)

  Engine modes (--mode):
    flatout  (default) synchronously call timerTickFunctions() + processFaders()
             as fast as possible; measure per-tick compute and overruns vs the
             20ms tick budget. Deterministic, isolates engine CPU cost.
    realtime run the real MasterTimer at its configured frequency with universe
             threads and an event loop; measure tick cadence/jitter + RSS.
    ramp     repeatedly run flatout while scaling the universe count to find the
             largest config that still fits inside the tick budget.

  We use `#define private public` (same trick as engine/test) to reach
  MasterTimer::timerTickFunctions() and Universe::processFaders() directly.
*/

#define private public
#define protected public
#include "mastertimer.h"
#include "universe.h"
#undef protected
#undef private

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QXmlStreamWriter>
#include <QCommandLineParser>
#include <QVector>
#include <QFile>
#include <QRandomGenerator>
#include <QtGlobal>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <cmath>

#include "doc.h"
#include "function.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "fixture.h"
#include "scene.h"
#include "qlcfixturedefcache.h"
#include "showbuilder.h"
#include <QXmlStreamReader>

#if defined(Q_OS_MAC)
#include <mach/mach.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <QTextStream>

static const double kTickBudgetMs = 20.0; // 50Hz default tick

// ---- resident memory (MB) ---------------------------------------------------
static double residentMB()
{
#if defined(Q_OS_MAC)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &count) == KERN_SUCCESS)
        return double(info.resident_size) / (1024.0 * 1024.0);
#endif
    return 0.0;
}

// ---- open file-descriptor count (for soak FD-leak detection) ----------------
static int openFdCount()
{
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    int n = 0;
    for (int fd = 0; fd < 1024; fd++)
        if (fcntl(fd, F_GETFD) != -1) n++;
    return n;
#else
    return -1;
#endif
}

// ---- FNV-1a 64-bit rolling hash ---------------------------------------------
static quint64 fnv1a(const uchar *data, int len, quint64 h = 1469598103934665603ULL)
{
    for (int i = 0; i < len; i++) { h ^= data[i]; h *= 1099511628211ULL; }
    return h;
}

// ---- percentile over a copy -------------------------------------------------
static double pct(QVector<double> v, double p)
{
    if (v.isEmpty()) return 0.0;
    std::sort(v.begin(), v.end());
    int idx = int(std::ceil(p / 100.0 * v.size())) - 1;
    idx = qBound(0, idx, v.size() - 1);
    return v.at(idx);
}

struct TickStats
{
    double min = 0, mean = 0, p50 = 0, p95 = 0, p99 = 0, max = 0;
    int overruns = 0;
    int count = 0;
};

static TickStats summarize(const QVector<double> &ms)
{
    TickStats s;
    s.count = ms.size();
    if (ms.isEmpty()) return s;
    double sum = 0, mn = ms.first(), mx = ms.first();
    for (double v : ms) { sum += v; mn = qMin(mn, v); mx = qMax(mx, v); if (v > kTickBudgetMs) s.overruns++; }
    s.mean = sum / ms.size();
    s.min = mn; s.max = mx;
    s.p50 = pct(ms, 50); s.p95 = pct(ms, 95); s.p99 = pct(ms, 99);
    return s;
}

static void printStats(const QString &label, const TickStats &s)
{
    fprintf(stdout,
        "%-10s ticks=%-6d  min=%.3f  mean=%.3f  p50=%.3f  p95=%.3f  p99=%.3f  max=%.3f ms  overruns(>%.0fms)=%d (%.1f%%)\n",
        qPrintable(label), s.count, s.min, s.mean, s.p50, s.p95, s.p99, s.max,
        kTickBudgetMs, s.overruns, s.count ? 100.0 * s.overruns / s.count : 0.0);
    fflush(stdout);
}

// Machine-readable result line consumed by the orchestrator (run.py). One per
// scenario; greppable by the "RESULT_JSON " prefix. Only emitted with --json.
static bool g_json = false;
static QString g_scenario = "adhoc";
static void emitJson(const QString &mode, const QString &scenario,
                     const ShowSpec &spec, const TickStats &s,
                     double rssMB, double budgetMs)
{
    if (!g_json) return;
    fprintf(stdout,
        "RESULT_JSON {\"mode\":\"%s\",\"scenario\":\"%s\",\"universes\":%d,"
        "\"fixturesPerUni\":%d,\"fixtures\":%d,\"channels\":%d,\"scenes\":%d,"
        "\"chasers\":%d,\"matrices\":%d,\"efx\":%d,\"collections\":%d,"
        "\"scripts\":%d,\"sequences\":%d,\"shows\":%d,\"ticks\":%d,\"min\":%.4f,\"mean\":%.4f,"
        "\"p50\":%.4f,\"p95\":%.4f,\"p99\":%.4f,\"max\":%.4f,\"overruns\":%d,"
        "\"budgetMs\":%.1f,\"rssMB\":%.1f,\"withinBudget\":%s}\n",
        qPrintable(mode), qPrintable(scenario), spec.universes, spec.fixturesPerUni,
        spec.totalFixtures(), spec.totalChannels(), spec.scenes, spec.chasers,
        spec.matrices, spec.efx, spec.collections, spec.scripts, spec.sequences, spec.shows,
        s.count, s.min, s.mean, s.p50, s.p95, s.p99, s.max, s.overruns,
        budgetMs, rssMB, (s.p99 < budgetMs ? "true" : "false"));
    fflush(stdout);
}

// ---- start every top-level function ----------------------------------------
static void startAll(Doc *doc, const QList<Function*> &funcs)
{
    MasterTimer *mt = doc->masterTimer();
    foreach (Function *f, funcs)
        f->start(mt, FunctionParent::master());
}

// Synchronous stop for flatout/ramp: the platform timer thread is NOT running,
// so MasterTimer::stopAllFunctions() (which spins until runningFunctions()==0)
// would deadlock. Instead, mark everything stopped and pump ticks ourselves so
// postRun()/removal actually happens.
static void stopAllSync(Doc *doc, QList<Universe*> universes)
{
    MasterTimer *mt = doc->masterTimer();
    foreach (Function *f, mt->m_functionList)
        if (f) f->stop(FunctionParent::master());
    for (int i = 0; i < 20 && mt->runningFunctions() > 0; i++)
    {
        mt->timerTickFunctions(universes);
        foreach (Universe *u, universes) u->processFaders();
    }
}

// ---- FLATOUT: synchronous tick loop (deterministic compute benchmark) -------
static TickStats runFlatout(Doc *doc, const QList<Function*> &funcs, int ticks, int warmup)
{
    MasterTimer *mt = doc->masterTimer();
    doc->setMode(Doc::Operate);
    startAll(doc, funcs);

    QList<Universe*> universes = doc->inputOutputMap()->claimUniverses();

    // warmup drains the start queue (preRun) and primes faders
    for (int i = 0; i < warmup; i++)
    {
        mt->timerTickFunctions(universes);
        foreach (Universe *u, universes) u->processFaders();
    }

    QVector<double> ms;
    ms.reserve(ticks);
    QElapsedTimer t;
    for (int i = 0; i < ticks; i++)
    {
        t.restart();
        mt->timerTickFunctions(universes);
        foreach (Universe *u, universes) u->processFaders();
        ms.append(t.nsecsElapsed() / 1.0e6);
    }
    stopAllSync(doc, universes);
    doc->inputOutputMap()->releaseUniverses();
    return summarize(ms);
}

// ---- REALTIME tick cadence probe (runs in MasterTimer thread) ---------------
class CadenceProbe : public QObject
{
public:
    QVector<double> gaps;        // ms between consecutive ticks
    qint64 lastNs = 0;
    QElapsedTimer clock;
    double rssStart = 0, rssEnd = 0;

    void onTick()
    {
        qint64 now = clock.nsecsElapsed();
        if (lastNs != 0)
            gaps.append((now - lastNs) / 1.0e6);
        lastNs = now;
    }
};

static int runRealtime(Doc *doc, const QList<Function*> &funcs, int seconds, const ShowSpec &spec)
{
    MasterTimer *mt = doc->masterTimer();
    doc->setMode(Doc::Operate);
    doc->inputOutputMap()->startUniverses();

    CadenceProbe probe;
    probe.clock.start();
    probe.rssStart = residentMB();
    // DirectConnection -> the lambda runs in the timer thread at each tick
    QObject::connect(mt, &MasterTimer::tickReady, &probe,
                     [&probe]() { probe.onTick(); }, Qt::DirectConnection);

    mt->start();
    startAll(doc, funcs);

    fprintf(stdout, "realtime: running %d s at %u Hz (budget %.1f ms/tick), RSS start %.1f MB ...\n",
            seconds, MasterTimer::frequency(), kTickBudgetMs, probe.rssStart);
    fflush(stdout);

    QElapsedTimer wall; wall.start();
    double rssPeak = probe.rssStart;
    while (wall.elapsed() < qint64(seconds) * 1000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        rssPeak = qMax(rssPeak, residentMB());
    }
    probe.rssEnd = residentMB();

    mt->stop();

    // cadence stats: expected gap = 1000/freq; overrun = gap noticeably late
    double expected = 1000.0 / MasterTimer::frequency();
    int late = 0;
    for (double g : probe.gaps) if (g > expected * 1.5) late++;
    TickStats s = summarize(probe.gaps);
    fprintf(stdout, "realtime: observed inter-tick gap (expected %.2f ms):\n", expected);
    printStats("gap", s);
    fprintf(stdout, "realtime: late ticks(>1.5x)=%d/%d  running funcs=%d  RSS start=%.1f peak=%.1f end=%.1f MB (growth %.1f MB)\n",
            late, s.count, mt->runningFunctions(), probe.rssStart, rssPeak, probe.rssEnd, probe.rssEnd - probe.rssStart);
    fflush(stdout);
    // for realtime the "budget" is the expected gap; withinBudget = cadence held
    emitJson("realtime", g_scenario, spec, s, rssPeak, expected * 1.5);
    return s.overruns > 0 || late > s.count / 20 ? 1 : 0;
}

// ---- RAMP: scale universes until the tick budget is blown -------------------
static int runRamp(ShowSpec base, const QString &fixtureDir, const QString &scriptsDir, int ticks)
{
    const QList<int> steps = {10, 20, 40, 60, 80, 100, 140, 180};
    fprintf(stdout, "ramp: scaling universes, %d ticks each (budget %.1f ms)\n", ticks, kTickBudgetMs);
    int lastGood = 0;
    for (int n : steps)
    {
        ShowSpec spec = base;
        spec.universes = n;
        Doc *doc = new Doc(nullptr, n);
        QList<Function*> funcs = buildShow(doc, spec, fixtureDir, scriptsDir);
        TickStats s = runFlatout(doc, funcs, ticks, 50);
        QString label = QString("uni=%1").arg(n);
        printStats(label, s);
        emitJson("ramp", QString("ramp-uni-%1").arg(n), spec, s, residentMB(), kTickBudgetMs);
        bool ok = s.p99 < kTickBudgetMs;
        if (ok) lastGood = n;
        delete doc; // functions already stopped synchronously inside runFlatout
        if (!ok)
        {
            fprintf(stdout, "ramp: BROKE at %d universes (p99 %.3f ms > budget). Last sustainable: %d universes.\n",
                    n, s.p99, lastGood);
            break;
        }
    }
    if (g_json)
        fprintf(stdout, "RESULT_JSON {\"mode\":\"ramp\",\"scenario\":\"ramp-ceiling\",\"ceilingUniverses\":%d}\n", lastGood);
    fprintf(stdout, "ramp: largest sustainable configuration: %d universes within budget.\n", lastGood);
    return 0;
}

// ---- GOLDEN: hash the DMX frames a show produces (determinism regression) ---
// Runs flatout for N ticks; each tick hashes the post-GM bytes of every
// universe. capture writes the per-tick hashes; verify compares and reports the
// first divergent tick (the workspace must be deterministic: no random scripts).
static int runGolden(Doc *doc, const QList<Function*> &funcs, int ticks,
                     const QString &goldenFile, bool verify)
{
    MasterTimer *mt = doc->masterTimer();
    doc->setMode(Doc::Operate);
    startAll(doc, funcs);
    QList<Universe*> universes = doc->inputOutputMap()->claimUniverses();

    const int warmup = 50;
    for (int i = 0; i < warmup; i++)
    {
        mt->timerTickFunctions(universes);
        foreach (Universe *u, universes) u->processFaders();
    }

    QVector<quint64> perTick;
    perTick.reserve(ticks);
    for (int i = 0; i < ticks; i++)
    {
        mt->timerTickFunctions(universes);
        quint64 h = 1469598103934665603ULL;
        foreach (Universe *u, universes)
        {
            u->processFaders();
            const QByteArray *pg = u->postGMValues();
            h = fnv1a(reinterpret_cast<const uchar*>(pg->constData()), pg->size(), h);
        }
        perTick.append(h);
    }
    stopAllSync(doc, universes);
    doc->inputOutputMap()->releaseUniverses();

    quint64 combined = fnv1a(reinterpret_cast<const uchar*>(perTick.constData()),
                             perTick.size() * int(sizeof(quint64)));

    if (!verify)
    {
        QFile f(goldenFile);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        { fprintf(stderr, "golden: cannot write %s\n", qPrintable(goldenFile)); return 2; }
        QTextStream out(&f);
        out << "ticks " << ticks << "\n" << "combined " << QString::number(combined, 16) << "\n";
        for (quint64 h : perTick) out << QString::number(h, 16) << "\n";
        f.close();
        fprintf(stdout, "golden: captured %d frame-hashes to %s (combined %llx)\n",
                ticks, qPrintable(goldenFile), combined);
        return 0;
    }

    // verify
    QFile f(goldenFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    { fprintf(stderr, "golden: cannot read %s (capture first)\n", qPrintable(goldenFile)); return 2; }
    QTextStream in(&f);
    in.readLine(); // ticks
    QString combLine = in.readLine();
    quint64 storedComb = combLine.section(' ', 1, 1).toULongLong(nullptr, 16);
    if (storedComb == combined)
    {
        fprintf(stdout, "golden: MATCH (%d ticks, combined %llx) - output is deterministic & unchanged\n",
                ticks, combined);
        return 0;
    }
    // find first divergent tick
    int divergeTick = -1;
    for (int i = 0; i < perTick.size(); i++)
    {
        QString line = in.readLine();
        if (line.isNull()) { divergeTick = i; break; }
        if (line.toULongLong(nullptr, 16) != perTick[i]) { divergeTick = i; break; }
    }
    fprintf(stdout, "golden: MISMATCH - output changed (combined %llx vs stored %llx); first divergence at tick %d\n",
            combined, storedComb, divergeTick);
    return 1;
}

// ---- SOAK: long realtime run, sample RSS/FD/jitter, report leak rate ---------
static int runSoak(Doc *doc, const QList<Function*> &funcs, int seconds, int sampleSec)
{
    MasterTimer *mt = doc->masterTimer();
    doc->setMode(Doc::Operate);
    doc->inputOutputMap()->startUniverses();
    mt->start();
    startAll(doc, funcs);

    fprintf(stdout, "soak: %d s at %u Hz, sampling every %d s (running funcs target=%d)\n",
            seconds, MasterTimer::frequency(), sampleSec, funcs.size());

    struct Sample { double t, rss; int fd, funcs; };
    QVector<Sample> samples;
    QElapsedTimer wall; wall.start();
    qint64 nextSample = 0;
    double rss0 = residentMB(), rssPeak = rss0;
    int fd0 = openFdCount();

    while (wall.elapsed() < qint64(seconds) * 1000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        rssPeak = qMax(rssPeak, residentMB());
        if (wall.elapsed() >= nextSample)
        {
            double t = wall.elapsed() / 1000.0;
            double rss = residentMB();
            int fd = openFdCount();
            int rf = mt->runningFunctions();
            samples.append({t, rss, fd, rf});
            fprintf(stdout, "soak: t=%6.0fs  rss=%7.1f MB  fd=%d  runningFuncs=%d\n", t, rss, fd, rf);
            fflush(stdout);
            nextSample += qint64(sampleSec) * 1000;
        }
    }
    mt->stop();

    // Leak rate via least-squares slope of rss over time (MB/hr). Skip the first
    // ~60s (or the first sample) so the startup RSS ramp (functions spinning up,
    // caches warming) isn't mistaken for a leak. Meaningful only for runs of a
    // few minutes+; short runs are reported but flagged as low-confidence.
    double leakPerHr = 0;
    QVector<Sample> fit;
    for (auto &s : samples) if (s.t >= 60.0) fit.append(s);
    if (fit.size() < 2 && samples.size() >= 2)        // run too short to warm up
        fit = samples.mid(1);                         // at least drop the first point
    int n = fit.size();
    if (n >= 2)
    {
        double sx=0, sy=0, sxx=0, sxy=0;
        for (auto &s : fit) { sx+=s.t; sy+=s.rss; sxx+=s.t*s.t; sxy+=s.t*s.rss; }
        double denom = n*sxx - sx*sx;
        if (denom != 0) leakPerHr = (n*sxy - sx*sy) / denom * 3600.0;
    }
    bool lowConfidence = (seconds < 300);
    int fdEnd = samples.isEmpty() ? fd0 : samples.last().fd;
    fprintf(stdout, "soak: rss start=%.1f peak=%.1f end=%.1f MB; leak rate=%.2f MB/hr%s; fd start=%d end=%d\n",
            rss0, rssPeak, samples.isEmpty()?rss0:samples.last().rss, leakPerHr,
            lowConfidence ? " (low confidence: run >=5min for a real figure)" : "", fd0, fdEnd);
    if (g_json)
        fprintf(stdout, "RESULT_JSON {\"mode\":\"soak\",\"scenario\":\"%s\",\"seconds\":%d,"
                "\"rssStart\":%.1f,\"rssEnd\":%.1f,\"rssPeak\":%.1f,\"leakMBperHr\":%.2f,"
                "\"fdStart\":%d,\"fdEnd\":%d,\"samples\":%d}\n",
                qPrintable(g_scenario), seconds, rss0, samples.isEmpty()?rss0:samples.last().rss,
                rssPeak, leakPerHr, fd0, fdEnd, n);
    return 0;
}

// ---- load a .qxw into a Doc (positions reader at <Engine> then loadXML) ------
static bool loadWorkspace(Doc *doc, const QString &path, QString &err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { err = "cannot open file"; return false; }
    QXmlStreamReader r(&f);
    if (!r.readNextStartElement()) { err = "empty/invalid XML"; return false; }
    if (r.name().toString() != QLatin1String("Workspace")) { err = "root is not <Workspace>"; return false; }
    while (r.readNextStartElement())
    {
        if (r.name().toString() == QLatin1String("Engine"))
            return doc->loadXML(r);
        r.skipCurrentElement();
    }
    err = "no <Engine> node";
    return false;
}

// ---- LOAD: load a workspace, report (robustness/corpus/fuzz target) ----------
static int runLoad(const QString &path, const QString &fixtureDir)
{
    Doc doc(nullptr, 4);
    QDir dir(fixtureDir);
    doc.fixtureDefCache()->loadMap(dir);
    QString err;
    bool ok = loadWorkspace(&doc, path, err);
    fprintf(stdout, "load: %s -> %s (%d fixtures, %d functions)%s%s\n",
            qPrintable(path), ok ? "OK" : "FAILED",
            int(doc.fixtures().size()), int(doc.functions().size()),
            ok ? "" : " err=", ok ? "" : qPrintable(err));
    return ok ? 0 : 1;
}

// ---- ROUNDTRIP: load -> save -> reload -> save, assert byte-identical --------
static int saveWorkspace(Doc *doc, const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return 2;
    QXmlStreamWriter w(&f);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeDTD("<!DOCTYPE Workspace>");
    w.writeStartElement("Workspace");
    doc->saveXML(&w);
    w.writeEndElement();
    w.writeEndDocument();
    f.close();
    return 0;
}

static int runRoundtrip(const QString &path, const QString &fixtureDir)
{
    QDir dir(fixtureDir);
    Doc d1(nullptr, 16); d1.fixtureDefCache()->loadMap(dir);
    QString err;
    if (!loadWorkspace(&d1, path, err))
    { fprintf(stderr, "roundtrip: initial load failed: %s\n", qPrintable(err)); return 2; }
    QString f1 = path + ".rt1", f2 = path + ".rt2";
    saveWorkspace(&d1, f1);

    Doc d2(nullptr, 16); d2.fixtureDefCache()->loadMap(dir);
    if (!loadWorkspace(&d2, f1, err))
    { fprintf(stderr, "roundtrip: reload failed: %s\n", qPrintable(err)); return 1; }
    saveWorkspace(&d2, f2);

    QFile a(f1), b(f2);
    a.open(QIODevice::ReadOnly); b.open(QIODevice::ReadOnly);
    bool identical = (a.readAll() == b.readAll());
    fprintf(stdout, "roundtrip: %s -> %s (load/save/reload/save %s; %d->%d fixtures, %d->%d functions)\n",
            qPrintable(path), identical ? "STABLE" : "DRIFT",
            identical ? "identical" : "DIFFERS",
            int(d1.fixtures().size()), int(d2.fixtures().size()),
            int(d1.functions().size()), int(d2.functions().size()));
    QFile::remove(f1); QFile::remove(f2);
    return identical ? 0 : 1;
}

// ---- CHAOS: hammer object lifecycle at runtime (UAF/dangling under ASan) -----
// Realistic by default: before any structural delete it stops all functions
// (the engine teardown the app performs on leaving Operate mode), so deletes
// can't race the timer thread. With aggressive=true it deletes while functions
// run, reproducing the Doc::deleteFunction / deleteFixture use-after-free
// (timer thread in timerTickFunctions vs delete on the edit thread).
static int runChaos(Doc *doc, const ShowSpec &spec, const QString &fixtureDir,
                    const QString &scriptsDir, int seconds, bool aggressive)
{
    QList<Function*> funcs = buildShow(doc, spec, fixtureDir, scriptsDir);
    QLCFixtureDef *rgbDef = doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    QLCFixtureMode *rgbMode = (rgbDef && !rgbDef->modes().isEmpty()) ? rgbDef->modes().first() : nullptr;

    MasterTimer *mt = doc->masterTimer();
    doc->setMode(Doc::Operate);
    doc->inputOutputMap()->startUniverses();
    mt->start();

    QRandomGenerator rng(spec.seed);
    QElapsedTimer wall; wall.start();
    int ops = 0, addr = 0;
    fprintf(stdout, "chaos: %d s of runtime add/remove/start/stop/delete/mode-toggle (%s)\n",
            seconds, aggressive ? "AGGRESSIVE: unsafe deletes" : "realistic: stop-before-delete");

    while (wall.elapsed() < qint64(seconds) * 1000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QList<quint32> fnIds; for (Function *f : doc->functions()) fnIds << f->id();
        QList<quint32> fxIds; for (Fixture *f : doc->fixtures()) fxIds << f->id();

        switch (rng.bounded(7))
        {
            case 0: if (!fnIds.isEmpty()) { Function *f = doc->function(fnIds.at(rng.bounded(fnIds.size()))); if (f) f->start(mt, FunctionParent::master()); } break;
            case 1: if (!fnIds.isEmpty()) { Function *f = doc->function(fnIds.at(rng.bounded(fnIds.size()))); if (f) f->stop(FunctionParent::master()); } break;
            case 2: { // add a scene touching random fixtures
                if (!fxIds.isEmpty()) {
                    Scene *s = new Scene(doc); s->setName("ChaosScene");
                    for (int k = 0; k < 5; k++) { quint32 fid = fxIds.at(rng.bounded(fxIds.size())); s->setValue(fid, rng.bounded(3), uchar(rng.bounded(256))); }
                    if (!doc->addFunction(s)) delete s;
                } break; }
            case 3: // delete a function (UNSAFE against the running engine)
                // Runtime function deletion has no safe engine-level path: it
                // races the MasterTimer (UAF) AND leaves dangling refs in any
                // Collection/Chaser/Show that referenced it. Only aggressive mode
                // does it (to reproduce those crashes); realistic mode skips it.
                if (aggressive && fnIds.size() > 5)
                    doc->deleteFunction(fnIds.at(rng.bounded(fnIds.size())));
                break;
            case 4: { // add a fixture
                if (rgbDef && rgbMode) {
                    Fixture *fx = new Fixture(doc); fx->setName("ChaosFix");
                    fx->setUniverse(rng.bounded(qMax(1, spec.universes)));
                    fx->setAddress((addr += 3) % 500);
                    fx->setFixtureDefinition(rgbDef, rgbMode);
                    if (!doc->addFixture(fx)) delete fx;
                } break; }
            case 5: // delete a fixture (UNSAFE: dangling refs in scenes/groups)
                if (aggressive && fxIds.size() > 10)
                    doc->deleteFixture(fxIds.at(rng.bounded(fxIds.size())));
                break;
            case 6: doc->setMode(doc->mode() == Doc::Operate ? Doc::Design : Doc::Operate); break;
        }
        ops++;
    }
    doc->setMode(Doc::Operate);
    mt->stopAllFunctions();
    mt->stop();
    fprintf(stdout, "chaos: survived %d ops; final %d fixtures, %d functions; RSS %.1f MB\n",
            ops, int(doc->fixtures().size()), int(doc->functions().size()), residentMB());
    return 0;
}

// ---- EMIT: build show and save canonical .qxw -------------------------------
static int runEmit(Doc *doc, const ShowSpec &spec, const QString &outPath)
{
    QFile f(outPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        fprintf(stderr, "emit: cannot open %s for writing\n", qPrintable(outPath));
        return 2;
    }
    QXmlStreamWriter w(&f);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeDTD("<!DOCTYPE Workspace>");
    w.writeStartElement("Workspace");
    w.writeAttribute("CurrentWindow", "FunctionManager");
    w.writeStartElement("Creator");
    w.writeTextElement("Name", "Q Light Controller Plus");
    w.writeTextElement("Version", "4.14.4");
    w.writeTextElement("Author", "qlcstress");
    w.writeEndElement(); // Creator
    doc->saveXML(&w);     // writes the full <Engine> section
    w.writeEndElement();  // Workspace
    w.writeEndDocument();
    f.close();
    fprintf(stdout, "emit: wrote %s  (%d universes, %d fixtures, %d channels, %d functions)\n",
            qPrintable(outPath), spec.universes, spec.totalFixtures(),
            spec.totalChannels(), int(doc->functions().size()));
    return 0;
}

// ---- argument plumbing ------------------------------------------------------
// Swallow the engine's copious qDebug/qWarning chatter so stress output stays
// readable. Set QLC_STRESS_VERBOSE=1 to see it. qCritical/qFatal always pass.
static void quietHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    static bool verbose = qEnvironmentVariableIsSet("QLC_STRESS_VERBOSE");
    if (verbose || type == QtCriticalMsg || type == QtFatalMsg)
        fprintf(stderr, "%s\n", qPrintable(msg));
    Q_UNUSED(ctx);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(quietHandler);
    setvbuf(stdout, nullptr, _IOLBF, 0); // line-buffered so progress is visible when piped
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("qlcplus");
    QCoreApplication::setApplicationName("qlcstress");

    QCommandLineParser p;
    p.setApplicationDescription("QLC+ engine stress / torture harness");
    p.addHelpOption();
    p.addPositionalArgument("command", "engine | emit | golden | load | roundtrip");
    p.addPositionalArgument("file", ".qxw file (emit/golden/load/roundtrip)", "[file]");

    QCommandLineOption oUni("universes", "number of universes", "n", "16");
    QCommandLineOption oFpu("fixtures-per-uni", "RGB fixtures per universe", "n", "100");
    QCommandLineOption oScenes("scenes", "scene count", "n", "200");
    QCommandLineOption oChasers("chasers", "chaser count", "n", "40");
    QCommandLineOption oMatrices("matrices", "RGB matrix count", "n", "16");
    QCommandLineOption oEfx("efx", "EFX count", "n", "16");
    QCommandLineOption oColl("collections", "collection count", "n", "8");
    QCommandLineOption oSeed("seed", "RNG seed", "n", "1");
    QCommandLineOption oMode("mode", "flatout | realtime | ramp | soak | chaos", "mode", "flatout");
    QCommandLineOption oTicks("ticks", "flatout/ramp/golden tick count", "n", "5000");
    QCommandLineOption oSecs("seconds", "realtime/soak/chaos duration", "s", "15");
    QCommandLineOption oSampleSec("sample-seconds", "soak sampling interval", "s", "30");
    QCommandLineOption oGoldenFile("golden-file", "golden hashes file", "path", "golden.txt");
    QCommandLineOption oGoldenVerify("golden-verify", "verify against golden-file instead of capturing");
    QCommandLineOption oChaosAggr("chaos-aggressive", "chaos: delete running functions (reproduces the deleteFunction UAF)");
    QCommandLineOption oFreq("freq", "MasterTimer frequency Hz (0=default 50)", "hz", "0");
    QCommandLineOption oFix("fixtures-dir", "resources/fixtures path", "dir",
                            "../../resources/fixtures");
    QCommandLineOption oScr("scripts-dir", "resources/rgbscripts path", "dir",
                            "../../resources/rgbscripts");
    QCommandLineOption oScripts("scripts", "Script function count", "n", "0");
    QCommandLineOption oSequences("sequences", "Sequence function count", "n", "0");
    QCommandLineOption oShows("shows", "Show function count", "n", "0");
    QCommandLineOption oJson("json", "emit RESULT_JSON lines for the orchestrator");
    QCommandLineOption oScenario("scenario", "scenario name (for JSON)", "name", "adhoc");
    p.addOptions({oUni, oFpu, oScenes, oChasers, oMatrices, oEfx, oColl, oSeed,
                  oMode, oTicks, oSecs, oFreq, oFix, oScr,
                  oScripts, oSequences, oShows, oJson, oScenario,
                  oSampleSec, oGoldenFile, oGoldenVerify, oChaosAggr});
    p.process(app);

    const QStringList args = p.positionalArguments();
    if (args.isEmpty())
    {
        fprintf(stderr, "error: missing command (engine | emit)\n");
        return 2;
    }
    const QString cmd = args.first();

    // optional MasterTimer frequency override (read by MasterTimer ctor)
    uint freq = oFreq.names().isEmpty() ? 0 : p.value(oFreq).toUInt();
    if (freq > 0)
    {
        QSettings s;
        s.setValue("mastertimer/frequency", freq);
        s.sync();
    }

    ShowSpec spec;
    spec.universes      = p.value(oUni).toInt();
    spec.fixturesPerUni = p.value(oFpu).toInt();
    spec.scenes         = p.value(oScenes).toInt();
    spec.chasers        = p.value(oChasers).toInt();
    spec.matrices       = p.value(oMatrices).toInt();
    spec.efx            = p.value(oEfx).toInt();
    spec.collections    = p.value(oColl).toInt();
    spec.scripts        = p.value(oScripts).toInt();
    spec.sequences      = p.value(oSequences).toInt();
    spec.shows          = p.value(oShows).toInt();
    spec.seed           = p.value(oSeed).toUInt();
    g_json              = p.isSet(oJson);
    g_scenario          = p.value(oScenario);
    const QString fixDir = p.value(oFix);
    const QString scrDir = p.value(oScr);
    const QString mode   = p.value(oMode);
    const int ticks      = p.value(oTicks).toInt();
    const int seconds    = p.value(oSecs).toInt();

    fprintf(stdout, "qlcstress: %s spec: universes=%d fix/uni=%d -> %d fixtures, %d channels; "
            "scenes=%d chasers=%d matrices=%d efx=%d collections=%d (seed %u)\n",
            qPrintable(cmd), spec.universes, spec.fixturesPerUni, spec.totalFixtures(),
            spec.totalChannels(), spec.scenes, spec.chasers, spec.matrices, spec.efx,
            spec.collections, spec.seed);
    fflush(stdout);

    if (cmd == "emit")
    {
        if (args.size() < 2) { fprintf(stderr, "emit: need output file\n"); return 2; }
        Doc doc(nullptr, spec.universes);
        buildShow(&doc, spec, fixDir, scrDir);
        return runEmit(&doc, spec, args.at(1));
    }

    if (cmd == "load")
    {
        if (args.size() < 2) { fprintf(stderr, "load: need .qxw file\n"); return 2; }
        return runLoad(args.at(1), fixDir);
    }

    if (cmd == "roundtrip")
    {
        if (args.size() < 2) { fprintf(stderr, "roundtrip: need .qxw file\n"); return 2; }
        return runRoundtrip(args.at(1), fixDir);
    }

    if (cmd == "golden")
    {
        if (args.size() < 2) { fprintf(stderr, "golden: need golden hashes file\n"); return 2; }
        Doc *doc = new Doc(nullptr, spec.universes);
        QList<Function*> funcs = buildShow(doc, spec, fixDir, scrDir);
        int rc = runGolden(doc, funcs, ticks, args.at(1), p.isSet(oGoldenVerify));
        delete doc;
        return rc;
    }

    if (cmd == "engine")
    {
        if (mode == "ramp")
            return runRamp(spec, fixDir, scrDir, ticks);

        Doc *doc = new Doc(nullptr, spec.universes);
        QList<Function*> funcs = buildShow(doc, spec, fixDir, scrDir);
        int rc = 0;
        if (mode == "realtime")
            rc = runRealtime(doc, funcs, seconds, spec);
        else if (mode == "soak")
            rc = runSoak(doc, funcs, seconds, p.value(oSampleSec).toInt());
        else if (mode == "chaos")
            rc = runChaos(doc, spec, fixDir, scrDir, seconds, p.isSet(oChaosAggr));
        else
        {
            TickStats s = runFlatout(doc, funcs, ticks, 50);
            printStats("flatout", s);
            fprintf(stdout, "flatout: %s (p99 %.3f ms vs %.1f ms budget); RSS %.1f MB\n",
                    s.p99 < kTickBudgetMs ? "WITHIN budget" : "OVER budget",
                    s.p99, kTickBudgetMs, residentMB());
            emitJson("flatout", g_scenario, spec, s, residentMB(), kTickBudgetMs);
            rc = s.p99 < kTickBudgetMs ? 0 : 1;
        }
        if (mode == "realtime")
            doc->masterTimer()->stopAllFunctions(); // timer thread is running here
        delete doc;
        return rc;
    }

    fprintf(stderr, "error: unknown command '%s'\n", qPrintable(cmd));
    return 2;
}
