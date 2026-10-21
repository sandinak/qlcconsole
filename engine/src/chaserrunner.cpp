/*
  Q Light Controller Plus
  chaserrunner.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari
                Jano Svitok

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

#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QDebug>

#include "chaserrunner.h"
#include "speedmasters.h"
#include <cmath>
#include "mastertimer.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "chaserstep.h"
#include "collection.h"
#include "cueoutput.h"
#include "qlcmacros.h"
#include "universe.h"
#include "fixture.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"

ChaserRunner::ChaserRunner(const Doc *doc, const Chaser *chaser, quint32 startTime)
    : QObject(NULL)
    , m_doc(doc)
    , m_chaser(chaser)
    , m_updateOverrideSpeeds(false)
    , m_startOffset(0)
    , m_lastRunStepIdx(-1)
    , m_externalClock(false)
    , m_lastFunctionID(Function::invalidId())
    , m_roundTime(new QElapsedTimer())
    , m_order()
{
    Q_ASSERT(chaser != NULL);

    m_pendingAction.m_action = ChaserNoAction;
    m_pendingAction.m_masterIntensity = 1.0;
    m_pendingAction.m_stepIntensity = 1.0;
    m_pendingAction.m_fadeMode = Chaser::FromFunction;
    m_pendingAction.m_stepIndex = -1;
    m_pendingSubAction = m_pendingAction;

    if (startTime > 0)
    {
        qDebug() << "[ChaserRunner] startTime:" << startTime;
        int idx = 0;
        quint32 stepsTime = 0;
        foreach (ChaserStep step, chaser->steps())
        {
            uint duration = m_chaser->durationMode() == Chaser::Common ? m_chaser->duration() : step.duration;

            if (startTime < stepsTime + duration)
            {
                m_pendingAction.m_action = ChaserSetStepIndex;
                m_pendingAction.m_stepIndex = idx;
                m_startOffset = startTime - stepsTime;
                qDebug() << "[ChaserRunner] Starting from step:" << idx;
                break;
            }
            idx++;
            stepsTime += duration;
        }
    }

    m_direction = m_chaser->direction();
    connect(chaser, SIGNAL(changed(quint32)), this, SLOT(slotChaserChanged()));
    m_roundTime->restart();

    fillOrder();
}

ChaserRunner::~ChaserRunner()
{
    clearRunningList();
    delete m_roundTime;
}

/****************************************************************************
 * Speed
 ****************************************************************************/

void ChaserRunner::slotChaserChanged()
{
    // Handle (possible) speed change on the next write() pass
    m_updateOverrideSpeeds = true;
    QList<ChaserRunnerStep*> delList;
    foreach (ChaserRunnerStep *step, m_runnerSteps)
    {
        if (!m_chaser->steps().contains(ChaserStep(step->m_function->id())))
        {
            // Disappearing function: remove step
            delList.append(step);
        }
        else
        {
            // Recalculate the speed of each running step
            step->m_fadeIn = stepFadeIn(step->m_index);
            step->m_fadeOut = stepFadeOut(step->m_index);
            step->m_duration = stepDuration(step->m_index);
        }
    }
    foreach (ChaserRunnerStep *step, delList)
    {
        stopStepFunction(step);
        m_runnerSteps.removeAll(step);
        delete step;
    }
}

uint ChaserRunner::stepFadeIn(int stepIdx) const
{
    uint speed = 0;
    if (m_chaser->overrideFadeInSpeed() != Function::defaultSpeed())
    {
        // Override speed is used when another function has started the chaser,
        // i.e. chaser inside a chaser that wants to impose its own fade in speed
        // to its members.
        speed = m_chaser->overrideFadeInSpeed();
    }
    else
    {
        switch (m_chaser->fadeInMode())
        {
            case Chaser::Common:
                // All steps' fade in speed is dictated by the chaser
                speed = m_chaser->fadeInSpeed();
            break;
            case Chaser::PerStep:
            {
                // Each step specifies its own fade in speed. Take a single
                // snapshot: stepsCount() then steps().at() are separately locked,
                // so the GUI thread could shrink the list between them (crash).
                const QList<ChaserStep> steps = m_chaser->steps();
                if (stepIdx >= 0 && stepIdx < steps.count())
                    speed = steps.at(stepIdx).fadeIn;
                else
                    speed = Function::defaultSpeed();
            }
            break;
            default:
            case Chaser::Default:
                // Don't touch members' fade in speed at all
                speed = Function::defaultSpeed();
            break;
        }
    }

    return speed;
}

uint ChaserRunner::stepFadeOut(int stepIdx) const
{
    uint speed = 0;
    if (m_chaser->overrideFadeOutSpeed() != Function::defaultSpeed())
    {
        // Override speed is used when another function has started the chaser,
        // i.e. chaser inside a chaser that wants to impose its own fade out speed
        // to its members.
        speed = m_chaser->overrideFadeOutSpeed();
    }
    else
    {
        switch (m_chaser->fadeOutMode())
        {
            case Chaser::Common:
                // All steps' fade out speed is dictated by the chaser
                speed = m_chaser->fadeOutSpeed();
            break;
            case Chaser::PerStep:
            {
                // Each step specifies its own fade out speed. Single snapshot to
                // avoid a TOCTOU between stepsCount() and steps().at().
                const QList<ChaserStep> steps = m_chaser->steps();
                if (stepIdx >= 0 && stepIdx < steps.count())
                    speed = steps.at(stepIdx).fadeOut;
                else
                    speed = Function::defaultSpeed();
            }
            break;
            default:
            case Chaser::Default:
                // Don't touch members' fade out speed at all
                speed = Function::defaultSpeed();
            break;
        }
    }

    return speed;
}

uint ChaserRunner::stepDuration(int stepIdx) const
{
    uint speed = 0;
    if (m_chaser->overrideDuration() != Function::defaultSpeed())
    {
        // Override speed is used when another function has started the chaser,
        // i.e. chaser inside a chaser that wants to impose its own duration
        // to its members.
        speed = m_chaser->overrideDuration();
    }
    else
    {
        switch (m_chaser->durationMode())
        {
            default:
            case Chaser::Default:
            case Chaser::Common:
                // All steps' duration is dictated by the chaser
                speed = m_chaser->duration();
            break;
            case Chaser::PerStep:
            {
                // Each step specifies its own duration. Single snapshot to avoid a
                // TOCTOU between stepsCount() and steps().at().
                const QList<ChaserStep> steps = m_chaser->steps();
                if (stepIdx >= 0 && stepIdx < steps.count())
                    speed = steps.at(stepIdx).duration;
                else
                    speed = m_chaser->duration();
            }
            break;
        }
    }

    return speed;
}

