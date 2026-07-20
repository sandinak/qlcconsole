/*
  Q Light Controller Plus
  showtimelineeditor.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QColorDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QIcon>
#include <QBrush>
#include <QColor>

#include "showtimelineeditor.h"
#include "functionselection.h"
#include "multitrackview.h"
#include "sceneitem.h"
#include "showitem.h"
#include "showfunction.h"
#include "track.h"
#include "show.h"

#include "collection.h"
#include "rgbmatrix.h"
#include "sequence.h"
#include "chaser.h"
#include "audio.h"
#include "video.h"
#include "scene.h"
#include "efx.h"
#include "mastertimer.h"
#include "doc.h"

ShowTimelineEditor::ShowTimelineEditor(QWidget *parent, Show *show, Doc *doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_show(show)
    , m_showview(NULL)
    , m_currentTrack(NULL)
    , m_playAction(NULL)
    , m_stopAction(NULL)
    , m_deleteAction(NULL)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);

    // Compact transport + editing toolbar. (MTC source, undo, footer chips live
    // in the full Show Manager tab; this is the additive-build + preview core.)
    QToolBar *tb = new QToolBar(this);
    tb->setIconSize(QSize(20, 20));
    m_playAction = tb->addAction(QIcon(":/player_play.png"), tr("Play / Pause"));
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(slotPlayPause()));
    m_stopAction = tb->addAction(QIcon(":/player_stop.png"), tr("Stop"));
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(slotStop()));
    tb->addSeparator();
    QAction *addTrack = tb->addAction(QIcon(":/edit_add.png"), tr("Add Track"));
    connect(addTrack, SIGNAL(triggered()), this, SLOT(slotNewTrackRequested()));
    m_deleteAction = tb->addAction(QIcon(":/editdelete.png"), tr("Delete selected clip(s)"));
    connect(m_deleteAction, SIGNAL(triggered()), this, SLOT(slotDeleteSelected()));
    lay->addWidget(tb);

    m_showview = new MultiTrackView();
    m_showview->setRenderHint(QPainter::Antialiasing);
    m_showview->setAcceptDrops(true);
    m_showview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_showview->setBackgroundBrush(QBrush(QColor(88, 88, 88, 255), Qt::SolidPattern));
    // Size to the actual track count so unused rows don't render / scroll.
    m_showview->setCompact(true);
    lay->addWidget(m_showview, 1);

    // Building
    connect(m_showview, SIGNAL(functionDropped(quint32,quint32,Track*)),
            this, SLOT(slotFunctionDropped(quint32,quint32,Track*)));
    connect(m_showview, SIGNAL(addAtRequested(quint32,Track*)),
            this, SLOT(slotAddAtRequested(quint32,Track*)));
    connect(m_showview, SIGNAL(newTrackRequested()),
            this, SLOT(slotNewTrackRequested()));
    connect(m_showview, SIGNAL(showItemMoved(ShowItem*,quint32,bool)),
            this, SLOT(slotShowItemMoved(ShowItem*,quint32,bool)));
    connect(m_showview, &MultiTrackView::trackModified,
            m_doc, &Doc::setModified);
    // Tracks
    connect(m_showview, SIGNAL(trackDelete(Track*)),
            this, SLOT(slotTrackDelete(Track*)));
    connect(m_showview, SIGNAL(trackDoubleClicked(Track*)),
            this, SLOT(slotTrackDoubleClicked(Track*)));
    connect(m_showview, SIGNAL(trackMoved(Track*,int)),
            this, SLOT(slotTrackMoved(Track*,int)));
    connect(m_showview, SIGNAL(trackColorChangeRequested(Track*)),
            this, SLOT(slotTrackColorChangeRequested(Track*)));
    connect(m_showview, &MultiTrackView::trackIntensityChanged,
            this, [this](Track *t, qreal v) {
                if (m_show != NULL && t != NULL)
                    m_show->setTrackIntensity(t->id(), v);
            });
    // Markers
    connect(m_showview, SIGNAL(markerAddRequested(quint32)),
            this, SLOT(slotMarkerAddRequested(quint32)));
    connect(m_showview, SIGNAL(markerEditRequested(quint32)),
            this, SLOT(slotMarkerEditRequested(quint32)));
    connect(m_showview, SIGNAL(markerColorRequested(quint32)),
            this, SLOT(slotMarkerColorRequested(quint32)));
    connect(m_showview, SIGNAL(markerDeleteRequested(quint32)),
            this, SLOT(slotMarkerDeleteRequested(quint32)));
    connect(m_showview, SIGNAL(markerRelabelRequested(quint32,QString)),
            this, SLOT(slotMarkerRelabel(quint32,QString)));
    connect(m_showview, SIGNAL(markerMovedRequested(quint32,quint32,quint32,QString,QColor)),
            this, SLOT(slotMarkerMoved(quint32,quint32,quint32,QString,QColor)));
    connect(m_showview, SIGNAL(markerSetCueListRequested(quint32)),
            this, SLOT(slotMarkerSetCueList(quint32)));

    // Delete key removes the selected clip(s).
    QShortcut *del = new QShortcut(QKeySequence::Delete, this);
    connect(del, SIGNAL(activated()), this, SLOT(slotDeleteSelected()));

    reload();
    updateTransportState();
}

ShowTimelineEditor::~ShowTimelineEditor()
{
    // Leaving this editor stops a preview it started (the full Show Manager tab
    // is the run surface); don't leave a show playing behind an unseen tab.
    if (m_show != NULL && m_show->isRunning())
        m_show->stop(functionParent());
}

FunctionParent ShowTimelineEditor::functionParent() const
{
    return FunctionParent::master();
}

void ShowTimelineEditor::reload()
{
    m_showview->resetView();
    if (m_show == NULL)
        return;

    m_showview->setHeaderType(m_show->timeDivisionType());
    m_showview->setBPMValue(m_show->timeDivisionBPM());
    m_showview->setMarkers(m_show->markers());

    // Move the playhead with the show while it is previewed live, and keep the
    // transport button in sync with its running state.
    connect(m_show, SIGNAL(timeChanged(quint32)),
            this, SLOT(slotShowTimeChanged(quint32)), Qt::UniqueConnection);
    connect(m_show, SIGNAL(running(quint32)),
            this, SLOT(slotShowRunningChanged()), Qt::UniqueConnection);
    connect(m_show, SIGNAL(stopped(quint32)),
            this, SLOT(slotShowRunningChanged()), Qt::UniqueConnection);

    Track *firstTrack = NULL;
    foreach (Track *track, m_show->tracks())
    {
        if (firstTrack == NULL)
            firstTrack = track;
        m_showview->addTrack(track);

        foreach (ShowFunction *sf, track->showFunctions())
        {
            Function *fn = m_doc->function(sf->functionID());
            if (fn == NULL)
                continue;

            switch (fn->type())
            {
                case Function::SceneType:
                    m_showview->addScene(qobject_cast<Scene*>(fn), track, sf);
                break;
                case Function::ChaserType:
                case Function::SequenceType:
                    m_showview->addSequence(qobject_cast<Chaser*>(fn), track, sf);
                break;
                case Function::AudioType:
                    m_showview->addAudio(qobject_cast<Audio*>(fn), track, sf);
                break;
                case Function::RGBMatrixType:
                    m_showview->addRGBMatrix(qobject_cast<RGBMatrix*>(fn), track, sf);
                break;
                case Function::EFXType:
                    m_showview->addEFX(qobject_cast<EFX*>(fn), track, sf);
                break;
                case Function::VideoType:
                    m_showview->addVideo(qobject_cast<Video*>(fn), track, sf);
                break;
                case Function::CollectionType:
                    m_showview->addCollection(qobject_cast<Collection*>(fn), track, sf);
                break;
                default:
                break;
            }
        }
    }

    if (firstTrack != NULL)
    {
        m_currentTrack = firstTrack;
        m_showview->activateTrack(m_currentTrack);
        m_showview->setEmptyMessage(QString());
    }
    else
    {
        m_currentTrack = NULL;
        m_showview->setEmptyMessage(tr("Empty show — drag a scene, chaser, "
            "collection or effect here to build it, or use Add Track. "
            "Right-click the time ruler to add a section marker."));
    }

    m_showview->updateViewSize();
}

ShowFunction *ShowTimelineEditor::addFunctionToTrack(Function *f, Track *track,
                                                     quint32 startTime)
{
    if (f == NULL || track == NULL)
        return NULL;

    const Function::Type t = f->type();

    // A bare Scene is a simple timed clip (SceneItem) — no hidden Sequence wrap.
    if (t == Function::SceneType)
    {
        Scene *scene = qobject_cast<Scene*>(f);
        ShowFunction *sf = track->createShowFunction(scene->id());
        sf->setStartTime(startTime);
        sf->setDuration(SceneItem::defaultDuration());
        m_showview->addScene(scene, track, sf);
        return sf;
    }

    ShowFunction *sf = track->createShowFunction(f->id());
    sf->setStartTime(startTime);

    switch (t)
    {
        case Function::ChaserType:
        case Function::SequenceType:
            m_showview->addSequence(qobject_cast<Chaser*>(f), track, sf);
        break;
        case Function::AudioType:
            m_showview->addAudio(qobject_cast<Audio*>(f), track, sf);
        break;
        case Function::RGBMatrixType:
            m_showview->addRGBMatrix(qobject_cast<RGBMatrix*>(f), track, sf);
        break;
        case Function::EFXType:
            m_showview->addEFX(qobject_cast<EFX*>(f), track, sf);
        break;
        case Function::VideoType:
            m_showview->addVideo(qobject_cast<Video*>(f), track, sf);
        break;
        case Function::CollectionType:
        {
            Collection *c = qobject_cast<Collection*>(f);
            if (sf->duration() == 0 && c != NULL && c->totalDuration() == 0)
                sf->setDuration(10000);
            m_showview->addCollection(c, track, sf);
        }
        break;
        default:
            track->removeShowFunction(sf); // unsupported on a timeline
            return NULL;
        break;
    }
    return sf;
}

void ShowTimelineEditor::slotFunctionDropped(quint32 funcID, quint32 startTime,
                                             Track *track)
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    if (track != NULL && track->isLocked())
        return;

    Function *f = m_doc->function(funcID);
    if (f == NULL)
        return;

    // Never drop the show into itself (directly or transitively).
    if (f->id() == m_show->id() || f->contains(m_show->id()))
    {
        QMessageBox::warning(this, tr("Can't add"),
            tr("A show can't contain itself."));
        return;
    }

    if (track == NULL)
    {
        track = new Track();
        track->setName(tr("Track %1").arg(m_show->tracks().count() + 1));
        m_show->addTrack(track);
        m_showview->addTrack(track);
    }

    m_currentTrack = track;
    m_showview->activateTrack(track);
    ShowFunction *sf = addFunctionToTrack(f, track, startTime);
    if (sf == NULL)
    {
        QMessageBox::information(this, tr("Not supported"),
            tr("\"%1\" (%2) can't be placed on a show timeline.")
            .arg(f->name()).arg(Function::typeToString(f->type())));
        return;
    }
    m_showview->resolveCollisions(track, sf); // DAW-style push

    m_showview->setEmptyMessage(QString());
    m_doc->setModified();
    m_showview->updateViewSize();
}

void ShowTimelineEditor::slotAddAtRequested(quint32 startTime, Track *track)
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    FunctionSelection fs(this, m_doc);
    QList<quint32> disabled;
    foreach (Function *function, m_doc->functions())
        if (function->contains(m_show->id()))
            disabled << function->id();
    fs.setDisabledFunctions(disabled);
    fs.setMultiSelection(false);
    fs.setFilter(Function::SceneType | Function::ChaserType | Function::SequenceType |
                 Function::AudioType | Function::RGBMatrixType | Function::EFXType |
                 Function::CollectionType | Function::VideoType);
    fs.disableFilters(Function::ShowType | Function::ScriptType);
    if (fs.exec() != QDialog::Accepted || fs.selection().isEmpty())
        return;

    slotFunctionDropped(fs.selection().first(), startTime, track);
}

void ShowTimelineEditor::slotNewTrackRequested()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    Track *track = new Track();
    track->setName(tr("Track %1").arg(m_show->tracks().count() + 1));
    m_show->addTrack(track);
    m_showview->addTrack(track);
    m_currentTrack = track;
    m_showview->activateTrack(track);
    m_showview->setEmptyMessage(QString());
    m_doc->setModified();
    m_showview->updateViewSize();
}

void ShowTimelineEditor::slotDeleteSelected()
{
    if (m_show == NULL || m_doc->isShowLocked())
        return;

    QList<ShowItem *> items = m_showview->selectedItems();
    if (items.isEmpty())
        return;

    if (items.count() > 1)
    {
        QString msg = tr("Delete %1 clips from the timeline?").arg(items.count());
        msg += QString("\n\n") + tr("(The functions themselves are kept.)");
        if (QMessageBox::question(this, tr("Delete clips"), msg,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    }

    foreach (ShowItem *it, items)
    {
        ShowFunction *sf = it->showFunction();
        if (sf == NULL)
            continue;
        foreach (Track *track, m_show->tracks())
        {
            if (track->showFunctions().contains(sf))
            {
                track->removeShowFunction(sf);
                break;
            }
        }
    }
    m_doc->setModified();
    reload();   // rebuild cleanly (keeps view/model in sync)
}

void ShowTimelineEditor::slotShowItemMoved(ShowItem *item, quint32 time, bool moved)
{
    Q_UNUSED(item)
    Q_UNUSED(time)
    // The ShowItem persists its own new start time into the ShowFunction; just
    // flag the document dirty.
    if (moved)
        m_doc->setModified();
}

/* --------------------------------------------------------------------- Tracks */

