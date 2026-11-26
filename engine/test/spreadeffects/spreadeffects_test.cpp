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

#include <memory>
#include <QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <ctime>
#include <functional>

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
#include "chaser.h"
#include "chaserstep.h"
#include "chaseraction.h"
#include "effectscriptrunner.h"
#include "fixturerest.h"
#include "outputrulekeeper.h"
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

/* The show's 96 step bars (6,144 pixel heads) as it stands them: three
   sections of four steps, eight strips a step, two bars a universe. */
QList<Fixture *> placeStepBars(Doc *doc, QLCFixtureDef *stepDef)
{
    QList<Fixture *> bars;
    static const char *sections[] = { "US", "CS", "DS" };
    for (int s = 0; s < 3; ++s)
        for (int stepNo = 1; stepNo <= 4; ++stepNo)
            for (int strip = 1; strip <= 8; ++strip)
            {
                const int n = bars.size();
                Fixture *f = place(doc, stepDef, "ALL",
                                   QString("%1%2 #%3").arg(sections[s]).arg(stepNo).arg(strip),
                                   quint32(n / 2), quint32((n % 2) * 192),
                                   QVector3D(2200.0f * (strip - 1) - 7700.0f,
                                             3000.0f * s + 400.0f * stepNo, 0));
                if (f == nullptr)
                    return QList<Fixture *>();
                bars << f;
            }
    return bars;
}

/* This thread's CPU time, in ns: what a tick costs itself, whatever else
   the machine is doing (on a shared gate host wall-clock time swells under
   another leg's load). Where there is no per-thread clock: wall-clock
   time, as before. Same rule as EffectInstance_Test's budget. */
qint64 threadCpuNs(const QElapsedTimer &wall)
{
#if defined(Q_OS_UNIX) && defined(CLOCK_THREAD_CPUTIME_ID)
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0)
        return qint64(ts.tv_sec) * 1000000000LL + qint64(ts.tv_nsec);
#endif
    return wall.nsecsElapsed();
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
   descriptor's fields) at @p inputs, the look's colours @p looks in
   order, @p set over the defaults. One map an intent. */
QList<QVariantMap> scriptFrameColours(EffectScript &script, const QList<QVariantMap> &heads,
                                      const QVariantMap &inputs, const QVariantMap &set,
                                      const QList<QColor> &looks)
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
    QJSValue colours = eng->newArray(uint(looks.size()));
    for (int i = 0; i < looks.size(); ++i)
    {
        QJSValue c = eng->newObject();
        c.setProperty("r", looks.at(i).red());
        c.setProperty("g", looks.at(i).green());
        c.setProperty("b", looks.at(i).blue());
        colours.setProperty(quint32(i), c);
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

/* The same with the look's one colour @p look (none when invalid). */
QList<QVariantMap> scriptFrame(EffectScript &script, const QList<QVariantMap> &heads,
                               const QVariantMap &inputs, const QVariantMap &set,
                               const QColor &look = QColor(200, 0, 0))
{
    return scriptFrameColours(script, heads, inputs, set,
                              look.isValid() ? QList<QColor>{ look } : QList<QColor>());
}

/* @p script ticked again and again on one state, as the host does: @p
   heads (each a map of the descriptor's fields), @p inputs (with _time set
   each tick), @p set over the defaults, the look's colours @p looks. */
class ScriptRun
{
public:
    ScriptRun(EffectScript &script, const QList<QVariantMap> &heads, const QVariantMap &inputs,
              const QVariantMap &set, const QList<QColor> &looks = QList<QColor>())
        : m_script(script), m_n(heads.size())
    {
        QJSEngine *eng = script.engine();
        m_fixtures = eng->newArray(uint(heads.size()));
        for (int i = 0; i < heads.size(); ++i)
        {
            QJSValue o = eng->newObject();
            o.setProperty("heads", 1);
            for (auto it = heads.at(i).constBegin(); it != heads.at(i).constEnd(); ++it)
                o.setProperty(it.key(), eng->toScriptValue(it.value()));
            m_fixtures.setProperty(quint32(i), o);
        }
        m_inputs = eng->newObject();
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it)
            m_inputs.setProperty(it.key(), eng->toScriptValue(it.value()));
        m_params = eng->newObject();
        for (const EffectScript::ParamDef &d : script.paramDefs())
            m_params.setProperty(d.name, double(d.defaultValue));
        for (auto it = set.constBegin(); it != set.constEnd(); ++it)
            m_params.setProperty(it.key(), it.value().toDouble());
        m_palettes = eng->newObject();
        QJSValue lookObj = eng->newObject();
        QJSValue colours = eng->newArray(uint(looks.size()));
        for (int i = 0; i < looks.size(); ++i)
        {
            QJSValue c = eng->newObject();
            c.setProperty("r", looks.at(i).red());
            c.setProperty("g", looks.at(i).green());
            c.setProperty("b", looks.at(i).blue());
            colours.setProperty(quint32(i), c);
        }
        lookObj.setProperty("colors", colours);
        lookObj.setProperty("dimmer", 1.0);
        m_palettes.setProperty("look", lookObj);
        m_state = script.newState();
    }

    QList<QVariantMap> at(double seconds)
    {
        m_inputs.setProperty("_time", seconds);
        const QJSValue out = m_script.callTick(m_fixtures, m_inputs, m_palettes, m_params, m_state,
                                               m_script.engine()->newObject());
        QList<QVariantMap> frame;
        for (int i = 0; i < m_n; ++i)
            frame << out.property(quint32(i)).toVariant().toMap();
        return frame;
    }

    bool done() const { return m_state.property("done").toBool(); }

private:
    EffectScript &m_script;
    int m_n;
    QJSValue m_fixtures, m_inputs, m_params, m_palettes, m_state;
};

/* A head's level in a script's answer: its dimmer, else its brightest
   colour; -1 when nothing is said (left to what is under it). */
double levelOf(const QVariantMap &o)
{
    if (o.isEmpty())
        return -1.0;
    if (o.contains("dimmer"))
        return o.value("dimmer").toDouble();
    return qMax(o.value("r").toInt(), qMax(o.value("g").toInt(), o.value("b").toInt())) / 255.0;
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
    QVector<qint64> stepLit(64, -1), barLit(40, -1), stepWritten(64, -1), barWritten(40, -1);
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
        // The relay: left to what is under it (not written) until it
        // catches, then on, nothing between, and once on it stays on.
        const int relay = f.value(key(flame->id(), 0), -1);
        QVERIFY2(relay == -1 || relay == 255, qPrintable(QString("relay at %1 at %2 ms").arg(relay).arg(ms)));
        if (relay != lastRelay)        // from not written (-1) to on: one flip
            ++relayFlips;
        lastRelay = relay;
        if (relay == 255 && relayOn < 0)
            relayOn = ms;
        // The bars: each pixel left to what is under it (not written)
        // until it catches (Q259).
        for (int p = 0; p < 64; ++p)
        {
            if (stepLit[p] < 0 && pixelLit(f, step->id(), p))
                stepLit[p] = ms;
            if (stepWritten[p] < 0 && pixelWritten(f, step->id(), p))
                stepWritten[p] = ms;
        }
        for (int p = 0; p < 40; ++p)
        {
            if (barLit[p] < 0 && pixelLit(f, bar->id(), p))
                barLit[p] = ms;
            if (barWritten[p] < 0 && pixelWritten(f, bar->id(), p))
                barWritten[p] = ms;
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
    // Not written before the fire reaches it (its catch tick may write it
    // at none as the flame snaps up): left to what is under it.
    for (int p = 0; p < 64; ++p)
        QVERIFY2(stepWritten[p] >= stepLit[p] - 50,
                 qPrintable(QString("step pixel %1 written at %2 ms, lit at %3").arg(p).arg(stepWritten[p]).arg(stepLit[p])));
    for (int p = 0; p < 40; ++p)
        QVERIFY(barWritten[p] >= barLit[p] - 50);
    QVERIFY(barLit[39] > barLit[0]);
}

void SpreadEffects_Test::fireSpreadLeavesTheWashesDarkUntilItReachesThem()
{
    /* The show's washes, one from each place: an XL-450 on the centre
       truss and one on the wide truss (RGB rows and white rows, no dimmer:
       its 165ch mode), a wall washer on a step top (16 RGB pixels and 16
       white, no dimmer: 64ch). A white emitter is no dimmer: told it had
       one, Fire Spread lit them by the look's colour from the start. Over
       black (nothing under them, no colour of the look's own: the show's
       way, Q259) they are dark until it reaches them. */
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

    // What the effect writes: at the start nothing (left to what is under
    // it, Q259), just before the fire reaches each wash nothing lit -- its
    // RGB rows and its white rows; once the fire has passed it, it burns.
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
            QVERIFY2(!f.contains(key(fx->id(), c)),
                     qPrintable(QString("%1 channel %2 written before the fire").arg(fx->name()).arg(c)));
    }
    for (Fixture *fx : washes)
    {
        f = frameAt(inst, catchesAt.value(fx->id()) - 100);
        QVERIFY2(channelsLit(f, fx) == 0, qPrintable(QString("%1 lit before the fire reached it").arg(fx->name())));
    }
    f = frameAt(inst, 10500);
    for (Fixture *fx : washes)
        QVERIFY2(channelsLit(f, fx) > 0, qPrintable(QString("%1 dark after the fire passed it").arg(fx->name())));

    // And on the wire, over black: dark at the start.
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

namespace
{
/* The effect runner hears of starts and stops queued, as in the app (there
   the MasterTimer is its own thread): a direct call here would come while
   the tick holds the universes the runner claims (FollowSpotFeel_Test). */
void queueTheRunner(Doc *doc)
{
    EffectScriptRunner *runner = doc->effectScriptRunner();
    MasterTimer *mt = doc->masterTimer();
    QObject::disconnect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)));
    QObject::disconnect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)));
    QObject::disconnect(mt, SIGNAL(functionStartedAgain(quint32,quint32)),
                        runner, SLOT(slotFunctionStartedAgain(quint32,quint32)));
    QObject::connect(mt, SIGNAL(functionStarted(quint32)), runner, SLOT(slotFunctionStarted(quint32)),
                     Qt::QueuedConnection);
    QObject::connect(mt, SIGNAL(functionStopped(quint32)), runner, SLOT(slotFunctionStopped(quint32)),
                     Qt::QueuedConnection);
    QObject::connect(mt, SIGNAL(functionStartedAgain(quint32,quint32)),
                     runner, SLOT(slotFunctionStartedAgain(quint32,quint32)), Qt::QueuedConnection);
}