/****************************************************************************
 * Step control
 ****************************************************************************/

void ChaserRunner::setAction(ChaserAction &action)
{
    // apply the actions that can be applied immediately
    switch (action.m_action)
    {
        case ChaserNoAction:
            m_pendingAction.m_masterIntensity = action.m_masterIntensity;
            m_pendingAction.m_stepIntensity = action.m_stepIntensity;
        break;

        case ChaserStopStep:
        {
            bool stopped = false;

            foreach (ChaserRunnerStep *step, m_runnerSteps)
            {
                if (action.m_stepIndex == step->m_index)
                {
                    qDebug() << "[ChaserRunner] Stopping step idx:" << action.m_stepIndex << "(running:" << m_runnerSteps.count() << ")";
                    m_lastFunctionID = step->m_function->type() == Function::SceneType ? step->m_function->id() : Function::invalidId();
                    stopStepFunction(step);
                    m_runnerSteps.removeOne(step);
                    delete step;
                    stopped = true;
                }
            }

            if (stopped && m_runnerSteps.size() == 1)
            {
                ChaserRunnerStep *lastStep = m_runnerSteps.at(0);
                m_lastRunStepIdx = lastStep->m_index;
                emit currentStepChanged(m_lastRunStepIdx);
            }
        }
        break;

        // Sub-cues: kept apart from the step action, so a Sub GO doesn't
        // take the place of a GO waiting for the beat (or the other way).
        case ChaserSubNextStep:
        case ChaserSubPreviousStep:
            m_pendingSubAction = action;
        break;

        case ChaserSubSetStep:
            if (action.m_stepIndex < 0)
                break;
            // Its parent step first, if that isn't the one playing.
            if (currentIndexStep() == NULL || m_lastRunStepIdx != action.m_stepIndex)
            {
                m_pendingAction.m_stepIndex = action.m_stepIndex;
                m_pendingAction.m_masterIntensity = action.m_masterIntensity;
                m_pendingAction.m_stepIntensity = action.m_stepIntensity;
                m_pendingAction.m_fadeMode = action.m_fadeMode;
                m_pendingAction.m_action = ChaserSetStepIndex;
            }
            m_pendingSubAction = action;
        break;

        // copy to pending action. Will be processed at the next write call
        default:
            m_pendingAction.m_stepIndex = action.m_stepIndex;
            m_pendingAction.m_masterIntensity = action.m_masterIntensity;
            m_pendingAction.m_stepIntensity = action.m_stepIntensity;
            m_pendingAction.m_fadeMode = action.m_fadeMode;
            m_pendingAction.m_action = action.m_action;
        break;
    }
}

void ChaserRunner::tap()
{
    if (uint(m_roundTime->elapsed()) >= (stepDuration(m_lastRunStepIdx) / 4))
        m_pendingAction.m_action = ChaserNextStep;
}

int ChaserRunner::currentStepIndex() const
{
    return m_lastRunStepIdx;
}

int ChaserRunner::runningStepsNumber() const
{
    return m_runnerSteps.count();
}

ChaserRunnerStep *ChaserRunner::currentIndexStep() const
{
    foreach (ChaserRunnerStep *step, m_runnerSteps)
    {
        if (step->m_index == m_lastRunStepIdx)
            return step;
    }
    return NULL;
}

ChaserRunnerStep *ChaserRunner::currentRunningStep() const
{
    if (m_runnerSteps.count() > 0)
        return m_runnerSteps.at(0);
    return NULL;
}

int ChaserRunner::computeNextStep(int currentStep) const
{
    // Past skipped steps (ChaserStep::skip): the one GO will play.
    int next = computeNextStepIgnoringSkips(currentStep);
    for (int guard = 0; next >= 0 && m_chaser->isStepSkipped(next)
                        && guard < m_chaser->stepsCount(); ++guard)
        next = computeNextStepIgnoringSkips(next);
    return next >= 0 && m_chaser->isStepSkipped(next) ? -1 : next;
}

int ChaserRunner::computeNextStepIgnoringSkips(int currentStep) const
{
    int nextStep = currentStep;

    if (m_chaser->runOrder() == Function::Random)
    {
        nextStep = m_order.indexOf(nextStep);
        if (nextStep == -1)
        {
            qDebug() << "[ChaserRunner] steps order not found";
            nextStep = currentStep;
        }
    }

    // Next step
    if (m_direction == Function::Forward)
    {
        nextStep++;
    }
    else
    {
        nextStep--;
    }

    if (nextStep < m_chaser->stepsCount() && nextStep >= 0)
    {
        if (m_chaser->runOrder() == Function::Random)
        {
            nextStep = randomStepIndex(nextStep);
        }
        return nextStep; // In the middle of steps. No need to go any further.
    }

    if (m_chaser->runOrder() == Function::SingleShot)
    {
        return -1; // Forward or Backward SingleShot has been completed.
    }
    else if (m_chaser->runOrder() == Function::Loop)
    {
        if (m_direction == Function::Forward)
        {
            if (nextStep >= m_chaser->stepsCount())
                nextStep = 0;
            else
                nextStep = m_chaser->stepsCount() - 1; // Used by CueList with manual prev
        }
        else // Backward
        {
            if (nextStep < 0)
                nextStep = m_chaser->stepsCount() - 1;
            else
                nextStep = 0;
        }
    }
    else if (m_chaser->runOrder() == Function::Random)
    {
        nextStep = randomStepIndex(nextStep);
    }
    else // Ping Pong
    {
        // Change direction, but don't run the first/last step twice.
        if (m_direction == Function::Forward)
        {
            nextStep = m_chaser->stepsCount() - 2;
        }
        else // Backwards
        {
            nextStep = 1;
        }

        // Make sure we don't go beyond limits.
        nextStep = CLAMP(nextStep, 0, m_chaser->stepsCount() - 1);
    }

    return nextStep;
}

void ChaserRunner::shuffle(QVector<int> & data)
{
   int n = data.size();
   for (int i = n - 1; i > 0; --i)
   {
      qSwap(data[i], data[QRandomGenerator::global()->generate() % (i + 1)]);
   }
}

