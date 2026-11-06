/*
  Q Light Controller Plus
  units_test.cpp

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

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QFile>
#include <QtTest>
#include <functional>

#include "../../../engine/test/common/stdprelude.h"
#define protected public
#define private public
#include "monitor.h"
#include "monitorfixtureitem.h"
#include "stagepiecedialog.h"
#include "performersdialog.h"
#undef private
#undef protected

#include "units_test.h"
#include "addrgbpanel.h"
#include "lengthinputdialog.h"
#include "lengthspinbox.h"
#include "measurementunits.h"
#include "monitorproperties.h"
#include "networkreport.h"
#include "networkreportdialog.h"
#include "networktopology.h"
#include "networktopologydialog.h"
#include "stageplatform.h"
#include "stagetarget.h"
#include "fixture.h"
#include "qlcfile.h"
#include "stand.h"
#include "tower.h"
#include "truss.h"
#include "pipe.h"
#include "doc.h"

using MU = MeasurementUnits;

namespace
{

/****************************************************************************
 * The sources
 ****************************************************************************/

struct Literal
{
    QString file;   ///< relative to the source root
    int line;
    QString text;
};

struct Source
{
    QString file;           ///< relative to the source root
    QStringList lines;      ///< as written
    QStringList code;       ///< comments gone, string bodies blanked
    QList<Literal> literals;
};

/* Where a line says on purpose that a unit is meant: "units-ok: why" on it
   or up to three lines above it (a call broken over lines). */
bool markedOk(const Source &src, int line)
{
    for (int l = line; l >= qMax(1, line - 3); --l)
        if (src.lines.value(l - 1).contains(QLatin1String("units-ok:")))
            return true;
    return false;
}

/* Reads a C++ file: its string literals, and its code with comments and
   string bodies taken out (so a word in a comment or a string is not code). */
