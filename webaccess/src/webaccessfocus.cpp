/*
  Q Light Controller Plus - qlcconsole
  webaccessfocus.cpp

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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>

#include "webaccessfocus.h"
#include "webaccessgamepad.h"
#include "programmercontroller.h"
#include "controllerscheme.h"
#include "teachingsettings.h"
#include "focussession.h"
#include "showoperate.h"
#include "gamepadrole.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "doc.h"

WebAccessFocus::WebAccessFocus(Doc *doc, WebAccessRun *run, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_run(run)
{
    // A slow look for what has no signal (Design / Operate, the hold).
    m_poll.setInterval(500);
    connect(&m_poll, &QTimer::timeout, this, &WebAccessFocus::poll);
    /* A change goes out at once (the next turn of the event loop); a burst
       of them (a stick held) as one state every kStateMs at most. */
    m_soon.setSingleShot(true);
    connect(&m_soon, &QTimer::timeout, this, &WebAccessFocus::poll);
    m_apply.setSingleShot(true);
    connect(&m_apply, &QTimer::timeout, this, &WebAccessFocus::applyMove);

    if (FocusSession *s = session())
    {
        connect(s, &FocusSession::started, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::stopped, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::currentChanged, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::offsetChanged, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::referenceChanged, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::unsavedChanged, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::callChanged, this, &WebAccessFocus::schedule);
        connect(s, &FocusSession::scopeChanged, this, &WebAccessFocus::schedule);
    }
    connect(m_doc->operateSettings(), &ShowOperate::settingsChanged, this, &WebAccessFocus::schedule);
    connect(m_doc->inputOutputMap(), &InputOutputMap::outputInhibitedChanged, this, &WebAccessFocus::schedule);
    if (TeachingSettings *t = m_doc->teachingSettings())
        connect(t, &TeachingSettings::teachingChanged, this, &WebAccessFocus::schedule);
}

FocusSession *WebAccessFocus::session() const
{
    return m_doc->programmer() != nullptr ? m_doc->programmer()->focusSession() : nullptr;
}

void WebAccessFocus::setClients(int count)
{
    m_clients = count;
    if (m_clients > 0 && !m_poll.isActive())
    {
        m_last.clear();
        m_poll.start();
    }
    else if (m_clients <= 0)
    {
        m_poll.stop();
        m_soon.stop();
    }
}

void WebAccessFocus::schedule()
{
    if (m_clients <= 0 || m_soon.isActive())
        return;
    const qint64 since = m_lastEmit.isValid() ? m_lastEmit.elapsed() : kStateMs;
    m_soon.start(int(qMax<qint64>(0, kStateMs - since)));
}

void WebAccessFocus::poll()
{
    if (m_clients <= 0)
        return;
    const QByteArray now = stateJson();
    if (now == m_last)
        return;
    m_last = now;
    m_lastEmit.start();
    emit stateChanged(now);
}

WebAccessRun::Result WebAccessFocus::allowed(bool unlocked) const
{
    // The Run page's rules, then a session to work.
    if (!m_run->remoteAllowed())
        return WebAccessRun::Refused;
    if (m_run->pinRequired() && !unlocked)
        return WebAccessRun::NeedPin;
    FocusSession *s = session();
    if (s == nullptr || !s->isActive())
        return WebAccessRun::NothingToDo;
    return WebAccessRun::Done;
}

WebAccessRun::Result WebAccessFocus::nudge(double dPan, double dTilt, bool unlocked)
{
    const WebAccessRun::Result r = allowed(unlocked);
    if (r != WebAccessRun::Done)
        return r;
    if (!qIsFinite(dPan) || !qIsFinite(dTilt))
        return WebAccessRun::Unknown;
    m_pendPan += qBound(-kMaxStep, dPan, kMaxStep);
    m_pendTilt += qBound(-kMaxStep, dTilt, kMaxStep);
    if (m_apply.isActive())
        return WebAccessRun::Done;
    const qint64 since = m_lastApply.isValid() ? m_lastApply.elapsed() : kApplyMs;
    if (since >= kApplyMs)
        applyMove();
    else
        m_apply.start(int(kApplyMs - since));
    return WebAccessRun::Done;
}

