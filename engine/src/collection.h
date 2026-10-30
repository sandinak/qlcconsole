/*
  Q Light Controller Plus
  collection.h

  Copyright (c) Heikki Junnila
                Massimo Callegari

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

#ifndef COLLECTION_H
#define COLLECTION_H

#include <QMutex>
#include <QList>
#include <QHash>
#include <QSet>

#include "function.h"

class QXmlStreamReader;

/** @addtogroup engine_functions Functions
 * @{
 */

typedef struct
{
    quint32 m_id;
    int m_intensityId;
} ChildFunction;

class Collection final : public Function
{
    Q_OBJECT
    Q_DISABLE_COPY(Collection)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    Collection(Doc* doc);
    virtual ~Collection();

    /** @reimp */
    QIcon getIcon() const override;

    /** @reimp */
    quint32 totalDuration() override;

    /*********************************************************************
     * Copying
     *********************************************************************/
public:
    /** @reimp */
    Function* createCopy(Doc* doc, bool addToDoc = true) override;

    /** Copy the contents for this function from another function */
    bool copyFrom(const Function* function) override;

    /*********************************************************************
     * Contents
     *********************************************************************/
public:
    /**
     * Add a function to this collection. If the function is already a
     * member of the collection, this call fails.
     *
     * @param fid The function to add
     * @return true if successful, otherwise false
     */
    bool addFunction(quint32 fid, int insertIndex = -1);

    /**
     * Remove a function from this collection. If the function is not a
     * member of the collection, this call fails.
     *
     * @param fid The function to remove
     * @return true if successful, otherwise false
     */
    bool removeFunction(quint32 fid);

    /**
     * Get this function's list of member functions
     */
    QList <quint32> functions() const;

signals:
    void functionsChanged();

public slots:
    /** Catches Doc::functionRemoved() so that destroyed members can be
        removed immediately. */
    void slotFunctionRemoved(quint32 function);

protected:
    /** The list of Function IDs added to this Collection */
    QList <quint32> m_functions;
    /** A list of intesity attribute override IDs populated when this Collection is
     *  started and cleaned when it's stopped */
    QList <int> m_intensityOverrideIds;
    /** The intensity override this collection holds on each member it
     *  started, by member: given back in postRun() to a member another
     *  parent keeps running (m_intensityOverrideIds is by position, which
     *  an edit to the member list while running would shift). */
    QHash <quint32, int> m_overrideIdByMember;

    mutable QRecursiveMutex m_functionListMutex;

    /*********************************************************************
     * Save & Load
     *********************************************************************/
public:
    /** Save function's contents to an XML document */
    bool saveXML(QXmlStreamWriter *doc) const override;

    /** Load function's contents from an XML document */
    bool loadXML(QXmlStreamReader &root) override;

    /** @reimp */
    void postLoad() override;

public:
    /** @reimp */
    bool contains(quint32 functionId) const override;

    /** @reimp */
    QList<quint32> components() const override;

    /*********************************************************************
     * Running
     *********************************************************************/
private:
    FunctionParent functionParent() const;

public:
    /** @reimpl */
    void preRun(MasterTimer* timer) override;

    /** @reimpl */
    void setPause(bool enable) override;

    /** @reimpl */
    void write(MasterTimer* timer, QList<Universe *> universes) override;

    /** @reimpl */
    void postRun(MasterTimer* timer, QList<Universe *> universes) override;

protected slots:
    /** Called whenever one of this function's child functions stops */
    void slotChildStopped(quint32 fid);

    /** Called whenever one of this function's child functions stops */
    void slotChildStarted(quint32 fid);

protected:
    /** Number of currently running children */
    QSet <quint32> m_runningChildren;
    unsigned int m_tick;

    /*************************************************************************
     * Intensity
     *************************************************************************/
public:
    /** @reimp */
    int adjustAttribute(qreal fraction, int attributeId) override;

    /*************************************************************************
     * Blend mode
     *************************************************************************/
public:
    /** @reimp */
    void setBlendMode(Universe::BlendMode mode) override;

    /*************************************************************************
     * Flash (a Show Tree hit)
     *************************************************************************/
public:
    /** A Show Tree hit's Collection: a VC Flash button plays it while held
     *  (flash() starts it, unFlash() stops the run it started). Set by
     *  ShowTreeController as it bakes the hit; not saved. Off, a Collection
     *  flashes as in QLC+: not at all. */
    void setFlashPlays(bool on) { m_flashPlays = on; }
    bool flashPlays() const { return m_flashPlays; }

    /** A Show Tree hit's own fade in / out (ms, Q180): its members start
     *  with them when nothing that starts the Collection says otherwise
     *  (a chaser step's or a timeline's fade wins). 0: their own, as in
     *  QLC+. Set by ShowTreeController as it bakes the hit; not saved. */
    void setHitFades(uint fadeIn, uint fadeOut) { m_hitFadeIn = fadeIn; m_hitFadeOut = fadeOut; }
    uint hitFadeIn() const { return m_hitFadeIn; }
    uint hitFadeOut() const { return m_hitFadeOut; }

    /** @reimp */
    void flash(MasterTimer *timer, bool shouldOverride, bool forceLTP) override;
    /** @reimp */
    void unFlash(MasterTimer *timer) override;

private:
    bool m_flashPlays = false;
    uint m_hitFadeIn = 0;
    uint m_hitFadeOut = 0;
    bool m_flashStarted = false;    ///< flash() started the run unFlash() stops
};

/** @} */

#endif