int ChaserRunner::randomStepIndex(int step) const
{
   if (m_chaser->runOrder() == Function::Random && step >= 0 && step < m_order.size())
       return m_order[step];

   return step;
}

void ChaserRunner::fillOrder()
{
    fillOrder(m_chaser->stepsCount());
}

void ChaserRunner::fillOrder(int size)
{
   m_order.resize(size);
   for (int i = 0; i < size; ++i)
       m_order[i] = i;

   shuffle(m_order);
}

/****************************************************************************
 * Intensity
 ****************************************************************************/

void ChaserRunner::adjustStepIntensity(qreal fraction, int requestedStepIndex, int fadeControl)
{
    fraction = CLAMP(fraction, qreal(0.0), qreal(1.0));

    //qDebug() << "Adjust intensity" << fraction << "step:" << requestedStepIndex << "fade:" << fadeControl;

    int stepIndex = requestedStepIndex;
    if (stepIndex == -1)
    {
        stepIndex = m_lastRunStepIdx;
        // store the intensity to be applied at the next step startup
        m_pendingAction.m_masterIntensity = fraction;
    }

    foreach (ChaserRunnerStep *step, m_runnerSteps)
    {
        if (stepIndex == step->m_index && step->m_function != NULL)
        {
            if (requestedStepIndex == -1 && step->m_function->type() == Function::SceneType)
            {
                Scene *scene = qobject_cast<Scene *>(step->m_function);
                scene->adjustAttribute(fraction, step->m_pIntensityOverrideId);
            }
            else
            {
                step->m_function->adjustAttribute(fraction, step->m_intensityOverrideId);
            }
            return;
        }
    }

    // No need to start a new step if it is not wanted
    if (requestedStepIndex == -1)
        return;

    // Don't start a step with an intensity of zero
    if (fraction == qreal(0.0))
        return;

    // not found ? It means we need to start a new step and crossfade kicks in !
    startNewStep(stepIndex, m_doc->masterTimer(), m_pendingAction.m_masterIntensity, fraction, fadeControl);
}

/****************************************************************************
 * Running
 ****************************************************************************/

void ChaserRunner::clearRunningList(bool handOver)
{
    if (handOver == false)
    {
        // Stopping: the restacked state is let go of with the rest.
        dismissRestack();
        m_restackFrom = -1;
        m_raiseFor = 0;
        stopHandedOverSteps();
        stopHandedOverSteps();
        // Nothing new is coming: what a step change left goes now too.
        m_handedOverSteps = m_leavingSteps;
        m_leavingSteps.clear();
        stopHandedOverSteps();
    }

    // empty the running queue
    foreach (ChaserRunnerStep *step, m_runnerSteps)
    {
        if (step->m_function)
        {
            // restore the original Function fade out time
            step->m_function->setOverrideFadeOutSpeed(stepFadeOut(step->m_index));
            if (handOver && (step->m_function->type() == Function::CollectionType || step->m_subCues))
            {
                m_lastFunctionID = Function::invalidId();
                m_leavingSteps.append(step);
                continue;
            }
            stopStepFunction(step, m_chaser->type() == Function::SequenceType);
            m_lastFunctionID = step->m_function->type() == Function::SceneType ? step->m_function->id() : Function::invalidId();
        }
        delete step;
    }
    m_runnerSteps.clear();
}

/* What a step plays its cue with, for the hand-over: a Collection itself;
   a chase held as sub-cues (a Show Tree cue with sub-cues) the Collection of
   its sub-cue playing -- or, not started yet, the one it starts on. */
static const Function *playingCollection(const Doc *doc, const Function *f)
{
    const Chaser *inner = qobject_cast<const Chaser *>(f);
    if (inner == NULL || doc == NULL)
        return f;
    int index = inner->isRunning() ? inner->currentStepIndex() : -1;
    if (index < 0)
        index = inner->firstPlayableStep();
    const QList<ChaserStep> steps = inner->steps();
    if (index < 0 || index >= steps.count())
        return NULL;
    return doc->function(steps.at(index).fid);
}

bool ChaserRunner::sharesMembers(const Function *from, const Function *to)
{
    if (from == NULL || to == NULL || from == to
        || from->type() != Function::CollectionType || to->type() != Function::CollectionType)
        return false;
    // One inside the other (a cue that includes the cue before it: a
    // Collection of that Collection and more), or a member in common at any
    // depth (Collection::contains() looks inside nested ones).
    if (to->contains(from->id()) || from->contains(to->id()))
        return true;
    for (quint32 fid : from->components())
        if (to->contains(fid))
            return true;
    return false;
}

void ChaserRunner::leaveSteps()
{
    if (m_leavingSteps.isEmpty())
        return;

    const ChaserRunnerStep *now = m_runnerSteps.isEmpty() ? NULL : m_runnerSteps.last();
    foreach (ChaserRunnerStep *step, m_leavingSteps)
    {
        if (now != NULL && sharesMembers(playingCollection(m_doc, step->m_function),
                                         playingCollection(m_doc, now->m_function)))
        {
            if (step->m_subCues || now->m_subCues)
                m_handedOverLater.append(step);
            else
                m_handedOverSteps.append(step);
            continue;
        }
        stopStepFunction(step, m_chaser->type() == Function::SequenceType);
        delete step;
    }
    m_leavingSteps.clear();
}

void ChaserRunner::stopHandedOverSteps()
{
    foreach (ChaserRunnerStep *step, m_handedOverSteps)
    {
        stopStepFunction(step, m_chaser->type() == Function::SequenceType);
        delete step;
    }
    m_handedOverSteps = m_handedOverLater;
    m_handedOverLater.clear();
}

/****************************************************************************
 * Tracked state (Back, a jump)
 ****************************************************************************/

void ChaserRunner::stepStarted(int index, QList<Universe *> universes, bool jump)
{
    // Whatever was restacked for the cue before goes now: its channels keep
    // their values, as forward playback leaves them.
    dismissRestack();

    const int from = m_restackFrom;
    m_restackFrom = index;
    if (from < 0)
    {
        m_visited.clear();
        m_touched.clear();
        // The stage as the list found it: what a channel only later cues
        // set goes back to.
        m_startStage.clear();
        for (Universe *u : universes)
            m_startStage << (u != NULL ? u->preGMValues() : QByteArray());
    }

    const QList<ChaserStep> steps = m_chaser->steps();
    if (index < 0 || index >= steps.count())
        return;
    const ChaserStep &step = steps.at(index);
    const Function *f = m_doc->function(step.fid);
    const bool cue = f != NULL && (f->type() == Function::CollectionType
                                   || (step.subCues != ChaserStep::Auto && f->type() == Function::ChaserType));
    if (!cue)
        return;
    m_raiseFor = 4;
    if (!isNextStep(from, index, jump == false))
        restack(index, universes);
    m_visited.insert(index);
}

