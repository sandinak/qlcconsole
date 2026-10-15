/*
  Q Light Controller Plus
  chaser.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QDebug>
#include <QColor>
#include <QFile>

#include "chaserrunner.h"
#include "mastertimer.h"
#include "chaserstep.h"
#include "function.h"
#include "chaser.h"
#include "doc.h"
#include "bus.h"

#define KXMLQLCChaserSpeedModeCommon  QStringLiteral("Common")
#define KXMLQLCChaserSpeedModePerStep QStringLiteral("PerStep")
#define KXMLQLCChaserSpeedModeDefault QStringLiteral("Default")

#define KXMLQLCChaserLegacySequence QStringLiteral("Sequence")

/*****************************************************************************
 * Initialization
 *****************************************************************************/

Chaser::Chaser(Doc *doc)
    : Function(doc, Function::ChaserType)
    , m_legacyHoldBus(Bus::invalid())
    , m_fadeInMode(Default)
    , m_fadeOutMode(Default)
    , m_holdMode(Common)
    , m_runner(NULL)
    , m_showClocked(false)
{
    setName(tr("New Chaser"));

    // Listen to member Function removals
    connect(doc, SIGNAL(functionRemoved(quint32)),
            this, SLOT(slotFunctionRemoved(quint32)));

    m_startupAction.m_action = ChaserNoAction;
    m_startupAction.m_masterIntensity = 1.0;
    m_startupAction.m_stepIntensity = 1.0;
    m_startupAction.m_fadeMode = FromFunction;
    m_startupAction.m_stepIndex = -1;
}

Chaser::~Chaser()
{
}

QIcon Chaser::getIcon() const
{
    return QIcon(":/chaser.png");
}

/*****************************************************************************
 * Copying
 *****************************************************************************/

