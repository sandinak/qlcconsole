/*
  Q Light Controller Plus
  speedmastersfooter.cpp

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
#include <QToolButton>
#include <QTimer>
#include <cmath>

#include "speedmastersfooter.h"
#include "speedmasterspanel.h"
#include "speedmasters.h"
#include "doc.h"

SpeedMastersFooter::SpeedMastersFooter(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("speedMastersFooter"));
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 4, 0);
    lay->setSpacing(2);
    for (int i = 0; i < SpeedMasters::MaxCount; ++i)
    {
        QToolButton *b = new QToolButton(this);
        b->setAutoRaise(false);
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("lit", false);
        connect(b, &QToolButton::clicked, this, [this, i]() {
            SpeedMastersPanel::open(m_doc, window(), i);
        });
        lay->addWidget(b);
        m_buttons << b;
    }
    connect(m_doc->speedMasters(), &SpeedMasters::countChanged, this, [this](int) { applyCount(); });
    applyCount();

    // The beat light and the tempo: a follower's tempo moves with what it
    // follows, and a tap moves an own one, so read them as the panel does.
    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &SpeedMastersFooter::tick);
    m_timer->start();
    tick();
}

QToolButton *SpeedMastersFooter::button(int index) const
{
    return m_buttons.value(index);
}

QString SpeedMastersFooter::buttonText(const QString &name, double bpm, bool paused, int maxName)
{
    QString n = name;
    if (n.size() > maxName)
        n = n.left(maxName - 1) + QChar(0x2026);
    return paused ? tr("%1 paused").arg(n)
                  : QStringLiteral("%1 %2").arg(n).arg(qRound(bpm));
}

void SpeedMastersFooter::applyCount()
{
    const int n = m_doc->speedMasters()->count();
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons.at(i)->setVisible(i < n);
}

void SpeedMastersFooter::tick()
{
    if (!isVisible())
        return;
    SpeedMasters *sm = m_doc->speedMasters();
    for (int i = 0; i < sm->count() && i < m_buttons.size(); ++i)
    {
        QToolButton *b = m_buttons.at(i);
        const bool paused = sm->isPaused(i);
        const double pos = sm->beatPosition(i);
        // Many masters: short names, so the footer doesn't push the
        // window's minimum width past the screen (the tooltip has it all).
        const bool compact = sm->count() > 8;
        QString name = sm->name(i);
        if (compact && name == tr("Speed %1").arg(i + 1))
            name = tr("S%1").arg(i + 1);            // unnamed: its number
        const QString text = buttonText(name, sm->bpm(i), paused, compact ? 4 : 10);
        // Only on a change: a stylesheet re-polishes, and a label that
        // changes width every frame would jiggle the footer.
        if (b->text() != text)
        {
            b->setText(text);
            b->setToolTip(tr("%1: %2 BPM. Click for the speed masters.")
                          .arg(sm->name(i)).arg(sm->bpm(i), 0, 'f', 1));
        }
        const bool on = !paused && pos - std::floor(pos) < 0.15;
        if (b->property("lit").toBool() != on || b->styleSheet().isEmpty())
        {
            b->setProperty("lit", on);
            b->setStyleSheet(on ? QStringLiteral("QToolButton { background:#f5a623; color:black; padding:0 4px; }")
                                : QStringLiteral("QToolButton { padding:0 4px; }"));
        }
    }
}
