/*
  Q Light Controller Plus - qlcconsole
  pagescript_test.cpp

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
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include "pagescript_test.h"
#include "webaccessfocus.h"
#include "webaccessgamepad.h"
#include "webaccessrun.h"
#include "gamepadrole.h"

#include "chaser.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "showtreecontroller.h"
#include "teachingsettings.h"

namespace
{
QString jsonText(const QJsonValue &v)
{
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
}

QJsonObject merged(QJsonObject base, const QJsonObject &extra)
{
    for (auto it = extra.begin(); it != extra.end(); ++it)
        base[it.key()] = it.value();
    return base;
}

/** The Run page's state as the console sends it in a show that runs:
 *  remote GO on, no PIN, a list on its first cue. */
QJsonObject liveRun(const QJsonObject &extra = QJsonObject())
{
    return merged(QJsonObject{
        { "api", 1 }, { "remote", true }, { "pin", false }, { "mode", "LIVE" }, { "teaching", false },
        { "blind", false }, { "hasList", true }, { "production", "Tonight" }, { "list", "Act One" },
        { "running", true }, { "paused", false }, { "hasSub", false },
        { "now", QJsonObject{ { "num", "1" }, { "name", "Open" } } },
        { "next", QJsonObject{ { "num", "2" }, { "name", "Verse" } } },
        { "pad", WebAccessRun::padMap() },
    }, extra);
}

/** A Design state with a cue in the Show Tree's preview, three layers. */
QJsonObject designRun(const QJsonObject &extra = QJsonObject())
{
    const QJsonArray layers = { QJsonObject{ { "id", 4 }, { "name", "Towers" } },
                                QJsonObject{ { "id", 7 }, { "name", "USTs" } },
                                QJsonObject{ { "id", 9 }, { "name", "Backlight" } } };
    return liveRun(merged(QJsonObject{
        { "mode", "DESIGN" }, { "running", false },
        { "preview", QJsonObject{ { "num", "2" }, { "name", "Verse" }, { "layers", layers }, { "solo", -1 } } },
    }, extra));
}

/** The focus remote's state while a Venue focus runs on the console. */
QJsonObject activeFocus(const QJsonObject &extra = QJsonObject())
{
    return merged(QJsonObject{
        { "api", 1 }, { "remote", true }, { "pin", false }, { "mode", "DESIGN" }, { "teaching", false },
        { "blind", false }, { "coarse", 8.0 }, { "fine", 1.0 }, { "repeatDelay", 400 }, { "repeatInterval", 80 },
        { "labels", QJsonObject{ { "a", "Keep" }, { "b", "Revert" }, { "x", "Zero" }, { "y", "Scope" },
                                 { "lb", "Previous light" }, { "rb", "Next light" },
                                 { "lt", "Previous call" }, { "rt", "Next call" },
                                 { "dpad_up", "Zoom narrower" }, { "dpad_down", "Zoom wider" } } },
        { "active", true }, { "holding", false }, { "venue", "Main" }, { "scope", "fixture" },
        { "target", "DS centre" }, { "unsaved", 0 }, { "checked", 1 }, { "total", 2 },
        { "fixtures", QJsonArray{ QJsonObject{ { "id", 1 }, { "name", "Spot 1" } },
                                  QJsonObject{ { "id", 2 }, { "name", "Spot 2" } } } },
        { "fixture", QJsonObject{ { "id", 1 }, { "name", "Spot 1" }, { "index", 1 }, { "count", 2 },
                                  { "pan", 0.0 }, { "tilt", 0.0 } } },
    }, extra);
}

QStringList nudges(const QStringList &words)
{
    QStringList out;
    for (const QString &w : words)
        if (w.startsWith(QLatin1String("QLC+FOCUS|nudge|")))
            out << w;
    return out;
}
}

/*****************************************************************************
 * The stand-in browser
 *****************************************************************************/

void PageScript_Test::init()
{
    m_errors.clear();
    m_js = new QJSEngine(this);
    QFile stubs(QStringLiteral(":/pagestubs.js"));
    QVERIFY(stubs.open(QIODevice::ReadOnly));
    js(QString::fromUtf8(stubs.readAll()));
    // The standard layout by button index, as the page's own table has it.
    QJsonArray buttons;
    for (GamepadRole::Role r : WebAccessGamepad::standardButtons())
        buttons.append(GamepadRole::key(r));
    js(QStringLiteral("var __BTN=") + jsonText(buttons) + QLatin1Char(';'));
}

