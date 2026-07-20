/*
  Q Light Controller Plus
  track.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "sequence.h"
#include "track.h"
#include "scene.h"
#include "show.h"
#include "doc.h"

#define KXMLQLCTrackName      QStringLiteral("Name")
#define KXMLQLCTrackSceneID   QStringLiteral("SceneID")
#define KXMLQLCTrackIsMute    QStringLiteral("isMute")
#define KXMLQLCTrackIsLocked  QStringLiteral("Locked")
#define KXMLQLCTrackColor     QStringLiteral("Color")
#define KXMLQLCTrackIsSolo    QStringLiteral("Solo")
#define KXMLQLCTrackSoloSafe  QStringLiteral("SoloSafe")
#define KXMLQLCTrackHoldLast  QStringLiteral("HoldLast")
#define KXMLQLCTrackPriority  QStringLiteral("Priority")
#define KXMLQLCTrackIntensity QStringLiteral("Intensity")

#define KXMLQLCTrackFunctions QStringLiteral("Functions")

Track::Track(quint32 sceneID, QObject *parent)
    : QObject(parent)
    , m_id(Track::invalidId())
    , m_showId(Function::invalidId())
    , m_sceneID(sceneID)
    , m_isMute(false)
    , m_isLocked(false)
{
    setName(tr("New Track"));
}

Track::~Track()
{

}

/****************************************************************************
 * ID
 ****************************************************************************/

void Track::setId(quint32 id)
{
    m_id = id;
}

quint32 Track::id() const
{
    return m_id;
}

quint32 Track::invalidId()
{
    return UINT_MAX;
}

quint32 Track::showId()
{
    return m_showId;
}

void Track::setShowId(quint32 id)
{
    m_showId = id;
}

/****************************************************************************
 * Name
 ****************************************************************************/

void Track::setName(const QString& name)
{
    m_name = name;
    emit changed(this->id());
}

QString Track::name() const
{
    return m_name;
}

/*********************************************************************
 * Scene
 *********************************************************************/
void Track::setSceneID(quint32 id)
{
    m_sceneID = id;
}

quint32 Track::getSceneID()
{
    return m_sceneID;
}

/*********************************************************************
 * Mute state
 *********************************************************************/
void Track::setMute(bool state)
{
    if (m_isMute == state)
        return;

    m_isMute = state;

    emit muteChanged(state);
}

bool Track::isMute()
{
    return m_isMute;
}

void Track::setLocked(bool locked)
{
    if (m_isLocked == locked)
        return;

    m_isLocked = locked;

    emit lockedChanged(locked);
}

bool Track::isLocked() const
{
    return m_isLocked;
}

void Track::setSolo(bool solo)
{
    if (m_isSolo == solo)
        return;
    m_isSolo = solo;
    emit soloChanged(solo);
    emit changed(m_id);
}

bool Track::isSolo() const
{
    return m_isSolo;
}

void Track::setSoloSafe(bool safe)
{
    if (m_soloSafe == safe)
        return;
    m_soloSafe = safe;
    emit changed(m_id);
}

bool Track::isSoloSafe() const
{
    return m_soloSafe;
}

void Track::setHoldLast(bool hold)
{
    if (m_holdLast == hold)
        return;
    m_holdLast = hold;
    emit changed(m_id);
}

bool Track::isHoldLast() const
{
    return m_holdLast;
}

void Track::setPriority(Track::Priority p)
{
    if (m_priority == p)
        return;
    m_priority = p;
    emit changed(m_id);
}

Track::Priority Track::priority() const
{
    return m_priority;
}

void Track::setIntensity(qreal fraction)
{
    fraction = qBound(qreal(0.0), fraction, qreal(1.0));
    if (qFuzzyCompare(m_intensity, fraction))
        return;
    m_intensity = fraction;
    emit changed(m_id);
}

qreal Track::intensity() const
{
    return m_intensity;
}

QColor Track::color() const
{
    return m_color;
}

void Track::setColor(const QColor &color)
{
    m_color = color;
    emit changed(m_id);
}

/*********************************************************************
 * Sequences
 *********************************************************************/

