/*
  Q Light Controller Plus
  track.h

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

#ifndef TRACK_H
#define TRACK_H

#include <QObject>
#include <QHash>
#include <QColor>

#include "showfunction.h"
#include "scene.h"

class QXmlStreamReader;

/** @addtogroup engine_functions Functions
 * @{
 */

#define KXMLQLCTrack        QStringLiteral("Track")
#define KXMLQLCTrackID      QStringLiteral("ID")

class Track : public QObject
{
    Q_OBJECT

    Q_PROPERTY(quint32 id READ id CONSTANT)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool mute READ isMute WRITE setMute NOTIFY muteChanged)

    /************************************************************************
     * Initialization
     ************************************************************************/
public:
    /** Create a new Track and associate it to a Scene  */
    Track(quint32 sceneID = Function::invalidId(), QObject *parent = 0);

    /** destroy this Track */
    ~Track();

signals:
    /** Emitted whenever some property is changed */
    void changed(quint32 id);

    /************************************************************************
     * ID
     ************************************************************************/
public:
    /** Set a track id (only Doc is allowed to do this!) */
    void setId(quint32 id);

    /** Get a track unique id */
    quint32 id() const;

    /** Get an invalid track id */
    static quint32 invalidId();

    /** Get/Set the Show ID this Track belongs to */
    quint32 showId();
    void setShowId(quint32 id);

private:
    quint32 m_id;
    quint32 m_showId;

    /************************************************************************
     * Name
     ************************************************************************/
public:
    /** Set the name of the track */
    void setName(const QString& name);

    /** Get the name of this track */
    QString name() const;

signals:
    void nameChanged();

private:
    QString m_name;

    /*********************************************************************
     * Scene
     *********************************************************************/
public:
    /** Set the Scene ID associated to this track */
    void setSceneID(quint32 id);

    /** Return the Scene ID associated to this track */
    quint32 getSceneID();

private:
    /** ID of the Scene which this track represents
     *  Returns Function::invalidId() if no Scene is
     *  represented (e.g. audio/video tracks)
     */
    quint32 m_sceneID;

    /*********************************************************************
     * Mute state
     *********************************************************************/
public:
    /** Set the mute state of this track */
    void setMute(bool);

    /** Return the mute state of the track */
    bool isMute();

signals:
    void muteChanged(bool mute);

private:
    /** Flag to mute/unmute this track */
    bool m_isMute;

    /*********************************************************************
     * Lock state
     *********************************************************************/
public:
    /** Set the lock state of this track (locked = read-only on the timeline) */
    void setLocked(bool locked);

    /** Return the lock state of the track */
    bool isLocked() const;

signals:
    void lockedChanged(bool locked);

private:
    /** Flag to lock/unlock this track (read-only on the timeline) */
    bool m_isLocked;

    /*********************************************************************
     * Solo / solo-safe (mute-exempt)
     *********************************************************************/
public:
    /** Solo this track (model-backed so the runner honours it). When any track
     *  is soloed, only soloed (and solo-safe) tracks output. */
    void setSolo(bool solo);
    bool isSolo() const;

    /** Solo-safe / mute-exempt: solo and "mute others" never silence this track
     *  (house lights, work light, a safety look). It can still be muted directly. */
    void setSoloSafe(bool safe);
    bool isSoloSafe() const;

signals:
    void soloChanged(bool solo);

private:
    bool m_isSolo = false;
    bool m_soloSafe = false;

    /*********************************************************************
     * Hold-last (keep the last look across gaps)
     *********************************************************************/
public:
    /** When true, the track holds its last cue's look (via the last-look holder)
     *  when its content ends, instead of releasing to black — a "stays up
     *  between numbers" track. Operate only. */
    void setHoldLast(bool hold);
    bool isHoldLast() const;

private:
    bool m_holdLast = false;

    /*********************************************************************
     * Priority / role (output arbitration)
     *********************************************************************/
public:
    /** How this track's output arbitrates vs other tracks and the VC:
     *  Background = low (a base wash yields to everything);
     *  Normal = Auto (last-writer-wins, the default);
     *  Override = high (an accent track wins LTP). */
    enum Priority { Background = 0, Normal = 1, Override = 2 };
    void setPriority(Priority p);
    Priority priority() const;

private:
    Priority m_priority = Normal;

    /*********************************************************************
     * Intensity submaster (0..1 scales this track's dimmer output)
     *********************************************************************/
public:
    /** Per-track intensity submaster (0.0..1.0). Scales the track's dimmer
     *  output live, without editing cues — a fader per track. Default 1.0. */
    void setIntensity(qreal fraction);
    qreal intensity() const;

private:
    qreal m_intensity = 1.0;

    /*********************************************************************
     * Colour
     *********************************************************************/
public:
    /** Get/Set a display colour for the track header (invalid = default). */
    QColor color() const;
    void setColor(const QColor &color);

private:
    QColor m_color;

    /*********************************************************************
     * Functions
     *********************************************************************/
public:
    /**
     * Add a ShowFunction with the given Function ID to the track.
     * If the function doesn't exist, it creates it.
     * In any case it returns the ShowFunction pointer
     */
    ShowFunction *createShowFunction(quint32 functionID);

    /** remove a function ID association from this track */
    bool removeShowFunction(ShowFunction *function, bool performDelete = true);

    /** add a ShowFunction element to this track */
    bool addShowFunction(ShowFunction *func);

    /** Get a reference to a ShowFunction with the provided ID */
    ShowFunction *showFunction(quint32 id);

    /** Returns the list of ShowFunctions added to this Track */
    QList <ShowFunction *> showFunctions() const;

private:
    /** List of Function IDs present in this track */
    QList <ShowFunction *> m_functions;

    /*********************************************************************
     * Load & Save
     *********************************************************************/
public:
    bool saveXML(QXmlStreamWriter *doc);

    bool loadXML(QXmlStreamReader &root);

    bool postLoad(Doc *doc);

public:
    bool contains(Doc *doc, quint32 functionId) const;

    QList<quint32> components() const;

};

/** @} */

#endif