Source readCpp(const QString &root, const QString &path)
{
    Source src;
    src.file = QDir(root).relativeFilePath(path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return src;
    const QString all = QString::fromUtf8(f.readAll());
    src.lines = all.split(QLatin1Char('\n'));

    enum State { Code, LineComment, BlockComment, String, Char };
    State st = Code;
    int line = 1;
    QString code, lit;
    int litLine = 0;
    for (int i = 0; i < all.size(); ++i)
    {
        const QChar c = all.at(i);
        const QChar n = i + 1 < all.size() ? all.at(i + 1) : QChar();
        if (c == QLatin1Char('\n'))
        {
            if (st == LineComment)
                st = Code;
            src.code << code;
            code.clear();
            ++line;
            if (st == String)       // an unterminated literal: drop it
                st = Code;
            continue;
        }
        switch (st)
        {
            case Code:
                if (c == QLatin1Char('/') && n == QLatin1Char('/')) { st = LineComment; ++i; }
                else if (c == QLatin1Char('/') && n == QLatin1Char('*')) { st = BlockComment; ++i; }
                else if (c == QLatin1Char('"')) { st = String; lit.clear(); litLine = line; code += c; }
                else if (c == QLatin1Char('\'')) { st = Char; code += c; }
                else code += c;
            break;
            case LineComment:
            break;
            case BlockComment:
                if (c == QLatin1Char('*') && n == QLatin1Char('/')) { st = Code; ++i; }
            break;
            case String:
                if (c == QLatin1Char('\\')) { lit += c; lit += n; ++i; }
                else if (c == QLatin1Char('"'))
                {
                    st = Code;
                    code += c;
                    src.literals << Literal{ src.file, litLine, lit };
                }
                else lit += c;
            break;
            case Char:
                if (c == QLatin1Char('\\')) ++i;
                else if (c == QLatin1Char('\'')) { st = Code; code += c; }
            break;
        }
    }
    src.code << code;
    return src;
}

/* A .ui form: each <string> body as a literal */
Source readUi(const QString &root, const QString &path)
{
    Source src;
    src.file = QDir(root).relativeFilePath(path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return src;
    src.lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    static const QRegularExpression re(QStringLiteral("<string[^>]*>([^<]*)</string>"));
    for (int i = 0; i < src.lines.size(); ++i)
    {
        QRegularExpressionMatchIterator it = re.globalMatch(src.lines.at(i));
        while (it.hasNext())
            src.literals << Literal{ src.file, i + 1, it.next().captured(1) };
    }
    return src;
}

const QList<Source> &sources()
{
    static QList<Source> list;
    if (!list.isEmpty())
        return list;
    const QString root = QStringLiteral(SOURCE_ROOT_DIR);
    for (const char *dir : { "ui/src", "webaccess/src", "engine/src" })
    {
        QDirIterator it(root + QLatin1Char('/') + QLatin1String(dir),
                        { QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.ui") },
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString path = it.next();
            list << (path.endsWith(QLatin1String(".ui")) ? readUi(root, path) : readCpp(root, path));
        }
    }
    return list;
}

/* A length unit written into a user string: "%1 m", "2.5 ft", " mm" (a
   suffix), "(m)" (a column), "metres", "feet". Case matters: "M" is not
   metres. */
bool namesALengthUnit(const QString &s)
{
    static const QList<QRegularExpression> res = {
        QRegularExpression(QStringLiteral("(?:%L?\\d+|\\d)\\s?(?:mm|cm|m|ft)(?![A-Za-z0-9²³])")),
        QRegularExpression(QStringLiteral("^\\s?(?:mm|cm|m|ft)$")),
        QRegularExpression(QStringLiteral("\\b(?:[Mm]etres?|[Mm]eters|[Ff]eet)\\b")),
        QRegularExpression(QStringLiteral("\\((?:mm|cm|m|ft|in)\\)")),
    };
    for (const QRegularExpression &re : res)
        if (re.match(s).hasMatch())
            return true;
    return false;
}

/* Files whose strings name units because that is their job, each with why. */
const QHash<QString, QString> &unitFiles()
{
    static const QHash<QString, QString> files = {
        { QStringLiteral("engine/src/measurementunits.cpp"),
          QStringLiteral("the units' own names, symbols and parsing") },
        { QStringLiteral("engine/src/measurementunits.h"),
          QStringLiteral("documents what MeasurementUnits reads and writes") },
        { QStringLiteral("engine/src/function.cpp"),
          QStringLiteral("\"m\" is minutes in a speed (1m30s), not metres") },
        { QStringLiteral("engine/src/scriptv4.cpp"),
          QStringLiteral("\"m\" is minutes in a script's time, not metres") },
        { QStringLiteral("ui/src/virtualconsole/vcclockproperties.ui"),
          QStringLiteral("the countdown's \"m\" suffix is minutes") },
        { QStringLiteral("engine/src/qlcfixturedefcache.cpp"),
          QStringLiteral("\"m\" is the model attribute of a definition file") },
    };
    return files;
}

/* Where the show grid's unit may be read, each with why and how many
   times: the grid's own size and spacing, never a displayed length. */
struct GridUse
{
    int count;
    const char *why;
};

const QHash<QString, GridUse> &gridFiles()
{
    static const QHash<QString, GridUse> files = {
        { QStringLiteral("engine/src/monitorproperties.h"),
          { 1, "declares it: the unit the grid's size is kept in" } },
        { QStringLiteral("engine/src/monitorproperties.cpp"),
          { 4, "the grid's own conversions (load, save, convertGridUnits, a new view's size)" } },
        { QStringLiteral("ui/src/app.cpp"),
          { 1, "a never-set preference seeded from an older show's grid unit, once" } },
        { QStringLiteral("ui/src/monitor/monitor.cpp"),
          { 1, "the plot's grid metrics: a cell is a metre or a foot" } },
        { QStringLiteral("ui/src/monitor/studioplaneview.cpp"),
          { 1, "the grid lines' spacing: feet steps or metre steps" } },
        { QStringLiteral("ui/src/monitor/structurestudioview.cpp"),
          { 1, "the floor drawn to the grid's declared cells" } },
    };
    return files;
}

/****************************************************************************
 * The dialogs
 ****************************************************************************/

/* A metric length in text: "1.8 m", "190 mm", "metres" */
bool showsMetric(const QString &t)
{
    static const QRegularExpression re(QStringLiteral(
        "\\d\\s?(?:mm|cm|m)\\b|\\b[Mm]et(?:res?|ers)\\b"));
    return re.match(t).hasMatch();
}

/* An imperial length in text: "6' 3\"", "12 ft", "feet" */
bool showsImperial(const QString &t)
{
    static const QRegularExpression re(QStringLiteral(
        "\\d\\s?(?:ft|in)\\b(?!\\s+(?:the|a|an|this|that|its|his|her)\\b)|\\d['\"′″]|\\b(?:[Ff]eet|[Ff]oot|[Ii]nch(?:es)?)\\b"));
    return re.match(t).hasMatch();
}

/* What the help of a length field says: every unit it takes, on purpose */
bool isLengthHelp(const QString &tip)
{
    for (MU::LengthUnit u : { MU::Millimetres, MU::Centimetres, MU::Metres, MU::Inches, MU::Feet })
        if (tip == MU::lengthHelp(u))
            return true;
    return false;
}

struct Seen
{
    QStringList texts;      ///< "where: text"
    int lengthFields = 0;
    QStringList plainLengthSpins;
};

QString where(const QWidget *w)
{
    return w->objectName().isEmpty() ? QString::fromLatin1(w->metaObject()->className()) : w->objectName();
}

/* Every word a person can read in @p top, and its length fields */
Seen look(QWidget *top)
{
    Seen s;
    auto add = [&s](const QWidget *w, const QString &text) {
        if (!text.trimmed().isEmpty())
            s.texts << QStringLiteral("%1: %2").arg(where(w), text);
    };
    add(top, top->windowTitle());
    QList<QWidget *> all = top->findChildren<QWidget *>();
    all.prepend(top);
    for (QWidget *w : all)
    {
        if (w != top && !w->isVisibleTo(top))
            continue;
        if (!w->toolTip().isEmpty() && !isLengthHelp(w->toolTip()))
            add(w, w->toolTip());
        if (auto *l = qobject_cast<QLabel *>(w)) add(w, l->text());
        else if (auto *b = qobject_cast<QAbstractButton *>(w)) add(w, b->text());
        else if (auto *g = qobject_cast<QGroupBox *>(w)) add(w, g->title());
        else if (auto *c = qobject_cast<QComboBox *>(w))
            for (int i = 0; i < c->count(); ++i)
                add(w, c->itemText(i));
        else if (auto *lw = qobject_cast<QListWidget *>(w))
            for (int i = 0; i < lw->count(); ++i)
                add(w, lw->item(i)->text());
        else if (auto *tw = qobject_cast<QTableWidget *>(w))
        {
            for (int c = 0; c < tw->columnCount(); ++c)
                if (tw->horizontalHeaderItem(c) != nullptr)
                    add(w, tw->horizontalHeaderItem(c)->text());
            for (int r = 0; r < tw->rowCount(); ++r)
                for (int c = 0; c < tw->columnCount(); ++c)
                    if (tw->item(r, c) != nullptr)
                        add(w, tw->item(r, c)->text());
        }
        else if (auto *tr = qobject_cast<QTreeWidget *>(w))
        {
            QTreeWidgetItemIterator it(tr);
            for (; *it; ++it)
                for (int c = 0; c < tr->columnCount(); ++c)
                    add(w, (*it)->text(c));
        }
        if (auto *sb = qobject_cast<QAbstractSpinBox *>(w))
        {
            add(w, sb->text());
            const bool length = dynamic_cast<LengthSpinBox *>(w) != nullptr;
            if (length)
                s.lengthFields++;
            else if (auto *d = qobject_cast<QDoubleSpinBox *>(w))
            {
                // A plain number with a length unit on it: not a length field.
                if (namesALengthUnit(d->suffix()) || namesALengthUnit(d->text()))
                    s.plainLengthSpins << where(w);
            }
        }
        else if (auto *e = qobject_cast<QLineEdit *>(w))
        {
            if (qobject_cast<QAbstractSpinBox *>(e->parentWidget()) == nullptr)
                add(w, e->text());
            if (e->inherits("MeasurementEdit"))
                s.lengthFields++;
        }
    }
    return s;
}

/** A dialog to open, and whether it has a length field to type into */
struct Opener
{
    QString name;
    bool hasLengthField;
    /** Opens it. A modal one blocks; look() runs on it from inside. */
    std::function<void(QWidget *parentWindow)> open;
    /** A non-modal one: built and returned instead (open is empty) */
    std::function<QWidget *()> build;
};

/* The show the dialogs look at: one of everything with a length. */
struct Rig
{
    Doc *doc = nullptr;
    quint32 fid = 0, trussId = 0, towerId = 0, platformId = 0, pipeId = 0, standId = 0, targetId = 0;
};

void buildRig(Rig &rig)
{
    Doc *doc = rig.doc;
    MonitorProperties *props = doc->monitorProperties();
    props->setGridSize(QVector3D(12, 8, 10));
    Truss *t = props->addTruss();
    t->setName(QStringLiteral("FOH"));
    t->setLength(6.0f);
    t->setOrigin(QVector3D(2.0f, 3.0f, 5.5f));
    rig.trussId = t->id();
    Tower *tw = props->addTower();
    tw->setName(QStringLiteral("Tower A"));
    tw->setOriginX(8.0f);
    tw->setOriginY(4.0f);
    tw->setHeight(3.5f);
    tw->setShelves({ 1.2f, 2.4f });
    rig.towerId = tw->id();
    StagePlatform *deck = props->addPlatform();
    deck->setName(QStringLiteral("Riser"));
    deck->setOriginX(1.0f); deck->setOriginY(1.0f);
    deck->setWidth(2.4f); deck->setDepth(1.2f); deck->setHeight(0.4f);
    rig.platformId = deck->id();
    Pipe *p = props->addPipe();
    p->setName(QStringLiteral("Boom"));
    p->setHeight(3.0f);
    rig.pipeId = p->id();
    Stand *st = props->addStand();
    st->setName(QStringLiteral("Stand"));
    st->setHeight(2.5f);
    rig.standId = st->id();
    StageTarget *tg = props->addStageTarget();
    tg->setName(QStringLiteral("DSC"));
    tg->setZ(1.5f);
    rig.targetId = tg->id();
    Fixture *fx = new Fixture(doc);
    fx->setName(QStringLiteral("Par 1"));
    fx->setChannels(3);
    doc->addFixture(fx);
    rig.fid = fx->id();
    props->setFixturePosition(fx->id(), 0, 0, QVector3D(3000.0f, 2000.0f, 3048.0f));
    NetworkNode node;
    node.name = QStringLiteral("Wireless TX");
    node.transport = NetworkNode::WirelessDMX;
    NetworkPort port;
    port.universe = 0;
    port.receiver.placed = true;
    port.receiver.posX = 4.0;
    port.receiver.posY = 2.5;
    node.ports << port;
    doc->networkTopology()->nodes() << node;
}

} // namespace

/****************************************************************************
 * Setup
 ****************************************************************************/

void Units_Test::initTestCase()
{
    /* A settings file of its own: the preference is what this test sets,
       never the developer's. */
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QDir::temp().filePath(QStringLiteral("units_test-settings")));
    QSettings(QSettings::defaultFormat(), QSettings::UserScope,
              QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole")).remove(QStringLiteral("units"));
}

void Units_Test::cleanupTestCase()
{
    QSettings(QSettings::defaultFormat(), QSettings::UserScope,
              QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole")).remove(QStringLiteral("units"));
}

/****************************************************************************
 * The sources
 ****************************************************************************/

void Units_Test::noUnitWrittenIntoAString()
{
    QStringList bad;
    for (const Source &src : sources())
    {
        if (unitFiles().contains(src.file))
            continue;
        for (const Literal &l : src.literals)
            if (namesALengthUnit(l.text) && !markedOk(src, l.line))
                bad << QStringLiteral("%1:%2: \"%3\"").arg(l.file).arg(l.line).arg(l.text);
    }
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral(
        "A length unit written into a string. Show it with MeasurementUnits::formatLength() / "
        "formatMetres() or MonitorProperties::formatStageDistance(), and type it in a "
        "LengthSpinBox / MeasurementEdit, so it follows Settings > Units (a unit that is not a "
        "length measurement: say why with \"units-ok:\" on the line):\n  ")
        + bad.join(QStringLiteral("\n  "))));
}

void Units_Test::noBareNumberDialogForALength()
{
    QStringList bad;
    for (const Source &src : sources())
        for (int i = 0; i < src.code.size(); ++i)
            if (src.code.at(i).contains(QLatin1String("QInputDialog::getDouble")) && !markedOk(src, i + 1))
                bad << QStringLiteral("%1:%2").arg(src.file).arg(i + 1);
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral(
        "QInputDialog::getDouble: a number with no unit. For a length use "
        "LengthInputDialog::getMillimetres() / getMetres(); for anything else say what it is "
        "with \"units-ok:\" on the line:\n  ") + bad.join(QStringLiteral("\n  "))));
}