void PageScript_Test::cleanup()
{
    delete m_js;
    m_js = nullptr;
    QVERIFY2(m_errors.isEmpty(), qPrintable(m_errors.join(QLatin1Char('\n'))));
}

QJSValue PageScript_Test::js(const QString &code)
{
    const QJSValue v = m_js->evaluate(code);
    if (v.isError())
        m_errors << QStringLiteral("%1 (line %2) in: %3")
                        .arg(v.toString(), v.property(QStringLiteral("lineNumber")).toString(), code.left(160));
    return v;
}

void PageScript_Test::loadPage(const QString &html)
{
    // The markup, then every script of the page in order, as a browser would.
    m_js->globalObject().setProperty(QStringLiteral("__html"), html);
    js(QStringLiteral("__loadMarkup(__html)"));
    static const QRegularExpression script(QStringLiteral("<script>(.*?)</script>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    int n = 0;
    for (auto it = script.globalMatch(html); it.hasNext(); ++n)
        js(it.next().captured(1));
    QVERIFY2(n >= 2, "the page has its own script and the gamepad's");
}

void PageScript_Test::hear(const QString &message)
{
    m_js->globalObject().setProperty(QStringLiteral("__msg"), message);
    js(QStringLiteral("__recv(__msg)"));
}

void PageScript_Test::runState(const QJsonObject &state)
{
    hear(QStringLiteral("RUN|state|") + jsonText(state));
}

void PageScript_Test::focusState(const QJsonObject &state)
{
    hear(QStringLiteral("FOCUS|state|") + jsonText(state));
}

QStringList PageScript_Test::sent()
{
    return js(QStringLiteral("__take()")).toVariant().toStringList();
}

QString PageScript_Test::text(const QString &id)
{
    return js(QStringLiteral("document.getElementById('%1').text()").arg(id)).toString();
}

bool PageScript_Test::shown(const QString &id)
{
    return js(QStringLiteral("document.getElementById('%1').style.display").arg(id)).toString() != QLatin1String("none");
}

bool PageScript_Test::enabled(const QString &id)
{
    return !js(QStringLiteral("document.getElementById('%1').disabled").arg(id)).toBool();
}

bool PageScript_Test::click(const QString &id)
{
    return js(QStringLiteral("__click('%1')").arg(id)).toBool();
}

void PageScript_Test::pad(const QJsonObject &down, int ms, const QJsonArray &axes)
{
    js(QStringLiteral("__setPad(%1,%2);__advance(%3)")
           .arg(jsonText(down), axes.isEmpty() ? QStringLiteral("[0,0,0,0]") : jsonText(axes))
           .arg(ms));
}

/*****************************************************************************
 * Both pages
 *****************************************************************************/

void PageScript_Test::bothPagesLoadAndSayHello()
{
    loadPage(WebAccessRun::html());
    QCOMPARE(js(QStringLiteral("__ws().url")).toString(), QStringLiteral("ws://console.local:9999/qlcplusWS"));
    js(QStringLiteral("__open()"));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|hello") });
    QVERIFY(text(QStringLiteral("where")).contains(QStringLiteral("Connecting")));
    QCOMPARE(js(QStringLiteral("document.getElementById('dot').className")).toString(), QStringLiteral("on"));
    // Until the console says so, nothing can be pressed.
    QVERIFY(!enabled(QStringLiteral("go")));
    runState(liveRun());
    QCOMPARE(text(QStringLiteral("where")), QStringLiteral("Tonight · Act One"));
    QCOMPARE(text(QStringLiteral("nnum")), QStringLiteral("1"));
    QCOMPARE(text(QStringLiteral("xname")), QStringLiteral("Verse"));
    QVERIFY(enabled(QStringLiteral("go")));

    cleanup();
    init();
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    // hello, then its first round-trip ping
    const QStringList hello = sent();
    QCOMPARE(hello.value(0), QStringLiteral("QLC+FOCUS|hello"));
    QVERIFY(hello.value(1).startsWith(QStringLiteral("QLC+FOCUS|ping|")));
    focusState(activeFocus());
    QVERIFY(shown(QStringLiteral("main")));
    QVERIFY(!shown(QStringLiteral("off")));
    QCOMPARE(text(QStringLiteral("fname")), QStringLiteral("Spot 1"));
    // A focus that ends: the page says to start one on the console.
    focusState(activeFocus(QJsonObject{ { "active", false } }));
    QVERIFY(shown(QStringLiteral("off")));
    QVERIFY(!shown(QStringLiteral("main")));
}

void PageScript_Test::theyReconnectAndGiveThePinAgain()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(liveRun(QJsonObject{ { "pin", true } }));
    js(QStringLiteral("document.getElementById('pin').value='4321'"));
    QVERIFY(click(QStringLiteral("pinok")));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|pin|4321") });
    hear(QStringLiteral("RUN|pin|ok"));

    // The Wi-Fi drops: the page tries again, and gives the PIN it was given.
    js(QStringLiteral("__ws().close()"));
    QCOMPARE(js(QStringLiteral("document.getElementById('dot').className")).toString(), QString());
    js(QStringLiteral("__advance(600)"));
    QCOMPARE(js(QStringLiteral("__sockets.length")).toInt(), 2);
    js(QStringLiteral("__open()"));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+RUN|hello"), QStringLiteral("QLC+RUN|pin|4321") }));
}

/*****************************************************************************
 * The Run page
 *****************************************************************************/

void PageScript_Test::runButtonsSendTheirWords()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(liveRun());
    QVERIFY(click(QStringLiteral("go")));
    QVERIFY(click(QStringLiteral("back")));
    QVERIFY(click(QStringLiteral("hold")));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+RUN|go"), QStringLiteral("QLC+RUN|back"),
                                   QStringLiteral("QLC+RUN|pause") }));
    QCOMPARE(js(QStringLiteral("__vibrations.length")).toInt(), 1);

    // Paused: HOLD is RESUME; sub-cues: their row shows.
    runState(liveRun(QJsonObject{ { "paused", true }, { "hasSub", true } }));
    QCOMPARE(text(QStringLiteral("hold")), QStringLiteral("RESUME"));
    QVERIFY(shown(QStringLiteral("subrow")));
    QVERIFY(click(QStringLiteral("hold")));
    QVERIFY(click(QStringLiteral("subgo")));
    QVERIFY(click(QStringLiteral("subback")));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+RUN|resume"), QStringLiteral("QLC+RUN|subgo"),
                                   QStringLiteral("QLC+RUN|subback") }));

    // Stopped: HOLD has nothing to hold.
    runState(liveRun(QJsonObject{ { "running", false } }));
    QVERIFY(!enabled(QStringLiteral("hold")));
    QCOMPARE(text(QStringLiteral("nnum")), QStringLiteral("Stopped"));
}

