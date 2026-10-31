/*
  Q Light Controller Plus - qlcconsole
  webaccessrun.cpp

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

#include <QJsonDocument>
#include <QJsonObject>

#include "webaccessrun.h"
#include "showtreecontroller.h"
#include "teachingsettings.h"
#include "showcontrol.h"
#include "showoperate.h"
#include "chaser.h"
#include "doc.h"

using namespace ShowTree;

WebAccessRun::WebAccessRun(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &WebAccessRun::poll);
    // A command lands at once; the poll catches a list that steps on its own.
    connect(m_doc->showControl(), &ShowControl::commandHandled, this, &WebAccessRun::poll);
    connect(m_doc->operateSettings(), &ShowOperate::settingsChanged, this, &WebAccessRun::poll);
}

void WebAccessRun::setClients(int count)
{
    m_clients = count;
    if (m_clients > 0 && !m_timer.isActive())
    {
        m_last.clear();
        m_timer.start();
    }
    else if (m_clients <= 0)
    {
        m_timer.stop();
    }
}

void WebAccessRun::poll()
{
    if (m_clients <= 0)
        return;
    const QByteArray now = stateJson();
    if (now == m_last)
        return;
    m_last = now;
    emit stateChanged(now);
}

bool WebAccessRun::remoteAllowed() const
{
    return m_doc->operateSettings()->remoteGo();
}

bool WebAccessRun::pinRequired() const
{
    return m_doc->operateSettings()->hasPin();
}

WebAccessRun::PinResult WebAccessRun::checkPin(const QString &pin)
{
    if (m_locked)
    {
        if (m_lockout.elapsed() < kLockoutMs)
            return PinLocked;
        m_locked = false;
        m_failures = 0;
    }
    if (m_doc->operateSettings()->checkPin(pin))
    {
        m_failures = 0;
        return PinOk;
    }
    if (++m_failures >= kMaxTries)
    {
        m_locked = true;
        m_lockout.start();
        return PinLocked;
    }
    return PinBad;
}

WebAccessRun::Result WebAccessRun::command(const QString &command, bool unlocked)
{
    ShowControl::Request r;
    if (command == QLatin1String("go"))
        r.command = ShowControl::Go;
    else if (command == QLatin1String("back"))
        r.command = ShowControl::Back;
    else if (command == QLatin1String("stop"))
        r.command = ShowControl::Stop;
    else if (command == QLatin1String("subgo"))
        r.command = ShowControl::SubGo;
    else if (command == QLatin1String("subback"))
        r.command = ShowControl::SubBack;
    else
        return Unknown;

    if (!remoteAllowed())
        return Refused;
    if (pinRequired() && !unlocked)
        return NeedPin;

    // The main cue list, as the Run tab's default follow does.
    Chaser *ch = m_doc->showControl()->cueList(QString());
    if (ch == nullptr)
        return NothingToDo;
    r.chaser = ch->id();
    return m_doc->showControl()->run(r, ShowControl::RouteWeb) ? Done : NothingToDo;
}

/*********************************************************************
 * State
 *********************************************************************/

static QJsonObject cueObject(Doc *doc, Chaser *ch, quint32 listId, int step, int sub)
{
    ShowTreeController *st = doc->showTree();
    QJsonObject o;
    if (step < 0)
        return o;
    const quint32 cueId = st->cueOfStep(listId, step, sub);
    if (const CueList *l = st->listOfCue(cueId))
    {
        const int idx = l->indexOf(cueId);
        o[QStringLiteral("num")] = st->cueNumber(*l, idx);
        o[QStringLiteral("name")] = idx >= 0 && idx < l->cues.count() ? l->cues.at(idx).name : QString();
        return o;
    }
    if (sub >= 0)
    {
        o[QStringLiteral("num")] = ch->subCue(step, sub);
        o[QStringLiteral("name")] = QString();
        return o;
    }
    const QList<ChaserStep> steps = ch->steps();
    const Function *f = step < steps.count() ? doc->function(steps.at(step).fid) : nullptr;
    o[QStringLiteral("num")] = ch->stepCue(step);
    o[QStringLiteral("name")] = f == nullptr ? QString() : f->name();
    return o;
}

QByteArray WebAccessRun::stateJson() const
{
    ShowTreeController *st = m_doc->showTree();
    ShowControl *sc = m_doc->showControl();
    Chaser *ch = sc->cueList(QString());

    QJsonObject o;
    o[QStringLiteral("remote")] = remoteAllowed();
    o[QStringLiteral("pin")] = pinRequired();
    o[QStringLiteral("mode")] = m_doc->mode() == Doc::Operate ? QStringLiteral("LIVE") : QStringLiteral("DESIGN");
    o[QStringLiteral("teaching")] = m_doc->teachingSettings() != nullptr && m_doc->teachingSettings()->enabled();
    o[QStringLiteral("hasList")] = ch != nullptr;
    if (ch == nullptr)
        return QJsonDocument(o).toJson(QJsonDocument::Compact);

    quint32 lid = 0xFFFFFFFF;
    const quint32 pid = st->productionOfChaser(ch->id(), &lid);
    const Production *p = st->production(pid);
    const CueList *l = st->cueList(lid);
    o[QStringLiteral("production")] = p != nullptr ? p->name : QString();
    o[QStringLiteral("list")] = l != nullptr ? l->name : ch->name();

    const bool running = ch->isRunning();
    const int cur = running ? ch->currentStepIndex() : -1;
    const int curSub = running ? ch->currentSubCueIndex() : -1;
    const int standby = sc->standbyStep(ch);
    const bool loaded = sc->loadedStep(ch->id()) >= 0;
    const int loadedSub = loaded ? sc->loadedSubCue(ch->id()) : -1;
    const int subNext = running && ch->isSubGoStep(cur) ? ch->nextSubCueIndex() : -1;

    bool hasSub = false;
    for (int i = 0; i < ch->stepsCount() && !hasSub; ++i)
        hasSub = ch->isSubGoStep(i);

    o[QStringLiteral("running")] = running;
    o[QStringLiteral("paused")] = running && ch->isPaused();
    o[QStringLiteral("hasSub")] = hasSub;
    o[QStringLiteral("loaded")] = loaded;

    if (running)
    {
        // The sub-cue is the cue on now when there is one: its number is the full one.
        QJsonObject now = cueObject(m_doc, ch, lid, cur, -1);
        if (curSub >= 0)
            now[QStringLiteral("sub")] = cueObject(m_doc, ch, lid, cur, curSub);
        o[QStringLiteral("now")] = now;
    }

    if (standby >= 0)
    {
        int toSub = loaded ? loadedSub : -1;
        if (toSub < 0 && ch->isSubGoStep(standby))
            toSub = 0;
        o[QStringLiteral("next")] = cueObject(m_doc, ch, lid, standby, loaded ? toSub : -1);
    }
    if (subNext >= 0)
        o[QStringLiteral("subNext")] = cueObject(m_doc, ch, lid, cur, subNext);

    // The song section of the cue on now, else of the one next.
    quint32 sectionCue = running ? st->cueOfStep(lid, cur, -1) : st->cueOfStep(lid, standby, -1);
    Section sec;
    if (sectionCue != 0xFFFFFFFF && st->sectionOfCue(sectionCue, &sec))
        o[QStringLiteral("section")] = sectionTitle(sec);
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
