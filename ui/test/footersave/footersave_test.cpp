/*
  Q Light Controller Plus
  footersave_test.cpp

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

#include <QApplication>
#include <QStatusBar>
#include <QToolButton>
#include <QAbstractButton>
#include <QTabBar>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QLabel>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QtTest>

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "programmercontroller.h"
#undef protected
#undef private
#include "powerdistribution.h"
#include "programmingmanager.h"
#include "doc.h"
#include "showtreecontroller.h"
#include "showtreeview.h"
#include "fixturegroup.h"
#include "qlcpalette.h"
#include "previewkeep.h"
#include "footersave_test.h"

namespace
{

/* Where every footer chip is and whether the footer squeezed it: a save
   that changes any of it is the footer jumping. */
QStringList footer(App *app)
{
    QStringList out;
    QStatusBar *sb = app->statusBar();
    for (QWidget *w : sb->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
    {
        if (!w->isVisibleTo(sb))
            continue;
        QString name = w->objectName();
        if (w == app->m_statusPowerLabel)
            name = QStringLiteral("power");
        else if (w == app->m_statusDirtyLabel)
            name = QStringLiteral("unsaved");
        else if (w == app->m_statusStandby)
            name = QStringLiteral("next cue");
        else if (w == app->m_statusProgrammerLabel)
            name = QStringLiteral("programmer");
        else if (name.isEmpty())
            name = QString::fromLatin1(w->metaObject()->className());
        out << QStringLiteral("%1 at %2 width %3%4").arg(name).arg(w->x()).arg(w->width())
               .arg(w->property("statusSqueezed").toBool() ? QStringLiteral(" (squeezed)") : QString());
    }
    return out;
}

void settle(int ms)
{
    QElapsedTimer t;
    t.start();
    do
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    while (t.elapsed() < ms);
}

}

void FooterSave_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
    QString src = qEnvironmentVariable("FOOTER_SHOW");
    if (src.isEmpty())
        src = QStringLiteral(FOOTERSAVE_SHOW);
    const QString show = m_dir.filePath(QStringLiteral("show.") + QFileInfo(src).suffix());
    QVERIFY(QFile::copy(src, show));
    QFile::setPermissions(show, QFile::ReadOwner | QFile::WriteOwner);
    QCOMPARE(m_app->loadXML(show), QFile::NoError);
    m_app->resize(1280, 800);
    m_app->show();
    settle(300);
}

void FooterSave_Test::cleanupTestCase()
{
    m_app->doc()->resetModified();
    delete m_app;
    m_app = nullptr;
}

void FooterSave_Test::savingLeavesTheFooterWhereItWas()
{
    sweepSaves();
}

void FooterSave_Test::savingWithEditsInTheProgrammerLeavesTheFooterWhereItWas()
{
    /* A scene edited through the programmer: its pre-edit state survives the
       save, and the chip turns from the red line to the longer grey one. */
    ProgrammerController *pc = m_app->doc()->programmer();
    QVERIFY(pc != nullptr);
    pc->m_editedScenes.insert(1);
    auto tidy = qScopeGuard([&]() {
        pc->m_editedScenes.clear();
        m_app->slotProgrammerDirtyChanged(false);
    });
    m_app->slotProgrammerDirtyChanged(true);
    QVERIFY(m_app->m_statusProgrammerLabel->isVisibleTo(m_app->statusBar()));
    sweepSaves();
    QVERIFY(m_app->m_statusProgrammerLabel->text().contains(QStringLiteral("saved, revertable")));
}

namespace
{
/* Counts what a chip does to the footer: a style change is a re-polish. */
class StyleCounter : public QObject
{
public:
    QHash<QObject *, int> styles;
    void watch(QWidget *w) { if (w != nullptr) { w->installEventFilter(this); styles[w] = 0; } }
    void reset() { for (auto it = styles.begin(); it != styles.end(); ++it) it.value() = 0; }
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() == QEvent::StyleChange)
            ++styles[o];
        return false;
    }
};
}