ShowFunction *Track::createShowFunction(quint32 functionID)
{
    Show *show = qobject_cast<Show *>(parent());
    quint32 uId = show == NULL ? 0 : show->getLatestShowFunctionId();
    ShowFunction *func = new ShowFunction(uId);
    func->setFunctionID(functionID);
    m_functions.append(func);

    return func;
}

bool Track::addShowFunction(ShowFunction *func)
{
    if (func == NULL || func->functionID() == Function::invalidId())
        return false;

    m_functions.append(func);

    return true;
}

ShowFunction *Track::showFunction(quint32 id)
{
    foreach (ShowFunction *sf, m_functions)
        if (sf->id() == id)
            return sf;

    return NULL;
}

bool Track::removeShowFunction(ShowFunction *function, bool performDelete)
{
    if (m_functions.contains(function) == false)
        return false;

    ShowFunction *func = m_functions.takeAt(m_functions.indexOf(function));
    if (performDelete && func)
        delete func;

    return true;
}

QList <ShowFunction *> Track::showFunctions() const
{
    return m_functions;
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/
bool Track::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    /* Track entry */
    doc->writeStartElement(KXMLQLCTrack);
    doc->writeAttribute(KXMLQLCTrackID, QString::number(this->id()));
    doc->writeAttribute(KXMLQLCTrackName, this->name());
    if (m_sceneID != Scene::invalidId())
        doc->writeAttribute(KXMLQLCTrackSceneID, QString::number(m_sceneID));
    doc->writeAttribute(KXMLQLCTrackIsMute, QString::number(m_isMute));
    if (m_isLocked)
        doc->writeAttribute(KXMLQLCTrackIsLocked, QString::number(1));
    if (m_color.isValid())
        doc->writeAttribute(KXMLQLCTrackColor, m_color.name());
    if (m_isSolo)
        doc->writeAttribute(KXMLQLCTrackIsSolo, QString::number(1));
    if (m_soloSafe)
        doc->writeAttribute(KXMLQLCTrackSoloSafe, QString::number(1));
    if (m_holdLast)
        doc->writeAttribute(KXMLQLCTrackHoldLast, QString::number(1));
    if (m_priority != Normal)
        doc->writeAttribute(KXMLQLCTrackPriority, QString::number(int(m_priority)));
    if (m_intensity < 1.0)
        doc->writeAttribute(KXMLQLCTrackIntensity, QString::number(m_intensity, 'f', 4));

    /* Save the list of Functions if any is present */
    if (m_functions.isEmpty() == false)
    {
        foreach (ShowFunction *func, showFunctions())
            func->saveXML(doc);
    }

    doc->writeEndElement();

    return true;
}

