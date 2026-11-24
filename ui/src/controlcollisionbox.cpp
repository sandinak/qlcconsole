/*
  Q Light Controller Plus
  controlcollisionbox.cpp

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

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "controlcollisionbox.h"

ControlCollisionBox::ControlCollisionBox(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("controlCollision"));
    setFrameShape(QFrame::StyledPanel);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 6, 8, 6);
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("collisionText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    lay->addWidget(m_text);
    m_note = new QLabel(ControlOwners::moveNote(), this);
    m_note->setObjectName(QStringLiteral("collisionNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_note);
    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch(1);
    m_move = new QPushButton(tr("Move It Here"), this);
    m_move->setObjectName(QStringLiteral("collisionMove"));
    m_move->setToolTip(tr("Take the control from where it is now and give it to this one"));
    m_cancel = new QPushButton(tr("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("collisionCancel"));
    m_cancel->setToolTip(tr("Leave the control where it is"));
    // Return must never move a control: only a click or Space on the button.
    m_move->setAutoDefault(false);
    m_cancel->setAutoDefault(false);
    buttons->addWidget(m_move);
    buttons->addWidget(m_cancel);
    lay->addLayout(buttons);
    connect(m_move, &QPushButton::clicked, this, &ControlCollisionBox::moveRequested);
    connect(m_cancel, &QPushButton::clicked, this, [this]() {
        clear();
        emit cancelled();
    });
    hide();
}

void ControlCollisionBox::refuse(Doc *doc, quint32 universe, quint32 channel,
                                 const QList<ControlOwners::Owner> &owners)
{
    if (owners.isEmpty())
    {
        clear();
        return;
    }
    m_owners = owners;
    m_universe = universe;
    m_channel = channel;
    m_text->setText(ControlOwners::refusal(doc, universe, channel, owners));
    const bool movable = ControlOwners::canMove(owners);
    m_move->setVisible(movable);
    m_note->setVisible(movable);
    show();
}

void ControlCollisionBox::clear()
{
    m_owners.clear();
    m_text->clear();
    hide();
}

QString ControlCollisionBox::text() const
{
    return m_text->text();
}