bool ChaserRunner::isNextStep(int from, int to, bool wrap) const
{
    if (m_chaser->direction() != Function::Forward
        || (m_chaser->runOrder() != Function::SingleShot && m_chaser->runOrder() != Function::Loop))
        return true;
    if (from < 0)
        return to == m_chaser->firstPlayableStep();
    // Out of a skipped step (a jump played it): played from the top it
    // wasn't there, so what it set is put back as the cues before had it.
    if (m_chaser->isStepSkipped(from))
        return false;
    const int count = m_chaser->stepsCount();
    int next = from + 1;
    while (next < count && m_chaser->isStepSkipped(next))
        ++next;
    if (next >= count)
        next = wrap && m_chaser->runOrder() == Function::Loop ? m_chaser->firstPlayableStep() : -1;
    return to == next;
}

QList<quint32> ChaserRunner::stepLooks(const ChaserStep &step, bool every) const
{
    const Chaser *inner = step.subCues != ChaserStep::Auto
                          ? qobject_cast<const Chaser *>(m_doc->function(step.fid)) : NULL;
    if (inner == NULL)
        return QList<quint32>() << step.fid;
    // Sub-cues: the first plays with GO; ones that follow on by themselves
    // all play, in order.
    QList<quint32> looks;
    const QList<ChaserStep> subs = inner->steps();
    for (int i = 0; i < subs.count(); ++i)
    {
        if (inner->isStepSkipped(i))
            continue;
        looks << subs.at(i).fid;
        if (!every && step.subCues != ChaserStep::SubGoFollow)
            break;
    }
    return looks;
}

/* The Scenes @a fid plays, in the order they start (a later one wins an LTP
   channel they share): a Collection's members, at any depth. Other kinds
   of function move by themselves, so have no one value to put back. */
static void sceneLeaves(const Doc *doc, quint32 fid, QList<quint32> &out, int depth = 0)
{
    const Function *f = doc->function(fid);
    if (f == NULL || depth > 8)
        return;
    if (f->type() == Function::SceneType)
        out << fid;
    else if (const Collection *coll = qobject_cast<const Collection *>(f))
        for (quint32 member : coll->functions())
            sceneLeaves(doc, member, out, depth + 1);
}

void ChaserRunner::restack(int index, QList<Universe *> universes)
{
    typedef QPair<quint32, quint32> Key;   // fixture, channel
    // What a scene sets (CueOutput), once per restack: a layer that tracks
    // through many cues is one Scene in each.
    QHash<quint32, QList<QPair<Key, uchar> > > outputs;
    auto output = [&](quint32 sid) -> const QList<QPair<Key, uchar> > & {
        auto it = outputs.find(sid);
        if (it != outputs.end())
            return it.value();
        QList<QPair<Key, uchar> > flat;
        const QHash<quint32, QHash<quint32, uchar> > out = CueOutput::compute(const_cast<Doc *>(m_doc), sid);
        for (auto fx = out.constBegin(); fx != out.constEnd(); ++fx)
            for (auto ch = fx.value().constBegin(); ch != fx.value().constEnd(); ++ch)
                flat << qMakePair(Key(fx.key(), ch.key()), ch.value());
        return outputs.insert(sid, flat).value();
    };
    auto leavesOf = [&](const ChaserStep &step, bool every) {
        QList<quint32> leaves;
        for (quint32 fid : stepLooks(step, every))
            sceneLeaves(m_doc, fid, leaves);
        return leaves;
    };

    const QList<ChaserStep> steps = m_chaser->steps();
    if (index < 0 || index >= steps.count())
        return;

    // What the target sets itself: its own scenes play it.
    QSet<Key> own;
    for (quint32 sid : leavesOf(steps.at(index), false))
        for (const QPair<Key, uchar> &kv : output(sid))
            own.insert(kv.first);

    // Walking back up the list to its start: the last step before the
    // target to set a channel gives its value (inside a step, its last
    // scene to). A scene met again further up has nothing left to give.
    QHash<Key, uchar> value;
    QSet<quint32> seen;
    for (int k = index - 1; k >= 0; --k)
    {
        if (m_chaser->isStepSkipped(k))
            continue;
        const QList<quint32> leaves = leavesOf(steps.at(k), false);
        for (int i = leaves.count() - 1; i >= 0; --i)
        {
            const quint32 sid = leaves.at(i);
            if (seen.contains(sid))
                continue;
            seen.insert(sid);
            for (const QPair<Key, uchar> &kv : output(sid))
                if (!own.contains(kv.first) && !value.contains(kv.first))
                    value.insert(kv.first, kv.second);
        }
    }

    // And what the steps played since the list started set, or a restack
    // before this one put back, that none of those does: back to what it
    // was when the list started. (Nothing else of the list has touched
    // the stage.)
    QSet<Key> needed(value.keyBegin(), value.keyEnd());
    for (const Key &key : std::as_const(m_touched))
        if (!own.contains(key))
            needed.insert(key);
    seen.clear();
    for (int k : std::as_const(m_visited))
    {
        // Steps played after the target, and skipped ones played before it
        // (a jump): from the top, neither has set anything yet.
        if (k == index || k >= steps.count() || (k < index && !m_chaser->isStepSkipped(k)))
            continue;
        for (quint32 sid : leavesOf(steps.at(k), true))
        {
            if (seen.contains(sid))
                continue;
            seen.insert(sid);
            for (const QPair<Key, uchar> &kv : output(sid))
                if (!own.contains(kv.first))
                    needed.insert(kv.first);
        }
    }

    m_touched.unite(needed);

    // Under everything, until the next step change. A channel no step
    // before the target sets goes back to what it was when the list
    // started.
    QHash<int, QSharedPointer<GenericFader> > faders;
    for (const Key &key : std::as_const(needed))
    {
        const Fixture *fxi = m_doc->fixture(key.first);
        if (fxi == NULL || fxi->universe() >= quint32(universes.count()) || key.second >= fxi->channels())
            continue;
        const quint32 address = fxi->address() + key.second;
        const int uni = int(fxi->universe());
        Universe *universe = universes.at(uni);
        if (universe == NULL || address >= UNIVERSE_SIZE)
            continue;
        // HTP intensity starts from zero every frame: nothing stays on it.
        FadeChannel probe(m_doc, key.first, key.second);
        if (probe.flags() & FadeChannel::HTP)
            continue;

        uchar v = 0;
        if (value.contains(key))
            v = value.value(key);
        else if (uni < m_startStage.count() && int(address) < m_startStage.at(uni).size())
            v = uchar(m_startStage.at(uni).at(int(address)));

        QSharedPointer<GenericFader> fader = faders.value(uni);
        if (fader.isNull())
        {
            fader = universe->requestFader(Universe::Tracked);
            fader->setName(QStringLiteral("%1 (tracked)").arg(m_chaser->name()));
            faders.insert(uni, fader);
            m_restackFaders << qMakePair(universe, fader);
        }
        GenericFader::ChannelRef fc = fader->getChannelFader(m_doc, universe, key.first, key.second);
        fc->setStart(quint32(v));
        fc->setCurrent(quint32(v));
        fc->setTarget(quint32(v));
        fc->setFadeTime(0);
        fc->setReady(true);
    }
}

