/*
  Q Light Controller Plus
  vcshowcontrol.cpp

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
#include <QToolButton>
#include <QLabel>
#include <QApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

#include "vcshowcontrol.h"
#include "vcshowcontrolproperties.h"
#include "mastertimer.h"
#include "show.h"
#include "doc.h"

#define HYSTERESIS 3

const quint8 VCShowControl::playInputSourceId    = 0;
const quint8 VCShowControl::stopInputSourceId    = 1;
const quint8 VCShowControl::followInputSourceId  = 2;
const quint8 VCShowControl::suspendInputSourceId = 3;

VCShowControl::VCShowControl(QWidget *parent, Doc *doc)
    : VCWidget(parent, doc)
    , m_showID(Function::invalidId())
    , m_nameLabel(NULL)
    , m_statusLabel(NULL)
    , m_timeLabel(NULL)
    , m_sectionLabel(NULL)
    , m_stopButton(NULL)
    , m_playButton(NULL)
    , m_followButton(NULL)
    , m_suspendButton(NULL)
    , m_playLatest(0)
    , m_stopLatest(0)
    , m_followLatest(0)
    , m_suspendLatest(0)
{
    setObjectName(VCShowControl::staticMetaObject.className());
    setType(VCWidget::ShowControlWidget);
    setCaption("");
    resize(QSize(240, 110));

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(4, 3, 4, 3);
    v->setSpacing(2);

    // Header: bound-show name + a control-state chip.
    QHBoxLayout *head = new QHBoxLayout();
    head->setSpacing(4);
    m_nameLabel = new QLabel(tr("No show"), this);
    QFont nf = qApp->font();
    nf.setBold(true);
    m_nameLabel->setFont(nf);
    head->addWidget(m_nameLabel, 1);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    head->addWidget(m_statusLabel, 0);
    v->addLayout(head);

    // Big timecode + current section marker.
    m_timeLabel = new QLabel("00:00:00", this);
    QFont tf = qApp->font();
    tf.setBold(true);
    tf.setPixelSize(26);
    m_timeLabel->setFont(tf);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    v->addWidget(m_timeLabel);

    m_sectionLabel = new QLabel(this);
    m_sectionLabel->setAlignment(Qt::AlignCenter);
    v->addWidget(m_sectionLabel);

    // Transport row.
    QHBoxLayout *row = new QHBoxLayout();
    row->setSpacing(3);

    m_stopButton = new QToolButton(this);
    m_stopButton->setIcon(QIcon(":/player_stop.png"));
    m_stopButton->setIconSize(QSize(22, 22));
    m_stopButton->setToolTip(tr("Stop the show"));
    row->addWidget(m_stopButton);

    m_playButton = new QToolButton(this);
    m_playButton->setIcon(QIcon(":/player_play.png"));
    m_playButton->setIconSize(QSize(22, 22));
    m_playButton->setToolTip(tr("Play the show from the current position"));
    row->addWidget(m_playButton);

    m_followButton = new QToolButton(this);
    m_followButton->setIcon(QIcon(":/clock.png"));
    m_followButton->setIconSize(QSize(22, 22));
    m_followButton->setCheckable(true);
    m_followButton->setToolTip(tr("Follow MIDI Time Code"));
    row->addWidget(m_followButton);

    m_suspendButton = new QToolButton(this);
    m_suspendButton->setIcon(QIcon(":/player_pause.png"));
    m_suspendButton->setIconSize(QSize(22, 22));
    m_suspendButton->setCheckable(true);
    m_suspendButton->setToolTip(tr("Suspend the timeline — hand the rig to the "
                                   "Virtual Console (keeps the playhead)"));
    row->addWidget(m_suspendButton);
    row->addStretch(1);
    v->addLayout(row);

    connect(m_stopButton, SIGNAL(clicked()), this, SLOT(slotStopClicked()));
    connect(m_playButton, SIGNAL(clicked()), this, SLOT(slotPlayClicked()));
    connect(m_followButton, SIGNAL(clicked()), this, SLOT(slotFollowClicked()));
    connect(m_suspendButton, SIGNAL(clicked()), this, SLOT(slotSuspendClicked()));

    // Design mode: controls inert so the widget can be selected/moved.
    slotModeChanged(m_doc->mode());
    refresh();
}

VCShowControl::~VCShowControl()
{
}

void VCShowControl::setID(quint32 id)
{
    VCWidget::setID(id);
    if (caption().isEmpty())
        setCaption(tr("Show Control %1").arg(id));
}

/*********************************************************************
 * Bound Show
 *********************************************************************/

Show *VCShowControl::showFunction() const
{
    return qobject_cast<Show *>(m_doc->function(m_showID));
}

FunctionParent VCShowControl::functionParent() const
{
    // Same parent the Show Manager uses, so the two cooperate on start/stop.
    return FunctionParent::master();
}

