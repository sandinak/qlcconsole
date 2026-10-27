/*
  Q Light Controller Plus
  showfixtureproperties_test.cpp

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

/* Split from showfiles_test (RC5): with it, that program ran past CI
   Windows' five minutes a test program. */

#include <QtTest>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidgetItemIterator>

#include "showfixtureproperties_test.h"

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

/* Close whatever modal opens while a show loads (a missing definition's
   question, an old-show notice): a test must never wait for a click. */
static void closeModalsFor(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        QWidget *w = QApplication::activeModalWidget();
        if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
        {
            qWarning() << "closed a modal while a show opened:" << w->metaObject()->className();
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
            else
                w->close();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

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
                qWarning() << "closed a modal while a show opened:" << w->metaObject()->className();
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

static QFile::FileError open(App *app, const QString &path)
{
    ModalCloser closer;
    app->clearDocument();
    const QFile::FileError e = app->loadXML(path);
    closeModalsFor(30);         // and the venue question queued after it
    return e;
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

void ShowFixtureProperties_Test::initTestCase()
{
    m_app = new App();
    m_app->init();
}

void ShowFixtureProperties_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void ShowFixtureProperties_Test::cleanup()
{
    ModalCloser closer;
    m_app->clearDocument();
    closeModalsFor(50);
}

void ShowFixtureProperties_Test::fixturePropertiesOpensForEveryFixture_data()
{
    QTest::addColumn<QString>("path");
    QStringList paths = showFiles();
    QDir course(QStringLiteral(SHOWFILES_COURSE));
    for (const QString &name : course.entryList({ QStringLiteral("*.qlcc") }, QDir::Files, QDir::Name))
        paths << course.absoluteFilePath(name);
    for (const QString &path : paths)
        QTest::newRow(qPrintable(QFileInfo(path).fileName())) << path;
}

void ShowFixtureProperties_Test::fixturePropertiesOpensForEveryFixture()
{
    QFETCH(QString, path);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QStringLiteral("show.") + QFileInfo(path).suffix());
    QVERIFY(QFile::copy(path, copy));
    QCOMPARE(open(m_app, copy), QFile::NoError);
    Doc *doc = m_app->doc();
    if (doc->fixtures().isEmpty())
        QSKIP("no fixtures in this show");
    doc->setMode(Doc::Design);

    FixtureManager *fm = FixtureManager::instance();
    QVERIFY(fm != nullptr);
    fm->updateView();
    Monitor *mon = Monitor::instance();
    QVERIFY(mon != nullptr);

    // Every fixture's row in the Fixture Manager (not a head row).
    QHash<quint32, QTreeWidgetItem *> rows;
    for (QTreeWidgetItemIterator it(fm->m_fixtures_tree); *it; ++it)
    {
        const QVariant v = (*it)->data(0, PROP_ID);
        if (v.isValid() && !(*it)->data(0, PROP_HEAD).isValid() && !rows.contains(v.toUInt()))
            rows.insert(v.toUInt(), *it);
    }

    /* Each opening's modal: its title, and whether its position rows are
       greyed. Rejected, so nothing is changed. */
    QString title;
    bool posEnabled = false;
    bool offPlotNote = false;
    QTimer watch;
    watch.setInterval(5);
    QObject::connect(&watch, &QTimer::timeout, [&]() {
        QDialog *d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (d == nullptr)
            return;
        title = d->windowTitle();
        QWidget *x = d->findChild<QWidget *>(QStringLiteral("fixtureX"));
        posEnabled = x != nullptr && x->isEnabled();
        offPlotNote = d->findChild<QWidget *>(QStringLiteral("fixtureNotOnPlot")) != nullptr;
        d->reject();
    });
    watch.start();

    QStringList failed;
    int offPlot = 0;
    const QList<Fixture *> fixtures = doc->fixtures();
    for (Fixture *fx : fixtures)
    {
        const quint32 fid = fx->id();
        QTreeWidgetItem *row = rows.value(fid, nullptr);
        if (row == nullptr)
        {
            failed << QStringLiteral("%1 '%2': no row in the Fixture Manager").arg(fid).arg(fx->name());
            continue;
        }
        const bool onPlot = mon->m_graphicsView->fixtureItemForId(fid) != nullptr;
        offPlot += onPlot ? 0 : 1;

        // A double-click on its row.
        title.clear();
        fm->slotDoubleClicked(row);
        if (!title.startsWith(QStringLiteral("Fixture Properties")))
            failed << QStringLiteral("%1 '%2' (%3): double-click opened '%4'")
                          .arg(fid).arg(fx->name()).arg(onPlot ? "on the plot" : "not on the plot").arg(title);
        else if (posEnabled == !onPlot || offPlotNote == onPlot)
            failed << QStringLiteral("%1 '%2': position rows %3 for a fixture %4")
                          .arg(fid).arg(fx->name()).arg(posEnabled ? "enabled" : "greyed")
                          .arg(onPlot ? "on the plot" : "not on the plot");

        // Its right-click ▸ Fixture Properties… (the same action), for the first one.
        if (fid == fixtures.first()->id())
        {
            fm->m_fixtures_tree->clearSelection();
            fm->m_fixtures_tree->setCurrentItem(row);
            row->setSelected(true);
            title.clear();
            QVERIFY(fm->m_propertiesAction->isEnabled());
            fm->m_propertiesAction->trigger();
            if (!title.startsWith(QStringLiteral("Fixture Properties")))
                failed << QStringLiteral("%1: Fixture Properties… opened '%2'").arg(fid).arg(title);
        }
    }
    watch.stop();
    qDebug() << fixtures.size() << "fixtures," << offPlot << "not on the plot";
    QVERIFY2(failed.isEmpty(), qPrintable(failed.mid(0, 12).join(QStringLiteral("\n  "))));
}

QTEST_MAIN(ShowFixtureProperties_Test)
