/*
  Q Light Controller Plus
  showfiles_test.cpp

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

#include <QtTest>
#include <QApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>
#include <algorithm>

#include "showfiles_test.h"

#define private public
#define protected public
#include "app.h"
#include "fixturemanager.h"
#include "monitor.h"
#undef protected
#undef private
#include "fixturetreewidget.h"
#include "monitorgraphicsview.h"
#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "channelsgroup.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcpalette.h"
#include "function.h"
#include "scene.h"
#include "chaser.h"
#include "collection.h"
#include "universe.h"
#include "inputoutputmap.h"
#include "monitorproperties.h"
#include "truss.h"
#include "tower.h"
#include "showtreecontroller.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "shutterautoopen.h"
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcwidget.h"

/* Close whatever modal opens while a show loads or saves (a missing
   definition's question, an old-show notice): a test must never wait for a
   click. Each one closed is remembered, and said, so a test can see it. */
static QStringList g_closed;

static void closeModalsFor(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        QWidget *w = QApplication::activeModalWidget();
        // The load's own progress is not a question.
        if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
        {
            QString what = w->metaObject()->className();
            if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
                what += QStringLiteral(": ") + box->text();
            g_closed << what;
            qWarning() << "closed a modal while a show opened:" << what;
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
            else
                w->close();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

/* A watchdog for the length of one call: any modal it opens is closed. */
class ModalCloser
{
public:
    ModalCloser()
    {
        m_timer.setInterval(50);
        QObject::connect(&m_timer, &QTimer::timeout, []() {
            QWidget *w = QApplication::activeModalWidget();
            if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
            {
                QString what = w->metaObject()->className();
                if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
                    what += QStringLiteral(": ") + box->text();
                g_closed << what;
                qWarning() << "closed a modal while a show opened:" << what;
                if (QDialog *d = qobject_cast<QDialog *>(w))
                    d->reject();
                else
                    w->close();
            }
        });
        m_timer.start();
    }
private:
    QTimer m_timer;
};

static QByteArray fileBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

/* Every file in a folder (recursively) with its hash: what is there. */
static QStringList folderState(const QString &dir)
{
    QStringList out;
    QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString p = it.next();
        out << QDir(dir).relativeFilePath(p) + QStringLiteral(" ")
               + QString::fromLatin1(QCryptographicHash::hash(fileBytes(p),
                                                              QCryptographicHash::Sha1).toHex());
    }
    out.sort();
    return out;
}

/* The shows every build has, or every one found here. */
static QStringList showFiles()
{
    QStringList out;
    QDir dir(QStringLiteral(SHOWFILES_WORKSPACES));
    for (const QString &name : dir.entryList({ QStringLiteral("*.qxw"), QStringLiteral("*.qlcc") },
                                             QDir::Files, QDir::Name))
        out << dir.absoluteFilePath(name);
    out << QStringLiteral(SHOWFILES_SAMPLE);
    return out;
}

static bool savedByQlcconsole(const QString &path)
{
    const QByteArray head = fileBytes(path).left(4096);
    const int a = head.indexOf("<Name>");
    const int b = head.indexOf("</Name>", a);
    if (a < 0 || b < 0)
        return false;
    return ShutterAutoOpen::savedByQlcconsole(QString::fromUtf8(head.mid(a + 6, b - a - 6)));
}

void ShowFiles_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
}

void ShowFiles_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void ShowFiles_Test::cleanup()
{
    ModalCloser closer;
    m_app->clearDocument();
    closeModalsFor(50);
}

void ShowFiles_Test::addShowRows(bool stockOnly)
{
    QTest::addColumn<QString>("path");
    int rows = 0;
    for (const QString &path : showFiles())
    {
        if (stockOnly && savedByQlcconsole(path))
            continue;
        QTest::newRow(qPrintable(QFileInfo(path).fileName())) << path;
        ++rows;
    }
    QVERIFY2(rows > 0, "no show files found");
}

