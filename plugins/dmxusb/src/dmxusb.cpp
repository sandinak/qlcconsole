/*
  Q Light Controller Plus
  dmxusb.cpp

  Copyright (C) Heikki Junnila
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

#include <QStringList>
#include <QMessageBox>
#include <QDebug>

#include "dmxusbconfig.h"
#include "dmxusbwidget.h"
#include "enttecdmxusbpro.h"
#include "enttecdmxusbopen.h"
#include "euroliteusbdmxpro.h"
#include "dmxusb.h"

/****************************************************************************
 * Initialization
 ****************************************************************************/

DMXUSB::~DMXUSB()
{
    while (m_widgets.isEmpty() == false)
        delete m_widgets.takeFirst();
}

void DMXUSB::init()
{
    /* Search for new widgets */
    rescanWidgets();
}

QString DMXUSB::name() const
{
    return QString("DMX USB");
}

int DMXUSB::capabilities() const
{
    return QLCIOPlugin::Output | QLCIOPlugin::Input | QLCIOPlugin::RDM;
}

bool DMXUSB::rescanWidgets()
{
    int linesCount = m_inputs.count() + m_outputs.count();
    const QStringList outputsBefore = outputs();

    /* Review 10 M2. A rescan replaces every widget, and the devices may
       have moved or gone: each open output is sent its closing frame (the
       Show-only channels at safe -- best effort, the device may be gone
       already) and closed (H1: on the wire before the port goes) before
       its widget is deleted -- and no universe thread is inside
       writeUniverse() while the widgets are swapped. The closing frames
       are fetched first, with no lock of ours held: the engine's builder
       takes the universe's frame lock, which a universe thread holds
       while it waits for ours. */
    m_rescanning.store(true, std::memory_order_release);
    {
        QMutexLocker barrier(&m_widgetsMutex);     // a write in progress is done
    }
    struct Closing { quint32 line; quint32 universe; QByteArray frame; };
    QList<Closing> closing;
    typedef QPair<quint32, quint32> LineUniverse;
    foreach (const LineUniverse &lu, openOutputLines())
    {
        Closing c { lu.first, lu.second, QByteArray() };
        if (closingFrameFor(c.universe, c.frame))
            closing << c;
    }

    {
        QMutexLocker locker(&m_widgetsMutex);
        foreach (const Closing &c, closing)
            if (c.line < quint32(m_outputs.size()) && m_outputs.at(int(c.line))->isOpen())
                m_outputs.at(int(c.line))->writeUniverse(c.universe, c.line, c.frame, true);
        foreach (const LineUniverse &lu, openOutputLines())
            if (lu.first < quint32(m_outputs.size()))
                m_outputs.at(int(lu.first))->close(lu.first, false);

        m_inputs.clear();
        m_outputs.clear();

        while (m_widgets.isEmpty() == false)
            delete m_widgets.takeFirst();

        m_widgets = DMXUSBWidget::widgets();
        /* A widget that stops taking data says so in the footer: the write
           threads tell the GUI thread, which asks outputProblems() again. */
        foreach (DMXUSBWidget *widget, m_widgets)
            widget->setProblemNotifier([this]() {
                QMetaObject::invokeMethod(this, "targetProblemsChanged", Qt::QueuedConnection);
            });

        foreach (DMXUSBWidget* widget, m_widgets)
        {
            for (int o = 0; o < widget->outputsNumber(); o++)
                m_outputs.append(widget);

            for (int i = 0; i < widget->inputsNumber(); i++)
                m_inputs.append(widget);
        }
        m_openLines.clear();

        /* Review 11 M-1: the same devices on the same lines (their names,
           serials included, unchanged): nothing tells the engine, so the
           lines it had open are opened again here -- or output would stop
           without a word. Changed, the engine reconnects them. */
        if (outputs() == outputsBefore)
        {
            foreach (const LineUniverse &lu, openOutputLines())
            {
                if (lu.first >= quint32(m_outputs.size()))
                    continue;
                if (m_outputs.at(int(lu.first))->open(lu.first, false) && m_openLines.contains(lu.first) == false)
                    m_openLines.append(lu.first);
            }
        }
        m_rescanning.store(false, std::memory_order_release);
    }

    /* Told when the lines are not the same ones as before, not only when
       their number changed (a device swapped for another): the engine then
       follows each patch by its device's name (serial), and never reopens
       a Show-only universe on whatever took its number. With no lock held:
       the engine reopens lines, under its frame locks. */
    if (m_inputs.count() + m_outputs.count() != linesCount || outputs() != outputsBefore)
        emit configurationChanged();

    return true;
}