void PageScript_Test::stopNeedsASecondTapInTwoSeconds()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(liveRun());
    QVERIFY(click(QStringLiteral("stop")));
    QCOMPARE(sent(), QStringList());
    QCOMPARE(text(QStringLiteral("stop")), QStringLiteral("TAP AGAIN TO STOP"));
    QVERIFY(click(QStringLiteral("stop")));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|stop") });

    // Too slow: the first tap is forgotten.
    QVERIFY(click(QStringLiteral("stop")));
    js(QStringLiteral("__advance(2100)"));
    QCOMPARE(text(QStringLiteral("stop")), QStringLiteral("STOP"));
    QVERIFY(click(QStringLiteral("stop")));
    QCOMPARE(sent(), QStringList());
}

void PageScript_Test::nothingIsSentWhileRemoteGoIsOff()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(designRun(QJsonObject{ { "remote", false } }));
    for (const char *id : { "go", "back", "stop", "hold", "blind", "solo", "solonext" })
        QVERIFY2(!enabled(QLatin1String(id)), id);
    QVERIFY(!shown(QStringLiteral("rigrow")));
    QVERIFY(text(QStringLiteral("msg")).contains(QStringLiteral("off in Show Settings")));
    // Not by the pad either.
    pad(QJsonObject{ { "lb", 1 } });
    pad(QJsonObject{ { "lb", 1 }, { "a", 1 } });
    pad(QJsonObject{ { "lb", 1 }, { "ls", 1 } });
    pad(QJsonObject());
    QCOMPARE(sent(), QStringList());

    // Turned on: the message goes, the buttons work.
    runState(designRun());
    QCOMPARE(text(QStringLiteral("msg")), QString());
    QVERIFY(enabled(QStringLiteral("go")));
}