/* What a show is, in lines: a difference names what changed. */
QStringList ShowFiles_Test::fingerprint() const
{
    Doc *doc = m_app->doc();
    QStringList out;

    for (Fixture *fx : doc->fixtures())
    {
        const QLCFixtureDef *def = fx->fixtureDef();
        const QLCFixtureMode *mode = fx->fixtureMode();
        out << QStringLiteral("fixture %1 '%2' %3/%4/%5 u%6 a%7 ch%8 look%9 rules%10")
                   .arg(fx->id()).arg(fx->name())
                   .arg(def ? def->manufacturer() : QString())
                   .arg(def ? def->model() : QString())
                   .arg(mode ? mode->name() : QString())
                   .arg(fx->universe()).arg(fx->address()).arg(fx->channels())
                   .arg(int(fx->stageLook())).arg(int(fx->outputRules()));
    }
    for (FixtureGroup *g : doc->fixtureGroups())
        out << QStringLiteral("group %1 '%2' path '%3' heads %4 size %5x%6")
                   .arg(g->id()).arg(g->name()).arg(g->path())
                   .arg(g->headList().size()).arg(g->size().width()).arg(g->size().height());
    for (ChannelsGroup *g : doc->channelsGroups())
        out << QStringLiteral("channels group %1 '%2' %3")
                   .arg(g->id()).arg(g->name()).arg(g->getChannels().size());
    for (QLCPalette *p : doc->palettes())
        out << QStringLiteral("palette %1 '%2' type %3 values %4")
                   .arg(p->id()).arg(p->name()).arg(int(p->type())).arg(p->values().size());
    for (Function *f : doc->functions())
    {
        QString line = QStringLiteral("function %1 %2 '%3' path '%4'")
                           .arg(f->id()).arg(f->typeString()).arg(f->name()).arg(f->path(true));
        if (Scene *s = qobject_cast<Scene *>(f))
            line += QStringLiteral(" values %1").arg(s->values().size());
        else if (Chaser *c = qobject_cast<Chaser *>(f))
            line += QStringLiteral(" steps %1").arg(c->stepsCount());
        else if (Collection *c = qobject_cast<Collection *>(f))
        {
            QStringList ids;
            for (quint32 id : c->functions())
                ids << QString::number(id);
            line += QStringLiteral(" members ") + ids.join(',');
        }
        out << line;
    }
    for (Universe *u : doc->inputOutputMap()->universes())
        out << QStringLiteral("universe %1 '%2' passthrough %3")
                   .arg(u->id()).arg(u->name()).arg(u->passthrough());

    MonitorProperties *mp = doc->monitorProperties();
    out << QStringLiteral("plot fixtures %1 trusses %2 decks %3 targets %4 towers %5 scenery %6")
               .arg(mp->fixtureItemsID().size()).arg(mp->trusses().size())
               .arg(mp->platforms().size()).arg(mp->stageTargets().size())
               .arg(mp->towers().size()).arg(mp->sceneries().size());

    if (ShowTreeController *tree = doc->showTree())
        out << QStringLiteral("show tree productions %1 group sets %2")
                   .arg(tree->productions().size()).arg(tree->groupSets().size());

    QStringList widgets;
    if (VirtualConsole::instance() != nullptr && VirtualConsole::instance()->contents() != nullptr)
        for (VCWidget *w : VirtualConsole::instance()->contents()->findChildren<VCWidget *>())
            widgets << QStringLiteral("vc %1 %2 '%3'").arg(w->id()).arg(w->type()).arg(w->caption());
    widgets.sort();
    out << widgets;

    out << QStringLiteral("auto-open shutters %1").arg(doc->shutterAutoOpen()->isEnabled());
    out << QStringLiteral("view %1").arg(int(m_app->workspaceView()));
    return out;
}

static void compareLines(const QStringList &got, const QStringList &want)
{
    QStringList lost, gained;
    for (const QString &l : want)
        if (!got.contains(l))
            lost << l;
    for (const QString &l : got)
        if (!want.contains(l))
            gained << l;
    const int shown = 12;
    QVERIFY2(lost.isEmpty() && gained.isEmpty() && got.size() == want.size(),
             qPrintable(QStringLiteral("lost %1: %2\ngained %3: %4")
                            .arg(lost.size()).arg(lost.mid(0, shown).join(QStringLiteral("\n  ")))
                            .arg(gained.size()).arg(gained.mid(0, shown).join(QStringLiteral("\n  ")))));
}

static QFile::FileError open(App *app, const QString &path)
{
    ModalCloser closer;
    app->clearDocument();
    const QFile::FileError e = app->loadXML(path);
    closeModalsFor(30);         // and the venue question queued after it
    return e;
}

static QFile::FileError save(App *app, const QString &path)
{
    ModalCloser closer;
    const QFile::FileError e = app->saveXML(path);
    closeModalsFor(10);
    return e;
}

void ShowFiles_Test::aShowSavedAndOpenedAgainIsTheSame_data()
{
    addShowRows(false);
}

