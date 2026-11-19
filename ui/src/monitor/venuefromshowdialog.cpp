/*
  Q Light Controller Plus
  venuefromshowdialog.cpp

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
#include <QStandardPaths>
#include <QFormLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QLabel>
#include <algorithm>

#include "programmercontroller.h"
#include "venuefromshowdialog.h"
#include "venuecorrections.h"
#include "roomoutputpicker.h"
#include "inputoutputmap.h"
#include "venuerigdialog.h"
#include "focussession.h"
#include "fixturegroup.h"
#include "doc.h"

namespace
{
VenueCorrections *venuesOf(Doc *doc)
{
    return doc->programmer()->venueCorrections();
}

QString kindText(VenueImport::MatchKind kind)
{
    switch (kind)
    {
        case VenueImport::SameName:    return VenueFromShowDialog::tr("same name");
        case VenueImport::SimilarName: return VenueFromShowDialog::tr("similar name");
        case VenueImport::NoMatch:     break;
    }
    return QString();
}
}

/*****************************************************************************
 * The group table
 *****************************************************************************/

GroupMatchTable::GroupMatchTable(Doc *doc, QWidget *parent)
    : QTableWidget(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("vfsGroups"));
    setColumnCount(4);
    setHorizontalHeaderLabels({ tr("Group there"), tr("Heads"), tr("Fills this show's group"), tr("Confirmed") });
    horizontalHeaderItem(GroupCol)->setToolTip(
        tr("The group of this show the room's lights fill while the venue is applied: cues on it play on "
           "them. Not matched: held in the venue under its own name until you match it."));
    horizontalHeaderItem(ConfirmCol)->setToolTip(tr("Tick each suggested match you want: none is used until "
                                                    "it is confirmed"));
    verticalHeader()->hide();
    horizontalHeader()->setSectionResizeMode(NameCol, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(GroupCol, QHeaderView::Stretch);
    setSelectionMode(QAbstractItemView::NoSelection);
    connect(this, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *it) {
        if (!m_loading && it->column() == ConfirmCol)
            emit matchesChanged();
    });
}

void GroupMatchTable::setRows(const QStringList &names, const QList<int> &heads,
                              const QList<VenueImport::Match> &matches)
{
    m_loading = true;
    clearContents();
    setRowCount(int(names.size()));
    m_kinds.clear();
    QList<FixtureGroup *> groups = m_doc->fixtureGroups();
    std::sort(groups.begin(), groups.end(), [](const FixtureGroup *a, const FixtureGroup *b) {
        return a->name().localeAwareCompare(b->name()) < 0;
    });
    for (int row = 0; row < names.size(); ++row)
    {
        const VenueImport::Match m = matches.value(row);
        m_kinds << m.kind;
        QTableWidgetItem *name = new QTableWidgetItem(names.at(row));
        name->setFlags(Qt::ItemIsEnabled);
        setItem(row, NameCol, name);
        QTableWidgetItem *count = new QTableWidgetItem(QString::number(heads.value(row)));
        count->setFlags(Qt::ItemIsEnabled);
        count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setItem(row, HeadsCol, count);

        QComboBox *box = new QComboBox(this);
        box->setObjectName(QStringLiteral("vfsGroup%1").arg(row));
        box->addItem(tr("Not matched (kept for later)"), uint(UINT_MAX));
        for (const FixtureGroup *g : groups)
        {
            const bool suggested = g->id() == m.group && m.kind != VenueImport::NoMatch;
            box->addItem(suggested ? tr("%1 (%2)").arg(g->name(), kindText(m.kind)) : g->name(), g->id());
        }
        box->setCurrentIndex(qMax(0, box->findData(uint(m.group))));
        setCellWidget(row, GroupCol, box);

        QTableWidgetItem *tick = new QTableWidgetItem;
        tick->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        tick->setCheckState(m.confirmed ? Qt::Checked : Qt::Unchecked);
        setItem(row, ConfirmCol, tick);

        // A group chosen by hand is his choice: confirmed.
        connect(box, &QComboBox::currentIndexChanged, this, [this, row, box]() {
            if (m_loading)
                return;
            const bool chosen = box->currentData().toUInt() != UINT_MAX;
            m_loading = true;
            item(row, ConfirmCol)->setCheckState(chosen ? Qt::Checked : Qt::Unchecked);
            m_loading = false;
            emit matchesChanged();
        });
    }
    resizeColumnToContents(HeadsCol);
    resizeColumnToContents(ConfirmCol);
    m_loading = false;
    emit matchesChanged();
}