void PageScript_Test::thePinIsAskedAndKeptForTheSession()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(designRun(QJsonObject{ { "pin", true } }));
    QCOMPARE(js(QStringLiteral("document.getElementById('lock').className")).toString(), QStringLiteral("show"));
    QVERIFY(!enabled(QStringLiteral("go")));
    QVERIFY(!enabled(QStringLiteral("blind")));
    js(QStringLiteral("document.getElementById('pin').value='1111'"));
    QVERIFY(click(QStringLiteral("pinok")));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|pin|1111") });
    hear(QStringLiteral("RUN|pin|bad"));
    QCOMPARE(text(QStringLiteral("pinerr")), QStringLiteral("Wrong PIN."));
    QVERIFY(js(QStringLiteral("sessionStorage.getItem('pin')")).isNull());

    // Enter in the box is the same as Unlock.
    js(QStringLiteral("document.getElementById('pin').value='2222'"));
    js(QStringLiteral("document.getElementById('pin').onkeydown({key:'Enter'})"));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|pin|2222") });
    hear(QStringLiteral("RUN|pin|ok"));
    QCOMPARE(js(QStringLiteral("document.getElementById('lock').className")).toString(), QString());
    QCOMPARE(js(QStringLiteral("sessionStorage.getItem('pin')")).toString(), QStringLiteral("2222"));
    QVERIFY(enabled(QStringLiteral("go")));
    QVERIFY(enabled(QStringLiteral("blind")));

    // The console forgot it (restarted): the page asks again.
    hear(QStringLiteral("RUN|ack|go|needpin"));
    QCOMPARE(js(QStringLiteral("document.getElementById('lock').className")).toString(), QStringLiteral("show"));
    hear(QStringLiteral("RUN|pin|locked"));
    QVERIFY(text(QStringLiteral("pinerr")).contains(QStringLiteral("Too many tries")));
}

void PageScript_Test::everyPadWordNeedsLBHeld()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(designRun());

    // The map is the console's (WebAccessRun::padMap()): every word in it.
    const QJsonObject map = WebAccessRun::padMap();
    for (const char *k : { "a", "b", "x", "y", "rb", "back", "dpad_left", "dpad_right", "ls" })
        QVERIFY2(map.contains(QLatin1String(k)), k);
    int checked = 0;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        const QJsonObject a = it.value().toObject();
        if (a.value(QStringLiteral("do")).toString() == QLatin1String("arm"))
            continue;
        const QString word = QStringLiteral("QLC+RUN|")
            + (a.contains(QStringLiteral("cmd")) ? a.value(QStringLiteral("cmd")).toString()
                                                : a.value(QStringLiteral("do")).toString());
        // Alone: nothing.
        pad(QJsonObject{ { it.key(), 1 } });
        pad(QJsonObject());
        QVERIFY2(sent().isEmpty(), qPrintable(it.key()));
        // With LB held: the word, once.
        pad(QJsonObject{ { "lb", 1 } });
        QVERIFY(shown(QStringLiteral("armed")));
        pad(QJsonObject{ { "lb", 1 }, { it.key(), 1 } }, 200);
        pad(QJsonObject());
        QVERIFY(!shown(QStringLiteral("armed")));
        QCOMPARE(sent(), QStringList{ word });
        ++checked;
    }
    QCOMPARE(checked, map.count() - 1);
    QVERIFY(js(QStringLiteral("__vibrations.length")).toInt() >= 1);     // LB + A buzzes

    // The labels in the controller test are the console's.
    pad(QJsonObject{ { "start", 1 } });
    pad(QJsonObject());
    QVERIFY(js(QStringLiteral("document.getElementById('qpadbtns').text()")).toString()
                .contains(map.value(QStringLiteral("ls")).toObject().value(QStringLiteral("label")).toString()));
}

