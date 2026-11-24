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
#include <QColor>
#include <QTimer>

#include "showfeatures.h"
#include "controlmappanel.h"
#include "controllearndialog.h"
#include "controlcollisionbox.h"
#include "controlowners.h"
#include "inputoutputmap.h"
#include "encoderbank.h"
#include "controlmap.h"
#include "doc.h"

namespace
{
enum Column { ControlCol = 0, TargetCol, PlaceCol, ModeCol, LampCol };
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
    m_empty = new QLabel(tr("No triggers yet. Right-click a cue list, cue or hit in the Show Tree (or a "
                            "scene or effect in Build), and choose Learn trigger…; then press the "
                            "key, pad or button that should fire it."), this);
    m_empty->setObjectName(QStringLiteral("controlMapEmpty"));
    m_empty->setWordWrap(true);
    m_empty->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(m_empty);
    m_featureOff = new QLabel(tr("Control surfaces are off for this show (View ▸ Workspace ▸ Tabs & "
                                 "Features…): the triggers and the encoder bank are put away, and a "
                                 "learned control still fires. The emergency stop is always here."), this);
    m_featureOff->setObjectName(QStringLiteral("controlMapFeatureOff"));
    m_featureOff->setWordWrap(true);
    m_featureOff->setStyleSheet(QStringLiteral("color: gray;"));
    m_featureOff->hide();
    lay->addWidget(m_featureOff);