void FooterSave_Test::routineUpdatesChangeNoChipAndMoveNone()
{
    m_app->m_showFooterLoad = true;
    m_app->m_statusLoadLabel->setVisible(true);
    m_app->resize(1280, 800);
    settle(300);
    StyleCounter count;
    const QList<QWidget *> chips = { m_app->m_statusModeLabel, m_app->m_statusRigNetLabel, m_app->m_statusReviewLabel,
                                     m_app->m_statusTimelineLabel, m_app->m_statusLoadLabel, m_app->m_statusDirtyLabel,
                                     m_app->m_statusStandby };
    for (QWidget *w : chips)
        count.watch(w);
    auto routine = [this]() {
        m_app->slotUpdateHealthFooter();
        m_app->updateStatusBar();
        m_app->updateRigNetworkFooter();
        m_app->updateReviewStatus();
        m_app->slotTimelineControlChanged();
        m_app->slotUpdateStandby();
        m_app->slotDocModified(m_app->doc()->isModified());
    };
    routine();                  // the first reading may set each chip once
    settle(100);
    const QStringList before = footer(m_app);
    QStringList texts;
    for (QWidget *w : chips)
        if (QLabel *l = qobject_cast<QLabel *>(w))
            texts << l->text();
    count.reset();
    for (int i = 0; i < 6; ++i)
    {
        routine();
        settle(60);
    }
    for (QWidget *w : chips)
        QVERIFY2(count.styles.value(w) == 0,
                 qPrintable(QStringLiteral("%1 was restyled %2 times by routine updates")
                            .arg(w->objectName().isEmpty() ? QString::fromLatin1(w->metaObject()->className())
                                                           : w->objectName()).arg(count.styles.value(w))));
    QCOMPARE(footer(m_app), before);
}

void FooterSave_Test::anAutosaveDoesNotTurnTheDirtyChipOverAndBack()
{
    m_app->resize(1280, 800);
    m_app->doc()->setModified();
    settle(100);
    StyleCounter count;
    count.watch(m_app->m_statusDirtyLabel);
    const QString unsaved = QString::fromUtf8("● Unsaved changes");
    QCOMPARE(m_app->m_statusDirtyLabel->text(), unsaved);
    const QStringList before = footer(m_app);
    QStringList seen;
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        m_app->slotAutosave();
        seen << m_app->m_statusDirtyLabel->text();
        m_app->doc()->setModified();    // the next edit
        seen << m_app->m_statusDirtyLabel->text();
        settle(50);
    }
    QCOMPARE(seen, QStringList() << unsaved << unsaved << unsaved << unsaved << unsaved << unsaved);
    QCOMPARE(count.styles.value(m_app->m_statusDirtyLabel), 0);
    QCOMPARE(footer(m_app), before);
    QVERIFY(!m_app->m_lastAutosaveTime.isEmpty());      // it did autosave
    m_app->doc()->resetModified();
}

