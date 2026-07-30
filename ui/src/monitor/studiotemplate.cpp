/*
  Q Light Controller Plus
  studiotemplate.cpp

  Copyright (c) Massimo Callegari

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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QObject>

#include <algorithm>

#include "studiotemplate.h"
#include "qlcfile.h"
#include "qlcconfig.h"
#include "doc.h"
#include "fixture.h"
#include "monitorproperties.h"

namespace {

const char *kMagic = "fixturestudio_template";

// Turn a display name into a safe, stable filename stem.
QString sanitize(const QString &name)
{
    QString s;
    foreach (QChar c, name.trimmed())
        s += (c.isLetterOrNumber() || c == '-' || c == '_' || c == ' ') ? c : QChar('_');
    s = s.simplified().replace(' ', '_');
    return s.isEmpty() ? QStringLiteral("component") : s;
}

// Members of a studio frame group, ordered by fixture id for a stable role order.
QList<quint32> frameMembers(MonitorProperties *props, quint32 groupId)
{
    QList<quint32> out;
    foreach (quint32 fid, props->fixtureItemsID())
        if (props->fixtureFrameGroup(fid) == groupId)
            out << fid;
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

bool StudioTemplate::saveGroup(Doc *doc, quint32 groupId, const QString &filePath, QString *error)
{
    if (doc == nullptr)
        return false;
    MonitorProperties *props = doc->monitorProperties();
    if (!props->hasGroup(groupId))
    {
        if (error) *error = QObject::tr("No such studio group.");
        return false;
    }
    const MonitorProperties::MonitorGroup g = props->group(groupId);

    QJsonArray roles;
    foreach (quint32 fid, frameMembers(props, groupId))
    {
        const QVector3D lp = props->fixtureRigProps(fid).groupLocal;
        Fixture *fx = doc->fixture(fid);
        QJsonObject role;
        role["label"] = fx ? fx->name() : QString::number(fid);
        role["x"] = double(lp.x());
        role["y"] = double(lp.y());
        role["z"] = double(lp.z());
        roles.append(role);
    }
    if (roles.isEmpty())
    {
        if (error) *error = QObject::tr("The studio group has no members to save.");
        return false;
    }

    QJsonObject root;
    root[kMagic]      = 1;
    root["name"]      = g.name;
    root["anchorKind"] = g.anchorKind;
    root["pitchX"]    = double(g.pitchX);
    root["pitchY"]    = double(g.pitchY);
    root["roles"]     = roles;

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

static QJsonObject loadRoot(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    QJsonObject root = d.object();
    if (!root.contains(kMagic))
        return QJsonObject();
    return root;
}

int StudioTemplate::roleCount(const QString &filePath)
{
    const QJsonObject root = loadRoot(filePath);
    return root.value("roles").toArray().size();
}

quint32 StudioTemplate::stamp(Doc *doc, const QString &filePath,
                              const QList<quint32> &fixtureIds, QString *error)
{
    if (doc == nullptr)
        return 0;
    const QJsonObject root = loadRoot(filePath);
    if (root.isEmpty())
    {
        if (error) *error = QObject::tr("Not a Fixture Studio template.");
        return 0;
    }
    const QJsonArray roles = root.value("roles").toArray();
    if (roles.isEmpty() || fixtureIds.isEmpty())
    {
        if (error) *error = QObject::tr("Nothing to stamp (no roles or no fixtures).");
        return 0;
    }
    MonitorProperties *props = doc->monitorProperties();

    // Bind roles to fixtures in order, up to the shorter of the two.
    const int n = qMin(roles.size(), fixtureIds.size());

    // Origin = centroid of the chosen fixtures' current world positions, so the
    // stamped unit lands where the selection already is.
    QVector3D sum;
    int counted = 0;
    for (int i = 0; i < n; ++i)
    {
        const quint32 fid = fixtureIds.at(i);
        if (!props->containsFixture(fid))
            continue;
        sum += props->fixtureRigPosition(fid);
        ++counted;
    }
    const QVector3D origin = counted > 0 ? (sum / float(counted)) : QVector3D();

    const quint32 gid = props->nextGroupId();
    props->createGroup(gid, root.value("name").toString(QObject::tr("Component")),
                       props->activeLayerId(), 0);
    props->setGroupFrame(gid, origin, 0.0f);
    props->setGroupHasFrame(gid, true);
    const QString anchorKind = root.value("anchorKind").toString();
    props->setGroupBinding(gid, 0,
                           float(root.value("pitchX").toDouble(0.5)),
                           float(root.value("pitchY").toDouble(0.5)));

    for (int i = 0; i < n; ++i)
    {
        const quint32 fid = fixtureIds.at(i);
        if (!props->containsFixture(fid))
            continue;
        const QJsonObject role = roles.at(i).toObject();
        props->setFixtureGroup(fid, gid);
        FixtureRigProps rp = props->fixtureRigProps(fid);
        rp.groupLocal = QVector3D(float(role.value("x").toDouble()),
                                  float(role.value("y").toDouble()),
                                  float(role.value("z").toDouble()));
        props->setFixtureRigProps(fid, rp);
    }
    Q_UNUSED(anchorKind);
    doc->setModified();
    return gid;
}

// --- Browsable library -------------------------------------------------------

QString StudioTemplate::libraryPath()
{
    // Under the user's QLC+ data dir (USERQLCPLUSDIR is platform-correct), in a
    // dedicated StudioComponents subfolder. userDirectory() creates it for us.
    const QString sub = QString(USERQLCPLUSDIR) + "/StudioComponents";
    QDir dir = QLCFile::userDirectory(sub, sub, QStringList() << "*.json");
    return dir.absolutePath();
}

StudioTemplate::Info StudioTemplate::info(const QString &filePath)
{
    Info out;
    const QJsonObject root = loadRoot(filePath);
    if (root.isEmpty())
        return out;
    out.path = filePath;
    out.name = root.value("name").toString(QFileInfo(filePath).completeBaseName());
    out.anchorKind = root.value("anchorKind").toString();
    foreach (const QJsonValue &v, root.value("roles").toArray())
    {
        const QJsonObject r = v.toObject();
        out.locals << QVector3D(float(r.value("x").toDouble()),
                                float(r.value("y").toDouble()),
                                float(r.value("z").toDouble()));
    }
    return out;
}

QList<StudioTemplate::Info> StudioTemplate::library()
{
    QList<Info> out;
    QDir dir(libraryPath());
    foreach (const QString &f, dir.entryList(QStringList() << "*.json", QDir::Files, QDir::Name))
    {
        const Info i = info(dir.absoluteFilePath(f));
        if (i.isValid())
            out << i;
    }
    std::sort(out.begin(), out.end(), [](const Info &a, const Info &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

QString StudioTemplate::saveToLibrary(Doc *doc, quint32 groupId, const QString &name, QString *error)
{
    const QString stem = sanitize(name);
    QDir dir(libraryPath());
    // Avoid clobbering a different component that sanitises to the same stem.
    QString path = dir.absoluteFilePath(stem + ".json");
    int n = 2;
    while (QFile::exists(path))
    {
        const Info existing = info(path);
        if (existing.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            break;   // same component — overwrite it
        path = dir.absoluteFilePath(QString("%1_%2.json").arg(stem).arg(n++));
    }
    if (!saveGroup(doc, groupId, path, error))
        return QString();
    // saveGroup writes the group's current name; force the requested display name.
    QJsonObject root = loadRoot(path);
    if (!root.isEmpty())
    {
        root["name"] = name.trimmed().isEmpty() ? stem : name.trimmed();
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
            f.close();
        }
    }
    return path;
}

bool StudioTemplate::removeFile(const QString &filePath)
{
    return QFile::remove(filePath);
}

QString StudioTemplate::renameInLibrary(const QString &filePath, const QString &newName, QString *error)
{
    QJsonObject root = loadRoot(filePath);
    if (root.isEmpty())
    {
        if (error) *error = QObject::tr("Not a Fixture Studio component.");
        return QString();
    }
    root["name"] = newName.trimmed();

    QDir dir = QFileInfo(filePath).absoluteDir();
    QString stem = sanitize(newName);
    QString newPath = dir.absoluteFilePath(stem + ".json");
    int n = 2;
    while (QFile::exists(newPath) && newPath != filePath)
        newPath = dir.absoluteFilePath(QString("%1_%2.json").arg(stem).arg(n++));

    QFile f(newPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error) *error = f.errorString();
        return QString();
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    if (newPath != filePath)
        QFile::remove(filePath);
    return newPath;
}