QList<VenueImport::Match> GroupMatchTable::matches() const
{
    QList<VenueImport::Match> out;
    for (int row = 0; row < rowCount(); ++row)
    {
        VenueImport::Match m;
        const QComboBox *box = groupBox(row);
        m.group = box != nullptr ? box->currentData().toUInt() : UINT_MAX;
        m.kind = m_kinds.value(row);
        m.confirmed = m.group != UINT_MAX && item(row, ConfirmCol) != nullptr
                      && item(row, ConfirmCol)->checkState() == Qt::Checked;
        out << m;
    }
    return out;
}

QComboBox *GroupMatchTable::groupBox(int row) const
{
    return qobject_cast<QComboBox *>(cellWidget(row, GroupCol));
}

void GroupMatchTable::setConfirmed(int row, bool on)
{
    if (QTableWidgetItem *it = item(row, ConfirmCol))
        it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
}

/*****************************************************************************
 * The dialog
 *****************************************************************************/

VenueFromShowDialog::VenueFromShowDialog(Doc *doc, const QString &name, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setObjectName(QStringLiteral("VenueFromShowDialog"));
    setWindowTitle(tr("New Venue from a Show"));
    setProperty("helpTopic", QStringLiteral("venues.md"));
    QVBoxLayout *vl = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr("Take a room's house rig from another show file: its fixtures come in as "
                                  "House, at their places on its plot, on the universe this show keeps for "
                                  "the room, into this venue only. This show's file is not changed."), this);
    intro->setWordWrap(true);
    vl->addWidget(intro);

    QFormLayout *form = new QFormLayout;
    m_name = new QLineEdit(name, this);
    m_name->setObjectName(QStringLiteral("vfsName"));
    m_name->setPlaceholderText(tr("The room's name, e.g. Lincoln HS Auditorium"));
    form->addRow(tr("Name"), m_name);

    QHBoxLayout *fl = new QHBoxLayout;
    m_file = new QLineEdit(this);
    m_file->setObjectName(QStringLiteral("vfsFile"));
    m_file->setReadOnly(true);
    m_file->setPlaceholderText(tr("The room's show file (.qlcc), e.g. one made from its desk's export"));
    m_browse = new QPushButton(tr("Browse\342\200\246"), this);
    m_browse->setObjectName(QStringLiteral("vfsBrowse"));
    fl->addWidget(m_file, 1);
    fl->addWidget(m_browse);
    form->addRow(tr("Show file"), fl);

    m_universe = new QComboBox(this);
    m_universe->setObjectName(QStringLiteral("vfsUniverse"));
    m_universe->setToolTip(tr("The one universe this show keeps for the room's rig. Each fixture keeps its "
                              "address there."));
    form->addRow(tr("Its fixtures go on"), m_universe);

    m_output = new RoomOutputPicker(doc, this);
    form->addRow(tr("Output in this room"), m_output);
    vl->addLayout(form);

    QGroupBox *groups = new QGroupBox(tr("Groups"), this);
    QVBoxLayout *gl = new QVBoxLayout(groups);
    m_groups = new GroupMatchTable(doc, groups);
    gl->addWidget(m_groups);
    vl->addWidget(groups, 1);

    QGroupBox *what = new QGroupBox(tr("What comes in"), this);
    QVBoxLayout *wl = new QVBoxLayout(what);
    m_summary = new QLabel(what);
    m_summary->setObjectName(QStringLiteral("vfsSummary"));
    m_summary->setWordWrap(true);
    wl->addWidget(m_summary);
    m_warnings = new QLabel(what);
    m_warnings->setObjectName(QStringLiteral("vfsWarnings"));
    m_warnings->setWordWrap(true);
    wl->addWidget(m_warnings);
    m_clashes = new QListWidget(what);
    m_clashes->setObjectName(QStringLiteral("vfsClashes"));
    m_clashes->setMaximumHeight(110);
    wl->addWidget(m_clashes);
    vl->addWidget(what);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_make = bb->addButton(tr("Make Venue"), QDialogButtonBox::AcceptRole);
    m_make->setObjectName(QStringLiteral("vfsMake"));
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_make, &QPushButton::clicked, this, [this]() {
        if (!makeVenue().isEmpty())
            accept();
    });
    vl->addWidget(bb);

    connect(m_browse, &QPushButton::clicked, this, [this]() {
        const QString start = m_file->text().isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QFileInfo(m_file->text()).path();
        QFileDialog *fd = new QFileDialog(this, tr("The room's show file"), start,
                                          tr("Shows (*.qlcc *.qxw);;All files (*)"));
        fd->setObjectName(QStringLiteral("vfsFileDialog"));
        fd->setAttribute(Qt::WA_DeleteOnClose);
        fd->setFileMode(QFileDialog::ExistingFile);
        connect(fd, &QFileDialog::fileSelected, this, [this](const QString &path) { setFile(path); });
        fd->open();
    });
    connect(m_name, &QLineEdit::textChanged, this, &VenueFromShowDialog::refresh);
    connect(m_universe, &QComboBox::currentIndexChanged, this, &VenueFromShowDialog::refresh);
    connect(m_output, &RoomOutputPicker::changed, this, &VenueFromShowDialog::refresh);
    connect(m_groups, &GroupMatchTable::matchesChanged, this, &VenueFromShowDialog::refresh);

    // The universes, the suggested one first chosen.
    InputOutputMap *iom = doc->inputOutputMap();
    const QSet<quint32> house = venuesOf(doc)->rig()->venueFixtures();
    QHash<quint32, int> onIt;
    for (const Fixture *fx : doc->fixtures())
        if (!house.contains(fx->id()))
            onIt[fx->universe()]++;
    for (quint32 u = 0; u < iom->universesCount(); ++u)
    {
        const int n = onIt.value(u);
        const QString what = n == 0 ? tr("nothing on it") : n == 1 ? tr("1 fixture of the show")
                                                                   : tr("%1 fixtures of the show").arg(n);
        m_universe->addItem(tr("U%1 %2 (%3)").arg(u + 1).arg(iom->getUniverseNameByIndex(int(u)), what), u);
    }
    const quint32 suggested = VenueImport::suggestUniverse(doc, house);
    m_universe->setCurrentIndex(qMax(0, m_universe->findData(suggested)));

    resize(720, 640);
    refresh();
}

