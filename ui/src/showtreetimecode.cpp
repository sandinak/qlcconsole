/*
  Q Light Controller Plus
  showtreetimecode.cpp

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

/* Timecode anchors in the Show Tree (Q198): a section's "Starts at" (and
   Learn), a cue's Anchor and what its time counts from, the list page's
   Section times / Import Logic markers… / Learn section starts, and the
   cue's Ripple timecode… The engine side is ShowTreeController's
   (engine/src/showtreetimecode.cpp) and MarkerPlan / SectionLearn
   (engine/src/timecodeanchors.*). No new menu for a show that doesn't
   follow timecode: Ripple timecode… is only on cues of a list that does. */

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialog>
#include <QLabel>
#include <QMenu>

#include "showtreeview.h"
#include "timecodeanchors.h"
#include "timecodesource.h"
#include "timecodecues.h"
#include "midimarkers.h"
#include "doc.h"

using namespace ShowTree;

/* A time typed in a field: hh:mm:ss:ff at the source's rate. */
static bool readTimecode(const QString &text, int fps, qint32 *ms)
{
    if (text.trimmed().isEmpty())
    {
        *ms = -1;
        return true;
    }
    return TimecodeCues::parse(text, fps, ms);
}

void ShowTreeView::buildTimecodeRows()
{
    // The cue page: Anchor, and what its time counts from.
    if (QHBoxLayout *tl = qobject_cast<QHBoxLayout *>(m_cueTcRow->layout()))
    {
        m_cueTcAnchor = new QCheckBox(tr("Anchor"), m_cueTcRow);
        m_cueTcAnchor->setObjectName("stCueTimecodeAnchor");
        m_cueTcAnchor->setToolTip(tr("The cues after it in its section count from this cue's time: move it and "
                                     "they move with it"));
        tl->addWidget(m_cueTcAnchor);
        connect(m_cueTcAnchor, &QCheckBox::toggled, this, [this](bool on) {
            if (!m_inspectorUpdating && m_pageCue != invalidId())
                m_st->setCueTimecodeAnchor(m_pageCue, on);
        });
    }
    m_cueTcBase = new QLabel(m_cueTc->parentWidget());
    m_cueTcBase->setObjectName("stCueTimecodeBase");
    m_cueTcBase->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // never widens the page
    m_cueTcBase->setToolTip(tr("Its time counts from its section's start (or the anchor before it): move "
                               "that and this cue follows"));
    if (QHBoxLayout *tl = qobject_cast<QHBoxLayout *>(m_cueTcRow->layout()))
        tl->insertWidget(1, m_cueTcBase);

    // A section's page: Starts at, Learn, Later sections follow.
    if (QFormLayout *f = qobject_cast<QFormLayout *>(m_sectionPage->layout()))
    {
        m_sectionStartRow = new QWidget(m_sectionPage);
        QVBoxLayout *v = new QVBoxLayout(m_sectionStartRow);
        v->setContentsMargins(0, 0, 0, 0);
        QHBoxLayout *h = new QHBoxLayout();
        v->addLayout(h);
        m_sectionStart = new QLineEdit(m_sectionStartRow);
        m_sectionStart->setObjectName("stSectionStart");
        m_sectionStart->setPlaceholderText(tr("hh:mm:ss:ff -- none"));
        m_sectionStart->setToolTip(tr("Where this section starts on the incoming timecode. Its cues' times count "
                                      "from it: change this one number and they all follow."));
        m_sectionStartLearn = new QPushButton(tr("Learn"), m_sectionStartRow);
        m_sectionStartLearn->setObjectName("stSectionStartLearn");
        m_sectionStartLearn->setToolTip(tr("Set the start to the timecode coming in now"));
        m_sectionFollow = new QCheckBox(tr("Later sections follow"), m_sectionStartRow);
        m_sectionFollow->setObjectName("stSectionFollow");
        m_sectionFollow->setToolTip(tr("Moving the start moves every later section, and every later cue, by as "
                                       "much (a section cut or made longer)"));
        h->addWidget(m_sectionStart, 1);
        h->addWidget(m_sectionStartLearn);
        v->addWidget(m_sectionFollow);
        f->insertRow(f->rowCount() - 1, tr("Starts at"), m_sectionStartRow);
        auto setStart = [this](qint32 ms) {
            Section s;
            if (currentKind() == SectionNode && currentSection(&s))
                m_st->setSectionStart(currentId(), s, ms, m_sectionFollow->isChecked());
        };
        connect(m_sectionStart, &QLineEdit::editingFinished, this, [this, setStart]() {
            if (m_inspectorUpdating)
                return;
            qint32 ms = -1;
            if (!readTimecode(m_sectionStart->text(), tcFps(), &ms))
            {
                m_sectionStart->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #c62828; }"));
                return;
            }
            m_sectionStart->setStyleSheet(QString());
            setStart(ms);
        });
        connect(m_sectionStartLearn, &QPushButton::clicked, this, [this, setStart]() {
            const TimecodeSource *src = m_doc->timecodeSource();
            if (src != nullptr && src->health() != TimecodeSource::NoSource)
                setStart(qint32(src->positionMs()));
        });
    }

    // The list page: Section times, Import Logic markers…, Learn section starts.
    if (m_listForm != nullptr)
    {
        m_listTcRow = new QWidget(m_listPage);
        QVBoxLayout *h = new QVBoxLayout(m_listTcRow);
        h->setContentsMargins(0, 0, 0, 0);
        m_listTcConvert = new QPushButton(tr("Section times…"), m_listTcRow);
        m_listTcConvert->setObjectName("stListTcConvert");
        m_listTcConvert->setToolTip(tr("Time its cues from their sections' starts: each section starts at its first "
                                       "cue, every cue stays where it is. Then moving a section moves its cues. "
                                       "One undo step."));
        m_listTcMarkers = new QPushButton(tr("Import Logic markers…"), m_listTcRow);
        m_listTcMarkers->setObjectName("stListTcMarkers");
        m_listTcMarkers->setToolTip(tr("The markers of a MIDI file exported from Logic set the starts of the "
                                       "sections they are named for; you see what moves first. One undo step."));
        m_listTcLearn = new QPushButton(m_listTcRow);
        m_listTcLearn->setObjectName("stListTcLearn");
        h->addWidget(m_listTcConvert);
        h->addWidget(m_listTcMarkers);
        h->addWidget(m_listTcLearn);
        for (QPushButton *b : { m_listTcConvert, m_listTcMarkers, m_listTcLearn })
            h->setAlignment(b, Qt::AlignLeft);
        m_listForm->addRow(tr("Timecode"), m_listTcRow);
        connect(m_listTcConvert, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ListNode)
                convertListToSectionTimes(currentId());
        });
        connect(m_listTcMarkers, &QPushButton::clicked, this, [this]() {
            if (currentKind() == ListNode)
                importLogicMarkers(currentId());
        });
        connect(m_listTcLearn, &QPushButton::clicked, this, [this]() {
            if (currentKind() != ListNode)
                return;
            if (m_learnSection < 0 || m_learnList != currentId())
            {
                startLearnSections(currentId());
                return;
            }
            const TimecodeSource *src = m_doc->timecodeSource();
            if (src == nullptr || src->health() == TimecodeSource::NoSource)
            {
                m_hint->setText(tr("No timecode is coming in: play the session with its timecode out."));
                return;
            }
            learnTap(qint32(src->positionMs()));
        });
        updateLearnButton();
    }
}