QList <DMXUSBWidget*> DMXUSB::widgets() const
{
    return m_widgets;
}

/****************************************************************************
 * Outputs
 ****************************************************************************/

bool DMXUSB::openOutput(quint32 output, quint32 universe)
{
    if (output < quint32(m_outputs.size()))
    {
        DMXUSBWidget *widget = m_outputs.at(output);
        if (widget->supportRDM())
        {
            EnttecDMXUSBPro *pro = static_cast<EnttecDMXUSBPro*>(widget);
            connect(pro, SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)),
                    this, SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)), Qt::QueuedConnection);
        }
        addToMap(universe, output, Output);
        {
            QMutexLocker locker(&m_widgetsMutex);
            if (m_openLines.contains(output) == false)
                m_openLines.append(output);
        }
        return m_outputs.at(output)->open(output, false);
    }
    return false;
}

void DMXUSB::closeOutput(quint32 output, quint32 universe)
{
    if (output < quint32(m_outputs.size()))
    {
        DMXUSBWidget *widget = m_outputs.at(output);
        if (widget->supportRDM())
        {
            EnttecDMXUSBPro *pro = static_cast<EnttecDMXUSBPro*>(widget);
            disconnect(pro, SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)),
                       this , SIGNAL(rdmValueChanged(quint32, quint32, QVariantMap)));
        }
        removeFromMap(universe, output, Output);
        {
            QMutexLocker locker(&m_widgetsMutex);
            m_openLines.removeAll(output);
        }
        m_outputs.at(output)->close(output, false);
    }
}

QStringList DMXUSB::outputs()
{
    QStringList list;

    for (int w = 0; w < m_outputs.count();)
    {
        DMXUSBWidget* widget = m_outputs.at(w);
        foreach (QString name, widget->outputNames())
            list << name;
        w += widget->outputsNumber();
    }
    return list;
}

QString DMXUSB::pluginInfo() const
{
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides DMX output support for");
    str += QString(" DMXKing ultraDMX range, Enttec DMX USB Pro, "
                   "Enttec Open DMX USB, FTDI USB COM485 Plus1, "
                   "Vince USB-DMX512, usbdmx.com (legacy) ");
    str += tr("and compatible devices.");
    str += QString("</P>");

    return str;
}

QString DMXUSB::outputInfo(quint32 output)
{
    QString str;

    if (output == QLCIOPlugin::invalidLine())
    {
        if (m_outputs.size() == 0)
        {
            str += QString("<BR><B>%1</B>").arg(tr("No output support available."));
            str += QString("<P>");
            str += tr("Make sure that you have your hardware firmly plugged in. "
                      "NOTE: FTDI VCP interface is not supported by this plugin.");
            str += QString("</P>");
        }
    }
    else if (output < quint32(m_outputs.size()))
    {
        str += QString("<H3>%1</H3>").arg(outputs()[output]);
        str += QString("<P>");
        str += tr("Device is operating correctly.");
        str += QString("<BR>");
        str += tr("Driver in use: %1").arg(m_outputs[output]->interfaceTypeString());
        str += QString("</P>");
        QString add = m_outputs[output]->additionalInfo();
        if (add.isEmpty() == false)
            str += add;
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

void DMXUSB::writeUniverse(quint32 universe, quint32 output, const QByteArray &data, bool dataChanged)
{
    // A rescan swapping the widgets: the old ones keep their closing frame.
    if (m_rescanning.load(std::memory_order_acquire))
        return;
    QMutexLocker locker(&m_widgetsMutex);
    if (output < quint32(m_outputs.size()))
    {
        DMXUSBWidget *widget = m_outputs.at(output);
        // Not opened by the engine (a new device that took this number in
        // a rescan): nothing is kept for it, nothing reaches the rig -- and
        // the footer says so (WriteHealth), never silently.
        if (m_openLines.contains(output) == false)
        {
            widget->recordWrite(false);
            return;
        }
        // Not open (it went away, or never opened): nothing reaches the rig.
        if (widget->writeUniverse(universe, output, data, dataChanged) == false
            && widget->isOpen() == false)
            widget->recordWrite(false);
    }
}

void DMXUSB::flushOutput(quint32 output, quint32 universe)
{
    Q_UNUSED(universe)
    /* Asked only after a closing frame (a universe with Show-only
       channels): a widget whose thread serves other ports (the Pro) waits
       for it to have sent it. The others send it as they close. GUI thread,
       as a rescan: no lock needed, and none held while it waits. */
    if (output < quint32(m_outputs.size()))
        m_outputs.at(int(output))->flush(output);
}

QStringList DMXUSB::outputProblems(quint32 output, quint32 universe) const
{
    Q_UNUSED(universe)
    QStringList out;
    if (output >= quint32(m_outputs.size()))
        return out;
    DMXUSBWidget *widget = m_outputs.at(output);
    const QString problem = widget->writeProblem();
    if (!problem.isEmpty())
        out << tr("DMX USB %1: %2").arg(widget->uniqueName(), problem);
    return out;
}

/****************************************************************************
 * Inputs
 ****************************************************************************/

bool DMXUSB::openInput(quint32 input, quint32 universe)
{
    if (input < quint32(m_inputs.size()))
    {
        DMXUSBWidget *widget = m_inputs.at(input);
        if (widget->type() == DMXUSBWidget::ProRXTX ||
            widget->type() == DMXUSBWidget::ProMk2 ||
            widget->type() == DMXUSBWidget::OpenRX ||
            widget->type() == DMXUSBWidget::UltraPro)
        {
            EnttecDMXUSBPro *pro = static_cast<EnttecDMXUSBPro*>(widget);
            connect(pro, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                    this, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));
        }
        addToMap(universe, input, Input);
        return widget->open(input, true);
    }
    return false;
}

void DMXUSB::closeInput(quint32 input, quint32 universe)
{
    if (input < quint32(m_inputs.size()))
    {
        DMXUSBWidget *widget = m_inputs.at(input);
        removeFromMap(universe, input, Input);
        widget->close(input, true);
        if (widget->type() == DMXUSBWidget::ProRXTX ||
            widget->type() == DMXUSBWidget::ProMk2 ||
            widget->type() == DMXUSBWidget::UltraPro)
        {
            EnttecDMXUSBPro* pro = (EnttecDMXUSBPro*) widget;
            disconnect(pro, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)),
                       this, SIGNAL(valueChanged(quint32,quint32,quint32,uchar)));
        }
    }
}

