/*
  Q Light Controller Plus
  hitmap.cpp

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
#include <QPushButton>
#include <QMessageBox>
#include <QVariant>

#include "showtreecontroller.h"
#include "showstatus.h"
#include "vcbutton.h"
#include "hitmap.h"
#include "usedby.h"
#include "doc.h"

static QString tr(const char *text, const char *disambiguation = nullptr, int n = -1)
{
    return QCoreApplication::translate("HitMap", text, disambiguation, n);
}

static HitMap::Asker s_asker;

static QString hitName(const ShowTreeController *st, quint32 hit)
{
    const ShowTree::Cue *c = st->cue(hit);
    if (c == nullptr || !st->isLook(hit))
        return tr("(deleted)");
    return c->name.isEmpty() ? tr("(unnamed)") : c->name;
}

static QString samePlaceWord(int samePlace)
{
    return samePlace == VCButton::StepThrough ? tr("step through")
           : samePlace == VCButton::Together ? tr("together")
                                             : tr("not chosen yet: it fires the first");
}

QList<HitMap::Row> HitMap::rows(Doc *doc, const QList<quint32> &hits, int samePlace)
{
    QList<Row> out;
    const ShowTreeController *st = doc->showTree();
    for (quint32 h : hits)
    {
        Row r;
        r.hit = h;
        r.name = hitName(st, h);
        if (!st->isLook(h))
        {
            r.note = tr("deleted: Remove it");
            out << r;
            continue;
        }
        r.when = st->hitScope(h) == ShowTree::HitScope::Global ? tr("Always")
                                                               : tr("While %1").arg(st->hitPlace(h));
        QStringList notes;
        QStringList over, shares;
        for (quint32 o : hits)
        {
            if (o == h)
                continue;
            if (st->hitOverrides(h, o))
                over << hitName(st, o);
            else if (st->sameHitPlace(h, o))
                shares << hitName(st, o);
        }
        if (!over.isEmpty())
            notes << tr("↳ overrides %1").arg(over.join(QStringLiteral(", ")));
        if (!shares.isEmpty())
            notes << tr("shares its place with %1: %2").arg(shares.join(QStringLiteral(", ")),
                                                            samePlaceWord(samePlace));
        r.note = notes.join(QStringLiteral("; "));
        out << r;
    }
    return out;
}

QString HitMap::sentence(Doc *doc, const QString &caption, const QList<quint32> &hits, int samePlace)
{
    const ShowTreeController *st = doc->showTree();
    QList<quint32> live;
    for (quint32 h : hits)
        if (st->isLook(h))
            live << h;
    const QString who = caption.trimmed().isEmpty() ? tr("This button") : caption.trimmed();
    if (live.isEmpty())
        return tr("%1 fires no hit.").arg(who);
    // The widest first: what it fires unless something narrower is armed.
    int widest = 99;
    for (quint32 h : live)
        widest = qMin(widest, int(st->hitScope(h)));
    QStringList base, except, also;
    for (quint32 h : live)
    {
        const QString what = QStringLiteral("%1 %2").arg(hitName(st, h), st->hitPlace(h));
        if (int(st->hitScope(h)) == widest)
        {
            if (!base.contains(what))
                base << what;
            continue;
        }
        QStringList over;
        for (quint32 o : live)
            if (o != h && st->hitOverrides(h, o))
                over << hitName(st, o);
        if (over.isEmpty())
            also << what;
        else
            except << tr("%1 (overrides %2)").arg(what, over.join(QStringLiteral(", ")));
    }
    QString text = tr("%1 fires %2").arg(who, base.join(tr(" or ")));
    if (!except.isEmpty())
        text += tr(", except %1").arg(except.join(tr(" and ")));
    if (!also.isEmpty())
        text += tr(", and %1").arg(also.join(tr(" and ")));
    // Pairs sharing a place.
    QList<quint32> done;
    for (quint32 h : live)
    {
        if (done.contains(h))
            continue;
        QStringList names{ hitName(st, h) };
        for (quint32 o : live)
            if (o != h && st->sameHitPlace(h, o))
            {
                names << hitName(st, o);
                done << o;
            }
        done << h;
        if (names.count() > 1)
            text += tr("; %1 hits live %2 (%3): %4").arg(QString::number(names.count()), st->hitPlace(h),
                                                        names.join(QStringLiteral(", ")), samePlaceWord(samePlace));
    }
    return text + QStringLiteral(".");
}

QStringList HitMap::buttonNames(quint32 hitId)
{
    QStringList out;
    for (const VCButton *b : UsedBy::allVcButtons())
        if (b->hits().contains(hitId))
            out << (b->caption().trimmed().isEmpty() ? tr("button %1").arg(b->id()) : b->caption().trimmed());
    return out;
}

bool HitMap::overridesOnAButton(Doc *doc, quint32 hitId)
{
    const ShowTreeController *st = doc->showTree();
    for (const VCButton *b : UsedBy::allVcButtons())
        if (b->hits().contains(hitId))
            for (quint32 o : b->hits())
                if (o != hitId && st->hitOverrides(hitId, o))
                    return true;
    return false;
}

void HitMap::refreshChecks(Doc *doc)
{
    const ShowTreeController *st = doc->showTree();
    QStringList items, groups;
    QVariantList data;
    bool warn = false;
    for (const VCButton *b : UsedBy::allVcButtons())
    {
        const QList<quint32> hits = b->hits();
        if (hits.isEmpty())
            continue;
        const QString name = b->caption().trimmed().isEmpty() ? tr("button %1").arg(b->id()) : b->caption().trimmed();
        QList<quint32> paired;
        for (quint32 h : hits)
        {
            if (!st->isLook(h))
            {
                items << tr("%1 holds a hit that was deleted").arg(name);
                groups << tr("Dead hits");
                data << b->id();
                warn = true;
                continue;
            }
            for (quint32 o : hits)
            {
                if (o == h || !st->isLook(o))
                    continue;
                if (st->hitOverrides(h, o))
                {
                    items << tr("%1: %2 (%3) overrides %4 (%5)")
                                 .arg(name, hitName(st, h), st->hitPlace(h), hitName(st, o), st->hitPlace(o));
                    groups << tr("Overrides");
                    data << b->id();
                }
                else if (st->sameHitPlace(h, o) && !paired.contains(o))
                {
                    items << tr("%1: %2 and %3 share a place (%4): %5")
                                 .arg(name, hitName(st, h), hitName(st, o), st->hitPlace(h),
                                      samePlaceWord(b->samePlace()));
                    groups << tr("Sharing a place");
                    data << b->id();
                    paired << h;
                    warn = warn || b->samePlace() == VCButton::SamePlaceUnset;
                }
            }
        }
    }
    ShowStatus *status = ShowStatus::instance();
    if (items.isEmpty())
    {
        status->clearStatus(QStringLiteral("hits.places"));
        return;
    }
    status->setStatus(QStringLiteral("hits.places"), warn ? ShowStatus::Warning : ShowStatus::Ok,
                      tr("%n hit button note(s): overrides, shared places, dead hits", "", int(items.count())),
                      tr("A VC button fires its armed hit with the narrowest place; where hits override or "
                         "share a place it says which. A deleted hit does nothing: remove it in the "
                         "button's Hits."),
                      items, data, groups);
}

bool HitMap::askToAdd(Doc *doc, QList<quint32> &hits, int &samePlace, const QList<quint32> &hitIds, QWidget *parent)
{
    const ShowTreeController *st = doc->showTree();
    bool any = false;
    for (quint32 h : hitIds)
    {
        if (!st->isLook(h) || hits.contains(h))
            continue;
        const ShowTree::Cue *c = st->cue(h);
        const QString name = c == nullptr || c->name.isEmpty() ? tr("this hit") : c->name;
        QList<quint32> same;
        quint32 wider = Function::invalidId(), narrower = Function::invalidId();
        for (quint32 o : hits)
        {
            if (st->sameHitPlace(o, h))
                same << o;
            else if (st->hitOverrides(h, o))
                wider = o;
            else if (st->hitOverrides(o, h))
                narrower = o;
        }
        if (!same.isEmpty())
        {
            // Q157 (2): no silent default.
            const QString other = st->cue(same.first()) == nullptr ? QString() : st->cue(same.first())->name;
            const int answer = ask(parent, tr("Two hits in one place"),
                tr("“%1” and “%2” both live %3. When this button is pressed there:")
                    .arg(other, name, st->hitPlace(h)),
                { tr("Step through"), tr("Together"), tr("Replace"), tr("Cancel") });
            if (answer == 0)
                samePlace = VCButton::StepThrough;
            else if (answer == 1)
                samePlace = VCButton::Together;
            else if (answer == 2)
                for (quint32 o : same)
                    hits.removeAll(o);
            else
                continue;
        }
        else if (wider != Function::invalidId() || narrower != Function::invalidId())
        {
            // Q157 (3): a narrower place overriding a wider one, asked once.
            const bool narrowerIsNew = wider != Function::invalidId();
            const quint32 w = narrowerIsNew ? wider : h;
            const quint32 n = narrowerIsNew ? h : narrower;
            const QString wn = st->cue(w) == nullptr ? QString() : st->cue(w)->name;
            const QString nn = st->cue(n) == nullptr ? QString() : st->cue(n)->name;
            const int answer = ask(parent, tr("Override there?"),
                tr("“%1” fires %2; “%3” fires instead %4. Override there?")
                    .arg(wn, st->hitPlace(w), nn, st->hitPlace(n)),
                { tr("Override"), tr("Cancel") });
            if (answer != 0)
                continue;
        }
        hits << h;
        any = true;
    }
    return any;
}

void HitMap::watch(Doc *doc)
{
    ShowTreeController *st = doc->showTree();
    if (st->property("hitMapWatched").toBool())
        return;
    st->setProperty("hitMapWatched", true);
    QObject::connect(st, &ShowTreeController::changed, st, [doc]() { refreshChecks(doc); });
}

int HitMap::ask(QWidget *parent, const QString &title, const QString &text, const QStringList &choices)
{
    if (s_asker)
        return s_asker(title, text, choices);
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    QList<QAbstractButton *> buttons;
    for (int i = 0; i < choices.count(); ++i)
        buttons << box.addButton(choices.at(i), i == choices.count() - 1 ? QMessageBox::RejectRole
                                                                         : QMessageBox::AcceptRole);
    if (!buttons.isEmpty())
        box.setEscapeButton(buttons.last());
    box.exec();
    const int at = int(buttons.indexOf(box.clickedButton()));
    return at < 0 ? int(choices.count()) - 1 : at;
}

void HitMap::setAsker(const Asker &asker)
{
    s_asker = asker;
}
