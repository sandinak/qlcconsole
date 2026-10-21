/*
  Q Light Controller Plus
  vccuerunner.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QVBoxLayout>
#include <QDebug>

#include "vccuerunnerproperties.h"
#include "showtreecontroller.h"
#include "qlcinputsource.h"
#include "vccuerunner.h"
#include "doc.h"

using namespace ShowTree;

#define HYSTERESIS 3 // as a VC Cue List's: a fader must fall to 0 between hits

VCCueRunner::VCCueRunner(QWidget *parent, Doc *doc)
    : VCWidget(parent, doc)
    , m_runner(new CueRunner(doc, CueRunner::Compact, this))
{
    setObjectName(VCCueRunner::staticMetaObject.className());
    setType(VCWidget::CueRunnerWidget);
    setCaption(QString());
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(2, 2, 2, 2);
    v->addWidget(m_runner);
    for (int a = 0; a < ActionCount; ++a)
    {
        m_latestValue[a] = 0;
        m_lastFeedback[a] = -1;
    }
    resize(QSize(300, 160));
    connect(m_runner, &CueRunner::followChanged, this, [this]() { m_doc->setModified(); });
    connect(m_runner, &CueRunner::layoutEdited, this, [this]() { m_doc->setModified(); });
    connect(m_runner, &CueRunner::statesChanged, this, [this]() { sendActionFeedback(false); });
    syncMinimumSize();
    slotModeChanged(m_doc->mode());
}

VCCueRunner::~VCCueRunner()
{
}

void VCCueRunner::setID(quint32 id)
{
    VCWidget::setID(id);
    if (caption().isEmpty())
        setCaption(tr("Cue Runner %1").arg(id));
}

quint32 VCCueRunner::follow() const
{
    return m_runner->follow();
}

void VCCueRunner::setFollow(quint32 listId)
{
    m_runner->setFollow(listId);
}

CueRunner::Size VCCueRunner::runnerSize() const
{
    return m_runner->runnerSize();
}

void VCCueRunner::setRunnerSize(CueRunner::Size size)
{
    if (size == CueRunner::Full)
        size = CueRunner::List;             // full is the Run tab's
    m_runner->setRunnerSize(size);
    syncMinimumSize();
    enableWidgetUI(mode() == Doc::Operate && !isDisabled());
}

void VCCueRunner::syncMinimumSize()
{
    const QMargins m = layout()->contentsMargins();
    setMinimumSize(m_runner->minimumSize() + QSize(m.left() + m.right(), m.top() + m.bottom()));
}

/*********************************************************************
 * Actions: external input, keys, feedback
 *********************************************************************/

QString VCCueRunner::actionName(Action action)
{
    switch (action)
    {
        case Go: return tr("GO");
        case SubGo: return tr("Sub GO");
        case Back: return tr("Back");
        case SubBack: return tr("Sub Back");
        case Pause: return tr("Pause / Resume");
        case Stop: return tr("Stop");
        case LoadNext: return tr("Load next");
        case LoadPrevious: return tr("Load previous");
        default: return QString();
    }
}

QString VCCueRunner::actionTag(Action action)
{
    switch (action)
    {
        case Go: return QStringLiteral("Go");
        case SubGo: return QStringLiteral("SubGo");
        case Back: return QStringLiteral("Back");
        case SubBack: return QStringLiteral("SubBack");
        case Pause: return QStringLiteral("Pause");
        case Stop: return QStringLiteral("Stop");
        case LoadNext: return QStringLiteral("LoadNext");
        case LoadPrevious: return QStringLiteral("LoadPrevious");
        default: return QString();
    }
}

QKeySequence VCCueRunner::keySequence(Action action) const
{
    return action >= 0 && action < ActionCount ? m_keys[action] : QKeySequence();
}

void VCCueRunner::setKeySequence(Action action, const QKeySequence &keySequence)
{
    if (action >= 0 && action < ActionCount)
        m_keys[action] = keySequence.isEmpty() ? QKeySequence() : stripKeySequence(keySequence);
}