    m_table = new QTableWidget(0, 5, this);
    m_table->setObjectName(QStringLiteral("controlMapTable"));
    m_table->setHorizontalHeaderLabels({ tr("Control"), tr("Fires"), tr("Works"), tr("When pressed"),
                                         tr("Its light") });
    m_table->horizontalHeaderItem(PlaceCol)->setToolTip(
        tr("Where the trigger works (its place): only while that cue is live, that cue list runs or that "
           "production is current -- or everywhere. One control can have a trigger in each place: the "
           "narrowest live place has it."));
    m_table->horizontalHeader()->setSectionResizeMode(ControlCol, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(TargetCol, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(PlaceCol, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ModeCol, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(LampCol, QHeaderView::Interactive);
    m_table->verticalHeader()->hide();
    // Names wrap, never cut (UX-36): a narrow panel shows "Scene: Warm
    // wash" on two lines rather than "Scene: War…".
    m_table->setWordWrap(true);
    m_table->setTextElideMode(Qt::ElideNone);
    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized, this, [this](int section) {
        if (section == TargetCol)
            m_table->resizeRowsToContents();
    });
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
    m_estop = new QPushButton(this);
    m_estop->setObjectName(QStringLiteral("controlMapEmergencyStop"));
    m_estop->setToolTip(tr("One button on a MIDI or OSC controller that is ALL OFF: every press stops "
                           "everything, from Design or Operate, at any moment. It is learned with a second "
                           "press to confirm, and never on a fader, knob or encoder."));
    m_dim = new QCheckBox(tr("Dim a control that is ready"), this);
    m_dim->setObjectName(QStringLiteral("controlMapDimReady"));
    m_dim->setToolTip(tr("A control whose press would fire something glows dim, and bright "
                         "while it runs. Switch this off for a pad whose lights take a "
                         "colour in the velocity and not a brightness (the APCs): a ready "
                         "control then stays dark, and running, GO held and stopped are "
                         "unchanged."));
    m_last = new QLabel(this);
    m_last->setObjectName(QStringLiteral("controlMapLast"));
    m_last->setStyleSheet(QStringLiteral("color: gray;"));
    row->addWidget(m_remove);
    row->addWidget(m_removeAll);
    row->addWidget(m_estop);
    row->addWidget(m_dim);
    row->addWidget(m_last, 1);
    lay->addLayout(row);
    buildEncoderBox(lay);

    connect(m_remove, &QPushButton::clicked, this, &ControlMapPanel::slotRemove);
    connect(m_removeAll, &QPushButton::clicked, this, &ControlMapPanel::slotRemoveAll);
    connect(m_estop, &QPushButton::clicked, this, &ControlMapPanel::slotEmergencyStop);
    connect(m_dim, &QCheckBox::toggled, this, [this](bool on) {
        m_doc->controlMap()->setDimWhenReady(on);
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_remove->setEnabled(!m_table->selectionModel()->selectedRows().isEmpty());
    });
    ControlMap *cm = m_doc->controlMap();
    connect(cm, &ControlMap::bindingsChanged, this, &ControlMapPanel::refresh);
    connect(cm, &ControlMap::fired, this, &ControlMapPanel::slotFired);
    connect(cm, &ControlMap::blocked, this, &ControlMapPanel::slotFired);
    // A profile that now calls the stop's control a fader: the row says so.
    connect(m_doc->inputOutputMap(), &InputOutputMap::profileChanged, this, [this]() { refresh(); });
    connect(cm, &ControlMap::stopStateChanged, this, &ControlMapPanel::refreshStop);
    // What each control's light says now (CS3): one source with the desk.
    connect(cm, &ControlMap::lampsChanged, this, &ControlMapPanel::refreshLamps);
    // A key's "Armed" follows the mode, the Run tab and the Show Tree.
    connect(cm, &ControlMap::armingChanged, this, &ControlMapPanel::refreshArming);
    // Names follow renames.
    connect(m_doc, &Doc::functionChanged, this, [this]() { if (isVisible()) refresh(); });
    connect(m_doc, &Doc::functionNameChanged, this, [this]() { if (isVisible()) refresh(); });
    connect(ShowFeatures::instance(), &ShowFeatures::changed, this, &ControlMapPanel::refresh);
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
        QTableWidgetItem *c = new QTableWidgetItem(controlText(b));
        if (b.target.type == ControlMap::EmergencyStopTarget && cm->stopHasProblem())
        {
            c->setText(stopRowText(b));
            c->setForeground(QColor(0xb0, 0x00, 0x20));
        }
        c->setData(TargetRole, b.target.id);
        c->setData(TypeRole, int(b.target.type));
        m_table->setItem(r, ControlCol, c);
        QTableWidgetItem *t = new QTableWidgetItem(cm->targetName(b.target));
        t->setToolTip(cm->targetName(b.target));
        if (!cm->targetExists(b.target))
            t->setForeground(Qt::gray);
        m_table->setItem(r, TargetCol, t);

        QComboBox *mode = new QComboBox(m_table);
        mode->setObjectName(QStringLiteral("controlMapMode"));
        // Narrow: what it fires keeps the room; the list shows every name in full.
        mode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        mode->setMinimumContentsLength(8);
        const bool list = b.target.type == ControlMap::CueListTarget || b.target.type == ControlMap::CueTarget;
        const bool estop = b.target.type == ControlMap::EmergencyStopTarget;
        const QList<ControlMap::Mode> modes = estop
            ? QList<ControlMap::Mode>{ ControlMap::StartOnly }
            : list
            ? QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StopOnly }
            : QList<ControlMap::Mode>{ ControlMap::Toggle, ControlMap::Momentary, ControlMap::StartOnly,
                                       ControlMap::StopOnly };
        for (ControlMap::Mode m : modes)
            mode->addItem(ControlMap::modeName(m, b.target.type), int(m));
        int at = mode->findData(int(b.mode));
        mode->setCurrentIndex(at < 0 ? 0 : at);
        mode->setEnabled(!estop);       // the stop has one way to act
        const ControlMap::Target target = b.target;
        connect(mode, QOverload<int>::of(&QComboBox::activated), this, [this, mode, target](int) {
            m_doc->controlMap()->setMode(target, ControlMap::Mode(mode->currentData().toInt()));
        });
        m_table->setCellWidget(r, ModeCol, mode);

        // Works: its place (Q295), changed here as at Learn.
        QComboBox *place = new QComboBox(m_table);
        place->setObjectName(QStringLiteral("controlMapPlace"));
        place->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        place->setMinimumContentsLength(10);
        QList<ControlMap::Place> places = estop ? QList<ControlMap::Place>{ ControlMap::Place() }
                                                : cm->placeChoices(b.place);
        if (!places.contains(b.place))
            places.prepend(b.place);
        for (const ControlMap::Place &p : std::as_const(places))
        {
            // A place another trigger on this control has is not offered:
            // moving there would take it (Learn refuses it, or steps through).
            if (p != b.place && cm->isPlaceTaken(b, p))
                continue;
            QString name = cm->placeName(p);
            if (!name.isEmpty())
                name[0] = name.at(0).toUpper();
            place->addItem(name, QVariantList{ int(p.scope), p.id });
            if (p == b.place)
                place->setCurrentIndex(place->count() - 1);
        }
        place->setToolTip(place->currentText());
        place->setEnabled(!estop && place->count() > 1);
        connect(place, QOverload<int>::of(&QComboBox::activated), this, [this, place, target](int) {
            const QVariantList v = place->currentData().toList();
            if (v.count() == 2
                && !m_doc->controlMap()->setPlace(target, ControlMap::Place{ ControlMap::PlaceScope(v.at(0).toInt()),
                                                                             v.at(1).toUInt() }))
                QTimer::singleShot(0, this, &ControlMapPanel::refresh);     // refused: back to where it is
        });
        m_table->setCellWidget(r, PlaceCol, place);

        m_table->setItem(r, LampCol, new QTableWidgetItem());
    }
    refreshStop();
    refreshLamps();
    const bool any = !bindings.isEmpty();
    /* Control surfaces off for the show (Q293): the triggers and the encoder
       bank are put away; the Emergency Stop button never is. */
    const bool surfaces = ShowFeatures::isOn(ShowFeatures::ControlSurfaces);
    m_empty->setVisible(surfaces && !any);
    m_table->setVisible(surfaces && any);
    m_remove->setVisible(surfaces);
    m_removeAll->setVisible(surfaces);
    m_dim->setVisible(surfaces && any);     // nothing is lit with no trigger
    m_featureOff->setVisible(!surfaces);
    if (m_encoderBox != nullptr)
        m_encoderBox->setVisible(surfaces);
    m_estop->setVisible(true);
    m_removeAll->setEnabled(any);
    m_remove->setEnabled(any && !m_table->selectionModel()->selectedRows().isEmpty());
    refreshEncoder();
    emit summaryChanged();
}

