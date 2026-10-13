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

#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QHBoxLayout>
#include <QSettings>
#include <QTimer>
#include <cmath>

#include "speedmastersfooter.h"
#include "speedmasterspanel.h"
#include "speedmasters.h"
#include "doc.h"

/* How long the level bar stays after the rate stops moving, and its fade. */
static const qint64 kLevelHoldMs = 1200;
static const qint64 kLevelFadeMs = 500;

SpeedMasterButton::SpeedMasterButton(QWidget *parent)
    : QToolButton(parent)
{
    setAutoRaise(false);
    setFocusPolicy(Qt::NoFocus);
}

void SpeedMasterButton::setState(BeatStyle style, double phase, double level, double levelAlpha)
{
    const bool fired = style == Blink && phase < 0.15;
    const int barPx = style == Bar ? qRound(qBound(0.0, phase, 1.0) * (width() - 4)) : 0;
    level = qBound(0.0, level, 1.0);
    const int alpha = qMin(qRound(qBound(0.0, levelAlpha, 1.0) * 8) * 32, 255);   // a few steps, not every frame
    const bool sameLevel = alpha == 0 ? m_levelAlpha == 0
                         : (alpha == m_levelAlpha && qRound(level * width()) == qRound(m_level * width()));
    // Only when what's drawn changes: a bar moves a pixel at a time.
    if (fired == m_fired && barPx == m_barPx && sameLevel)
        return;
    m_fired = fired;
    m_barPx = barPx;
    m_level = level;
    m_levelAlpha = alpha;
    update();
}

int SpeedMasterButton::levelWidth() const
{
    // From the button's width now: the text (and so the width) changes
    // with the tempo the rate is moving.
    return m_levelAlpha > 0 ? qRound(m_level * (width() - 4)) : 0;
}

void SpeedMasterButton::paintEvent(QPaintEvent *)
{
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    const QString text = opt.text;
    opt.text.clear();
    p.drawComplexControl(QStyle::CC_ToolButton, opt);

    const QRect inner = rect().adjusted(2, 2, -2, -2);
    if (m_fired)
        p.fillRect(inner, QColor(0xf5, 0xa6, 0x23));                    // the beat
    if (m_barPx > 0)                                                    // the beat, across it
        p.fillRect(QRect(inner.left(), inner.bottom() - 3, m_barPx, 4), QColor(0xf5, 0xa6, 0x23));
    if (levelWidth() > 0)
        p.fillRect(QRect(inner.left(), inner.top(), levelWidth(), inner.height()),
                   QColor(0x43, 0xa0, 0x47, m_levelAlpha * 3 / 4));   // the level, while moved
    p.setPen(m_fired ? QColor(Qt::black) : palette().color(QPalette::ButtonText));
    p.drawText(rect(), Qt::AlignCenter, text);
}

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
        SpeedMasterButton *b = new SpeedMasterButton(this);
        connect(b, &QToolButton::clicked, this, [this, i]() {
            SpeedMastersPanel::open(m_doc, window(), i);
        });
        lay->addWidget(b);
        m_buttons << b;
        m_lastRate << m_doc->speedMasters()->rate(i);
        m_rateMovedAt << -1;
    }
    m_clock.start();
    connect(m_doc->speedMasters(), &SpeedMasters::countChanged, this, [this](int) { applyCount(); });
    applyCount();
    reloadBeatOptions();

    // The beat light and the tempo: a follower's tempo moves with what it
    // follows, and a tap moves an own one, so read them as the panel does.
    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &SpeedMastersFooter::tick);
    m_timer->start();
    tick();
}

SpeedMasterButton *SpeedMastersFooter::button(int index) const
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

SpeedMasterButton::BeatStyle SpeedMastersFooter::beatStyle()
{
    return SpeedMasterButton::BeatStyle(
        qBound(0, QSettings().value(QStringLiteral("workspace/speedFooterBeat"), int(SpeedMasterButton::Blink)).toInt(), 2));
}

void SpeedMastersFooter::setBeatStyle(SpeedMasterButton::BeatStyle style)
{
    QSettings().setValue(QStringLiteral("workspace/speedFooterBeat"), int(style));
}

SpeedMastersFooter::BeatWhen SpeedMastersFooter::beatWhen()
{
    return BeatWhen(qBound(0, QSettings().value(QStringLiteral("workspace/speedFooterBeatWhen"), int(Always)).toInt(), 1));
}

void SpeedMastersFooter::setBeatWhen(BeatWhen when)
{
    QSettings().setValue(QStringLiteral("workspace/speedFooterBeatWhen"), int(when));
}

void SpeedMastersFooter::applyCount()
{
    // Only the masters something is on: an unused one neither shows nor
    // flashes ("If the masters are not used .. they should not fire or be
    // visible").
    m_used = m_doc->speedMasters()->usedMask();
    m_playing = m_doc->speedMasters()->playingMask();
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons.at(i)->setVisible((m_used >> i) & 1u);
}

void SpeedMastersFooter::tick()
{
    if (!isVisible())
        return;
    // What's used changes with any edit anywhere (a look's Speed, a chase
    // step's): look again twice a second rather than chase every signal.
    if (++m_ticks % 16 == 0)
    {
        applyCount();
        m_style = beatStyle();
        m_when = beatWhen();
    }
    SpeedMasters *sm = m_doc->speedMasters();
    for (int i = 0; i < sm->count() && i < m_buttons.size(); ++i)
    {
        if (!((m_used >> i) & 1u))
            continue;
        SpeedMasterButton *b = m_buttons.at(i);
        const bool paused = sm->isPaused(i);
        const double pos = sm->beatPosition(i);
        const bool compact = sm->count() > 8;
        QString name = sm->name(i);
        if (compact && name == tr("Speed %1").arg(i + 1))
            name = tr("S%1").arg(i + 1);            // unnamed: its number
        const QString text = buttonText(name, sm->bpm(i), paused, compact ? 4 : 10);
        // Only on a change: a label that changes width every frame would
        // jiggle the footer.
        if (b->text() != text)
        {
            b->setText(text);
            b->setToolTip(tr("%1: %2 BPM. Click for the speed masters.")
                          .arg(sm->name(i)).arg(sm->bpm(i), 0, 'f', 1));
        }
        // The level (the rate, 0..200 %) shows only while it's being moved,
        // then fades: the blink is the beat.
        const qint64 now = m_clock.elapsed();
        const double rate = sm->rate(i);
        if (rate != m_lastRate.at(i))
        {
            m_lastRate[i] = rate;
            m_rateMovedAt[i] = now;
        }
        double levelAlpha = 0;
        if (m_rateMovedAt.at(i) >= 0)
        {
            const qint64 since = now - m_rateMovedAt.at(i);
            levelAlpha = since < kLevelHoldMs ? 1.0
                       : since < kLevelHoldMs + kLevelFadeMs ? 1.0 - double(since - kLevelHoldMs) / kLevelFadeMs
                       : 0.0;
            if (levelAlpha == 0)
                m_rateMovedAt[i] = -1;
        }
        // The beat, as the View menu says: in its style, always or only
        // while something on this master plays; none while paused.
        SpeedMasterButton::BeatStyle style = paused ? SpeedMasterButton::NoBeat : m_style;
        if (m_when == WhilePlaying && !((m_playing >> i) & 1u))
            style = SpeedMasterButton::NoBeat;
        b->setState(style, pos - std::floor(pos), rate / 2.0, levelAlpha);
    }
}