void ShowFiles_Test::aShowSavedAndOpenedAgainIsTheSame()
{
    QFETCH(QString, path);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ext = QFileInfo(path).suffix();
    const QString copy = dir.filePath(QStringLiteral("original.") + ext);
    QVERIFY(QFile::copy(path, copy));

    QCOMPARE(open(m_app, copy), QFile::NoError);
    // A whole show is not taken for a damaged one.
    QVERIFY(m_app->damagedFile().isEmpty());
    const QStringList first = fingerprint();
    QVERIFY2(first.size() > 3, "the show opened empty");

    /* A QLC+ 4.x show opens with Auto-open shutters off, so it sends the
       same DMX here as in QLC+; saved here, the setting is in the file and
       it opens the same (aStockShowOpensWithAutoOpenOff, for every stock
       show without opening each again). */
    const bool stock = !savedByQlcconsole(path) && !fileBytes(path).contains("<ShutterAutoOpen");
    if (stock)
        QVERIFY2(!m_app->doc()->shutterAutoOpen()->isEnabled(),
                 "a QLC+ show must send the DMX it sends in QLC+");

    const QString a = dir.filePath(QStringLiteral("saved-a.") + ext);
    QCOMPARE(save(m_app, a), QFile::NoError);
    if (stock)
        QVERIFY(fileBytes(a).contains("<ShutterAutoOpen"));

    QCOMPARE(open(m_app, a), QFile::NoError);
    compareLines(fingerprint(), first);
    if (QTest::currentTestFailed())
        return;
    if (stock)
        QVERIFY(!m_app->doc()->shutterAutoOpen()->isEnabled());

    // Saved again, it is the same file: nothing drifts each time it's saved.
    const QString b = dir.filePath(QStringLiteral("saved-b.") + ext);
    QCOMPARE(save(m_app, b), QFile::NoError);
    QString A = QString::fromUtf8(fileBytes(a));
    QString B = QString::fromUtf8(fileBytes(b));
    QVERIFY(!A.isEmpty());
    if (A != B)
    {
        const QStringList la = A.split('\n'), lb = B.split('\n');
        int i = 0;
        while (i < la.size() && i < lb.size() && la.at(i) == lb.at(i))
            ++i;
        QFAIL(qPrintable(QStringLiteral("the second save differs at line %1:\n  was: %2\n  now: %3")
                             .arg(i + 1).arg(la.value(i)).arg(lb.value(i))));
    }
}

void ShowFiles_Test::aShowFromANewerBuildOpens_data()
{
    QTest::addColumn<QString>("path");
    // A stock QLC+ 4 show, the old sample, and a qlcconsole 0.1 show.
    QTest::newRow("help-demo.qxw") << QStringLiteral(SHOWFILES_WORKSPACES) + QStringLiteral("help-demo.qxw");
    QTest::newRow("Sample.qxw") << QStringLiteral(SHOWFILES_SAMPLE);
    QTest::newRow("DanceHysteria-0.1.qlcc")
        << QStringLiteral(SHOWFILES_WORKSPACES) + QStringLiteral("DanceHysteria-0.1.qlcc");
}

/* Put @p what after the opening tag of the first @p n elements named @p tag
   that have content (not <tag/>). How many it went into. */
static int insertInto(QString &xml, const QString &tag, const QString &what, int n)
{
    int done = 0;
    int from = 0;
    while (done < n)
    {
        const int at = xml.indexOf(QStringLiteral("<") + tag, from);
        if (at < 0)
            break;
        const QChar next = xml.at(at + tag.size() + 1);
        const int close = xml.indexOf('>', at);
        from = close + 1;
        if ((next != ' ' && next != '>') || close < 0 || xml.at(close - 1) == '/')
            continue;
        xml.insert(close + 1, what);
        from += what.size();
        ++done;
    }
    return done;
}