void VCCueRunner::trigger(Action action)
{
    switch (action)
    {
        case Go: m_runner->go(); break;
        case SubGo: m_runner->subGo(); break;
        case Back: m_runner->back(); break;
        case SubBack: m_runner->subBack(); break;
        case Pause: m_runner->pauseResume(); break;
        case Stop: m_runner->stop(); break;
        case LoadNext: m_runner->loadNext(); break;
        case LoadPrevious: m_runner->loadPrevious(); break;
        default: break;
    }
}

bool VCCueRunner::feedbackOn(Action action) const
{
    switch (action)
    {
        case Go: return m_runner->canGo();
        case SubGo: return m_runner->canSubGo();
        case Back: return m_runner->canBack();
        case SubBack: return m_runner->canSubBack();
        case Pause: return m_runner->isPaused();
        case Stop: return m_runner->canStop();
        case LoadNext: return m_runner->canLoadNext();
        case LoadPrevious: return m_runner->canLoadPrevious();
        default: return false;
    }
}

void VCCueRunner::updateFeedback()
{
    sendActionFeedback(true);
}

void VCCueRunner::sendActionFeedback(bool force)
{
    if (acceptsInput() == false)
        return;
    for (int a = 0; a < ActionCount; ++a)
    {
        QSharedPointer<QLCInputSource> src = inputSource(quint8(a));
        if (src.isNull() || !src->isValid())
            continue;
        const QLCInputFeedback::FeedbackType type = feedbackOn(Action(a)) ? QLCInputFeedback::UpperValue
                                                                          : QLCInputFeedback::LowerValue;
        const int value = src->feedbackValue(type);
        if (!force && value == m_lastFeedback[a])
            continue;
        m_lastFeedback[a] = value;
        sendFeedback(value, src, src->feedbackExtraParams(type));
    }
}

void VCCueRunner::slotKeyPressed(const QKeySequence &keySequence)
{
    if (acceptsInput() == false || keySequence.isEmpty())
        return;
    for (int a = 0; a < ActionCount; ++a)
    {
        if (m_keys[a] == keySequence)
        {
            trigger(Action(a));
            return;
        }
    }
}

void VCCueRunner::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    /* Don't let input data through in design mode or if disabled */
    if (acceptsInput() == false)
        return;

    const quint32 pagedCh = (page() << 16) | channel;
    for (int a = 0; a < ActionCount; ++a)
    {
        if (!checkInputSource(universe, pagedCh, value, sender(), quint8(a)))
            continue;
        // A press fires once: from a fader, the value has to fall back to
        // zero before the next non-zero value counts (as a VC Cue List).
        uchar &latest = m_latestValue[a];
        if (latest == 0 && value > 0)
        {
            trigger(Action(a));
            latest = value;
        }
        else if (latest > HYSTERESIS && value == 0)
        {
            latest = 0;
        }
        if (value > HYSTERESIS)
            latest = value;
        return;
    }
}

void VCCueRunner::enableWidgetUI(bool enable)
{
    // Operate: the buttons and the list are live, the handles locked.
    // Design (and live edit): the mouse moves and sizes the widget, the
    // handles size the info and the buttons.
    m_runner->setControlsInteractive(enable);
    m_runner->setHandlesLocked(mode() != Doc::Design);
}

void VCCueRunner::slotModeChanged(Doc::Mode mode)
{
    enableWidgetUI(mode == Doc::Operate && !isDisabled());
    VCWidget::slotModeChanged(mode);
    if (mode == Doc::Operate)
        updateFeedback();
}

/*********************************************************************
 * Clipboard
 *********************************************************************/

VCWidget *VCCueRunner::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != NULL);
    VCCueRunner *r = new VCCueRunner(parent, m_doc);
    if (r->copyFrom(this) == false)
    {
        delete r;
        r = NULL;
    }
    return r;
}

bool VCCueRunner::copyFrom(const VCWidget *widget)
{
    const VCCueRunner *r = qobject_cast<const VCCueRunner *>(widget);
    if (r == NULL)
        return false;
    setRunnerSize(r->runnerSize());
    setFollow(r->follow());
    CueRunner *from = r->runner();
    m_runner->setUpcomingCount(from->upcomingCount());
    m_runner->setShowTimecode(from->showTimecode());
    m_runner->setShowSecondary(from->showSecondary());
    m_runner->setShowSubGo(from->showSubGo());
    m_runner->setButtonHeight(from->buttonHeight());
    m_runner->setGoShare(from->goShare());
    for (int a = 0; a < ActionCount; ++a)
        m_keys[a] = r->m_keys[a];
    return VCWidget::copyFrom(widget);
}

