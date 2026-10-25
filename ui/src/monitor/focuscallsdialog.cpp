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
#include <QCheckBox>
#include <QPointer>
#include <QMessageBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QTextDocument>
#include <QFileDialog>
#include <QPdfWriter>
#include <QPageSize>
#include <QSaveFile>
#include <QSettings>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QDir>
#include <QLabel>

#include "programmercontroller.h"
#include "monitorproperties.h"
#include "venuecorrections.h"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QTimer>

#include "focuscallsdialog.h"
#include "venuerigdialog.h"
#include "focusreport.h"
#include "operatorvoice.h"
#include "focuscallcue.h"
#include "focusmodepanel.h"
#include "fixturegroup.h"
#include "stagetarget.h"
#include "fixture.h"
#include "truss.h"
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
    // A tour (Q140): a new room starts from the last one's corrections.
    m_fromLast = new QCheckBox(this);
    m_fromLast->setObjectName(QStringLiteral("fcFromLast"));
    m_fromLast->setChecked(true);
    m_fromLast->setToolTip(tr("Copy the last venue's corrections, trims and targets into the new one, "
                              "rather than starting from the show's own"));
    venueRow->addWidget(m_fromLast);
    // The venue manager: apply now, rename, delete (the engine's own).
    m_applyVenue = new QPushButton(tr("Apply now"), this);
    m_applyVenue->setObjectName(QStringLiteral("fcApplyVenue"));
    m_applyVenue->setToolTip(tr("Put this venue's corrections on the rig now (or, for None, the show's own)"));
    m_renameVenue = new QPushButton(tr("Rename\342\200\246"), this);
    m_renameVenue->setObjectName(QStringLiteral("fcRenameVenue"));
    m_deleteVenue = new QPushButton(tr("Delete"), this);
    m_deleteVenue->setObjectName(QStringLiteral("fcDeleteVenue"));
    m_deleteVenue->setToolTip(tr("Forget this venue and its corrections (the show is not changed)"));
    m_corrections = new QPushButton(tr("Corrections\342\200\246"), this);
    m_corrections->setObjectName(QStringLiteral("fcCorrections"));
    m_corrections->setToolTip(tr("See and clear the applied venue's corrections, one light at a time"));
    for (QPushButton *b : { m_applyVenue, m_renameVenue, m_deleteVenue, m_corrections })
        venueRow->addWidget(b);
    /* What the venue owns (board Q137/Q138, VENUES.md): its house rig, the
       patch of every universe in this room, what is not ours. */
    QPushButton *venueRig = new QPushButton(tr("Rig and universes\342\200\246"), this);
    venueRig->setObjectName(QStringLiteral("fcVenueRig"));
    venueRig->setToolTip(tr("This venue's house rig, its outputs for each universe, the channels it keeps, "
                            "and the clash check"));
    venueRow->addWidget(venueRig);
    connect(venueRig, &QPushButton::clicked, this, [this]() {
        VenueRigDialog::open(m_doc, this, venueAt(m_venue->currentIndex()));
    });
    vl->addLayout(venueRow);
    // Who is focusing, for the report (Q134), and the report itself.
    QHBoxLayout *reportRow = new QHBoxLayout;
    reportRow->addWidget(new QLabel(tr("Focused by:"), this));
    m_checkedBy = new QLineEdit(this);
    m_checkedBy->setObjectName(QStringLiteral("fcCheckedBy"));
    m_checkedBy->setPlaceholderText(tr("Your name, for the focus report"));
    m_checkedBy->setText(m_doc->programmer()->venueCorrections()->checkedBy());
    connect(m_checkedBy, &QLineEdit::textEdited, this, [this](const QString &t) {
        m_doc->programmer()->venueCorrections()->setCheckedBy(t);
        QSettings().setValue(QStringLiteral("focusCalls/checkedBy"), t.trimmed());
    });
    reportRow->addWidget(m_checkedBy, 1);
    m_report = new QPushButton(tr("Export focus report\342\200\246"), this);
    m_report->setObjectName(QStringLiteral("fcReport"));
    m_report->setToolTip(tr("This venue's focus, light by light: a PDF or web page to print, "
                            "or a CSV for Lightwright"));
    connect(m_report, &QPushButton::clicked, this, [this]() {
        exportReport(m_doc, this, venueAt(m_venue->currentIndex()));
    });
    reportRow->addWidget(m_report);
    // Teaching: a scrambled copy of the room to focus back (never a real venue).
    m_practice = new QPushButton(tr("Practice (scramble)\342\200\246"), this);
    m_practice->setObjectName(QStringLiteral("fcPractice"));
    m_practice->setToolTip(tr("Put every moving head in the calls off by a few degrees, in a practice "
                              "venue, and focus them back: scored on how close, how fast and how "
                              "many saves. The real venues are not changed."));
    connect(m_practice, &QPushButton::clicked, this, [this]() {
        const QStringList levels = { tr("Easy: 1\342\200\2233\302\260 off"),
                                     tr("Medium: 2\342\200\2236\302\260 off"),
                                     tr("Hard: 4\342\200\22310\302\260 off") };
        bool ok = false;
        const QString level = QInputDialog::getItem(this, tr("Practice"),
            tr("Each moving head in the calls is put off its focus by a random amount. Focus them "
               "back to their targets and save. How far off?"), levels, 1, false, &ok);
        if (!ok)
            return;
        const int at = int(levels.indexOf(level));
        const float lo = at == 0 ? 1.0f : at == 1 ? 2.0f : 4.0f;
        const float hi = at == 0 ? 3.0f : at == 1 ? 6.0f : 10.0f;
        QString why;
        if (!startPractice(lo, hi, quint32(QDateTime::currentMSecsSinceEpoch()), &why))
        {
            QMessageBox::information(this, windowTitle(), why);
            return;
        }
        accept();
    });
    reportRow->addWidget(m_practice);
    vl->addLayout(reportRow);
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
        // Its clashes with the show first, before it goes live (VENUES.md section 6).
        if (!VenueRigDialog::confirmClashes(m_doc, this, venueChoice()))
            return;
        QString why;
        if (!switchVenue(venueChoice(), &why))
            QMessageBox::information(this, windowTitle(), why);
    });
    connect(m_corrections, &QPushButton::clicked, this, [this]() { showCorrections(m_doc, this); });
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
    const bool practice = d.isPractice();
    FocusSession::Options o;
    o.calls = doc->programmer()->focusCalls()->sessionCalls();
    o.order = FocusSession::ByStagePosition;
    // Only the head being worked at full: the rest of the call dim, so it
    // can be told apart on the stage.
    o.others = FocusSession::OthersDim;
    QString why;
    if (FocusModePanel::startWith(doc, parent, o, &why) == nullptr)
    {
        if (practice)
            doc->programmer()->venueCorrections()->endPractice();
        QMessageBox::warning(parent, tr("Focus Calls"), why);
        return;
    }
    // A practice: when the calls end, the score, and the real venue back.
    if (practice)
    {
        FocusSession *s = doc->programmer()->focusSession();
        QPointer<QWidget> owner(parent);
        QObject::connect(s, &FocusSession::stopped, doc, [doc, owner]() {
            showPracticeResult(doc, owner.data());
        }, Qt::SingleShotConnection);
    }
}