void ShowTreeView::updateListTimecodeRows(const CueList &list)
{
    if (m_listTcRow == nullptr)
        return;
    const bool sections = !m_st->sections(list.id).isEmpty();
    const bool show = list.followTimecode && !list.looks && !list.isSubCues() && sections;
    m_listForm->setRowVisible(m_listTcRow, show);
    if (!show)
        return;
    const int absolute = m_st->absoluteTimedCues(list.id);
    m_listTcConvert->setVisible(absolute > 0);
    m_listTcConvert->setText(tr("Section times (%n cue(s))…", "", absolute));
    updateLearnButton();
}

void ShowTreeView::updateCueTimecodeRows(const Cue &cue)
{
    if (m_cueTcAnchor == nullptr)
        return;
    {
        QSignalBlocker block(m_cueTcAnchor);
        m_cueTcAnchor->setChecked(cue.tcAnchor);
    }
    QString from;
    qint32 base = -1;
    if (cue.timecode >= 0 && m_st->cueTimecodeBase(cue.id, &from, &base))
        m_cueTcBase->setText(tr("%1 from %2").arg(MarkerPlan::formatDelta(qint64(cue.timecode) - base), from));
    else
        m_cueTcBase->clear();
    m_cueTcBase->setVisible(!m_cueTcBase->text().isEmpty());
}