/*********************************************************************
 * Properties
 *********************************************************************/

void VCCueRunner::editProperties()
{
    VCCueRunnerProperties prop(this, m_doc);
    if (prop.exec() == QDialog::Accepted)
        m_doc->setModified();
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCCueRunner::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCCueRunner)
    {
        qWarning() << Q_FUNC_INFO << "Cue runner node not found";
        return false;
    }
    const QXmlStreamAttributes attrs = root.attributes();
    loadXMLCommon(root);
    setRunnerSize(CueRunner::stringToSize(attrs.value(KXMLQLCVCCueRunnerSize).toString()));
    bool ok = false;
    const quint32 lid = attrs.value(KXMLQLCVCCueRunnerFollow).toString().toUInt(&ok);
    setFollow(ok ? lid : invalidId());
    auto number = [&attrs](const QString &name, int fallback) {
        bool isNumber = false;
        const int n = attrs.value(name).toString().toInt(&isNumber);
        return isNumber ? n : fallback;
    };
    // Older workspaces have none of these: the defaults.
    m_runner->setUpcomingCount(number(KXMLQLCVCCueRunnerUpcoming, 2));
    m_runner->setShowTimecode(number(KXMLQLCVCCueRunnerTimecode, 1) != 0);
    m_runner->setShowSecondary(number(KXMLQLCVCCueRunnerSecondary, 1) != 0);
    m_runner->setShowSubGo(number(KXMLQLCVCCueRunnerSubGo, 1) != 0);
    m_runner->setButtonHeight(number(KXMLQLCVCCueRunnerButtonHeight, 64));
    m_runner->setGoShare(number(KXMLQLCVCCueRunnerGoShare, 70));

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            int x = 0, y = 0, w = 0, h = 0;
            bool visible = true;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else
        {
            bool known = false;
            for (int a = 0; a < ActionCount && !known; ++a)
            {
                if (root.name() != actionTag(Action(a)))
                    continue;
                known = true;
                const QString key = loadXMLSources(root, quint8(a));
                setKeySequence(Action(a), key.isEmpty() ? QKeySequence() : QKeySequence(key));
            }
            if (!known)
                root.skipCurrentElement();
        }
    }
    return true;
}

bool VCCueRunner::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);
    doc->writeStartElement(KXMLQLCVCCueRunner);
    if (follow() != invalidId())
        doc->writeAttribute(KXMLQLCVCCueRunnerFollow, QString::number(follow()));
    doc->writeAttribute(KXMLQLCVCCueRunnerSize, CueRunner::sizeToString(runnerSize()));
    doc->writeAttribute(KXMLQLCVCCueRunnerUpcoming, QString::number(m_runner->upcomingCount()));
    doc->writeAttribute(KXMLQLCVCCueRunnerTimecode, m_runner->showTimecode() ? "1" : "0");
    doc->writeAttribute(KXMLQLCVCCueRunnerSecondary, m_runner->showSecondary() ? "1" : "0");
    doc->writeAttribute(KXMLQLCVCCueRunnerSubGo, m_runner->showSubGo() ? "1" : "0");
    doc->writeAttribute(KXMLQLCVCCueRunnerButtonHeight, QString::number(m_runner->buttonHeight()));
    doc->writeAttribute(KXMLQLCVCCueRunnerGoShare, QString::number(m_runner->goShare()));
    saveXMLCommon(doc);
    saveXMLWindowState(doc);
    saveXMLAppearance(doc);

    /* Each action's key and input, as a VC Cue List's */
    for (int a = 0; a < ActionCount; ++a)
    {
        QSharedPointer<QLCInputSource> src = inputSource(quint8(a));
        const bool hasInput = !src.isNull() && src->isValid();
        if (m_keys[a].isEmpty() && !hasInput)
            continue;
        doc->writeStartElement(actionTag(Action(a)));
        if (!m_keys[a].isEmpty())
            doc->writeTextElement(KXMLQLCVCWidgetKey, m_keys[a].toString());
        if (hasInput)
            saveXMLInput(doc, src);
        doc->writeEndElement();
    }
    doc->writeEndElement();
    return true;
}
