/*
  Q Light Controller Plus
  deviceprofile.cpp

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

#include <QXmlStreamReader>
#include <QFile>
#include <QDir>

#include "deviceprofile.h"
#include "qlcconfig.h"
#include "qlcfile.h"

/** Expand a 0-based port index into an Art-Net style packed address,
 *  wrapping into the next subnet/net the same way a real installation
 *  would once 16 universes (one subnet) or 256 (one net) are used up. */
static quint32 packedPortAddress(int index)
{
    const uint net = uint(index / 256);
    const uint sub = uint((index % 256) / 16);
    const uint uni = uint(index % 16);
    return quint32((net << 8) | (sub << 4) | uni);
}

/** Parse "net:subnet:universe" the same way ConnectionsTree::addManualPort()
 *  reads it from its dialog. Returns false (leaving addr untouched) if the
 *  text isn't that shape -- a bad hand-edited profile should lose just that
 *  one port, not the whole file. */
static bool parsePortAddress(const QString &text, quint32 &addr)
{
    const QStringList parts = text.split(':');
    if (parts.count() != 3)
        return false;

    bool okNet = false, okSub = false, okUni = false;
    const uint net = parts.at(0).toUInt(&okNet);
    const uint sub = parts.at(1).toUInt(&okSub);
    const uint uni = parts.at(2).toUInt(&okUni);
    if (!okNet || !okSub || !okUni || net > 127 || sub > 15 || uni > 15)
        return false;

    addr = quint32((net << 8) | (sub << 4) | uni);
    return true;
}

static void loadDeviceProfileFile(const QString &path, QList<DeviceProfile> &out)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return;

    QXmlStreamReader xml(&file);
    while (xml.readNextStartElement())
    {
        if (xml.name() != QLatin1String("DeviceProfiles"))
        {
            xml.skipCurrentElement();
            continue;
        }

        while (xml.readNextStartElement())
        {
            if (xml.name() != QLatin1String("Profile"))
            {
                xml.skipCurrentElement();
                continue;
            }

            DeviceProfile dp;
            dp.manufacturer = xml.attributes().value("Manufacturer").toString();
            dp.model = xml.attributes().value("Model").toString();
            dp.protocol = xml.attributes().value("Protocol").toString();
            const QString portsAttr = xml.attributes().value("Ports").toString();

            if (portsAttr.isEmpty() == false)
            {
                bool ok = false;
                const int count = portsAttr.toInt(&ok);
                if (ok && count > 0)
                    for (int i = 0; i < count; i++)
                        dp.ports << packedPortAddress(i);
                xml.skipCurrentElement();
            }
            else
            {
                while (xml.readNextStartElement())
                {
                    if (xml.name() == QLatin1String("Port"))
                    {
                        quint32 addr = 0;
                        if (parsePortAddress(xml.attributes().value("Address").toString(), addr))
                            dp.ports << addr;
                    }
                    xml.skipCurrentElement();
                }
            }

            if (dp.manufacturer.isEmpty() == false && dp.model.isEmpty() == false
                    && dp.ports.isEmpty() == false)
                out << dp;
        }
    }
}

/** Load every *.xml directly inside one manufacturer subdirectory. */
static void loadManufacturerDirectory(const QDir &manufacturerDir, QList<DeviceProfile> &out)
{
    QDir xmlDir(manufacturerDir);
    xmlDir.setFilter(QDir::Files);
    xmlDir.setNameFilters(QStringList() << QString("*.xml"));
    foreach (const QString &fileName, xmlDir.entryList())
    {
        const QString path = xmlDir.absoluteFilePath(fileName);
        if (QLCFile::isOnlineOnly(path))   // in cloud storage, not here: do not wait for it
        {
            QLCFile::noteSkippedOnlineOnly(path);
            continue;
        }
        loadDeviceProfileFile(path, out);
    }
}

/** Walk one root (the bundled library, or a user's own) for its
 *  manufacturer subdirectories -- same shape as resources/fixtures, one
 *  folder per manufacturer -- and load every profile file under each. Both
 *  QLCFile::systemDirectory() and userDirectory() always filter to files
 *  regardless of what is asked for, so only their resolved path is used
 *  here; the directory listing is built fresh. */
static void loadLibraryRoot(const QDir &root, QList<DeviceProfile> &out)
{
    QDir dirs(root);
    dirs.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QString &manufacturer, dirs.entryList())
        loadManufacturerDirectory(QDir(dirs.absoluteFilePath(manufacturer)), out);
}

QList<DeviceProfile> loadDeviceProfiles()
{
    QList<DeviceProfile> result;

    QDir sysDir = QLCFile::systemDirectory(QString(DEVICEPROFILEDIR));
    loadLibraryRoot(sysDir, result);

    QDir usrDir = QLCFile::userDirectory(QString(USERDEVICEPROFILEDIR), QString(DEVICEPROFILEDIR),
            QStringList() << QString("*.xml"));
    loadLibraryRoot(usrDir, result);

    return result;
}
