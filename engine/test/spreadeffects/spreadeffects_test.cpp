/*
  Q Light Controller Plus - Unit test
  spreadeffects_test.cpp

  Fire Spread and Spell Spread (lib/spread.js) on the kinds of light the
  show they were made for has: what each kind gets, and what a tick costs
  over its 6,144 step pixels.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <cmath>

#include "spreadeffects_test.h"
#include "../common/stdprelude.h"
#define private public                  // the instance's insides
#define protected public                // Universe::processFaders()
#include "doc.h"
#include "fixture.h"
#include "scene.h"
#include "mastertimer.h"
#include "qlcpalette.h"
#include "effectruns.h"
#include "qlcfixturedef.h"
#include "qlcfixturedefcache.h"
#include "qlcfixturemode.h"
#include "effectinstance.h"
#include "effectscript.h"
#include "effectscriptcache.h"
#include "universe.h"
#include "inputoutputmap.h"
#undef protected
#undef private
#include "monitorproperties.h"
#include "stagetarget.h"

namespace
{
const QString kScripts = QString(QLC_TEST_RESOURCE_ROOT) + "effectscripts/";
const QString kFixtures = QString(QLC_TEST_RESOURCE_ROOT) + "fixtures/";
const QString kData = QString(SPREADEFFECTS_DATA);

/* A definition from a file, owned by the doc's cache (or the copy it
   already has). */
QLCFixtureDef *loadDef(Doc *doc, const QString &path)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    if (def->loadXML(path) != QFile::NoError)
    {
        delete def;
        return nullptr;
    }
    QLCFixtureDef *known = doc->fixtureDefCache()->fixtureDef(def->manufacturer(), def->model());
    if (known != nullptr)
    {
        delete def;
        return known;
    }
    doc->fixtureDefCache()->addFixtureDef(def);
    return def;
}

/* A fixture of @p def in @p mode, standing at @p mm (millimetres, the
   plan's own units). */
Fixture *place(Doc *doc, QLCFixtureDef *def, const QString &mode, const QString &name,
               quint32 universe, quint32 address, const QVector3D &mm)
{
    if (def == nullptr || def->mode(mode) == nullptr)
        return nullptr;
    Fixture *f = new Fixture(doc);
    f->setFixtureDefinition(def, def->mode(mode));
    f->setName(name);
    f->setUniverse(universe);
    f->setAddress(address);
    if (!doc->addFixture(f))
        return nullptr;
    doc->monitorProperties()->setFixturePosition(f->id(), 0, 0, mm);
    return f;
}

QLCPalette *effectPalette(Doc *doc, const QString &script, const QVariantMap &set)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Effect, doc);
    p->setScriptPath(kScripts + script);
    for (auto it = set.constBegin(); it != set.constEnd(); ++it)
        p->setEffectParamValue(it.key(), it.value().toDouble());
    p->setEffectParamValue(EffectRuns::runsKey(), 0);         // Loop
    doc->addPalette(p);
    return p;
}

QLCPalette *aimAt(Doc *doc, const QVector3D &metres)
{
    StageTarget *point = doc->monitorProperties()->addStageTarget();
    point->setName("Witch");
    point->setPosition(metres);
    QLCPalette *aim = new QLCPalette(QLCPalette::Aim, doc);
    aim->setStageTargetId(point->id());
    doc->addPalette(aim);
    return aim;
}

QLCPalette *colourPalette(Doc *doc, const QColor &c)
{
    QLCPalette *p = new QLCPalette(QLCPalette::Color, doc);
    p->setValue(c);
    doc->addPalette(p);
    return p;
}

quint64 key(quint32 fid, quint32 ch) { return (quint64(fid) << 32) | ch; }

/* What the effect writes at clock @p ms: (fixture, channel) -> value. */
QHash<quint64, int> frameAt(EffectInstance &inst, qint64 ms)
{
    inst.setOffStageClock(ms);
    inst.runTick();
    QHash<quint64, int> out;
    for (const EffectInstance::DmxWrite &w : inst.dmxWrites())
        out.insert(key(w.fixtureId, w.channel), w.value);
    return out;
}

/* A pixel of a pixel bar (3 channels a pixel) lit: any of its channels
   above 0. */
bool pixelLit(const QHash<quint64, int> &f, quint32 fid, int pixel)
{
    for (int c = 0; c < 3; ++c)
        if (f.value(key(fid, quint32(pixel * 3 + c)), 0) > 0)
            return true;
    return false;
}

bool pixelWritten(const QHash<quint64, int> &f, quint32 fid, int pixel)
{
    for (int c = 0; c < 3; ++c)
        if (f.contains(key(fid, quint32(pixel * 3 + c))))
            return true;
    return false;
}

/* One tick of @p script by itself: @p heads (each a map of the
   descriptor's fields) at @p inputs, the look's colour @p look (none when
   invalid), @p set over the defaults. One map an intent. */
QList<QVariantMap> scriptFrame(EffectScript &script, const QList<QVariantMap> &heads,
                               const QVariantMap &inputs, const QVariantMap &set,
                               const QColor &look = QColor(200, 0, 0))
{
    QJSEngine *eng = script.engine();
    QJSValue fixtures = eng->newArray(uint(heads.size()));
    for (int i = 0; i < heads.size(); ++i)
    {
        QJSValue o = eng->newObject();
        o.setProperty("heads", 1);
        for (auto it = heads.at(i).constBegin(); it != heads.at(i).constEnd(); ++it)
            o.setProperty(it.key(), eng->toScriptValue(it.value()));
        fixtures.setProperty(quint32(i), o);
    }
    QJSValue in = eng->newObject();
    for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it)
        in.setProperty(it.key(), eng->toScriptValue(it.value()));
    QJSValue params = eng->newObject();
    for (const EffectScript::ParamDef &d : script.paramDefs())
        params.setProperty(d.name, double(d.defaultValue));
    for (auto it = set.constBegin(); it != set.constEnd(); ++it)
        params.setProperty(it.key(), it.value().toDouble());
    QJSValue palettes = eng->newObject();
    QJSValue lookObj = eng->newObject();
    QJSValue colours = eng->newArray(look.isValid() ? 1 : 0);
    if (look.isValid())
    {
        QJSValue c = eng->newObject();
        c.setProperty("r", look.red());
        c.setProperty("g", look.green());
        c.setProperty("b", look.blue());
        colours.setProperty(0, c);
    }
    lookObj.setProperty("colors", colours);
    lookObj.setProperty("dimmer", 1.0);
    palettes.setProperty("look", lookObj);
    QJSValue state = script.newState();
    const QJSValue out = script.callTick(fixtures, in, palettes, params, state, eng->newObject());
    QList<QVariantMap> frame;
    for (int i = 0; i < heads.size(); ++i)
        frame << out.property(quint32(i)).toVariant().toMap();
    return frame;
}

