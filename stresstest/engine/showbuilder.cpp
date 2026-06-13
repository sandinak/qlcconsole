/*
  QLC+ stress test - parametric show builder implementation
*/

#include "showbuilder.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QColor>
#include <QSize>
#include <QDir>
#include <QtGlobal>
#include <QDebug>

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "scene.h"
#include "chaser.h"
#include "chaserstep.h"
#include "rgbmatrix.h"
#include "rgbalgorithm.h"
#include "rgbscriptscache.h"
#include "efx.h"
#include "efxfixture.h"
#include "collection.h"
#include "script.h"
#include "sequence.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "function.h"
#include "grouphead.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcfile.h"

// RGBMatrix script algorithms that ship with QLC+ (display names as used by
// RGBAlgorithm::algorithm()). Span cheap and expensive per-tick patterns.
static const char *kMatrixAlgos[] = {
    "Stripes", "Balls", "Plasma", "Noise", "Fireworks", "Gradient"
};
static const int kMatrixAlgoCount = int(sizeof(kMatrixAlgos) / sizeof(kMatrixAlgos[0]));

QList<Function*> buildShow(Doc *doc, const ShowSpec &spec,
                           const QString &fixtureDir, const QString &scriptsDir)
{
    QList<Function*> topLevel;
    QRandomGenerator rng(spec.seed);

    // RGB script cache must be loaded or RGBAlgorithm::algorithm() returns null
    // and every RGBMatrix becomes a no-op (no per-tick work).
    if (doc->rgbScriptsCache()->load(QDir(scriptsDir)) == false ||
        doc->rgbScriptsCache()->names().isEmpty())
        qWarning() << "stress: RGB scripts NOT loaded from" << scriptsDir
                   << "- matrices will be no-ops!";

    // ---- fixture definitions -------------------------------------------------
    QDir dir(fixtureDir);
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QString("*%1").arg(KExtFixture));
    if (doc->fixtureDefCache()->loadMap(dir) == false)
        qWarning() << "stress: failed to load fixture map from" << fixtureDir;

    QLCFixtureDef *rgbDef = doc->fixtureDefCache()->fixtureDef("Generic", "Generic RGB");
    QLCFixtureMode *rgbMode = nullptr;
    if (rgbDef != nullptr)
    {
        foreach (QLCFixtureMode *m, rgbDef->modes())
            if (m->name() == "RGB") { rgbMode = m; break; }
        if (rgbMode == nullptr && !rgbDef->modes().isEmpty())
            rgbMode = rgbDef->modes().first();
    }

    const int fpu = qMin(spec.fixturesPerUni, 512 / ShowSpec::channelsPerFix);

    // ---- fixtures ------------------------------------------------------------
    QList<quint32> fixtureIds;
    fixtureIds.reserve(spec.universes * fpu);
    for (int u = 0; u < spec.universes; u++)
    {
        for (int i = 0; i < fpu; i++)
        {
            Fixture *fx = new Fixture(doc);
            fx->setName(QString("U%1-F%2").arg(u).arg(i));
            fx->setUniverse(u);
            fx->setAddress(i * ShowSpec::channelsPerFix);
            if (rgbDef != nullptr && rgbMode != nullptr)
                fx->setFixtureDefinition(rgbDef, rgbMode);
            else
                fx->setChannels(ShowSpec::channelsPerFix); // fallback: headless intensity
            if (doc->addFixture(fx) == false)
            {
                delete fx;
                continue;
            }
            fixtureIds.append(fx->id());
        }
    }
    fprintf(stdout, "stress: created %d fixtures across %d universes (%d channels)\n",
            int(fixtureIds.size()), spec.universes,
            int(fixtureIds.size()) * ShowSpec::channelsPerFix);

    if (fixtureIds.isEmpty())
        return topLevel;

    // ---- scenes --------------------------------------------------------------
    QList<quint32> sceneIds;
    for (int s = 0; s < spec.scenes; s++)
    {
        Scene *scene = new Scene(doc);
        scene->setName(QString("Scene %1").arg(s));
        // touch a random slice of fixtures so different scenes overlap channels
        int touch = qMax(1, fixtureIds.size() / 8);
        for (int t = 0; t < touch; t++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            for (int c = 0; c < ShowSpec::channelsPerFix; c++)
                scene->setValue(fid, c, uchar(rng.bounded(256)));
        }
        scene->setFadeInSpeed(rng.bounded(2000));
        scene->setFadeOutSpeed(rng.bounded(2000));
        if (doc->addFunction(scene) == false) { delete scene; continue; }
        sceneIds.append(scene->id());
        topLevel.append(scene);
    }

    // ---- chasers (step through scenes) --------------------------------------
    QList<quint32> chaserIds;
    for (int c = 0; c < spec.chasers && !sceneIds.isEmpty(); c++)
    {
        Chaser *chaser = new Chaser(doc);
        chaser->setName(QString("Chaser %1").arg(c));
        chaser->setDurationMode(Chaser::Common);
        int steps = 4 + rng.bounded(12);
        for (int st = 0; st < steps; st++)
        {
            quint32 sid = sceneIds.at(rng.bounded(sceneIds.size()));
            chaser->addStep(ChaserStep(sid, rng.bounded(500), 200 + rng.bounded(800), rng.bounded(500)));
        }
        chaser->setRunOrder(Function::Loop);
        chaser->setDirection(Function::Forward);
        if (doc->addFunction(chaser) == false) { delete chaser; continue; }
        chaserIds.append(chaser->id());
        topLevel.append(chaser);
    }

    // ---- RGB matrices (the CPU-heavy per-tick functions) --------------------
    QList<quint32> matrixIds;
    for (int m = 0; m < spec.matrices; m++)
    {
        // build a fixture group covering a chunk of fixtures laid out as a grid
        FixtureGroup *grp = new FixtureGroup(doc);
        grp->setName(QString("Group %1").arg(m));
        int side = 8 + rng.bounded(9);               // 8..16 -> up to 256 heads
        grp->setSize(QSize(side, side));
        int want = side * side;
        for (int k = 0; k < want; k++)
            grp->assignFixture(fixtureIds.at(rng.bounded(fixtureIds.size())));
        if (doc->addFixtureGroup(grp) == false) { delete grp; continue; }

        RGBMatrix *mtx = new RGBMatrix(doc);
        mtx->setName(QString("Matrix %1").arg(m));
        mtx->setFixtureGroup(grp->id());
        RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc, kMatrixAlgos[m % kMatrixAlgoCount]);
        if (algo != nullptr)
            mtx->setAlgorithm(algo);
        mtx->setColor(0, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setColor(1, QColor(rng.bounded(256), rng.bounded(256), rng.bounded(256)));
        mtx->setFadeInSpeed(0);
        mtx->setDuration(100 + rng.bounded(400)); // step every 100-500ms
        if (doc->addFunction(mtx) == false) { delete mtx; continue; }
        matrixIds.append(mtx->id());
        topLevel.append(mtx);
    }

    // ---- EFX -----------------------------------------------------------------
    for (int e = 0; e < spec.efx; e++)
    {
        EFX *efx = new EFX(doc);
        efx->setName(QString("EFX %1").arg(e));
        efx->setAlgorithm(EFX::Algorithm(e % 6)); // Circle..Square
        efx->setWidth(64 + rng.bounded(64));
        efx->setHeight(64 + rng.bounded(64));
        int heads = 4 + rng.bounded(16);
        for (int h = 0; h < heads; h++)
        {
            EFXFixture *ef = new EFXFixture(efx);
            ef->setHead(GroupHead(fixtureIds.at(rng.bounded(fixtureIds.size())), 0));
            ef->setMode(EFXFixture::RGB);
            efx->addFixture(ef);
        }
        efx->setDuration(2000 + rng.bounded(3000));
        if (doc->addFunction(efx) == false) { delete efx; continue; }
        topLevel.append(efx);
    }

    // ---- collections (group scenes + chasers) -------------------------------
    for (int c = 0; c < spec.collections; c++)
    {
        Collection *col = new Collection(doc);
        col->setName(QString("Collection %1").arg(c));
        int members = 3 + rng.bounded(6);
        for (int k = 0; k < members; k++)
        {
            if (!sceneIds.isEmpty())
                col->addFunction(sceneIds.at(rng.bounded(sceneIds.size())));
            if (!chaserIds.isEmpty())
                col->addFunction(chaserIds.at(rng.bounded(chaserIds.size())));
        }
        if (doc->addFunction(col) == false) { delete col; continue; }
        topLevel.append(col);
    }

    // ---- Script functions (exercise the script executor every tick) --------
    for (int s = 0; s < spec.scripts; s++)
    {
        Script *scr = new Script(doc);
        scr->setName(QString("Script %1").arg(s));
        QString body = "label:loop\n";
        int sets = 4 + rng.bounded(8);
        for (int k = 0; k < sets; k++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            body += QString("setfixture:%1 ch:%2 val:%3\n")
                        .arg(fid).arg(rng.bounded(ShowSpec::channelsPerFix)).arg(rng.bounded(256));
        }
        // occasionally drive another function, then wait and loop forever
        if (!sceneIds.isEmpty())
            body += QString("startfunction:%1\n").arg(sceneIds.at(rng.bounded(sceneIds.size())));
        body += "wait:40\njump:loop\n";
        scr->setData(body);
        if (doc->addFunction(scr) == false) { delete scr; continue; }
        topLevel.append(scr);
    }

    // ---- Sequences (scene-bound chasers) -----------------------------------
    QList<quint32> sequenceIds;
    for (int s = 0; s < spec.sequences; s++)
    {
        // each sequence owns a bound scene whose values its steps animate
        Scene *bound = new Scene(doc);
        bound->setName(QString("SeqScene %1").arg(s));
        int touch = qMax(1, fixtureIds.size() / 16);
        for (int t = 0; t < touch; t++)
        {
            quint32 fid = fixtureIds.at(rng.bounded(fixtureIds.size()));
            for (int c = 0; c < ShowSpec::channelsPerFix; c++)
                bound->setValue(fid, c, uchar(rng.bounded(256)));
        }
        if (doc->addFunction(bound) == false) { delete bound; continue; }

        Sequence *seq = new Sequence(doc);
        seq->setName(QString("Sequence %1").arg(s));
        seq->setBoundSceneID(bound->id());
        int steps = 4 + rng.bounded(10);
        for (int st = 0; st < steps; st++)
            seq->addStep(ChaserStep(bound->id(), rng.bounded(400), 150 + rng.bounded(600), rng.bounded(400)));
        seq->setRunOrder(Function::Loop);
        if (doc->addFunction(seq) == false) { delete seq; continue; }
        sequenceIds.append(seq->id());
        topLevel.append(seq);
    }

    // ---- Shows (timeline tracks of existing functions) ---------------------
    // candidate functions a show track can schedule
    QList<quint32> showable = sceneIds + chaserIds + sequenceIds;
    for (int sh = 0; sh < spec.shows && !showable.isEmpty(); sh++)
    {
        Show *show = new Show(doc);
        show->setName(QString("Show %1").arg(sh));
        if (doc->addFunction(show) == false) { delete show; continue; }

        int tracks = 2 + rng.bounded(4);
        for (int tr = 0; tr < tracks; tr++)
        {
            Track *track = new Track();
            track->setName(QString("Track %1.%2").arg(sh).arg(tr));
            show->addTrack(track);
            quint32 t = 0;
            int items = 3 + rng.bounded(5);
            for (int it = 0; it < items; it++)
            {
                quint32 fid = showable.at(rng.bounded(showable.size()));
                ShowFunction *sf = track->createShowFunction(fid);
                sf->setStartTime(t);
                quint32 dur = 1000 + rng.bounded(4000);
                sf->setDuration(dur);
                t += dur + rng.bounded(500); // stagger along the timeline
            }
        }
        topLevel.append(show);
    }

    fprintf(stdout, "stress: created %d functions (%d scenes, %d chasers, %d matrices, %d efx, "
            "%d collections, %d scripts, %d sequences, %d shows)\n",
            int(doc->functions().size()), int(sceneIds.size()), int(chaserIds.size()),
            int(matrixIds.size()), spec.efx, spec.collections, spec.scripts,
            int(sequenceIds.size()), spec.shows);

    return topLevel;
}