void PageScript_Test::theControllerTestSendsNothing()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(liveRun());
    // Menu opens it (the page's button too).
    pad(QJsonObject{ { "start", 1 } });
    pad(QJsonObject());
    QCOMPARE(js(QStringLiteral("document.getElementById('qpadtest').className")).toString(), QStringLiteral("show"));
    QVERIFY(text(QStringLiteral("qpadid")).contains(QStringLiteral("28de")));
    // Everything lights its box; nothing goes to the console.
    pad(QJsonObject{ { "lb", 1 }, { "a", 1 } }, 100, QJsonArray{ 1, 0, 0, 0 });
    QCOMPARE(js(QStringLiteral("document.getElementById('qpadbtns').querySelectorAll('div')"
                               ".filter(function(d){return d.className==='qpb on'}).length")).toInt(), 2);
    pad(QJsonObject(), 50);
    QCOMPARE(sent(), QStringList());
    // A key the Deck's grips are bound to shows as the last key.
    js(QStringLiteral("__key('keydown','PageDown');__key('keyup','PageDown')"));
    QVERIFY(text(QStringLiteral("qpadkey")).contains(QStringLiteral("PageDown = RT")));
    // Menu again closes it; the page works again.
    pad(QJsonObject{ { "start", 1 } });
    pad(QJsonObject());
    QCOMPARE(js(QStringLiteral("document.getElementById('qpadtest').className")).toString(), QString());
    pad(QJsonObject{ { "lb", 1 } });
    pad(QJsonObject{ { "lb", 1 }, { "a", 1 } });
    pad(QJsonObject());
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|go") });
}

void PageScript_Test::acksSayWhyInPlainWords()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    runState(liveRun());
    const QList<QPair<QString, QString>> cases = {
        { QStringLiteral("RUN|ack|go|refused"), QStringLiteral("GO from the phone is off") },
        { QStringLiteral("RUN|ack|go|nothing"), QStringLiteral("Nothing to play.") },
        { QStringLiteral("RUN|ack|hit|design"), QStringLiteral("Hits fire in Operate") },
        { QStringLiteral("RUN|ack|blind|operate"), QStringLiteral("Blind and Solo are for Design") },
        { QStringLiteral("RUN|ack|solo|operate"), QStringLiteral("Blind and Solo are for Design") },
        { QStringLiteral("RUN|ack|solo|nothing"), QStringLiteral("Solo needs a cue playing in the Show Tree preview") },
    };
    for (const auto &c : cases)
    {
        hear(c.first);
        QVERIFY2(text(QStringLiteral("msg")).contains(c.second), qPrintable(c.first + " -> " + text("msg")));
    }
    hear(QStringLiteral("RUN|ack|go|ok"));
    QCOMPARE(text(QStringLiteral("msg")), QString());
}

void PageScript_Test::teachingShowsWhatItHolds()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    runState(liveRun());
    QVERIFY(!shown(QStringLiteral("teach")));
    QCOMPARE(text(QStringLiteral("held")), QString());

    // A Teaching show with the instructor's defaults: everything held, said.
    runState(liveRun(QJsonObject{ { "teaching", true },
                                  { "held", QJsonObject{ { "output", true }, { "showControl", "Teaching" },
                                                         { "last", "" } } } }));
    QVERIFY(shown(QStringLiteral("teach")));
    QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("real output held")));
    QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("show control out held (Teaching)")));
    // A GO's trigger held: the console's own words for it.
    runState(liveRun(QJsonObject{ { "teaching", true },
                                  { "held", QJsonObject{ { "output", true }, { "showControl", "Teaching" },
                                                         { "last", "held: Teaching (cue 2 not sent)" } } } }));
    QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("cue 2 not sent")));
    // Output let through by the instructor: only the show control line.
    runState(liveRun(QJsonObject{ { "teaching", true },
                                  { "held", QJsonObject{ { "output", false }, { "showControl", "" }, { "last", "" } } } }));
    QCOMPARE(text(QStringLiteral("held")), QStringLiteral("Teaching"));
    // The page itself never changes what Teaching holds: GO stays a GO.
    QVERIFY(click(QStringLiteral("go")));
    QCOMPARE(sent().last(), QStringLiteral("QLC+RUN|go"));
}