void WebAccessFocus::applyMove()
{
    FocusSession *s = session();
    if (s == nullptr || !s->isActive())
    {
        m_pendPan = m_pendTilt = 0.0;
        return;
    }
    /* Offsets are held to hundredths (FocusSession::tick()): what rounds
       away waits for the next move instead of being lost. */
    const double stepPan = qRound(m_pendPan * 100.0) / 100.0;
    const double stepTilt = qRound(m_pendTilt * 100.0) / 100.0;
    if (stepPan == 0.0 && stepTilt == 0.0)
        return;
    m_pendPan -= stepPan;
    m_pendTilt -= stepTilt;
    m_lastApply.start();
    ++m_applied;
    s->nudge(float(stepPan), float(stepTilt));
}

WebAccessRun::Result WebAccessFocus::press(const QString &roleKey, bool unlocked)
{
    const GamepadRole::Role role = GamepadRole::fromProfile(roleKey, QString());
    const FocusSession::Control c = FocusSession::controlFor(role);
    // Exit is the console's question (unsaved corrections); the rest is the pad's.
    if (role == GamepadRole::None || c == FocusSession::NoControl || c == FocusSession::Exit)
        return WebAccessRun::Unknown;
    const WebAccessRun::Result r = allowed(unlocked);
    if (r != WebAccessRun::Done)
        return r;
    // A move still waiting lands on the light it was meant for.
    if (m_apply.isActive())
    {
        m_apply.stop();
        applyMove();
    }
    session()->apply(c);
    return WebAccessRun::Done;
}

WebAccessRun::Result WebAccessFocus::selectCall(int index, bool unlocked)
{
    const WebAccessRun::Result r = allowed(unlocked);
    if (r != WebAccessRun::Done)
        return r;
    FocusSession *s = session();
    if (!s->isRunningCalls() || index < 0 || index >= s->callCount())
        return WebAccessRun::NothingToDo;
    s->setCallIndex(index);
    s->flashSelection();
    return WebAccessRun::Done;
}

WebAccessRun::Result WebAccessFocus::selectFixture(quint32 fid, bool unlocked)
{
    const WebAccessRun::Result r = allowed(unlocked);
    if (r != WebAccessRun::Done)
        return r;
    FocusSession *s = session();
    if (s->isRunningCalls())
        return s->jumpTo(fid) ? WebAccessRun::Done : WebAccessRun::NothingToDo;
    const int at = s->order().indexOf(fid);
    if (at < 0)
        return WebAccessRun::NothingToDo;
    s->setIndex(at);
    s->flashSelection();
    return WebAccessRun::Done;
}

/*********************************************************************
 * State
 *********************************************************************/

static double hundredths(double v)
{
    return qRound(v * 100.0) / 100.0;
}

