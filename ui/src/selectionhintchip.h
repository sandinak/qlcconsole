/*
  Q Light Controller Plus
  selectionhintchip.h

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

#ifndef SELECTIONHINTCHIP_H
#define SELECTIONHINTCHIP_H

#include <QPointer>
#include <QFrame>
#include <QList>

#include "selectionshape.h"
#include "qlcui_export.h"

class QToolButton;
class QLabel;
class Doc;

/** @addtogroup ui
 * @{
 */

/**
 * The near-miss chip (Q201): "Front Wash is 6 — select all 6?" next to a
 * selection that is a group but for one or two fixtures (or one stray).
 * Non-modal and never acting by itself: a click on it, or Tab in the view
 * it sits in, takes the offer; Esc, ✕ or any other pick dismisses it.
 */
class QLC_UI_EXPORT SelectionHintChip final : public QFrame
{
    Q_OBJECT

public:
    /** A chip over @a host (the view the picks are made in). */
    SelectionHintChip(Doc *doc, QWidget *host);
    ~SelectionHintChip();

    /** Offer @a hint about the programmer selection now, near @a anchor
     *  (@a host's coordinates); a null hint takes any offer away. */
    void offer(const SelectionShape::Hint &hint, const QPoint &anchor = QPoint());
    /** Is an offer up (the tests, the host's outline). */
    bool isOffering() const { return m_offering; }
    SelectionShape::Hint hint() const { return m_hint; }
    QString text() const;

    /** Take the offer: the programmer selection becomes its result. */
    bool accept();
    /** Take the offer away; the selection stays as it is. */
    void dismiss();

signals:
    /** An offer came or went (the plot outlines the missed fixtures). */
    void offerChanged();

protected:
    /** Tab / Esc in the host while an offer is up. */
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void placeAt(const QPoint &anchor);

    QPointer<Doc> m_doc;
    QWidget *m_host;
    QLabel *m_label;
    QToolButton *m_close;
    SelectionShape::Hint m_hint;
    QList<quint32> m_for;       //!< the selection it was made for
    bool m_offering = false;
};

/** @} */

#endif