void FooterSave_Test::aChipTheFooterKeepsIsLaidOutWhole()
{
    /* Low-priority chips on, so that at each width some are squeezed after
       the last one that fits: their gaps are still in the bar's layout. */
    QStatusBar *sb = m_app->statusBar();
    QList<QLabel *> extra = { m_app->m_statusTeachingLabel, m_app->m_statusRigNetLabel,
                              m_app->m_statusUpdateLabel, m_app->m_statusTimecodeLabel };
    QList<QPair<QLabel *, QString>> before;
    for (QLabel *l : extra)
    {
        QVERIFY(l != nullptr);
        before << qMakePair(l, l->isVisibleTo(sb) ? l->text() : QString());
        if (l->text().isEmpty())
            l->setText(QStringLiteral("A chip"));
        l->show();
    }
    auto tidy = qScopeGuard([&]() {
        for (const auto &w : std::as_const(before))
        {
            w.first->setText(w.second);
            w.first->setVisible(!w.second.isEmpty());
        }
        m_app->resize(1280, 800);
        settle(60);
    });
    QStringList cut;
    for (int width = 1400; width >= 820; width -= 10)
    {
        m_app->resize(width, 800);
        /* Fitted and still: a chip's own timer (Next, the Follow chip, the
           power estimate) can change it after a fit, and the fit that
           follows is queued -- under load, after a fixed wait. */
        QElapsedTimer still;
        still.start();
        QStringList was;
        do
        {
            m_app->fitStatusBar();
            settle(40);
            const QStringList now = footer(m_app);
            if (now == was)
                break;
            was = now;
        } while (still.elapsed() < 3000);
        for (QWidget *w : m_app->statusChipsByPriority())
        {
            if (!w->isVisibleTo(sb) || w->property("statusSqueezed").toBool())
                continue;
            const int want = qMin(qMax(w->sizeHint().width(), w->minimumSizeHint().width()),
                                  w->maximumWidth());
            if (w->width() < want)
                cut << QStringLiteral("window %1: %2 %3 is %4 wide, wants %5")
                           .arg(width).arg(QString::fromLatin1(w->metaObject()->className()))
                           .arg(w->objectName()).arg(w->width()).arg(want);
        }
    }
    QVERIFY2(cut.isEmpty(), qPrintable(cut.mid(0, 8).join(QLatin1Char('\n'))));
}

void FooterSave_Test::theLoadAndNextChipsKeepTheirRoom()
{
    m_app->m_showFooterLoad = true;
    m_app->m_statusLoadLabel->setVisible(true);
    const QFont f = m_app->m_statusLoadLabel->font();
    const int stallText = QFontMetrics(f).horizontalAdvance(
        QStringLiteral("Load: 123.45 / 20 ms (100%)  stall 100 ms"));
    m_app->resize(2400, 800);       // room for every chip
    settle(300);
    QVERIFY2(m_app->m_statusLoadLabel->width() >= stallText,
             qPrintable(QStringLiteral("Load chip %1 px, its stall text %2 px")
                        .arg(m_app->m_statusLoadLabel->width()).arg(stallText)));

    // A production named at length: the Next chip names it in front of the cue.
    ShowTreeController *st = m_app->doc()->showTree();
    const QString longName = QString(260, QLatin1Char('W'));
    const quint32 prod = st->addProduction(longName);
    const quint32 was = st->currentProduction();
    QVERIFY(st->setCurrentProduction(prod));
    const quint32 list = st->addCueList(prod, "Main", ShowTree::ListMode::Tracking);
    QVERIFY(st->addCue(list, -1, "One") != 0);
    for (int width = 1500; width >= 900; width -= 150)
    {
        m_app->resize(width, 800);
        settle(60);
        m_app->slotUpdateStandby();
        settle(60);
        if (!m_app->m_statusStandby->isVisibleTo(m_app->statusBar()))
            continue;
        QVERIFY2(m_app->m_statusStandby->width() <= qMax(480, width / 4) + 40,
                 qPrintable(QStringLiteral("Next cue chip %1 px at window %2")
                            .arg(m_app->m_statusStandby->width()).arg(width)));
        if (qEnvironmentVariableIsSet("FOOTER_SHOTS"))   // before/after pictures, on request
            m_app->statusBar()->grab().save(QStringLiteral("%1/footer-%2.png")
                                            .arg(qEnvironmentVariable("FOOTER_SHOTS")).arg(width));
        QVERIFY(m_app->m_statusStandby->text().endsWith(QString::fromUtf8("…")));
        QVERIFY(m_app->m_statusStandby->toolTip().contains(longName));
    }
    if (was != ShowTree::invalidId())
        st->setCurrentProduction(was);
}

