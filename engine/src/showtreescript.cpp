/*
  Q Light Controller Plus
  showtreescript.cpp

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

/* The calling script (0.5): a production keeps its script or lyrics as
   plain text, and cues placed on it by id. An id survives a renumber and a
   move; a cue that is deleted just stops being found, so its marker is
   gone (and an undo of the delete brings it back). Every edit is a tree
   step like any other. */

#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>

#include "showtreecontroller.h"

using namespace ShowTree;

static bool cueInProduction(const Production &p, quint32 cueId)
{
    for (const CueList &l : p.lists)
        if (l.indexOf(cueId) >= 0)
            return true;
    return false;
}

CallingScript ShowTreeController::callingScript(quint32 productionId) const
{
    const Production *p = production(productionId);
    if (p == nullptr)
        return CallingScript();
    CallingScript out = p->script;
    out.anchors.clear();
    for (const ScriptAnchor &a : p->script.anchors)
        if (cueInProduction(*p, a.cue))
            out.anchors << a;
    return out;
}

static int shifted(int offset, int prefix, int oldLen, int newLen, int suffix)
{
    if (offset < 0)
        return offset;
    if (offset <= prefix)
        return offset;
    if (offset >= oldLen - suffix)
        return qMin(newLen, offset + (newLen - oldLen));
    return prefix;                      // inside what was replaced
}

bool ShowTreeController::setScriptText(quint32 productionId, const QString &textIn, const QString &title)
{
    const Production *have = production(productionId);
    if (have == nullptr)
        return false;
    QString text = textIn;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QString newTitle = title.isNull() ? have->script.title : title;
    if (text == have->script.text && newTitle == have->script.title)
        return false;

    const QString old = have->script.text;
    int prefix = 0;
    const int minLen = qMin(old.length(), text.length());
    while (prefix < minLen && old.at(prefix) == text.at(prefix))
        ++prefix;
    int suffix = 0;
    while (suffix < minLen - prefix
           && old.at(old.length() - 1 - suffix) == text.at(text.length() - 1 - suffix))
        ++suffix;

    return edit(tr("Calling script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                QList<ScriptAnchor> kept;
                for (ScriptAnchor a : p.script.anchors)
                {
                    if (!cueInProduction(p, a.cue))
                        continue;
                    a.go = shifted(a.go, prefix, old.length(), text.length(), suffix);
                    a.standby = shifted(a.standby, prefix, old.length(), text.length(), suffix);
                    kept << a;
                }
                p.script.anchors = kept;
                p.script.text = text;
                p.script.title = newTitle;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::importScript(quint32 productionId, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
            *error = tr("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    const QString text = QString::fromUtf8(file.readAll());
    if (production(productionId) == nullptr)
    {
        if (error != nullptr)
            *error = tr("No such production.");
        return false;
    }
    const QString title = QFileInfo(path).fileName();
    if (setScriptText(productionId, text, title))
        return true;
    return production(productionId)->script.text == text;       // the same script again
}

bool ShowTreeController::anchorCue(quint32 productionId, quint32 cueId, int offset, ScriptMark mark)
{
    const Production *have = production(productionId);
    if (have == nullptr || offset < 0 || offset > have->script.text.length()
        || !cueInProduction(*have, cueId))
        return false;
    const ScriptAnchor *now = have->script.anchor(cueId);
    if (now != nullptr && (mark == ScriptMark::Go ? now->go : now->standby) == offset)
        return false;
    return edit(tr("Place cue on the script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                for (ScriptAnchor &a : p.script.anchors)
                    if (a.cue == cueId)
                    {
                        (mark == ScriptMark::Go ? a.go : a.standby) = offset;
                        return true;
                    }
                ScriptAnchor a;
                a.cue = cueId;
                (mark == ScriptMark::Go ? a.go : a.standby) = offset;
                p.script.anchors << a;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::unanchorCue(quint32 productionId, quint32 cueId, ScriptMark mark, bool both)
{
    const Production *have = production(productionId);
    const ScriptAnchor *now = have == nullptr ? nullptr : have->script.anchor(cueId);
    if (now == nullptr)
        return false;
    return edit(tr("Take cue off the script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
                for (int i = 0; i < p.script.anchors.count(); ++i)
                    if (p.script.anchors.at(i).cue == cueId)
                    {
                        ScriptAnchor &a = p.script.anchors[i];
                        if (both)
                            a.go = a.standby = -1;
                        else
                            (mark == ScriptMark::Go ? a.go : a.standby) = -1;
                        if (a.go < 0 && a.standby < 0)
                            p.script.anchors.removeAt(i);
                        return true;
                    }
        return false;
    });
}

bool ShowTreeController::clearScript(quint32 productionId)
{
    const Production *have = production(productionId);
    if (have == nullptr || have->script.isEmpty())
        return false;
    return edit(tr("Clear the calling script"), [&](QList<Production> &ps) {
        for (Production &p : ps)
            if (p.id == productionId)
            {
                p.script = CallingScript();
                return true;
            }
        return false;
    });
}