void Units_Test::gridUnitsOnlyForTheGrid()
{
    QStringList bad;
    QHash<QString, int> counts;
    static const QRegularExpression re(QStringLiteral("\\bgridUnits\\(\\)"));
    for (const Source &src : sources())
        for (int i = 0; i < src.code.size(); ++i)
        {
            QRegularExpressionMatchIterator it = re.globalMatch(src.code.at(i));
            while (it.hasNext())
            {
                it.next();
                counts[src.file]++;
                if (!gridFiles().contains(src.file))
                    bad << QStringLiteral("%1:%2").arg(src.file).arg(i + 1);
            }
        }
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        if (gridFiles().contains(it.key()) && it.value() > gridFiles().value(it.key()).count)
            bad << QStringLiteral("%1: %2 uses, %3 allowed (%4)").arg(it.key()).arg(it.value())
                       .arg(gridFiles().value(it.key()).count).arg(gridFiles().value(it.key()).why);
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral(
        "MonitorProperties::gridUnits() is the grid's spacing only, never the unit a length is "
        "shown or typed in: that is MeasurementUnits::preferredLength():\n  ")
        + bad.join(QStringLiteral("\n  "))));
}

/****************************************************************************
 * The default and the formatters
 ****************************************************************************/

void Units_Test::feetAndInchesByDefault()
{
    QSettings(QSettings::defaultFormat(), QSettings::UserScope,
              QLCFile::settingsOrganisation(), QStringLiteral("qlcconsole")).remove(QStringLiteral("units"));
    QVERIFY(!MU::isPreferenceSet());
    QCOMPARE(MU::preferredLength(), MU::Feet);
    QCOMPARE(MU::formatLength(1905.0), QString("6' 3\""));
    // A metric first show leaves it unset: still feet and inches.
    QVERIFY(!MU::seedFromFirstShow(false));
    QCOMPARE(MU::preferredLength(), MU::Feet);
}