/* Universe 1 on the wire after @p ticks of the MasterTimer (the runner
   ticks on its own timer meanwhile): its first @p n channels. */
QByteArray wireAfter(Doc *doc, int ticks, int n = 16)
{
    MasterTimer *timer = doc->masterTimer();
    for (int i = 0; i < ticks; ++i)
    {
        timer->timerTick();
        QTest::qWait(15);
    }
    QList<Universe *> unis = doc->inputOutputMap()->claimUniverses();
    for (Universe *u : unis)
        u->processFaders();
    const QByteArray dmx = unis.first()->preGMValues().left(n);
    doc->inputOutputMap()->releaseUniverses(false);
    return dmx;
}

QString bytes(const QByteArray &b, int from, int n)
{
    QStringList out;
    for (int i = from; i < from + n; ++i)
        out << QString::number(uchar(b.at(i)));
    return out.join(' ');
}
}

void SpreadEffects_Test::fireSpreadCastsOverWhatIsThere()
{
    queueTheRunner(m_doc);
    QLCFixtureDef *rgbDef = loadDef(m_doc, kFixtures + "Generic/Generic-Generic-RGB.qxf");
    QVERIFY(rgbDef != nullptr);
    // Two RGB washes (no dimmer) 8 m apart: the fire starts on the first.
    Fixture *nearWash = place(m_doc, rgbDef, "RGB", "Near", 0, 0, QVector3D(1000, 2000, 0));
    Fixture *farWash = place(m_doc, rgbDef, "RGB", "Far", 0, 3, QVector3D(9000, 2000, 0));
    QVERIFY(nearWash && farWash);

    // The cue under them: green.
    Scene *under = new Scene(m_doc);
    m_doc->addFunction(under);
    under->addFixture(nearWash->id());
    under->addFixture(farWash->id());
    under->addPalette(colourPalette(m_doc, QColor(0, 255, 0))->id());

    // The fire over them, two minutes to reach the far one.
    QLCPalette *fire = effectPalette(m_doc, "fire-spread.js",
                                     { { "edge", 0.0 }, { "flare", 0.0 }, { "spreadTime", 120.0 } });
    Scene *over = new Scene(m_doc);
    m_doc->addFunction(over);
    over->addFixture(nearWash->id());
    over->addFixture(farWash->id());
    over->addPalette(aimAt(m_doc, QVector3D(1.0f, 2.0f, 0))->id());
    over->addPalette(fire->id());

    MasterTimer *timer = m_doc->masterTimer();
    under->start(timer, FunctionParent::master());
    const QByteArray green = wireAfter(m_doc, 12);
    QCOMPARE(bytes(green, 0, 6), QString("0 255 0 0 255 0"));

    over->start(timer, FunctionParent::master());
    QByteArray dmx = wireAfter(m_doc, 12);
    QElapsedTimer waited;
    waited.start();
    while (uchar(dmx.at(0)) == 0 && waited.elapsed() < 2000)
        dmx = wireAfter(m_doc, 4);
    // The near wash burns: the fire's red over the green.
    QVERIFY2(uchar(dmx.at(0)) > 0, qPrintable(QString("near wash %1").arg(bytes(dmx, 0, 3))));
    QVERIFY2(bytes(dmx, 0, 3) != QString("0 255 0"), qPrintable(bytes(dmx, 0, 3)));
    // The far wash, not reached: the cue's green, as it was.
    QVERIFY2(bytes(dmx, 3, 3) == QString("0 255 0"),
             qPrintable(QString("far wash %1 before the fire reached it; the cue under it %2")
                        .arg(bytes(dmx, 3, 3), bytes(green, 3, 3))));

    // Let go (the runner told queued) so the Doc can go.
    timer->m_stopAllFunctions = true;
    timer->timerTick();
    timer->timerTick();
    timer->m_stopAllFunctions = false;
    QCoreApplication::processEvents();
}

void SpreadEffects_Test::fireSpreadHoldsAsItEndsUntilLetGo()
{
    queueTheRunner(m_doc);
    QLCFixtureDef *rgbDef = loadDef(m_doc, kFixtures + "Generic/Generic-Generic-RGB.qxf");
    QVERIFY(rgbDef != nullptr);
    Fixture *nearWash = place(m_doc, rgbDef, "RGB", "Near", 0, 0, QVector3D(1000, 2000, 0));
    Fixture *farWash = place(m_doc, rgbDef, "RGB", "Far", 0, 3, QVector3D(9000, 2000, 0));
    QVERIFY(nearWash && farWash);
    QLCPalette *aim = aimAt(m_doc, QVector3D(1.0f, 2.0f, 0));

    auto lit = [](const QHash<quint64, int> &f, Fixture *fx) {
        return f.value(key(fx->id(), 0), 0) + f.value(key(fx->id(), 1), 0) + f.value(key(fx->id(), 2), 0) > 0;
    };

    /* By the effect's own clock: Once, Hold, Length auto; the fire takes
       20 s to reach the far wash. */
    {
        QLCPalette *fire = effectPalette(m_doc, "fire-spread.js", { { "edge", 0.0 }, { "spreadTime", 20.0 } });
        fire->setEffectParamValue(EffectRuns::runsKey(), 1);         // Once
        fire->setEffectParamValue(EffectRuns::atEndKey(), 0);        // Hold
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        scene->addFixture(nearWash->id());
        scene->addFixture(farWash->id());
        scene->addPalette(aim->id());
        scene->addPalette(fire->id());
        EffectInstance inst(m_doc, scene->id(), fire->id());
        QVERIFY2(inst.isValid(), qPrintable(inst.error()));

        QHash<quint64, int> f = frameAt(inst, 10000);
        QVERIFY(lit(f, nearWash));
        QVERIFY(!lit(f, farWash));
        // After the fire has reached everything: both burn, and that frame
        // stays, frame for frame.
        const QHash<quint64, int> end = frameAt(inst, 25000);
        QVERIFY2(lit(end, farWash), "the far wash is dark once the run of Once is over");
        QVERIFY(lit(end, nearWash));
        for (qint64 ms : { 30000, 60000, 300000 })
            QVERIFY2(frameAt(inst, ms) == end, qPrintable(QString("not held at %1 ms").arg(ms)));
        QVERIFY(inst.runDone());
    }

    /* Once to Release still ends on time (the cue's, here the effect's own
       8 s), not when the fire has spread: let go then, not before. */
    {
        QLCPalette *fire = effectPalette(m_doc, "fire-spread.js", { { "edge", 0.0 }, { "spreadTime", 2.0 } });
        fire->setEffectParamValue(EffectRuns::runsKey(), 1);         // Once
        fire->setEffectParamValue(EffectRuns::atEndKey(), 1);        // Release
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        scene->addFixture(nearWash->id());
        scene->addFixture(farWash->id());
        scene->addPalette(aim->id());
        scene->addPalette(fire->id());
        EffectInstance inst(m_doc, scene->id(), fire->id());
        QVERIFY(inst.isValid());
        QHash<quint64, int> f = frameAt(inst, 5000);
        QVERIFY2(lit(f, nearWash) && lit(f, farWash), "Once to Release let go when the fire had spread");
        f = frameAt(inst, 8500);
        QVERIFY2(f.isEmpty(), "Once to Release did not let go at its time");
        QVERIFY(inst.runDone());
    }

    /* On the wire: a fire of a third of a second, no flare, run Once and
       held; then let go each way. Over black: 0 after. */
    QLCPalette *fire = effectPalette(m_doc, "fire-spread.js",
                                     { { "edge", 0.0 }, { "flare", 0.0 }, { "spreadTime", 0.3 } });
    fire->setEffectParamValue(EffectRuns::runsKey(), 1);
    fire->setEffectParamValue(EffectRuns::atEndKey(), 0);
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    scene->addFixture(nearWash->id());
    scene->addFixture(farWash->id());
    scene->addPalette(aim->id());
    scene->addPalette(fire->id());
    Scene *nextCue = new Scene(m_doc);               // a cue without the effect
    m_doc->addFunction(nextCue);
    Chaser *list = new Chaser(m_doc);
    m_doc->addFunction(list);
    list->setDurationMode(Chaser::Common);
    list->setDuration(Function::infiniteSpeed());   // each cue holds until GO
    list->setFadeInMode(Chaser::Common);
    list->setFadeInSpeed(0);
    list->setFadeOutMode(Chaser::Common);
    list->setFadeOutSpeed(0);
    list->addStep(ChaserStep(scene->id()));
    list->addStep(ChaserStep(nextCue->id()));
    MasterTimer *timer = m_doc->masterTimer();
    const QByteArray dark(6, '\0');

    enum LetGo { Released, NextCue, StopAll, EmergencyStop };
    const char *names[] = { "the cue released", "GO to the next cue", "Stop all", "the emergency stop" };
    for (LetGo how : { Released, NextCue, StopAll, EmergencyStop })
    {
        if (how == NextCue)
            list->start(timer, FunctionParent::master());
        else
            scene->start(timer, FunctionParent::master());
        // Past the spread's end: held, both lit, the same frame on.
        QByteArray held = wireAfter(m_doc, 12);
        QElapsedTimer waited;
        waited.start();
        while ((uchar(held.at(3)) == 0 && uchar(held.at(4)) == 0) && waited.elapsed() < 3000)
            held = wireAfter(m_doc, 4);
        QTest::qWait(400);
        held = wireAfter(m_doc, 4);
        QVERIFY2(held.left(6) != dark, qPrintable(QString("%1: dark %2").arg(names[how], bytes(held, 0, 6))));
        QVERIFY2(uchar(held.at(0)) + uchar(held.at(1)) > 0 && uchar(held.at(3)) + uchar(held.at(4)) > 0,
                 qPrintable(QString("%1: not every wash burns %2").arg(names[how], bytes(held, 0, 6))));
        for (int i = 0; i < 3; ++i)
        {
            QTest::qWait(150);
            const QByteArray again = wireAfter(m_doc, 4);
            QVERIFY2(again.left(6) == held.left(6),
                     qPrintable(QString("%1: not held, %2 then %3").arg(names[how], bytes(held, 0, 6), bytes(again, 0, 6))));
        }

        switch (how)
        {
            case Released:
                scene->stop(FunctionParent::master());
            break;
            case NextCue:
            {
                ChaserAction go;
                go.m_action = ChaserNextStep;
                go.m_stepIndex = -1;
                go.m_masterIntensity = 1.0;
                go.m_stepIntensity = 1.0;
                go.m_fadeMode = Chaser::FromFunction;
                list->setAction(go);
            }
            break;
            case StopAll:
                timer->m_stopAllFunctions = true;
                timer->timerTick();
                timer->timerTick();
                timer->m_stopAllFunctions = false;
                FixtureRest::restAfterStop(m_doc);
            break;
            case EmergencyStop:
                m_doc->outputRuleKeeper()->emergencyStop();
            break;
        }
        const QByteArray after = wireAfter(m_doc, 12);
        QVERIFY2(after.left(6) == dark,
                 qPrintable(QString("%1: still %2 after it").arg(names[how], bytes(after, 0, 6))));
        if (how == NextCue)
        {
            list->stop(FunctionParent::master());
            wireAfter(m_doc, 4);
        }
        QVERIFY2(!scene->isRunning(), names[how]);
    }
}