Function* Chaser::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    Function* copy = new Chaser(doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool Chaser::copyFrom(const Function* function)
{
    const Chaser *chaser = qobject_cast<const Chaser*> (function);
    if (chaser == NULL)
        return false;

    // Copy chaser stuff
    m_steps = chaser->m_steps;
    m_goQuantize = chaser->m_goQuantize;
    m_cueListNumber = chaser->m_cueListNumber;
    m_fadeInMode = chaser->m_fadeInMode;
    m_fadeOutMode = chaser->m_fadeOutMode;
    m_holdMode = chaser->m_holdMode;

    // Copy common function stuff
    return Function::copyFrom(function);
}

/*****************************************************************************
 * Contents
 *****************************************************************************/

bool Chaser::addStep(const ChaserStep& step, int index)
{
    if (step.fid != this->id())
    {
        {
            QMutexLocker stepListLocker(&m_stepListMutex);
            if (index < 0)
                m_steps.append(step);
            else if (index <= m_steps.size())
                m_steps.insert(index, step);
        }

        emit changed(this->id());
        emit stepsListChanged(this->id());
        return true;
    }
    else
    {
        return false;
    }
}

bool Chaser::removeStep(int index)
{
    if (index >= 0 && index < m_steps.size())
    {
        {
            QMutexLocker stepListLocker(&m_stepListMutex);
            m_steps.removeAt(index);
        }

        emit changed(this->id());
        emit stepsListChanged(this->id());
        return true;
    }
    else
    {
        return false;
    }
}

bool Chaser::replaceStep(const ChaserStep& step, int index)
{
    if (index >= 0 && index < m_steps.size())
    {
        {
            QMutexLocker stepListLocker(&m_stepListMutex);
            m_steps[index] = step;
        }

        emit changed(this->id());
        emit stepChanged(index);
        return true;
    }
    else
    {
        return false;
    }
}

bool Chaser::moveStep(int sourceIdx, int destIdx)
{
    if (sourceIdx < 0 || sourceIdx >= m_steps.size())
        return false;
    if (destIdx < 0 || destIdx >= m_steps.size() || destIdx == sourceIdx)
        return false;

    {
        QMutexLocker stepListLocker(&m_stepListMutex);
        ChaserStep cs = m_steps[sourceIdx];
        m_steps.removeAt(sourceIdx);
        m_steps.insert(destIdx, cs);
    }

    emit changed(this->id());

    return true;
}

int Chaser::stepsCount() const
{
    QMutexLocker stepListLocker(&m_stepListMutex);
    return m_steps.count();
}

ChaserStep *Chaser::stepAt(int idx)
{
    QMutexLocker stepListLocker(&m_stepListMutex);
    if (idx >= 0 && idx < m_steps.count())
        return &(m_steps[idx]);

    return NULL;
}

QList <ChaserStep> Chaser::steps() const
{
    QMutexLocker stepListLocker(&m_stepListMutex);
    return m_steps;
}

int Chaser::stepIndexAtLocalMs(quint32 localMs, quint32 untimedCueMs) const
{
    // Timer-thread hot path (show clock, every tick per running chaser): walk the
    // cumulative dwell under the step lock WITHOUT copying m_steps.
    QMutexLocker stepListLocker(&m_stepListMutex);
    const int count = m_steps.count();
    if (count == 0)
        return -1;

    const bool common = (m_holdMode == Chaser::Common);
    const quint32 commonDur = duration();   // Function::duration() — simple read
    quint32 acc = 0;
    for (int s = 0; s < count; s++)
    {
        const quint32 raw = common ? commonDur : m_steps.at(s).duration;
        const quint32 dwell =
            (raw == 0 || raw == Function::infiniteSpeed()) ? untimedCueMs : raw;
        if (localMs < acc + dwell)
            return s;
        acc += dwell;
    }
    return count - 1;               // past the last cue: clamp (block stop ends it)
}

void Chaser::setTotalDuration(quint32 msec)
{
    if (durationMode() == Chaser::Common)
    {
        int stepsCount = m_steps.count();
        if (stepsCount == 0)
            stepsCount = 1;
        setDuration(msec / stepsCount);
    }
    else
    {
        // scale all the Chaser steps to resize
        // to the desired duration
        double dtDuration = (double)totalDuration();
        for (int i = 0; i < m_steps.count(); i++)
        {
            uint origDuration = m_steps[i].duration;
            m_steps[i].duration = ((double)m_steps[i].duration * msec) / dtDuration;
            if (m_steps[i].hold)
                m_steps[i].hold = ((double)m_steps[i].hold * (double)m_steps[i].duration) / (double)origDuration;
            m_steps[i].fadeIn = m_steps[i].duration - m_steps[i].hold;
            if (m_steps[i].fadeOut)
                m_steps[i].fadeOut = ((double)m_steps[i].fadeOut * (double)m_steps[i].duration) / (double)origDuration;
        }
    }
    emit changed(this->id());
}

quint32 Chaser::totalDuration()
{
    quint32 totalDuration = 0;

    if (durationMode() == Chaser::Common)
        totalDuration = duration() * m_steps.count();
    else
    {
        foreach (ChaserStep step, m_steps)
            totalDuration += step.duration;
    }

    return totalDuration;
}

void Chaser::slotFunctionRemoved(quint32 fid)
{
    int count;
    {
        QMutexLocker stepListLocker(&m_stepListMutex);
        count = m_steps.removeAll(ChaserStep(fid));
    }

    if (count > 0)
        emit changed(this->id());
}

/*****************************************************************************
 * Speed modes
 *****************************************************************************/

void Chaser::setFadeInMode(Chaser::SpeedMode mode)
{
    m_fadeInMode = mode;
    emit changed(this->id());
}

Chaser::SpeedMode Chaser::fadeInMode() const
{
    return m_fadeInMode;
}

void Chaser::setFadeOutMode(Chaser::SpeedMode mode)
{
    m_fadeOutMode = mode;
    emit changed(this->id());
}

Chaser::SpeedMode Chaser::fadeOutMode() const
{
    return m_fadeOutMode;
}

void Chaser::setDurationMode(Chaser::SpeedMode mode)
{
    m_holdMode = mode;
    emit changed(this->id());
}

Chaser::SpeedMode Chaser::durationMode() const
{
    return m_holdMode;
}

QString Chaser::speedModeToString(Chaser::SpeedMode mode)
{
    if (mode == Common)
        return KXMLQLCChaserSpeedModeCommon;
    else if (mode == PerStep)
        return KXMLQLCChaserSpeedModePerStep;
    else
        return KXMLQLCChaserSpeedModeDefault;
}

Chaser::SpeedMode Chaser::stringToSpeedMode(const QString& str)
{
    if (str == KXMLQLCChaserSpeedModeCommon)
        return Common;
    else if (str == KXMLQLCChaserSpeedModePerStep)
        return PerStep;
    else
        return Default;
}

void Chaser::setGoQuantize(Chaser::GoQuantize q)
{
    if (q == m_goQuantize)
        return;
    m_goQuantize = q;
    emit changed(this->id());
}

QString Chaser::goQuantizeToString(Chaser::GoQuantize q)
{
    switch (q)
    {
        case GoOnBeat: return QStringLiteral("Beat");
        case GoOnBar:  return QStringLiteral("Bar");
        default:       return QStringLiteral("Now");
    }
}

Chaser::GoQuantize Chaser::stringToGoQuantize(const QString &str)
{
    if (str == QLatin1String("Beat"))
        return GoOnBeat;
    if (str == QLatin1String("Bar"))
        return GoOnBar;
    return GoNow;
}

/*****************************************************************************
 * Cues for show control
 *****************************************************************************/

void Chaser::setCueListNumber(const QString &number)
{
    const QString n = number.trimmed();
    if (n == m_cueListNumber)
        return;
    m_cueListNumber = n;
    emit changed(this->id());
}

QString Chaser::stepCue(int index) const
{
    QMutexLocker locker(&m_stepListMutex);
    if (index < 0 || index >= m_steps.count())
        return QString();
    const QString n = m_steps.at(index).cueNumber.trimmed();
    return n.isEmpty() ? QString::number(index + 1) : n;
}

int Chaser::findCue(const QString &cue) const
{
    const int count = stepsCount();
    for (int i = 0; i < count; ++i)
        if (cuesMatch(stepCue(i), cue))
            return i;
    return -1;
}

void Chaser::renumberCues()
{
    {
        QMutexLocker locker(&m_stepListMutex);
        for (int i = 0; i < m_steps.count(); ++i)
            m_steps[i].cueNumber = QString::number(i + 1);
    }
    emit changed(this->id());
}

bool Chaser::isStepSkipped(int index) const
{
    QMutexLocker locker(&m_stepListMutex);
    return index >= 0 && index < m_steps.count() && m_steps.at(index).skip;
}

void Chaser::setStepSkipped(int index, bool skip)
{
    {
        QMutexLocker locker(&m_stepListMutex);
        if (index < 0 || index >= m_steps.count() || m_steps.at(index).skip == skip)
            return;
        m_steps[index].skip = skip;
    }
    emit changed(this->id());
}

int Chaser::firstPlayableStep() const
{
    const int count = stepsCount();
    const bool back = direction() == Function::Backward;
    for (int n = 0; n < count; ++n)
    {
        const int i = back ? count - 1 - n : n;
        if (!isStepSkipped(i))
            return i;
    }
    return -1;
}

bool Chaser::cuesMatch(const QString &a, const QString &b)
{
    const QString x = a.trimmed(), y = b.trimmed();
    if (x.isEmpty() || y.isEmpty())
        return false;
    bool okX = false, okY = false;
    const double dx = x.toDouble(&okX), dy = y.toDouble(&okY);
    if (okX && okY)
        return qAbs(dx - dy) < 1e-9;
    return x.compare(y, Qt::CaseInsensitive) == 0;
}

/*****************************************************************************
 * Sub-cues (SUBCUES_DESIGN.md)
 *****************************************************************************/

Chaser *Chaser::subCueChase(int index) const
{
    quint32 fid = Function::invalidId();
    {
        QMutexLocker locker(&m_stepListMutex);
        if (index < 0 || index >= m_steps.count())
            return NULL;
        fid = m_steps.at(index).fid;
    }
    if (fid == id() || doc() == NULL)
        return NULL;
    return qobject_cast<Chaser *>(doc()->function(fid));
}

ChaserStep::SubCueMode Chaser::stepSubCues(int index) const
{
    QMutexLocker locker(&m_stepListMutex);
    if (index < 0 || index >= m_steps.count())
        return ChaserStep::Auto;
    return m_steps.at(index).subCues;
}

void Chaser::setStepSubCues(int index, ChaserStep::SubCueMode mode)
{
    {
        QMutexLocker locker(&m_stepListMutex);
        if (index < 0 || index >= m_steps.count() || m_steps.at(index).subCues == mode)
            return;
        m_steps[index].subCues = mode;
    }
    emit changed(this->id());
    emit stepChanged(index);
}

bool Chaser::isSubGoStep(int index) const
{
    return stepSubCues(index) == ChaserStep::SubGo && subCueChase(index) != NULL;
}

bool Chaser::hasSubGoSteps() const
{
    const int count = stepsCount();
    for (int i = 0; i < count; ++i)
        if (isSubGoStep(i))
            return true;
    return false;
}

int Chaser::subCueCount(int index) const
{
    const Chaser *inner = subCueChase(index);
    return inner == NULL ? 0 : inner->stepsCount();
}

QString Chaser::subCue(int index, int subIndex) const
{
    const Chaser *inner = subCueChase(index);
    if (inner == NULL)
        return QString();
    QString own;
    {
        QMutexLocker locker(&inner->m_stepListMutex);
        if (subIndex < 0 || subIndex >= inner->m_steps.count())
            return QString();
        own = inner->m_steps.at(subIndex).cueNumber.trimmed();
    }
    if (own.isEmpty() == false)
        return own;
    return QString("%1.%2").arg(stepCue(index)).arg(subIndex + 1);
}

bool Chaser::findSubCue(const QString &cue, int *index, int *subIndex) const
{
    const int count = stepsCount();
    for (int i = 0; i < count; ++i)
    {
        const int subs = subCueCount(i);
        for (int j = 0; j < subs; ++j)
        {
            if (cuesMatch(subCue(i, j), cue))
            {
                if (index != NULL)
                    *index = i;
                if (subIndex != NULL)
                    *subIndex = j;
                return true;
            }
        }
    }
    return false;
}

bool Chaser::findCueOrSubCue(const QString &cue, int *index, int *subIndex) const
{
    const int step = findCue(cue);
    if (step >= 0)
    {
        if (index != NULL)
            *index = step;
        if (subIndex != NULL)
            *subIndex = -1;
        return true;
    }
    return findSubCue(cue, index, subIndex);
}

int Chaser::currentSubCueIndex() const
{
    if (isRunning() == false)
        return -1;
    const Chaser *inner = subCueChase(currentStepIndex());
    if (inner == NULL || inner->isRunning() == false)
        return -1;
    return inner->currentStepIndex();
}

int Chaser::nextSubCueIndex() const
{
    const int current = currentSubCueIndex();
    if (current < 0 || isSubGoStep(currentStepIndex()) == false)
        return -1;
    const Chaser *inner = subCueChase(currentStepIndex());
    return inner->nextPlayableStep(current, inner->direction() == Function::Backward ? -1 : 1);
}

int Chaser::previousSubCueIndex() const
{
    const int current = currentSubCueIndex();
    if (current < 0 || isSubGoStep(currentStepIndex()) == false)
        return -1;
    const Chaser *inner = subCueChase(currentStepIndex());
    return inner->nextPlayableStep(current, inner->direction() == Function::Backward ? 1 : -1);
}

int Chaser::nextPlayableStep(int from, int delta) const
{
    if (delta == 0)
        return -1;
    const int count = stepsCount();
    for (int i = from + delta; i >= 0 && i < count; i += delta)
    {
        if (isStepSkipped(i) == false)
            return i;
    }
    return -1;
}

namespace
{
ChaserAction subAction(ChaserActionType type, int index = -1, int subIndex = -1)
{
    ChaserAction action;
    action.m_action = type;
    action.m_masterIntensity = 1.0;
    action.m_stepIntensity = 1.0;
    action.m_fadeMode = Chaser::FromFunction;
    action.m_stepIndex = index;
    action.m_subStepIndex = subIndex;
    return action;
}
}

void Chaser::subGo()
{
    ChaserAction action = subAction(ChaserSubNextStep);
    setAction(action);
}

void Chaser::subBack()
{
    ChaserAction action = subAction(ChaserSubPreviousStep);
    setAction(action);
}

void Chaser::jumpToSubCue(int index, int subIndex)
{
    ChaserAction action = subAction(ChaserSubSetStep, index, subIndex);
    setAction(action);
}

void Chaser::setSubCueHeld(bool held)
{
    QMutexLocker runnerLocker(&m_runnerMutex);
    m_subCueHeld = held;
    if (m_runner != NULL)
        m_runner->setSubCueHeld(held);
}

bool Chaser::isSubCueHeld() const
{
    QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
    return m_subCueHeld;
}

/*****************************************************************************
 * Save & Load
 *****************************************************************************/

bool Chaser::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* Function tag */
    doc->writeStartElement(KXMLQLCFunction);

    /* Common attributes */
    saveXMLCommon(doc);
    if (m_goQuantize != GoNow)
        doc->writeAttribute(QStringLiteral("GoQuantize"), goQuantizeToString(m_goQuantize));
    if (m_cueListNumber.isEmpty() == false)
        doc->writeAttribute(QStringLiteral("CueList"), m_cueListNumber);

    /* Save Tempo type */
    saveXMLTempoType(doc);

    /* Speed */
    saveXMLSpeed(doc);

    /* Direction */
    saveXMLDirection(doc);

    /* Run order */
    saveXMLRunOrder(doc);

    /* Speed modes */
    doc->writeStartElement(KXMLQLCChaserSpeedModes);
    doc->writeAttribute(KXMLQLCFunctionSpeedFadeIn, speedModeToString(fadeInMode()));
    doc->writeAttribute(KXMLQLCFunctionSpeedFadeOut, speedModeToString(fadeOutMode()));
    doc->writeAttribute(KXMLQLCFunctionSpeedDuration, speedModeToString(durationMode()));
    doc->writeEndElement();

    /* Steps */
    for (int i = 0; i < m_steps.count(); i++)
        m_steps.at(i).saveXML(doc, i, false);

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

bool Chaser::loadXMLSpeedModes(QXmlStreamReader &root)
{
    QXmlStreamAttributes attrs = root.attributes();
    QString str;

    str = attrs.value(KXMLQLCFunctionSpeedFadeIn).toString();
    setFadeInMode(stringToSpeedMode(str));

    str = attrs.value(KXMLQLCFunctionSpeedFadeOut).toString();
    setFadeOutMode(stringToSpeedMode(str));

    str = attrs.value(KXMLQLCFunctionSpeedDuration).toString();
    setDurationMode(stringToSpeedMode(str));
    root.skipCurrentElement();

    return true;
}

bool Chaser::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::ChaserType))
    {
        qWarning() << Q_FUNC_INFO << root.attributes().value(KXMLQLCFunctionType).toString()
                   << "is not a Chaser";
        return false;
    }

    m_goQuantize = stringToGoQuantize(root.attributes().value(QStringLiteral("GoQuantize")).toString());
    m_cueListNumber = root.attributes().value(QStringLiteral("CueList")).toString().trimmed();

    /* Load chaser contents */
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCBus)
        {
            m_legacyHoldBus = root.readElementText().toUInt();
        }
        else if (root.name() == KXMLQLCFunctionSpeed)
        {
            loadXMLSpeed(root);
        }
        else if (root.name() == KXMLQLCFunctionDirection)
        {
            loadXMLDirection(root);
        }
        else if (root.name() == KXMLQLCFunctionRunOrder)
        {
            loadXMLRunOrder(root);
        }
        else if (root.name() == KXMLQLCFunctionTempoType)
        {
            loadXMLTempoType(root);
        }
        else if (root.name() == KXMLQLCChaserSpeedModes)
        {
            loadXMLSpeedModes(root);
        }
        else if (root.name() == KXMLQLCFunctionStep)
        {
            //! @todo stepNumber is useless if the steps are in the wrong order
            ChaserStep step;
            int stepNumber = -1;

            if (step.loadXML(root, stepNumber, doc()) == true)
            {
                if (stepNumber >= m_steps.size())
                    m_steps.append(step);
                else
                    m_steps.insert(stepNumber, step);
            }
        }
        else if (root.name() == KXMLQLCChaserLegacySequence)
        {
            doc()->appendToErrorLog(QString("<b>Unsupported sequences found</b>. Please convert your project "
                                            "at <a href=https://www.qlcplus.org/sequence_migration.php>https://www.qlcplus.org/sequence_migration.php</a>"));
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown chaser tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

void Chaser::postLoad()
{
    if (m_legacyHoldBus != Bus::invalid())
    {
        quint32 value = Bus::instance()->value(m_legacyHoldBus);
        setDuration((value / MasterTimer::frequency()) * 1000);
    }

    Doc *doc = this->doc();
    Q_ASSERT(doc != NULL);

    QMutableListIterator <ChaserStep> it(m_steps);
    while (it.hasNext() == true)
    {
        ChaserStep step(it.next());
        Function *function = doc->function(step.fid);

        if (function == NULL)
            it.remove();
        else if (function->contains(id())) // forbid self-containment
            it.remove();
    }
}

/*****************************************************************************
 * Next/Previous
 * Protected ChaserRunner wrappers
 *****************************************************************************/

void Chaser::tap()
{
    QMutexLocker runnerLocker(&m_runnerMutex);
    if (m_runner != NULL && durationMode() == Common)
        m_runner->tap();
}

void Chaser::setAction(ChaserAction &action)
{
    QMutexLocker runnerLocker(&m_runnerMutex);
    if (m_runner != NULL)
    {
        m_runner->setAction(action);
    }
    else if (action.m_action == ChaserSubNextStep || action.m_action == ChaserSubPreviousStep)
    {
        // Not playing: no sub-cues to step through.
        return;
    }
    else
    {
        m_startupAction.m_action = action.m_action;
        m_startupAction.m_stepIndex = action.m_stepIndex;
        m_startupAction.m_masterIntensity = action.m_masterIntensity;
        m_startupAction.m_stepIntensity = action.m_stepIntensity;
        m_startupAction.m_fadeMode = action.m_fadeMode;
        m_startupAction.m_subStepIndex = action.m_subStepIndex;
    }
}

int Chaser::currentStepIndex() const
{
    int ret = m_startupAction.m_stepIndex;
    {
        QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
        if (m_runner != NULL)
            ret = m_runner->currentStepIndex();
    }
    return ret;
}

int Chaser::computeNextStep(int currentStepIndex) const
{
    int ret = m_startupAction.m_stepIndex;
    {
        QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
        if (m_runner != NULL)
            ret = m_runner->computeNextStep(currentStepIndex);
    }
    return ret;
}

int Chaser::runningStepsNumber() const
{
    int ret = 0;
    {
        QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
        if (m_runner != NULL)
            ret = m_runner->runningStepsNumber();
    }
    return ret;
}

uint Chaser::currentStepDuration() const
{
    QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
    if (m_runner == NULL)
        return 0;
    return m_runner->currentStepDuration();
}

bool Chaser::goHeld() const
{
    QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
    return m_runner != NULL && m_runner->goHeld();
}

ChaserRunnerStep Chaser::currentRunningStep() const
{
    ChaserRunnerStep ret;
    ret.m_function = NULL;

    {
        QMutexLocker runnerLocker(const_cast<QRecursiveMutex*>(&m_runnerMutex));
        if (m_runner != NULL)
        {
            ChaserRunnerStep *step = m_runner->currentRunningStep();
            if (step != NULL)
                ret = *step;
        }
    }
    return ret;
}

bool Chaser::contains(quint32 functionId) const
{
    Doc *doc = this->doc();
    Q_ASSERT(doc != NULL);

    foreach (ChaserStep step, m_steps)
    {
        Function *function = doc->function(step.fid);
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

QList<quint32> Chaser::components() const
{
    QList<quint32> ids;

    foreach (ChaserStep step, m_steps)
        ids.append(step.fid);

    return ids;
}

/*****************************************************************************
 * Running
 *****************************************************************************/

void Chaser::createRunner(quint32 startTime)
{
    Q_ASSERT(m_runner == NULL);

    {
        QMutexLocker stepListLocker(&m_stepListMutex);
        m_runner = new ChaserRunner(doc(), this, startTime);
    }
    m_runner->moveToThread(QCoreApplication::instance()->thread());
    m_runner->setParent(this);
    m_runner->setExternalClock(m_showClocked);
    m_runner->setSubCueHeld(m_subCueHeld);
    m_runner->setAction(m_startupAction);
    m_startupAction.m_action = ChaserNoAction;
}

void Chaser::preRun(MasterTimer* timer)
{
    {
        QMutexLocker runnerLocker(&m_runnerMutex);
        createRunner(elapsed());
        connect(m_runner, SIGNAL(currentStepChanged(int)), this, SIGNAL(currentStepChanged(int)));
    }

    Function::preRun(timer);
}

void Chaser::setShowClocked(bool enable)
{
    m_showClocked = enable;
    QMutexLocker runnerLocker(&m_runnerMutex);
    if (m_runner != NULL)
        m_runner->setExternalClock(enable);
}

void Chaser::setPause(bool enable)
{
    QMutexLocker runnerLocker(&m_runnerMutex);
    if (m_runner != NULL)
    {
        // request a change of pause state at the next write call
        m_startupAction.m_action = ChaserPauseRequest;
        // use fade mode to pass through enable/disable flag
        m_startupAction.m_fadeMode = enable ? 1 : 0;
    }
    Function::setPause(enable);
}

void Chaser::write(MasterTimer* timer, QList<Universe *> universes)
{
    // Take m_runnerMutex up front: m_startupAction is written by the GUI thread
    // (setPause/setAction) under this same lock, so the pause-request read/clear
    // below must hold it too — not just the m_runner->write() call.
    QMutexLocker runnerLocker(&m_runnerMutex);

    if (isPaused() && m_startupAction.m_action != ChaserPauseRequest)
        return;

    if (m_startupAction.m_action == ChaserPauseRequest)
    {
        qDebug() << "[Chaser] Request PAUSE" << m_startupAction.m_fadeMode;
        m_runner->setAction(m_startupAction);
        m_startupAction.m_action = ChaserNoAction;
    }

    {
        QMutexLocker stepListLocker(&m_stepListMutex);
        Q_ASSERT(m_runner != NULL);

        if (m_runner->write(timer, universes) == false)
            stop(FunctionParent::master());
    }

    incrementElapsed();
}

void Chaser::postRun(MasterTimer* timer, QList<Universe *> universes)
{
    {
        QMutexLocker runnerLocker(&m_runnerMutex);
        Q_ASSERT(m_runner != NULL);

        if (isPaused())
            m_runner->setPause(false, universes);

        m_runner->postRun(timer, universes);

        delete m_runner;
        m_runner = NULL;
    }

    Function::postRun(timer, universes);
}

/*****************************************************************************
 * Intensity
 *****************************************************************************/

int Chaser::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (attrIndex == Intensity)
    {
        QMutexLocker runnerLocker(&m_runnerMutex);
        QMutexLocker stepListLocker(&m_stepListMutex);
        if (m_runner != NULL)
        {
            m_runner->adjustStepIntensity(getAttributeValue(Function::Intensity));
        }
        else
        {
            m_startupAction.m_masterIntensity = getAttributeValue(Function::Intensity);
        }
    }

    return attrIndex;
}

void Chaser::adjustStepIntensity(qreal fraction, int stepIndex, FadeControlMode fadeControl)
{
    QMutexLocker runnerLocker(&m_runnerMutex);
    if (m_runner != NULL)
    {
        m_runner->adjustStepIntensity(fraction, stepIndex, fadeControl);
    }
    else
    {
        m_startupAction.m_masterIntensity = getAttributeValue(Function::Intensity);
        m_startupAction.m_stepIntensity = fraction;
    }
}