void ShowFiles_Test::aShowFromANewerBuildOpens()
{
    QFETCH(QString, path);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ext = QFileInfo(path).suffix();
    const QString copy = dir.filePath(QStringLiteral("original.") + ext);
    QVERIFY(QFile::copy(path, copy));

    // What this build makes of it, saved by this build.
    QCOMPARE(open(m_app, copy), QFile::NoError);
    const QString saved = dir.filePath(QStringLiteral("saved.") + ext);
    QCOMPARE(save(m_app, saved), QFile::NoError);
    QCOMPARE(open(m_app, saved), QFile::NoError);
    const QStringList known = fingerprint();

    /* The same file as a later build might write it: new elements in
       elements that hold elements (not in text ones such as a chaser's or
       a collection's <Step>id</Step>, which no reader could take). */
    QString xml = QString::fromUtf8(fileBytes(saved));
    const QString future = QStringLiteral("<FromTheFuture Kind=\"x\"><Nested A=\"1\">text</Nested><Empty/></FromTheFuture>");
    int places = 0;
    for (const QString &tag : { QStringLiteral("Workspace"), QStringLiteral("Engine"),
                                QStringLiteral("InputOutputMap"), QStringLiteral("Fixture"),
                                QStringLiteral("FixtureGroup"), QStringLiteral("Function"),
                                QStringLiteral("Monitor"),
                                QStringLiteral("VirtualConsole"), QStringLiteral("Frame"),
                                QStringLiteral("Button"), QStringLiteral("Slider"),
                                QStringLiteral("SimpleDesk"), QStringLiteral("ShowTree") })
        places += insertInto(xml, tag, future, 3);
    QVERIFY2(places > 10, qPrintable(QString::number(places)));
    // An attribute it doesn't know, on every element that has attributes.
    xml.replace(QRegularExpression(QStringLiteral("<([A-Za-z]+) ([A-Za-z]+=\")")),
                QStringLiteral("<\\1 FutureAttr=\"7\" \\2"));
    const QString newer = dir.filePath(QStringLiteral("newer.") + ext);
    QFile f(newer);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(xml.toUtf8());
    f.close();

    QCOMPARE(open(m_app, newer), QFile::NoError);
    compareLines(fingerprint(), known);
}

void ShowFiles_Test::openingAShowNeverWritesIt_data()
{
    addShowRows(false);
}

void ShowFiles_Test::openingAShowNeverWritesIt()
{
    QFETCH(QString, path);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QFileInfo(path).fileName());
    QVERIFY(QFile::copy(path, copy));
    QFile::setPermissions(copy, QFile::ReadOwner | QFile::WriteOwner);
    /* The App's own crash-recovery autosave (<show>.autosave, every five
       minutes from App::init) is not the show being written: on a slow
       host this suite outlasts five minutes and the timer can land while
       a show is open (val, 2026-10-26). Everything else must be as it was. */
    const QString autosave = QFileInfo(copy).fileName() + QStringLiteral(".autosave");
    auto state = [&dir, &autosave]() {
        QStringList out;
        for (const QString &line : folderState(dir.path()))
            if (!line.startsWith(autosave))
                out << line;
        return out;
    };
    const QStringList before = state();

    QCOMPARE(open(m_app, copy), QFile::NoError);
    QCoreApplication::processEvents();
    /* Opened and not touched, it is not modified: closing it asks nothing
       and no autosave is written (aShowOpensUnmodified, folded in here so
       each show is opened once for both). */
    QVERIFY2(!m_app->doc()->isModified(), "opened, not changed: nothing to save");
    // Looked at, and closed without saving.
    m_app->clearDocument();
    QCoreApplication::processEvents();

    const QStringList after = state();
    QVERIFY2(after == before, qPrintable(QStringLiteral("before:\n%1\nafter:\n%2")
                                         .arg(before.join('\n'), after.join('\n'))));
}

void ShowFiles_Test::aDamagedShowOpensAndIsNotSavedOver_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<double>("kept");
    const QStringList shows = {
        QStringLiteral(SHOWFILES_WORKSPACES "/help-demo.qxw"),
        QStringLiteral(SHOWFILES_WORKSPACES "/stage-structures-demo.qxw"),
        QStringLiteral(SHOWFILES_WORKSPACES "/DanceHysteria-0.1.qlcc"),
        QStringLiteral(SHOWFILES_SAMPLE),
    };
    /* One cut per show, the cut points taken in turn, so every show and
       every point is covered without opening each show three times (the
       suite ran past CI Windows' five minutes with all twelve). */
    const double cuts[] = { 0.3, 0.6, 0.95 };
    int n = 0;
    for (const QString &show : shows)
    {
        if (!QFileInfo::exists(show))
            continue;
        const double kept = cuts[n++ % 3];
        QTest::newRow(qPrintable(QStringLiteral("%1 %2%").arg(QFileInfo(show).fileName())
                                     .arg(int(kept * 100))))
            << show << kept;
    }
}

