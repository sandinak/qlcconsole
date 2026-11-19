/*
  Q Light Controller Plus
  roomoutputpicker.cpp

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

#include <QRegularExpressionValidator>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHostAddress>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include "roomoutputpicker.h"
#include "inputoutputmap.h"
#include "venuerig.h"
#include "doc.h"

namespace
{
const QString kUsbPlugin = QStringLiteral("DMX USB");
const QString kArtNetPlugin = QStringLiteral("ArtNet");
const QString kOutputIP = QStringLiteral("outputIP");
const QString kOutputUni = QStringLiteral("outputUni");
}

RoomOutputPicker::RoomOutputPicker(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("roomOutput"));
    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    QHBoxLayout *hl = new QHBoxLayout;
    m_kind = new QComboBox(this);
    m_kind->setObjectName(QStringLiteral("roomOutputKind"));
    m_kind->addItem(tr("USB DMX interface"), int(Usb));
    m_kind->addItem(tr("Art-Net node"), int(ArtNet));
    m_kind->addItem(tr("Not yet (no output)"), int(NotYet));
    m_kind->setToolTip(tr("Where this venue's universe goes in this room: the room's DMX line through a USB "
                          "DMX interface, or a spare Art-Net node"));
    hl->addWidget(m_kind);

    m_usb = new QComboBox(this);
    m_usb->setObjectName(QStringLiteral("roomOutputUsb"));
    m_usb->setToolTip(tr("The USB DMX interface plugged into this computer for the room's DMX line"));
    m_usb->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    hl->addWidget(m_usb, 1);

    m_ip = new QLineEdit(this);
    m_ip->setObjectName(QStringLiteral("roomOutputIp"));
    m_ip->setPlaceholderText(tr("Node address, e.g. 10.0.0.20"));
    static const QRegularExpression ipRe(QStringLiteral("^[0-9.]{0,15}$"));
    m_ip->setValidator(new QRegularExpressionValidator(ipRe, m_ip));
    m_ip->setToolTip(tr("The Art-Net node's IP address. This computer's network that reaches it is "
                        "found when the venue is applied."));
    hl->addWidget(m_ip, 1);
    m_artnetUniverse = new QSpinBox(this);
    m_artnetUniverse->setObjectName(QStringLiteral("roomOutputArtNetUniverse"));
    m_artnetUniverse->setRange(0, 32767);
    m_artnetUniverse->setPrefix(tr("Art-Net universe "));
    m_artnetUniverse->setToolTip(tr("The Art-Net universe the node listens on (0 is the first)"));
    hl->addWidget(m_artnetUniverse);
    vl->addLayout(hl);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("roomOutputNote"));
    m_note->setWordWrap(true);
    vl->addWidget(m_note);

    refreshInterfaces();
    // No interface here: nothing goes out until one is chosen in the room.
    m_kind->setCurrentIndex(m_kind->findData(int(m_usb->count() > 0 ? Usb : NotYet)));

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this]() { sync(); emit changed(); });
    connect(m_usb, &QComboBox::currentIndexChanged, this, [this]() { sync(); emit changed(); });
    connect(m_ip, &QLineEdit::textChanged, this, [this]() { sync(); emit changed(); });
    connect(m_artnetUniverse, &QSpinBox::valueChanged, this, [this]() { emit changed(); });
    sync();
}

QStringList RoomOutputPicker::usbInterfaces() const
{
    if (m_doc == nullptr || m_doc->inputOutputMap() == nullptr)
        return QStringList();
    return m_doc->inputOutputMap()->pluginOutputs(kUsbPlugin);
}

void RoomOutputPicker::refreshInterfaces()
{
    const QString was = m_usb->currentText();
    const QSignalBlocker block(m_usb);
    m_usb->clear();
    for (const QString &name : usbInterfaces())
        m_usb->addItem(name, name);
    const int at = m_usb->findData(was);
    if (at >= 0)
        m_usb->setCurrentIndex(at);
    sync();
}

void RoomOutputPicker::setImported(const QList<Universe::OfflinePatch> &imported, const QString &fileName)
{
    m_imported = imported;
    const int at = m_kind->findData(int(AsImported));
    if (at >= 0)
        m_kind->removeItem(at);
    if (!imported.isEmpty())
        m_kind->addItem(tr("As %1 had it: %2").arg(fileName, VenueRig::outputText(imported)), int(AsImported));
    sync();
}

void RoomOutputPicker::setOutput(const QList<Universe::OfflinePatch> &patch)
{
    Kind k = NotYet;
    if (patch.isEmpty())
        k = NotYet;
    else if (!m_imported.isEmpty() && VenueRig::samePatch(patch, m_imported))
        k = AsImported;
    else if (patch.first().plugin == kUsbPlugin)
        k = Usb;
    else if (patch.first().plugin == kArtNetPlugin)
        k = ArtNet;
    else
        k = NotYet;
    {
        const QSignalBlocker b1(m_kind), b2(m_usb), b3(m_ip), b4(m_artnetUniverse);
        if (k == Usb)
        {
            const QString name = patch.first().uid;
            if (!name.isEmpty() && m_usb->findData(name) < 0)
                m_usb->addItem(tr("%1 (not plugged in)").arg(name), name);
            m_usb->setCurrentIndex(qMax(0, m_usb->findData(name)));
        }
        else if (k == ArtNet)
        {
            m_ip->setText(patch.first().params.value(kOutputIP).toString());
            m_artnetUniverse->setValue(patch.first().params.value(kOutputUni).toInt());
        }
        m_kind->setCurrentIndex(qMax(0, m_kind->findData(int(k))));
    }
    sync();
}

RoomOutputPicker::Kind RoomOutputPicker::kind() const
{
    return Kind(m_kind->currentData().toInt());
}

bool RoomOutputPicker::isComplete() const
{
    switch (kind())
    {
        case Usb: return m_usb->currentIndex() >= 0;
        case ArtNet:
        {
            // All four parts: QHostAddress alone takes "10.0.0" as 10.0.0.0.
            static const QRegularExpression dotted(QStringLiteral("^\\d{1,3}(\\.\\d{1,3}){3}$"));
            const QString t = m_ip->text().trimmed();
            QHostAddress a;
            return dotted.match(t).hasMatch() && a.setAddress(t) && a.protocol() == QAbstractSocket::IPv4Protocol;
        }
        default: return true;
    }
}

QList<Universe::OfflinePatch> RoomOutputPicker::output() const
{
    QList<Universe::OfflinePatch> out;
    if (!isComplete())
        return out;
    Universe::OfflinePatch p;
    p.tag = Universe::OutputPatchTag;
    switch (kind())
    {
        case Usb:
            p.plugin = kUsbPlugin;
            p.uid = m_usb->currentData().toString();
            p.line = quint32(qMax(0, usbInterfaces().indexOf(p.uid)));
            out << p;
        break;
        case ArtNet:
            /* By the node's address: whichever of this computer's networks
               reaches it, found when it is applied (no interface of the
               office's carried to the room). */
            p.plugin = kArtNetPlugin;
            p.line = 0;
            p.params.insert(kOutputIP, m_ip->text().trimmed());
            if (m_artnetUniverse->value() != 0)
                p.params.insert(kOutputUni, QString::number(m_artnetUniverse->value()));
            out << p;
        break;
        case AsImported:
            out = m_imported;
        break;
        case NotYet:
        break;
    }
    return out;
}

void RoomOutputPicker::sync()
{
    const Kind k = kind();
    m_usb->setVisible(k == Usb);
    m_ip->setVisible(k == ArtNet);
    m_artnetUniverse->setVisible(k == ArtNet);
    QString note;
    switch (k)
    {
        case Usb:
            note = m_usb->count() == 0
                 ? tr("No USB DMX interface is plugged in. Plug it in, or choose Not yet and choose it in the "
                      "room (Rig and Universes).")
                 : tr("The room's 5-pin DMX line, through this interface.");
        break;
        case ArtNet:
            note = isComplete() ? tr("Sent to the node at %1.").arg(m_ip->text().trimmed())
                                : tr("Type the node's IP address.");
        break;
        case NotYet:
            note = tr("Nothing goes out for this universe in this room until an output is chosen. The clash "
                      "check says so.");
        break;
        case AsImported:
            note = tr("That is the network the other show was made on. The clash check flags it until this "
                      "room's output is chosen.");
        break;
    }
    m_note->setText(note);
}