void PageScript_Test::blindAndSoloFollowTheState()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    // Operate: the console's Blind is a view of its screens, Solo the tree's:
    // neither is offered.
    runState(liveRun());
    QVERIFY(!shown(QStringLiteral("rigrow")));
    QVERIFY(!shown(QStringLiteral("blindchip")));

    // Design, nothing in the preview: Blind yes, Solo greyed.
    runState(designRun(QJsonObject{ { "preview", QJsonValue() } }));
    QVERIFY(shown(QStringLiteral("rigrow")));
    QVERIFY(enabled(QStringLiteral("blind")));
    QVERIFY(!enabled(QStringLiteral("solo")));
    QVERIFY(!enabled(QStringLiteral("solonext")));
    QVERIFY(js(QStringLiteral("document.getElementById('solo').title")).toString().contains(QStringLiteral("preview")));

    // Blind on, USTs soloed: both lit, the chip in the header, the layer named.
    QJsonObject pv = designRun().value(QStringLiteral("preview")).toObject();
    pv[QStringLiteral("solo")] = 7;
    runState(designRun(QJsonObject{ { "blind", true }, { "preview", pv } }));
    QCOMPARE(js(QStringLiteral("document.getElementById('blind').className")).toString(), QStringLiteral("on"));
    QCOMPARE(text(QStringLiteral("blind")), QStringLiteral("BLIND ON"));
    QVERIFY(shown(QStringLiteral("blindchip")));
    QCOMPARE(js(QStringLiteral("document.getElementById('solo').className")).toString(), QStringLiteral("on"));
    QCOMPARE(text(QStringLiteral("solo")), QStringLiteral("SOLO: USTs"));
    QVERIFY(enabled(QStringLiteral("soloprev")));

    // Solo off: the whole cue again.
    pv[QStringLiteral("solo")] = -1;
    runState(designRun(QJsonObject{ { "preview", pv } }));
    QCOMPARE(text(QStringLiteral("solo")), QStringLiteral("SOLO"));
    QCOMPARE(js(QStringLiteral("document.getElementById('solo').className")).toString(), QString());

    // Blind and Solo need no cue list (a look being built).
    runState(designRun(QJsonObject{ { "hasList", false } }));
    QVERIFY(!enabled(QStringLiteral("go")));
    QVERIFY(enabled(QStringLiteral("blind")));
    QVERIFY(enabled(QStringLiteral("solo")));
}

void PageScript_Test::blindAndSoloButtonsSendTheirWords()
{
    loadPage(WebAccessRun::html());
    js(QStringLiteral("__open()"));
    sent();
    runState(designRun());
    QVERIFY(click(QStringLiteral("blind")));
    QVERIFY(click(QStringLiteral("solo")));
    QVERIFY(click(QStringLiteral("solonext")));
    QVERIFY(click(QStringLiteral("soloprev")));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+RUN|blind|toggle"), QStringLiteral("QLC+RUN|solo|toggle"),
                                   QStringLiteral("QLC+RUN|solo|next"), QStringLiteral("QLC+RUN|solo|prev") }));
    // The D-pad walks the layers (with LB), as the buttons do.
    pad(QJsonObject{ { "lb", 1 } });
    pad(QJsonObject{ { "lb", 1 }, { "dpad_right", 1 } });
    pad(QJsonObject{ { "lb", 1 } });
    pad(QJsonObject{ { "lb", 1 }, { "dpad_right", 1 } });
    pad(QJsonObject());
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+RUN|solo|next"), QStringLiteral("QLC+RUN|solo|next") }));
}

void PageScript_Test::theConsolesOwnStateDrawsOnThePage()
{
    /* The real state, as WebAccessRun writes it, on the real page: the two
       sides agree on every name. */
    Doc *doc = new Doc(this);
    {
        ShowTreeController *st = doc->showTree();
        const quint32 prod = st->addProduction(QStringLiteral("Tonight"));
        const quint32 listId = st->addCueList(prod, QStringLiteral("Act One"), ShowTree::ListMode::NonTracking);
        st->addCue(listId, -1, QStringLiteral("Open"));
        st->addCue(listId, 0, QStringLiteral("Verse"));
        Chaser *list = qobject_cast<Chaser *>(doc->function(st->cueList(listId)->chaser));
        QVERIFY(list != nullptr);
        doc->showControl()->setMainCueList(list->id());
        doc->operateSettings()->setRemoteGo(true);
        doc->teachingSettings()->setEnabled(true);

        WebAccessRun run(doc);
        loadPage(WebAccessRun::html());
        js(QStringLiteral("__open()"));
        sent();
        runState(QJsonDocument::fromJson(run.stateJson()).object());
        QCOMPARE(text(QStringLiteral("where")), QStringLiteral("Tonight · Act One"));
        QCOMPARE(text(QStringLiteral("xname")), QStringLiteral("Open"));
        QVERIFY(enabled(QStringLiteral("go")));
        QVERIFY(shown(QStringLiteral("teach")));
        QVERIFY2(text(QStringLiteral("held")).contains(QStringLiteral("real output held")), qPrintable(text("held")));
        QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("show control out held (Teaching)")));

        // Blind from the page, as the console does it in Design: on the page next.
        QVERIFY(shown(QStringLiteral("rigrow")));
        QVERIFY(click(QStringLiteral("blind")));
        QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|blind|toggle") });
        QCOMPARE(run.blind(QStringLiteral("toggle"), false), WebAccessRun::Done);
        runState(QJsonDocument::fromJson(run.stateJson()).object());
        QVERIFY(shown(QStringLiteral("blindchip")));
        QCOMPARE(text(QStringLiteral("blind")), QStringLiteral("BLIND ON"));
        doc->inputOutputMap()->setOutputInhibited(false);

        // The pad map in the state is the one the page acts on.
        pad(QJsonObject{ { "lb", 1 } });
        pad(QJsonObject{ { "lb", 1 }, { "ls", 1 } });
        pad(QJsonObject());
        QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+RUN|blind|toggle") });
    }
    delete doc;
}