bool lit(const QVariantMap &o)
{
    if (o.contains("dimmer") && o.value("dimmer").toDouble() <= 0)
        return false;
    return o.value("r").toInt() > 0 || o.value("g").toInt() > 0 || o.value("b").toInt() > 0
           || o.value("dimmer").toDouble() > 0;
}

QVariantMap rgb(int r, int g, int b) { return { { "r", r }, { "g", g }, { "b", b } }; }

QVariantMap with(QVariantMap m, const QString &k, const QVariant &v)
{
    m.insert(k, v);
    return m;
}
}

void SpreadEffects_Test::init()
{
    m_doc = new Doc(this);
}

void SpreadEffects_Test::cleanup()
{
    /* Not deleted: a scene still playing can't be stopped from this thread
       without the timer tick it would need. One Doc a test. */
    m_doc = nullptr;
}

void SpreadEffects_Test::spellSpreadReachesEachLightInTurn()
{
    EffectScript ss;
    QVERIFY(ss.load(kScripts + "spell-spread.js"));
    QCOMPARE(ss.name(), QString("Spell Spread"));
    QVERIFY(ss.wantsTargetDistance());
    QVERIFY(ss.wantsFloorPlace());
    QVERIFY(ss.wantsCueTime());
    // A colour effect: listed under Colour, and not for a light with no
    // colour (a dimmer, a relay).
    QCOMPARE(EffectScriptCache::categoryForTypes(ss.fixtureTypes()), QString("Color"));
    QVERIFY(!ss.fixtureTypes().contains("dimmer"));

    // A par (colour and a dimmer) on the point, a colour-only batten 1 m
    // out, a colour-wheel mover 2 m, a pixel of a bar with no dimmer 3 m,
    // a pixel of a bar with one dimmer for all its pixels 4 m.
    const QList<QVariantMap> rig = {
        { { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", 0.0 } },
        { { "hasRGB", true }, { "fromTarget", 1.0 } },
        { { "hasColorWheel", true }, { "hasDimmer", true }, { "hasPanTilt", true }, { "fromTarget", 2.0 } },
        { { "heads", 64 }, { "hasRGB", true }, { "headRGB", true }, { "fromTarget", 3.0 } },
        { { "heads", 8 }, { "hasRGB", true }, { "headRGB", true }, { "hasDimmer", true }, { "fromTarget", 4.0 } },
    };
    const QVariantMap in = { { "_fromTargetFar", 4.0 } };
    auto at = [&](double t, const QVariantMap &set, const QColor &look = QColor(200, 0, 0)) {
        return scriptFrame(ss, rig, with(in, "_time", t), set, look);
    };
    const QVariantMap hard = { { "edge", 0.0 }, { "flash", 0.0 }, { "spreadTime", 8.0 } };

    // Each reached in proportion to its distance, to a frame (20 ms); before
    // it, nothing at all is written for it: it keeps what is under it.
    const double expect[] = { 0.0, 2.0, 4.0, 6.0, 8.0 };
    QVector<double> reached(rig.size(), -1);
    for (int tick = 0; tick <= 450; ++tick)
    {
        const double t = tick * 0.02;
        const QList<QVariantMap> f = at(t, hard);
        for (int i = 0; i < rig.size(); ++i)
        {
            if (reached[i] < 0 && !f.at(i).isEmpty())
                reached[i] = t;
            if (t < expect[i] - 1e-6)
                QVERIFY2(f.at(i).isEmpty(), qPrintable(QString("head %1 written at %2").arg(i).arg(t)));
        }
    }
    for (int i = 0; i < rig.size(); ++i)
        QVERIFY2(reached[i] >= expect[i] - 1e-6 && reached[i] <= expect[i] + 0.041,
                 qPrintable(QString("head %1 reached at %2, not %3").arg(i).arg(reached[i]).arg(expect[i])));

    // Reached, each kind in the look's colour, at the look's level.
    {
        const QList<QVariantMap> f = at(9.0, hard);
        QCOMPARE(f.at(0), with(rgb(200, 0, 0), "dimmer", 1));      // its dimmer: the look's level
        QCOMPARE(f.at(1), rgb(200, 0, 0));                          // no dimmer: the colour is its level
        QCOMPARE(f.at(2), with(rgb(200, 0, 0), "dimmer", 1));      // the wheel's nearest slot
        QCOMPARE(f.at(3), rgb(200, 0, 0));                          // a pixel, by itself
        QCOMPARE(f.at(4), with(rgb(200, 0, 0), "dimmer", 1));      // the bar's one dimmer at full
        for (const QVariantMap &o : f)
            QVERIFY(!o.contains("pan") && !o.contains("tilt") && !o.contains("w"));
    }
    // It holds the colour, and never starts again: long after, the same.
    QCOMPARE(at(60.0, hard), at(9.0, hard));
    QCOMPARE(at(600.0, hard), at(9.0, hard));
    // No colour in the look: white.
    QCOMPARE(at(9.0, hard, QColor()).at(1), rgb(255, 255, 255));

    // The flash: whiter as it arrives, settling into the colour -- but a
    // colour wheel keeps its slot (a whiter colour would move the wheel).
    QVariantMap flashing = hard;
    flashing["flash"] = 1.0;
    {
        const QList<QVariantMap> f = at(2.1, flashing);
        QVERIFY(f.at(1).value("g").toInt() > 100 && f.at(1).value("b").toInt() > 100);
        QCOMPARE(f.at(1).value("r").toInt(), 200 + qRound((255 - 200) * std::pow(1 - 0.1 / 0.8, 2)));
        const QList<QVariantMap> w = at(4.1, flashing);
        QCOMPARE(w.at(2), with(rgb(200, 0, 0), "dimmer", 1));
        QCOMPARE(at(2.85, flashing).at(1), rgb(200, 0, 0));        // settled within 0.8 s
    }

    // A ragged edge: the same seed, the same front; another seed, another.
    // A light glows faintly in the colour just before it is reached, and
    // never before its ragged reach less the glow.
    const QVariantMap ragged = { { "edge", 1.0 }, { "flash", 0.0 }, { "spreadTime", 8.0 }, { "seed", 3.0 } };
    QVariantMap other = ragged;
    other["seed"] = 4.0;
    bool differs = false, glowed = false;
    for (double t = 0; t < 10; t += 0.1)
    {
        const QList<QVariantMap> f = at(t, ragged);
        QCOMPARE(f, at(t, ragged));
        differs = differs || f != at(t, other);
        const QVariantMap &b = f.at(1);
        if (!b.isEmpty() && b.value("r").toInt() > 0 && b.value("r").toInt() < 200)
            glowed = true;
    }
    QVERIFY(differs);
    QVERIFY(glowed);
    for (double t = 0; t < 8.0 * 0.75 - 0.6; t += 0.1)
        QVERIFY(at(t, ragged).at(4).isEmpty());
    for (const QVariantMap &o : at(8.05, ragged))
        QCOMPARE(o.value("r").toInt(), 200);

    // Spread over the cue's time: the host's _cueTime times it.
    {
        const QVariantMap cue = { { "edge", 0.0 }, { "flash", 0.0 }, { "spreadOver", 1 }, { "spreadTime", 30.0 } };
        QVariantMap cueIn = in;
        cueIn["_cueTime"] = 2.0;
        QVERIFY(scriptFrame(ss, rig, with(cueIn, "_time", 1.9), cue).at(4).isEmpty());
        QCOMPARE(scriptFrame(ss, rig, with(cueIn, "_time", 2.05), cue).at(4).value("r").toInt(), 200);
    }

    // Everything on the point (an unplaced rig): all at once.
    for (const QVariantMap &o : scriptFrame(ss, rig, { { "_time", 0.1 }, { "_fromTargetFar", 0.0 } }, hard))
        QVERIFY(lit(o));

    // A light that only switches (a relay) is not a spell's: left alone.
    {
        const QList<QVariantMap> relay = { { { "hasDimmer", true }, { "switches", true }, { "fromTarget", 0.0 } } };
        QVERIFY(scriptFrame(ss, relay, { { "_time", 5.0 }, { "_fromTargetFar", 1.0 } }, hard).first().isEmpty());
    }
}

void SpreadEffects_Test::spellSpreadSweepsInFromASide()
{
    EffectScript ss;
    QVERIFY(ss.load(kScripts + "spell-spread.js"));
    // Four lights on a diagonal: A upstage right, D downstage left (+X is
    // stage left, +Y downstage).
    const QList<QVariantMap> rig = {
        { { "hasRGB", true }, { "floorX", 0.0 }, { "floorY", 4.0 } },
        { { "hasRGB", true }, { "floorX", 2.0 }, { "floorY", 6.0 } },
        { { "hasRGB", true }, { "floorX", 4.0 }, { "floorY", 8.0 } },
        { { "hasRGB", true }, { "floorX", 6.0 }, { "floorY", 10.0 } },
    };
    const QVariantMap box = { { "x0", 0.0 }, { "y0", 4.0 }, { "x1", 6.0 }, { "y1", 10.0 } };
    auto reachedAt = [&](int from, const QVariantMap &theBox) {
        QVector<double> when(rig.size(), -1);
        const QVariantMap set = { { "from", from }, { "edge", 0.0 }, { "flash", 0.0 }, { "spreadTime", 6.0 } };
        for (int tick = 0; tick <= 340; ++tick)
        {
            const double t = tick * 0.02;
            const QList<QVariantMap> f = scriptFrame(ss, rig, { { "_time", t }, { "_floorBox", theBox } }, set);
            for (int i = 0; i < rig.size(); ++i)
                if (when[i] < 0 && !f.at(i).isEmpty())
                    when[i] = t;
        }
        return when;
    };
    auto within = [](const QVector<double> &got, const QVector<double> &want) {
        for (int i = 0; i < got.size(); ++i)
            if (got.at(i) < want.at(i) - 1e-6 || got.at(i) > want.at(i) + 0.041)
                return false;
        return true;
    };
    auto str = [](const QVector<double> &v) {
        QStringList s;
        for (double d : v)
            s << QString::number(d);
        return s.join(", ");
    };
    const struct { int from; QVector<double> want; const char *what; } sides[] = {
        { 1, { 0, 2, 4, 6 }, "Upstage: it runs downstage" },
        { 2, { 6, 4, 2, 0 }, "Downstage: it runs upstage" },
        { 3, { 6, 4, 2, 0 }, "Stage Left: from +X" },
        { 4, { 0, 2, 4, 6 }, "Stage Right: from -X" },
    };
    for (const auto &s : sides)
    {
        const QVector<double> got = reachedAt(s.from, box);
        QVERIFY2(within(got, s.want), qPrintable(QString("%1: %2").arg(s.what, str(got))));
    }
    // All in one line across the stage, swept from upstage: all at once.
    QVariantMap flat = box;
    flat["y1"] = 4.0;
    QCOMPARE(reachedAt(1, flat), QVector<double>(4, 0.0));
    // The host gave no extent (an older host): all at once, no error.
    const QList<QVariantMap> f = scriptFrame(ss, rig, { { "_time", 0.0 } }, { { "from", 1 }, { "edge", 0.0 }, { "flash", 0.0 } });
    for (const QVariantMap &o : f)
        QCOMPARE(o, rgb(200, 0, 0));
}

void SpreadEffects_Test::spellSpreadSweepsTheRigFromWhereItStands()
{
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QVERIFY(stepDef != nullptr);
    // Two step bars, 2.134 m wide, centred 6 m stage left: one 2 m
    // downstage of the plan's origin, one 5 m.
    Fixture *up = place(m_doc, stepDef, "ALL", "US1 #1", 0, 0, QVector3D(6000, 2000, 0));
    Fixture *down = place(m_doc, stepDef, "ALL", "DS1 #1", 1, 0, QVector3D(6000, 5000, 0));
    QVERIFY(up && down);
    QCOMPARE(up->heads(), 64);

    QLCPalette *spell = effectPalette(m_doc, "spell-spread.js",
                                      { { "from", 1 }, { "edge", 0.0 }, { "flash", 0.0 }, { "spreadTime", 10.0 } });
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(up->id());
    scene->addFixture(down->id());
    scene->addPalette(spell->id());
    scene->addPalette(colourPalette(m_doc, QColor(200, 0, 0))->id());     // its own colour
    EffectInstance inst(m_doc, scene->id(), spell->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));

    // Each pixel's own place, and the heads' extent.
    {
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        QCOMPARE(cells.size(), 128);
        const QJSValue arr = inst.buildFixturesArray(cells, cols, rows);
        QVector<double> xs;
        for (int i = 0; i < cells.size(); ++i)
        {
            const QJSValue o = arr.property(quint32(i));
            const double y = o.property("floorY").toNumber();
            QVERIFY2(qAbs(y - (cells.at(i).fixtureId == up->id() ? 2.0 : 5.0)) < 0.05,
                     qPrintable(QString("pixel %1 at y %2, x %3").arg(i).arg(y).arg(o.property("floorX").toNumber())));
            if (cells.at(i).fixtureId == up->id())
                xs << o.property("floorX").toNumber();
        }
        QCOMPARE(xs.size(), 64);
        for (int i = 1; i < xs.size(); ++i)
            QVERIFY(xs.at(i) > xs.at(i - 1));
        QVERIFY2(qAbs((xs.last() - xs.first()) - 2.1) < 0.1, qPrintable(QString("%1 .. %2").arg(xs.first()).arg(xs.last())));
        const QJSValue boxObj = inst.buildInputsObject().property("_floorBox");
        QVERIFY(qAbs(boxObj.property("y0").toNumber() - 2.0) < 0.05);
        QVERIFY(qAbs(boxObj.property("y1").toNumber() - 5.0) < 0.05);
        QCOMPARE(boxObj.property("x0").toNumber(), xs.first());
        QCOMPARE(boxObj.property("x1").toNumber(), xs.last());
    }

    // From Upstage: the upstage bar at once, in the colour; the downstage
    // one written nowhere until the spell comes down to it.
    QHash<quint64, int> f = frameAt(inst, 500);
    for (int p = 0; p < 64; ++p)
    {
        QCOMPARE(f.value(key(up->id(), quint32(p * 3)), -1), 200);
        QCOMPARE(f.value(key(up->id(), quint32(p * 3 + 1)), -1), 0);
        QVERIFY(!pixelWritten(f, down->id(), p));
    }
    f = frameAt(inst, 10500);
    for (int p = 0; p < 64; ++p)
        QCOMPARE(f.value(key(down->id(), quint32(p * 3)), -1), 200);

    // Worked out once, however many ticks...
    const quint64 once = inst.placesWorkedOut();
    QVERIFY(once >= 1);
    for (int i = 0; i < 50; ++i)
        frameAt(inst, 11000 + i * 20);
    QCOMPARE(inst.placesWorkedOut(), once);
    // ...and again when a bar moves: the downstage bar comes up level with
    // the other, and From Upstage they light together.
    m_doc->monitorProperties()->setFixturePosition(down->id(), 0, 0, QVector3D(6000, 2000, 0));
    f = frameAt(inst, 500);
    QCOMPARE(inst.placesWorkedOut(), once + 1);
    for (int p = 0; p < 64; ++p)
        QVERIFY(pixelLit(f, down->id(), p));

    // From Stage Right, across the bars: half way through, the stage-right
    // half lit and the stage-left half not written.
    spell->setEffectParamValue("from", 4);
    inst.reloadParamsFromPalette();
    f = frameAt(inst, 5000);
    int litPixels = 0;
    for (int p = 0; p < 64; ++p)
    {
        if (pixelLit(f, up->id(), p))
        {
            ++litPixels;
            QVERIFY2(p < 33, qPrintable(QString("pixel %1 lit half way").arg(p)));
        }
        else
            QVERIFY(!pixelWritten(f, up->id(), p));
    }
    QVERIFY2(litPixels >= 30 && litPixels <= 33, qPrintable(QString("%1 lit").arg(litPixels)));
}

void SpreadEffects_Test::fireSpreadOnTheShowsKinds()
{
    QLCFixtureDef *lanternDef = loadDef(m_doc, kFixtures + "WLED/WLED-Lantern.qxf");
    QLCFixtureDef *flameDef = loadDef(m_doc, kFixtures + "Generic/Generic-Silk-Flame.qxf");
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QLCFixtureDef *plDef = loadDef(m_doc, kData + "V-Show-PL4003-1Bar.qxf");
    QVERIFY(lanternDef && flameDef && stepDef && plDef);

    // The lantern on the point (SR tower), the fountain's flame 2 m off,
    // a step bar and a PL-4003 bar further out.
    Fixture *lantern = place(m_doc, lanternDef, "Effect (15ch)", "Lantern #1", 0, 0, QVector3D(1000, 2000, 0));
    Fixture *flame = place(m_doc, flameDef, "Relay", "Fountain 25 flame", 0, 30, QVector3D(3000, 2000, 0));
    Fixture *step = place(m_doc, stepDef, "ALL", "CS1 #1", 1, 0, QVector3D(6000, 2000, 0));
    Fixture *bar = place(m_doc, plDef, "1 Bar", "PL 1", 2, 0, QVector3D(6000, 4000, 0));
    QVERIFY(lantern && flame && step && bar);
    QCOMPARE(step->heads(), 64);
    QCOMPARE(bar->heads(), 40);
    QVERIFY(lantern->masterIntensityChannel() == 0);
    QVERIFY(step->masterIntensityChannel() == QLCChannel::invalid());

    QLCPalette *fire = effectPalette(m_doc, "fire-spread.js", { { "edge", 0.0 }, { "spreadTime", 10.0 } });
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (Fixture *fx : { lantern, flame, step, bar })
        scene->addFixture(fx->id());
    scene->addPalette(aimAt(m_doc, QVector3D(1.0f, 2.0f, 0))->id());
    scene->addPalette(fire->id());
    EffectInstance inst(m_doc, scene->id(), fire->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));

    // Every kind is the fire's, the relay too; it is told it only switches.
    int cols = 0, rows = 0;
    const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
    QCOMPARE(cells.size(), 1 + 1 + 64 + 40);
    {
        const QJSValue arr = inst.buildFixturesArray(cells, cols, rows);
        for (int i = 0; i < cells.size(); ++i)
            QCOMPARE(arr.property(quint32(i)).property("switches").toBool(), cells.at(i).fixtureId == flame->id());
    }
    const double farthest = inst.buildInputsObject().property("_fromTargetFar").toNumber();
    QVERIFY2(inst.buildInputsObject().property("_targetAimed").toBool(), "no Aim target found");
    QVERIFY2(farthest > 5.0 && farthest < 8.0, qPrintable(QString("farthest %1").arg(farthest)));
    auto catchMs = [&](double metres) { return qint64(metres / farthest * 10000.0); };

    // Tick by tick through the spread and on.
    QVector<qint64> stepLit(64, -1), barLit(40, -1);
    qint64 relayOn = -1;
    int relayFlips = 0, lastRelay = -1;
    bool lanternLit = false;
    for (qint64 ms = 0; ms <= 14000; ms += 50)
    {
        const QHash<quint64, int> f = frameAt(inst, ms);
        // The lantern: its master dimmer and colour; its Effect, Speed,
        // Intensity, Palette and Option channels never.
        for (quint32 c = 1; c <= 5; ++c)
            QVERIFY2(!f.contains(key(lantern->id(), c)), qPrintable(QString("lantern channel %1 written").arg(c)));
        QVERIFY(f.contains(key(lantern->id(), 0)));
        if (ms >= 200)
        {
            QVERIFY(f.value(key(lantern->id(), 0)) > 0);
            QVERIFY(f.value(key(lantern->id(), 6)) > 0);      // red: it burns
            lanternLit = true;
        }
        // The relay: off or on, nothing between, and once on it stays on.
        const int relay = f.value(key(flame->id(), 0), -1);
        QVERIFY2(relay == 0 || relay == 255, qPrintable(QString("relay at %1 at %2 ms").arg(relay).arg(ms)));
        if (relay != lastRelay && lastRelay >= 0)
            ++relayFlips;
        lastRelay = relay;
        if (relay == 255 && relayOn < 0)
            relayOn = ms;
        // The bars: each pixel dark (written 0) until it catches.
        for (int p = 0; p < 64; ++p)
        {
            if (stepLit[p] < 0 && pixelLit(f, step->id(), p))
                stepLit[p] = ms;
            if (stepLit[p] < 0)
                QVERIFY2(pixelWritten(f, step->id(), p),
                         qPrintable(QString("step pixel %1 left to what is under it at %2 ms").arg(p).arg(ms)));
        }
        for (int p = 0; p < 40; ++p)
        {
            if (barLit[p] < 0 && pixelLit(f, bar->id(), p))
                barLit[p] = ms;
            if (barLit[p] < 0)
                QVERIFY(pixelWritten(f, bar->id(), p));
        }
    }
    QVERIFY(lanternLit);
    QCOMPARE(relayFlips, 1);
    QVERIFY2(qAbs(relayOn - catchMs(2.0)) <= 100, qPrintable(QString("relay on at %1, catches at %2").arg(relayOn).arg(catchMs(2.0))));
    // Pixel by pixel, outward: the near end of the step bar first (the
    // bar stands from its place, 6 m, out to 8.134 m, as the rig view
    // draws it; the point is at 1), its far end last, every pixel caught.
    for (int p = 0; p < 64; ++p)
        QVERIFY2(stepLit[p] >= 0, qPrintable(QString("step pixel %1 never caught").arg(p)));
    for (int p = 1; p < 64; ++p)
        QVERIFY2(stepLit[p] >= stepLit[p - 1], qPrintable(QString("pixel %1 at %2, %3 at %4")
                 .arg(p).arg(stepLit[p]).arg(p - 1).arg(stepLit[p - 1])));
    QVERIFY(stepLit[63] - stepLit[0] > 2500);
    QVERIFY2(qAbs(stepLit[0] - catchMs(5.0)) <= 150,
             qPrintable(QString("first pixel at %1, catches at %2").arg(stepLit[0]).arg(catchMs(5.0))));
    for (int p = 0; p < 40; ++p)
        QVERIFY(barLit[p] >= 0);
    QVERIFY(barLit[39] > barLit[0]);
}

