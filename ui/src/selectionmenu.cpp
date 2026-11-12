/*
  Q Light Controller Plus
  selectionmenu.cpp

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

#include <QPointer>
#include <QAction>
#include <QMenu>

#include "selectionmenu.h"
#include "programmingmanager.h"
#include "showtreeview.h"
#include "selectionshape.h"
#include "workbench.h"
#include "doc.h"

QString SelectionMenu::title()
{
    return tr("Selection");
}

QString SelectionMenu::newLayerText()
{
    return tr("New Layer from Selection");
}

QString SelectionMenu::addToLayerText()
{
    return tr("Add to Current Layer");
}

Workbench *SelectionMenu::currentWorkbench()
{
    // The Show Tree's, while a cue is open in it and it shows...
    if (ShowTreeView *tree = ShowTreeView::instance())
    {
        Workbench *docked = tree->dockedWorkbench();
        if (docked != nullptr && docked->isVisible())
            return docked;
    }
    // ...else Build's, in front or not.
    if (ProgrammingManager *pm = ProgrammingManager::instance())
        return pm->workbench();
    return nullptr;
}

QMenu *SelectionMenu::addTo(QMenu *menu, Doc *doc, const QList<quint32> &fixtures,
                            const QList<QAction *> &extra, Workbench *bench)
{
    if (menu == nullptr)
        return nullptr;
    QMenu *sub = menu->addMenu(title());
    sub->setToolTipsVisible(true);
    const QList<quint32> ids = fixtures.isEmpty() && doc != nullptr ? doc->programmerSelection() : fixtures;
    QPointer<Workbench> wb(bench != nullptr ? bench : currentWorkbench());
    QPointer<Doc> d(doc);

    auto layerAction = [&](const QString &text, const QString &why,
                           const std::function<bool(Workbench *)> &run) {
        QAction *a = sub->addAction(text);
        a->setEnabled(why.isEmpty());
        a->setToolTip(why.isEmpty() ? QString() : why);
        QObject::connect(a, &QAction::triggered, a, [wb, d, ids, run]() {
            if (wb.isNull())
                return;
            // What is acted on is the selection too.
            if (!d.isNull())
                d->setProgrammerSelection(ids);
            run(wb.data());
        });
        return a;
    };

    // What it is, in the show's groups: "Front Wash (6)", "SR Tower + 1 fixture".
    if (doc != nullptr && !ids.isEmpty())
    {
        QAction *what = sub->addAction(SelectionShape::label(doc, ids));
        what->setObjectName(QStringLiteral("selectionLabel"));
        what->setEnabled(false);
        sub->addSeparator();
    }
    const QString none = tr("Open a Show Tree cue or a scene in the Workbench first");
    QAction *newLayer = layerAction(newLayerText(), wb.isNull() ? none : wb->newLayerBlocker(ids),
                                    [ids](Workbench *w) { return w->newLayerFromFixtures(ids); });
    if (newLayer->isEnabled() && doc != nullptr)
    {
        const SelectionShape shape = SelectionShape::resolve(doc, ids);
        const int count = int(shape.groups.size()) + (shape.fixtures.isEmpty() ? 0 : 1);
        newLayer->setToolTip(count > 1 ? tr("%1 layers: one for each whole group, then one for the rest").arg(count)
                                       : tr("One layer on it"));
    }
    layerAction(addToLayerText(), wb.isNull() ? none : wb->addToLayerBlocker(ids),
                [ids](Workbench *w) { return w->addFixturesToCurrentLayer(ids); });

    if (!extra.isEmpty())
    {
        sub->addSeparator();
        for (QAction *a : extra)
            if (a != nullptr)
                sub->addAction(a);
    }
    return sub;
}
