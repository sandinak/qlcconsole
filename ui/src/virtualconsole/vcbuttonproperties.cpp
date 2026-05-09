/*
  Q Light Controller
  vcbuttonproperties.cpp

  Copyright (c) Heikki Junnila

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

#include <QIntValidator>
#include <QKeySequence>
#include <QRegularExpression>
#include <QRadioButton>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QAction>
#include <qmath.h>

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "inputselectionwidget.h"
#include "vcbuttonproperties.h"
#include "functionselection.h"
#include "speeddialwidget.h"
#include "fixturegroup.h"
#include "fixture.h"
#include "doc.h"
#include "function.h"
#include "doc.h"

VCButtonProperties::VCButtonProperties(VCButton* button, Doc* doc)
    : QDialog(button)
    , m_button(button)
    , m_doc(doc)
    , m_speedDials(NULL)
{
    Q_ASSERT(button != NULL);
    Q_ASSERT(doc != NULL);

    setupUi(this);

    m_inputSelWidget = new InputSelectionWidget(m_doc, this);
    m_inputSelWidget->setCustomFeedbackVisibility(true);
    m_inputSelWidget->setMonitoringSupport(true);
    m_inputSelWidget->setKeySequence(m_button->keySequence());
    m_inputSelWidget->setInputSource(m_button->inputSource());
    m_inputSelWidget->setWidgetPage(m_button->page());
    m_inputSelWidget->show();
    m_extControlLayout->addWidget(m_inputSelWidget);

    QAction* action = new QAction(this);
    action->setShortcut(QKeySequence(QKeySequence::Close));
    connect(action, SIGNAL(triggered(bool)), this, SLOT(reject()));
    addAction(action);

    /* Button text and function */
    m_nameEdit->setText(m_button->caption());
    slotSetFunction(button->function());

    /* Press action */
    if (button->action() == VCButton::Flash)
        m_flash->setChecked(true);
    else if (button->action() == VCButton::Blackout)
        m_blackout->setChecked(true);
    else if (button->action() == VCButton::StopAll)
        m_stopAll->setChecked(true);
    else if (button->action() == VCButton::SelectFixtures)
        m_selectFixtures->setChecked(true);
    else
        m_toggle->setChecked(true);
    m_fadeOutTime = m_button->stopAllFadeTime();
    m_fadeOutEdit->setText(Function::speedToString(m_fadeOutTime));

    /* Selection mode + tree */
    switch (m_button->selectionMode())
    {
    case VCButton::SelectAdd:    m_selectionAddRadio->setChecked(true); break;
    case VCButton::SelectRemove: m_selectionRemoveRadio->setChecked(true); break;
    case VCButton::SelectToggle: m_selectionToggleRadio->setChecked(true); break;
    default:                     m_selectionReplaceRadio->setChecked(true); break;
    }
    populateSelectionTree();

    slotActionToggled();

    m_forceLTP->setChecked(m_button->flashForceLTP());
    m_overridePriority->setChecked(m_button->flashOverrides());

    /* Intensity adjustment */
    m_intensityEdit->setValidator(new QIntValidator(0, 100, this));
    m_intensityGroup->setChecked(m_button->isStartupIntensityEnabled());
    int intensity = int(floor(m_button->startupIntensity() * double(100)));
    m_intensityEdit->setText(QString::number(intensity));
    m_intensitySlider->setValue(intensity);

    /* Button connections */
    connect(m_attachFunction, SIGNAL(clicked()), this, SLOT(slotAttachFunction()));
    connect(m_detachFunction, SIGNAL(clicked()), this, SLOT(slotSetFunction()));

    connect(m_toggle, SIGNAL(toggled(bool)), this, SLOT(slotActionToggled()));
    connect(m_blackout, SIGNAL(toggled(bool)), this, SLOT(slotActionToggled()));
    connect(m_stopAll, SIGNAL(toggled(bool)), this, SLOT(slotActionToggled()));
    connect(m_flash, SIGNAL(toggled(bool)), this, SLOT(slotActionToggled()));
    connect(m_selectFixtures, SIGNAL(toggled(bool)), this, SLOT(slotActionToggled()));

    connect(m_speedDialButton, SIGNAL(toggled(bool)),
            this, SLOT(slotSpeedDialToggle(bool)));

    connect(m_intensitySlider, SIGNAL(valueChanged(int)),
            this, SLOT(slotIntensitySliderMoved(int)));
    connect(m_intensityEdit, SIGNAL(textEdited(QString)),
            this, SLOT(slotIntensityEdited(QString)));

    connect(m_fadeOutEdit, SIGNAL(editingFinished()),
            this, SLOT(slotFadeOutTextEdited()));
}