void ChaserRunner::dismissRestack()
{
    for (const QPair<Universe *, QSharedPointer<GenericFader> > &uf : std::as_const(m_restackFaders))
        if (uf.first != NULL)
            uf.first->dismissFader(uf.second);
    m_restackFaders.clear();
}

void ChaserRunner::raiseMembers(const Function *f, QList<Universe *> universes, int depth)
{
    const Collection *coll = qobject_cast<const Collection *>(f);
    if (coll == NULL || depth > 8)
        return;
    for (quint32 fid : coll->functions())
    {
        Function *member = m_doc->function(fid);
        if (member == NULL || !member->isRunning())
            continue;
        if (Scene *scene = qobject_cast<Scene *>(member))
            scene->raiseFaders(universes);
        else
            raiseMembers(member, universes, depth + 1);
    }
}

void ChaserRunner::stopStepFunction(ChaserRunnerStep *step, bool preserveAttributes)
{
    if (step == NULL || step->m_function == NULL)
        return;
    // Stopped first, then let go of: stopped, it is never written again, so
    // it can't run on by its own timing for a tick in between.
    step->m_function->stop(functionParent(), preserveAttributes);
    if (step->m_subCues)
    {
        Chaser *inner = qobject_cast<Chaser *>(step->m_function);
        if (inner != NULL)
        {
            inner->setSubCueHeld(false);
            inner->setSubCueFollow(false);
        }
    }
}

void ChaserRunner::applySubAction(const ChaserAction &action)
{
    ChaserRunnerStep *current = currentIndexStep();
    if (current == NULL || current->m_function == NULL)
        return;
    Chaser *inner = qobject_cast<Chaser *>(current->m_function);
    if (inner == NULL)
        return;

    int target = -1;
    switch (action.m_action)
    {
        case ChaserSubSetStep:
            // A jump: to any chase step's sub-cue (one on its own timing
            // carries on from there).
            if (action.m_stepIndex != current->m_index)
                return;
            target = action.m_subStepIndex;
        break;
        case ChaserSubNextStep:
        case ChaserSubPreviousStep:
        {
            // Sub GO / Sub Back: only for a step held as sub-cues; at the
            // ends, nothing.
            if (current->m_subCues == false)
                return;
            int delta = inner->direction() == Function::Backward ? -1 : 1;
            if (action.m_action == ChaserSubPreviousStep)
                delta = -delta;
            target = inner->nextPlayableStep(inner->currentStepIndex(), delta);
        }
        break;
        default:
            return;
    }

    if (target < 0 || target >= inner->stepsCount())
        return;

    ChaserAction a;
    a.m_action = ChaserSetStepIndex;
    a.m_stepIndex = target;
    a.m_masterIntensity = inner->getAttributeValue(Function::Intensity);
    a.m_stepIntensity = 1.0;
    a.m_fadeMode = Chaser::FromFunction;
    inner->setAction(a);
}

