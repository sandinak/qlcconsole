/*
  Q Light Controller Plus
  controlmappanel.cpp

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

#include <QAbstractItemView>
#include <QGroupBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QAction>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>

#include "controlmappanel.h"
#include "inputoutputmap.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "doc.h"

namespace
{
enum Column { ControlCol = 0, TargetCol, ModeCol };
const int TargetRole = Qt::UserRole;
const int TypeRole = Qt::UserRole + 1;
}

ControlMapPanel::ControlMapPanel(Doc *doc, QWidget *parent)
    : QWidget(parent)
    , m_doc(doc)
{
    Q_ASSERT(doc != nullptr);
    setObjectName(QStringLiteral("controlMapPanel"));
    setProperty("helpTopic", QStringLiteral("other-improvements.md#control-map-midi-and-osc-triggers"));

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_empty = new QLabel(tr("No triggers yet. Right-click a scene or effect in Build, or a cue list, "
                            "cue or hit in the Show Tree, and choose Learn trigger…; then press the "
                            "key, pad or button that should fire it."), this);
    m_empty->setObjectName(QStringLiteral("controlMapEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_empty);

    m_table = new QTableWidget(0, 3, this);
    m_table->setObjectName(QStringLiteral("controlMapTable"));
    m_table->setHorizontalHeaderLabels({ tr("Control"), tr("Fires"), tr("When pressed") });
    m_table->horizontalHeader()->setSectionResizeMode(ControlCol, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(TargetCol, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ModeCol, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(140);
    lay->addWidget(m_table);

    QHBoxLayout *row = new QHBoxLayout();
    m_remove = new QPushButton(tr("Remove"), this);
    m_remove->setObjectName(QStringLiteral("controlMapRemove"));
    m_remove->setToolTip(tr("The selected triggers: their controls fire nothing (their lights go off)"));
    m_removeAll = new QPushButton(tr("Remove All…"), this);
    m_removeAll->setObjectName(QStringLiteral("controlMapRemoveAll"));
    m_last = new QLabel(this);
    m_last->setObjectName(QStringLiteral("controlMapLast"));
    m_last->setStyleSheet(QStringLiteral("color: gray;"));
    row->addWidget(m_remove);
    row->addWidget(m_removeAll);
    row->addWidget(m_last, 1);
    lay->addLayout(row);
    buildEncoderBox(lay);

    connect(m_remove, &QPushButton::clicked, this, &ControlMapPanel::slotRemove);
    connect(m_removeAll, &QPushButton::clicked, this, &ControlMapPanel::slotRemoveAll);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_remove->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
    });
    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &ControlMapPanel::refresh);
    connect(cm, &ControlMap::fired, this, &ControlMapPanel::slotFired);
    // Names follow renames.
    connect(m_doc, &Doc::functionChanged, this, [this]() { if (isVisible()) refresh(); });
    connect(m_doc, &Doc::functionNameChanged, this, [this]() { if (isVisible()) refresh(); });
    refresh();
}

QString ControlMapPanel::summary() const
{
    const int n = m_doc->controlMap()->count();
    const EncoderBank *enc = m_doc->controlMap()->encoderBank();
    QString text = n == 0 ? tr("None") : (n == 1 ? tr("1 trigger") : tr("%1 triggers").arg(n));
    if (enc->isEnabled())
        text = n == 0 ? tr("Encoder bank") : tr("%1, encoder bank").arg(text);
    return text;
}

void ControlMapPanel::refresh()
{
    ControlMap *cm = m_doc->controlMap();
    const QList<ControlMap::Binding> bindings = cm->bindings();
    m_table->setRowCount(0);
    m_table->setRowCount(bindings.count());
    for (int r = 0; r < bindings.count(); ++r)
    {
        const ControlMap::Binding &b = bindings.at(r);
        QTableWidgetItem *c = new QTableWidgetItem(cm->controlName(b.universe, b.channel));
        c->setData(TargetRole, b.target.id);
        c->setData(TypeRole, int(b.target.type));
        m_table->setItem(r, ControlCol, c);
        QTableWidgetItem *t = new QTableWidgetItem(cm->targetName(b.target));
        if (!cm->targetExists(b.target))
            t->setForeground(Qt::gray);
        m_table->setItem(r, TargetCol, t);

        QComboBox *mode = new QComboBox(m_table);
        mode->setObjectName(QStringLiteral("controlMapMode"));
        const bool list = b.target.type == ControlMap::CueListTarget || b.target.type == ControlMap::CueTarget;
        const QList<ControlMap::Mode> modes = list
            ? QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StopOnly }
            : QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StartOnly,
                                       ControlMap::StopOnly };
        for (ControlMap::Mode m : modes)
            mode->addItem(ControlMap::modeName(m, b.target.type), int(m));
        int at = mode->findData(int(b.mode));
        mode->setCurrentIndex(at < 0 ? 0 : at);
        const ControlMap::Target target = b.target;
        connect(mode, QOverload<int>::of(&QComboBox::activated), this, [this, mode, target](int) {
            m_doc->controlMap()->setMode(target, ControlMap::Mode(mode->currentData().toInt()));
        });
        m_table->setCellWidget(r, ModeCol, mode);
    }
    const bool any = !bindings.isEmpty();
    m_empty->setVisible(!any);
    m_table->setVisible(any);
    m_removeAll->setEnabled(any);
    m_remove->setEnabled(any && !m_table->selectionModel()->selectedRows().isEmpty());
    refreshEncoder();
    emit summaryChanged();
}

void ControlMapPanel::showEvent(QShowEvent *ev)
{
    refresh();      // names of things renamed since
    QWidget::showEvent(ev);
}

void ControlMapPanel::slotRemove()
{
    QList<ControlMap::Target> targets;
    for (const QModelIndex &i : m_table->selectionModel()->selectedRows())
    {
        QTableWidgetItem *it = m_table->item(i.row(), ControlCol);
        if (it != nullptr)
            targets << ControlMap::Target{ ControlMap::TargetType(it->data(TypeRole).toInt()),
                                           it->data(TargetRole).toUInt() };
    }
    for (const ControlMap::Target &t : targets)
        m_doc->controlMap()->removeBinding(t);
}

void ControlMapPanel::slotRemoveAll()
{
    const int n = m_doc->controlMap()->count();
    if (n == 0)
        return;
    if (property("noConfirm").toBool()
        || QMessageBox::question(this, tr("Remove all triggers"),
                                 n == 1 ? tr("Remove the trigger? Its control will fire nothing.")
                                       : tr("Remove all %1 triggers? Their controls will fire nothing.").arg(n))
               == QMessageBox::Yes)
        m_doc->controlMap()->removeAll();
}

void ControlMapPanel::slotFired(const QString &what)
{
    m_last->setText(tr("Last: %1").arg(what));
}

/*****************************************************************************
 * Encoder bank
 *****************************************************************************/

