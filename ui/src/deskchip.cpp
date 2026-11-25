/*
  Q Light Controller Plus - qlcconsole
  deskchip.cpp

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

#include <QTimer>

#include "deskrecord.h"
#include "deskchip.h"
#include "doc.h"

DeskChip::DeskChip(Doc *doc, QWidget *parent)
    : QToolButton(parent)
    , m_doc(doc)
    , m_undoTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("footerDesk"));
    setAutoRaise(true);
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setCursor(Qt::PointingHandCursor);
    setProperty("helpTopic", QStringLiteral("control-surfaces.md#this-computers-desk"));
    m_undoTimer->setSingleShot(true);
    m_undoTimer->setInterval(15000);
    connect(m_undoTimer, &QTimer::timeout, this, [this]() {
        m_patched.clear();
        refresh();
    });
    connect(this, &QToolButton::clicked, this, &DeskChip::slotClicked);
    if (DeskRecord *desk = m_doc != nullptr ? m_doc->deskRecord() : nullptr)
    {
        connect(desk, &DeskRecord::changed, this, &DeskChip::refresh);
        connect(desk, &DeskRecord::grandMasterWaitingChanged, this, &DeskChip::refresh);
        connect(desk, &DeskRecord::goTargetChanged, this, &DeskChip::refresh);
    }
    refresh();
}

void DeskChip::setUndoMs(int ms)
{
    m_undoTimer->setInterval(ms);
}

void DeskChip::refresh()
{
    DeskRecord *desk = m_doc != nullptr ? m_doc->deskRecord() : nullptr;
    if (desk == nullptr || !desk->anySet())
    {
        m_colour = Hidden;
        setProperty("deskState", QStringLiteral("hidden"));
        hide();
        return;
    }

    QStringList red, amber;
    for (DeskRecord::Role r : DeskRecord::roles())
    {
        switch (desk->state(r))
        {
            case DeskRecord::NotPatched:
                if (!red.contains(desk->binding(r).device))
                    red << desk->binding(r).device;
                break;
            case DeskRecord::NotConnected:
                amber << tr("%1 not connected").arg(desk->binding(r).device);
                break;
            case DeskRecord::Off:
                amber << tr("%1 off").arg(DeskRecord::roleName(r));
                break;
            case DeskRecord::NotSet:
            case DeskRecord::Live:
                break;
        }
    }
    if (desk->grandMasterWaiting())
        amber.prepend(desk->grandMasterHint());
    if (!desk->duplicateWarning().isEmpty())
        amber << tr("two devices of one name");
    amber.removeDuplicates();

    QString text;
    QString colour;
    if (!m_patched.isEmpty())
    {
        m_colour = red.isEmpty() ? (amber.isEmpty() ? Green : Amber) : Red;
        text = tr("Desk: %1 · Undo").arg(m_patched);
    }
    else if (!red.isEmpty())
    {
        m_colour = Red;
        text = tr("Desk: %1 not patched · Patch It").arg(red.join(QStringLiteral(", ")));
    }
    else if (!amber.isEmpty())
    {
        m_colour = Amber;
        text = tr("Desk: %1").arg(amber.first());
    }
    else
    {
        m_colour = Green;
        text = tr("Desk");
    }
    switch (m_colour)
    {
        case Green: colour = QStringLiteral("#43a047"); setProperty("deskState", QStringLiteral("green")); break;
        case Amber: colour = QStringLiteral("#a06000"); setProperty("deskState", QStringLiteral("amber")); break;
        case Red:   colour = QStringLiteral("#c62828"); setProperty("deskState", QStringLiteral("red")); break;
        case Hidden: break;
    }
    setStyleSheet(QStringLiteral("QToolButton { color: %1; font-weight: bold; padding: 0 6px; border: none; }")
                      .arg(colour));
    if (this->text() != text)
        setText(text);

    // The tooltip: each role, and what GO plays now.
    QStringList tip;
    tip << tr("This computer's desk:");
    for (const QString &line : desk->reviewLines())
        tip << line;
    if (!desk->isLive(DeskRecord::Go) && !desk->isLive(DeskRecord::Back))
        tip << desk->goTargetText();
    if (m_colour == Red && m_patched.isEmpty())
        tip << tr("Click to patch it into this show (Undo stays here for a moment).");
    else if (!m_patched.isEmpty())
        tip << tr("Click to take it off the patch again.");
    else
        tip << tr("Click to open Connections ▸ Control surfaces.");
    setToolTip(tip.join(QStringLiteral("\n")));
    setAccessibleName(text);
    show();
}

void DeskChip::slotClicked()
{
    DeskRecord *desk = m_doc != nullptr ? m_doc->deskRecord() : nullptr;
    if (desk == nullptr)
        return;
    QString what;
    if (!m_patched.isEmpty())
    {
        m_undoTimer->stop();
        m_patched.clear();
        if (desk->undoPatchIt(&what))
            emit said(what);
        refresh();
        return;
    }
    if (m_colour == Red)
    {
        const QStringList missing = desk->missingDevices();
        if (desk->patchIt(&what) != 0xFFFFFFFF)
        {
            m_patched = tr("patched %1").arg(missing.value(0));
            m_undoTimer->start();
            emit said(what);
        }
        refresh();
        return;
    }
    emit setUpRequested();
}