VCButtonProperties::~VCButtonProperties()
{
}

void VCButtonProperties::slotAttachFunction()
{
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    if (fs.exec() == QDialog::Accepted && fs.selection().size() > 0)
        slotSetFunction(fs.selection().first());
}

void VCButtonProperties::slotSetFunction(quint32 fid)
{
    m_function = fid;
    Function* func = m_doc->function(m_function);

    if (func == NULL)
    {
        m_functionEdit->setText(tr("No function"));
    }
    else
    {
        m_functionEdit->setText(func->name());
        if (m_nameEdit->text().simplified().contains(QString::number(m_button->id())))
            m_nameEdit->setText(func->name());
    }
}

void VCButtonProperties::slotActionToggled()
{
    bool isFunctional = m_toggle->isChecked() || m_flash->isChecked();
    if (isFunctional == false)
    {
        m_generalGroup->setEnabled(false);
        m_intensityGroup->setEnabled(false);
    }
    else
    {
        m_generalGroup->setEnabled(true);
        m_intensityGroup->setEnabled(true);
    }

    m_fadeOutEdit->setEnabled(m_stopAll->isChecked());
    m_safFadeLabel->setEnabled(m_stopAll->isChecked());
    m_speedDialButton->setEnabled(m_stopAll->isChecked());

    m_forceLTP->setEnabled(m_flash->isChecked());
    m_overridePriority->setEnabled(m_flash->isChecked());

    m_selectionGroup->setVisible(m_selectFixtures->isChecked());
}

void VCButtonProperties::populateSelectionTree()
{
    m_selectionTree->clear();

    // Bind to locals so the .begin()/.end() iterators don't point into
    // destroyed temporaries (selectionFixtures()/selectionGroups()
    // return QList by value).
    const QList<quint32> fixturesSelected = m_button->selectionFixtures();
    const QList<quint32> groupsSelected = m_button->selectionGroups();
    const QSet<quint32> selectedFix(fixturesSelected.begin(), fixturesSelected.end());
    const QSet<quint32> selectedGrp(groupsSelected.begin(), groupsSelected.end());

    // Group node first; each group's members appear under it.
    QTreeWidgetItem* groupsRoot = new QTreeWidgetItem(m_selectionTree);
    groupsRoot->setText(0, tr("Fixture Groups"));
    groupsRoot->setExpanded(true);
    groupsRoot->setFlags(groupsRoot->flags() & ~Qt::ItemIsUserCheckable);

    for (FixtureGroup* grp : m_doc->fixtureGroups())
    {
        QTreeWidgetItem* g = new QTreeWidgetItem(groupsRoot);
        g->setText(0, grp->name());
        g->setData(0, Qt::UserRole, QStringLiteral("group"));
        g->setData(0, Qt::UserRole + 1, grp->id());
        g->setFlags(g->flags() | Qt::ItemIsUserCheckable);
        g->setCheckState(0, selectedGrp.contains(grp->id())
                            ? Qt::Checked : Qt::Unchecked);
    }

    // Then individual fixtures so users can pick one-offs that aren't
    // members of a group.
    QTreeWidgetItem* fixturesRoot = new QTreeWidgetItem(m_selectionTree);
    fixturesRoot->setText(0, tr("Individual Fixtures"));
    fixturesRoot->setExpanded(true);
    fixturesRoot->setFlags(fixturesRoot->flags() & ~Qt::ItemIsUserCheckable);

    for (Fixture* fxi : m_doc->fixtures())
    {
        QTreeWidgetItem* f = new QTreeWidgetItem(fixturesRoot);
        f->setText(0, fxi->name());
        f->setData(0, Qt::UserRole, QStringLiteral("fixture"));
        f->setData(0, Qt::UserRole + 1, fxi->id());
        f->setFlags(f->flags() | Qt::ItemIsUserCheckable);
        f->setCheckState(0, selectedFix.contains(fxi->id())
                            ? Qt::Checked : Qt::Unchecked);
    }
}

