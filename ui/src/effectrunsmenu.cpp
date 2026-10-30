/*
  Q Light Controller Plus
  effectrunsmenu.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QActionGroup>
#include <QInputDialog>
#include <QMenu>

#include "effectscriptrunner.h"
#include "effectrunsmenu.h"
#include "effectorigin.h"
#include "paletteops.h"
#include "qlcpalette.h"
#include "function.h"
#include "doc.h"

EffectRuns::Settings EffectRunsMenu::settings(const QLCPalette *p)
{
    if (p == nullptr || p->type() != QLCPalette::Effect)
        return EffectRuns::Settings();
    bool oneShot = false, release = false;
    if (!p->scriptPath().isEmpty())
        EffectOrigin::scriptRunDefaults(p->scriptPath(), &oneShot, &release);
    return EffectRuns::resolve(p->effectParamValues(), oneShot, release);
}

void EffectRunsMenu::apply(Doc *doc, const QList<quint32> &palettes, int runs, int release, int lengthMs)
{
    if (doc == nullptr)
        return;
    bool any = false;
    for (quint32 pid : palettes)
    {
        QLCPalette *p = doc->palette(pid);
        if (p == nullptr || p->type() != QLCPalette::Effect)
            continue;
        const EffectRuns::Settings now = settings(p);
        p->setEffectParamValue(EffectRuns::runsKey(), qBound(0, runs, EffectRuns::kMaxRuns));
        p->setEffectParamValue(EffectRuns::atEndKey(), (release < 0 ? now.release : release > 0) ? 1.0 : 0.0);
        p->setEffectParamValue(EffectRuns::lengthKey(), lengthMs < 0 ? now.lengthMs : lengthMs);
        if (doc->effectScriptRunner() != nullptr)
            doc->effectScriptRunner()->updateEffectParams(pid);
        any = true;
    }
    if (any)
        doc->setModified();
}

QMenu *EffectRunsMenu::add(QMenu *menu, Doc *doc, const QList<quint32> &palettes, QWidget *dialogParent)
{
    QList<quint32> effects;
    for (quint32 pid : palettes)
        if (QLCPalette *p = doc->palette(pid))
            if (p->type() == QLCPalette::Effect)
                effects << pid;
    if (effects.isEmpty())
        return nullptr;
    const EffectRuns::Settings now = settings(doc->palette(effects.first()));
    bool shared = false;
    for (quint32 pid : effects)
        shared = shared || doc->paletteOps()->isManaged(pid);

    QMenu *sub = menu->addMenu(now.loops() ? tr("Runs: Loop")
                               : now.runs == 1 ? tr("Runs: Once") : tr("Runs: %1 times").arg(now.runs));
    sub->setObjectName(QStringLiteral("effectRunsMenu"));
    sub->setToolTipsVisible(true);
    if (shared)
        sub->setToolTip(tr("This is a palette of the show: it runs this way everywhere it plays"));

    QActionGroup *runs = new QActionGroup(sub);
    auto addRun = [&](const QString &text, const char *name, bool on, const std::function<void()> &f) {
        QAction *a = sub->addAction(text);
        a->setObjectName(QLatin1String(name));
        a->setCheckable(true);
        a->setChecked(on);
        runs->addAction(a);
        QObject::connect(a, &QAction::triggered, sub, f);
    };
    addRun(tr("Loop"), "effectRunsLoop", now.loops(), [doc, effects]() { apply(doc, effects, 0); });
    addRun(tr("Once"), "effectRunsOnce", now.runs == 1, [doc, effects]() { apply(doc, effects, 1); });
    addRun(now.runs >= 2 ? tr("%1 times…").arg(now.runs) : tr("N times…"), "effectRunsTimes", now.runs >= 2,
           [doc, effects, now, dialogParent]() {
        bool ok = false;
        const int n = QInputDialog::getInt(dialogParent, tr("Runs"), tr("How many times:"),
                                           qMax(2, now.runs), 2, EffectRuns::kMaxRuns, 1, &ok);
        if (ok)
            apply(doc, effects, n);
    });

    sub->addSection(tr("At the end"));
    QActionGroup *end = new QActionGroup(sub);
    auto addEnd = [&](const QString &text, const char *name, bool release) {
        QAction *a = sub->addAction(text);
        a->setObjectName(QLatin1String(name));
        a->setCheckable(true);
        a->setChecked(now.release == release);
        a->setEnabled(!now.loops());
        end->addAction(a);
        QObject::connect(a, &QAction::triggered, sub, [doc, effects, now, release]() {
            apply(doc, effects, now.runs, release ? 1 : 0);
        });
    };
    addEnd(tr("Hold"), "effectAtEndHold", false);
    addEnd(tr("Release"), "effectAtEndRelease", true);
    QAction *len = sub->addAction(now.lengthMs > 0 ? tr("Length: %1 s…").arg(now.lengthMs / 1000.0)
                                                   : tr("Length: auto…"));
    len->setObjectName(QStringLiteral("effectRunLengthAction"));
    len->setToolTip(tr("One run's length: auto is the cue's time"));
    len->setEnabled(!now.loops());
    QObject::connect(len, &QAction::triggered, sub, [doc, effects, now, dialogParent]() {
        bool ok = false;
        const double s = QInputDialog::getDouble(dialogParent, tr("Length"),
                                                 tr("One run, in seconds (0 = auto: the cue's time):"),
                                                 now.lengthMs / 1000.0, 0.0, 600.0, 1, &ok);
        if (ok)
            apply(doc, effects, now.runs, -1, int(s * 1000.0 + 0.5));
    });
    return sub;
}

QMenu *EffectRunsMenu::addForFunction(QMenu *menu, Doc *doc, Function *f)
{
    if (f == nullptr || (f->type() != Function::EFXType && f->type() != Function::RGBMatrixType))
        return nullptr;
    const Function::RunOrder order = f->runOrder();
    const bool once = order == Function::SingleShot;
    QMenu *sub = menu->addMenu(once ? tr("Runs: Once") : tr("Runs: Loop"));
    sub->setObjectName(QStringLiteral("effectRunsMenu"));
    sub->setToolTipsVisible(true);
    QActionGroup *g = new QActionGroup(sub);
    const quint32 id = f->id();
    auto set = [doc, id](Function::RunOrder to) {
        Function *fn = doc->function(id);
        if (fn == nullptr || fn->runOrder() == to)
            return;
        doc->paletteOps()->functionStep(tr("%1 runs %2").arg(fn->name(), to == Function::SingleShot
                                                                          ? tr("once") : tr("in a loop")),
                                        { id }, [fn, to](QList<quint32> &) {
            fn->setRunOrder(to);
            return true;
        });
    };
    QAction *loop = sub->addAction(order == Function::PingPong ? tr("Loop (ping pong)")
                                   : order == Function::Random ? tr("Loop (random)") : tr("Loop"));
    loop->setObjectName(QStringLiteral("effectRunsLoop"));
    loop->setCheckable(true);
    loop->setChecked(!once);
    g->addAction(loop);
    QObject::connect(loop, &QAction::triggered, sub, [set, order]() {
        if (order == Function::SingleShot)
            set(Function::Loop);
    });
    QAction *one = sub->addAction(tr("Once"));
    one->setObjectName(QStringLiteral("effectRunsOnce"));
    one->setCheckable(true);
    one->setChecked(once);
    one->setToolTip(tr("Its Run order Single shot: it plays once and stops"));
    g->addAction(one);
    QObject::connect(one, &QAction::triggered, sub, [set]() { set(Function::SingleShot); });
    return sub;
}
