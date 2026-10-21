/*
  Q Light Controller Plus
  usedby.cpp

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

#include <QCoreApplication>

#include "virtualconsole.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "vcbutton.h"
#include "vcframe.h"
#include "usedby.h"
#include "doc.h"

static QString tr(const char *text, const char *comment = nullptr, int n = -1)
{
    return QCoreApplication::translate("UsedBy", text, comment, n);
}

QList<VCButton *> UsedBy::vcButtons(const QSet<quint32> &functionIds)
{
    QList<VCButton *> out;
    VirtualConsole *vc = VirtualConsole::instance();
    if (vc == nullptr || vc->contents() == nullptr || functionIds.isEmpty())
        return out;
    for (VCButton *b : vc->contents()->findChildren<VCButton *>())
        if (functionIds.contains(b->function()))
            out << b;
    return out;
}

QStringList UsedBy::vcLines(const QSet<quint32> &functionIds)
{
    QStringList out;
    for (const VCButton *b : vcButtons(functionIds))
        out << tr("VC button “%1”").arg(b->caption());
    return out;
}

static QSet<quint32> functionsOf(const QList<PaletteOps::Use> &uses)
{
    QSet<quint32> out;
    for (const PaletteOps::Use &u : uses)
        if (u.function != 0xFFFFFFFF)
            out.insert(u.function);
    return out;
}

QStringList UsedBy::paletteLines(Doc *doc, quint32 paletteId)
{
    const QList<PaletteOps::Use> uses = doc->paletteOps()->usage(paletteId);
    QStringList out;
    for (const PaletteOps::Use &u : uses)
        out << u.label;
    return out + vcLines(functionsOf(uses));
}

QString UsedBy::paletteSummary(Doc *doc, quint32 paletteId)
{
    const QList<PaletteOps::Use> uses = doc->paletteOps()->usage(paletteId);
    QString text = PaletteOps::usageSummary(uses);
    const int vc = int(vcButtons(functionsOf(uses)).count());
    if (vc > 0)
        text += QStringLiteral(" · ") + tr("%n VC button(s)", "", vc);
    return text;
}

QString UsedBy::deletePalettesQuestion(Doc *doc, const QList<quint32> &paletteIds)
{
    QStringList names;
    QString used;
    bool anyScene = false, anySlot = false, anyTree = false;
    for (quint32 pid : paletteIds)
    {
        const QLCPalette *p = doc->palette(pid);
        if (p == nullptr)
            continue;
        names << p->name();
        const QList<PaletteOps::Use> uses = doc->paletteOps()->usage(pid);
        const QStringList lines = paletteLines(doc, pid);
        if (lines.isEmpty())
            continue;
        for (const PaletteOps::Use &u : uses)
        {
            anyScene |= u.kind == PaletteOps::Use::Scene;
            anySlot |= u.kind == PaletteOps::Use::EffectSlot;
            anyTree |= u.kind == PaletteOps::Use::Cue || u.kind == PaletteOps::Use::Look
                       || u.kind == PaletteOps::Use::Reusable;
        }
        QStringList shown = lines.mid(0, 12);
        if (lines.count() > shown.count())
            shown << tr("… and %1 more").arg(lines.count() - shown.count());
        used += QStringLiteral("\n\n") + tr("“%1” is used in %n place(s):", "", int(lines.count())).arg(p->name())
                + QStringLiteral("\n  • ") + shown.join(QStringLiteral("\n  • "));
    }
    if (names.isEmpty())
        return QString();
    QString msg = names.count() > 1 ? tr("Delete %1 palettes?\n\n%2").arg(names.count()).arg(names.join(QStringLiteral(", ")))
                                    : tr("Delete palette “%1”?").arg(names.first());
    if (used.isEmpty())
        return msg + QStringLiteral("\n\n") + tr("Nothing uses it.");
    msg += used + QStringLiteral("\n");
    if (anyTree)
        msg += QStringLiteral("\n") + tr("Those cues, layers and looks lose it: a look with nothing else "
                                         "left is cleared.");
    if (anyScene)
        msg += QStringLiteral("\n") + tr("Those scenes lose that look; their other looks and values stay.");
    if (anySlot)
        msg += QStringLiteral("\n") + tr("Those effect looks lose that slot's palette and use their look's "
                                         "colours instead.");
    msg += QStringLiteral("\n\n") + tr("Undo brings it back, and every use with it.");
    return msg;
}