/*****************************************************************************
 * The focus remote
 *****************************************************************************/

void PageScript_Test::focusButtonsAndKeysPressTheirRoles()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    sent();
    focusState(activeFocus());
    // The buttons carry the console's words; one with none is greyed.
    QCOMPARE(js(QStringLiteral("document.getElementById('keep').querySelector('small').textContent")).toString(),
             QStringLiteral("Keep"));
    QVERIFY(js(QStringLiteral("document.querySelectorAll('button[data-role]').filter(function(b){"
                              "return b.getAttribute('data-role')==='ls'})[0].disabled")).toBool());
    QVERIFY(click(QStringLiteral("keep")));
    // The pad: a press, once, however long it is held.
    pad(QJsonObject{ { "rb", 1 } }, 300);
    pad(QJsonObject());
    // The keys a Steam Input layout binds the back grips to.
    js(QStringLiteral("__key('keydown','PageDown');__key('keyup','PageDown');"
                      "__key('keydown','[');__key('keyup','[')"));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+FOCUS|press|a"), QStringLiteral("QLC+FOCUS|press|rb"),
                                   QStringLiteral("QLC+FOCUS|press|rt"), QStringLiteral("QLC+FOCUS|press|lb") }));
    // A call and a light, by a tap on the list.
    focusState(activeFocus(QJsonObject{ { "calls", QJsonArray{ QJsonObject{ { "name", "Front" }, { "count", 2 }, { "done", 0 } },
                                                               QJsonObject{ { "name", "Backs" }, { "count", 2 }, { "done", 0 } } } },
                                        { "call", 0 } }));
    js(QStringLiteral("document.getElementById('calls').childNodes[1].onclick();"
                      "document.getElementById('fixtures').childNodes[1].onclick()"));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+FOCUS|call|1"), QStringLiteral("QLC+FOCUS|fixture|2") }));
    // Menu is the controller test, never "leave focus".
    pad(QJsonObject{ { "start", 1 } });
    pad(QJsonObject());
    QCOMPARE(sent(), QStringList());
}

void PageScript_Test::aTriggerPressesPastHalfAndLetsGoUnderAThird()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    sent();
    for (double v : { 0.6, 0.4, 0.2, 0.4, 0.6, 0.0 })
        pad(QJsonObject{ { "lt", v } }, 40);
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+FOCUS|press|lt"), QStringLiteral("QLC+FOCUS|press|lt") }));
}

void PageScript_Test::aStickSendsMovesAtMost40ASecond()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    pad(QJsonObject(), 2500);      // the sticks' rest learned
    sent();
    // The left stick full right for a second: about 8 degrees of pan, coarse.
    pad(QJsonObject(), 1000, QJsonArray{ 1, 0, 0, 0 });
    const QStringList moves = nudges(sent());
    QVERIFY2(moves.count() >= 20 && moves.count() <= 41, qPrintable(QString::number(moves.count())));
    double pan = 0.0;
    for (const QString &m : moves)
    {
        pan += m.section(QLatin1Char('|'), 2, 2).toDouble();
        QCOMPARE(m.section(QLatin1Char('|'), 3, 3).toDouble(), 0.0);
    }
    QVERIFY2(pan > 7.5 && pan < 8.2, qPrintable(QString::number(pan)));
    // The right stick, up: fine, and tilt the other way round (up is minus).
    pad(QJsonObject(), 100);
    sent();
    pad(QJsonObject(), 1000, QJsonArray{ 0, 0, 0, -1 });
    double tilt = 0.0;
    for (const QString &m : nudges(sent()))
        tilt += m.section(QLatin1Char('|'), 3, 3).toDouble();
    QVERIFY2(tilt > 0.9 && tilt < 1.05, qPrintable(QString::number(tilt)));
    // Let go: nothing more.
    pad(QJsonObject(), 100);
    sent();
    pad(QJsonObject(), 500);
    QCOMPARE(nudges(sent()), QStringList());
}

