/*
  Q Light Controller Plus
  vccuerunnerproperties.cpp

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

#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QListWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>

#include "inputselectionwidget.h"
#include "vccuerunnerproperties.h"
#include "showtreecontroller.h"
#include "doc.h"

using namespace ShowTree;

VCCueRunnerProperties::VCCueRunnerProperties(VCCueRunner *runner, Doc *doc)
    : QDialog(runner)
    , m_runner(runner)
    , m_doc(doc)
{
    Q_ASSERT(runner != nullptr);
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("cueRunnerProperties"));
    setWindowTitle(tr("Cue Runner properties"));
    CueRunner *cr = runner->runner();

    QVBoxLayout *v = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("crPropTabs"));
    v->addWidget(m_tabs);

    /* General */
    QWidget *general = new QWidget(m_tabs);
    QFormLayout *f = new QFormLayout(general);
    m_caption = new QLineEdit(runner->caption(), general);
    m_caption->setObjectName(QStringLiteral("crPropCaption"));
    f->addRow(tr("Caption"), m_caption);
    m_follow = new QComboBox(general);
    m_follow->setObjectName(QStringLiteral("crPropFollow"));
    ShowTreeController *st = m_doc->showTree();
    m_follow->addItem(tr("The current production"), invalidId());
    for (const Production &p : st->productions())
        for (quint32 lid : st->playableLists(p.id))
            m_follow->addItem(QStringLiteral("%1 · %2").arg(p.name, st->cueList(lid)->name), lid);
    m_follow->setCurrentIndex(qMax(0, m_follow->findData(runner->follow())));
    f->addRow(tr("Plays"), m_follow);
    m_size = new QComboBox(general);
    m_size->setObjectName(QStringLiteral("crPropSize"));
    m_size->addItem(tr("Compact: on now, next, GO and Sub GO"), int(CueRunner::Compact));
    m_size->addItem(tr("List: the whole runner"), int(CueRunner::List));
    m_size->setCurrentIndex(runner->runnerSize() == CueRunner::List ? 1 : 0);
    f->addRow(tr("Size"), m_size);

    m_upcoming = new QSpinBox(general);
    m_upcoming->setObjectName(QStringLiteral("crPropUpcoming"));
    m_upcoming->setRange(0, CueRunner::kMaxUpcoming);
    m_upcoming->setValue(cr->upcomingCount());
    m_upcoming->setToolTip(tr("Compact: how many cues after NEXT it lists, as the room allows"));
    f->addRow(tr("Upcoming cues"), m_upcoming);
    m_timecode = new QCheckBox(tr("The timecode (while there is a source)"), general);
    m_timecode->setObjectName(QStringLiteral("crPropTimecode"));
    m_timecode->setChecked(cr->showTimecode());
    f->addRow(tr("Show"), m_timecode);
    m_subGo = new QCheckBox(tr("Sub GO"), general);
    m_subGo->setObjectName(QStringLiteral("crPropSubGo"));
    m_subGo->setChecked(cr->showSubGo());
    f->addRow(QString(), m_subGo);
    m_secondary = new QCheckBox(tr("Back and Stop (list: Sub Back and Pause too)"), general);
    m_secondary->setObjectName(QStringLiteral("crPropBackStop"));
    m_secondary->setChecked(cr->showSecondary());
    f->addRow(QString(), m_secondary);
    QLabel *sizing = new QLabel(tr("In Design mode, drag the handles inside the widget: between the info and "
                                   "the buttons for the buttons' height, between GO and Sub GO for their "
                                   "widths."), general);
    sizing->setWordWrap(true);
    f->addRow(sizing);
    m_tabs->addTab(general, tr("General"));

    /* External Input: one input and key per action */
    QWidget *input = new QWidget(m_tabs);
    QVBoxLayout *iv = new QVBoxLayout(input);
    QLabel *about = new QLabel(tr("Map each action to a controller (auto-detect, or choose) and a key. "
                                  "Feedback lights a mapped control while its action can do something: GO "
                                  "while there is a cue to GO to, Sub GO while a sub-cue waits, Back and "
                                  "Stop while the list runs, Pause while paused."), input);
    about->setWordWrap(true);
    iv->addWidget(about);
    QHBoxLayout *h = new QHBoxLayout();
    m_actions = new QListWidget(input);
    m_actions->setObjectName(QStringLiteral("crPropActions"));
    m_actions->setMaximumWidth(150);
    m_inputs = new QStackedWidget(input);
    for (int a = 0; a < VCCueRunner::ActionCount; ++a)
    {
        const VCCueRunner::Action action = VCCueRunner::Action(a);
        m_actions->addItem(VCCueRunner::actionName(action));
        InputSelectionWidget *w = new InputSelectionWidget(m_doc, m_inputs);
        w->setObjectName(QStringLiteral("crPropInput%1").arg(VCCueRunner::actionTag(action)));
        w->setTitle(VCCueRunner::actionName(action));
        w->setCustomFeedbackVisibility(true);
        w->setKeySequence(runner->keySequence(action));
        w->setInputSource(runner->inputSource(quint8(a)));
        w->setWidgetPage(runner->page());
        m_inputs->addWidget(w);
        m_inputWidgets << w;
    }
    connect(m_actions, &QListWidget::currentRowChanged, this, [this](int row) {
        slotStopAutoDetection();
        m_inputs->setCurrentIndex(row);
    });
    m_actions->setCurrentRow(0);
    h->addWidget(m_actions);
    h->addWidget(m_inputs, 1);
    iv->addLayout(h, 1);
    m_tabs->addTab(input, tr("External Input"));
    connect(m_tabs, &QTabWidget::currentChanged, this, &VCCueRunnerProperties::slotStopAutoDetection);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
}

VCCueRunnerProperties::~VCCueRunnerProperties()
{
}

InputSelectionWidget *VCCueRunnerProperties::inputWidget(VCCueRunner::Action action) const
{
    return action >= 0 && action < m_inputWidgets.count() ? m_inputWidgets.at(action) : nullptr;
}

void VCCueRunnerProperties::slotStopAutoDetection()
{
    for (InputSelectionWidget *w : m_inputWidgets)
        w->stopAutoDetection();
}

void VCCueRunnerProperties::accept()
{
    slotStopAutoDetection();
    m_runner->setCaption(m_caption->text());
    m_runner->setRunnerSize(CueRunner::Size(m_size->currentData().toInt()));
    m_runner->setFollow(m_follow->currentData().toUInt());
    CueRunner *cr = m_runner->runner();
    cr->setUpcomingCount(m_upcoming->value());
    cr->setShowTimecode(m_timecode->isChecked());
    cr->setShowSubGo(m_subGo->isChecked());
    cr->setShowSecondary(m_secondary->isChecked());
    for (int a = 0; a < VCCueRunner::ActionCount; ++a)
    {
        InputSelectionWidget *w = m_inputWidgets.at(a);
        m_runner->setKeySequence(VCCueRunner::Action(a), w->keySequence());
        m_runner->setInputSource(w->inputSource(), quint8(a));
    }
    m_runner->updateFeedback();
    QDialog::accept();
}