VenueFromShowDialog *VenueFromShowDialog::open(Doc *doc, QWidget *parent, const QString &name)
{
    VenueFromShowDialog *d = new VenueFromShowDialog(doc, name, parent);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->show();
    return d;
}

bool VenueFromShowDialog::setFile(const QString &path)
{
    QString why;
    VenueImport::Source src;
    m_read = VenueImport::read(path, src, &why);
    m_file->setText(path);
    if (!m_read)
    {
        m_source = VenueImport::Source();
        m_groups->setRows(QStringList(), QList<int>(), QList<VenueImport::Match>());
        m_summary->setText(why);
        refresh();
        return false;
    }
    m_source = src;
    QStringList names;
    QList<int> heads;
    for (const VenueImport::Group &g : m_source.groups)
    {
        names << g.name;
        heads << int(g.heads.size());
    }
    m_groups->setRows(names, heads, VenueImport::suggest(m_source, m_doc));
    m_output->setImported(m_source.fixtureOutputs(), m_source.fileName());
    if (m_name->text().trimmed().isEmpty())
        m_name->setText(QFileInfo(path).completeBaseName());
    refresh();
    return true;
}

VenueImport::Options VenueFromShowDialog::options() const
{
    VenueImport::Options opt;
    opt.universe = m_universe->currentData().toUInt();
    opt.matches = m_groups->matches();
    opt.output = m_output->output();
    return opt;
}

