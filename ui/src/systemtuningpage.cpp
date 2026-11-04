/*
  Q Light Controller Plus - qlcconsole
  systemtuningpage.cpp

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

#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QClipboard>
#include <QLabel>

#include "systemtuningpage.h"
#include "systemprobe.h"

SystemTuningPage::SystemTuningPage(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("settingsPerformance"));
    setProperty("helpTopic", QStringLiteral("system-tuning.md"));
    m_probe = [this]() { return SystemTuning::probe(m_doc); };

    QVBoxLayout *layout = new QVBoxLayout(this);

    QGroupBox *operate = new QGroupBox(tr("During Operate"), this);
    QVBoxLayout *opLayout = new QVBoxLayout(operate);
#if defined(Q_OS_MACOS)
    m_fullSpeed = new QCheckBox(tr("Run at full speed (no App Nap, no idle sleep)"), operate);
#else
    m_fullSpeed = new QCheckBox(tr("Run at full speed (no power throttling, no idle sleep)"), operate);
#endif
    m_fullSpeed->setObjectName(QStringLiteral("tuningFullSpeed"));
    m_fullSpeed->setChecked(SystemTuning::fullSpeedInOperate());
    m_display = new QCheckBox(tr("Keep the display awake"), operate);
    m_display->setObjectName(QStringLiteral("tuningDisplay"));
    m_display->setChecked(SystemTuning::keepDisplayAwakeInOperate());
    if (SystemProbe::canHoldFullSpeed() == false)
    {
        const QString why = tr("This system has nothing for the app to hold: see the list below.");
        m_fullSpeed->setEnabled(false);
        m_fullSpeed->setToolTip(why);
        m_display->setEnabled(false);
        m_display->setToolTip(why);
    }
    m_warn = new QCheckBox(tr("Say in Show Status when something marked important below is off"), operate);
    m_warn->setObjectName(QStringLiteral("tuningWarn"));
    m_warn->setChecked(SystemTuning::warnInOperate());
    opLayout->addWidget(m_fullSpeed);
    opLayout->addWidget(m_display);
    opLayout->addWidget(m_warn);
    layout->addWidget(operate);

    QGroupBox *check = new QGroupBox(tr("This computer"), this);
    QVBoxLayout *checkLayout = new QVBoxLayout(check);
    QLabel *intro = new QLabel(tr("What helps the DMX timer send every frame on time (50 a second). "
                                  "The app changes nothing here that needs an administrator: "
                                  "copy the command, or follow the steps."), check);
    intro->setWordWrap(true);
    checkLayout->addWidget(intro);

    m_tree = new QTreeWidget(check);
    m_tree->setObjectName(QStringLiteral("tuningItems"));
    m_tree->setRootIsDecorated(false);
    m_tree->setHeaderLabels(QStringList() << tr("Status") << tr("Item") << tr("Now"));
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setMinimumHeight(120);
    checkLayout->addWidget(m_tree, 1);

    m_what = new QLabel(check);
    m_what->setWordWrap(true);
    checkLayout->addWidget(m_what);
    m_howTo = new QLabel(check);
    m_howTo->setWordWrap(true);
    m_howTo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    checkLayout->addWidget(m_howTo);

    QHBoxLayout *cmdRow = new QHBoxLayout();
    m_command = new QPlainTextEdit(check);
    m_command->setObjectName(QStringLiteral("tuningCommand"));
    m_command->setReadOnly(true);
    m_command->setMaximumHeight(m_command->fontMetrics().lineSpacing() * 4);
    m_copy = new QPushButton(tr("Copy"), check);
    m_copy->setObjectName(QStringLiteral("tuningCopy"));
    m_copy->setToolTip(tr("Copy the command, to paste in a terminal"));
    cmdRow->addWidget(m_command, 1);
    cmdRow->addWidget(m_copy, 0, Qt::AlignTop);
    checkLayout->addLayout(cmdRow);

    QPushButton *again = new QPushButton(tr("Check Again"), check);
    again->setObjectName(QStringLiteral("tuningCheckAgain"));
    checkLayout->addWidget(again, 0, Qt::AlignLeft);
    layout->addWidget(check, 1);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this]() { showSelected(); });
    connect(m_copy, &QPushButton::clicked, this, &SystemTuningPage::copyCommand);
    connect(again, &QPushButton::clicked, this, &SystemTuningPage::refresh);
    /* The full-speed item follows the box, before OK */
    connect(m_fullSpeed, &QCheckBox::toggled, this, &SystemTuningPage::refresh);

    refresh();
}

void SystemTuningPage::setProbe(const std::function<SystemTuning::Probe()> &probeFn)
{
    m_probe = probeFn;
    refresh();
}

void SystemTuningPage::refresh()
{
    SystemTuning::Probe p = m_probe();
    p.fullSpeedInOperate = m_fullSpeed->isChecked();
    m_items = SystemTuning::check(p);

    const QString key = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString()
                                              : QString();
    m_tree->clear();
    QTreeWidgetItem *select = nullptr;
    for (const SystemTuning::Item &it : m_items)
    {
        QTreeWidgetItem *row = new QTreeWidgetItem(m_tree);
        QString status = SystemTuning::statusText(it.status);
        if (it.important && it.status == SystemTuning::CanImprove)
            status += QStringLiteral(" ●");
        row->setText(0, status);
        row->setText(1, it.title);
        row->setText(2, it.state);
        row->setToolTip(2, it.state);
        row->setData(0, Qt::UserRole, it.key);
        if (it.status == SystemTuning::CanImprove)
            row->setForeground(0, QColor(0xd0, 0x8a, 0x1c));
        else if (it.status == SystemTuning::Good)
            row->setForeground(0, QColor(0x3a, 0x9a, 0x4a));
        if (it.key == key || (select == nullptr && key.isEmpty() && it.status == SystemTuning::CanImprove))
            select = row;
    }
    if (select == nullptr)
        select = m_tree->topLevelItem(0);
    m_tree->setCurrentItem(select);
    showSelected();
}

void SystemTuningPage::showSelected()
{
    const int row = m_tree->currentItem() ? m_tree->indexOfTopLevelItem(m_tree->currentItem()) : -1;
    if (row < 0 || row >= m_items.size())
    {
        m_what->clear();
        m_howTo->clear();
        m_command->clear();
        m_command->setVisible(false);
        m_copy->setVisible(false);
        return;
    }
    const SystemTuning::Item &it = m_items.at(row);
    m_what->setText(it.important ? it.what + QLatin1Char(' ') + tr("(● important: said in Show Status during Operate)")
                                 : it.what);
    m_howTo->setText(it.howTo);
    m_howTo->setVisible(it.howTo.isEmpty() == false);
    m_command->setPlainText(it.command);
    m_command->setVisible(it.command.isEmpty() == false);
    m_copy->setVisible(it.command.isEmpty() == false);
}

void SystemTuningPage::copyCommand()
{
    if (m_command->toPlainText().isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_command->toPlainText());
}

void SystemTuningPage::apply()
{
    SystemTuning::setFullSpeedInOperate(m_fullSpeed->isChecked());
    SystemTuning::setKeepDisplayAwakeInOperate(m_display->isChecked());
    SystemTuning::setWarnInOperate(m_warn->isChecked());
    if (SystemTuning::instance() != nullptr)
        SystemTuning::instance()->apply();
}