void VCButtonProperties::slotSpeedDialToggle(bool state)
{
    if (state == true)
    {
        m_speedDials = new SpeedDialWidget(this);
        m_speedDials->setAttribute(Qt::WA_DeleteOnClose);
        m_speedDials->setWindowTitle(m_button->caption());
        m_speedDials->setFadeInVisible(false);
        m_speedDials->setFadeOutSpeed(m_fadeOutTime);
        m_speedDials->setDurationEnabled(false);
        m_speedDials->setDurationVisible(false);
        connect(m_speedDials, SIGNAL(fadeOutChanged(int)), this, SLOT(slotFadeOutDialChanged(int)));
        connect(m_speedDials, SIGNAL(destroyed(QObject*)), this, SLOT(slotDialDestroyed(QObject*)));
        m_speedDials->show();
    }
    else
    {
        if (m_speedDials != NULL)
            m_speedDials->deleteLater();
        m_speedDials = NULL;
    }
}

void VCButtonProperties::slotFadeOutDialChanged(int ms)
{
    m_fadeOutEdit->setText(Function::speedToString(ms));
    m_fadeOutTime = ms;
}

void VCButtonProperties::slotDialDestroyed(QObject *)
{
    m_speedDialButton->setChecked(false);
}

void VCButtonProperties::slotIntensitySliderMoved(int value)
{
    m_intensityEdit->setText(QString::number(value));
}

void VCButtonProperties::slotIntensityEdited(const QString& text)
{
    m_intensitySlider->setValue(text.toInt());
}

void VCButtonProperties::slotFadeOutTextEdited()
{
    m_fadeOutTime = Function::stringToSpeed(m_fadeOutEdit->text());
    m_fadeOutEdit->setText(Function::speedToString(m_fadeOutTime));
    if (m_speedDials != NULL)
        m_speedDials->setFadeOutSpeed(m_fadeOutTime);
}