void FooterSave_Test::aLookingJumpKeepsThePreviewPlaying()
{
    Doc *doc = m_app->doc();
    ShowTreeView *view = ShowTreeView::instance();
    QVERIFY(view != nullptr);
    QVERIFY(!doc->fixtureGroups().isEmpty() || !doc->fixtures().isEmpty());
    FixtureGroup *g = new FixtureGroup(doc);
    g->setName("Preview group");
    doc->addFixtureGroup(g);
    g->assignFixture(doc->fixtures().first()->id());
    QLCPalette *red = new QLCPalette(QLCPalette::Color, doc);
    red->setName("Preview red");
    red->setValue(QColor(255, 0, 0));
    QVERIFY(doc->addPalette(red));
    ShowTree::Target target;
    target.groups << g->id();
    ShowTreeController *st = doc->showTree();
    const quint32 prod = st->addProduction("Jumps");
    const quint32 list = st->addCueList(prod, "Jumps", ShowTree::ListMode::Tracking);
    const quint32 cue = st->addCue(list, -1, "Red");
    QVERIFY(st->setTargetLook(cue, target, st->lookForPalette(red->id())));
    QVERIFY(st->setCurrentProduction(prod));
    doc->setMode(Doc::Design);
    m_app->showContext(QStringLiteral("Show Tree"));
    settle(100);
    bool picked = false;
    for (QTreeWidgetItemIterator it(view->tree()); *it && !picked; ++it)
        if ((*it)->data(0, ShowTreeView::KindRole).toInt() == ShowTreeView::CueNode
            && (*it)->data(0, ShowTreeView::IdRole).toUInt() == cue)
        {
            view->tree()->setCurrentItem(*it);
            picked = true;
        }
    QVERIFY(picked);
    QTRY_COMPARE_WITH_TIMEOUT(st->previewState(), ShowTreeController::PreviewState::Playing, 3000);

    // The Show Status "Show" jump, the Review line, the Why-on line: look only.
    m_app->showContextKeepingPreview(QStringLiteral("Lighting Studio"));
    settle(100);
    QVERIFY(!view->isVisible());
    QCOMPARE(st->previewState(), ShowTreeController::PreviewState::Playing);
    // Back on the Show Tree it plays on, the same cue.
    m_app->showContext(QStringLiteral("Show Tree"));
    settle(100);
    QCOMPARE(st->previewedCue(), cue);
    QCOMPARE(st->previewState(), ShowTreeController::PreviewState::Playing);

    // Any other way out of the tab still lets go of the stage.
    m_app->showContext(QStringLiteral("Lighting Studio"));
    settle(100);
    QVERIFY(st->previewState() != ShowTreeController::PreviewState::Playing);
    m_app->showContext(QStringLiteral("Show Tree"));
    st->stopPreview();
}