void ControlMapPanel::buildEncoderBox(QVBoxLayout *lay)
{
    m_encoderBox = new QGroupBox(tr("Encoder bank"), this);
    m_encoderBox->setObjectName(QStringLiteral("encoderBankBox"));
    m_encoderBox->setToolTip(tr("A MIDI controller with endless encoders turns the attributes of what is "
                                "selected in Build: banks for Intensity, Colour, Position, Beam and "
                                "Gobo / prism"));
    QVBoxLayout *box = new QVBoxLayout(m_encoderBox);

    QHBoxLayout *top = new QHBoxLayout();
    m_encPreset = new QComboBox(m_encoderBox);
    m_encPreset->setObjectName(QStringLiteral("encoderPreset"));
    m_encPreset->addItem(tr("Off"), QString());
    for (const QString &key : EncoderBank::presetKeys())
        m_encPreset->addItem(EncoderBank::presetName(key), key);
    m_encPreset->addItem(tr("Custom"), QStringLiteral("custom"));
    m_encUniverse = new QComboBox(m_encoderBox);
    m_encUniverse->setObjectName(QStringLiteral("encoderUniverse"));
    m_encUniverse->setToolTip(tr("The input universe the controller is patched to"));
    m_encEncoding = new QComboBox(m_encoderBox);
    m_encEncoding->setObjectName(QStringLiteral("encoderEncoding"));
    m_encEncoding->setToolTip(tr("How the encoders say which way they turned. If a knob runs backwards "
                                 "or jumps, try another"));
    for (int e = EncoderBank::TwosComplement; e <= EncoderBank::AbsoluteDelta; ++e)
        m_encEncoding->addItem(EncoderBank::encodingName(EncoderBank::Encoding(e)), e);
    m_encInvert = new QCheckBox(tr("Reverse"), m_encoderBox);
    m_encInvert->setObjectName(QStringLiteral("encoderInvert"));
    m_encInvert->setToolTip(tr("Clockwise lowers the value"));
    m_encLearn = new QPushButton(tr("Learn Encoder 1…"), m_encoderBox);
    m_encLearn->setObjectName(QStringLiteral("encoderLearn"));
    m_encLearn->setToolTip(tr("Turn the first encoder of the controller: the other encoders are the "
                              "channels after it"));
    m_encLearnMore = new QPushButton(tr("Learn More…"), m_encoderBox);
    m_encLearnMore->setObjectName(QStringLiteral("encoderLearnMore"));
    m_encLearnMore->setToolTip(tr("Press or turn the control: it is set as the pushes, a bank button or the "
                                  "rings. The ones that follow are the controls after it"));
    QMenu *more = new QMenu(m_encLearnMore);
    auto learnItem = [&](const QString &text, const QString &name, int target) {
        QAction *a = more->addAction(text);
        a->setObjectName(name);
        a->setData(target);
    };
    learnItem(tr("Push of encoder 1…"), QStringLiteral("encoderLearnPushes"), LearnPushes);
    learnItem(tr("Next-bank button…"), QStringLiteral("encoderLearnNext"), LearnNext);
    learnItem(tr("Previous-bank button…"), QStringLiteral("encoderLearnPrevious"), LearnPrevious);
    learnItem(tr("Bank 1 button (the next four follow)…"), QStringLiteral("encoderLearnDirect"), LearnDirect);
    learnItem(tr("Ring of encoder 1 (turn that encoder)…"), QStringLiteral("encoderLearnRings"), LearnRings);
    more->addSeparator();
    QAction *forget = more->addAction(tr("Forget the Pushes, Bank Buttons and Rings"));
    forget->setObjectName(QStringLiteral("encoderForget"));
    forget->setData(-1);
    m_encLearnMore->setMenu(more);
    top->addWidget(new QLabel(tr("Controller"), m_encoderBox));
    top->addWidget(m_encPreset, 1);
    top->addWidget(m_encUniverse);
    box->addLayout(top);
    QHBoxLayout *mid = new QHBoxLayout();
    mid->addWidget(new QLabel(tr("Encoders"), m_encoderBox));
    mid->addWidget(m_encEncoding, 1);
    mid->addWidget(m_encInvert);
    mid->addWidget(m_encLearn);
    mid->addWidget(m_encLearnMore);
    box->addLayout(mid);

    m_encMap = new QLabel(m_encoderBox);
    m_encMap->setObjectName(QStringLiteral("encoderMap"));
    m_encMap->setWordWrap(true);
    box->addWidget(m_encMap);
    m_encNote = new QLabel(m_encoderBox);
    m_encNote->setObjectName(QStringLiteral("encoderNote"));
    m_encNote->setWordWrap(true);
    m_encNote->setStyleSheet(QStringLiteral("color: gray;"));
    box->addWidget(m_encNote);
    lay->addWidget(m_encoderBox);

    connect(m_encPreset, QOverload<int>::of(&QComboBox::activated), this, &ControlMapPanel::slotEncoderPreset);
    connect(m_encUniverse, QOverload<int>::of(&QComboBox::activated), this, &ControlMapPanel::slotEncoderEdited);
    connect(m_encEncoding, QOverload<int>::of(&QComboBox::activated), this, &ControlMapPanel::slotEncoderEdited);
    connect(m_encInvert, &QCheckBox::clicked, this, &ControlMapPanel::slotEncoderEdited);
    connect(m_encLearn, &QPushButton::clicked, this, &ControlMapPanel::slotEncoderLearn);
    connect(more, &QMenu::triggered, this, &ControlMapPanel::slotEncoderLearnMore);
    connect(m_doc->controlMap(), &ControlMap::heard, this, &ControlMapPanel::slotEncoderHeard);
}