QString ControlMapPanel::lampText(const ControlMap::Binding &b) const
{
    ControlMap *cm = m_doc->controlMap();
    if (ControlMap::isKey(b))
        return tr("A key has no light");
    // A universe with no feedback line cannot light anything: say so here
    // rather than leave a column that never changes.
    if (m_doc->inputOutputMap()->feedbackPatch(b.universe) == nullptr)
        return tr("No feedback line");
    if (cm->isBlinking(b))
        return ControlMap::lampName(ControlMap::LampHeld);
    const ControlMap::Lamp lamp = cm->lamp(b);
    // Dark because the control is another trigger's here now (Q296), or its
    // place is not live: said, so the table answers "why is it dark".
    if (lamp == ControlMap::LampOff && b.target.type != ControlMap::EmergencyStopTarget
        && cm->targetExists(b.target) && !cm->isArmedOnControl(b))
    {
        if (!cm->isPlaceLive(b.place))
            return tr("Dark (not live here)");
        return tr("Dark (a narrower place has it)");
    }
    if (lamp == ControlMap::LampStandby && !cm->dimWhenReady())
        return tr("Ready (dark)");
    return ControlMap::lampName(lamp);
}

void ControlMapPanel::refreshLamps()
{
    ControlMap *cm = m_doc->controlMap();
    const QList<ControlMap::Binding> bindings = cm->bindings();
    bool resized = false;
    for (int r = 0; r < bindings.count() && r < m_table->rowCount(); ++r)
    {
        QTableWidgetItem *item = m_table->item(r, LampCol);
        if (item == nullptr)
            continue;
        const QString text = lampText(bindings.at(r));
        resized = resized || item->text() != text;
        item->setText(text);
        item->setToolTip(tr("What the control's light shows now: Dark (pressing it does "
                            "nothing here), Ready, Running, GO held (the double-GO guard "
                            "held a command: two blinks) or Stopped (after an emergency "
                            "stop, until the next GO)."));
    }
    if (m_dim != nullptr)
    {
        m_dim->blockSignals(true);
        m_dim->setChecked(cm->dimWhenReady());
        m_dim->blockSignals(false);
    }
    if (resized)
        fitRows();
}

