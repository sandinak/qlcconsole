/*
  Q Light Controller Plus
  audioitem.h

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

#ifndef AUDIOITEM_H
#define AUDIOITEM_H

#include <QGraphicsItem>
#include <QObject>
#include <QAction>
#include <QFont>

#include "showitem.h"
#include "audio.h"
#include "waveformpeaks.h"

/** @addtogroup ui_functions
 * @{
 */

/**
 *
 * Audio Item. Clickable and draggable object identifying an Audio object
 *
 */
class AudioItem final : public ShowItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)

public:
    AudioItem(Audio *aud, ShowFunction *func);

    /** @reimp */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    /** @reimp */
    void setTimeScale(int val) override;

    /** @reimp */
    void setDuration(quint32 msec, bool stretch) override;

    /** @reimp */
    QString functionName() const override;

    /** Return a pointer to a Audio Function associated to this item */
    Audio *getAudio() const;

    /** The file's waveform envelope once it is ready, else null */
    WaveformService::PeaksPtr peaks() const { return m_peaks; }

    /** How the waveform is drawn; Hidden draws none */
    enum { Hidden = -1 };
    void setWaveformMode(int mode);
    int waveformMode() const { return m_waveMode; }

signals:
    /** The waveform became available (or its mode changed) */
    void waveformChanged();

protected:
    /** @reimp */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

protected slots:
    void slotAudioChanged(quint32);

    void slotWaveformReady(const QString &path);

private:
    /** Calculate sequence width for paint() and boundingRect() */
    void calculateWidth();

    /** Ask the WaveformService for the file's envelope (cached on disk, built
     *  on a worker thread when missing) */
    void requestWaveform();

public:
    /** Reference to the actual Audio Function */
    Audio *m_audio;

    /** Context menu actions (a WaveformPeaks::Mode each, plus hide) */
    QAction *m_waveActions[5];

private:
    WaveformService::PeaksPtr m_peaks;
    int m_waveMode;
};

/** @} */

#endif