void ShowFiles_Test::aDamagedShowOpensAndIsNotSavedOver()
{
    QFETCH(QString, path);
    QFETCH(double, kept);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QFileInfo(path).fileName());
    const QByteArray whole = fileBytes(path);
    QVERIFY(!whole.isEmpty());
    {
        QFile f(copy);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(whole.left(int(whole.size() * kept)));
    }
    const QByteArray cut = fileBytes(copy);

    g_closed.clear();
    QCOMPARE(open(m_app, copy), QFile::NoError);
    QCOMPARE(m_app->damagedFile(), copy);
    QCOMPARE(m_app->fileName(), copy);
    QVERIFY2(g_closed.join('\n').contains(QStringLiteral("damaged or incomplete (line")),
             qPrintable(QStringLiteral("said: ") + g_closed.join(QStringLiteral(" | "))));
    QVERIFY2(m_app->doc()->errorLog().contains(QStringLiteral("damaged or incomplete")),
             qPrintable(m_app->doc()->errorLog()));
    QVERIFY(!m_app->doc()->isModified());

    // Save: Save As, not the original. Its dialog closed: nothing written.
    g_closed.clear();
    {
        ModalCloser closer;
        m_app->doc()->setModified();
        m_app->slotFileSave();
        closeModalsFor(30);
    }
    /* Its Save As dialog -- or, first, the question about fixtures with no
       definition on this computer (Sample, help-demo here), which Save As
       asks too; closed, it saves nothing. */
    const QString said = g_closed.join('\n');
    QVERIFY2(said.contains(QStringLiteral("QFileDialog"))
             || said.contains(QStringLiteral("standing in as dimmers")),
             qPrintable(QStringLiteral("said: ") + g_closed.join(QStringLiteral(" | "))));
    QCOMPARE(fileBytes(copy), cut);
    QCOMPARE(m_app->damagedFile(), copy);

    // Saved under a name of its own: an ordinary show, Save saves it.
    const QString recovered = dir.filePath(QStringLiteral("recovered.") + QFileInfo(path).suffix());
    QCOMPARE(save(m_app, recovered), QFile::NoError);
    QVERIFY(m_app->damagedFile().isEmpty());
    QCOMPARE(m_app->fileName(), recovered);
    QCOMPARE(fileBytes(copy), cut);
    QCOMPARE(open(m_app, recovered), QFile::NoError);
    QVERIFY(m_app->damagedFile().isEmpty());
    QVERIFY(!m_app->doc()->errorLog().contains(QStringLiteral("damaged or incomplete")));
    // (That a whole show is not taken for a damaged one is checked for
    // every show in aShowSavedAndOpenedAgainIsTheSame.)
}

void ShowFiles_Test::aStockShowOpensWithAutoOpenOff()
{
    /* One stock show through every step here; every other stock show is
       checked the same way in aShowSavedAndOpenedAgainIsTheSame. */
    const QString path = QStringLiteral(SHOWFILES_SAMPLE);    // QLC+'s own sample
    QVERIFY(!savedByQlcconsole(path));
    QVERIFY(!fileBytes(path).contains("<ShutterAutoOpen"));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QFileInfo(path).fileName());
    QVERIFY(QFile::copy(path, copy));

    QCOMPARE(open(m_app, copy), QFile::NoError);
    QVERIFY2(!m_app->doc()->shutterAutoOpen()->isEnabled(),
             "a QLC+ show must send the DMX it sends in QLC+");

    // Saved here, it says so -- and opens with it off still.
    const QString saved = dir.filePath(QStringLiteral("saved.") + QFileInfo(path).suffix());
    QCOMPARE(save(m_app, saved), QFile::NoError);
    QVERIFY(fileBytes(saved).contains("<ShutterAutoOpen"));
    QCOMPARE(open(m_app, saved), QFile::NoError);
    QVERIFY(!m_app->doc()->shutterAutoOpen()->isEnabled());

    // A new show keeps it on.
    m_app->clearDocument();
    QVERIFY(m_app->doc()->shutterAutoOpen()->isEnabled());
}