void ControlMapPanel::refreshEncoder()
{
    if (m_encShowing)
        return;
    m_encShowing = true;
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    const EncoderBank::Device d = enc->device();

    QString key = enc->presetKey();
    if (!enc->isEnabled() || d.encoders < 0)
        key.clear();
    else if (m_encPreset->findData(key) < 0)
        key = QStringLiteral("custom");
    m_encPreset->setCurrentIndex(qMax(0, m_encPreset->findData(key)));

    // The input universes of the show.
    m_encUniverse->clear();
    InputOutputMap *iom = m_doc->inputOutputMap();
    const int count = iom != nullptr ? int(iom->universesCount()) : 1;
    for (int u = 0; u < qMax(1, count); ++u)
        m_encUniverse->addItem(tr("Universe %1").arg(u + 1), u);
    m_encUniverse->setCurrentIndex(qBound(0, int(d.universe), m_encUniverse->count() - 1));

    m_encEncoding->setCurrentIndex(qMax(0, m_encEncoding->findData(int(d.encoding))));
    m_encInvert->setChecked(d.invert);

    const bool on = enc->isEnabled();
    m_encUniverse->setEnabled(on);
    m_encEncoding->setEnabled(on);
    m_encInvert->setEnabled(on);
    m_encLearn->setEnabled(on);
    m_encLearnMore->setEnabled(on);

    QString map;
    if (d.encoders >= 0)
    {
        map = tr("Encoders from %1").arg(EncoderBank::controlName(d.encoders));
        if (d.pushes >= 0)
            map += tr(" · pushes from %1").arg(EncoderBank::controlName(d.pushes));
        if (d.bankNext >= 0)
            map += tr(" · next bank %1").arg(EncoderBank::controlName(d.bankNext));
        if (d.bankPrevious >= 0)
            map += tr(" · previous %1").arg(EncoderBank::controlName(d.bankPrevious));
        if (d.bankDirect >= 0)
            map += tr(" · bank buttons from %1").arg(EncoderBank::controlName(d.bankDirect));
        if (d.rings >= 0)
            map += tr(" · rings from %1").arg(EncoderBank::controlName(d.rings));
        if (!on)
            map += tr(" (off)");
    }
    else
    {
        map = tr("Off. Pick the controller to turn Build's selection with its encoders.");
    }
    m_encMap->setText(map);
    m_encNote->setText(key.isEmpty() || key == QLatin1String("custom") ? QString() : EncoderBank::presetNote(key));
    m_encNote->setVisible(!m_encNote->text().isEmpty());
    m_encShowing = false;
}