void VenueFromShowDialog::refresh()
{
    VenueCorrections *vc = venuesOf(m_doc);
    const QString name = m_name->text().trimmed();
    QString blocked;
    m_clashes->clear();
    m_warnings->clear();
    if (!m_read)
    {
        if (m_file->text().isEmpty())
            m_summary->setText(tr("Choose the room's show file."));
        blocked = tr("Choose the room's show file first");
    }
    else
    {
        const VenueImport::Options opt = options();
        const VenueImport::Result res = VenueImport::build(m_source, m_doc, opt, vc->reservedIds());
        m_summary->setText(tr("From %1: %2. Groups: %3 fill this show's, %4 kept for later.")
                               .arg(m_source.fileName(), VenueImport::summary(res, opt.universe))
                               .arg(res.filled).arg(res.held));
        m_warnings->setText(res.warnings.join(QLatin1Char('\n')));
        m_warnings->setVisible(!res.warnings.isEmpty());
        // The clash check, before anything is applied.
        for (const VenueRig::Clash &c : vc->rig()->clashes(res.data, false))
            new QListWidgetItem(QStringLiteral("%1 \342\200\224 %2").arg(VenueRig::kindName(c.kind), c.text), m_clashes);
        if (m_clashes->count() == 0)
            new QListWidgetItem(tr("No clashes."), m_clashes);
        const int waiting = VenueImport::unconfirmed(opt.matches);
        if (name.isEmpty())
            blocked = tr("Name the room");
        else if (vc->hasVenue(name))
            blocked = tr("\342\200\234%1\342\200\235 is already a venue").arg(name);
        else if (waiting > 0)
            blocked = waiting == 1 ? tr("Confirm each match: 1 to go")
                                   : tr("Confirm each match: %1 to go").arg(waiting);
        else if (!m_output->isComplete())
            blocked = tr("Finish the room's output, or choose Not yet");
    }
    m_make->setEnabled(blocked.isEmpty());
    m_make->setToolTip(blocked.isEmpty() ? tr("Make %1 and apply it").arg(name) : blocked);
    m_make->setText(blocked.isEmpty() || !m_read ? tr("Make Venue") : tr("Make Venue (%1)").arg(blocked));
}

QString VenueFromShowDialog::makeVenue()
{
    VenueCorrections *vc = venuesOf(m_doc);
    if (!m_make->isEnabled())
        return QString();
    const VenueImport::Options opt = options();
    const VenueImport::Result res = VenueImport::build(m_source, m_doc, opt, vc->reservedIds());
    QString why;
    const QString made = vc->addVenueFromShow(m_name->text(), res.data, &why);
    if (made.isEmpty())
    {
        QMessageBox::information(this, windowTitle(), why);
        return QString();
    }
    // Its clashes were on screen before Make Venue: applied now.
    const bool applied = !m_doc->programmer()->focusSession()->isActive() && vc->setActiveVenue(made);

    QWidget *over = parentWidget();
    QPointer<QMessageBox> note = new QMessageBox(QMessageBox::Information, tr("New Venue from a Show"),
        tr("%1 made from %2: %3. %4 of its groups fill this show's; %5 wait in the venue for a match "
           "(Rig and Universes).").arg(made, m_source.fileName(), VenueImport::summary(res, opt.universe))
            .arg(res.filled).arg(res.held)
        + (applied ? QString() : QStringLiteral("\n") + tr("Not applied: leave Quick focus or Venue focus "
                                                             "first, then switch to it.")),
        QMessageBox::Ok, over);
    note->setObjectName(QStringLiteral("vfsDone"));
    note->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton *undo = note->addButton(tr("Undo"), QMessageBox::DestructiveRole);
    undo->setObjectName(QStringLiteral("vfsUndo"));
    undo->setToolTip(tr("Take %1 off the rig and remove it. The show was not changed.").arg(made));
    Doc *doc = m_doc;
    connect(undo, &QPushButton::clicked, doc, [doc, made]() { undoImport(doc, made); });
    note->open();
    return made;
}

bool VenueFromShowDialog::undoImport(Doc *doc, const QString &venue)
{
    VenueCorrections *vc = venuesOf(doc);
    if (!vc->hasVenue(venue))
        return false;
    return vc->removeVenue(venue);
}