void ShowTimelineEditor::slotTrackDelete(Track *track)
{
    if (track == NULL || m_show == NULL || m_doc->isShowLocked())
        return;

    QList<ShowFunction *> sfs = track->showFunctions();
    QString msg = tr("Delete track \"%1\"?").arg(track->name());
    if (sfs.isEmpty() == false)
    {
        msg += QString("\n\n") + tr("This will remove %1 item(s) from the timeline:")
                                 .arg(sfs.count()) + QString("\n");
        foreach (ShowFunction *sf, sfs)
        {
            Function *f = m_doc->function(sf->functionID());
            if (f != NULL)
                msg += QString("\n • ") + f->name();
        }
        msg += QString("\n\n") + tr("(The functions themselves are kept.)");
    }

    if (QMessageBox::question(this, tr("Delete Track"), msg,
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    if (m_currentTrack == track)
        m_currentTrack = NULL;
    m_show->removeTrack(track->id());
    m_doc->setModified();
    reload();
}

void ShowTimelineEditor::slotTrackDoubleClicked(Track *track)
{
    if (track == NULL || m_show == NULL)
        return;
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Track name"),
                       tr("Track name:"), QLineEdit::Normal, track->name(), &ok);
    if (ok == false || name.isEmpty())
        return;
    const int idx = m_show->getAttributeIndex(track->name());
    track->setName(name);
    m_show->renameAttribute(idx, name);
    m_doc->setModified();
}

void ShowTimelineEditor::slotTrackMoved(Track *track, int direction)
{
    if (m_show == NULL || direction == 0)
        return;
    const int step = direction > 0 ? 1 : -1;
    for (int i = 0; i < qAbs(direction); i++)
        m_show->moveTrack(track, step);
    m_doc->setModified();
    reload();
}

void ShowTimelineEditor::slotTrackColorChangeRequested(Track *track)
{
    if (track == NULL)
        return;
    QColor c = QColorDialog::getColor(track->color().isValid() ? track->color()
                                      : QColor(76, 98, 115), this, tr("Track Colour"));
    if (c.isValid() == false)
        return;
    track->setColor(c);
    m_doc->setModified();
}

/* -------------------------------------------------------------------- Markers */

void ShowTimelineEditor::slotMarkerAddRequested(quint32 time)
{
    if (m_show == NULL)
        return;
    bool ok = false;
    QString label = QInputDialog::getText(this, tr("Add Section Marker"),
                        tr("Label for this section of the show:"),
                        QLineEdit::Normal, QString(), &ok);
    if (ok == false || label.trimmed().isEmpty())
        return;

    quint32 end = time + 15000;
    QMapIterator<quint32, ShowMarker> it(m_show->markers());
    while (it.hasNext())
    {
        it.next();
        if (it.key() > time && it.key() < end)
            end = it.key();
    }

    static const char *palette[] = { "#e0a820", "#4a90d9", "#5aa469", "#c0504d",
                                     "#8e6fb0", "#3fa8a0", "#d07030" };
    int idx = m_show->markers().count() % 7;
    m_show->setMarker(time, end, label.trimmed(), QColor(palette[idx]));
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerEditRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    bool ok = false;
    QString label = QInputDialog::getText(this, tr("Rename Marker"),
                        tr("Marker label:"), QLineEdit::Normal, m.label, &ok);
    if (ok == false)
        return;
    m_show->setMarker(time, m.end, label.trimmed(), m.color);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerRelabel(quint32 time, QString label)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    m_show->setMarker(time, m.end, label, m.color);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerColorRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    ShowMarker m = m_show->markers().value(time);
    QColor c = QColorDialog::getColor(m.color.isValid() ? m.color : QColor("#e0a820"),
                                      this, tr("Marker Colour"));
    if (c.isValid() == false)
        return;
    m_show->setMarker(time, m.end, m.label, c);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerDeleteRequested(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;
    m_show->removeMarker(time);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerMoved(quint32 oldStart, quint32 newStart,
                                         quint32 newEnd, QString label, QColor color)
{
    if (m_show == NULL || oldStart == UINT_MAX || label.isEmpty())
        return;
    // Carry the manual-cue-list link across the move (new start = new key).
    const quint32 cue = m_show->markerCueList(oldStart);
    m_show->removeMarker(oldStart);
    m_show->setMarker(newStart, newEnd, label, color);
    if (cue != Function::invalidId())
        m_show->setMarkerCueList(newStart, cue);
    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

void ShowTimelineEditor::slotMarkerSetCueList(quint32 time)
{
    if (m_show == NULL || time == UINT_MAX)
        return;

    // Pick a Chaser (the manual GO cue list) that covers this off-clock section.
    FunctionSelection fs(this, m_doc);
    fs.setMultiSelection(false);
    fs.setFilter(Function::ChaserType);
    fs.disableFilters(Function::SceneType | Function::CollectionType | Function::EFXType |
                      Function::RGBMatrixType | Function::AudioType | Function::VideoType |
                      Function::ShowType | Function::ScriptType | Function::SequenceType);
    // Offer an explicit "unlink" via cancel-with-clear: use a 3rd button.
    const int res = fs.exec();
    if (res == QDialog::Accepted && fs.selection().isEmpty() == false)
        m_show->setMarkerCueList(time, fs.selection().first());
    else if (res == QDialog::Rejected)
        return; // leave the link as-is on cancel

    m_showview->setMarkers(m_show->markers());
    m_doc->setModified();
}

/* ------------------------------------------------------------------ Transport */

void ShowTimelineEditor::slotPlayPause()
{
    if (m_show == NULL)
        return;

    if (m_show->isRunning() == false)
    {
        m_show->start(m_doc->masterTimer(), functionParent(),
                      m_showview->getTimeFromCursor());
    }
    else if (m_show->isPaused())
    {
        m_show->setPause(false);
    }
    else
    {
        m_show->setPause(true);
    }
    updateTransportState();
}

void ShowTimelineEditor::slotStop()
{
    if (m_show != NULL && m_show->isRunning())
        m_show->stop(functionParent());
    m_showview->rewindCursor();
    updateTransportState();
}

void ShowTimelineEditor::slotShowTimeChanged(quint32 ms)
{
    if (m_showview != NULL)
        m_showview->setPlayheadTarget(ms);
}

void ShowTimelineEditor::slotShowRunningChanged()
{
    updateTransportState();
}

void ShowTimelineEditor::updateTransportState()
{
    if (m_playAction == NULL || m_show == NULL)
        return;
    const bool running = m_show->isRunning() && m_show->isPaused() == false;
    m_playAction->setIcon(QIcon(running ? ":/player_pause.png" : ":/player_play.png"));
}