void VCShowControl::setShow(quint32 id)
{
    Show *old = showFunction();
    if (old != NULL)
    {
        disconnect(old, SIGNAL(timeChanged(quint32)), this, SLOT(slotShowTimeChanged(quint32)));
        disconnect(old, SIGNAL(running(quint32)), this, SLOT(slotShowRunning(quint32)));
        disconnect(old, SIGNAL(stopped(quint32)), this, SLOT(slotShowStopped(quint32)));
    }

    m_showID = id;

    Show *show = showFunction();
    if (show != NULL)
    {
        connect(show, SIGNAL(timeChanged(quint32)), this, SLOT(slotShowTimeChanged(quint32)));
        connect(show, SIGNAL(running(quint32)), this, SLOT(slotShowRunning(quint32)));
        connect(show, SIGNAL(stopped(quint32)), this, SLOT(slotShowStopped(quint32)));
        if (caption().isEmpty() || caption().startsWith(tr("Show Control")))
            setCaption(show->name());
    }
    refresh();
}

/*********************************************************************
 * Transport / state
 *********************************************************************/

void VCShowControl::playShow()
{
    Show *show = showFunction();
    if (show == NULL || show->isRunning())
        return;
    show->start(m_doc->masterTimer(), functionParent());
    refresh();
}

void VCShowControl::stopShow()
{
    Show *show = showFunction();
    if (show == NULL || show->isRunning() == false)
        return;
    show->stop(functionParent());
    refresh();
}

void VCShowControl::toggleFollow()
{
    Show *show = showFunction();
    if (show == NULL)
        return;
    show->setTimecodeFollow(show->timecodeFollow() == false);
    refresh();
}

void VCShowControl::toggleSuspend()
{
    Show *show = showFunction();
    if (show == NULL || show->isRunning() == false)
        return;
    show->setTimelineSuspended(show->isTimelineSuspended() == false);
    refresh();
}

void VCShowControl::slotPlayClicked()    { playShow(); }
void VCShowControl::slotStopClicked()    { stopShow(); }
void VCShowControl::slotFollowClicked()  { toggleFollow(); }
void VCShowControl::slotSuspendClicked() { toggleSuspend(); }

void VCShowControl::slotShowTimeChanged(quint32 ms)
{
    // Timecode
    uint totalSec = ms / 1000;
    m_timeLabel->setText(QString("%1:%2:%3")
                         .arg(totalSec / 3600, 2, 10, QChar('0'))
                         .arg((totalSec % 3600) / 60, 2, 10, QChar('0'))
                         .arg(totalSec % 60, 2, 10, QChar('0')));

    // Current section marker (the region [start, end] containing the position).
    Show *show = showFunction();
    QString section;
    if (show != NULL)
    {
        QMapIterator<quint32, ShowMarker> it(show->markers());
        while (it.hasNext())
        {
            it.next();
            if (ms >= it.key() && ms < it.value().end)
            {
                section = it.value().label;
                break;
            }
        }
    }
    m_sectionLabel->setText(section);
}

void VCShowControl::slotShowRunning(quint32 id)
{
    Q_UNUSED(id)
    refresh();
}

void VCShowControl::slotShowStopped(quint32 id)
{
    Q_UNUSED(id)
    if (m_timeLabel != NULL)
        m_timeLabel->setText("00:00:00");
    if (m_sectionLabel != NULL)
        m_sectionLabel->setText(QString());
    refresh();
}

void VCShowControl::refresh()
{
    Show *show = showFunction();
    const bool running = (show != NULL && show->isRunning());
    const bool suspended = (running && show->isTimelineSuspended());
    const bool following = (show != NULL && show->timecodeFollow());

    if (m_nameLabel != NULL)
        m_nameLabel->setText(show != NULL ? show->name() : tr("No show"));

    if (m_playButton != NULL)
        m_playButton->setIcon(QIcon(running ? ":/player_pause.png" : ":/player_play.png"));

    if (m_followButton != NULL)
    {
        m_followButton->blockSignals(true);
        m_followButton->setChecked(following);
        m_followButton->blockSignals(false);
    }
    if (m_suspendButton != NULL)
    {
        m_suspendButton->blockSignals(true);
        m_suspendButton->setChecked(suspended);
        m_suspendButton->setEnabled(running && m_doc->mode() == Doc::Operate);
        m_suspendButton->blockSignals(false);
    }

    if (m_statusLabel != NULL)
    {
        if (suspended)
        {
            m_statusLabel->setText(tr("SUSPENDED"));
            m_statusLabel->setStyleSheet("QLabel { color:#000; background:#f5a623; padding:0 5px; border-radius:3px; font-weight:bold; }");
        }
        else if (running)
        {
            m_statusLabel->setText(tr("LIVE"));
            m_statusLabel->setStyleSheet("QLabel { color:#fff; background:#1565c0; padding:0 5px; border-radius:3px; font-weight:bold; }");
        }
        else
        {
            m_statusLabel->setText(QString());
            m_statusLabel->setStyleSheet(QString());
        }
    }

    updateFeedback();
}

/*********************************************************************
 * QLC+ Mode
 *********************************************************************/