bool FocusCallsDialog::startPractice(float minDeg, float maxDeg, quint32 seed, QString *why)
{
    storeCall();
    m_doc->programmer()->focusCalls()->setCalls(m_calls);
    QList<quint32> fids;
    for (const FocusCall &c : m_calls)
        for (quint32 fid : FocusCalls::fixturesOf(m_doc, c))
            if (!fids.contains(fid))
                fids << fid;
    fids = FocusSession::movingHeads(m_doc, fids);
    if (m_doc->programmer()->focusSession()->isActive())
    {
        if (why != nullptr)
            *why = tr("Leave Focus mode first: its heads are being worked on.");
        return false;
    }
    const QString name = m_doc->programmer()->venueCorrections()->startPractice(fids, seed, minDeg, maxDeg, why);
    if (name.isEmpty())
        return false;
    m_practiceStarted = true;
    fillVenues(name);
    return true;
}

QDialog *FocusCallsDialog::showPracticeResult(Doc *doc, QWidget *parent)
{
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    if (!venues->isPracticeActive())
        return nullptr;
    const QString practice = venues->activeVenue();
    const VenueCorrections::Score sc = venues->practiceScore(practice);
    venues->endPractice();
    QDialog *d = new QDialog(parent);
    d->setObjectName(QStringLiteral("practiceResult"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Practice"));
    d->setProperty("helpTopic", QStringLiteral("focus-calls.md#practice-scramble-and-focus-back"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    const int secs = int(sc.seconds);
    QLabel *text = new QLabel(tr("<b>%1 of %2 lights within 1\302\260.</b><br>Average %3\302\260 off, worst "
                                 "%4\302\260. %5 lights saved, %6 saves, %7.<br>%8 is back on the rig; "
                                 "the practice is kept as a venue for the report.")
                              .arg(sc.withinOne).arg(sc.lights)
                              .arg(sc.meanError, 0, 'f', 1).arg(sc.worstError, 0, 'f', 1)
                              .arg(sc.checked).arg(sc.saves)
                              .arg(secs >= 60 ? tr("%1 min %2 s").arg(secs / 60).arg(secs % 60)
                                              : tr("%1 s").arg(secs))
                              .arg(venues->isActive() ? venues->activeVenue().toHtmlEscaped()
                                                      : tr("The show's own calibration")), d);
    text->setObjectName(QStringLiteral("practiceResultText"));
    text->setWordWrap(true);
    vl->addWidget(text);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, d);
    QPushButton *report = bb->addButton(tr("Export the report\342\200\246"), QDialogButtonBox::ActionRole);
    report->setObjectName(QStringLiteral("practiceResultReport"));
    QObject::connect(report, &QPushButton::clicked, d, [doc, d, practice]() {
        exportReport(doc, d, practice);
    });
    QObject::connect(bb, &QDialogButtonBox::rejected, d, &QDialog::close);
    vl->addWidget(bb);
    d->show();
    d->raise();
    return d;
}

QDialog *FocusCallsDialog::askVenue(Doc *doc, QWidget *parent, const QString &fallback, int timeoutMs,
                                    const QString &current)
{
    FocusCallCue *fcc = doc->programmer()->focusCallCue();
    QDialog *d = new QDialog(parent);
    d->setObjectName(QStringLiteral("focusCallsVenueAsk"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Venue"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    // Board Q129: always asked, the one applied too -- the room may be new.
    QLabel *question = new QLabel(current.isEmpty()
        ? tr("Which venue is this? Corrections are saved for it, beside the show.")
        : tr("Still at %1? If this is another room, name it (or wait): its corrections "
             "start from the show's own.").arg(current), d);
    question->setObjectName(QStringLiteral("fcAskVenueQuestion"));
    question->setWordWrap(true);
    vl->addWidget(question);
    QLineEdit *name = new QLineEdit(d);
    name->setObjectName(QStringLiteral("fcAskVenueName"));
    name->setPlaceholderText(fallback);
    vl->addWidget(name);
    QLabel *left = new QLabel(d);
    left->setObjectName(QStringLiteral("fcAskVenueLeft"));
    vl->addWidget(left);
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok, d);
    bb->button(QDialogButtonBox::Ok)->setText(tr("New venue"));
    QPushButton *date = bb->addButton(tr("New venue, today's date"), QDialogButtonBox::RejectRole);
    date->setObjectName(QStringLiteral("fcAskVenueDate"));
    if (!current.isEmpty())
    {
        QPushButton *still = bb->addButton(tr("Still at %1").arg(current), QDialogButtonBox::ActionRole);
        still->setObjectName(QStringLiteral("fcAskVenueStill"));
        QObject::connect(still, &QPushButton::clicked, d, [fcc, current, d]() {
            fcc->useVenue(current);
            d->close();
        });
    }
    vl->addWidget(bb);

    // The countdown: once someone types, it waits for them.
    QTimer *tick = new QTimer(d);
    tick->setInterval(250);
    QElapsedTimer *clock = new QElapsedTimer();
    clock->start();
    auto update = [=]() {
        const int secs = qMax(0, int((timeoutMs - clock->elapsed() + 999) / 1000));
        left->setText(name->text().isEmpty()
                      ? tr("A new venue in %1 s: %2").arg(secs).arg(fallback)
                      : tr("Press New venue to use this name."));
    };
    QObject::connect(tick, &QTimer::timeout, d, update);
    QObject::connect(d, &QObject::destroyed, [clock]() { delete clock; });
    QObject::connect(name, &QLineEdit::textEdited, d, [fcc, name]() {
        // A name being typed: the date is not put in under it.
        if (!name->text().isEmpty())
            fcc->holdVenueAsk();
    });
    QObject::connect(bb, &QDialogButtonBox::accepted, d, [fcc, name, d]() {
        fcc->useVenue(name->text());
        d->close();
    });
    QObject::connect(date, &QPushButton::clicked, d, [fcc, d]() {
        fcc->useVenue(QString());
        d->close();
    });
    QObject::connect(fcc, &FocusCallCue::stateChanged, d, [d](FocusCallCue::State s) {
        if (s != FocusCallCue::NamingVenue)
            d->close();
    });
    update();
    tick->start();
    d->show();
    d->raise();
    return d;
}

QDialog *FocusCallsDialog::confirmVenue(Doc *doc, QWidget *parent)
{
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    if (!venues->needsConfirm())
        return nullptr;
    const QString current = venues->activeVenue();
    QDialog *d = new QDialog(parent);
    d->setObjectName(QStringLiteral("venueConfirm"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Venue"));
    d->setProperty("helpTopic", QStringLiteral("focus-calls.md"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    const QDateTime when = venues->lastChanged(current);
    QLabel *question = new QLabel(tr("<b>Still at %1?</b><br>This show's focus corrections for %1 were "
                                     "last changed %2. In another room, start a new venue: its "
                                     "corrections start from the show's own, and %1's are kept.")
                                  .arg(current.toHtmlEscaped(),
                                       when.isValid() ? QLocale().toString(when, QLocale::ShortFormat)
                                                      : tr("some time ago")), d);
    question->setObjectName(QStringLiteral("venueConfirmQuestion"));
    question->setWordWrap(true);
    vl->addWidget(question);
    QLineEdit *name = new QLineEdit(d);
    name->setObjectName(QStringLiteral("venueConfirmName"));
    name->setPlaceholderText(VenueCorrections::defaultVenueName());
    vl->addWidget(name);
    QDialogButtonBox *bb = new QDialogButtonBox(d);
    QPushButton *fresh = bb->addButton(tr("New venue"), QDialogButtonBox::AcceptRole);
    fresh->setObjectName(QStringLiteral("venueConfirmNew"));
    fresh->setDefault(true);
    QPushButton *still = bb->addButton(tr("Still at %1").arg(current), QDialogButtonBox::ActionRole);
    still->setObjectName(QStringLiteral("venueConfirmStill"));
    QPushButton *none = bb->addButton(tr("None: the show's own"), QDialogButtonBox::RejectRole);
    none->setObjectName(QStringLiteral("venueConfirmNone"));
    vl->addWidget(bb);
    QObject::connect(fresh, &QPushButton::clicked, d, [venues, name, d]() {
        venues->setActiveVenue(venues->addVenue(name->text()));
        d->close();
    });
    QObject::connect(still, &QPushButton::clicked, d, [venues, d]() {
        venues->confirmVenue();
        d->close();
    });
    QObject::connect(none, &QPushButton::clicked, d, [venues, d]() {
        venues->setActiveVenue(QString());
        d->close();
    });
    d->show();
    d->raise();
    return d;
}

QDialog *FocusCallsDialog::offerVenueImport(Doc *doc, QWidget *parent, const QString &showFile)
{
    const QStringList candidates = VenueCorrections::importCandidates(showFile);
    if (candidates.isEmpty())
        return nullptr;
    const QString from = candidates.first();
    QDialog *d = new QDialog(parent);
    d->setObjectName(QStringLiteral("venueImport"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Venues"));
    d->setProperty("helpTopic", QStringLiteral("focus-calls.md"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    QLabel *text = new QLabel(tr("This show has no venue file of its own, but %1 beside it holds the "
                                 "focus corrections of an earlier copy of it. Use its venues for this "
                                 "show? (It is copied; that file is not changed.)")
                              .arg(QFileInfo(from).fileName()), d);
    text->setObjectName(QStringLiteral("venueImportText"));
    text->setWordWrap(true);
    vl->addWidget(text);
    QDialogButtonBox *bb = new QDialogButtonBox(d);
    QPushButton *use = bb->addButton(tr("Use its venues"), QDialogButtonBox::AcceptRole);
    use->setObjectName(QStringLiteral("venueImportUse"));
    use->setDefault(true);
    QPushButton *no = bb->addButton(tr("Not now"), QDialogButtonBox::RejectRole);
    no->setObjectName(QStringLiteral("venueImportNo"));
    vl->addWidget(bb);
    QObject::connect(use, &QPushButton::clicked, d, [doc, parent, from, d]() {
        doc->programmer()->venueCorrections()->importFrom(from);
        d->close();
        // Then, as on any open: an old venue is confirmed.
        FocusCallsDialog::confirmVenue(doc, parent);
    });
    QObject::connect(no, &QPushButton::clicked, d, &QDialog::close);
    d->show();
    d->raise();
    return d;
}

QDialog *FocusCallsDialog::showCorrections(Doc *doc, QWidget *parent)
{
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    if (!venues->isActive())
        return nullptr;
    QDialog *d = new QDialog(parent);
    d->setObjectName(QStringLiteral("venueCorrections"));
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(tr("Corrections: %1").arg(venues->activeVenue()));
    d->setProperty("helpTopic", QStringLiteral("focus-calls.md"));
    QVBoxLayout *vl = new QVBoxLayout(d);
    QListWidget *list = new QListWidget(d);
    list->setObjectName(QStringLiteral("venueCorrectionList"));
    vl->addWidget(list, 1);
    enum Kind { LightKind = 1, TrimKind, TargetKind, DeadKind };
    auto fill = [doc, venues, list]() {
        list->clear();
        const VenueCorrections::Venue v = venues->venue(venues->activeVenue());
        MonitorProperties *props = doc->monitorProperties();
        for (auto it = v.fixtures.constBegin(); it != v.fixtures.constEnd(); ++it)
        {
            const Fixture *fx = doc->fixture(it.key());
            QString text = tr("%1: pan %2\302\260, tilt %3\302\260")
                           .arg(fx != nullptr ? fx->name() : venues->correctedAs(it.key()))
                           .arg(double(it.value().panOffsetDeg), 0, 'f', 2)
                           .arg(double(it.value().tiltOffsetDeg), 0, 'f', 2);
            if (fx == nullptr)
                text += tr("  (no longer in the show: kept)");
            else if (venues->needsFocus(it.key()))
                text += tr("  ! needs focus: not the fixture focused here");
            if (venues->isDead(it.key()))
                text += tr("  \342\234\225 dead");
            QListWidgetItem *item = new QListWidgetItem(text, list);
            item->setData(Qt::UserRole, int(LightKind));
            item->setData(Qt::UserRole + 1, it.key());
        }
        // Lights marked dead here (Q140) with no correction of their own.
        for (quint32 fid : venues->deadLights())
        {
            if (v.fixtures.contains(fid))
                continue;
            const Fixture *fx = doc->fixture(fid);
            QListWidgetItem *item = new QListWidgetItem(tr("%1: \342\234\225 dead (Clear: not dead)")
                                                        .arg(fx != nullptr ? fx->name() : QString::number(fid)),
                                                        list);
            item->setData(Qt::UserRole, int(DeadKind));
            item->setData(Qt::UserRole + 1, fid);
        }
        for (auto it = v.trims.constBegin(); it != v.trims.constEnd(); ++it)
        {
            const Truss *t = props->truss(it.key());
            // In the show's unit, as the stage marks say it.
            QListWidgetItem *item = new QListWidgetItem(tr("Trim %1: %2").arg(t != nullptr ? t->name() : QString::number(it.key()))
                                                        .arg(MonitorProperties::formatStageDistance(it.value(), props->gridUnits())),
                                                        list);
            item->setData(Qt::UserRole, int(TrimKind));
            item->setData(Qt::UserRole + 1, it.key());
        }
        for (auto it = v.targets.constBegin(); it != v.targets.constEnd(); ++it)
        {
            const StageTarget *t = props->stageTarget(it.key());
            // Where it stands from the stage marks ("SL 2.4 · US 3.1 m").
            QListWidgetItem *item = new QListWidgetItem(tr("Target %1: %2")
                                                        .arg(t != nullptr ? t->name() : QString::number(it.key()))
                                                        .arg(props->stagePositionText(it.value())), list);
            item->setData(Qt::UserRole, int(TargetKind));
            item->setData(Qt::UserRole + 1, it.key());
        }
        if (list->count() == 0)
            new QListWidgetItem(tr("No corrections yet: the rig is as the show has it."), list);
    };
    fill();
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close, d);
    QPushButton *clear = bb->addButton(tr("Clear"), QDialogButtonBox::ActionRole);
    clear->setObjectName(QStringLiteral("venueCorrectionClear"));
    clear->setToolTip(tr("Back to the show's own for the one picked"));
    vl->addWidget(bb);
    QObject::connect(clear, &QPushButton::clicked, d, [venues, list, fill]() {
        QListWidgetItem *item = list->currentItem();
        if (item == nullptr)
            return;
        const quint32 id = item->data(Qt::UserRole + 1).toUInt();
        switch (item->data(Qt::UserRole).toInt())
        {
            case LightKind:  venues->clearCorrection(id); break;
            case TrimKind:   venues->clearTrim(id); break;
            case TargetKind: venues->clearTarget(id); break;
            case DeadKind:   venues->setDead(id, false); break;
            default:         return;
        }
        fill();
    });
    QObject::connect(bb, &QDialogButtonBox::rejected, d, &QDialog::close);
    d->resize(520, 360);
    d->show();
    d->raise();
    return d;
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
    if (m_practice != nullptr)
        m_practice->setEnabled(!m_calls.isEmpty());
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
    /* An applied venue nobody has said is still the room (last changed
       over a day ago, Q129) is not offered as the default: New venue is. */
    const QString want = !select.isEmpty() ? select
                       : venues->needsConfirm() ? QString() : venues->activeVenue();
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
    const QString target = name.isEmpty() ? QString() : makeVenue(name);
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
    {
        const QString last = m_doc->programmer()->venueCorrections()->lastVenue();
        m_fromLast->setText(tr("Start from %1").arg(last));
        m_fromLast->setVisible(kind == kNewVenue && !last.isEmpty());
    }
    m_renameVenue->setEnabled(kind == 0);
    m_deleteVenue->setEnabled(kind == 0);
    m_corrections->setEnabled(m_doc->programmer()->venueCorrections()->isActive());
    m_report->setEnabled(kind == 0);
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    const QString file = venues->filePath();
    if (kind == kNoVenue)
        m_venueNote->setText(tr("Saves go into the show's own calibration, as Focus mode does."));
    else if (file.isEmpty())
        m_venueNote->setText(tr("Save the show first: a venue's corrections are kept in a file beside it."));
    else if (kind == kNewVenue && venues->needsConfirm())
        m_venueNote->setText(tr("%1 is applied, but was last changed %2: pick it above if this is "
                                "still that room.")
                             .arg(venues->activeVenue(),
                                  QLocale().toString(venues->lastChanged(venues->activeVenue()),
                                                     QLocale::ShortFormat)));
    else
    {
        QString text = tr("Corrections are saved for this venue in %1, next to the show, the "
                          "moment A is pressed. The show file is never changed by them.")
                       .arg(QFileInfo(file).fileName());
        // Q133: what the venue applied could not use.
        const int skipped = venues->needsFocus().size();
        const int orphans = venues->orphaned().size();
        if (kind == 0 && venueAt(m_venue->currentIndex()) == venues->activeVenue())
        {
            if (skipped > 0)
                text += QLatin1Char(' ') + tr("%n light(s) need focus: the fixture is not the one focused here.",
                                              "", skipped);
            if (orphans > 0)
                text += QLatin1Char(' ') + tr("%n correction(s) for fixtures no longer in the show are kept.",
                                              "", orphans);
        }
        m_venueNote->setText(text);
    }
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
    return venues->setActiveVenue(makeVenue(venue));
}

QString FocusCallsDialog::makeVenue(const QString &name)
{
    VenueCorrections *venues = m_doc->programmer()->venueCorrections();
    const QString last = venues->lastVenue();
    if (!venues->hasVenue(name.trimmed()) && !m_fromLast->isHidden() && m_fromLast->isChecked()
        && !last.isEmpty())
        return venues->addVenueFrom(name, last);
    return venues->addVenue(name);
}

bool FocusCallsDialog::writeReport(Doc *doc, const QString &venue, const QString &path, QString *why)
{
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    if (!venues->hasVenue(venue))
    {
        if (why != nullptr)
            *why = tr("There is no venue called \342\200\234%1\342\200\235.").arg(venue);
        return false;
    }
    const QString show = QFileInfo(venues->showFile()).completeBaseName();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("pdf"))
    {
        QPdfWriter pdf(path);
        pdf.setTitle(tr("Focus report: %1").arg(venue));
        pdf.setCreator(QStringLiteral("qlcconsole"));
        // Landscape: ten columns read across the page.
        pdf.setPageLayout(QPageLayout(QPageSize(QPageSize::Letter), QPageLayout::Landscape,
                                      QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter));
        QTextDocument page;
        page.setHtml(FocusReport::html(doc, venue, show));
        page.print(&pdf);
        if (!QFileInfo::exists(path))
        {
            if (why != nullptr)
                *why = tr("Cannot write %1.").arg(QDir::toNativeSeparators(path));
            return false;
        }
        return true;
    }
    const QString text = suffix == QLatin1String("csv") ? FocusReport::csv(doc, venue)
                                                        : FocusReport::html(doc, venue, show);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit())
    {
        if (why != nullptr)
            *why = tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    return true;
}

void FocusCallsDialog::exportReport(Doc *doc, QWidget *parent, const QString &venue)
{
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    if (!venues->hasVenue(venue))
        return;
    const QFileInfo show(venues->showFile());
    const QString dir = show.exists() ? show.absolutePath() : QDir::homePath();
    QString name = tr("%1 focus report").arg(venue);
    name.replace(QRegularExpression(QStringLiteral("[/:\\\\]")), QStringLiteral("-"));
    QString filter;
    const QString pdf = tr("PDF, to print (*.pdf)");
    const QString html = tr("Web page (*.html)");
    const QString csv = tr("CSV for Lightwright (*.csv)");
    QString path = QFileDialog::getSaveFileName(parent, tr("Export focus report"),
                                                QDir(dir).filePath(name + QStringLiteral(".pdf")),
                                                QStringList({ pdf, html, csv }).join(QStringLiteral(";;")),
                                                &filter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += filter == csv ? QStringLiteral(".csv") : filter == html ? QStringLiteral(".html")
                                                                         : QStringLiteral(".pdf");
    QString why;
    if (!writeReport(doc, venue, path, &why))
        QMessageBox::warning(parent, tr("Export focus report"), why);
}