void ChaserRunner::startNewStep(int index, MasterTimer *timer, qreal mIntensity, qreal sIntensity,
                                int fadeControl, quint32 elapsed)
{
    if (m_chaser == NULL)
        return;

    // Single snapshot of the step list — bound-check and index against the SAME
    // copy so the GUI thread cannot shrink it between the check and the .at().
    const QList<ChaserStep> steps = m_chaser->steps();
    if (steps.isEmpty())
        return;

    if (index < 0 || index >= steps.count())
        index = 0; // fallback to the first step

    ChaserStep step(steps.at(index));
    Function *func = m_doc->function(step.fid);
    if (func == NULL)
        return;

    // The same chase as a step just left (consecutive cues on one chase):
    // it is stopped first, as before the hand-over held sub-cue chases, so
    // it starts again here (beginRestart) rather than being stopped after.
    foreach (ChaserRunnerStep *leaving, m_leavingSteps)
    {
        if (leaving->m_function != func)
            continue;
        m_leavingSteps.removeOne(leaving);
        stopStepFunction(leaving, m_chaser->type() == Function::SequenceType);
        delete leaving;
    }

    ChaserRunnerStep *newStep = new ChaserRunnerStep();
    newStep->m_index = index;
    newStep->m_subCues = false;

    // check if blending between Scenes is needed
    if (m_lastFunctionID != Function::invalidId() &&
        func->type() == Function::SceneType)
    {
        Scene *scene = qobject_cast<Scene *>(func);
        scene->setBlendFunctionID(m_lastFunctionID);
    }

    // this happens only during crossfades
    if (m_runnerSteps.count())
    {
        ChaserRunnerStep *lastStep = m_runnerSteps.last();
        if (lastStep->m_function &&
            lastStep->m_function->type() == Function::SceneType &&
            func->type() == Function::SceneType)
        {
            Scene *lastScene = qobject_cast<Scene *>(lastStep->m_function);
            lastScene->setBlendFunctionID(Function::invalidId());
            Scene *scene = qobject_cast<Scene *>(func);
            scene->setBlendFunctionID(lastStep->m_function->id());
        }
    }

    switch (fadeControl)
    {
        case Chaser::FromFunction:
            newStep->m_fadeIn = stepFadeIn(index);
            newStep->m_fadeOut = stepFadeOut(index);
        break;
        case Chaser::Blended:
            newStep->m_fadeIn = stepFadeIn(index);
            newStep->m_fadeOut = stepFadeOut(index);
        break;
        case Chaser::Crossfade:
            newStep->m_fadeIn = 0;
            newStep->m_fadeOut = 0;
        break;
        case Chaser::BlendedCrossfade:
            newStep->m_fadeIn = 0;
            newStep->m_fadeOut = 0;
        break;
    }

    newStep->m_duration = stepDuration(index);

    if (m_startOffset != 0)
        newStep->m_elapsed = m_startOffset + MasterTimer::tick();
    else
        newStep->m_elapsed = MasterTimer::tick() + elapsed;
    newStep->m_elapsedBeats = 0; //(newStep->m_elapsed / timer->beatTimeDuration()) * 1000;

    m_startOffset = 0;

    newStep->m_function = func;

    if (m_chaser->type() == Function::SequenceType)
    {
        Scene *s = qobject_cast<Scene*>(func);
        // blind == true is a workaround to reuse the same scene
        // without messing up the previous values
        for (int i = 0; i < step.values.count(); i++)
            s->setValue(step.values.at(i), true);
    }

    qDebug() << "[ChaserRunner] Starting step" << index << "fade in" << newStep->m_fadeIn
             << "fade out" << newStep->m_fadeOut << "intensity" << mIntensity
             << "fadeMode" << fadeControl;

    // Set intensity before starting the function. Otherwise the intensity
    // might momentarily jump too high.
    if (func->type() == Function::SceneType)
    {
        Scene *scene = qobject_cast<Scene *>(func);
        newStep->m_intensityOverrideId = func->requestAttributeOverride(Function::Intensity, sIntensity);
        newStep->m_pIntensityOverrideId = scene->requestAttributeOverride(Scene::ParentIntensity, mIntensity);
        qDebug() << "[ChaserRunner] Set step intensity:" << sIntensity << ", master:" << mIntensity;
    }
    else
    {
        newStep->m_intensityOverrideId = func->requestAttributeOverride(Function::Intensity, mIntensity * sIntensity);
    }

    // Propagate the chaser's fader priority to the step (the Show timeline
    // lowers a chaser to Background so the VC overrides it; steps must inherit).
    func->setFadePriority(m_chaser->fadePriority());
    // ...and its speed master: the step's own, else the chaser's.
    func->setInheritedSpeedMaster(step.speedMaster != SpeedMasters::Inherit
                                  ? step.speedMaster : m_chaser->effectiveSpeedMaster());

    /* A chase step held as sub-cues (SUBCUES_DESIGN.md): the inner chase
       starts on its first playable step and holds there until a Sub GO.
       Its fades are its own steps', not this step's. */
    uint fadeIn = newStep->m_fadeIn, fadeOut = newStep->m_fadeOut;
    Chaser *inner = step.subCues != ChaserStep::Auto ? qobject_cast<Chaser *>(func) : NULL;
    if (inner != NULL)
    {
        newStep->m_subCues = true;
        // Just stopped by the step before (the same chase): its next run
        // gets what's asked here, not the run being torn down.
        inner->beginRestart();
        inner->setSubCueHeld(true);
        inner->setSubCueFollow(step.subCues == ChaserStep::SubGoFollow);
        ChaserAction first;
        first.m_action = ChaserSetStepIndex;
        first.m_stepIndex = inner->firstPlayableStep();
        first.m_masterIntensity = inner->getAttributeValue(Function::Intensity);
        first.m_stepIntensity = 1.0;
        first.m_fadeMode = Chaser::FromFunction;
        if (first.m_stepIndex >= 0)
            inner->setAction(first);
        fadeIn = Function::defaultSpeed();
        fadeOut = Function::defaultSpeed();
    }

    // Start the fire up!
    func->start(timer, functionParent(), 0, fadeIn, fadeOut,
                func->defaultSpeed(), m_chaser->tempoType());
    m_runnerSteps.append(newStep);
    m_roundTime->restart();
}

int ChaserRunner::getNextStepIndex()
{
    // Next, Back and auto-follow pass over skipped steps (ChaserStep::skip),
    // stepping on from each in the same way; all skipped: none.
    const int from = m_lastRunStepIdx;
    int next = nextStepIndexIgnoringSkips();
    for (int guard = 0; next >= 0 && m_chaser->isStepSkipped(next)
                        && guard < 2 * m_chaser->stepsCount(); ++guard)
    {
        m_lastRunStepIdx = next;
        next = nextStepIndexIgnoringSkips();
    }
    m_lastRunStepIdx = from;
    return next >= 0 && m_chaser->isStepSkipped(next) ? -1 : next;
}