void ShowFiles_Test::collectForTransferTakesTheVenues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // A show whose definitions are all in the app, saved here as Tour.qlcc.
    const QString demo = dir.filePath(QStringLiteral("demo.qxw"));
    QVERIFY(QFile::copy(QStringLiteral(SHOWFILES_WORKSPACES) + QStringLiteral("help-demo.qxw"), demo));
    QCOMPARE(open(m_app, demo), QFile::NoError);
    const QString show = dir.filePath(QStringLiteral("Tour.qlcc"));
    QCOMPARE(save(m_app, show), QFile::NoError);
    QCOMPARE(m_app->fileName(), show);

    // A venue with a correction, kept beside the show.
    Doc *doc = m_app->doc();
    VenueCorrections *venues = doc->programmer()->venueCorrections();
    const QString lincoln = venues->addVenue(QStringLiteral("Lincoln HS"));
    QVERIFY(venues->setActiveVenue(lincoln));
    QString why;
    QVERIFY2(venues->save(&why), qPrintable(why));
    const QByteArray kept = fileBytes(VenueCorrections::sidecarPath(show));
    QVERIFY(kept.contains("Lincoln HS"));

    // Collect into a folder of its own, somewhere else.
    QTemporaryDir away;
    QVERIFY(away.isValid());
    /* What it asks, in turn: the definitions this computer lacks (Save
       anyway), the folder, then what it collected. */
    /* Answered from a timer, one at a time: each must return to its own
       event loop before the next can open. */
    QString told;
    const QString parent = away.path();
    QTimer answer;
    answer.setInterval(30);
    connect(&answer, &QTimer::timeout, this, [&told, &answer, parent]() {
        QWidget *w = QApplication::activeModalWidget();
        if (QFileDialog *fd = qobject_cast<QFileDialog *>(w))
        {
            // Opened on the folder, and Choose.
            fd->setDirectory(parent);
            static_cast<QDialog *>(fd)->accept();
        }
        else if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
        {
            if (box->button(QMessageBox::Save) != nullptr)
                box->button(QMessageBox::Save)->click();
            else
            {
                told = box->text();
                answer.stop();
                box->accept();
            }
        }
    });
    answer.start();
    m_app->slotFileCollectForTransfer();
    QCoreApplication::processEvents();
    QVERIFY2(told.startsWith(QStringLiteral("Collected")), qPrintable(told));

    const QString folder = QDir(away.path()).absoluteFilePath(QStringLiteral("Tour"));
    const QString moved = QDir(folder).absoluteFilePath(QStringLiteral("Tour.qlcc"));
    QVERIFY2(QFile::exists(moved), qPrintable(told));
    const QString sidecar = VenueCorrections::sidecarPath(moved);
    QVERIFY2(QFile::exists(sidecar), qPrintable(folderState(away.path()).join('\n')));

    // This window goes on with the show it has open.
    QCOMPARE(m_app->fileName(), show);

    // Opened from there, it has its venue, applied.
    QCOMPARE(open(m_app, moved), QFile::NoError);
    venues = m_app->doc()->programmer()->venueCorrections();
    QVERIFY(venues->venueNames().contains(QStringLiteral("Lincoln HS")));
    QCOMPARE(venues->activeVenue(), QStringLiteral("Lincoln HS"));
}

