/*
  Q Light Controller Plus - qlcconsole
  showreviewcli.cpp

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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <cstdio>
#include <cstring>

#include "showreviewcli.h"
#include "qlcfixturedefcache.h"
#include "qlcmodifierscache.h"
#include "rgbscriptscache.h"
#include "showembedding.h"
#include "inputoutputmap.h"
#include "showreview.h"
#include "qlcconfig.h"
#include "universe.h"
#include "qlcfile.h"
#include "doc.h"

static bool s_verbose = false;

static void quietHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    // Loading a show talks a lot; the review's output is what matters.
    if (s_verbose || type == QtFatalMsg)
        fprintf(stderr, "%s\n", qPrintable(msg));
}

bool ShowReviewCli::wanted(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (strcmp(argv[i], "--review") == 0)
            return true;
    return false;
}

QString ShowReviewCli::usage()
{
    return QStringLiteral(
        "Usage: qlcconsole --review <show.qlcc|show.qxw> [options]\n"
        "Show Review with no window: lists what is likely wrong with the show's design.\n"
        "  --format text|json     How to print (default text)\n"
        "  --min note|warning|blocker\n"
        "                         The least severity listed and counted for the exit code\n"
        "                         (default warning)\n"
        "  --check ID,ID,AREA-*   Only these checks (e.g. PAT-01,CUE-*)\n"
        "  --include-accepted     Also list accepted findings, with their notes\n"
        "  --verbose              Show the loader's messages\n"
        "Exit: 0 nothing open, 1 open warnings, 2 open blockers, 3 the show couldn't be read.\n"
        "Never writes the show; loads no plugins and sends nothing.\n");
}

int ShowReviewCli::main(int argc, char **argv)
{
    // No window, no Dock icon, no display needed (a CI box, a Pi over ssh).
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcplus"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qlcplus.org"));
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));

    QStringList args = QCoreApplication::arguments();
    args.removeFirst();
    s_verbose = args.contains(QStringLiteral("--verbose"));
    qInstallMessageHandler(quietHandler);
    QTextStream out(stdout);
    QTextStream err(stderr);
    const int code = run(args, out, err, true);
    out.flush();
    err.flush();
    return code;
}

/** Which universes the file patches an output on (the Doc loads no I/O). */
static QSet<quint32> universesWithOutput(const QString &path)
{
    QSet<quint32> unis;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return unis;
    QXmlStreamReader r(&file);
    bool inMap = false;
    qint64 current = -1;
    while (!r.atEnd())
    {
        const QXmlStreamReader::TokenType t = r.readNext();
        if (t == QXmlStreamReader::StartElement)
        {
            if (r.name() == KXMLIOMap)
                inMap = true;
            else if (inMap && r.name() == KXMLQLCUniverse)
                current = r.attributes().value(KXMLQLCUniverseID).toLongLong();
            else if (inMap && current >= 0 && r.name() == KXMLQLCUniverseOutputPatch)
                unis.insert(quint32(current));
        }
        else if (t == QXmlStreamReader::EndElement)
        {
            if (r.name() == KXMLIOMap)
                break;
            if (r.name() == KXMLQLCUniverse)
                current = -1;
        }
    }
    return unis;
}

static ReviewFinding::Severity severityFrom(const QString &s, bool *ok)
{
    *ok = true;
    if (s == QLatin1String("note"))
        return ReviewFinding::Note;
    if (s == QLatin1String("warning"))
        return ReviewFinding::Warning;
    if (s == QLatin1String("blocker"))
        return ReviewFinding::Blocker;
    *ok = false;
    return ReviewFinding::Warning;
}

static QString stateName(const ReviewFinding &f)
{
    switch (f.state)
    {
        case ReviewFinding::Accepted: return QStringLiteral("accepted");
        case ReviewFinding::Changed: return QStringLiteral("changed");
        default: return QStringLiteral("open");
    }
}

