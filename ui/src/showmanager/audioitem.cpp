/*
  Q Light Controller Plus
  audioitem.cpp

  Copyright (C) Massimo Callegari

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

#include <QApplication>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <qmath.h>
#include <QDebug>
#include <QMenu>

#include "audioitem.h"
#include "trackitem.h"
#include "audiodecoder.h"
#include "audioplugincache.h"
#include "doc.h"

AudioItem::AudioItem(Audio *aud, ShowFunction *func)
    : ShowItem(func)
    , m_audio(aud)
    , m_waveMode(WaveformPeaks::Mixed)
{
    Q_ASSERT(aud != NULL);

    if (func->color().isValid())
        setColor(func->color());
    else
        setColor(ShowFunction::defaultColor(Function::AudioType));

    setIconResource(":/audio.png");

    if (func->duration() == 0)
        func->setDuration(aud->totalDuration());

    calculateWidth();
    connect(m_audio, SIGNAL(changed(quint32)),
            this, SLOT(slotAudioChanged(quint32)));
    connect(m_audio, SIGNAL(nameChanged(quint32)),
            this, SLOT(slotAudioChanged(quint32)));

    /* Waveform actions: hide / mixed / left / right / stereo */
    static const char *names[5] = { QT_TR_NOOP("Hide waveform"), QT_TR_NOOP("Waveform"),
                                    QT_TR_NOOP("Left channel"), QT_TR_NOOP("Right channel"),
                                    QT_TR_NOOP("Both channels") };
    for (int i = 0; i < 5; i++)
    {
        m_waveActions[i] = new QAction(tr(names[i]), this);
        m_waveActions[i]->setCheckable(true);
        m_waveActions[i]->setChecked(i - 1 == m_waveMode);
        connect(m_waveActions[i], &QAction::triggered, this, [this, i]() { setWaveformMode(i - 1); });
    }

    connect(WaveformService::instance(), &WaveformService::ready,
            this, &AudioItem::slotWaveformReady);
    requestWaveform();
}

void AudioItem::calculateWidth()
{
    int newWidth = 0;
    qint64 audio_duration = m_audio->totalDuration();

    if (audio_duration != 0)
        newWidth = ((50/(float)getTimeScale()) * (float)audio_duration) / 1000;
    else
        newWidth = 100;

    if (newWidth < (50 / m_timeScale))
        newWidth = 50 / m_timeScale;
    setWidth(newWidth);
}

void AudioItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    float timeScale = 50/(float)m_timeScale;

    ShowItem::paint(painter, option, widget);

    if (m_peaks && m_waveMode != Hidden)
    {
        // ms per pixel: 50 px per second at time scale 1
        const double msPerPx = 20.0 * double(m_timeScale);
        const QColor base = getColor();
        painter->setPen(QPen(base.lightnessF() > 0.55 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 190), 1));
        m_peaks->paint(painter, QRectF(0, 2, m_width, TRACK_HEIGHT - 4), msPerPx,
                       option->exposedRect, WaveformPeaks::Mode(m_waveMode));
    }

    if (m_audio->fadeInSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeInSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(1, TRACK_HEIGHT - 4, fadeXpos, 2);
    }

    if (m_audio->fadeOutSpeed() != 0)
    {
        int fadeXpos = (timeScale * (float)m_audio->fadeOutSpeed()) / 1000;
        painter->setPen(QPen(Qt::gray, 1));
        painter->drawLine(m_width - fadeXpos, 2, m_width - 1, TRACK_HEIGHT - 4);
    }

    ShowItem::postPaint(painter);
}

void AudioItem::setTimeScale(int val)
{
    ShowItem::setTimeScale(val);
    calculateWidth();
}

void AudioItem::setDuration(quint32 msec, bool stretch)
{
    Q_UNUSED(msec)
    Q_UNUSED(stretch)
    // nothing to do
}

QString AudioItem::functionName() const
{
    if (m_audio)
        return m_audio->name();
    return QString();
}

Audio *AudioItem::getAudio() const
{
    return m_audio;
}

void AudioItem::requestWaveform()
{
    const QString path = m_audio->getSourceFileName();
    Doc *doc = m_audio->doc();
    m_peaks = WaveformService::instance()->request(path, [doc, path]() -> AudioDecoder *
    {
        return doc->audioPluginCache()->getDecoderForFile(path);
    });
    update();
    emit waveformChanged();
}

void AudioItem::slotWaveformReady(const QString &path)
{
    if (path != m_audio->getSourceFileName())
        return;
    m_peaks = WaveformService::instance()->lookup(path);
    update();
    emit waveformChanged();
}

void AudioItem::setWaveformMode(int mode)
{
    if (mode < Hidden || mode > WaveformPeaks::Stereo)
        mode = WaveformPeaks::Mixed;
    m_waveMode = mode;
    for (int i = 0; i < 5; i++)
        m_waveActions[i]->setChecked(i - 1 == mode);
    update();
    emit waveformChanged();
}

void AudioItem::slotAudioChanged(quint32)
{
    requestWaveform();
    prepareGeometryChange();
    calculateWidth();
    if (m_function)
        m_function->setDuration(m_audio->totalDuration());
}

void AudioItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    QMenu menu;
    QFont menuFont = qApp->font();
    menuFont.setPixelSize(14);
    menu.setFont(menuFont);

    if (m_audio->getAudioDecoder() != NULL)
    {
        AudioDecoder *ad = m_audio->getAudioDecoder();
        AudioParameters ap = ad->audioParameters();

        menu.addAction(m_waveActions[0]);
        menu.addAction(m_waveActions[1]);
        if (ap.channels() == 2)
        {
            menu.addAction(m_waveActions[2]);
            menu.addAction(m_waveActions[3]);
            menu.addAction(m_waveActions[4]);
        }
        menu.addSeparator();
    }

    foreach (QAction *action, getDefaultActions())
        menu.addAction(action);

    menu.exec(QCursor::pos());
}