void ShowFiles_Test::saveAsToAnotherFolderTakesThePictures()
{
    QTemporaryDir base;
    QVERIFY(base.isValid());
    QDir root(base.path());
    QVERIFY(root.mkpath(QStringLiteral("Show 2")) && root.mkpath(QStringLiteral("Show"))
            && root.mkpath(QStringLiteral("Downloads")) && root.mkpath(QStringLiteral("Elsewhere")));
    const QString oldFolder = root.absoluteFilePath(QStringLiteral("Show 2"));
    const QString newFolder = root.absoluteFilePath(QStringLiteral("Show"));
    const QString picture = root.absoluteFilePath(QStringLiteral("Downloads/stones.jpg"));
    {
        QFile f(picture);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("stones");
    }

    // A show in "Show 2" whose truss and tower wear a picture from elsewhere.
    const QString demo = QDir(oldFolder).absoluteFilePath(QStringLiteral("demo.qxw"));
    QVERIFY(QFile::copy(QStringLiteral(SHOWFILES_WORKSPACES) + QStringLiteral("help-demo.qxw"), demo));
    QCOMPARE(open(m_app, demo), QFile::NoError);
    MonitorProperties *mp = m_app->doc()->monitorProperties();
    Truss *truss = mp->addTruss();
    truss->setSock(true);
    truss->setSockPattern(picture);
    Tower *tower = mp->addTower();
    tower->setFaces(true);
    tower->setFacesPattern(picture);
    const QString oldShow = QDir(oldFolder).absoluteFilePath(QStringLiteral("Tour.qlcc"));
    QCOMPARE(save(m_app, oldShow), QFile::NoError);
    QCOMPARE(fileBytes(QDir(oldFolder).absoluteFilePath(QStringLiteral("images/stones.jpg"))),
             QByteArray("stones"));
    const QStringList oldBefore = folderState(oldFolder);

    // File > Save As..., into "Show".
    const QString newShow = QDir(newFolder).absoluteFilePath(QStringLiteral("Tour.qlcc"));
    QTimer answer;
    answer.setInterval(30);
    connect(&answer, &QTimer::timeout, this, [&answer, newShow]() {
        QWidget *w = QApplication::activeModalWidget();
        if (QFileDialog *fd = qobject_cast<QFileDialog *>(w))
        {
            fd->selectFile(newShow);
            answer.stop();
            static_cast<QDialog *>(fd)->accept();
        }
        else if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
        {
            if (box->button(QMessageBox::Save) != nullptr)
                box->button(QMessageBox::Save)->click();
            else
                box->accept();
        }
    });
    answer.start();
    QCOMPARE(m_app->slotFileSaveAs(), QFile::NoError);
    answer.stop();
    QCoreApplication::processEvents();
    QCOMPARE(m_app->fileName(), newShow);
    QCOMPARE(QDir(m_app->doc()->workspacePath()).absolutePath(), newFolder);

    // The picture went into the new folder; nothing was written in the old.
    QCOMPARE(fileBytes(QDir(newFolder).absoluteFilePath(QStringLiteral("images/stones.jpg"))),
             QByteArray("stones"));
    QCOMPARE(folderState(oldFolder), oldBefore);
    const QByteArray xml = fileBytes(newShow);
    QVERIFY2(xml.contains("SockPattern=\"images/stones.jpg\""), xml.constData());
    QVERIFY(xml.contains("FacesPattern=\"images/stones.jpg\""));
    QVERIFY(!xml.contains("Show 2"));

    // Opened from its new place, it finds them there.
    auto inFolder = [](const QString &path, const QString &folder) {
        return QFileInfo(path).exists()
               && QFileInfo(QFileInfo(path).absolutePath()).canonicalFilePath()
                      == QFileInfo(QDir(folder).absoluteFilePath(QStringLiteral("images"))).canonicalFilePath();
    };
    QCOMPARE(open(m_app, newShow), QFile::NoError);
    mp = m_app->doc()->monitorProperties();
    QVERIFY(!mp->trusses().isEmpty() && !mp->towers().isEmpty());
    QVERIFY2(inFolder(mp->trusses().last()->sockPattern(), newFolder),
             qPrintable(mp->trusses().last()->sockPattern()));
    QVERIFY2(inFolder(mp->towers().last()->facesPattern(), newFolder),
             qPrintable(mp->towers().last()->facesPattern()));

    // Saved by name from code (the same App::saveXML), it is the same story.
    const QString elsewhere = root.absoluteFilePath(QStringLiteral("Elsewhere/Tour.qlcc"));
    const QStringList newBefore = folderState(newFolder);
    QCOMPARE(save(m_app, elsewhere), QFile::NoError);
    QCOMPARE(folderState(newFolder), newBefore);
    QVERIFY(QFile::exists(root.absoluteFilePath(QStringLiteral("Elsewhere/images/stones.jpg"))));
    QVERIFY(fileBytes(elsewhere).contains("SockPattern=\"images/stones.jpg\""));

    // A save that fails leaves the show's folder as it was.
    const QString folderBefore = m_app->doc()->workspacePath();
    QVERIFY(save(m_app, root.absoluteFilePath(QStringLiteral("nowhere/at/all/Tour.qlcc")))
            != QFile::NoError);
    QCOMPARE(m_app->doc()->workspacePath(), folderBefore);
}

/* File ▸ Collect for Transfer… into @p parent, answering its questions
   (Save anyway, the folder, the result) from a timer. Returns what it said. */
static QString collectInto(App *app, const QString &parent)
{
    QString told;
    QTimer answer;
    answer.setInterval(30);
    QObject::connect(&answer, &QTimer::timeout, app, [&told, &answer, parent]() {
        QWidget *w = QApplication::activeModalWidget();
        if (QFileDialog *fd = qobject_cast<QFileDialog *>(w))
        {
            fd->setDirectory(parent);
            static_cast<QDialog *>(fd)->accept();
        }
        else if (QMessageBox *box = qobject_cast<QMessageBox *>(w))
        {
            if (box->button(QMessageBox::Save) != nullptr)
                box->button(QMessageBox::Save)->click();
            else
            {
                told = box->text();
                answer.stop();
                box->accept();
            }
        }
    });
    answer.start();
    app->slotFileCollectForTransfer();
    QCoreApplication::processEvents();
    return told;
}