void SpreadEffects_Test::fireSpreadLeavesTheWashesDarkUntilItReachesThem()
{
    /* The show's washes, one from each place: an XL-450 on the centre
       truss and one on the wide truss (RGB rows and white rows, no dimmer:
       its 165ch mode), a wall washer on a step top (16 RGB pixels and 16
       white, no dimmer: 64ch). A white emitter is no dimmer: told it had
       one, Fire Spread left each pixel it hadn't reached to the look's
       colour, so the washes were lit from the start. */
    QLCFixtureDef *xlDef = loadDef(m_doc, kData + "Warmdance-XL-450.qxf");
    QLCFixtureDef *washerDef = loadDef(m_doc, kFixtures + "Oppsk/Oppsk-Wall-Washer-Light-Bar.qxf");
    QVERIFY(xlDef && washerDef);
    // Where the show stands them (mm): the towers' trusses upstage, the
    // step top downstage.
    Fixture *trussC = place(m_doc, xlDef, "165ch", "T2-CW", 0, 0, QVector3D(4755.0f, 3463.0f, 1524.0f));
    Fixture *trussW = place(m_doc, xlDef, "165ch", "T3-SW", 0, 170, QVector3D(7278.5f, 3640.1f, 1524.0f));
    Fixture *stepTop = place(m_doc, washerDef, "64ch", "DS4t2", 0, 340, QVector3D(9844.8f, 7389.7f, 0));
    QVERIFY(trussC && trussW && stepTop);
    QCOMPARE(trussC->channels(), 165u);
    QCOMPARE(stepTop->channels(), 64u);
    QCOMPARE(trussC->masterIntensityChannel(), QLCChannel::invalid());
    QCOMPARE(stepTop->masterIntensityChannel(), QLCChannel::invalid());
    const QList<Fixture *> washes = { trussC, trussW, stepTop };

    // The fire starts stage right of everything, downstage: the nearest
    // wash is metres away.
    QLCPalette *fire = effectPalette(m_doc, "fire-spread.js", { { "edge", 0.0 }, { "flare", 0.0 }, { "spreadTime", 10.0 } });
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (Fixture *f : washes)
        scene->addFixture(f->id());
    scene->addPalette(colourPalette(m_doc, QColor(0, 255, 0))->id());     // the look's colour
    scene->addPalette(aimAt(m_doc, QVector3D(-6.0f, 9.0f, 0))->id());
    scene->addPalette(fire->id());
    EffectInstance inst(m_doc, scene->id(), fire->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));

    // No wash says it has a dimmer.
    {
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        const QJSValue arr = inst.buildFixturesArray(cells, cols, rows);
        QCOMPARE(cells.size(), 75 + 75 + 32);
        for (int i = 0; i < cells.size(); ++i)
            QVERIFY2(!arr.property(quint32(i)).property("hasDimmer").toBool(),
                     qPrintable(QString("cell %1 (fixture %2 head %3) says it has a dimmer")
                                .arg(i).arg(cells.at(i).fixtureId).arg(cells.at(i).head)));
    }

    // When each wash's nearest pixel catches.
    QHash<quint32, qint64> catchesAt;
    {
        frameAt(inst, 0);
        const double farthest = inst.buildInputsObject().property("_fromTargetFar").toNumber();
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        const QJSValue arr = inst.buildFixturesArray(cells, cols, rows);
        for (int i = 0; i < cells.size(); ++i)
        {
            const qint64 ms = qint64(arr.property(quint32(i)).property("fromTarget").toNumber() / farthest * 10000.0);
            const quint32 fid = cells.at(i).fixtureId;
            catchesAt[fid] = catchesAt.contains(fid) ? qMin(catchesAt.value(fid), ms) : ms;
        }
        for (Fixture *f : washes)
            QVERIFY2(catchesAt.value(f->id()) > 2000, qPrintable(QString("%1 catches at %2 ms").arg(f->name()).arg(catchesAt.value(f->id()))));
    }

    // What the effect writes: at the start and just before the fire reaches
    // each wash, every channel of it 0 -- its RGB rows and its white rows;
    // once the fire has passed it, it burns.
    auto channelsLit = [](const QHash<quint64, int> &f, Fixture *fx) {
        int lit = 0;
        for (quint32 c = 0; c < fx->channels(); ++c)
            if (f.value(key(fx->id(), c), 0) > 0)
                ++lit;
        return lit;
    };
    QHash<quint64, int> f = frameAt(inst, 0);
    for (Fixture *fx : washes)
    {
        QCOMPARE(channelsLit(f, fx), 0);
        for (quint32 c = 0; c < fx->channels(); ++c)
            QVERIFY2(f.contains(key(fx->id(), c)),
                     qPrintable(QString("%1 channel %2 left to the look's colour at the start").arg(fx->name()).arg(c)));
    }
    for (Fixture *fx : washes)
    {
        f = frameAt(inst, catchesAt.value(fx->id()) - 100);
        QVERIFY2(channelsLit(f, fx) == 0, qPrintable(QString("%1 lit before the fire reached it").arg(fx->name())));
    }
    f = frameAt(inst, 10500);
    for (Fixture *fx : washes)
        QVERIFY2(channelsLit(f, fx) > 0, qPrintable(QString("%1 dark after the fire passed it").arg(fx->name())));

    // And on the wire, the look's green under the effect: dark at the start.
    MasterTimer *timer = m_doc->masterTimer();
    fire->setEffectParamValue("spreadTime", 120.0);         // nothing reached while we look
    scene->start(timer, FunctionParent::master());
    for (int i = 0; i < 12; ++i)
    {
        timer->timerTick();
        QTest::qWait(15);
    }
    QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const QByteArray dmx = unis.first()->preGMValues();
    m_doc->inputOutputMap()->releaseUniverses(false);
    for (Fixture *fx : washes)
        for (quint32 c = 0; c < fx->channels(); ++c)
            QVERIFY2(uchar(dmx.at(int(fx->address() + c))) == 0,
                     qPrintable(QString("%1 channel %2 at %3 before the fire").arg(fx->name()).arg(c)
                                .arg(uchar(dmx.at(int(fx->address() + c))))));
    /* Left playing (cleanup() keeps the Doc), as theCueUnderShowsUntilTheSpellArrives. */
}

