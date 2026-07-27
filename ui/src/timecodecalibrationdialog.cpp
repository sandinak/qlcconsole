/*
  Q Light Controller Plus
  timecodecalibrationdialog.cpp

  Copyright (c) Massimo Callegari

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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QTimer>
#include <QMap>

#include "timecodecalibrationdialog.h"
#include "timecodesource.h"
#include "show.h"
#include "doc.h"

// Format an absolute ms position as SMPTE-ish hh:mm:ss:ff (30 fps assumed for
// the frame field — the exact fps only affects the cosmetic ff digits here).
static QString fmtCode(qint64 ms)
{
    const bool neg = ms < 0;
    if (neg)
        ms = -ms;
    const uint totalSec = uint(ms / 1000);
    const int hh = totalSec / 3600;
    const int mm = (totalSec % 3600) / 60;
    const int ss = totalSec % 60;
    const int ff = int((ms % 1000) * 30 / 1000);
    return QString("%1%2:%3:%4:%5").arg(neg ? "-" : "")
            .arg(hh, 2, 10, QChar('0')).arg(mm, 2, 10, QChar('0'))
            .arg(ss, 2, 10, QChar('0')).arg(ff, 2, 10, QChar('0'));
}

// Short mm:ss for the reference dropdown labels.
static QString fmtClock(qint64 ms)
{
    const uint totalSec = uint(qMax<qint64>(0, ms) / 1000);
    return QString("%1:%2").arg(totalSec / 60, 2, 10, QChar('0'))
                           .arg(totalSec % 60, 2, 10, QChar('0'));
}

TimecodeCalibrationDialog::TimecodeCalibrationDialog(Doc *doc, Show *show, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_show(show)
    , m_suggested(0)
{
    setWindowTitle(tr("Calibrate Timecode Offset — %1")
                   .arg(m_show != NULL ? m_show->name() : tr("(no show)")));
    setModal(false);
    resize(460, 560);

    m_suggested = (m_show != NULL) ? qint64(m_show->timecodeOffset()) : 0;

    QVBoxLayout *lay = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr(
        "Roll the show under MTC, then TAP in time with the music when it reaches "
        "the chosen reference. Repeat over a few rolls — averaging cancels your "
        "reaction time and yields a repeatable offset. Apply, then trim ±10 ms by "
        "feel while watching the rig."), this);
    intro->setWordWrap(true);
    lay->addWidget(intro);

    // --- Reference to align to ---
    QHBoxLayout *refRow = new QHBoxLayout;
    refRow->addWidget(new QLabel(tr("Align to:"), this));
    m_refCombo = new QComboBox(this);
    m_refCombo->addItem(tr("Timeline start (00:00)"), QVariant(qlonglong(0)));
    if (m_show != NULL)
    {
        const QMap<quint32, ShowMarker> markers = m_show->markers();
        QMapIterator<quint32, ShowMarker> it(markers);
        while (it.hasNext())
        {
            it.next();
            const QString lbl = tr("%1  (%2)")
                    .arg(it.value().label.isEmpty() ? tr("Section") : it.value().label)
                    .arg(fmtClock(it.key()));
            m_refCombo->addItem(lbl, QVariant(qlonglong(it.key())));
        }
    }
    refRow->addWidget(m_refCombo, 1);
    lay->addLayout(refRow);

    // --- Live feed readout ---
    m_liveLabel = new QLabel(this);
    m_liveLabel->setStyleSheet("font-family: monospace;");
    lay->addWidget(m_liveLabel);
    m_healthLabel = new QLabel(this);
    lay->addWidget(m_healthLabel);

    // --- The TAP button ---
    m_tapBtn = new QPushButton(tr("TAP  (space)"), this);
    m_tapBtn->setMinimumHeight(64);
    QFont bf = m_tapBtn->font();
    bf.setPointSize(bf.pointSize() + 4);
    bf.setBold(true);
    m_tapBtn->setFont(bf);
    m_tapBtn->setAutoDefault(false);
    m_tapBtn->setDefault(false);
    lay->addWidget(m_tapBtn);

    // --- Captured taps ---
    m_tapList = new QListWidget(this);
    lay->addWidget(m_tapList, 1);

    m_statsLabel = new QLabel(this);
    lay->addWidget(m_statsLabel);

    // --- Suggested offset + nudge ---
    QHBoxLayout *sugRow = new QHBoxLayout;
    QPushButton *minus = new QPushButton(tr("− 10 ms"), this);
    QPushButton *plus  = new QPushButton(tr("+ 10 ms"), this);
    minus->setProperty("nudge", -10);
    plus->setProperty("nudge", 10);
    minus->setAutoDefault(false);
    plus->setAutoDefault(false);
    m_suggestLabel = new QLabel(this);
    m_suggestLabel->setAlignment(Qt::AlignCenter);
    QFont sf = m_suggestLabel->font();
    sf.setBold(true);
    m_suggestLabel->setFont(sf);
    sugRow->addWidget(minus);
    sugRow->addWidget(m_suggestLabel, 1);
    sugRow->addWidget(plus);
    lay->addLayout(sugRow);

    m_currentOffsetLabel = new QLabel(this);
    lay->addWidget(m_currentOffsetLabel);

    // --- Buttons ---
    QDialogButtonBox *bb = new QDialogButtonBox(this);
    QPushButton *clearBtn = bb->addButton(tr("Clear taps"), QDialogButtonBox::ResetRole);
    m_applyBtn = bb->addButton(tr("Apply offset"), QDialogButtonBox::ApplyRole);
    bb->addButton(QDialogButtonBox::Close);
    clearBtn->setAutoDefault(false);
    m_applyBtn->setAutoDefault(false);
    lay->addWidget(bb);

    connect(m_tapBtn, &QPushButton::clicked, this, &TimecodeCalibrationDialog::slotTap);
    connect(minus, &QPushButton::clicked, this, &TimecodeCalibrationDialog::slotNudge);
    connect(plus,  &QPushButton::clicked, this, &TimecodeCalibrationDialog::slotNudge);
    connect(clearBtn, &QPushButton::clicked, this, &TimecodeCalibrationDialog::slotClear);
    connect(m_applyBtn, &QPushButton::clicked, this, &TimecodeCalibrationDialog::slotApply);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_refCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotReferenceChanged()));

    // Live readout: a light poll covers both fresh positions and the flip to
    // "holding" without wiring several engine signals.
    m_liveTimer = new QTimer(this);
    m_liveTimer->setInterval(150);
    connect(m_liveTimer, &QTimer::timeout, this, &TimecodeCalibrationDialog::slotLiveRefresh);
    m_liveTimer->start();

    refreshStats();
    slotLiveRefresh();
}

void TimecodeCalibrationDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space)
    {
        slotTap();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

qint64 TimecodeCalibrationDialog::referenceMs() const
{
    return m_refCombo->currentData().toLongLong();
}

void TimecodeCalibrationDialog::slotReferenceChanged()
{
    // Reference changed → old taps were measured against a different anchor and
    // no longer combine. Clear so the operator re-taps against the new one.
    if (m_offsets.isEmpty() == false)
    {
        m_offsets.clear();
        m_tapList->clear();
        refreshStats();
    }
}

void TimecodeCalibrationDialog::slotTap()
{
    TimecodeSource *tc = (m_doc != NULL) ? m_doc->timecodeSource() : NULL;
    if (tc == NULL)
        return;
    if (tc->isRunning() == false)
    {
        m_statsLabel->setText(tr("⚠ No timecode rolling — start the show's MTC source, then tap."));
        m_statsLabel->setStyleSheet("color:#c0392b;");
        return;
    }

    const qint64 code = qint64(tc->positionMs());
    const qint64 off = code - referenceMs();
    m_offsets << off;

    QListWidgetItem *row = new QListWidgetItem(
        tr("#%1   code %2   →   offset %3 ms")
            .arg(m_offsets.size()).arg(fmtCode(code))
            .arg(off >= 0 ? QString("+%1").arg(off) : QString::number(off)));
    // Flag taps taken against a shaky feed so they can be culled.
    if (tc->jitterMs() > 120.0 || qAbs(tc->rateEstimate() - 1.0) > 0.15)
    {
        row->setForeground(QColor("#e67e22"));
        row->setToolTip(tr("Feed was unstable at this tap — consider discarding."));
    }
    m_tapList->addItem(row);
    m_tapList->scrollToBottom();

    refreshStats();
}

void TimecodeCalibrationDialog::slotClear()
{
    m_offsets.clear();
    m_tapList->clear();
    refreshStats();
}

void TimecodeCalibrationDialog::slotNudge()
{
    const int delta = sender() != NULL ? sender()->property("nudge").toInt() : 0;
    m_suggested += delta;
    refreshStats();
}

void TimecodeCalibrationDialog::slotApply()
{
    if (m_show == NULL)
        return;
    const quint32 applied = quint32(qMax<qint64>(0, m_suggested));
    m_show->setTimecodeOffset(applied);
    m_doc->setModified();
    m_currentOffsetLabel->setText(tr("Current show offset: %1 ms  (applied ✓)").arg(applied));
    m_currentOffsetLabel->setStyleSheet("color:#27ae60;");
}

void TimecodeCalibrationDialog::refreshStats()
{
    if (m_offsets.isEmpty() == false)
    {
        qint64 sum = 0, lo = m_offsets.first(), hi = m_offsets.first();
        foreach (qint64 v, m_offsets)
        {
            sum += v;
            lo = qMin(lo, v);
            hi = qMax(hi, v);
        }
        const qint64 mean = sum / m_offsets.size();
        m_suggested = mean;   // taps drive the suggestion; nudges adjust from here
        m_statsLabel->setStyleSheet(QString());
        m_statsLabel->setText(tr("%n tap(s) · mean %1 ms · spread %2 ms",
                                 "", m_offsets.size())
                              .arg(mean).arg(hi - lo));
    }
    else
    {
        m_statsLabel->setStyleSheet(QString());
        m_statsLabel->setText(tr("No taps yet — or nudge the current offset by feel."));
    }

    m_suggestLabel->setText(tr("Suggested offset:  %1 ms   (%2)")
                            .arg(m_suggested).arg(fmtCode(m_suggested)));

    if (m_show != NULL && m_currentOffsetLabel->styleSheet().contains("27ae60") == false)
        m_currentOffsetLabel->setText(tr("Current show offset: %1 ms")
                                      .arg(m_show->timecodeOffset()));
    if (m_applyBtn != NULL)
        m_applyBtn->setEnabled(m_show != NULL);
}

void TimecodeCalibrationDialog::slotLiveRefresh()
{
    TimecodeSource *tc = (m_doc != NULL) ? m_doc->timecodeSource() : NULL;
    if (tc == NULL)
    {
        m_liveLabel->setText(tr("Timecode now:  —"));
        m_healthLabel->setText(QString());
        return;
    }

    const bool running = tc->isRunning();
    m_liveLabel->setText(tr("Timecode now:  %1   %2")
                         .arg(fmtCode(qint64(tc->positionMs())))
                         .arg(running ? tr("● rolling") : tr("❚❚ holding")));

    if (tc->lastUniverse() < 0)
    {
        m_healthLabel->setText(tr("Feed: no source"));
        m_healthLabel->setStyleSheet("color:#7f8c8d;");
        m_tapBtn->setEnabled(false);
        return;
    }

    const double rate = tc->rateEstimate();
    const double jit = tc->jitterMs();
    QString verdict; QString colour;
    if (!running) { verdict = tr("held"); colour = "#f39c12"; }
    else if (qAbs(rate - 1.0) <= 0.05 && jit < 40.0) { verdict = tr("✓ healthy"); colour = "#27ae60"; }
    else if (qAbs(rate - 1.0) <= 0.15 && jit < 120.0) { verdict = tr("~ usable"); colour = "#f39c12"; }
    else { verdict = tr("⚠ unstable"); colour = "#c0392b"; }

    m_healthLabel->setText(tr("Feed: %1 · %2× · jitter %3 ms")
                           .arg(verdict).arg(rate, 0, 'f', 2).arg(qRound(jit)));
    m_healthLabel->setStyleSheet(QString("color:%1;").arg(colour));
    m_tapBtn->setEnabled(running);
}
