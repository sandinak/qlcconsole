/*
  Q Light Controller Plus
  focuscallsdialog.cpp

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
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QFileInfo>
#include <QLabel>

#include "programmercontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include <QGuiApplication>

#include "focuscallsdialog.h"
#include "operatorvoice.h"
#include "focusmodepanel.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "doc.h"

namespace
{
const int kNewVenue = -2;
const int kNoVenue = -1;
}

FocusCallsDialog::FocusCallsDialog(Doc *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("focusCallsDialog"));
    setWindowTitle(tr("Focus Calls"));
    setProperty("helpTopic", QStringLiteral("focus-calls.md"));

    QVBoxLayout *vl = new QVBoxLayout(this);
    QLabel *intro = new QLabel(tr("The order the rig is focused in when the show moves into a "
                                  "new room. Each call is some groups and the point they focus "
                                  "to. On the controller the triggers step calls, the bumpers "
                                  "fixtures, A saves."), this);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    QHBoxLayout *main = new QHBoxLayout;
    QVBoxLayout *left = new QVBoxLayout;
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("fcCalls"));
    left->addWidget(m_list, 1);
    QHBoxLayout *listButtons = new QHBoxLayout;
    QPushButton *add = new QPushButton(tr("+ Call"), this);
    add->setObjectName(QStringLiteral("fcAdd"));
    QPushButton *remove = new QPushButton(tr("Remove"), this);
    remove->setObjectName(QStringLiteral("fcRemove"));
    QPushButton *up = new QPushButton(tr("Up"), this);
    QPushButton *down = new QPushButton(tr("Down"), this);
    for (QPushButton *b : { add, remove, up, down })
        listButtons->addWidget(b);
    left->addLayout(listButtons);
    main->addLayout(left, 1);

    QFormLayout *form = new QFormLayout;
    m_name = new QLineEdit(this);
    m_name->setObjectName(QStringLiteral("fcName"));
    m_name->setPlaceholderText(tr("Named from the groups and target"));
    form->addRow(tr("Name"), m_name);
    m_groups = new QListWidget(this);
    m_groups->setObjectName(QStringLiteral("fcGroups"));
    for (const FixtureGroup *g : m_doc->fixtureGroups())
    {
        QListWidgetItem *it = new QListWidgetItem(g->name(), m_groups);
        it->setData(Qt::UserRole, g->id());
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(Qt::Unchecked);
    }
    form->addRow(tr("Groups"), m_groups);
    m_target = new QComboBox(this);
    m_target->setObjectName(QStringLiteral("fcTarget"));
    m_target->addItem(tr("Where they land (fixed lights)"), QVariant(0xFFFFFFFFu));
    if (MonitorProperties *props = m_doc->monitorProperties())
        for (const StageTarget *t : props->stageTargets())
            m_target->addItem(t->name(), QVariant(t->id()));
    form->addRow(tr("Focus to"), m_target);
    m_note = new QLineEdit(this);
    m_note->setObjectName(QStringLiteral("fcNote"));
    m_note->setPlaceholderText(tr("A word for the crew: \"cut off the proscenium\""));
    form->addRow(tr("Note"), m_note);
    main->addLayout(form, 2);
    vl->addLayout(main, 1);

    // The venue the corrections are kept for (board Q105).
    QHBoxLayout *venueRow = new QHBoxLayout;
    venueRow->addWidget(new QLabel(tr("Venue:"), this));
    m_venue = new QComboBox(this);
    m_venue->setObjectName(QStringLiteral("fcVenue"));
    venueRow->addWidget(m_venue, 1);
    m_newVenue = new QLineEdit(this);
    m_newVenue->setObjectName(QStringLiteral("fcNewVenue"));
    m_newVenue->setPlaceholderText(VenueCorrections::defaultVenueName());
    venueRow->addWidget(m_newVenue, 1);
    // The venue manager: apply now, rename, delete (the engine's own).
    m_applyVenue = new QPushButton(tr("Apply now"), this);
    m_applyVenue->setObjectName(QStringLiteral("fcApplyVenue"));
    m_applyVenue->setToolTip(tr("Put this venue's corrections on the rig now (or, for None, the show's own)"));
    m_renameVenue = new QPushButton(tr("Rename\342\200\246"), this);
    m_renameVenue->setObjectName(QStringLiteral("fcRenameVenue"));
    m_deleteVenue = new QPushButton(tr("Delete"), this);
    m_deleteVenue->setObjectName(QStringLiteral("fcDeleteVenue"));
    m_deleteVenue->setToolTip(tr("Forget this venue and its corrections (the show is not changed)"));
    for (QPushButton *b : { m_applyVenue, m_renameVenue, m_deleteVenue })
        venueRow->addWidget(b);
    vl->addLayout(venueRow);
    // The voice is the crew's cue: send it to the output that feeds the headsets.
    {
        const QStringList devices = QGuiApplication::platformName() == QLatin1String("offscreen")
                                    ? QStringList() : OperatorVoice::outputDevices();
        if (!devices.isEmpty())
        {
            QHBoxLayout *voiceRow = new QHBoxLayout;
            voiceRow->addWidget(new QLabel(tr("Voice to:"), this));
            QComboBox *voice = new QComboBox(this);
            voice->setObjectName(QStringLiteral("fcVoiceDevice"));
            voice->addItem(tr("The system's output"), QString());
            for (const QString &d : devices)
                voice->addItem(d, d);
            const int at = voice->findData(OperatorVoice::outputDevice());
            voice->setCurrentIndex(qMax(0, at));
            voice->setToolTip(tr("Where the spoken calls go: the output that feeds the crew's headsets"));
            connect(voice, &QComboBox::currentIndexChanged, this, [voice]() {
                OperatorVoice::setOutputDevice(voice->currentData().toString());
            });
            voiceRow->addWidget(voice, 1);
            vl->addLayout(voiceRow);
        }
    }
    m_venueNote = new QLabel(this);
    m_venueNote->setObjectName(QStringLiteral("fcVenueNote"));
    m_venueNote->setWordWrap(true);
    vl->addWidget(m_venueNote);

    QDialogButtonBox *bb = new QDialogButtonBox(this);
    m_start = bb->addButton(tr("Start focus calls"), QDialogButtonBox::AcceptRole);
    m_start->setObjectName(QStringLiteral("fcStart"));
    QPushButton *save = bb->addButton(tr("Save calls"), QDialogButtonBox::ApplyRole);
    save->setObjectName(QStringLiteral("fcSave"));
    bb->addButton(QDialogButtonBox::Cancel);
    vl->addWidget(bb);

    fillVenues(QString());
    connect(m_applyVenue, &QPushButton::clicked, this, [this]() {
        QString why;
        if (!switchVenue(venueChoice(), &why))
            QMessageBox::information(this, windowTitle(), why);
    });
    connect(m_renameVenue, &QPushButton::clicked, this, [this]() {
        const QString from = venueAt(m_venue->currentIndex());
        bool ok = false;
        const QString to = QInputDialog::getText(this, tr("Rename venue"), tr("Venue name:"),
                                                 QLineEdit::Normal, from, &ok);
        if (ok && !renameVenue(from, to))
            QMessageBox::information(this, windowTitle(),
                                     tr("\342\200\234%1\342\200\235 is empty or already a venue.").arg(to));
    });
    connect(m_deleteVenue, &QPushButton::clicked, this, [this]() {
        const QString name = venueAt(m_venue->currentIndex());
        if (QMessageBox::question(this, windowTitle(),
                tr("Forget the venue \342\200\234%1\342\200\235 and its corrections? The show is not changed.")
                .arg(name)) == QMessageBox::Yes)
            deleteVenue(name);
    });

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        storeCall();
        loadCall(row);
    });
    connect(add, &QPushButton::clicked, this, &FocusCallsDialog::addCall);
    connect(remove, &QPushButton::clicked, this, &FocusCallsDialog::removeCall);
    connect(up, &QPushButton::clicked, this, [this]() { moveCall(-1); });
    connect(down, &QPushButton::clicked, this, [this]() { moveCall(1); });
    for (QLineEdit *e : { m_name, m_note })
        connect(e, &QLineEdit::textEdited, this, [this]() { storeCall(); fillCallList(); });
    connect(m_groups, &QListWidget::itemChanged, this, [this]() { storeCall(); fillCallList(); });
    connect(m_target, &QComboBox::currentIndexChanged, this, [this]() { storeCall(); fillCallList(); });
    connect(m_venue, &QComboBox::currentIndexChanged, this, &FocusCallsDialog::refreshVenue);
    connect(save, &QPushButton::clicked, this, [this]() {
        storeCall();
        m_doc->programmer()->focusCalls()->setCalls(m_calls);
    });
    connect(bb, &QDialogButtonBox::accepted, this, [this]() {
        QString why;
        if (!apply(&why))
        {
            QMessageBox::information(this, windowTitle(), why);
            return;
        }
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setCalls(m_doc->programmer()->focusCalls()->calls());
    refreshVenue();
    resize(760, 480);
}

void FocusCallsDialog::run(Doc *doc, QWidget *parent)
{
    FocusCallsDialog d(doc, parent);
    if (d.exec() != QDialog::Accepted)
        return;
    FocusSession::Options o;
    o.calls = doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::ByStagePosition;
    // Only the head being worked at full: the rest of the call dim, so it
    // can be told apart on the stage.
    o.others = FocusSession::OthersDim;
    QString why;
    if (FocusModePanel::startWith(doc, parent, o, &why) == nullptr)
        QMessageBox::warning(parent, tr("Focus Calls"), why);
}

void FocusCallsDialog::setCalls(const QList<FocusCall> &calls)
{
    m_calls = calls;
    m_current = -1;
    fillCallList();
    m_list->setCurrentRow(m_calls.isEmpty() ? -1 : 0);
    loadCall(m_list->currentRow());
}

void FocusCallsDialog::fillCallList()
{
    const bool was = m_list->blockSignals(true);
    const int row = m_list->currentRow();
    m_list->clear();
    for (int i = 0; i < m_calls.size(); ++i)
        m_list->addItem(QStringLiteral("%1. %2").arg(i + 1).arg(FocusCalls::displayName(m_doc, m_calls.at(i))));
    m_list->setCurrentRow(qMin(row, m_list->count() - 1));
    m_list->blockSignals(was);
    m_start->setEnabled(!m_calls.isEmpty());
}

void FocusCallsDialog::loadCall(int index)
{
    m_current = index;
    const bool on = index >= 0 && index < m_calls.size();
    for (QWidget *w : { static_cast<QWidget *>(m_name), static_cast<QWidget *>(m_groups),
                        static_cast<QWidget *>(m_target), static_cast<QWidget *>(m_note) })
        w->setEnabled(on);
    m_loading = true;
    const FocusCall c = on ? m_calls.at(index) : FocusCall();
    m_name->setText(c.name);
    m_note->setText(c.note);
    for (int i = 0; i < m_groups->count(); ++i)
        m_groups->item(i)->setCheckState(c.groups.contains(m_groups->item(i)->data(Qt::UserRole).toUInt())
                                         ? Qt::Checked : Qt::Unchecked);
    const int t = c.target.kind == FocusSession::Reference::Target
                  ? m_target->findData(QVariant(c.target.id)) : 0;
    m_target->setCurrentIndex(qMax(0, t));
    m_loading = false;
}

void FocusCallsDialog::storeCall()
{
    if (m_loading || m_current < 0 || m_current >= m_calls.size())
        return;
    FocusCall &c = m_calls[m_current];
    c.name = m_name->text().trimmed();
    c.note = m_note->text().trimmed();
    c.groups.clear();
    for (int i = 0; i < m_groups->count(); ++i)
        if (m_groups->item(i)->checkState() == Qt::Checked)
            c.groups << m_groups->item(i)->data(Qt::UserRole).toUInt();
    const quint32 tid = m_target->currentData().toUInt();
    c.target = FocusSession::Reference();
    if (tid != 0xFFFFFFFFu)
    {
        c.target.kind = FocusSession::Reference::Target;
        c.target.id = tid;
    }
}

void FocusCallsDialog::addCall()
{
    storeCall();
    FocusCall c;
    // The next point along by default: most calls follow the one before.
    if (!m_calls.isEmpty())
        c.target = m_calls.last().target;
    m_calls << c;
    fillCallList();
    m_list->setCurrentRow(m_calls.size() - 1);
    loadCall(m_calls.size() - 1);
}

void FocusCallsDialog::removeCall()
{
    if (m_current < 0 || m_current >= m_calls.size())
        return;
    m_calls.removeAt(m_current);
    const int next = qMin(m_current, int(m_calls.size()) - 1);
    m_current = -1;
    fillCallList();
    m_list->setCurrentRow(next);
    loadCall(next);
}

void FocusCallsDialog::moveCall(int by)
{
    storeCall();
    const int to = m_current + by;
    if (m_current < 0 || to < 0 || to >= m_calls.size())
        return;
    m_calls.move(m_current, to);
    m_current = -1;
    fillCallList();
    m_list->setCurrentRow(to);
    loadCall(to);
}

QString FocusCallsDialog::venueChoice() const
{
    const int kind = m_venue->currentData().toInt();
    if (kind == kNoVenue)
        return QString();
    if (kind == kNewVenue)
    {
        const QString typed = m_newVenue->text().trimmed();
        return typed.isEmpty() ? m_newVenue->placeholderText() : typed;
    }
    return venueAt(m_venue->currentIndex());
}

void FocusCallsDialog::setVenueChoice(const QString &name)
{
    if (name.isEmpty())
    {
        m_venue->setCurrentIndex(m_venue->findData(kNoVenue));
        return;
    }
    const int at = m_doc->programmer()->venueCorrections()->venueNames().indexOf(name);
    if (at >= 0)
    {
        m_venue->setCurrentIndex(at + 1);
        return;
    }
    m_venue->setCurrentIndex(m_venue->findData(kNewVenue));
    m_newVenue->setText(name);
}

void FocusCallsDialog::fillVenues(const QString &select)
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    const bool was = m_venue->blockSignals(true);
    m_venue->clear();
    m_venue->addItem(tr("None: the show's own calibration"), kNoVenue);
    for (const QString &v : venues->venueNames())
        m_venue->addItem(venues->activeVenue() == v ? tr("%1 (applied)").arg(v) : v, 0);
    m_venue->addItem(tr("New venue\342\200\246"), kNewVenue);
    const QString want = select.isEmpty() ? venues->activeVenue() : select;
    int at = want.isEmpty() ? -1 : venues->venueNames().indexOf(want);
    m_venue->setCurrentIndex(at >= 0 ? at + 1 : m_venue->count() - 1);
    m_venue->blockSignals(was);
    refreshVenue();
}

QString FocusCallsDialog::venueAt(int index) const
{
    if (m_venue->itemData(index).toInt() != 0)
        return QString();
    // The combo's names may say "(applied)": the engine's list is the truth.
    const QStringList names = m_doc->programmer()->venueCorrections()->venueNames();
    return names.value(index - 1);
}

bool FocusCallsDialog::switchVenue(const QString &name, QString *why)
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    if (!name.isEmpty() && venues->filePath().isEmpty())
    {
        if (why != nullptr)
            *why = tr("Save the show first: a venue's corrections are kept in a file beside it.");
        return false;
    }
    if (m_doc->programmer()->focusSession()->isActive())
    {
        if (why != nullptr)
            *why = tr("Leave Focus mode first: its heads are being worked on.");
        return false;
    }
    const QString target = name.isEmpty() ? QString() : venues->addVenue(name);
    if (!venues->setActiveVenue(target))
        return false;
    fillVenues(target);
    return true;
}

bool FocusCallsDialog::renameVenue(const QString &from, const QString &to)
{
    const QString source = from;
    if (!m_doc->programmer()->venueCorrections()->renameVenue(source, to.trimmed()))
        return false;
    fillVenues(to.trimmed());
    return true;
}

bool FocusCallsDialog::deleteVenue(const QString &name)
{
    const QString source = name;
    if (m_doc->programmer()->focusSession()->isActive()
            || !m_doc->programmer()->venueCorrections()->removeVenue(source))
        return false;
    fillVenues(QString());
    return true;
}

void FocusCallsDialog::refreshVenue()
{
    const int kind = m_venue->currentData().toInt();
    m_newVenue->setVisible(kind == kNewVenue);
    m_renameVenue->setEnabled(kind == 0);
    m_deleteVenue->setEnabled(kind == 0);
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    const QString file = venues->filePath();
    if (kind == kNoVenue)
        m_venueNote->setText(tr("Saves go into the show's own calibration, as Focus mode does."));
    else if (file.isEmpty())
        m_venueNote->setText(tr("Save the show first: a venue's corrections are kept in a file beside it."));
    else
        m_venueNote->setText(tr("Corrections are saved for this venue in %1, next to the show, the "
                                "moment A is pressed. The show file is never changed by them.")
                             .arg(QFileInfo(file).fileName()));
}

bool FocusCallsDialog::apply(QString *why)
{
    storeCall();
    m_doc->programmer()->focusCalls()->setCalls(m_calls);
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    const QString venue = venueChoice();
    if (venue.isEmpty())
        return venues->setActiveVenue(QString());
    if (venues->filePath().isEmpty())
    {
        if (why != nullptr)
            *why = tr("Save the show first: a venue's corrections are kept in a file beside it.");
        return false;
    }
    return venues->setActiveVenue(venues->addVenue(venue));
}
