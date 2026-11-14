/*
  Q Light Controller Plus
  cueresume.cpp

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

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QSaveFile>
#include <QLocale>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QDir>

#include <algorithm>

#include "showtreecontroller.h"
#include "showcontrol.h"
#include "cueresume.h"
#include "qlcconfig.h"
#include "chaser.h"
#include "doc.h"

namespace
{
const int kVersion = 1;

QString samePath(const QString &path)
{
    return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString joined(const QStringList &parts)
{
    if (parts.count() <= 1)
        return parts.join(QString());
    return QCoreApplication::translate("CueResume", "%1 and %2")
        .arg(QStringList(parts.mid(0, parts.count() - 1)).join(QStringLiteral(", ")), parts.last());
}
} // namespace

CueResume::CueResume(Doc *doc, const QString &path, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_path(path.isEmpty() ? defaultPath() : path)
    , m_writeTimer(new QTimer(this))
{
    /* After the command has acted, in the same turn of the event loop: one
       write for a GO that took several lists, none per frame. */
    m_writeTimer->setSingleShot(true);
    m_writeTimer->setInterval(0);
    connect(m_writeTimer, &QTimer::timeout, this, &CueResume::write);

    if (m_doc != nullptr)
    {
        registry().insert(m_doc, this);
        ShowControl *sc = m_doc->showControl();
        connect(sc, &ShowControl::cueTaken, this, &CueResume::slotCueTaken);
        connect(sc, &ShowControl::cueListStopped, this, &CueResume::slotListStopped);
        connect(m_doc, &Doc::modeChanged, this, [this]() {
            if (m_armed)
                emit offerChanged();
        });
    }
}

CueResume::~CueResume()
{
    if (registry().value(m_doc) == this)
        registry().remove(m_doc);
    // A command still to be written is written: a quit right after a GO.
    if (m_writeTimer->isActive())
        write();
}

QHash<const Doc *, CueResume *> &CueResume::registry()
{
    static QHash<const Doc *, CueResume *> map;
    return map;
}

CueResume *CueResume::of(const Doc *doc)
{
    return registry().value(doc, nullptr);
}

QString CueResume::defaultPath()
{
#if defined(WIN32) || defined(Q_OS_WIN)
    const QString home = QString::fromLocal8Bit(qgetenv("USERPROFILE"));
#else
    const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
#endif
    return QStringLiteral("%1/%2/resume-cue.json").arg(home, QStringLiteral(USERQLCPLUSDIR));
}

/*****************************************************************************
 * What the App tells it
 *****************************************************************************/

void CueResume::setShowFile(const QString &path)
{
    const QString was = m_showFile;
    m_showFile = samePath(path);
    /* Saved under a new name: this session's record follows it. */
    if (!was.isEmpty() && !m_showFile.isEmpty() && was != m_showFile && !m_current.isEmpty())
        scheduleWrite();
}

void CueResume::showOpened(const QString &path)
{
    setShowFile(path);
    /* Read now: a clean close of an earlier show removed what this session
       wrote, so what is there is what a session that ended without one
       left behind. */
    setArmed(false);
    if (m_current.isEmpty() && !m_writeTimer->isActive())
        readFile();
    if (m_pending.isEmpty() || m_showFile.isEmpty() || m_pendingShow != m_showFile)
        return;
    /* The show the last session left without a clean close: its record is
       this session's now, removed on a clean close or a dismiss. */
    m_owns = true;
    setArmed(true);
}

void CueResume::showClosed()
{
    m_writeTimer->stop();
    setArmed(false);
    m_current.clear();
    if (m_owns)
    {
        removeFile();
        m_pending.clear();
        m_pendingShow.clear();
    }
    m_owns = false;
}

/*****************************************************************************
 * The offer
 *****************************************************************************/

bool CueResume::offerShown() const
{
    return m_armed && m_doc != nullptr && m_doc->mode() == Doc::Operate;
}

bool CueResume::resolve(const Entry &e, Chaser **chaserOut, int *stepOut, int *subOut) const
{
    if (m_doc == nullptr)
        return false;
    ShowTreeController *st = m_doc->showTree();
    Chaser *chaser = nullptr;
    int step = -1;
    int sub = -1;
    if (e.list != UINT_MAX)
    {
        /* A Show Tree list: by its id, and the cue by its own -- a deleted
           cue is gone even when another has its number now. */
        const ShowTree::CueList *list = st != nullptr ? st->cueList(e.list) : nullptr;
        if (list == nullptr)
            return false;
        chaser = qobject_cast<Chaser *>(m_doc->function(list->chaser));
        if (chaser == nullptr || e.cue == UINT_MAX)
            return false;
        for (int s = 0; s < chaser->stepsCount() && step < 0; ++s)
        {
            if (st->cueOfStep(e.list, s) == e.cue)
            {
                step = s;
                break;
            }
            for (int ss = 0;; ++ss)
            {
                const quint32 id = st->cueOfStep(e.list, s, ss);
                if (id == ShowTree::invalidId())
                    break;
                if (id == e.cue)
                {
                    step = s;
                    sub = ss;
                    break;
                }
            }
        }
        if (step < 0)
            return false;
    }
    else
    {
        // A plain chaser: by its id, the cue by its number.
        chaser = qobject_cast<Chaser *>(m_doc->function(e.chaser));
        if (chaser == nullptr || !chaser->findCueOrSubCue(e.number, &step, &sub))
            return false;
    }
    if (chaserOut != nullptr)
        *chaserOut = chaser;
    if (stepOut != nullptr)
        *stepOut = step;
    if (subOut != nullptr)
        *subOut = sub;
    return true;
}