void VCButtonProperties::accept()
{
    m_button->setFunction(m_function);
    m_button->setKeySequence(m_inputSelWidget->keySequence());
    m_button->setInputSource(m_inputSelWidget->inputSource());
    m_button->enableStartupIntensity(m_intensityGroup->isChecked());
    m_button->setStartupIntensity(qreal(m_intensitySlider->value()) / qreal(100));

    /* Pull the per-fixture and per-group selections from the tree
       regardless of which radio is currently active so toggling between
       Action types preserves the user's choices. */
    QList<quint32> fixtures;
    QList<quint32> groups;
    for (int i = 0; i < m_selectionTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* root = m_selectionTree->topLevelItem(i);
        for (int j = 0; j < root->childCount(); ++j)
        {
            QTreeWidgetItem* child = root->child(j);
            if (child->checkState(0) != Qt::Checked)
                continue;
            QString kind = child->data(0, Qt::UserRole).toString();
            quint32 id = child->data(0, Qt::UserRole + 1).toUInt();
            if (kind == QStringLiteral("group"))
                groups.append(id);
            else
                fixtures.append(id);
        }
    }
    m_button->setSelectionFixtures(fixtures);
    m_button->setSelectionGroups(groups);

    /* Auto-rename when a single group is selected, but only if the
       current caption still looks like a wizard placeholder
       ("Group <N>") — never overwrite a caption the user has typed.
       Preserves the mode suffix (set/add/tog/rem) on the second line so
       reassigning a group keeps the visual mode indicator.

       Two-line format: "<group>\n<mode>". Older single-line/symbol
       formats are accepted and rewritten to the new two-line form. */
    if (groups.size() == 1 && fixtures.isEmpty())
    {
        FixtureGroup* grp = m_doc->fixtureGroup(groups.first());
        if (grp != NULL)
        {
            QRegularExpression placeholder(
                QString::fromUtf8(
                    "^(Group |G)\\d+"
                    "(\\n(set|add|tog|rem)|"
                    " [+\xe2\x88\x92\xe2\x86\xbb])?$"));
            QString caption = m_nameEdit->text();
            QRegularExpressionMatch m = placeholder.match(caption);
            if (caption.isEmpty()
                || caption == tr("Clear")
                || m.hasMatch())
            {
                // Preserve the mode word from the existing placeholder
                // so an assigned group still carries its action label.
                QString mode = m.captured(3); // "set"|"add"|"tog"|"rem" or empty
                if (mode.isEmpty() && m.hasMatch())
                {
                    // Older symbol form: map the symbol back to a word.
                    const QString sym = m.captured(2).trimmed();
                    if (sym == QStringLiteral("+"))      mode = QStringLiteral("add");
                    else if (sym == QString::fromUtf8("\xe2\x88\x92")) mode = QStringLiteral("rem");
                    else if (sym == QString::fromUtf8("\xe2\x86\xbb")) mode = QStringLiteral("tog");
                }
                if (mode.isEmpty())
                {
                    // No mode hint in the caption — derive from current
                    // SelectionMode radio state so we still produce the
                    // right second line.
                    if (m_selectionAddRadio->isChecked())          mode = QStringLiteral("add");
                    else if (m_selectionRemoveRadio->isChecked())  mode = QStringLiteral("rem");
                    else if (m_selectionToggleRadio->isChecked())  mode = QStringLiteral("tog");
                    else                                            mode = QStringLiteral("set");
                }
                m_nameEdit->setText(grp->name() + QStringLiteral("\n") + mode);
            }
        }
    }

    // Apply caption AFTER auto-rename so the renamed text reaches the
    // button. (Was sequenced wrong before — caption was set before the
    // rename so the auto-update never landed on the button.)
    m_button->setCaption(m_nameEdit->text());

    if (m_selectionAddRadio->isChecked())
        m_button->setSelectionMode(VCButton::SelectAdd);
    else if (m_selectionRemoveRadio->isChecked())
        m_button->setSelectionMode(VCButton::SelectRemove);
    else if (m_selectionToggleRadio->isChecked())
        m_button->setSelectionMode(VCButton::SelectToggle);
    else
        m_button->setSelectionMode(VCButton::SelectReplace);

    if (m_toggle->isChecked() == true)
        m_button->setAction(VCButton::Toggle);
    else if (m_blackout->isChecked() == true)
        m_button->setAction(VCButton::Blackout);
    else if (m_stopAll->isChecked() == true)
    {
        m_button->setAction(VCButton::StopAll);
        m_button->setStopAllFadeOutTime(m_fadeOutTime);
    }
    else if (m_selectFixtures->isChecked() == true)
    {
        m_button->setAction(VCButton::SelectFixtures);
    }
    else
    {
        m_button->setAction(VCButton::Flash);
        m_button->setFlashOverride(m_overridePriority->isChecked());
        m_button->setFlashForceLTP(m_forceLTP->isChecked());
    }


    m_button->updateState();

    QDialog::accept();
}

