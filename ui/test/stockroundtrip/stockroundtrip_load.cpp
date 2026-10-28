/*
  Q Light Controller Plus - Unit test
  stockroundtrip_load.cpp

  Stock QLC+ 4.14.4's engine (built from the pinned upstream copy in
  stock/), reading a workspace the way QLC+ 4's App::loadXML does: the
  <Workspace>'s <Engine> through Doc::loadXML, everything else read past.
  It writes what it read as the fingerprint (fingerprint.h).

  usage: stockroundtrip_load <show.qxw> <resources dir> <fingerprint out>
  exit 0: loaded; 2: not a workspace; 3: Doc::loadXML failed; 4: I/O.

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

#include <QGuiApplication>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QFile>
#include <QDir>

#include "qlcfixturedefcache.h"
#include "rgbscriptscache.h"
#include "fingerprint.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.count() < 4)
    {
        QTextStream(stderr) << "usage: stockroundtrip_load <show.qxw> <resources dir> <out>\n";
        return 4;
    }
    const QDir res(args.at(2));

    Doc doc(nullptr);
    doc.fixtureDefCache()->loadMap(QDir(res.filePath(QStringLiteral("fixtures"))));
    doc.rgbScriptsCache()->load(QDir(res.filePath(QStringLiteral("rgbscripts"))));

    QFile file(args.at(1));
    if (!file.open(QIODevice::ReadOnly))
        return 4;
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("Workspace"))
        return 2;
    bool engine = false;
    while (xml.readNextStartElement())
    {
        if (xml.name() == QLatin1String("Engine"))
        {
            if (!doc.loadXML(xml))
                return 3;
            engine = true;
        }
        else
            xml.skipCurrentElement();
    }
    if (!engine)
        return 3;
    if (!doc.errorLog().isEmpty())
        QTextStream(stderr) << "stock QLC+ 4.14.4 says: " << doc.errorLog() << "\n";

    QFile out(args.at(3));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return 4;
    QTextStream ts(&out);
    for (const QString &line : StockRoundTrip::fingerprint(&doc))
        ts << line << "\n";
    return 0;
}