void ControlMapPanel::slotEncoderPreset(int index)
{
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    const QString key = m_encPreset->itemData(index).toString();
    if (key.isEmpty())
    {
        enc->setEnabled(false);
    }
    else if (key == QLatin1String("custom"))
    {
        if (enc->device().encoders >= 0)
            enc->setEnabled(true);
    }
    else
    {
        EncoderBank::Device d = enc->device();
        d.universe = quint32(qMax(0, m_encUniverse->currentData().toInt()));
        if (EncoderBank::preset(key, &d))
            enc->setDevice(d, key);
    }
    refresh();
}

void ControlMapPanel::slotEncoderEdited()
{
    if (m_encShowing)
        return;
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    EncoderBank::Device d = enc->device();
    if (d.encoders < 0)
        return;
    d.universe = quint32(qMax(0, m_encUniverse->currentData().toInt()));
    d.encoding = EncoderBank::Encoding(m_encEncoding->currentData().toInt());
    d.invert = m_encInvert->isChecked();
    enc->setDevice(d, enc->presetKey());
    refresh();
}

void ControlMapPanel::startEncoderLearn(int target)
{
    endEncoderLearn();
    m_encLearning = target;
    if (target == LearnEncoders)
    {
        m_encLearn->setText(tr("Turn Encoder 1…"));
    }
    else
    {
        const QString what = target == LearnPushes ? tr("Push encoder 1…")
                           : target == LearnNext ? tr("Press next bank…")
                           : target == LearnPrevious ? tr("Press previous bank…")
                           : target == LearnDirect ? tr("Press bank 1…") : tr("Turn Encoder 1…");
        m_encLearnMore->setText(what);
    }
    m_doc->controlMap()->listen();
}

void ControlMapPanel::endEncoderLearn()
{
    m_encLearning = LearnNone;
    m_encLearn->setText(tr("Learn Encoder 1…"));
    m_encLearnMore->setText(tr("Learn More…"));
}

void ControlMapPanel::slotEncoderLearn()
{
    startEncoderLearn(LearnEncoders);
}

void ControlMapPanel::slotEncoderLearnMore(QAction *action)
{
    const int target = action->data().toInt();
    if (target < 0)
    {
        // Forget: the controls a preset or a learn set, other than the encoders.
        EncoderBank *enc = m_doc->controlMap()->encoderBank();
        EncoderBank::Device d = enc->device();
        d.pushes = d.rings = d.bankNext = d.bankPrevious = d.bankDirect = d.bankLamps = -1;
        enc->setDevice(d, QStringLiteral("custom"));
        refresh();
        return;
    }
    startEncoderLearn(target);
}

void ControlMapPanel::slotEncoderHeard(quint32 universe, quint32 channel)
{
    if (m_encLearning == LearnNone)
        return;
    const int target = m_encLearning;
    endEncoderLearn();
    EncoderBank *enc = m_doc->controlMap()->encoderBank();
    EncoderBank::Device d = enc->device();
    d.universe = universe;
    const qint32 ch = qint32(channel);
    switch (target)
    {
    case LearnEncoders: d.encoders = ch; break;
    case LearnPushes: d.pushes = ch; break;
    case LearnNext: d.bankNext = ch; break;
    case LearnPrevious: d.bankPrevious = ch; break;
    case LearnDirect:
        // The lamps are the buttons' own lines unless they were set apart.
        if (d.bankLamps < 0 || d.bankLamps == d.bankDirect)
            d.bankLamps = ch;
        d.bankDirect = ch;
        break;
    case LearnRings: d.rings = ch; break;
    }
    enc->setDevice(d, QStringLiteral("custom"));
    refresh();
}