int CueResume::loadableCount() const
{
    int n = 0;
    for (const Entry &e : offer())
        if (resolve(e, nullptr, nullptr, nullptr))
            ++n;
    return n;
}

QString CueResume::cueWords(const Entry &e) const
{
    Chaser *chaser = nullptr;
    int step = -1;
    int sub = -1;
    QString number = e.number;
    QString name = e.listName;
    if (resolve(e, &chaser, &step, &sub))
    {
        // As it is now: renumbered or renamed since, said as it reads now.
        number = ShowControl::cueName(chaser, step, sub);
        const ShowTree::CueList *list = e.list != UINT_MAX && m_doc->showTree() != nullptr
                                        ? m_doc->showTree()->cueList(e.list) : nullptr;
        name = list != nullptr ? list->name : chaser->name();
    }
    return tr("cue %1 (%2)").arg(number, name);
}

QString CueResume::offerText() const
{
    if (!m_armed || m_doc == nullptr)
        return QString();
    QStringList here;
    QStringList gone;           // "5 (Band)"
    QStringList listsGone;
    for (const Entry &e : m_pending)
    {
        if (resolve(e, nullptr, nullptr, nullptr))
            here << cueWords(e);
        else if (e.list != UINT_MAX && (m_doc->showTree() == nullptr || m_doc->showTree()->cueList(e.list) == nullptr))
            listsGone << e.listName;
        else if (e.list == UINT_MAX && qobject_cast<Chaser *>(m_doc->function(e.chaser)) == nullptr)
            listsGone << e.listName;
        else
            gone << tr("%1 (%2)").arg(e.number, e.listName);
    }

    QString text;
    if (!here.isEmpty())
    {
        text = here.count() == 1
               ? tr("The show was at %1 when it stopped. Load it?").arg(here.first())
               : tr("The show was at %1 when it stopped. Load them?").arg(joined(here));
        if (gone.count() == 1)
            text += QLatin1Char(' ') + tr("Cue %1 is no longer in the show.").arg(gone.first());
        else if (gone.count() > 1)
            text += QLatin1Char(' ') + tr("Cues %1 are no longer in the show.").arg(joined(gone));
        if (listsGone.count() == 1)
            text += QLatin1Char(' ') + tr("The cue list %1 is no longer in the show.").arg(listsGone.first());
        else if (listsGone.count() > 1)
            text += QLatin1Char(' ') + tr("The cue lists %1 are no longer in the show.").arg(joined(listsGone));
        return text;
    }

    /* Nothing of it is left: said, never loaded. */
    QStringList all;
    for (const Entry &e : m_pending)
        all << tr("cue %1 (%2)").arg(e.number, e.listName);
    return all.count() == 1
           ? tr("The show was at %1 when it stopped, but that cue is no longer in the show: "
                "nothing to load.").arg(all.first())
           : tr("The show was at %1 when it stopped, but those cues are no longer in the show: "
                "nothing to load.").arg(joined(all));
}

QString CueResume::offerDetail() const
{
    if (!m_armed)
        return QString();
    const QString when = m_pendingTime.isValid()
                         ? QLocale().toString(m_pendingTime.toLocalTime(), QLocale::ShortFormat) : QString();
    return tr("Last GO before qlcconsole stopped without closing the show%1. Load as Standby puts the "
              "cue on standby: nothing plays and the stage stays as it is; the next GO plays it. "
              "Dismiss forgets it.")
        .arg(when.isEmpty() ? QString() : tr(" (%1)").arg(when));
}

int CueResume::accept()
{
    if (!m_armed || m_doc == nullptr)
        return 0;
    const QList<Entry> entries = m_pending;
    /* Gone first: the LOADs below say what they did (commandHandled) and
       the Run tab redraws from that. */
    setArmed(false);
    int count = 0;
    for (const Entry &e : entries)
    {
        Chaser *chaser = nullptr;
        int step = -1;
        int sub = -1;
        if (!resolve(e, &chaser, &step, &sub))
            continue;
        ShowControl::Request load;
        load.command = ShowControl::Load;           // never Go: the next GO plays it
        load.cue = ShowControl::cueName(chaser, step, sub);
        load.chaser = chaser->id();
        if (m_doc->showControl()->run(load, ShowControl::RouteRunTab))
            ++count;
    }
    emit loaded(count);
    return count;
}

void CueResume::dismiss()
{
    if (!m_armed)
        return;
    setArmed(false);
    if (m_current.isEmpty())
        removeFile();
    m_pending.clear();
    m_pendingShow.clear();
}