bool Track::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCTrack)
    {
        qWarning() << Q_FUNC_INFO << "Track node not found";
        return false;
    }

    bool ok = false;
    QXmlStreamAttributes attrs = root.attributes();
    quint32 id = attrs.value(KXMLQLCTrackID).toString().toUInt(&ok);
    if (ok == false)
    {
        qWarning() << "Invalid Track ID:" << attrs.value(KXMLQLCTrackID).toString();
        return false;
    }
    // Assign the ID to myself
    m_id = id;

    if (attrs.hasAttribute(KXMLQLCTrackName) == true)
        m_name = attrs.value(KXMLQLCTrackName).toString();

    if (attrs.hasAttribute(KXMLQLCTrackSceneID))
    {
        ok = false;
        id = attrs.value(KXMLQLCTrackSceneID).toString().toUInt(&ok);
        if (ok == false)
        {
            qWarning() << "Invalid Scene ID:" << attrs.value(KXMLQLCTrackSceneID).toString();
            return false;
        }
        m_sceneID = id;
    }

    ok = false;
    bool mute = attrs.value(KXMLQLCTrackIsMute).toString().toInt(&ok);
    if (ok == false)
    {
        qWarning() << "Invalid Mute flag:" << root.attributes().value(KXMLQLCTrackIsMute).toString();
        return false;
    }
    m_isMute = mute;

    if (attrs.hasAttribute(KXMLQLCTrackIsLocked))
        m_isLocked = (attrs.value(KXMLQLCTrackIsLocked).toString().toInt() != 0);

    if (attrs.hasAttribute(KXMLQLCTrackColor))
        m_color = QColor(attrs.value(KXMLQLCTrackColor).toString());

    if (attrs.hasAttribute(KXMLQLCTrackIsSolo))
        m_isSolo = (attrs.value(KXMLQLCTrackIsSolo).toString().toInt() != 0);
    if (attrs.hasAttribute(KXMLQLCTrackSoloSafe))
        m_soloSafe = (attrs.value(KXMLQLCTrackSoloSafe).toString().toInt() != 0);
    if (attrs.hasAttribute(KXMLQLCTrackHoldLast))
        m_holdLast = (attrs.value(KXMLQLCTrackHoldLast).toString().toInt() != 0);
    if (attrs.hasAttribute(KXMLQLCTrackPriority))
    {
        const int p = attrs.value(KXMLQLCTrackPriority).toString().toInt();
        m_priority = (p == Background) ? Background : (p == Override ? Override : Normal);
    }
    if (attrs.hasAttribute(KXMLQLCTrackIntensity))
    {
        bool iok = false;
        const double iv = attrs.value(KXMLQLCTrackIntensity).toString().toDouble(&iok);
        if (iok)   // garbage must not silently darken the track to 0
            m_intensity = qBound(0.0, iv, 1.0);
    }

    /* look for show functions */
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLShowFunction)
        {
            Show *show = qobject_cast<Show *>(parent());
            quint32 uId = show == NULL ? 0 : show->getLatestShowFunctionId();
            ShowFunction *newFunc = new ShowFunction(uId);
            newFunc->loadXML(root);
            if (addShowFunction(newFunc) == false)
                delete newFunc;
        }
        /* LEGACY code: to be removed */
        else if (root.name() == KXMLQLCTrackFunctions)
        {
            QString strvals = root.readElementText();
            if (strvals.isEmpty() == false)
            {
                QStringList varray = strvals.split(",");
                for (int i = 0; i < varray.count(); i++)
                    createShowFunction(QString(varray.at(i)).toUInt());
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Track tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool Track::postLoad(Doc* doc)
{
    bool modified = false;
    QMutableListIterator<ShowFunction*> it(m_functions);
    while (it.hasNext())
    {
        ShowFunction* showFunction = it.next();

        Function* function = doc->function(showFunction->functionID());
        if (function == NULL
                || (m_showId != Function::invalidId()
                    && function->contains(m_showId)))
        {
            it.remove();
            delete showFunction;
            modified = true;
            continue;
        }

        //if (showFunction->duration() == 0)
        //    showFunction->setDuration(function->totalDuration());
        if (showFunction->color().isValid() == false)
            showFunction->setColor(ShowFunction::defaultColor(function->type()));

        if (function->type() == Function::SequenceType)
        {
            Sequence* sequence = qobject_cast<Sequence*>(function);
            if (sequence == NULL || getSceneID() == sequence->boundSceneID())
                continue;
#ifndef QMLUI
            if (getSceneID() == Function::invalidId())
            {
                // No scene ID, use the one from this sequence
                setSceneID(sequence->boundSceneID());
            }
            else
            {
                // Conflicting scene IDs, we have to remove this sequence
                it.remove();
                delete showFunction;
            }
#endif
            modified = true;
        }
    }
    return modified;
}

bool Track::contains(Doc* doc, quint32 functionId) const
{
    if (m_sceneID == functionId)
        return true;

    QListIterator<ShowFunction*> it(m_functions);
    while (it.hasNext())
    {
        ShowFunction* showFunction = it.next();

        Function* function = doc->function(showFunction->functionID());
        // contains() can be called during init, function may be NULL
        if (function == NULL)
            continue;

        if (function->id() == functionId)
            return true;
        if (function->contains(functionId))
            return true;
    }

    return false;
}

QList<quint32> Track::components() const
{
    QList<quint32> ids;

    QListIterator<ShowFunction*> it(m_functions);
    while (it.hasNext())
    {
        ShowFunction* showFunction = it.next();
        ids.append(showFunction->functionID());
    }

    return ids;
}