void Units_Test::textFollowsThePreference_data()
{
    QTest::addColumn<int>("unit");
    QTest::newRow("feet") << int(MU::Feet);
    QTest::newRow("inches") << int(MU::Inches);
    QTest::newRow("millimetres") << int(MU::Millimetres);
    QTest::newRow("centimetres") << int(MU::Centimetres);
    QTest::newRow("metres") << int(MU::Metres);
}

void Units_Test::textFollowsThePreference()
{
    QFETCH(int, unit);
    const MU::LengthUnit u = MU::LengthUnit(unit);
    MU::setPreferredLength(u);
    const bool imperial = MU::isImperial(u);
    auto check = [imperial](const QString &what, const QString &text) {
        QVERIFY2(imperial ? !showsMetric(text) : !showsImperial(text),
                 qPrintable(QStringLiteral("%1: %2").arg(what, text)));
        QVERIFY2(text.contains(MU::symbol(MU::preferredLength()))
                 || (MU::preferredLength() == MU::Feet && (text.contains('\'') || text.contains('"'))),
                 qPrintable(QStringLiteral("%1 not in %2: %3").arg(what, MU::symbol(MU::preferredLength()), text)));
    };

    Doc doc(nullptr);
    Rig rig;
    rig.doc = &doc;
    buildRig(rig);
    MonitorProperties *props = doc.monitorProperties();
    // A grid that disagrees with the preference changes nothing shown.
    props->setGridUnits(imperial ? MonitorProperties::Meters : MonitorProperties::Feet);

    check("formatStageDistance", MonitorProperties::formatStageDistance(1.8f));
    check("curtain line offset", MonitorProperties::formatCurtainLineOffset(1.5f));
    check("stage position", props->stagePositionText(QVector3D(2.0f, 1.0f, 1.5f)));
    check("mount description", props->mountDescription(rig.fid).isEmpty()
                                   ? MU::formatMetres(1.0) : MU::formatMetres(1.0));
    check("mounting height menu", MonitorFixtureItem::mountingHeightText(props, rig.fid));
    QCOMPARE(MonitorFixtureItem::mountingHeightText(props, rig.fid),
             QStringLiteral("Set Mounting Height… (%1)").arg(MU::formatLength(3048.0)));
    if (u == MU::Feet)
        QCOMPARE(MonitorFixtureItem::mountingHeightText(props, rig.fid), QString("Set Mounting Height… (10')"));

    // Reports and their CSV: the column names the preferred unit.
    const NetworkReport report = NetworkReport::build(&doc);
    QVERIFY2(report.toCsv().contains(QStringLiteral("Run (%1)").arg(MU::symbol(u))), qPrintable(report.toCsv()));
    QVERIFY(imperial ? !showsMetric(report.toText()) : !showsImperial(report.toText()));
}