void VCShowControl::slotModeChanged(Doc::Mode mode)
{
    const bool operate = (mode == Doc::Operate);
    // In design the transport is inert so the widget can be arranged.
    if (m_stopButton != NULL) m_stopButton->setEnabled(operate);
    if (m_playButton != NULL) m_playButton->setEnabled(operate);
    if (m_followButton != NULL) m_followButton->setEnabled(operate);
    if (m_suspendButton != NULL) m_suspendButton->setEnabled(operate);
    refresh();
    VCWidget::slotModeChanged(mode);
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCShowControl::updateFeedback()
{
    Show *show = showFunction();
    const bool running = (show != NULL && show->isRunning());
    sendFeedback(running ? UCHAR_MAX : 0, playInputSourceId);
    sendFeedback(0, stopInputSourceId);
    sendFeedback((show != NULL && show->timecodeFollow()) ? UCHAR_MAX : 0, followInputSourceId);
    sendFeedback((running && show->isTimelineSuspended()) ? UCHAR_MAX : 0, suspendInputSourceId);
}

void VCShowControl::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    if (acceptsInput() == false)
        return;

    quint32 pagedCh = (page() << 16) | channel;

    // Edge-trigger each control on a rising value (with hysteresis).
    struct { quint8 id; quint32 *latest; void (VCShowControl::*fn)(); } actions[] = {
        { playInputSourceId,    &m_playLatest,    &VCShowControl::playShow },
        { stopInputSourceId,    &m_stopLatest,    &VCShowControl::stopShow },
        { followInputSourceId,  &m_followLatest,  &VCShowControl::toggleFollow },
        { suspendInputSourceId, &m_suspendLatest, &VCShowControl::toggleSuspend },
    };

    for (uint i = 0; i < sizeof(actions) / sizeof(actions[0]); i++)
    {
        if (checkInputSource(universe, pagedCh, value, sender(), actions[i].id) == false)
            continue;
        quint32 &latest = *actions[i].latest;
        if (latest == 0 && value > 0)
        {
            (this->*(actions[i].fn))();
            latest = value;
        }
        else if (latest > HYSTERESIS && value == 0)
            latest = 0;
        if (value > HYSTERESIS)
            latest = value;
        break;
    }
}

/*********************************************************************
 * Clipboard
 *********************************************************************/

VCWidget *VCShowControl::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != NULL);
    VCShowControl *sc = new VCShowControl(parent, m_doc);
    if (sc->copyFrom(this) == false)
    {
        delete sc;
        sc = NULL;
    }
    return sc;
}

bool VCShowControl::copyFrom(const VCWidget *widget)
{
    const VCShowControl *sc = qobject_cast<const VCShowControl *>(widget);
    if (sc == NULL)
        return false;

    setShow(sc->show());
    return VCWidget::copyFrom(widget);
}

/*********************************************************************
 * Properties
 *********************************************************************/

void VCShowControl::editProperties()
{
    VCShowControlProperties prop(this, m_doc);
    if (prop.exec() == QDialog::Accepted)
        m_doc->setModified();
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool VCShowControl::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCShowControl)
    {
        qWarning() << Q_FUNC_INFO << "Show control node not found";
        return false;
    }

    QXmlStreamAttributes attrs = root.attributes();
    if (attrs.hasAttribute(KXMLQLCVCShowControlFunction))
        setShow(attrs.value(KXMLQLCVCShowControlFunction).toString().toUInt());

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            int x = 0, y = 0, w = 0, h = 0; bool v = true;
            loadXMLWindowState(root, &x, &y, &w, &h, &v);
            setGeometry(x, y, w, h);
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCShowControlPlay)
        {
            loadXMLSources(root, playInputSourceId);
        }
        else if (root.name() == KXMLQLCVCShowControlStop)
        {
            loadXMLSources(root, stopInputSourceId);
        }
        else if (root.name() == KXMLQLCVCShowControlFollow)
        {
            loadXMLSources(root, followInputSourceId);
        }
        else if (root.name() == KXMLQLCVCShowControlSuspend)
        {
            loadXMLSources(root, suspendInputSourceId);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown show control tag" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool VCShowControl::saveXML(QXmlStreamWriter *doc)
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCVCShowControl);

    if (m_showID != Function::invalidId())
        doc->writeAttribute(KXMLQLCVCShowControlFunction, QString::number(m_showID));

    saveXMLCommon(doc);
    saveXMLWindowState(doc);
    saveXMLAppearance(doc);

    doc->writeStartElement(KXMLQLCVCShowControlPlay);
    saveXMLInput(doc, inputSource(playInputSourceId));
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCVCShowControlStop);
    saveXMLInput(doc, inputSource(stopInputSourceId));
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCVCShowControlFollow);
    saveXMLInput(doc, inputSource(followInputSourceId));
    doc->writeEndElement();

    doc->writeStartElement(KXMLQLCVCShowControlSuspend);
    saveXMLInput(doc, inputSource(suspendInputSourceId));
    doc->writeEndElement();

    doc->writeEndElement();
    return true;
}