QByteArray WebAccessFocus::stateJson() const
{
    QJsonObject o;
    o[QStringLiteral("api")] = 1;
    o[QStringLiteral("remote")] = m_run->remoteAllowed();
    o[QStringLiteral("pin")] = m_run->pinRequired();
    o[QStringLiteral("mode")] = m_doc->mode() == Doc::Operate ? QStringLiteral("LIVE") : QStringLiteral("DESIGN");
    const TeachingSettings *teach = m_doc->teachingSettings();
    o[QStringLiteral("teaching")] = teach != nullptr && teach->enabled();
    // Blind mutes the rig: a focus then moves nothing you can see.
    o[QStringLiteral("blind")] = m_doc->inputOutputMap()->outputInhibited();
    if (teach != nullptr && teach->enabled())
    {
        QJsonObject held;
        held[QStringLiteral("output")] = teach->holdsOutput();
        o[QStringLiteral("held")] = held;
    }
    o[QStringLiteral("coarse")] = double(FocusSession::coarseRate());
    o[QStringLiteral("fine")] = double(FocusSession::fineRate());
    o[QStringLiteral("repeatDelay")] = int(FocusSession::kRepeatDelay * 1000);
    o[QStringLiteral("repeatInterval")] = int(FocusSession::kRepeatInterval * 1000);

    // What each control does here: the console's own words.
    FocusSession *s = session();
    const ControllerScheme::Tool tool = s != nullptr && s->isRunningCalls()
            ? ControllerScheme::FocusCallsTool : ControllerScheme::FocusTool;
    QJsonObject labels;
    for (GamepadRole::Role role : WebAccessGamepad::standardButtons())
    {
        const FocusSession::Control c = FocusSession::controlFor(role);
        if (c == FocusSession::NoControl || c == FocusSession::Exit)
            continue;
        QString text = ControllerScheme::label(tool, ControllerScheme::verbFor(role));
        // A hold is the console pad's; the page presses.
        const int hold = text.indexOf(QLatin1String(" (hold"));
        if (hold > 0)
            text.truncate(hold);
        labels[GamepadRole::key(role)] = text;
    }
    o[QStringLiteral("labels")] = labels;

    const bool active = s != nullptr && s->isActive();
    o[QStringLiteral("active")] = active;
    if (!active)
        return QJsonDocument(o).toJson(QJsonDocument::Compact);

    o[QStringLiteral("holding")] = m_doc->programmer()->isHolding() || s->isHolding();
    o[QStringLiteral("venue")] = s->savedVenue();
    o[QStringLiteral("scope")] = s->scope() == FocusSession::GroupScope ? QStringLiteral("group") : QStringLiteral("fixture");
    o[QStringLiteral("targetPage")] = s->isTargetPage();
    o[QStringLiteral("target")] = s->referenceName();
    o[QStringLiteral("unsaved")] = int(s->unsavedFixtures().size());
    o[QStringLiteral("checked")] = s->checkedCount();
    o[QStringLiteral("total")] = int(s->allFixtures().size());

    QList<quint32> inCall = s->order();
    if (s->isRunningCalls())
    {
        QJsonArray calls;
        for (int i = 0; i < s->callCount(); ++i)
        {
            const FocusSession::Call c = s->callAt(i);
            QJsonObject co;
            co[QStringLiteral("name")] = c.name;
            int done = 0;
            const QList<quint32> fids = s->callFixtures(i);
            for (quint32 fid : fids)
                done += s->isChecked(fid) && !s->isDirty(fid) ? 1 : 0;
            co[QStringLiteral("count")] = int(fids.size());
            co[QStringLiteral("done")] = done;
            calls.append(co);
        }
        o[QStringLiteral("calls")] = calls;
        o[QStringLiteral("call")] = s->callIndex();
        const FocusSession::Call cur = s->currentCall();
        o[QStringLiteral("callName")] = cur.name;
        o[QStringLiteral("note")] = cur.note;
        inCall = s->callFixtures(s->callIndex());
    }

    QJsonArray fixtures;
    for (quint32 fid : inCall)
    {
        QJsonObject fo;
        fo[QStringLiteral("id")] = int(fid);
        const Fixture *fx = m_doc->fixture(fid);
        fo[QStringLiteral("name")] = fx != nullptr ? fx->name() : QString();
        fo[QStringLiteral("unsaved")] = s->isDirty(fid);
        fo[QStringLiteral("checked")] = s->isChecked(fid);
        fixtures.append(fo);
    }
    o[QStringLiteral("fixtures")] = fixtures;

    const quint32 fid = s->currentFixture();
    if (fid != 0xFFFFFFFF)
    {
        QJsonObject fo;
        const Fixture *fx = m_doc->fixture(fid);
        fo[QStringLiteral("id")] = int(fid);
        fo[QStringLiteral("name")] = fx != nullptr ? fx->name() : QString();
        fo[QStringLiteral("index")] = int(inCall.indexOf(fid)) + 1;
        fo[QStringLiteral("count")] = int(inCall.size());
        const FocusSession::Offset live = s->liveOffset(fid);
        fo[QStringLiteral("pan")] = hundredths(live.pan);
        fo[QStringLiteral("tilt")] = hundredths(live.tilt);
        fo[QStringLiteral("delta")] = s->deltaText(fid);
        fo[QStringLiteral("unsaved")] = s->isDirty(fid);
        fo[QStringLiteral("checked")] = s->isChecked(fid);
        fo[QStringLiteral("fixed")] = s->isFixedLight(fid);
        double deg = 0.0, lo = 0.0, hi = 0.0;
        if (s->beamAngle(fid, deg, lo, hi))
            fo[QStringLiteral("beam")] = qRound(deg * 10.0) / 10.0;
        double level = 0.0;
        if (s->focusLevel(fid, level))
            fo[QStringLiteral("focus")] = qRound(level * 100.0);
        o[QStringLiteral("fixture")] = fo;
    }
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