void SpreadEffects_Test::spellSpreadOnTheShowsKinds()
{
    QLCFixtureDef *lanternDef = loadDef(m_doc, kFixtures + "WLED/WLED-Lantern.qxf");
    QLCFixtureDef *flameDef = loadDef(m_doc, kFixtures + "Generic/Generic-Silk-Flame.qxf");
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QLCFixtureDef *plDef = loadDef(m_doc, kData + "V-Show-PL4003-1Bar.qxf");
    QVERIFY(lanternDef && flameDef && stepDef && plDef);
    Fixture *lantern = place(m_doc, lanternDef, "Effect (15ch)", "Lantern #1", 0, 0, QVector3D(1000, 2000, 0));
    Fixture *flame = place(m_doc, flameDef, "Relay", "Fountain 25 flame", 0, 30, QVector3D(3000, 2000, 0));
    Fixture *step = place(m_doc, stepDef, "ALL", "CS1 #1", 1, 0, QVector3D(6000, 2000, 0));
    Fixture *bar = place(m_doc, plDef, "1 Bar", "PL 1", 2, 0, QVector3D(6000, 4000, 0));
    QVERIFY(lantern && flame && step && bar);

    QLCPalette *spell = effectPalette(m_doc, "spell-spread.js",
                                      { { "edge", 0.0 }, { "flash", 0.0 }, { "spreadTime", 10.0 } });
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (Fixture *fx : { lantern, flame, step, bar })
        scene->addFixture(fx->id());
    scene->addPalette(aimAt(m_doc, QVector3D(1.0f, 2.0f, 0))->id());
    scene->addPalette(spell->id());
    scene->addPalette(colourPalette(m_doc, QColor(200, 0, 0))->id());     // the spell's own red
    EffectInstance inst(m_doc, scene->id(), spell->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));

    // The relay is not the spell's: no colour.
    int cols = 0, rows = 0;
    for (const EffectInstance::EffectCell &c : inst.effectiveCells(cols, rows))
        QVERIFY(c.fixtureId != flame->id());

    QVector<qint64> stepLit(64, -1);
    for (qint64 ms = 0; ms <= 12000; ms += 50)
    {
        const QHash<quint64, int> f = frameAt(inst, ms);
        QVERIFY(!f.contains(key(flame->id(), 0)));
        for (quint32 c = 1; c <= 5; ++c)
            QVERIFY(!f.contains(key(lantern->id(), c)));
        // The lantern on the point: the colour on its colour 1, at the
        // look's level (no Dimmer look: full).
        QCOMPARE(f.value(key(lantern->id(), 0), -1), 255);
        QCOMPARE(f.value(key(lantern->id(), 6), -1), 200);
        QCOMPARE(f.value(key(lantern->id(), 7), -1), 0);
        QCOMPARE(f.value(key(lantern->id(), 8), -1), 0);
        // A pixel not yet reached: nothing written; reached, the colour.
        for (int p = 0; p < 64; ++p)
        {
            if (stepLit[p] < 0 && pixelWritten(f, step->id(), p))
                stepLit[p] = ms;
            if (stepLit[p] < 0)
                continue;
            QCOMPARE(f.value(key(step->id(), quint32(p * 3)), -1), 200);
            QCOMPARE(f.value(key(step->id(), quint32(p * 3 + 1)), -1), 0);
            QCOMPARE(f.value(key(step->id(), quint32(p * 3 + 2)), -1), 0);
        }
        if (ms == 2000)
            for (int p = 0; p < 40; ++p)
                QVERIFY(!pixelWritten(f, bar->id(), p));
    }
    for (int p = 1; p < 64; ++p)
        QVERIFY(stepLit[p] >= stepLit[p - 1] && stepLit[p - 1] >= 0);
    QVERIFY(stepLit[63] - stepLit[0] > 2500);
}

