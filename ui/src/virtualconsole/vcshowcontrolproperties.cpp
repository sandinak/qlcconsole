/*
  Q Light Controller Plus
  vcshowcontrolproperties.cpp

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

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>

#include "vcshowcontrolproperties.h"
#include "inputselectionwidget.h"
#include "vcshowcontrol.h"
#include "function.h"
#include "show.h"
#include "doc.h"

VCShowControlProperties::VCShowControlProperties(VCShowControl *sc, Doc *doc)
    : QDialog(sc)
    , m_sc(sc)
    , m_doc(doc)
{
    setWindowTitle(tr("Show Control properties"));

    QVBoxLayout *main = new QVBoxLayout(this);

    // Bound show picker.
    QHBoxLayout *showRow = new QHBoxLayout();
    showRow->addWidget(new QLabel(tr("Show:"), this));
    m_showCombo = new QComboBox(this);
    m_showCombo->addItem(tr("(none)"), Function::invalidId());
    foreach (Function *f, m_doc->functionsByType(Function::ShowType))
    {
        m_showCombo->addItem(f->name(), f->id());
        if (f->id() == m_sc->show())
            m_showCombo->setCurrentIndex(m_showCombo->count() - 1);
    }
    showRow->addWidget(m_showCombo, 1);
    main->addLayout(showRow);

    m_cueInfoCheck = new QCheckBox(tr("Show current / next cue (name + note)"), this);
    m_cueInfoCheck->setChecked(m_sc->showCueInfo());
    main->addWidget(m_cueInfoCheck);

    // External-input rows (each MIDI-mappable).
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    QWidget *inner = new QWidget(scroll);
    QVBoxLayout *iv = new QVBoxLayout(inner);

    auto makeInput = [&](const QString &title, quint8 id) -> InputSelectionWidget *
    {
        InputSelectionWidget *w = new InputSelectionWidget(m_doc, inner);
        w->setTitle(title);
        w->setInputSource(m_sc->inputSource(id));
        w->setWidgetPage(m_sc->page());
        w->show();
        iv->addWidget(w);
        return w;
    };
    m_playInput    = makeInput(tr("Play / Pause input"), VCShowControl::playInputSourceId);
    m_stopInput    = makeInput(tr("Stop input"),         VCShowControl::stopInputSourceId);
    m_followInput  = makeInput(tr("Follow MTC input"),   VCShowControl::followInputSourceId);
    m_suspendInput = makeInput(tr("Suspend timeline input"), VCShowControl::suspendInputSourceId);
    iv->addStretch(1);
    scroll->setWidget(inner);
    main->addWidget(scroll, 1);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    main->addWidget(box);

    resize(420, 460);
}

VCShowControlProperties::~VCShowControlProperties()
{
}

void VCShowControlProperties::accept()
{
    m_sc->setShow(m_showCombo->currentData().toUInt());
    m_sc->setShowCueInfo(m_cueInfoCheck->isChecked());
    m_sc->setInputSource(m_playInput->inputSource(),    VCShowControl::playInputSourceId);
    m_sc->setInputSource(m_stopInput->inputSource(),    VCShowControl::stopInputSourceId);
    m_sc->setInputSource(m_followInput->inputSource(),  VCShowControl::followInputSourceId);
    m_sc->setInputSource(m_suspendInput->inputSource(), VCShowControl::suspendInputSourceId);

    QDialog::accept();
}