void SpreadEffects_Test::fireSpreadInFireIsAsItWas()
{
    EffectScript now;
    QVERIFY(now.load(kScripts + "fire-spread.js"));
    // The choice is there, and Fire is the default: a show saved before it
    // plays Fire.
    bool hasColours = false;
    for (const EffectScript::ParamDef &d : now.paramDefs())
        if (d.name == QLatin1String("colours"))
        {
            hasColours = true;
            QCOMPARE(d.defaultValue, 0.0f);
            QCOMPARE(d.enumValues, QStringList({ "Fire", "Front and Behind" }));
        }
    QVERIFY2(hasColours, "Fire Spread has no Colours setting");

    // The script as it was before Colours (data/fire-spread-0412.js),
    // beside today's libraries.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir().mkpath(dir.filePath("lib")));
    QVERIFY(QFile::copy(kScripts + "lib/fire.js", dir.filePath("lib/fire.js")));
    QVERIFY(QFile::copy(kScripts + "lib/spread.js", dir.filePath("lib/spread.js")));
    QVERIFY(QFile::copy(kData + "fire-spread-0412.js", dir.filePath("fire-spread.js")));
    EffectScript before;
    QVERIFY(before.load(dir.filePath("fire-spread.js")));

    // Every kind: a par, a dimmer, a colour-only par, a colour-wheel mover,
    // a pixel of a bar with no dimmer, one of a bar with one dimmer, a
    // dimmer pixel, a relay.
    const QList<QVariantMap> rig = {
        { { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", 0.0 } },
        { { "hasDimmer", true }, { "fromTarget", 1.0 } },
        { { "hasRGB", true }, { "fromTarget", 2.0 } },
        { { "hasColorWheel", true }, { "hasDimmer", true }, { "hasPanTilt", true }, { "fromTarget", 3.0 } },
        { { "heads", 8 }, { "hasRGB", true }, { "headRGB", true }, { "fromTarget", 4.0 } },
        { { "heads", 8 }, { "hasRGB", true }, { "headRGB", true }, { "hasDimmer", true }, { "fromTarget", 2.5 } },
        { { "heads", 8 }, { "headRGB", false }, { "fromTarget", 3.5 } },
        { { "hasDimmer", true }, { "switches", true }, { "fromTarget", 1.5 } },
    };
    const QVariantMap in = { { "_fromTargetFar", 4.0 }, { "_cueTime", 3.0 } };
    const QList<QColor> two = { QColor(0, 0, 255), QColor(255, 0, 0) };
    const QList<QVariantMap> sets = {
        {},
        { { "edge", 1.0 }, { "seed", 3.0 } },
        { { "edge", 0.0 }, { "flare", 1.0 } },
        { { "coolness", 0.5 }, { "intensity", 0.7 }, { "speed", 2.0 } },
        { { "spreadOver", 1 } },
    };
    int frames = 0;
    for (const QVariantMap &set : sets)
        for (int tick = 0; tick <= 120; ++tick)
        {
            const QVariantMap at = with(in, "_time", tick * 0.1);
            const QList<QVariantMap> was = scriptFrameColours(before, rig, at, set, two);
            /* A head the fire hasn't reached was held dark; since Q259 it is
               left to what is under it (nothing written). Everything else
               byte for byte. */
            auto same = [&](const QList<QVariantMap> &got) {
                QCOMPARE(got.size(), was.size());
                for (int i = 0; i < got.size(); ++i)
                {
                    if (got.at(i).isEmpty() && was.at(i) == QVariantMap({ { "dimmer", 0 } }))
                        continue;
                    QVERIFY2(got.at(i) == was.at(i),
                             qPrintable(QString("head %1 at %2 s differs").arg(i).arg(tick * 0.1)));
                }
            };
            // Fire by default, and chosen; and Front and Behind with no
            // colour to be the front: byte for byte what it was.
            same(scriptFrameColours(now, rig, at, set, two));
            if (QTest::currentTestFailed())
                return;
            same(scriptFrameColours(now, rig, at, with(set, "colours", 0), two));
            if (QTest::currentTestFailed())
                return;
            same(scriptFrameColours(now, rig, at, with(set, "colours", 1), {}));
            if (QTest::currentTestFailed())
                return;
            ++frames;
        }
    QCOMPARE(frames, 5 * 121);
}

void SpreadEffects_Test::fireSpreadFrontAndBehind()
{
    EffectScript fs;
    QVERIFY(fs.load(kScripts + "fire-spread.js"));
    // A par with a dimmer on the point, a colour-only par 1 m out, a
    // colour-wheel mover 2 m, a pixel of a bar with no dimmer 3 m, a dimmer
    // 3.5 m, a relay 4 m.
    const QList<QVariantMap> rig = {
        { { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", 0.0 } },
        { { "hasRGB", true }, { "fromTarget", 1.0 } },
        { { "hasColorWheel", true }, { "hasDimmer", true }, { "hasPanTilt", true }, { "fromTarget", 2.0 } },
        { { "heads", 64 }, { "hasRGB", true }, { "headRGB", true }, { "fromTarget", 3.0 } },
        { { "hasDimmer", true }, { "fromTarget", 3.5 } },
        { { "hasDimmer", true }, { "switches", true }, { "fromTarget", 4.0 } },
    };
    const QVariantMap in = { { "_fromTargetFar", 4.0 } };
    const QColor blue(0, 0, 255), red(255, 0, 0);
    // Catches at 0, 2, 4, 6, 7 and 8 s; the backfill 1.5 s after each.
    const QVariantMap hard = { { "colours", 1 }, { "edge", 0.0 }, { "flare", 0.0 },
                               { "spreadTime", 8.0 }, { "frontWidth", 1.5 } };
    const QVariantMap fire = { { "edge", 0.0 }, { "flare", 0.0 }, { "spreadTime", 8.0 } };
    auto at = [&](double t, const QVariantMap &set, const QList<QColor> &looks) {
        return scriptFrameColours(fs, rig, with(in, "_time", t), set, looks);
    };
    const QList<QColor> two = { blue, red };

    // Between the fronts: a head reached shows the first colour, at full.
    {
        const QList<QVariantMap> f = at(3.0, hard, two);
        QCOMPARE(f.at(1), rgb(0, 0, 255));                          // reached at 2, the backfill at 3.5
        const QList<QVariantMap> g = at(5.0, hard, two);
        QCOMPARE(g.at(2), with(rgb(0, 0, 255), "dimmer", 1));      // the wheel on its blue slot
        QCOMPARE(at(7.0, hard, two).at(3), rgb(0, 0, 255));         // a pixel
        QCOMPARE(at(1.0, hard, two).at(0), with(rgb(0, 0, 255), "dimmer", 1));
    }
    // A head the front hasn't reached yet is left to what is under it
    // (Q259): nothing written, the relay's neither.
    {
        const QList<QVariantMap> f = at(3.9, hard, two);
        QCOMPARE(f.at(2), QVariantMap());
        QCOMPARE(f.at(3), QVariantMap());
        QCOMPARE(f.at(4), QVariantMap());
        QCOMPARE(f.at(5), QVariantMap());
    }
    // After the backfill: the second colour, flickering in level (the
    // colour-only par in its colour, the par and the wheel on the dimmer).
    QSet<int> parLevels, plainReds, wheelLevels;
    for (double t = 10; t < 20; t += 0.25)
    {
        const QList<QVariantMap> f = at(t, hard, two);
        QCOMPARE(f.at(0).value("r").toInt(), 255);
        QCOMPARE(f.at(0).value("g").toInt(), 0);
        QCOMPARE(f.at(0).value("b").toInt(), 0);
        parLevels.insert(qRound(f.at(0).value("dimmer").toDouble() * 100));
        QVERIFY(!f.at(1).contains("dimmer"));
        QCOMPARE(f.at(1).value("g").toInt(), 0);
        QCOMPARE(f.at(1).value("b").toInt(), 0);
        plainReds.insert(f.at(1).value("r").toInt());
        QCOMPARE(f.at(2).value("r").toInt(), 255);                  // the wheel's red slot
        QCOMPARE(f.at(2).value("b").toInt(), 0);
        wheelLevels.insert(qRound(f.at(2).value("dimmer").toDouble() * 100));
        QCOMPARE(f.at(3).value("g").toInt(), 0);
        QCOMPARE(f.at(3).value("b").toInt(), 0);
        // The dimmer can't show a colour: it burns as Fire's. The relay is on.
        QCOMPARE(f.at(4), scriptFrameColours(fs, rig, with(in, "_time", t), fire, two).at(4));
        QCOMPARE(f.at(5), QVariantMap({ { "dimmer", 1 } }));
        for (const QVariantMap &o : f)
            QVERIFY(!o.contains("pan") && !o.contains("tilt"));
    }
    QVERIFY(parLevels.size() > 5);
    QVERIFY(plainReds.size() > 5);
    QVERIFY(wheelLevels.size() > 5);
    // Just after the backfill reaches it, a head is between the two colours.
    {
        const QVariantMap mid = at(3.7, hard, two).at(1);       // 0.2 s into a 0.4 s blend
        QVERIFY2(mid.value("b").toInt() > 50 && mid.value("b").toInt() < 205, qPrintable(QString::number(mid.value("b").toInt())));
        QVERIFY(mid.value("r").toInt() > 0);
        // A colour wheel never stops between slots: blue, then red.
        for (double t = 5.5; t < 6.0; t += 0.05)
        {
            const QVariantMap w = at(t, hard, two).at(2);
            QVERIFY((w.value("r").toInt() == 0 && w.value("b").toInt() == 255)
                    || (w.value("r").toInt() == 255 && w.value("b").toInt() == 0));
        }
    }
    // Flicker Behind off: the backfill is steady.
    for (double t = 10; t < 12; t += 0.25)
    {
        const QList<QVariantMap> f = at(t, with(hard, "flickerBehind", 0), two);
        QCOMPARE(f.at(0), with(rgb(255, 0, 0), "dimmer", 1));
        QCOMPARE(f.at(1), rgb(255, 0, 0));
        QCOMPARE(f.at(3), rgb(255, 0, 0));
    }
    // Only one colour: the front is it, and behind the front it burns as
    // the fire does.
    QCOMPARE(at(3.0, hard, { blue }).at(1), rgb(0, 0, 255));
    for (double t = 10; t < 14; t += 0.5)
        QCOMPARE(at(t, hard, { blue }), scriptFrameColours(fs, rig, with(in, "_time", t), fire, { blue }));
    // ...and Flicker Behind changes nothing then: a fire always flickers.
    QCOMPARE(at(11.0, with(hard, "flickerBehind", 0), { blue }), at(11.0, hard, { blue }));

    // The Flare: the front's colour whiter as it catches, then the colour.
    {
        const QVariantMap flaring = at(2.1, with(hard, "flare", 1.0), two).at(1);
        QVERIFY(flaring.value("r").toInt() > 100 && flaring.value("g").toInt() > 100);
        QCOMPARE(at(3.2, with(hard, "flare", 1.0), two).at(1), rgb(0, 0, 255));
        // Never on the wheel: it holds the blue slot.
        QCOMPARE(at(4.1, with(hard, "flare", 1.0), two).at(2).value("r").toInt(), 0);
    }
    // Front Width: the backfill that much behind.
    QCOMPARE(at(5.5, with(hard, "frontWidth", 4.0), two).at(1), rgb(0, 0, 255));
    QVERIFY(at(6.5, with(hard, "frontWidth", 4.0), two).at(1).value("b").toInt() == 0);
}

void SpreadEffects_Test::fireSpreadBackfillIsRaggedBySeed()
{
    EffectScript fs;
    QVERIFY(fs.load(kScripts + "fire-spread.js"));
    // Sixteen colour-only pars in a line out from the point.
    QList<QVariantMap> rig;
    for (int i = 0; i < 16; ++i)
        rig << QVariantMap({ { "hasRGB", true }, { "fromTarget", double(i) } });
    const QVariantMap in = { { "_fromTargetFar", 15.0 } };
    const QList<QColor> two = { QColor(0, 0, 255), QColor(255, 0, 0) };

    /* When each catches (its blue up: a kindle is fainter) and when its
       backfill arrives (the first of its red), to a frame. */
    auto fronts = [&](const QVariantMap &set, QVector<double> &caught, QVector<double> &filled) {
        caught.fill(-1, rig.size());
        filled.fill(-1, rig.size());
        for (int tick = 0; tick <= 700; ++tick)
        {
            const double t = tick * 0.02;
            const QList<QVariantMap> f = scriptFrameColours(fs, rig, with(in, "_time", t), set, two);
            for (int i = 0; i < rig.size(); ++i)
            {
                if (caught[i] < 0 && f.at(i).value("b").toInt() >= 200)
                    caught[i] = t;
                if (filled[i] < 0 && f.at(i).value("r").toInt() > 0)
                    filled[i] = t;
            }
        }
    };
    const QVariantMap base = { { "colours", 1 }, { "flare", 0.0 }, { "spreadTime", 8.0 },
                               { "frontWidth", 1.5 } };
    QVector<double> caught, filled;

    // A hard edge: the backfill Front Width behind every head (less the
    // front's 60 ms snap up).
    fronts(with(base, "edge", 0.0), caught, filled);
    for (int i = 0; i < rig.size(); ++i)
        QVERIFY2(caught[i] >= 0 && qAbs(filled[i] - caught[i] - 1.5) <= 0.08,
                 qPrintable(QString("head %1: caught %2, filled %3").arg(i).arg(caught[i]).arg(filled[i])));

    // A ragged edge: the backfill is ragged too, in its own way (not the
    // front's ragged line moved back), up to half the width early or late,
    // so the front's colour always shows; the same seed the same, another
    // seed another.
    QVariantMap ragged = with(with(base, "edge", 1.0), "seed", 3.0);
    fronts(ragged, caught, filled);
    QSet<int> gaps;
    for (int i = 0; i < rig.size(); ++i)
    {
        const double gap = filled[i] - caught[i];
        QVERIFY2(caught[i] >= 0 && filled[i] >= 0 && gap >= 0.75 - 0.08 && gap <= 2.25 + 0.04,
                 qPrintable(QString("head %1: caught %2, filled %3").arg(i).arg(caught[i]).arg(filled[i])));
        gaps.insert(qRound(gap * 10));
    }
    QVERIFY2(gaps.size() > 4, qPrintable(QString("%1 different gaps").arg(gaps.size())));
    QVector<double> caught2, filled2;
    fronts(ragged, caught2, filled2);
    QCOMPARE(caught2, caught);
    QCOMPARE(filled2, filled);
    fronts(with(ragged, "seed", 4.0), caught2, filled2);
    QVERIFY(filled2 != filled);
}

void SpreadEffects_Test::lightningSpreadBurstsOutAndFlickersOut()
{
    EffectScript ls;
    QVERIFY(ls.load(kScripts + "lightning-spread.js"));
    QCOMPARE(ls.name(), QString("Lightning Spread"));
    QVERIFY(ls.wantsTargetDistance());
    QVERIFY(ls.endsWhenDone());
    QVERIFY(ls.fixtureTypes().contains("dimmer"));
    QVERIFY(ls.fixtureTypes().contains("moving"));

    // Six pars with a dimmer, 0 to 5 m from the point; the blast takes a
    // second to reach the farthest, a clean ring (Edge 0); each head
    // flickers out over 1.5 s.
    QList<QVariantMap> rig;
    for (int i = 0; i < 6; ++i)
        rig << QVariantMap{ { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", double(i) } };
    const QVariantMap inputs = { { "_fromTargetFar", 5.0 } };
    const QVariantMap set = { { "mode", 0 }, { "spreadTime", 1.0 }, { "edge", 0.0 }, { "decay", 1.5 },
                              { "strikes", 3 }, { "seed", 7 } };
    const double step = 0.01;
    auto trace = [&](const QVariantMap &with) {
        ScriptRun run(ls, rig, inputs, with);
        QList<QList<double>> levels;            // [tick][head]
        bool doneEarly = false;
        for (int k = 0; k <= 300; ++k)
        {
            const QList<QVariantMap> f = run.at(k * step);
            QList<double> row;
            for (const QVariantMap &o : f)
                row << levelOf(o);
            levels << row;
            if (k * step < 1.0 + 1.5 - 0.02 && run.done())
                doneEarly = true;
        }
        return qMakePair(levels, doneEarly || !run.done());
    };
    const auto burst = trace(set);
    QVERIFY2(!burst.second, "done before the last head flickered out, or never done");
    const QList<QList<double>> &lv = burst.first;

    // Hit in order of distance: head i at i/5 of the second, at full the
    // tick it is hit (a sharp attack), nothing said before.
    QVector<int> hit(6, -1);
    for (int k = 0; k < lv.size(); ++k)
        for (int h = 0; h < 6; ++h)
            if (hit[h] < 0 && lv[k][h] >= 0)
                hit[h] = k;
    for (int h = 0; h < 6; ++h)
    {
        QVERIFY2(hit[h] >= 0, qPrintable(QString("head %1 never hit").arg(h)));
        QVERIFY2(qAbs(hit[h] * step - h * 0.2) <= 0.011,
                 qPrintable(QString("head %1 hit at %2 s, not %3").arg(h).arg(hit[h] * step).arg(h * 0.2)));
        QCOMPARE(lv[hit[h]][h], 1.0);
        if (h > 0)
            QVERIFY(hit[h] > hit[h - 1]);
    }

    // Lightning, not a sine: on each head the strike dies away, then it
    // strikes again at least once -- a jump up in one tick -- after
    // falling well below full.
    for (int h = 0; h < 6; ++h)
    {
        int restrikes = 0;
        double lowest = 1.0;
        for (int k = hit[h] + 1; k < lv.size() && lv[k][h] >= 0; ++k)
        {
            if (lv[k][h] - lv[k - 1][h] > 0.15)
                ++restrikes;
            if (restrikes == 0)
                lowest = qMin(lowest, lv[k][h]);
        }
        QVERIFY2(restrikes >= 1, qPrintable(QString("head %1 flickered with no re-strike").arg(h)));
        QVERIFY2(lowest < 0.3, qPrintable(QString("head %1 only fell to %2 before its re-strike").arg(h).arg(lowest)));
    }

    // At rest after its Flicker Out: nothing written, every head.
    for (int k = 0; k < lv.size(); ++k)
        for (int h = 0; h < 6; ++h)
            if (k * step >= h * 0.2 + 1.5 + 0.005)
                QVERIFY2(lv[k][h] < 0, qPrintable(QString("head %1 still lit at %2 s").arg(h).arg(k * step)));

    // Repeatable: the same seed is the same burst; another seed another.
    QCOMPARE(trace(set).first, lv);
    QVariantMap other = set;
    other.insert("seed", 8);
    QVERIFY(trace(other).first != lv);

    // No colour: a cold white; a colour of the look's: that colour.
    {
        ScriptRun run(ls, rig, inputs, set);
        const QVariantMap o = run.at(0.0).first();
        QVERIFY(o.value("b").toInt() >= o.value("r").toInt());
        QVERIFY(o.value("r").toInt() > 150);
        QVariantMap own = set;
        own.insert("strikeColour", 1);          // Look's Colour: every level in it
        ScriptRun red(ls, rig, inputs, own, { QColor(255, 0, 0) });
        const QVariantMap r = red.at(0.0).first();
        QCOMPARE(r.value("r").toInt(), 255);
        QCOMPARE(r.value("g").toInt(), 0);
        QCOMPARE(r.value("dimmer").toDouble(), 1.0);
    }
}

void SpreadEffects_Test::lightningSpreadStrikesWhiteHotWithABlueEdge()
{
    EffectScript ls;
    QVERIFY(ls.load(kScripts + "lightning-spread.js"));
    // One par with a dimmer on the point, a red look; the default Strike
    // (White-Hot): white as it strikes, a cold blue edge on the way down,
    // the look's red as it dies.
    const QList<QVariantMap> rig = { { { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", 0.0 } } };
    const QVariantMap inputs = { { "_fromTargetFar", 1.0 } };
    const QVariantMap set = { { "mode", 0 }, { "spreadTime", 0.1 }, { "edge", 0.0 }, { "decay", 2.0 },
                              { "strikes", 1 }, { "seed", 3 } };
    ScriptRun run(ls, rig, inputs, set, { QColor(255, 0, 0) });
    const QVariantMap peak = run.at(0.0).first();
    QCOMPARE(levelOf(peak), 1.0);
    QCOMPARE(QColor(peak.value("r").toInt(), peak.value("g").toInt(), peak.value("b").toInt()), QColor(255, 255, 255));
    bool edge = false, dying = false;
    for (int k = 1; k <= 200; ++k)
    {
        const QVariantMap o = run.at(k * 0.002).first();
        const double l = levelOf(o);
        const int r = o.value("r").toInt(), g = o.value("g").toInt(), b = o.value("b").toInt();
        if (l > 0.32 && l < 0.5)
        {
            QVERIFY2(b > r && b > g, qPrintable(QString("at %1: %2,%3,%4, not the blue edge").arg(l).arg(r).arg(g).arg(b)));
            edge = true;
        }
        if (l > 0 && l < 0.08)
        {
            QVERIFY2(r > b && r > g, qPrintable(QString("at %1: %2,%3,%4, not dying in the red").arg(l).arg(r).arg(g).arg(b)));
            dying = true;
        }
    }
    QVERIFY2(edge && dying, "the strike never passed through its edge and its dying levels");

    // A colour wheel holds the look's slot: the wheel never turns in a flicker.
    const QList<QVariantMap> wheel = { { { "hasColorWheel", true }, { "hasDimmer", true }, { "fromTarget", 0.0 } } };
    ScriptRun spun(ls, wheel, inputs, set, { QColor(255, 0, 0) });
    for (int k = 0; k <= 100; ++k)
    {
        const QVariantMap o = spun.at(k * 0.004).first();
        if (levelOf(o) > 0)
            QCOMPARE(QColor(o.value("r").toInt(), o.value("g").toInt(), o.value("b").toInt()), QColor(255, 0, 0));
    }
}

void SpreadEffects_Test::amberPulseHeadsWanderOnTheirOwn()
{
    EffectScript ap;
    QVERIFY(ap.load(kScripts + "amber-pulse.js"));
    // Eight heads in a row, all on the same beat (Spread 0), full Variation:
    // the wander is each head's own.
    QList<QVariantMap> rig;
    for (int i = 0; i < 8; ++i)
        rig << QVariantMap{ { "hasRGB", true } };
    ScriptRun run(ap, rig, QVariantMap(), { { "spread", 0 }, { "variation", 1.0 }, { "rate", 0.8 } });
    QList<QList<double>> a(8);                  // [head][tick]
    for (int k = 0; k < 600; ++k)
    {
        const QList<QVariantMap> f = run.at(k * 0.05);
        for (int i = 0; i < 8; ++i)
            a[i] << f.at(i).value("a").toDouble();
    }
    auto corr = [&](int x, int y) {
        double mx = 0, my = 0;
        for (int k = 0; k < a[x].size(); ++k) { mx += a[x][k]; my += a[y][k]; }
        mx /= a[x].size(); my /= a[y].size();
        double sxy = 0, sxx = 0, syy = 0;
        for (int k = 0; k < a[x].size(); ++k)
        {
            sxy += (a[x][k] - mx) * (a[y][k] - my);
            sxx += (a[x][k] - mx) * (a[x][k] - mx);
            syy += (a[y][k] - my) * (a[y][k] - my);
        }
        return sxy / std::sqrt(sxx * syy);
    };
    // In step with the order, every pair of neighbours moved alike (-0.55
    // each, the same lag): each its own, no pair is much alike.
    for (int i = 0; i + 1 < 8; ++i)
    {
        const double c = corr(i, i + 1);
        QVERIFY2(qAbs(c) < 0.35, qPrintable(QString("heads %1 and %2 move alike (%3): bands").arg(i).arg(i + 1).arg(c)));
    }
}

void SpreadEffects_Test::lightningSpreadOneBurstEndsAtRest()
{
    QLCFixtureDef *rgbDef = loadDef(m_doc, kFixtures + "Generic/Generic-Generic-RGB.qxf");
    QVERIFY(rgbDef != nullptr);
    Fixture *nearWash = place(m_doc, rgbDef, "RGB", "Near", 0, 0, QVector3D(1000, 2000, 0));
    Fixture *farWash = place(m_doc, rgbDef, "RGB", "Far", 0, 3, QVector3D(9000, 2000, 0));
    QVERIFY(nearWash && farWash);
    QLCPalette *aim = aimAt(m_doc, QVector3D(1.0f, 2.0f, 0));
    auto lit = [](const QHash<quint64, int> &f, Fixture *fx) {
        return f.value(key(fx->id(), 0), 0) + f.value(key(fx->id(), 1), 0) + f.value(key(fx->id(), 2), 0) > 0;
    };
    auto look = [&](QLCPalette *fx) {
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        scene->addFixture(nearWash->id());
        scene->addFixture(farWash->id());
        scene->addPalette(aim->id());
        scene->addPalette(fx->id());
        return scene;
    };
    const QVariantMap set = { { "edge", 0.0 }, { "spreadTime", 1.0 }, { "decay", 1.0 } };

    // Once, Hold, Length auto: the near wash flashes at once, the far one
    // a second later; both out by 2 s, and the run is over then -- held at
    // rest, nothing written, frame for frame.
    {
        QLCPalette *fx = effectPalette(m_doc, "lightning-spread.js", set);
        fx->setEffectParamValue(EffectRuns::runsKey(), 1);         // Once
        fx->setEffectParamValue(EffectRuns::atEndKey(), 0);        // Hold
        EffectInstance inst(m_doc, look(fx)->id(), fx->id());
        QVERIFY2(inst.isValid(), qPrintable(inst.error()));
        QHash<quint64, int> f = frameAt(inst, 0);
        QVERIFY(lit(f, nearWash));
        QVERIFY(!lit(f, farWash));
        f = frameAt(inst, 1020);
        QVERIFY(lit(f, farWash));
        QVERIFY(!inst.runDone());
        f = frameAt(inst, 2100);
        QVERIFY2(f.isEmpty(), "not at rest once both have flickered out");
        QVERIFY(inst.runDone());
        for (qint64 ms : { 3000, 10000, 60000 })
            QVERIFY2(frameAt(inst, ms).isEmpty(), qPrintable(QString("not held at rest at %1 ms").arg(ms)));
    }

    // Loop (the default): one burst, then at rest; it never bursts again
    // and never says it is done.
    {
        QLCPalette *fx = effectPalette(m_doc, "lightning-spread.js", set);
        EffectInstance inst(m_doc, look(fx)->id(), fx->id());
        QVERIFY(inst.isValid());
        QVERIFY(lit(frameAt(inst, 0), nearWash));
        for (qint64 ms = 2100; ms <= 20000; ms += 300)
            QVERIFY2(frameAt(inst, ms).isEmpty(), qPrintable(QString("lit again at %1 ms").arg(ms)));
        QVERIFY(!inst.runDone());
    }
}

void SpreadEffects_Test::lightningSpreadStaticGeneratorStaysInItsReach()
{
    EffectScript ls;
    QVERIFY(ls.load(kScripts + "lightning-spread.js"));
    // Eleven heads 0 to 10 m out; the arcs reach half way.
    QList<QVariantMap> rig;
    for (int i = 0; i <= 10; ++i)
        rig << QVariantMap{ { "hasRGB", true }, { "hasDimmer", true }, { "fromTarget", double(i) } };
    const QVariantMap set = { { "mode", 1 }, { "reach", 50 }, { "rate", 4 }, { "crawl", 0.3 },
                              { "core", 25 }, { "seed", 3 } };
    ScriptRun run(ls, rig, { { "_fromTargetFar", 10.0 } }, set);
    QVector<int> lit(11, 0), changes(11, 0);
    QVector<double> last(11, -2.0);
    for (int k = 0; k <= 1000; ++k)                 // 10 s at 10 ms
    {
        const QList<QVariantMap> f = run.at(k * 0.01);
        QVERIFY(!run.done());
        // The centre: never dark.
        QVERIFY2(levelOf(f.at(0)) > 0, qPrintable(QString("the centre dark at %1 s").arg(k * 0.01)));
        for (int h = 0; h <= 10; ++h)
        {
            const double l = levelOf(f.at(h));
            if (l > 0)
                ++lit[h];
            if (l != last[h])
                ++changes[h];
            last[h] = l;
            // Beyond the Reach (half way): never written.
            if (h > 5)
                QVERIFY2(l < 0, qPrintable(QString("head %1 (beyond the reach) at %2 s").arg(h).arg(k * 0.01)));
        }
    }
    // The arcs reach out: the heads inside the reach flicker now and then.
    for (int h = 1; h <= 4; ++h)
        QVERIFY2(lit[h] > 0 && lit[h] < 1000, qPrintable(QString("head %1 lit %2 ticks").arg(h).arg(lit[h])));
    // The centre's glow is jagged, not steady.
    QVERIFY2(changes[0] > 100, qPrintable(QString("the centre changed %1 times").arg(changes[0])));
}

void SpreadEffects_Test::lightningSpreadOnDimmersAndMovers()
{
    // A plain dimmer on the point (one channel, no definition), a moving
    // wash 2 m out (pan, tilt, colour, master dimmer), a silk flame's relay.
    Fixture *dim = new Fixture(m_doc);
    dim->setName("Dimmer");
    dim->setUniverse(0);
    dim->setAddress(0);
    dim->setChannels(1);
    QVERIFY(m_doc->addFixture(dim));
    m_doc->monitorProperties()->setFixturePosition(dim->id(), 0, 0, QVector3D(1000, 2000, 0));
    QLCFixtureDef *moverDef = loadDef(m_doc, kFixtures + "Eurolite/Eurolite-TMH-11-Moving-Head-Wash.qxf");
    QLCFixtureDef *flameDef = loadDef(m_doc, kFixtures + "Generic/Generic-Silk-Flame.qxf");
    QVERIFY(moverDef && flameDef);
    Fixture *mover = place(m_doc, moverDef, "Standard", "Mover", 0, 10, QVector3D(3000, 2000, 0));
    Fixture *flame = place(m_doc, flameDef, "Relay", "Flame", 0, 30, QVector3D(2000, 2000, 0));
    QVERIFY(mover && flame);

    QLCPalette *fx = effectPalette(m_doc, "lightning-spread.js",
                                   { { "edge", 0.0 }, { "spreadTime", 0.5 }, { "decay", 1.5 } });
    Scene *scene = new Scene(m_doc);
    m_doc->addFunction(scene);
    for (Fixture *f : { dim, mover, flame })
        scene->addFixture(f->id());
    scene->addPalette(aimAt(m_doc, QVector3D(1.0f, 2.0f, 0))->id());
    scene->addPalette(fx->id());
    EffectInstance inst(m_doc, scene->id(), fx->id());
    QVERIFY2(inst.isValid(), qPrintable(inst.error()));

    QSet<int> dimmerLevels, moverLevels;
    for (qint64 ms = 0; ms <= 2500; ms += 20)
    {
        const QHash<quint64, int> f = frameAt(inst, ms);
        if (f.contains(key(dim->id(), 0)))
            dimmerLevels << f.value(key(dim->id(), 0));
        if (f.contains(key(mover->id(), 8)))
            moverLevels << f.value(key(mover->id(), 8));
        // Never moved: pan, tilt and their fine channels left alone.
        for (quint32 c : { 0u, 1u, 10u, 11u })
            QVERIFY2(!f.contains(key(mover->id(), c)), qPrintable(QString("mover channel %1 written at %2 ms").arg(c).arg(ms)));
        QVERIFY2(!f.contains(key(flame->id(), 0)), "the relay was switched");
    }
    // The dimmer flickers in level: up to full, down, and between.
    QVERIFY2(dimmerLevels.contains(255), "the dimmer never flashed to full");
    QVERIFY2(dimmerLevels.size() >= 5, qPrintable(QString("the dimmer took %1 levels").arg(dimmerLevels.size())));
    QVERIFY2(moverLevels.contains(255) && moverLevels.size() >= 5, "the mover's dimmer did not flicker");
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
    const QList<Fixture *> bars = placeStepBars(m_doc, stepDef);
    QCOMPARE(bars.size(), 96);

    struct Cost { QString what; double tickMs = 0, scriptMs = 0; };
    /* An effect kept ready to be timed again: its instance and the arguments
       the host makes for its script. */
    struct Rig
    {
        std::unique_ptr<EffectInstance> inst;
        qint64 fromMs = 0;
        QJSValue fixtures, inputs, palettes, params, state, data;
    };
    // The whole host tick, per tick, over @p n ticks from the rig's clock.
    auto timeTick = [](Rig &r, int n) {
        QElapsedTimer t;
        t.start();
        for (int i = 1; i <= n; ++i)
        {
            r.inst->setOffStageClock(r.fromMs + i * 20);
            r.inst->runTick();
        }
        return double(t.nsecsElapsed()) / 1e6 / n;
    };
    // The script alone, per tick, over the same ticks.
    auto timeScript = [](Rig &r, int n) {
        QElapsedTimer t;
        t.start();
        for (int i = 1; i <= n; ++i)
        {
            r.inputs.setProperty("_time", double(r.fromMs + i * 20) / 1000.0);
            r.inst->m_script.callTick(r.fixtures, r.inputs, r.palettes, r.params, r.state, r.data);
        }
        return double(t.nsecsElapsed()) / 1e6 / n;
    };
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
                       bool ownColour, std::shared_ptr<Rig> *keep = nullptr) {
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
        auto rig = std::make_shared<Rig>();
        rig->inst.reset(new EffectInstance(m_doc, scene->id(), p->id()));
        rig->fromMs = fromMs;
        EffectInstance &inst = *rig->inst;
        if (!inst.isValid())
            return false;
        frameAt(inst, fromMs);                       // the first: cells and places
        const quint64 places = inst.placesWorkedOut();
        Cost c;
        c.what = what;
        c.tickMs = timeTick(*rig, ticks);
        // Its places stay worked out at this size.
        if (path.endsWith("spread.js") && inst.placesWorkedOut() != places)
            return false;
        // The script alone, on the arguments the host made.
        int cols = 0, rows = 0;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, rows);
        if (cells.size() != 96 * 64)
            return false;
        rig->fixtures = inst.buildFixturesArray(cells, cols, rows);
        rig->inputs = inst.buildInputsObject();
        rig->palettes = inst.buildPalettesObject();
        rig->params = inst.buildParamsObject();
        rig->state = inst.m_script.newState();
        rig->data = inst.m_script.engine()->newObject();
        // Its first tick works out what it keeps; the rest are timed.
        rig->inputs.setProperty("_time", double(fromMs) / 1000.0);
        inst.m_script.callTick(rig->fixtures, rig->inputs, rig->palettes, rig->params, rig->state, rig->data);
        c.scriptMs = timeScript(*rig, ticks);
        costs << c;
        if (keep)
            *keep = rig;
        return true;
    };
    const QVariantMap fireSet = { { "edge", 0.3 }, { "spreadTime", 8.0 } };
    const QVariantMap spellSet = { { "edge", 0.2 }, { "spreadTime", 8.0 } };
    QVERIFY(measure("nothing (the host alone)", dir.filePath("nothing.js"), {}, 4000, false));
    std::shared_ptr<Rig> fireRig, burstRig, genRig;
    QVERIFY(measure("Fire Spread, spreading", kScripts + "fire-spread.js", fireSet, 4000, false, &fireRig));
    QVERIFY(measure("Fire Spread, all burning", kScripts + "fire-spread.js", fireSet, 20000, false));
    QVERIFY(measure("Spell Spread, spreading", kScripts + "spell-spread.js", spellSet, 4000, true));
    QVERIFY(measure("Spell Spread, all reached", kScripts + "spell-spread.js", spellSet, 20000, true));
    const QVariantMap boltSet = { { "edge", 0.4 }, { "spreadTime", 8.0 }, { "decay", 8.0 } };
    const QVariantMap genSet = { { "mode", 1 }, { "reach", 100 }, { "rate", 8 } };
    QVERIFY(measure("Lightning Spread, bursting", kScripts + "lightning-spread.js", boltSet, 4000, false, &burstRig));
    QVERIFY(measure("Lightning Spread, at rest", kScripts + "lightning-spread.js", boltSet, 20000, false));
    QVERIFY(measure("Lightning Spread, generator", kScripts + "lightning-spread.js", genSet, 4000, false, &genRig));

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
    // Lightning Spread costs no more than twice what Fire Spread does as it
    // spreads, its script or the whole tick, bursting or generating.
    /* The comparison. One timing of each, taken once, is at the mercy of
       whatever else a shared runner does in those milliseconds (Windows CI
       read Lightning at 4.65 ms against Fire's 1.98 ms, twice, on a script
       that costs about the same). So each is timed again in rounds that go
       Fire, Lightning burst, Lightning generator, Fire ... -- a slow patch
       hits all three -- over the very same ticks (the scripts are seeded
       hashes of the clock, so a round does the same work as the last), and
       the least of the rounds is read: that is the cost with the machine
       out of the way, and a script that really costs more than twice
       Fire's costs more in every round. */
    const int rounds = 5;
    struct Best { double scriptMs = 1e9, tickMs = 1e9; };
    Best fireBest, burstBest, genBest;
    const std::shared_ptr<Rig> order[] = { fireRig, burstRig, genRig };
    Best *bests[] = { &fireBest, &burstBest, &genBest };
    for (int round = 0; round < rounds; ++round)
        for (int k = 0; k < 3; ++k)
        {
            bests[k]->scriptMs = qMin(bests[k]->scriptMs, timeScript(*order[k], ticks));
            bests[k]->tickMs = qMin(bests[k]->tickMs, timeTick(*order[k], ticks));
        }
    qInfo().noquote() << QString("least of %1 interleaved rounds: Fire Spread script %2 ms tick %3 ms;"
                                 " Lightning bursting script %4 ms tick %5 ms; generator script %6 ms tick %7 ms")
                             .arg(rounds).arg(fireBest.scriptMs, 0, 'f', 2).arg(fireBest.tickMs, 0, 'f', 2)
                             .arg(burstBest.scriptMs, 0, 'f', 2).arg(burstBest.tickMs, 0, 'f', 2)
                             .arg(genBest.scriptMs, 0, 'f', 2).arg(genBest.tickMs, 0, 'f', 2);
    const struct { const char *what; Best b; } lightning[] = { { "Lightning Spread, bursting", burstBest },
                                                              { "Lightning Spread, generator", genBest } };
    for (const auto &l : lightning)
    {
        QVERIFY2(l.b.scriptMs <= 2.0 * fireBest.scriptMs + 0.5,
                 qPrintable(QString("%1: script %2 ms, Fire Spread's %3 ms (least of %4 rounds)")
                                .arg(l.what).arg(l.b.scriptMs).arg(fireBest.scriptMs).arg(rounds)));
        QVERIFY2(l.b.tickMs <= 2.0 * fireBest.tickMs + 0.5,
                 qPrintable(QString("%1: tick %2 ms, Fire Spread's %3 ms (least of %4 rounds)")
                                .arg(l.what).arg(l.b.tickMs).arg(fireBest.tickMs).arg(rounds)));
    }
    // The numbers, brought back from the pool (check-all.sh --shots).
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QFile out(QDir(shots).filePath(QString("spread-tick-cost-%1.txt").arg(build)));
        if (out.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&out) << lines.join("\n") << "\n";
    }
}

void SpreadEffects_Test::theHostTickCostOverTheStepBars()
{
    /* It measures, so it takes its time: the whole run is reported at the
       end (and in unittest-serial.txt, so nothing else is measured with
       it). Over ten seconds is on purpose -- 6,144 heads are ticked for
       each of two answers, three times over, and the least of the three is
       what is read. */
    QElapsedTimer took;
    took.start();
    QLCFixtureDef *stepDef = loadDef(m_doc, kData + "PHS-Chorus-Step-Row-64.qxf");
    QVERIFY(stepDef != nullptr);
    const QList<Fixture *> bars = placeStepBars(m_doc, stepDef);
    QCOMPARE(bars.size(), 96);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    /* The two ends of what any spread hands back, head by head: nothing at
       all (the fire has not reached it -- it keeps what is under it) and a
       colour (all burning). Between them lies every effect's answer, so
       what the host costs for both is what it costs for all. */
    auto script = [&dir](const QString &name, const QString &intent) -> QString {
        QFile f(dir.filePath(name + ".js"));
        if (!f.open(QIODevice::WriteOnly))
            return QString();
        // It names palettes.look, so the host hands it the look's colour
        // rather than painting that over what it returns.
        f.write(QString("(function() { var e = {}; e.apiVersion = 1; e.name = 'T %1';"
                        " /* palettes.look */ e.fixtureTypes = ['rgb'];"
                        " e.tick = function(fx) { return fx.map(function() { return %2; }); };"
                        " return e; })()").arg(name, intent).toUtf8());
        return f.fileName();
    };
    const QString saysNothing = script(QStringLiteral("nothing"), QStringLiteral("{}"));
    const QString saysColour = script(QStringLiteral("colour"), QStringLiteral("{ r: 255, g: 80, b: 0 }"));
    QVERIFY(!saysNothing.isEmpty() && !saysColour.isEmpty());

#ifdef QT_NO_DEBUG
    const int ticks = qMax(12, qEnvironmentVariableIntValue("QLC_HOST_TICKS"));
#else
    const int ticks = qMax(4, qEnvironmentVariableIntValue("QLC_HOST_TICKS"));
#endif
    struct Piece { QString what; double ms = 0; };
    struct Row { QString what; QList<Piece> pieces; double hostMs = 0, tickMs = 0, baseMs = 0,
                 budget = 0; };
    QList<Row> table;

    /* The budget, one for each. The host's own work -- the cells, the look's
       base values, the per-head arguments and reading the answers -- runs on
       the GUI thread 50 times a second, beside the rig view and the whole UI:
       at a few ms a tick the operator sees the window go chunky (2026-11-17,
       the uplights added to Fire Spread). On val (Intel, Release) it measured
       14.2 ms saying nothing and 21.9 ms with every head a colour before
       perf/053-effect-host, and 53 / 98 ms unoptimised; it now sits at about
       a third of these lines. An unoptimised build pays about four times as
       much, and CI's instrumented leg (QLC_SLOW_BUILD) three times that
       again. QLC_HOST_TICK_BUDGET_MS overrides them both (0: no budget). */
#ifdef __OPTIMIZE__
    const double scale = 1.0;
#else
    const double scale = 4.0;
#endif
    const double slow = qEnvironmentVariableIsSet("QLC_SLOW_BUILD") ? 3.0 : 1.0;
    const bool pinned = qEnvironmentVariableIsSet("QLC_HOST_TICK_BUDGET_MS");
    const double pin = pinned ? qEnvironmentVariable("QLC_HOST_TICK_BUDGET_MS").toDouble() : 0.0;

    auto measure = [&](const QString &what, const QString &path, double budgetMs) -> bool {
        QLCPalette *pal = new QLCPalette(QLCPalette::Effect, m_doc);
        pal->setScriptPath(path);
        pal->setEffectParamValue(EffectRuns::runsKey(), 0);         // Loop
        m_doc->addPalette(pal);
        Scene *scene = new Scene(m_doc);
        m_doc->addFunction(scene);
        for (Fixture *f : bars)
            scene->addFixture(f->id());
        /* As a cue carries it: a Colour BEFORE the effect is the base the
           effect plays over (the show's blue under Fire Spread), so the
           host works out every head's base value, as it does on the rig. */
        scene->addPalette(colourPalette(m_doc, QColor(0, 0, 255))->id());
        scene->addPalette(aimAt(m_doc, QVector3D(0.0f, 1.0f, 0))->id());
        scene->addPalette(pal->id());
        EffectInstance inst(m_doc, scene->id(), pal->id());
        if (!inst.isValid())
            return false;
        inst.setOffStageClock(4000);
        inst.runTick();                   // the first: the cells, the facts, the places

        int cols = 1, gridRows = 1;
        const QList<EffectInstance::EffectCell> cells = inst.effectiveCells(cols, gridRows);
        if (cells.size() != 96 * 64)
            return false;
        // One answer, read again and again: what the host pays to read it.
        QJSValue fixtures = inst.buildFixturesArray(cells, cols, gridRows);
        QJSValue inputs = inst.buildInputsObject();
        const QJSValue palettes = inst.buildPalettesObject();
        const QJSValue params = inst.buildParamsObject();
        QJSValue state = inst.m_script.newState();
        const QJSValue data = inst.m_script.engine()->newObject();
        inputs.setProperty("_time", 4.0);
        const QJSValue answer = inst.m_script.callTick(fixtures, inputs, palettes, params, state, data);
        if (!answer.isArray())
            return false;
        inst.collectGarbage();            // not a GC pause inside a measurement

        QElapsedTimer clock;
        clock.start();
        /* The least of three runs: CPU time still swells when the machine
           is busy (shared caches), and what is really slow stays slow in
           every run (EffectInstance_Test's rule). */
        auto best = [&](const std::function<void()> &run) {
            double ms = 0;
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                const qint64 from = threadCpuNs(clock);
                for (int i = 0; i < ticks; ++i)
                    run();
                const double one = double(threadCpuNs(clock) - from) / 1e6 / ticks;
                ms = attempt == 0 ? one : qMin(ms, one);
            }
            return ms;
        };
        Row r;
        r.what = what;
        r.budget = pinned ? pin : budgetMs * scale * slow;
        int c2 = 1, r2 = 1;
        r.pieces << Piece{ QStringLiteral("cells"), best([&]() { inst.effectiveCells(c2, r2); }) };
        r.pieces << Piece{ QStringLiteral("look base"), best([&]() { inst.prepareSceneBase(); }) };
        r.pieces << Piece{ QStringLiteral("arguments"),
                           best([&]() { inst.buildFixturesArray(cells, cols, gridRows); }) };
        r.pieces << Piece{ QStringLiteral("answers"), best([&]() { inst.parseIntents(answer, cells); }) };
        for (const Piece &pc : r.pieces)
            r.hostMs += pc.ms;
        /* What the base values cost WHEN an effect reads one (a dimmer or a
           level intent, a look that points a head): not part of the host's
           tick for these two, which only set colours -- reported so that it
           is known, and so that a change which starts reading them shows up
           in "arguments" or "answers" above. */
        r.baseMs = best([&]() { inst.buildSceneBaseValues(); });
        qint64 at = 4000;
        r.tickMs = best([&]() { inst.setOffStageClock(at += 20); inst.runTick(); });
        table << r;
        return true;
    };
    QVERIFY(measure(QStringLiteral("says nothing"), saysNothing, 6.0));
    QVERIFY(measure(QStringLiteral("every head a colour"), saysColour, 13.0));

#ifdef QT_NO_DEBUG
    const QString build = QStringLiteral("release");
#else
    const QString build = QStringLiteral("debug");
#endif
    QStringList lines;
    for (const Row &r : table)
    {
        QStringList parts;
        for (const Piece &pc : r.pieces)
            parts << QString("%1 %2").arg(pc.what).arg(pc.ms, 0, 'f', 2);
        lines << QString("%1 build, 96 step bars x 64 (6144 heads), the host, %2:"
                         " %3 ms a tick (%4; %5 us a head); whole tick %6 ms;"
                         " base values when read %7 ms; budget %8 ms")
                     .arg(build, r.what).arg(r.hostMs, 0, 'f', 2).arg(parts.join(", "))
                     .arg(r.hostMs * 1000.0 / 6144.0, 0, 'f', 3).arg(r.tickMs, 0, 'f', 2)
                     .arg(r.baseMs, 0, 'f', 2).arg(r.budget, 0, 'f', 2);
    }
    for (const QString &l : lines)
        qInfo().noquote() << l;

    // The numbers, brought back from the pool (check-all.sh --shots).
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        QFile out(QDir(shots).filePath(QString("host-tick-cost-%1.txt").arg(build)));
        if (out.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&out) << lines.join("\n") << "\n";
    }
    auto pieceMs = [](const Row &r, const char *what) {
        for (const Piece &pc : r.pieces)
            if (pc.what == QLatin1String(what))
                return pc.ms;
        return 0.0;
    };

    /* What the rig's own work costs a tick, as a SHARE of it. Laying the
       6,144 cells out and working out the look's base values were a third
       of the host's tick before perf/053-effect-host (0.17 + 1.06 of
       3.68 ms on an M-series Mac, 0.68 + 3.88 of 14.24 ms on an Intel one);
       they are kept, and worked out on demand, so they are a rounding error
       now. A share and not a time, because both are measured in the same
       run as the tick they are a share of: how fast the machine is cancels
       out, which an absolute budget cannot do -- 6 ms passes the OLD code
       on a fast Mac and would fail the new code on a slow one. */
    for (const Row &r : table)
    {
        const double once = pieceMs(r, "cells") + pieceMs(r, "look base");
        QVERIFY2(r.hostMs > 0 && once <= 0.10 * r.hostMs,
                 qPrintable(QString("%1: the cells and the look's base values cost %2 of the"
                                    " host's %3 ms a tick (%4 per cent), more than a tenth")
                                .arg(r.what).arg(once, 0, 'f', 2).arg(r.hostMs, 0, 'f', 2)
                                .arg(r.hostMs > 0 ? 100.0 * once / r.hostMs : 0.0, 0, 'f', 0)));
    }
#ifdef __OPTIMIZE__
    /* ...and handing the rig over costs well under reading its answers
       back. Naming two or three properties on each of 6,144 cells made the
       arguments as dear as the answers (2.12 against 1.98 ms on an M-series
       Mac, 8.45 against 8.55 on an Intel one); each cell's objects are
       changed in place now, so no cell is named again. Optimised builds
       only: unoptimised, reading an answer swells enough to hide it. */
    for (const Row &r : table)
    {
        if (r.what != QStringLiteral("every head a colour"))
            continue;
        const double args = pieceMs(r, "arguments"), answers = pieceMs(r, "answers");
        QVERIFY2(answers > 0 && args <= 0.5 * answers,
                 qPrintable(QString("the per-head arguments cost %1 ms against %2 ms to read"
                                    " the answers: more than half")
                                .arg(args, 0, 'f', 2).arg(answers, 0, 'f', 2)));
    }
#endif

    /* The budget is the backstop over all of it: a share stays right while
       everything gets slower together. */
    for (const Row &r : table)
        if (r.budget > 0)
            QVERIFY2(r.hostMs <= r.budget,
                     qPrintable(QString("the host costs %1 ms a tick over 6,144 heads (%2),"
                                        " budget %3 ms")
                                    .arg(r.hostMs, 0, 'f', 2).arg(r.what).arg(r.budget, 0, 'f', 2)));
    qInfo().noquote() << "the whole measurement took" << took.elapsed() << "ms";
}

QTEST_MAIN(SpreadEffects_Test)
