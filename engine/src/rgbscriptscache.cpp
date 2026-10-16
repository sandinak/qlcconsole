/*
  Q Light Controller Plus
  rgbscriptscache.cpp

  Copyright (c) David Garyga

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

#include <QDebug>
#include <QDir>
#include <QTextStream>

#include "rgbscriptscache.h"
  #include "rgbscriptv4.h"
#include "qlcconfig.h"
#include "qlcfile.h"

RGBScriptsCache::RGBScriptsCache(Doc* doc)
    : m_doc(doc)
{
}

QStringList RGBScriptsCache::names() const
{
    QStringList list = m_scriptsMap.keys();
    for (auto it = m_showScripts.constBegin(); it != m_showScripts.constEnd(); ++it)
        if (!list.contains(it.key()))
            list << it.key();
    return list;
}

QString RGBScriptsCache::scriptFile(const QString &name) const
{
    const QString show = m_showScripts.value(name);
    return show.isEmpty() ? m_scriptsMap.value(name) : show;
}

void RGBScriptsCache::setShowScript(const QString &name, const QString &path)
{
    if (path.isEmpty())
        m_showScripts.remove(name);
    else
        m_showScripts.insert(name, path);
}

QString RGBScriptsCache::scriptNameIn(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream in(&file);
    for (QString line = in.readLine(); !line.isNull(); line = in.readLine())
    {
        const QStringList tokens = line.split("=");
        if (tokens.length() == 2 && tokens[0].simplified() == "algo.name")
        {
            QString algoName = tokens[1].simplified().remove('"');
            algoName.remove(';');
            return algoName;
        }
    }
    return QString();
}

RGBScript* RGBScriptsCache::script(QString name) const
{
    RGBScript *mScript = new RGBScript(m_doc);
    QString filename = scriptFile(name);
    if (filename.isEmpty())
    {
        return mScript;
    }
    else
    {
        mScript->load(filename);
        return mScript;
    }
}

bool RGBScriptsCache::load(const QDir& dir)
{
    qDebug() << "Loading RGB scripts in " << dir.path() << "...";

    if (dir.exists() == false || dir.isReadable() == false)
        return false;

    foreach (QString file, dir.entryList())
    {
        if (QLCFile::isOnlineOnly(dir.absoluteFilePath(file)))   // in cloud storage, not here: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(dir.absoluteFilePath(file));
            continue;
        }
        if (!file.endsWith(".js", Qt::CaseInsensitive))
        {
            qDebug() << "    " << file << " skipped (special file or does not end on *.js)";
            continue;
        }
        QFile absFile(dir.absoluteFilePath(file));
        QString absFilename = absFile.fileName();

        if (m_scriptsMap.value(absFilename).isEmpty())
        {
            if (!absFile.open(QIODevice::ReadOnly | QIODevice::Text))
                return false;

            QTextStream in(&absFile);
            QString line = in.readLine();
            while (!line.isNull())
            {
                QStringList tokens = line.split("=");
                if (tokens.length() == 2 && tokens[0].simplified() == "algo.name")
                {
                    QString algoName = tokens[1].simplified().remove('"');
                    algoName.remove(';');
                    m_scriptsMap.insert(algoName, absFilename);
                    qDebug() << "    " << algoName << "script loaded";
                    break;
                }
                line = in.readLine();
            }
            absFile.close();
        }
        else
        {
            qDebug() << "    " << file << " already known";
        }
    }
    return true;
}

QDir RGBScriptsCache::systemScriptsDirectory()
{
    return QLCFile::systemDirectory(QString(RGBSCRIPTDIR), QString(".js"));
}

QDir RGBScriptsCache::userScriptsDirectory()
{
    return QLCFile::userDirectory(QString(USERRGBSCRIPTDIR), QString(RGBSCRIPTDIR),
            QStringList() << QString("*%1").arg(".js"));
}