void CueResume::setArmed(bool armed)
{
    if (m_armed == armed)
        return;
    m_armed = armed;
    emit offerChanged();
}

/*****************************************************************************
 * What is written
 *****************************************************************************/

void CueResume::overtaken()
{
    /* A cue went on (or a list stopped) since the show opened: the offer
       would undo it. The record is this session's from here. */
    if (m_armed)
        setArmed(false);
}

void CueResume::slotCueTaken(ShowControl::Command command, quint32 chaserId, int fromStep, int fromSub,
                             int step, int sub)
{
    Q_UNUSED(command);
    Q_UNUSED(fromStep);
    Q_UNUSED(fromSub);
    overtaken();
    // While the show runs: Operate, a show with a file of its own.
    if (m_doc == nullptr || m_doc->mode() != Doc::Operate || m_showFile.isEmpty())
        return;
    Chaser *chaser = qobject_cast<Chaser *>(m_doc->function(chaserId));
    if (chaser == nullptr || step < 0)
        return;

    Entry e;
    e.chaser = chaserId;
    e.number = ShowControl::cueName(chaser, step, sub);
    e.listName = chaser->name();
    if (ShowTreeController *st = m_doc->showTree())
    {
        quint32 listId = ShowTree::invalidId();
        if (st->productionOfChaser(chaserId, &listId) != ShowTree::invalidId()
            && listId != ShowTree::invalidId())
        {
            e.list = listId;
            e.cue = st->cueOfStep(listId, step, sub);
            if (const ShowTree::CueList *list = st->cueList(listId))
                e.listName = list->name;
        }
    }
    m_current.insert(chaserId, e);
    m_mainChaser = m_doc->showControl()->mainCueList();
    scheduleWrite();
}

void CueResume::slotListStopped(quint32 chaserId)
{
    overtaken();
    if (chaserId == UINT_MAX)
    {
        if (m_current.isEmpty())
            return;
        m_current.clear();
    }
    else if (m_current.remove(chaserId) == 0)
    {
        return;
    }
    scheduleWrite();
}

void CueResume::scheduleWrite()
{
    m_owns = true;
    m_writeTimer->start();
}

void CueResume::flush()
{
    if (m_writeTimer->isActive())
    {
        m_writeTimer->stop();
        write();
    }
}

void CueResume::write()
{
    if (m_current.isEmpty() || m_showFile.isEmpty())
    {
        // Nothing on any list: nothing to resume.
        removeFile();
        return;
    }

    QJsonArray lists;
    // The main cue list first: the one said first. (Not asked of the Doc
    // here: this also runs from the destructor.)
    const quint32 main = m_mainChaser;
    QList<Entry> entries = m_current.values();
    std::stable_sort(entries.begin(), entries.end(), [main](const Entry &a, const Entry &b) {
        return (a.chaser == main) > (b.chaser == main);
    });
    for (const Entry &e : entries)
    {
        QJsonObject o;
        o.insert(QStringLiteral("chaser"), qint64(e.chaser));
        if (e.list != UINT_MAX)
            o.insert(QStringLiteral("list"), qint64(e.list));
        if (e.cue != UINT_MAX)
            o.insert(QStringLiteral("cue"), qint64(e.cue));
        o.insert(QStringLiteral("number"), e.number);
        o.insert(QStringLiteral("listName"), e.listName);
        lists.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), kVersion);
    root.insert(QStringLiteral("show"), m_showFile);
    root.insert(QStringLiteral("written"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    root.insert(QStringLiteral("lists"), lists);

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    // Whole or not at all, and on the disk before it counts (QSaveFile syncs).
    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly)
        || f.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) < 0
        || !f.commit())
    {
        qWarning() << "[CueResume] Could not write" << m_path << f.errorString();
        return;
    }
    ++m_writes;
}

void CueResume::removeFile()
{
    if (QFile::exists(m_path))
    {
        QFile::remove(m_path);
        ++m_writes;
    }
}

void CueResume::readFile()
{
    m_pending.clear();
    m_pendingShow.clear();
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != kVersion)
        return;
    m_pendingShow = samePath(root.value(QStringLiteral("show")).toString());
    m_pendingTime = QDateTime::fromString(root.value(QStringLiteral("written")).toString(), Qt::ISODate);
    for (const QJsonValue &v : root.value(QStringLiteral("lists")).toArray())
    {
        const QJsonObject o = v.toObject();
        Entry e;
        e.chaser = quint32(o.value(QStringLiteral("chaser")).toInteger(UINT_MAX));
        e.list = quint32(o.value(QStringLiteral("list")).toInteger(UINT_MAX));
        e.cue = quint32(o.value(QStringLiteral("cue")).toInteger(UINT_MAX));
        e.number = o.value(QStringLiteral("number")).toString();
        e.listName = o.value(QStringLiteral("listName")).toString();
        if (!e.number.isEmpty())
            m_pending << e;
    }
    if (m_pendingShow.isEmpty())
        m_pending.clear();
}