/****************************************************************************
 * The dialogs
 ****************************************************************************/

void Units_Test::dialogsFollowThePreference_data()
{
    QTest::addColumn<int>("unit");
    QTest::newRow("feet") << int(MU::Feet);
    QTest::newRow("millimetres") << int(MU::Millimetres);
}

void Units_Test::dialogsFollowThePreference()
{
    QFETCH(int, unit);
    const MU::LengthUnit u = MU::LengthUnit(unit);
    MU::setPreferredLength(u);
    const bool imperial = MU::isImperial(u);
    QElapsedTimer clock;
    clock.start();

    Doc *doc = new Doc(this);
    auto dropDoc = qScopeGuard([doc]() { delete doc; });
    Rig rig;
    rig.doc = doc;
    // The grid follows the preference, as App::applyUnitPreference() does.
    doc->monitorProperties()->setGridUnits(MonitorProperties::gridUnitsFor(u));
    buildRig(rig);
    MonitorProperties *props = doc->monitorProperties();

    QWidget window;
    window.resize(1280, 800);
    Monitor *mon = new Monitor(&window, doc);
    const QString shotDir = qEnvironmentVariable("QLC_SHOT_DIR");

    const QList<Opener> openers = {
        { QStringLiteral("Mounting Height"), true, [props, &rig](QWidget *w) {
              MonitorFixtureItem::askMountingHeight(w, props, rig.fid); }, {} },
        { QStringLiteral("Truss"), true, [mon, &rig](QWidget *) { mon->slotEditTruss(rig.trussId); }, {} },
        { QStringLiteral("Tower"), true, [mon, &rig](QWidget *) { mon->slotEditTower(rig.towerId); }, {} },
        { QStringLiteral("Platform"), true, [mon, &rig](QWidget *) { mon->slotEditPlatform(rig.platformId); }, {} },
        { QStringLiteral("Pipe"), true, [mon, &rig](QWidget *) { mon->slotEditPipe(rig.pipeId); }, {} },
        { QStringLiteral("Stand"), true, [mon, &rig](QWidget *) { mon->slotEditStand(rig.standId); }, {} },
        { QStringLiteral("Target"), true, [mon, &rig](QWidget *) { mon->slotEditTarget(rig.targetId); }, {} },
        { QStringLiteral("Fixture Properties"), true, [mon, &rig](QWidget *) { mon->editFixtureProperties(rig.fid); }, {} },
        { QStringLiteral("Curtain Line"), true, [mon](QWidget *) {
              mon->editStagePiece(MonitorProperties::CurtainPiece); }, {} },
        { QStringLiteral("Apron"), true, [mon](QWidget *) { mon->editStagePiece(MonitorProperties::ApronPiece); }, {} },
        { QStringLiteral("Network Topology"), true, {}, [doc]() -> QWidget * {
              return new NetworkTopologyDialog(doc, 0); } },
        { QStringLiteral("Network Report"), false, {}, [doc]() -> QWidget * { return new NetworkReportDialog(doc); } },
        { QStringLiteral("Fill with People"), true, {}, [props, &rig]() -> QWidget * {
              return new PerformersDialog(props, { rig.platformId }); } },
        { QStringLiteral("Add RGB Panel"), true, {}, [doc]() -> QWidget * { return new AddRGBPanel(nullptr, doc); } },
    };

    QStringList problems;
    auto inspect = [&](const Opener &o, QWidget *dlg) {
        const Seen seen = look(dlg);
        for (const QString &t : seen.texts)
            if (imperial ? showsMetric(t) : showsImperial(t))
                problems << QStringLiteral("%1 — %2").arg(o.name, t);
        for (const QString &s : seen.plainLengthSpins)
            problems << QStringLiteral("%1 — %2 is a plain number with a unit, not a length field").arg(o.name, s);
        if (o.hasLengthField && seen.lengthFields == 0)
            problems << QStringLiteral("%1 — no LengthSpinBox / MeasurementEdit").arg(o.name);
        if (!shotDir.isEmpty())
        {
            const QSize was = dlg->size();
            for (const QSize &sz : { QSize(1024, 640), QSize(1280, 800), QSize(1920, 1080) })
            {
                dlg->resize(qMin(sz.width(), qMax(was.width(), dlg->minimumSizeHint().width())),
                            qMin(sz.height(), qMax(was.height(), dlg->minimumSizeHint().height())));
                QApplication::processEvents();
                QPixmap canvas(sz);
                canvas.fill(QColor(40, 40, 44));
                QPainter p(&canvas);
                p.drawPixmap(QPoint(qMax(0, (sz.width() - dlg->width()) / 2),
                                    qMax(0, (sz.height() - dlg->height()) / 2)), dlg->grab());
                p.end();
                QString n = o.name.toLower();
                n.replace(QLatin1Char(' '), QLatin1Char('-'));
                canvas.save(QDir(shotDir).filePath(QStringLiteral("%1-%2-%3x%4.png")
                                                       .arg(n, MU::key(u)).arg(sz.width()).arg(sz.height())));
            }
            dlg->resize(was);
        }
    };

    for (const Opener &o : openers)
    {
        if (o.build)
        {
            QScopedPointer<QWidget> dlg(o.build());
            dlg->show();
            QApplication::processEvents();
            inspect(o, dlg.data());
            continue;
        }
        bool seenIt = false;
        QPointer<QTimer> poll = new QTimer(this);
        poll->setInterval(5);
        int tries = 0;
        connect(poll.data(), &QTimer::timeout, this, [&]() {
            QDialog *dlg = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dlg == nullptr)
            {
                if (++tries > 200 && poll) poll->stop();
                return;
            }
            poll->stop();
            seenIt = true;
            inspect(o, dlg);
            dlg->reject();
        });
        poll->start();
        o.open(&window);
        if (poll)
        {
            poll->stop();
            delete poll.data();
        }
        if (!seenIt)
            problems << QStringLiteral("%1 — did not open").arg(o.name);
    }

    QVERIFY2(problems.isEmpty(), qPrintable(QStringLiteral("With Settings > Units in %1:\n  ")
                                                .arg(MU::name(u)) + problems.join(QStringLiteral("\n  "))));
    qDebug() << MU::name(u) << "dialogs looked at in" << clock.elapsed() << "ms";
}

QTEST_MAIN(Units_Test)