void ShowFiles_Test::collectForTransferTakesThePictures()
{
    QTemporaryDir base;
    QVERIFY(base.isValid());
    QDir root(base.path());
    QVERIFY(root.mkpath(QStringLiteral("Show")) && root.mkpath(QStringLiteral("Downloads"))
            && root.mkpath(QStringLiteral("Away")));
    const QString showFolder = root.absoluteFilePath(QStringLiteral("Show"));
    auto picture = [&root](const QString &rel, const QByteArray &bytes) {
        const QString path = root.absoluteFilePath(rel);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(bytes);
        return path;
    };
    const QString stones = picture(QStringLiteral("Downloads/stones.jpg"), "stones");
    const QString bricks = picture(QStringLiteral("Downloads/bricks.png"), "bricks");
    const QString plan = picture(QStringLiteral("Downloads/plan.png"), "plan");

    // A show in "Show": a truss sock and tower faces, a picture on the plot.
    // (A gobo swap is a name in this computer's gobo library: not a file of the show.)
    const QString demo = QDir(showFolder).absoluteFilePath(QStringLiteral("demo.qxw"));
    QVERIFY(QFile::copy(QStringLiteral(SHOWFILES_WORKSPACES) + QStringLiteral("help-demo.qxw"), demo));
    QCOMPARE(open(m_app, demo), QFile::NoError);
    Doc *doc = m_app->doc();
    MonitorProperties *mp = doc->monitorProperties();
    Truss *truss = mp->addTruss();
    truss->setSock(true);
    truss->setSockPattern(stones);
    Tower *tower = mp->addTower();
    tower->setFaces(true);
    tower->setFacesPattern(bricks);
    const quint32 imageId = mp->addImage(plan);
    const QString show = QDir(showFolder).absoluteFilePath(QStringLiteral("Tour.qlcc"));
    QCOMPARE(save(m_app, show), QFile::NoError);
    const QString sockBefore = mp->trusses().last()->sockPattern();
    const QString imageBefore = mp->image(imageId).source;

    // Collected into a folder of its own, somewhere else.
    const QString told = collectInto(m_app, root.absoluteFilePath(QStringLiteral("Away")));
    QVERIFY2(told.startsWith(QStringLiteral("Collected")), qPrintable(told));
    const QString folder = root.absoluteFilePath(QStringLiteral("Away/Tour"));
    const QString moved = QDir(folder).absoluteFilePath(QStringLiteral("Tour.qlcc"));
    QVERIFY(QFile::exists(moved));
    const QDir images(QDir(folder).absoluteFilePath(QStringLiteral("images")));
    QCOMPARE(fileBytes(images.absoluteFilePath(QStringLiteral("stones.jpg"))), QByteArray("stones"));
    QCOMPARE(fileBytes(images.absoluteFilePath(QStringLiteral("bricks.png"))), QByteArray("bricks"));
    QCOMPARE(fileBytes(images.absoluteFilePath(QStringLiteral("plan.png"))), QByteArray("plan"));

    // The collected show names them there, relatively: nothing outside it.
    const QByteArray xml = fileBytes(moved);
    QVERIFY2(xml.contains("SockPattern=\"images/stones.jpg\""), xml.constData());
    QVERIFY(xml.contains("FacesPattern=\"images/bricks.png\""));
    QVERIFY(xml.contains("Source=\"images/plan.png\""));
    QVERIFY(!xml.contains(base.path().toUtf8()));

    // This window's show still names its own.
    QCOMPARE(m_app->fileName(), show);
    QCOMPARE(mp->trusses().last()->sockPattern(), sockBefore);
    QCOMPARE(mp->image(imageId).source, imageBefore);

    // Another computer: only the folder. It finds every picture in it.
    QVERIFY(QDir(showFolder).removeRecursively());
    QVERIFY(QDir(root.absoluteFilePath(QStringLiteral("Downloads"))).removeRecursively());
    QCOMPARE(open(m_app, moved), QFile::NoError);
    mp = m_app->doc()->monitorProperties();
    auto inImages = [&images](const QString &path) {
        return QFileInfo(path).isFile()
               && QFileInfo(QFileInfo(path).absolutePath()).canonicalFilePath()
                      == QFileInfo(images.absolutePath()).canonicalFilePath();
    };
    QVERIFY(!mp->trusses().isEmpty() && !mp->towers().isEmpty() && !mp->images().isEmpty());
    QVERIFY2(inImages(mp->trusses().last()->sockPattern()), qPrintable(mp->trusses().last()->sockPattern()));
    QVERIFY2(inImages(mp->towers().last()->facesPattern()), qPrintable(mp->towers().last()->facesPattern()));
    QVERIFY2(inImages(mp->images().last().source), qPrintable(mp->images().last().source));
}

QTEST_MAIN(ShowFiles_Test)