int ChaserRunner::nextStepIndexIgnoringSkips()
{
    int currentStepIndex = m_lastRunStepIdx;

    if (m_chaser->runOrder() == Function::Random)
    {
        currentStepIndex = m_order.indexOf(currentStepIndex);
        if (currentStepIndex == -1)
        {
            qDebug() << "[ChaserRunner] steps order not found";
            currentStepIndex = m_lastRunStepIdx;
        }
    }

    if (currentStepIndex == -1 &&
        m_chaser->direction() == Function::Backward)
            currentStepIndex = m_chaser->stepsCount();

    // Handle reverse Ping Pong at boundaries
    if (m_chaser->runOrder() == Function::PingPong &&
        m_pendingAction.m_action == ChaserPreviousStep)
    {
        if (currentStepIndex == 0)
            m_direction = Function::Backward;
        else if (currentStepIndex == m_chaser->stepsCount() - 1)
            m_direction = Function::Forward;
    }

    // Next step
    if (m_direction == Function::Forward)
    {
        // "Previous" for a forward chaser is -1
        if (m_pendingAction.m_action == ChaserPreviousStep)
            currentStepIndex--;
        else
            currentStepIndex++;
    }
    else
    {
        // "Previous" for a backward scene is +1
        if (m_pendingAction.m_action == ChaserPreviousStep)
            currentStepIndex++;
        else
            currentStepIndex--;
    }

    if (currentStepIndex < m_chaser->stepsCount() && currentStepIndex >= 0)
    {
        if (m_chaser->runOrder() == Function::Random)
        {
            currentStepIndex = randomStepIndex(currentStepIndex);
        }
        return currentStepIndex; // In the middle of steps. No need to go any further.
    }

    if (m_chaser->runOrder() == Function::SingleShot)
    {
        return -1; // Forward or Backward SingleShot has been completed.
    }
    else if (m_chaser->runOrder() == Function::Loop)
    {
        if (m_direction == Function::Forward)
        {
            if (currentStepIndex >= m_chaser->stepsCount())
                currentStepIndex = 0;
            else
                currentStepIndex = m_chaser->stepsCount() - 1; // Used by CueList with manual prev
        }
        else // Backward
        {
            if (currentStepIndex < 0)
                currentStepIndex = m_chaser->stepsCount() - 1;
            else
                currentStepIndex = 0;
        }
    }
    else if (m_chaser->runOrder() == Function::Random)
    {
        fillOrder();
        if (m_direction == Function::Forward)
        {
            if (currentStepIndex >= m_chaser->stepsCount())
                currentStepIndex = 0;
            else
                currentStepIndex = m_chaser->stepsCount() - 1; // Used by CueList with manual prev
        }
        else // Backward
        {
            if (currentStepIndex < 0)
                currentStepIndex = m_chaser->stepsCount() - 1;
            else
                currentStepIndex = 0;
        }
        // Don't run the same function 2 times in a row
        while (currentStepIndex < m_chaser->stepsCount()
                && randomStepIndex(currentStepIndex) == m_lastRunStepIdx)
            ++currentStepIndex;
        currentStepIndex = randomStepIndex(currentStepIndex);
    }
    else // Ping Pong
    {
        // Change direction, but don't run the first/last step twice.
        if (m_direction == Function::Forward)
        {
            currentStepIndex = m_chaser->stepsCount() - 2;
            m_direction = Function::Backward;
        }
        else // Backwards
        {
            currentStepIndex = 1;
            m_direction = Function::Forward;
        }

        // Make sure we don't go beyond limits.
        currentStepIndex = CLAMP(currentStepIndex, 0, m_chaser->stepsCount() - 1);
    }

    return currentStepIndex;
}

void ChaserRunner::setPause(bool enable, QList<Universe *> universes)
{
    // Nothing to do
    if (m_chaser->stepsCount() == 0)
        return;

    qDebug() << "[ChaserRunner] processing pause request:" << enable;

    foreach (ChaserRunnerStep *step, m_runnerSteps)
        step->m_function->setPause(enable);

    // there might be a Scene fading out, so request pause
    // to faders bound to the Scene ID running on universes
    Function *f = m_doc->function(m_lastFunctionID);
    if (f != NULL && f->type() == Function::SceneType)
    {
        foreach (Universe *universe, universes)
            universe->setFaderPause(m_lastFunctionID, enable);
    }
}

FunctionParent ChaserRunner::functionParent() const
{
    return FunctionParent(FunctionParent::Function, m_chaser->id());
}

double ChaserRunner::goClockUnits() const
{
    double pos = 0, bpm = 0;
    const int master = m_chaser->effectiveSpeedMaster();
    if (!(SpeedMasters::isMaster(master) && m_doc->speedMasters()->clockFor(master, &pos, &bpm)))
        pos = m_doc->masterTimer() != nullptr ? m_doc->masterTimer()->beatPosition() : 0.0;
    return m_chaser->goQuantize() == Chaser::GoOnBar ? pos / 4.0 : pos;
}