void FooterSave_Test::noTextIsUtf8ReadAsLatin1()
{
    /* UTF-8 bytes seen as Latin-1: a lead byte (Â Ã â ð) then a 0x80-0xBF
       code point -- no real text has that pair. */
    const QRegularExpression bad(QStringLiteral("[\u00C2\u00C3\u00E2\u00F0][\u0080-\u00BF]"));
    QStringList found;
    auto check = [&](const QString &what, const QString &text) {
        if (bad.match(text).hasMatch())
            found << what + QStringLiteral(": ") + text;
    };
    for (QWidget *w : m_app->findChildren<QWidget *>())
    {
        const QString name = QString::fromLatin1(w->metaObject()->className()) + QLatin1Char('#') + w->objectName();
        if (auto *b = qobject_cast<QAbstractButton *>(w))
            check(name, b->text());
        if (auto *l = qobject_cast<QLabel *>(w))
            check(name, l->text());
        if (auto *tb = qobject_cast<QTabBar *>(w))
            for (int i = 0; i < tb->count(); ++i)
                check(name, tb->tabText(i));
        if (auto *tree = qobject_cast<QTreeWidget *>(w))
            for (QTreeWidgetItemIterator it(tree); *it; ++it)
                for (int c = 0; c < tree->columnCount(); ++c)
                    check(name, (*it)->text(c));
        check(name + QStringLiteral(" tooltip"), w->toolTip());
    }
    QVERIFY2(found.isEmpty(), qPrintable(found.join(QLatin1Char('\n'))));

    /* The same, at the source: a QStringLiteral / QLatin1String holding
       UTF-8 escape bytes is read as Latin-1 (Qt 6 puts the bytes in a u""
       literal), which is how the Plot layers' "▾" came out as "â–¾". */
    const QRegularExpression literal(QStringLiteral(
        "(QStringLiteral|QLatin1String|QLatin1StringView)\\(\"[^\"\\n]*\\\\x[eEfF][0-9a-fA-F]\\\\x[89a-bA-B][0-9a-fA-F]"));
    QStringList sources;
    QDirIterator it(QStringLiteral(FOOTERSAVE_UI_SRC), { "*.cpp", "*.h" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QFile f(it.next());
        if (f.open(QIODevice::ReadOnly) && literal.match(QString::fromUtf8(f.readAll())).hasMatch())
            sources << f.fileName();
    }
    QVERIFY2(sources.isEmpty(), qPrintable(QStringLiteral("UTF-8 escape bytes in a Latin-1 literal: ")
                                           + sources.join(QStringLiteral(", "))));
}

void FooterSave_Test::sweepSaves()
{
    // The power chip on (a show with its power set up), a cue list's Next.
    PowerDistribution *power = m_app->doc()->powerDistribution();
    if (power->sources().isEmpty())
        power->sources().append(PowerSource());
    m_app->m_showFooterPower = true;
    /* Every chip as the app's own timers leave it, BEFORE the first reading:
       the rig's real estimate ("0.3 A", a character shorter than this one)
       and the Next chip come on timers (500 and 250 ms). A fixed wait for
       them lost the race on a busy Mac, and the reading that came late,
       mid-sweep, was taken for the save moving the footer (gate 043j). */
    QSignalSpy readings(ProgrammingManager::instance(), &ProgrammingManager::powerEstimateChanged);
    emit ProgrammingManager::instance()->powerEstimateChanged(12.5, 1.4, false, QString());
    m_app->doc()->setModified();        // what the estimate reads changed: its next tick reads it
    QTRY_VERIFY_WITH_TIMEOUT(readings.count() >= 2, 10000);
    m_app->slotUpdateStandby();
    // Then the footer as those leave it: two looks a beat apart the same.
    QElapsedTimer still;
    still.start();
    QStringList was = footer(m_app);
    do
    {
        settle(100);
        const QStringList now = footer(m_app);
        if (now == was)
            break;
        was = now;
    } while (still.elapsed() < 5000);
    QVERIFY(m_app->m_statusPowerLabel->isVisibleTo(m_app->statusBar()));

    // Wide to narrow, through where the footer squeezes one chip after another.
    for (int width = 1500; width >= 850; width -= 25)
    {
        m_app->resize(width, 800);
        settle(30);
        m_app->doc()->setModified();
        settle(30);
        const QStringList unsaved = footer(m_app);
        // Settled: fitting the footer again changes nothing (a chip squeezed
        // at one fit and back at the next is the footer flickering).
        m_app->fitStatusBar();
        settle(30);
        QCOMPARE(footer(m_app), unsaved);
        QCOMPARE(m_app->m_statusDirtyLabel->text(), QString::fromUtf8("● Unsaved changes"));

        QCOMPARE(m_app->saveXML(m_app->fileName()), QFile::NoError);   // what File ▸ Save writes
        settle(30);
        QVERIFY(!m_app->doc()->isModified());
        QCOMPARE(m_app->m_statusDirtyLabel->text(), QString::fromUtf8("✓ Saved"));
        const QStringList saved = footer(m_app);
        if (saved != unsaved)
        {
            QStringList moved;
            for (int i = 0; i < qMax(saved.size(), unsaved.size()); ++i)
                if (saved.value(i) != unsaved.value(i))
                    moved << unsaved.value(i) + QStringLiteral(" -> ") + saved.value(i);
            QFAIL(qPrintable(QStringLiteral("window %1 wide, saving moved the footer: %2")
                             .arg(width).arg(moved.join(QStringLiteral("; ")))));
        }
    }
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    qputenv("HOME", home.path().toUtf8());
    qputenv("QLC_NO_RECOVERY_PROMPT", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcconsole-test"));
    FooterSave_Test test;
    return QTest::qExec(&test, argc, argv);
}