void ShowTreeView::updateSectionTimecodeRows(quint32 listId, const Section &section)
{
    if (m_sectionStartRow == nullptr)
        return;
    const CueList *list = m_st->cueList(listId);
    if (QFormLayout *f = qobject_cast<QFormLayout *>(m_sectionPage->layout()))
        f->setRowVisible(m_sectionStartRow, list != nullptr && list->followTimecode);
    QSignalBlocker block(m_sectionStart);
    m_sectionStart->setStyleSheet(QString());
    if (!m_sectionStart->hasFocus())
        m_sectionStart->setText(section.start < 0 ? QString() : TimecodeCues::format(section.start, tcFps()));
    const TimecodeSource *src = m_doc->timecodeSource();
    m_sectionStartLearn->setEnabled(src != nullptr && src->health() != TimecodeSource::NoSource);
}

void ShowTreeView::timecodeMenu(QMenu *menu, quint32 cueId)
{
    const CueList *list = m_st->listOfCue(cueId);
    if (list == nullptr || !list->followTimecode || list->looks || list->isSubCues())
        return;
    QAction *a = menu->addAction(tr("Ripple Timecode…"), this, [this, cueId]() {
        const CueList *l = m_st->listOfCue(cueId);
        if (l == nullptr)
            return;
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Ripple timecode"));
        QFormLayout *f = new QFormLayout(&dialog);
        QComboBox *mode = new QComboBox(&dialog);
        mode->addItem(tr("Insert time at cue %1").arg(m_st->cueNumber(*l, l->indexOf(cueId))), RippleInsert);
        mode->addItem(tr("Remove time at cue %1").arg(m_st->cueNumber(*l, l->indexOf(cueId))), RippleRemove);
        Section s;
        if (m_st->sectionOfCue(cueId, &s))
            mode->addItem(tr("Stretch %1 to").arg(sectionTitle(s)), RippleStretch);
        QLineEdit *amount = new QLineEdit(&dialog);
        amount->setPlaceholderText(tr("seconds, or m:ss"));
        f->addRow(tr("Do"), mode);
        f->addRow(tr("Time"), amount);
        QLabel *note = new QLabel(tr("Everything after moves with it; one undo step. Typed: ripple +2.5 at 3.1, "
                                     "stretch Chorus 2 to 0:42"), &dialog);
        note->setWordWrap(true);
        f->addRow(note);
        QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        f->addRow(bb);
        if (dialog.exec() != QDialog::Accepted)
            return;
        // seconds ("2.5") or m:ss(.f)
        QString text = amount->text().trimmed();
        double total = 0;
        bool ok = !text.isEmpty();
        for (const QString &part : text.split(QLatin1Char(':')))
        {
            bool partOk = false;
            const double v = part.toDouble(&partOk);
            ok = ok && partOk && v >= 0;
            total = total * 60 + v;
        }
        if (!ok || total <= 0)
        {
            m_hint->setText(tr("Not a time: write seconds (2.5) or m:ss (0:42)."));
            return;
        }
        rippleCue(cueId, RippleMode(mode->currentData().toInt()), qint32(total * 1000.0 + 0.5));
    });
    a->setObjectName("stRippleTimecode");
}

bool ShowTreeView::rippleCue(quint32 cueId, RippleMode mode, qint32 ms)
{
    if (ms <= 0)
        return false;
    if (mode == RippleStretch)
    {
        Section s;
        const CueList *l = m_st->listOfCue(cueId);
        if (l == nullptr || !m_st->sectionOfCue(cueId, &s))
            return false;
        QString why;
        if (!m_st->stretchSection(l->id, s, ms, &why))
        {
            m_hint->setText(why);
            return false;
        }
        m_hint->setText(tr("%1 is %2 long now.").arg(sectionTitle(s), MarkerPlan::formatTime(ms)));
        return true;
    }
    const qint32 delta = mode == RippleInsert ? ms : -ms;
    if (!m_st->rippleTimecode(cueId, delta))
    {
        m_hint->setText(tr("Nothing changed: no cue from there on has a time."));
        return false;
    }
    m_hint->setText(mode == RippleInsert ? tr("%1 inserted.").arg(MarkerPlan::formatDelta(delta))
                                         : tr("%1 removed.").arg(MarkerPlan::formatDelta(delta)));
    return true;
}