void SpreadEffects_Test::theCueUnderShowsUntilTheSpellArrives()
{
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QVERIFY(stepDef != nullptr);
    // Two step bars on universe 1: the spell starts at the stage-right
    // end of the first (where the bar's place is); the second is 10 m away.
    Fixture *nearBar = place(m_doc, stepDef, "ALL", "CS1 #1", 0, 0, QVector3D(6000, 2000, 0));
    Fixture *farBar = place(m_doc, stepDef, "ALL", "CS1 #2", 0, 192, QVector3D(6000, 12000, 0));
    QVERIFY(nearBar && farBar);

    // The cue under them: blue.
    Scene *under = new Scene(m_doc);
    m_doc->addFunction(under);
    under->addFixture(nearBar->id());
    under->addFixture(farBar->id());
    under->addPalette(colourPalette(m_doc, QColor(0, 0, 255))->id());

    // The spell over them: its own red, from the middle of the near bar,
    // a minute to reach the far one.
    QLCPalette *spell = effectPalette(m_doc, "spell-spread.js",
                                      { { "edge", 0.0 }, { "flash", 0.0 }, { "spreadTime", 60.0 } });
    Scene *over = new Scene(m_doc);
    m_doc->addFunction(over);
    over->addFixture(nearBar->id());
    over->addFixture(farBar->id());
    over->addPalette(aimAt(m_doc, QVector3D(6.0f, 2.0f, 0))->id());
    over->addPalette(spell->id());
    over->addPalette(colourPalette(m_doc, QColor(255, 0, 0))->id());

    MasterTimer *timer = m_doc->masterTimer();
    under->start(timer, FunctionParent::master());
    over->start(timer, FunctionParent::master());
    QByteArray dmx;
    auto read = [&]() {
        for (int i = 0; i < 12; ++i)
        {
            timer->timerTick();
            QTest::qWait(15);               // the effect runner ticks on its own timer
        }
        QList<Universe *> unis = m_doc->inputOutputMap()->claimUniverses();
        for (Universe *u : unis)
            u->processFaders();
        dmx = unis.first()->preGMValues().left(384);
        m_doc->inputOutputMap()->releaseUniverses(false);
    };
    auto px = [&](int base, int pixel) {
        return QColor(uchar(dmx.at(base + pixel * 3)), uchar(dmx.at(base + pixel * 3 + 1)),
                      uchar(dmx.at(base + pixel * 3 + 2)));
    };
    QElapsedTimer waited;
    waited.start();
    read();
    // Give the runner up to two seconds to draw the end pixels.
    while (px(0, 1) != QColor(255, 0, 0) && waited.elapsed() < 2000)
        read();
    {
        // Where the spell stands for these bars, as an instance of it sees them.
        EffectInstance probe(m_doc, over->id(), spell->id());
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = probe.effectiveCells(cols, rows);
        const QJSValue arr = probe.buildFixturesArray(cells, cols, rows);
        const QJSValue in = probe.buildInputsObject();
        QStringList dists;
        for (int i : { 0, 1, 2, 63, 64 })
            if (i < cells.size())
                dists << QString("%1: %2").arg(i).arg(arr.property(quint32(i)).property("fromTarget").toNumber());
        QVERIFY2(px(0, 0) == QColor(255, 0, 0),
                 qPrintable(QString("pixel 0 %1 after %2 ms; aimed %3, farthest %4; %5")
                            .arg(px(0, 0).name()).arg(waited.elapsed())
                            .arg(in.property("_targetAimed").toBool()).arg(in.property("_fromTargetFar").toNumber())
                            .arg(dists.join(", "))));
    }
    QCOMPARE(px(0, 1), QColor(255, 0, 0));
    // The rest of the near bar, and all of the far one: the cue under them.
    QCOMPARE(px(0, 32), QColor(0, 0, 255));
    QCOMPARE(px(0, 63), QColor(0, 0, 255));
    for (int p = 0; p < 64; ++p)
        QCOMPARE(px(192, p), QColor(0, 0, 255));

    /* Left playing (cleanup() keeps the Doc): a scene stopped from this
       thread while its effect plays runs into the runner claiming the
       universes again (hotpath_test's note, fix/043-delete-running-deadlock). */
}