bool ChaserRunner::write(MasterTimer *timer, QList<Universe *> universes)
{
    // Nothing to do
    if (m_chaser->stepsCount() == 0)
        return false;

    /* GO on the beat (PLAYBACK_TIMING_DESIGN.md, P1): a step change waits for
       the next whole beat or bar of the chaser's clock. The chase carries on
       as it is meanwhile; a second GO in the same beat replaces the first
       rather than adding a step. Never for a show-driven chaser -- the
       timeline says when. */
    ChaserAction held;
    held.m_action = ChaserNoAction;
    const bool stepChange = m_pendingAction.m_action == ChaserNextStep
                            || m_pendingAction.m_action == ChaserPreviousStep
                            || (m_pendingAction.m_action == ChaserSetStepIndex
                                && m_pendingAction.m_stepIndex != -1);
    if (stepChange && m_chaser->goQuantize() != Chaser::GoNow && !m_externalClock && !m_subCueHeld)
    {
        const qint64 unit = qint64(std::floor(goClockUnits()));
        // (Its own hold only: goHeld() counts a held Sub GO too.)
        if (m_goHeldIn == std::numeric_limits<qint64>::min())
            m_goHeldIn = unit;
        if (unit <= m_goHeldIn)
        {
            held = m_pendingAction;                 // not yet: run on as we are
            m_pendingAction.m_action = ChaserNoAction;
        }
        else
        {
            m_goHeldIn = std::numeric_limits<qint64>::min();
        }
    }
    else if (!stepChange)
    {
        m_goHeldIn = std::numeric_limits<qint64>::min();
    }

    // A cue handed over a write ago: its successor has claimed what they
    // share by now, so it can go.
    stopHandedOverSteps();

    // The cue started lately: its members in its order, once they have
    // started (a member kept from the cue before is otherwise under them).
    if (m_raiseFor > 0)
    {
        --m_raiseFor;
        if (const ChaserRunnerStep *now = currentIndexStep())
            raiseMembers(now->m_function, universes);
    }

    switch (m_pendingAction.m_action)
    {
        case ChaserNextStep:
        case ChaserPreviousStep:
            clearRunningList(true);
            // the actual action will be performed below, on startNewStep
        break;
        case ChaserSetStepIndex:
            if (m_pendingAction.m_stepIndex != -1)
            {
                clearRunningList(true);
                if (m_chaser->runOrder() == Function::Random && !m_subCueHeld)
                    m_lastRunStepIdx = randomStepIndex(m_pendingAction.m_stepIndex);
                else
                    m_lastRunStepIdx = m_pendingAction.m_stepIndex;

                qDebug() << "[ChaserRunner] Starting from step" << m_lastRunStepIdx << "@ offset" << m_startOffset;
                startNewStep(m_lastRunStepIdx, timer, m_pendingAction.m_masterIntensity,
                             m_pendingAction.m_stepIntensity, m_pendingAction.m_fadeMode);
                stepStarted(m_lastRunStepIdx, universes, true);
                emit currentStepChanged(m_lastRunStepIdx);
            }
        break;
        case ChaserPauseRequest:
            setPause(m_pendingAction.m_fadeMode ? true : false, universes);
        break;
        default:
        break;
    }

    quint32 prevStepRoundElapsed = 0;

    /* A beat: the main clock's -- or, for a chase on a speed master, that
       master's (SPEED_SPREAD_DESIGN.md, S2), so the chase follows its tap
       and rate and holds while it is paused. */
    bool beat = false;
    if (m_chaser->tempoType() == Function::Beats)
    {
        const int master = m_chaser->effectiveSpeedMaster();
        double pos = 0, bpm = 0;
        if (SpeedMasters::isMaster(master) && m_doc->speedMasters()->clockFor(master, &pos, &bpm))
        {
            const qint64 index = qint64(std::floor(pos));
            if (m_masterBeat == std::numeric_limits<qint64>::min() || index < m_masterBeat)
                m_masterBeat = index;           // counting from here
            else if (index > m_masterBeat)
            {
                m_masterBeat = index;
                beat = true;
            }
        }
        else
        {
            beat = timer->isBeat();
        }
    }

    foreach (ChaserRunnerStep *step, m_runnerSteps)
    {
        if (beat)
        {
            step->m_elapsedBeats += 1000;
            qDebug() << "[ChaserRunner] Function" << step->m_function->name() << "duration:" << step->m_duration << "beats:" << step->m_elapsedBeats;
        }

        // External-clock (show-driven) chasers never auto-advance on their own
        // elapsed time: the ShowRunner steps them from the show/timecode clock,
        // and infinite-hold cues stay manual-GO barriers. Only explicit actions
        // (SetStepIndex / Next / Previous, handled above) change the step.
        // Held as sub-cues, likewise: a step changes on a Sub GO -- unless
        // it auto-follows (SubGoFollow, a finite hold).
        if (m_externalClock == false && (m_subCueHeld == false || m_subCueFollow) &&
            step->m_duration != Function::infiniteSpeed() &&
            ((m_chaser->tempoType() == Function::Time && step->m_elapsed >= step->m_duration) ||
             (m_chaser->tempoType() == Function::Beats && step->m_elapsedBeats >= step->m_duration)))
        {
            if (step->m_duration != 0)
                prevStepRoundElapsed = step->m_elapsed % step->m_duration;

            m_lastFunctionID = step->m_function->type() == Function::SceneType ? step->m_function->id() : Function::invalidId();
            m_runnerSteps.removeOne(step);
            if (step->m_function->type() == Function::CollectionType || step->m_subCues)
            {
                // Auto-follow into the next cue: hand over as GO does.
                m_leavingSteps.append(step);
                continue;
            }
            stopStepFunction(step, m_chaser->type() == Function::SequenceType);
            delete step;
        }
        else
        {
            if (step->m_elapsed < UINT_MAX)
                step->m_elapsed += MasterTimer::tick();

            // When the speeds of the chaser change, they need to be updated to the lower
            // level (only current function) as well. Otherwise the new speeds would take
            // effect only on the next step change.
            if (m_updateOverrideSpeeds == true)
            {
                m_updateOverrideSpeeds = false;
                if (step->m_function != NULL && step->m_subCues == false)
                {
                    step->m_function->setOverrideFadeInSpeed(step->m_fadeIn);
                    step->m_function->setOverrideFadeOutSpeed(step->m_fadeOut);
                }
            }
        }
    }

    if (m_runnerSteps.isEmpty() && held.m_action != ChaserNoAction)
    {
        // Nothing playing yet (a GO from stopped): the first step lands on
        // the beat too, rather than starting now and jumping on the beat.
        leaveSteps();
        m_pendingAction = held;
        return true;
    }

    if (m_runnerSteps.isEmpty())
    {
        m_lastRunStepIdx = getNextStepIndex();
        if (m_lastRunStepIdx != -1)
        {
            int blend = m_pendingAction.m_action == ChaserNoAction ? Chaser::FromFunction : m_pendingAction.m_fadeMode;

            startNewStep(m_lastRunStepIdx, timer, m_pendingAction.m_masterIntensity,
                         m_pendingAction.m_stepIntensity, blend, prevStepRoundElapsed);
            stepStarted(m_lastRunStepIdx, universes, false);
            emit currentStepChanged(m_lastRunStepIdx);
        }
        else
        {
            leaveSteps();
            m_pendingAction.m_action = ChaserNoAction;
            return false;
        }
    }
    leaveSteps();

    /* Sub GO / Sub Back / a jump to a sub-cue: after the step change, so a
       jump's parent step has started. A step change that landed drops a Sub
       GO or Back asked before it (they were for the cue it left). GO on the
       beat applies as to GO, from this chase's setting. */
    if (m_pendingSubAction.m_action != ChaserNoAction)
    {
        const bool stepLanded = stepChange && held.m_action == ChaserNoAction;
        const bool aJump = m_pendingSubAction.m_action == ChaserSubSetStep;
        bool apply = true;
        if (held.m_action != ChaserNoAction)
        {
            apply = false;                          // waits with the step change
        }
        else if (stepLanded && !aJump)
        {
            m_pendingSubAction.m_action = ChaserNoAction;
            m_subGoHeldIn = std::numeric_limits<qint64>::min();
            apply = false;
        }
        else if (!stepLanded && m_chaser->goQuantize() != Chaser::GoNow
                 && !m_externalClock && !m_subCueHeld)
        {
            const qint64 unit = qint64(std::floor(goClockUnits()));
            if (m_subGoHeldIn == std::numeric_limits<qint64>::min())
                m_subGoHeldIn = unit;
            apply = unit > m_subGoHeldIn;
        }
        if (apply)
        {
            m_subGoHeldIn = std::numeric_limits<qint64>::min();
            applySubAction(m_pendingSubAction);
            m_pendingSubAction.m_action = ChaserNoAction;
        }
    }

    m_pendingAction.m_action = ChaserNoAction;
    if (held.m_action != ChaserNoAction && m_pendingAction.m_action == ChaserNoAction)
        m_pendingAction = held;                     // still waiting for the beat
    return true;
}

void ChaserRunner::postRun(MasterTimer *timer, QList<Universe*> universes)
{
    Q_UNUSED(universes);
    Q_UNUSED(timer);

    qDebug() << Q_FUNC_INFO;
    clearRunningList();
}