bool ShowTreeView::convertListToSectionTimes(quint32 listId)
{
    const int n = m_st->absoluteTimedCues(listId);
    if (n == 0)
        return false;
    if (!confirm(tr("Section times"),
                 tr("Time %n cue(s) from their sections' starts?", "", n),
                 tr("Each section without a start starts at its first timed cue; every cue stays where it is. "
                    "Then moving a section's start moves its cues, and the Logic marker import and Learn "
                    "move whole sections. One undo step; a show saved like this still plays in an older "
                    "build.")))
        return false;
    int done = 0;
    if (!m_st->convertToSectionTimes(listId, &done))
        return false;
    m_hint->setText(tr("%n cue(s) now count from their sections.", "", done));
    return true;
}

bool ShowTreeView::importLogicMarkers(quint32 listId, const QString &path)
{
    if (m_st->cueList(listId) == nullptr)
        return false;
    QString file = path;
    if (file.isEmpty())
        file = QFileDialog::getOpenFileName(this, tr("Import Logic markers"), QString(),
                                            tr("MIDI files (*.mid *.midi);;All files (*)"));
    if (file.isEmpty())
        return false;
    const MidiMarkers::Result read = MidiMarkers::readFile(file);
    if (!read.ok)
    {
        m_hint->setText(read.error);
        return false;
    }
    if (read.markers.isEmpty())
    {
        m_hint->setText(tr("The file has no markers: export it from Logic with its markers."));
        return false;
    }
    const MarkerPlan plan = MarkerPlan::make(m_st, listId, read);
    if (!plan.changes())
    {
        m_hint->setText(plan.moves.isEmpty() ? tr("No marker is named for a section of this list.")
                                             : tr("Every section is where its marker is already."));
        if (!plan.lines().isEmpty())
            confirm(tr("Import Logic markers"), m_hint->text(), plan.lines().join(QLatin1Char('\n')));
        return false;
    }
    const QString text = tr("%n section(s) matched, %1 marker(s) not.", "", int(plan.moves.count()))
                             .arg(plan.unmatched.count())
                         + (plan.offsetMs > 0 ? QStringLiteral(" ") + tr("Markers count from %1.")
                                                .arg(TimecodeCues::format(qint32(plan.offsetMs), tcFps()))
                                              : QString());
    if (!confirm(tr("Import Logic markers"), text, plan.lines().join(QLatin1Char('\n'))))
        return false;
    if (!MarkerPlan::apply(m_st, listId, plan))
        return false;
    m_hint->setText(tr("Logic markers imported: %n section(s) set.", "", int(plan.moves.count())));
    return true;
}

void ShowTreeView::startLearnSections(quint32 listId, int fromSection)
{
    m_learnList = listId;
    m_learnSection = SectionLearn(m_st, listId, fromSection).section();
    m_learnCount = 0;
    m_learnLast = -1;
    if (m_learnSection >= 0)
        m_hint->setText(tr("Play the session with its timecode out, and tap at each section's start."));
    updateLearnButton();
}

bool ShowTreeView::learnTap(qint32 ms)
{
    if (m_learnSection < 0)
        return false;
    SectionLearn learn(m_st, m_learnList, m_learnSection, m_learnLast);
    if (!learn.tap(ms))
        return false;
    m_learnLast = ms;
    m_learnSection = learn.section();
    ++m_learnCount;
    if (m_learnSection < 0)
        m_hint->setText(tr("%n section start(s) learned.", "", m_learnCount));
    updateLearnButton();
    return true;
}

QString ShowTreeView::learnSection() const
{
    return m_learnSection < 0 ? QString() : SectionLearn(m_st, m_learnList, m_learnSection).sectionTitle();
}

void ShowTreeView::stopLearnSections()
{
    m_learnSection = -1;
    m_learnList = invalidId();
    updateLearnButton();
}

void ShowTreeView::updateLearnButton()
{
    if (m_listTcLearn == nullptr)
        return;
    if (m_learnSection >= 0)
    {
        QString title = learnSection();
        if (title.length() > 32)                // a long song name doesn't widen the page
            title = title.left(31) + QChar(0x2026);
        m_listTcLearn->setText(tr("Tap at: %1").arg(title));
        m_listTcLearn->setToolTip(tr("Tap as the section starts: it takes the timecode coming in, then the next "
                                     "section is up"));
    }
    else
    {
        m_listTcLearn->setText(tr("Learn section starts"));
        m_listTcLearn->setToolTip(tr("Play the new export with timecode out and tap at each section's start: "
                                     "each tap sets that section's start, and its cues follow"));
    }
}
