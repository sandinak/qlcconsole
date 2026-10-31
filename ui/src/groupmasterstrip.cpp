/*
  Q Light Controller Plus
  groupmasterstrip.cpp

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

#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>

#include "groupmasterstrip.h"
#include "groupmasters.h"
#include "fixturegroup.h"
#include "doc.h"

#define KPropGroup "groupMasterId"

GroupMasterStrip::GroupMasterStrip(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("groupMasterStrip"));
    setToolTip(tr("Group masters: each scales its fixture group's intensity, as the Grand Master does "
                  "everything's. Double-click a name for full. Fixture Manager ▸ a group ▸ Group master "
                  "adds one."));
    QHBoxLayout *row = new QHBoxLayout(this);
    row->setContentsMargins(4, 0, 0, 0);
    row->setSpacing(6);
    connect(doc->groupMasters(), &GroupMasters::changed, this, &GroupMasterStrip::rebuild);
    connect(doc->groupMasters(), &GroupMasters::levelChanged, this, &GroupMasterStrip::slotLevelChanged);
    rebuild();
}

void GroupMasterStrip::rebuild()
{
    QLayout *row = layout();
    while (QLayoutItem *item = row->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
    m_faders.clear();
    m_levels.clear();

    const QList<FixtureGroup *> groups = m_doc->groupMasters()->groups();
    for (FixtureGroup *grp : groups)
    {
        const quint32 id = grp->id();
        QWidget *col = new QWidget(this);
        col->setObjectName(QStringLiteral("groupMaster%1").arg(id));
        QVBoxLayout *v = new QVBoxLayout(col);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);

        QLabel *level = new QLabel(col);
        level->setAlignment(Qt::AlignHCenter);
        v->addWidget(level);

        QSlider *fader = new QSlider(Qt::Vertical, col);
        fader->setObjectName(QStringLiteral("groupMasterFader"));
        fader->setRange(0, UCHAR_MAX);
        fader->setProperty(KPropGroup, id);
        fader->setMinimumHeight(120);
        fader->setAccessibleName(tr("%1 master").arg(grp->name()));
        v->addWidget(fader, 1, Qt::AlignHCenter);
        connect(fader, &QSlider::valueChanged, this, [this, id](int value) {
            if (FixtureGroup *g = m_doc->fixtureGroup(id))
                g->setMasterLevel(uchar(value));
        });

        QLabel *name = new QLabel(grp->name(), col);
        name->setAlignment(Qt::AlignHCenter);
        name->setWordWrap(true);
        name->setMaximumWidth(90);
        name->setProperty(KPropGroup, id);
        name->setToolTip(tr("%1: double-click for full").arg(grp->name()));
        name->installEventFilter(this);
        v->addWidget(name);

        row->addWidget(col);
        m_faders.insert(id, fader);
        m_levels.insert(id, level);
        showLevel(id, grp->masterLevel());
    }
    setVisible(!groups.isEmpty());
}

void GroupMasterStrip::showLevel(quint32 groupId, uchar level)
{
    QSlider *fader = m_faders.value(groupId);
    QLabel *label = m_levels.value(groupId);
    if (fader == nullptr || label == nullptr)
        return;
    if (fader->value() != level && !fader->isSliderDown())
    {
        const QSignalBlocker block(fader);
        fader->setValue(level);
    }
    label->setText(QStringLiteral("%1%").arg(qRound(level * 100.0 / UCHAR_MAX)));
    // Below full reads as held back: the operator sees it from the GO button.
    label->setStyleSheet(level < UCHAR_MAX ? QStringLiteral("color: #d08a1c; font-weight: bold;")
                                           : QString());
}

void GroupMasterStrip::slotLevelChanged(quint32 groupId, uchar level)
{
    showLevel(groupId, level);
}

bool GroupMasterStrip::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::MouseButtonDblClick)
    {
        bool ok = false;
        const quint32 id = obj->property(KPropGroup).toUInt(&ok);
        if (FixtureGroup *g = ok ? m_doc->fixtureGroup(id) : nullptr)
        {
            g->setMasterLevel(UCHAR_MAX);
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}