QStringList DMXUSB::inputs()
{
    QStringList list;

    for (int w = 0; w < m_inputs.count();)
    {
        DMXUSBWidget* widget = m_inputs.at(w);
        foreach (QString name, widget->inputNames())
            list << name;
        w += widget->inputsNumber();
    }

    return list;
}

QString DMXUSB::inputInfo(quint32 input)
{
    QString str;

    if (input == QLCIOPlugin::invalidLine())
    {
        if (m_inputs.size() == 0)
        {
            str += QString("<BR><B>%1</B>").arg(tr("No input support available."));
            /*
            str += QString("<P>");
            str += tr("Make sure that you have your hardware firmly plugged in. "
                      "NOTE: FTDI VCP interface is not supported by this plugin.");
            str += QString("</P>");
            */
        }
    }
    else if (input < quint32(m_inputs.size()))
    {
        str += QString("<H3>%1</H3>").arg(inputs()[input]);
        str += QString("<P>");
        str += tr("Device is operating correctly.");
        str += QString("<BR>");
        str += tr("Driver in use: %1").arg(m_inputs[input]->interfaceTypeString());
        str += QString("</P>");
        QString add = m_inputs[input]->additionalInfo();
        if (add.isEmpty() == false)
            str += add;
    }

    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

/****************************************************************************
 * Configuration
 ****************************************************************************/

void DMXUSB::configure()
{
    qDebug() << Q_FUNC_INFO;
    DMXUSBConfig config(this);
    config.exec();
    rescanWidgets();
    emit configurationChanged();
}

bool DMXUSB::canConfigure() const
{
    return true;
}

bool DMXUSB::sendRDMCommand(quint32 universe, quint32 line, uchar command, QVariantList params)
{
    if (line < quint32(m_outputs.size()))
    {
        DMXUSBWidget *widget = m_outputs.at(line);
        if (widget->supportRDM())
            return widget->sendRDMCommand(universe, line, command, params);
    }

    return false;
}

/*****************************************************************************
 * Hotplug
 *****************************************************************************/

void DMXUSB::slotDeviceAdded(uint vid, uint pid)
{
    qDebug() << Q_FUNC_INFO << QString::number(vid, 16) << QString::number(pid, 16);
    if (!DMXInterface::validInterface(vid, pid))
    {
        qDebug() << Q_FUNC_INFO << "Invalid DMX USB device, nothing to do";
        return;
    }

    rescanWidgets();
}

void DMXUSB::slotDeviceRemoved(uint vid, uint pid)
{
    qDebug() << Q_FUNC_INFO << QString::number(vid, 16) << QString::number(pid, 16);
    if (!DMXInterface::validInterface(vid, pid))
    {
        qDebug() << Q_FUNC_INFO << "Invalid DMX USB device, nothing to do";
        return;
    }

    rescanWidgets();
}