void SpreadEffects_Test::theStepBarsTickCost()
{
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QVERIFY(stepDef != nullptr);
    // 96 step bars as the show stands them: three sections of four steps,
    // eight strips a step, two bars a universe.
    QList<Fixture *> bars;
    static const char *sections[] = { "US", "CS", "DS" };
    for (int s = 0; s < 3; ++s)
        for (int stepNo = 1; stepNo <= 4; ++stepNo)
            for (int strip = 1; strip <= 8; ++strip)
            {
                const int n = bars.size();
                Fixture *f = place(m_doc, stepDef, "ALL", QString("%1%2 #%3").arg(sections[s]).arg(stepNo).arg(strip),
                                   quint32(n / 2), quint32((n % 2) * 192),
                                   QVector3D(2200.0f * (strip - 1) - 7700.0f, 3000.0f * s + 400.0f * stepNo, 0));
                QVERIFY(f != nullptr);
                bars << f;
            }

    struct Cost { QString what; double tickMs = 0, scriptMs = 0; };
    QList<Cost> costs;
#ifdef QT_NO_DEBUG
    const int ticks = qMax(20, qEnvironmentVariableIntValue("QLC_RIG_TICKS"));
#else
    const int ticks = qMax(3, qEnvironmentVariableIntValue("QLC_RIG_TICKS"));
#endif
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        QFile f(dir.filePath("nothing.js"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        // It names palettes.look, so the host hands it the look's colour
        // rather than painting it over what it returns: nothing at all.
        f.write("(function() { var e = {}; e.apiVersion = 1; e.name = 'T Nothing'; /* palettes.look */"
                " e.fixtureTypes = ['rgb'];"
                " e.tick = function(fx) { return fx.map(function() { return {}; }); };"
                " return e; })()");
    }
    auto measure = [&](const QString &what, const QString &path, const QVariantMap &set, qint64 fromMs,
                       bool ownColour) {
        QLCPalette *p = new QLCPalette(QLCPalette::Effect, m_doc);
        p->setScriptPath(path);
        for (auto it = set.constBegin(); it != set.constEnd(); ++it)
            p->setEffectParamValue(it.key(), it.value().toDouble());
        m_doc->addPalette(p);
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        for (Fixture *f : bars)
            scene->addFixture(f->id());
        scene->addPalette(aimAt(m_doc, QVector3D(0.0f, 1.0f, 0))->id());
        scene->addPalette(p->id());
        if (ownColour)
            scene->addPalette(colourPalette(m_doc, QColor(200, 0, 0))->id());
        EffectInstance inst(m_doc, scene->id(), p->id());
        if (!inst.isValid())
            return false;
        frameAt(inst, fromMs);                       // the first: cells and places
        const quint64 places = inst.placesWorkedOut();
        Cost c;
        c.what = what;
        QElapsedTimer clock;
        clock.start();
        for (int i = 1; i <= ticks; ++i)
        {
            inst.setOffStageClock(fromMs + i * 20);
            inst.runTick();
        }
        c.tickMs = double(clock.nsecsElapsed()) / 1e6 / ticks;
        // Its places stay worked out at this size.
        if (path.endsWith("spread.js") && inst.placesWorkedOut() != places)
            return false;
        // The script alone, on the arguments the host made.
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        if (cells.size() != 96 * 64)
            return false;
        const QJSValue fixtures = inst.buildFixturesArray(cells, cols, rows);
        QJSValue inputs = inst.buildInputsObject();
        const QJSValue palettes = inst.buildPalettesObject();
        const QJSValue params = inst.buildParamsObject();
        QJSValue state = inst.m_script.newState();
        const QJSValue data = inst.m_script.engine()->newObject();
        // Its first tick works out what it keeps; the rest are timed.
        inputs.setProperty("_time", double(fromMs) / 1000.0);
        inst.m_script.callTick(fixtures, inputs, palettes, params, state, data);
        clock.restart();
        for (int i = 1; i <= ticks; ++i)
        {
            inputs.setProperty("_time", double(fromMs + i * 20) / 1000.0);
            inst.m_script.callTick(fixtures, inputs, palettes, params, state, data);
        }
        c.scriptMs = double(clock.nsecsElapsed()) / 1e6 / ticks;
        costs << c;
        return true;
    };
    const QVariantMap fireSet = { { "edge", 0.3 }, { "spreadTime", 8.0 } };
    const QVariantMap spellSet = { { "edge", 0.2 }, { "spreadTime", 8.0 } };
    QVERIFY(measure("nothing (the host alone)", dir.filePath("nothing.js"), {}, 4000, false));
    QVERIFY(measure("Fire Spread, spreading", kScripts + "fire-spread.js", fireSet, 4000, false));
    QVERIFY(measure("Fire Spread, all burning", kScripts + "fire-spread.js", fireSet, 20000, false));
    QVERIFY(measure("Spell Spread, spreading", kScripts + "spell-spread.js", spellSet, 4000, true));
    QVERIFY(measure("Spell Spread, all reached", kScripts + "spell-spread.js", spellSet, 20000, true));

#ifdef QT_NO_DEBUG
    const QString build = QStringLiteral("release");
#else
    const QString build = QStringLiteral("debug");
#endif
    QStringList lines;
    for (const Cost &c : costs)
        lines << QString("%1 build, 96 step bars x 64 (6144 heads), %2: %3 ms a tick (script %4 ms)")
                     .arg(build, c.what).arg(c.tickMs, 0, 'f', 2).arg(c.scriptMs, 0, 'f', 2);
    for (const QString &l : lines)
        qInfo().noquote() << l;
    // The numbers, brought back from the pool (check-all.sh --shots).
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QFile out(QDir(shots).filePath(QString("spread-tick-cost-%1.txt").arg(build)));
        if (out.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&out) << lines.join("\n") << "\n";
    }
}

QTEST_MAIN(SpreadEffects_Test)