void PageScript_Test::aStickInsideTheDeadZoneSendsNothing()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    pad(QJsonObject(), 2500);
    sent();
    // A handheld's drift: 5% of the travel (the dead zone is 6% of a half).
    pad(QJsonObject(), 1000, QJsonArray{ 0.1, -0.1, 0.11, 0.05 });
    QCOMPARE(nudges(sent()), QStringList());
    // A pad that rests off centre does not walk the light once its first
    // two seconds have learned where it rests.
    cleanup();
    init();
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    pad(QJsonObject(), 2500, QJsonArray{ 0.25, 0, 0, 0 });
    sent();
    pad(QJsonObject(), 1000, QJsonArray{ 0.25, 0, 0, 0 });
    QCOMPARE(nudges(sent()), QStringList());
}

void PageScript_Test::theDpadRepeatsWhileHeld()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    sent();
    // One press, then after 400 ms one every 80 ms.
    pad(QJsonObject{ { "dpad_up", 1 } }, 800);
    const QStringList held = sent();
    QVERIFY2(held.count() >= 5 && held.count() <= 8, qPrintable(held.join(QLatin1Char(' '))));
    for (const QString &w : held)
        QCOMPARE(w, QStringLiteral("QLC+FOCUS|press|dpad_up"));
    pad(QJsonObject(), 500);
    QCOMPARE(sent(), QStringList());
}

void PageScript_Test::arrowKeysNudgeAndTypingDoesNot()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    sent();
    js(QStringLiteral("__key('keydown','ArrowLeft');__key('keydown','ArrowUp',true)"));
    QCOMPARE(sent(), (QStringList{ QStringLiteral("QLC+FOCUS|nudge|-0.100|0.000"),
                                   QStringLiteral("QLC+FOCUS|nudge|0.000|1.000") }));
    // The step button: 0.1, 1, 0.01...
    QVERIFY(click(QStringLiteral("stepbtn")));
    QCOMPARE(text(QStringLiteral("stepbtn")), QStringLiteral("1°"));
    js(QStringLiteral("__key('keydown','ArrowRight')"));
    QCOMPARE(sent(), QStringList{ QStringLiteral("QLC+FOCUS|nudge|1.000|0.000") });
    // Typing the PIN is typing, not a control.
    js(QStringLiteral("__key('keydown','Enter',false,{tagName:'INPUT'})"));
    QCOMPARE(sent(), QStringList());
    // Remote control off: nothing at all.
    focusState(activeFocus(QJsonObject{ { "remote", false } }));
    js(QStringLiteral("__key('keydown','ArrowLeft');__key('keydown','Enter');__key('keyup','Enter')"));
    QCOMPARE(sent(), QStringList());
    QVERIFY(text(QStringLiteral("msg")).contains(QStringLiteral("off in Show Settings")));
}

void PageScript_Test::focusShowsBlindAndTeaching()
{
    loadPage(WebAccessFocus::html());
    js(QStringLiteral("__open()"));
    focusState(activeFocus());
    QVERIFY(!shown(QStringLiteral("blindchip")));
    QVERIFY(!shown(QStringLiteral("teach")));
    QCOMPARE(text(QStringLiteral("held")), QString());
    focusState(activeFocus(QJsonObject{ { "blind", true }, { "teaching", true },
                                        { "held", QJsonObject{ { "output", true } } } }));
    QVERIFY(shown(QStringLiteral("blindchip")));
    QVERIFY(shown(QStringLiteral("teach")));
    QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("rig is muted")));
    QVERIFY(text(QStringLiteral("held")).contains(QStringLiteral("Teaching holds real output")));
}

QTEST_MAIN(PageScript_Test)