void ControlMapPanel::fitRows()
{
    // The light's words wrap at about "No feedback line": what it fires
    // keeps the room.
    const int cap = m_table->fontMetrics().horizontalAdvance(tr("No feedback line")) + 16;
    m_table->resizeColumnToContents(LampCol);
    if (m_table->columnWidth(LampCol) > cap)
        m_table->setColumnWidth(LampCol, cap);
    m_table->resizeRowsToContents();
}

QString ControlMapPanel::controlText(const ControlMap::Binding &b) const
{
    ControlMap *cm = m_doc->controlMap();
    const QString name = cm->controlName(b.universe, b.channel);
    if (ControlMap::isKey(b) && m_doc->mode() == Doc::Design && cm->isArmed(b.target))
        return tr("%1 · Armed").arg(name);
    return name;
}

void ControlMapPanel::refreshArming()
{
    ControlMap *cm = m_doc->controlMap();
    if (!cm->hasKeyBindings() || !isVisible())
        return;
    const QList<ControlMap::Binding> bindings = cm->bindings();
    for (int r = 0; r < bindings.count() && r < m_table->rowCount(); ++r)
        if (ControlMap::isKey(bindings.at(r)))
            if (QTableWidgetItem *c = m_table->item(r, ControlCol))
                c->setText(controlText(bindings.at(r)));
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

QString ControlMapPanel::stopRowText(const ControlMap::Binding &b) const
{
    return tr("%1 · %2: learn it on a button").arg(controlText(b), m_doc->controlMap()->stopProblem());
}

void ControlMapPanel::refreshStop()
{
    // Only the stop's button and its row: a flagged stop is shown without
    // rebuilding the table for every message.
    ControlMap *cm = m_doc->controlMap();
    const ControlMap::Binding *stop = cm->binding(ControlMap::emergencyStop());
    const QString problem = cm->stopProblem();
    const bool dead = !problem.isEmpty();
    m_estop->setText(stop == nullptr ? tr("Emergency Stop…")
                                     : dead ? tr("Emergency Stop: %1 %2 — learn it on a button…")
                                                  .arg(cm->controlName(stop->universe, stop->channel), problem)
                                            : tr("Emergency Stop: %1…").arg(cm->controlName(stop->universe, stop->channel)));
    m_estop->setStyleSheet(dead ? QStringLiteral("color: white; background: #b00020;") : QString());
    m_estop->setProperty("stopFlagged", dead);
    m_estop->setProperty("stopProblem", problem);
    for (int r = 0; stop != nullptr && r < m_table->rowCount(); ++r)
    {
        QTableWidgetItem *c = m_table->item(r, ControlCol);
        if (c == nullptr || ControlMap::TargetType(c->data(TypeRole).toInt()) != ControlMap::EmergencyStopTarget)
            continue;
        c->setText(dead ? stopRowText(*stop) : controlText(*stop));
        c->setForeground(dead ? QBrush(QColor(0xb0, 0x00, 0x20)) : QBrush());
    }
}

void ControlMapPanel::slotEmergencyStop()
{
    ControlLearnDialog d(m_doc, ControlMap::emergencyStop(), this);
    d.exec();
}

void ControlMapPanel::slotFired(const QString &what)
{
    // A flagged stop stays in the stop's own line ("ALL OFF"), so firing does
    // not hide it; lines about other controls say nothing of it, and a line
    // that already says it is not told twice.
    const QString problem = m_doc->controlMap()->stopProblem();
    const bool stopsOwn = what == tr("ALL OFF");
    m_last->setText(problem.isEmpty() || !stopsOwn || what.contains(problem)
                        ? tr("Last: %1").arg(what)
                        : tr("Last: %1 · Emergency stop: %2").arg(what, problem));
}

/*****************************************************************************
 * Encoder bank
 *****************************************************************************/

void ControlMapPanel::buildEncoderBox(QVBoxLayout *lay)
{
    m_encoderBox = new QGroupBox(tr("Encoder bank"), this);
    m_encoderBox->setObjectName(QStringLiteral("encoderBankBox"));
    m_encoderBox->setToolTip(tr("A MIDI controller with endless encoders turns the attributes of what is "
                                "selected in the Workbench: banks for Intensity, Colour, Position, Beam and "
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
    m_encCollision = new ControlCollisionBox(m_encoderBox);
    m_encCollision->setObjectName(QStringLiteral("encoderCollision"));
    box->addWidget(m_encCollision);
    lay->addWidget(m_encoderBox);
    connect(m_encCollision, &ControlCollisionBox::moveRequested, this, [this]() {
        // Take the controls from there, then set them here.
        const int target = m_encPending;
        const quint32 u = m_encCollision->property("learnUniverse").toUInt();
        const quint32 ch = m_encCollision->property("learnChannel").toUInt();
        if (target == LearnNone || !ControlOwners::releaseAll(m_doc, m_encCollision->owners()))
            return;
        m_encCollision->clear();
        m_encPending = LearnNone;
        applyEncoderLearn(target, u, ch);
    });
    connect(m_encCollision, &ControlCollisionBox::cancelled, this, [this]() { m_encPending = LearnNone; });
    // The stop's control is refused by the Control Map itself while it
    // listens: the same refusal here, with no Move It Here.
    connect(m_doc->controlMap(), &ControlMap::refused, this, [this](const QString &) {
        const ControlMap::Binding *stop = m_doc->controlMap()->binding(ControlMap::emergencyStop());
        if (m_encLearning == LearnNone || stop == nullptr)
            return;
        ControlOwners::Owner self;
        self.kind = ControlOwners::Encoder;
        m_encPending = LearnNone;
        m_encCollision->refuse(m_doc, stop->universe, stop->channel,
                               ControlOwners::others(m_doc, stop->universe, stop->channel, self));
    });

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
        map = tr("Off. Pick the controller to turn the Workbench's selection with its encoders.");
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

void ControlMapPanel::hideEvent(QHideEvent *ev)
{
    // A learn left waiting when the panel goes away must not go on listening.
    if (m_encLearning != LearnNone)
        endEncoderLearn();
    QWidget::hideEvent(ev);
}

void ControlMapPanel::endEncoderLearn()
{
    if (m_encLearning != LearnNone)
        m_doc->controlMap()->stopListening();
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
    m_encCollision->clear();
    m_encPending = LearnNone;

    /* One job per control (Q297): every control the learn would hand the
       bank -- a run of encoders, pushes or bank buttons from the one heard --
       is asked who else listens to it. The rings are feedback lines: they
       hear nothing. */
    if (target != LearnRings)
    {
        const EncoderBank::Device d = m_doc->controlMap()->encoderBank()->device();
        const int run = target == LearnEncoders || target == LearnPushes ? d.encoderCount
                      : target == LearnDirect ? EncoderBank::bankCount() : 1;
        ControlOwners::Owner self;
        self.kind = ControlOwners::Encoder;
        QList<ControlOwners::Owner> taken;
        quint32 first = 0xFFFFFFFF;
        for (int i = 0; i < run; ++i)
        {
            const QList<ControlOwners::Owner> o = ControlOwners::others(m_doc, universe, channel + quint32(i), self);
            if (!o.isEmpty() && first == 0xFFFFFFFF)
                first = channel + quint32(i);
            for (const ControlOwners::Owner &one : o)
            {
                bool seen = false;
                for (const ControlOwners::Owner &t : std::as_const(taken))
                    seen = seen || t.sameAs(one);
                if (!seen)
                    taken << one;
            }
        }
        if (!taken.isEmpty())
        {
            m_encPending = target;
            m_encCollision->setProperty("learnUniverse", universe);
            m_encCollision->setProperty("learnChannel", channel);
            m_encCollision->refuse(m_doc, universe, first, taken);
            return;
        }
    }
    applyEncoderLearn(target, universe, channel);
}

void ControlMapPanel::applyEncoderLearn(int target, quint32 universe, quint32 channel)
{
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
