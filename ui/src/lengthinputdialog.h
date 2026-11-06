/*
  Q Light Controller Plus
  lengthinputdialog.h

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

#ifndef LENGTHINPUTDIALOG_H
#define LENGTHINPUTDIALOG_H

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QLabel>

#include "lengthspinbox.h"

/** \addtogroup ui UI
 * @{
 */

/**
 * Asking for one length: what QInputDialog::getDouble() would be, but the
 * field is a LengthSpinBox, so it shows Settings -> Units and takes a
 * length typed in any unit (12' 6", 3.8 m, 150 in). Never ask for a length
 * with getDouble: its number has no unit, and whichever one the label names
 * is not the person's.
 *
 * Header-only and without Q_OBJECT, so no moc pass.
 */
class LengthInputDialog
{
    Q_DECLARE_TR_FUNCTIONS(LengthInputDialog)

public:
    /** The field's object name, for a test to find it */
    static QString fieldName() { return QStringLiteral("lengthInputField"); }

    /** Asks for a length in millimetres: @a mm to start, between @a minMm
     *  and @a maxMm. Sets @a ok false (and returns @a mm) on Cancel. */
    static double getMillimetres(QWidget *parent, const QString &title, const QString &label,
                                 double mm, double minMm, double maxMm, bool *ok)
    {
        QDialog dlg(parent);
        dlg.setWindowTitle(title);
        dlg.setObjectName(QStringLiteral("lengthInputDialog"));
        QVBoxLayout *vl = new QVBoxLayout(&dlg);
        QLabel *lbl = new QLabel(label, &dlg);
        lbl->setWordWrap(true);
        vl->addWidget(lbl);
        LengthSpinBox *field = new LengthSpinBox(1.0, &dlg);
        field->setObjectName(fieldName());
        field->setDecimals(1);
        field->setRange(minMm, maxMm);
        field->setValue(mm);
        field->selectAll();
        lbl->setBuddy(field);
        vl->addWidget(field);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        vl->addWidget(bb);
        const bool accepted = dlg.exec() == QDialog::Accepted;
        if (ok != nullptr)
            *ok = accepted;
        if (!accepted)
            return mm;
        field->interpretText();
        return field->value();
    }

    /** The same in metres */
    static double getMetres(QWidget *parent, const QString &title, const QString &label,
                            double metres, double minMetres, double maxMetres, bool *ok)
    {
        return getMillimetres(parent, title, label, metres * 1000.0, minMetres * 1000.0,
                              maxMetres * 1000.0, ok) / 1000.0;
    }
};

/** @} */

#endif // LENGTHINPUTDIALOG_H
