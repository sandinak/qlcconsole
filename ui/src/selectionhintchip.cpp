/*
  Q Light Controller Plus
  selectionhintchip.cpp

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

#include <QHBoxLayout>
#include <QToolButton>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QLabel>

#include "selectionhintchip.h"
#include "doc.h"

SelectionHintChip::SelectionHintChip(Doc *doc, QWidget *host)
    : QFrame(host)
    , m_doc(doc)
    , m_host(host)
{
    setObjectName(QStringLiteral("selectionHintChip"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);        // never takes the view's keys away
    setStyleSheet(QStringLiteral("#selectionHintChip { background: #2b3a4a; border: 1px solid #5fa8e8;"
                                 " border-radius: 9px; } QLabel { color: #e8f2ff; }"));
    QHBoxLayout *row = new QHBoxLayout(this);
    row->setContentsMargins(8, 2, 2, 2);
    row->setSpacing(4);
    m_label = new QLabel(this);
    row->addWidget(m_label);
    m_close = new QToolButton(this);
    m_close->setText(QStringLiteral("✕"));
    m_close->setAutoRaise(true);
    m_close->setFocusPolicy(Qt::NoFocus);
    m_close->setToolTip(tr("Not this (Esc)"));
    row->addWidget(m_close);
    connect(m_close, &QToolButton::clicked, this, &SelectionHintChip::dismiss);
    setToolTip(tr("Click or press Tab to take it; Esc leaves the selection as it is"));
    hide();

    if (m_host != nullptr)
        m_host->installEventFilter(this);
    // Any other pick (here or anywhere): the offer was about the old one.
    if (doc != nullptr)
        connect(doc, &Doc::programmerSelectionChanged, this, [this]() {
            if (m_offering && !m_doc.isNull() && m_doc->programmerSelection() != m_for)
                dismiss();
        });
}

SelectionHintChip::~SelectionHintChip()
{
}

QString SelectionHintChip::text() const
{
    return m_offering ? m_hint.text : QString();
}

void SelectionHintChip::offer(const SelectionShape::Hint &hint, const QPoint &anchor)
{
    if (hint.isNull() || m_doc.isNull())
    {
        dismiss();
        return;
    }
    m_hint = hint;
    m_for = m_doc->programmerSelection();
    m_offering = true;
    m_label->setText(hint.text);
    adjustSize();
    placeAt(anchor);
    show();
    raise();
    emit offerChanged();
}

void SelectionHintChip::placeAt(const QPoint &anchor)
{
    if (m_host == nullptr)
        return;
    // Next to the selection, kept inside the view.
    const QSize s = sizeHint();
    int x = anchor.isNull() ? 8 : anchor.x() + 12;
    int y = anchor.isNull() ? m_host->height() - s.height() - 8 : anchor.y() + 12;
    x = qBound(4, x, qMax(4, m_host->width() - s.width() - 4));
    y = qBound(4, y, qMax(4, m_host->height() - s.height() - 4));
    setGeometry(x, y, s.width(), s.height());
}

bool SelectionHintChip::accept()
{
    if (!m_offering || m_doc.isNull())
        return false;
    const QList<quint32> result = m_hint.result;
    m_offering = false;         // the change below is ours, not another pick
    hide();
    emit offerChanged();
    m_doc->setProgrammerSelection(result);
    return true;
}

void SelectionHintChip::dismiss()
{
    if (!m_offering)
        return;
    m_offering = false;
    m_hint = SelectionShape::Hint();
    hide();
    emit offerChanged();
}

bool SelectionHintChip::eventFilter(QObject *watched, QEvent *event)
{
    if (m_offering && watched == m_host && event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Tab && ke->modifiers() == Qt::NoModifier)
        {
            accept();
            return true;
        }
        if (ke->key() == Qt::Key_Escape)
        {
            dismiss();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void SelectionHintChip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        accept();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}