int ShowReviewCli::run(const QStringList &args, QTextStream &out, QTextStream &err, bool libraries)
{
    QString path, format = QStringLiteral("text");
    ReviewFinding::Severity min = ReviewFinding::Warning;
    bool includeAccepted = false;
    ShowReview::Options options;
    for (int i = 0; i < args.count(); ++i)
    {
        const QString a = args.at(i);
        auto next = [&](QString *value) {
            if (i + 1 >= args.count())
                return false;
            *value = args.at(++i);
            return true;
        };
        QString v;
        if (a == QLatin1String("--review"))
        {
            if (!next(&path))
                path.clear();
        }
        else if (a == QLatin1String("--format") && next(&v) && (v == QLatin1String("text") || v == QLatin1String("json")))
            format = v;
        else if (a == QLatin1String("--min") && next(&v))
        {
            bool ok = false;
            min = severityFrom(v, &ok);
            if (!ok)
            {
                err << "Unknown severity: " << v << "\n" << usage();
                return Unreadable;
            }
        }
        else if (a == QLatin1String("--check") && next(&v))
            options.checks = v.split(QLatin1Char(','), Qt::SkipEmptyParts);
        else if (a == QLatin1String("--include-accepted"))
            includeAccepted = true;
        else if (a == QLatin1String("--verbose"))
            continue;
        else if (a == QLatin1String("-h") || a == QLatin1String("--help"))
        {
            out << usage();
            return Clean;
        }
        else
        {
            err << "Unknown argument: " << a << "\n" << usage();
            return Unreadable;
        }
    }
    if (path.isEmpty())
    {
        err << usage();
        return Unreadable;
    }
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable())
    {
        err << "Can't read " << path << "\n";
        return Unreadable;
    }

    Doc doc(nullptr);
    if (libraries)
    {
        doc.fixtureDefCache()->load(QLCFixtureDefCache::userDefinitionDirectory());
        doc.fixtureDefCache()->loadMap(QLCFixtureDefCache::systemDefinitionDirectory());
        doc.modifiersCache()->load(QLCModifiersCache::systemTemplateDirectory(), true);
        doc.modifiersCache()->load(QLCModifiersCache::userTemplateDirectory());
        doc.rgbScriptsCache()->load(RGBScriptsCache::systemScriptsDirectory());
        doc.rgbScriptsCache()->load(RGBScriptsCache::userScriptsDirectory());
    }
    const QString folder = info.absolutePath();
    doc.setWorkspacePath(folder);
    if (doc.showEmbedding()->readWorkspace(info.absoluteFilePath()))
        doc.showEmbedding()->apply(folder);

    bool loaded = false;
    {
        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly))
        {
            QXmlStreamReader r(&file);
            while (!r.atEnd() && !loaded)
            {
                if (r.readNextStartElement() && r.name() == KXMLQLCEngine)
                    loaded = doc.loadXML(r, false);
            }
        }
    }
    if (!loaded)
    {
        err << "Can't read " << path << " as a show\n";
        return Unreadable;
    }
    // The Virtual Console is the UI's: read what it saved, without it.
    doc.showReview()->setVcFile(info.absoluteFilePath());
    options.outputsLoaded = false;
    options.universesWithOutput = universesWithOutput(info.absoluteFilePath());

    const ShowReview::Result result = doc.showReview()->run(options);

    QList<ReviewFinding> listed;
    for (const ReviewFinding &f : result.findings)
    {
        if (f.severity < min)
            continue;
        if (f.state == ReviewFinding::Accepted && !includeAccepted)
            continue;
        listed << f;
    }
    int open[3] = { 0, 0, 0 };
    for (const ReviewFinding &f : result.findings)
        if (f.isOpen())
            open[f.severity]++;

    if (format == QLatin1String("json"))
    {
        QJsonObject root;
        root.insert(QStringLiteral("show"), info.fileName());
        root.insert(QStringLiteral("version"), QStringLiteral(APPVERSION));
        QJsonObject counts;
        counts.insert(QStringLiteral("blockers"), result.open[ReviewFinding::Blocker]);
        counts.insert(QStringLiteral("warnings"), result.open[ReviewFinding::Warning]);
        counts.insert(QStringLiteral("notes"), result.open[ReviewFinding::Note]);
        counts.insert(QStringLiteral("accepted"), result.accepted[0] + result.accepted[1] + result.accepted[2]);
        root.insert(QStringLiteral("open"), counts);
        root.insert(QStringLiteral("ms"), qint64(result.ms));
        QJsonObject perCheck;
        for (auto it = result.checkMs.constBegin(); it != result.checkMs.constEnd(); ++it)
            if (it.value() > 0)
                perCheck.insert(it.key(), qint64(it.value()));
        root.insert(QStringLiteral("checkMs"), perCheck);
        QJsonArray findings;
        for (const ReviewFinding &f : listed)
        {
            QJsonObject o;
            o.insert(QStringLiteral("check"), f.check);
            o.insert(QStringLiteral("severity"), ShowReview::severityName(f.severity));
            o.insert(QStringLiteral("state"), stateName(f));
            o.insert(QStringLiteral("message"), f.message);
            o.insert(QStringLiteral("targets"), QJsonArray::fromStringList(f.targets));
            if (f.state != ReviewFinding::Open)
            {
                o.insert(QStringLiteral("acceptedBy"), f.acceptedBy);
                o.insert(QStringLiteral("acceptedWhen"), f.acceptedWhen);
                o.insert(QStringLiteral("note"), f.acceptedNote);
            }
            findings.append(o);
        }
        root.insert(QStringLiteral("findings"), findings);
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    }
    else
    {
        const int accepted = result.accepted[0] + result.accepted[1] + result.accepted[2];
        out << "Show Review: " << info.fileName() << " -- "
            << result.open[ReviewFinding::Blocker] << " blocker(s), "
            << result.open[ReviewFinding::Warning] << " warning(s), "
            << result.open[ReviewFinding::Note] << " note(s) open";
        if (accepted > 0)
            out << ", " << accepted << " accepted";
        out << "\n";
        int lastSeverity = -1;
        for (const ReviewFinding &f : listed)
        {
            if (int(f.severity) != lastSeverity)
            {
                lastSeverity = int(f.severity);
                out << "\n" << (f.severity == ReviewFinding::Blocker ? "Blockers"
                                : f.severity == ReviewFinding::Warning ? "Warnings" : "Notes") << "\n";
            }
            out << "  " << f.check.leftJustified(8) << ' ';
            if (f.state != ReviewFinding::Open)
                out << '[' << stateName(f) << "] ";
            // The keys, short: a long list of fixtures is the message's to name.
            QStringList keys = f.targets.mid(0, 4);
            if (f.targets.count() > 4)
                keys << QStringLiteral("+%1").arg(f.targets.count() - 4);
            out << f.message << "  [" << keys.join(QLatin1Char(',')) << "]\n";
            if (f.state != ReviewFinding::Open)
                out << "           \"" << f.acceptedNote << "\" -- " << f.acceptedBy << ", "
                    << f.acceptedWhen.left(10) << "\n";
        }
    }

    if (open[ReviewFinding::Blocker] > 0 && min <= ReviewFinding::Blocker)
        return Blockers;
    if (open[ReviewFinding::Warning] > 0 && min <= ReviewFinding::Warning)
        return Warnings;
    return Clean;
}
