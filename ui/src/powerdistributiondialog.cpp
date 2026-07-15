/*
  Q Light Controller Plus
  powerdistributiondialog.cpp

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

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>

#include "powerdistributiondialog.h"
#include "powerdistributionwidget.h"
#include "doc.h"

#define SETTINGS_GEOMETRY "powerdistribution/geometry"

PowerDistributionDialog::PowerDistributionDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Power Distribution"));
    resize(940, 520);

    QVBoxLayout *top = new QVBoxLayout(this);

    m_widget = new PowerDistributionWidget(doc, true, this);
    top->addWidget(m_widget, 1);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::accept);
    top->addWidget(bb);

    QSettings settings;
    QVariant geom = settings.value(SETTINGS_GEOMETRY);
    if (geom.isValid())
        restoreGeometry(geom.toByteArray());
}

PowerDistributionDialog::~PowerDistributionDialog()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}
