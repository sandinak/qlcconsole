/*
  Q Light Controller Plus
  timelinefiledrop.cpp

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

#include <QFileInfo>
#include <QCoreApplication>

#include "timelinefiledrop.h"
#include "audioplugincache.h"
#include "audio.h"
#include "video.h"
#include "doc.h"

static bool fileMatches(const QString &path, const QStringList &patterns)
{
    const QString ext = QStringLiteral("*.") + QFileInfo(path).suffix().toLower();
    for (const QString &p : patterns)
        if (p.toLower() == ext)
            return true;
    return false;
}

static QString tr(const char *s)
{
    return QCoreApplication::translate("TimelineFileDrop", s);
}

TimelineFileDrop::Result TimelineFileDrop::prepare(Doc *doc, const QStringList &paths)
{
    Result r;
    const QStringList audioFmt = doc->audioPluginCache()->getSupportedFormats();
    const QStringList videoFmt = Video::getVideoCapabilities();

    foreach (const QString &path, paths)
    {
        const QString name = QFileInfo(path).fileName();
        if (QFileInfo(path).isFile() == false)
            r.errors << tr("Can't use %1: it is not a file.").arg(name);
        else if (fileMatches(path, audioFmt))
        {
            Audio *a = new Audio(doc);
            if (a->setSourceFileName(path) == false)
            {
                delete a;
                r.errors << tr("Can't use %1: no audio decoder could open it.").arg(name);
            }
            else
                r.made << a;
        }
        else if (fileMatches(path, videoFmt))
        {
            Video *v = new Video(doc);
            if (v->setSourceUrl(path) == false)
            {
                delete v;
                r.errors << tr("Can't use %1: this video can't be played.").arg(name);
            }
            else
                r.made << v;
        }
        else
            r.errors << tr("Can't use %1: not an audio or video format that can be played.").arg(name);
    }
    return r;
}

QList<quint32> TimelineFileDrop::probeLengths(Doc *doc, const QStringList &paths)
{
    const QStringList audioFmt = doc->audioPluginCache()->getSupportedFormats();
    QList<quint32> lengths;
    for (int i = 0; i < paths.count() && i < 8; i++)
    {
        quint32 len = 0;
        if (fileMatches(paths.at(i), audioFmt) && QFileInfo(paths.at(i)).isFile())
        {
            Audio probe(doc);
            if (probe.setSourceFileName(paths.at(i)))
                len = probe.totalDuration();
        }
        lengths << len;
    }
    return lengths;
}

QString TimelineFileDrop::summary(const Result &r)
{
    QString msg = (r.made.count() == 1)
        ? QCoreApplication::translate("TimelineFileDrop", "Added %1.").arg(r.made.first()->name())
        : QCoreApplication::translate("TimelineFileDrop", "Added %1 files.").arg(r.made.count());
    if (r.errors.isEmpty() == false)
        msg += QStringLiteral("  ") + r.errors.join(QStringLiteral("  "));
    return msg;
}
