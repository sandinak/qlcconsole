/*
  Q Light Controller Plus - Unit test
  plotsheet_test.cpp

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

/* The plot as a drawing (capture study 2026-11-27, item 2): a real show's
   plot -- test-workspaces/stage-structures-demo.qxw -- rendered to a PDF
   offscreen, and what is on the page checked: the title block, the symbol
   key and its counts, the scale bar in Settings > Units (feet and inches by
   default). */

#include <QtTest>
#include "../common/fixtureeditor.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPaintEngine>
#include <QPainter>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <climits>
#include <cmath>
#include <functional>

#ifdef PLOTSHEET_HAVE_QTPDF
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfSelection>
#endif

#include "plotsheet_test.h"

#include "../../../engine/test/common/stdprelude.h"
#define private public
#define protected public
#include "app.h"
#include "monitor.h"
#include "plotprintdialog.h"
#undef protected
#undef private
#include "monitorgraphicsview.h"
#include "monitorproperties.h"
#include "truss.h"
#include "measurementunits.h"
#include "plotsheet.h"
#include "qlcfixturedef.h"
#include "fixture.h"
#include "doc.h"

typedef MeasurementUnits MU;

namespace
{
/* A paint device that draws nothing and writes down every piece of text it
   is asked to draw (monitor_test's probe): the words on the sheet, on every
   host alike, whatever fonts it has. */
class TextProbeEngine : public QPaintEngine
{
public:
    QStringList texts;
    TextProbeEngine() : QPaintEngine(QPaintEngine::AllFeatures) {}
    bool begin(QPaintDevice *) override { return true; }
    bool end() override { return true; }
    void updateState(const QPaintEngineState &) override {}
    void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override {}
    void drawImage(const QRectF &, const QImage &, const QRectF &, Qt::ImageConversionFlags) override {}
    void drawTiledPixmap(const QRectF &, const QPixmap &, const QPointF &) override {}
    void drawPath(const QPainterPath &) override {}
    void drawPolygon(const QPointF *, int, PolygonDrawMode) override {}
    void drawPolygon(const QPoint *, int, PolygonDrawMode) override {}
    void drawRects(const QRectF *, int) override {}
    void drawRects(const QRect *, int) override {}
    void drawLines(const QLineF *, int) override {}
    void drawLines(const QLine *, int) override {}
    void drawPoints(const QPointF *, int) override {}
    void drawPoints(const QPoint *, int) override {}
    void drawEllipse(const QRectF &) override {}
    void drawEllipse(const QRect &) override {}
    void drawTextItem(const QPointF &, const QTextItem &ti) override { texts << ti.text(); }
    Type type() const override { return QPaintEngine::User; }
};

class TextProbeDevice : public QPaintDevice
{
public:
    explicit TextProbeDevice(const QSize &size) : m_size(size) {}
    QPaintEngine *paintEngine() const override { return &m_engine; }
    QStringList texts() const { return m_engine.texts; }
protected:
    int metric(PaintDeviceMetric m) const override
    {
        switch (m)
        {
        case PdmWidth:            return m_size.width();
        case PdmHeight:           return m_size.height();
        case PdmWidthMM:          return m_size.width() / 4;
        case PdmHeightMM:         return m_size.height() / 4;
        case PdmNumColors:        return INT_MAX;
        case PdmDepth:            return 32;
        case PdmDpiX: case PdmDpiY: case PdmPhysicalDpiX: case PdmPhysicalDpiY:
                                  return 72;
        case PdmDevicePixelRatio: return 1;
        case PdmDevicePixelRatioScaled: return int(QPaintDevice::devicePixelRatioFScale());
        default:                  return 0;
        }
    }
private:
    QSize m_size;
    mutable TextProbeEngine m_engine;
};

/* Every word the sheet paints, as the PDF gets them (one painter path). */
QStringList sheetTexts(const PlotSheet &sheet)
{
    TextProbeDevice dev(sheet.pageSizePoints().toSize());
    QPainter p(&dev);
    sheet.paint(&p);
    p.end();
    return dev.texts();
}

/* The text of the PDF's one page, where Qt has its PDF module. */
QString pdfText(const QString &path, int *pages)
{
#ifdef PLOTSHEET_HAVE_QTPDF
    QPdfDocument doc;
    doc.load(path);
    *pages = doc.status() == QPdfDocument::Status::Ready ? doc.pageCount() : -1;
    return *pages > 0 ? doc.getAllText(0).text() : QString();
#else
    Q_UNUSED(path)
    *pages = 0;
    return QString();
#endif
}

void settle(int ms = 60)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

/* Close whatever a show's loading asks (a missing definition, an old show):
   a test never waits for a click. */
void closeModalsFor(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms)
    {
        QWidget *w = modalOrFixtureEditor();
        if (w != nullptr && qobject_cast<QProgressDialog *>(w) == nullptr)
        {
            if (QDialog *d = qobject_cast<QDialog *>(w))
                d->reject();
            else
                w->close();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

Monitor *studio(App *app)
{
    for (int i = 0; i < app->m_tab->count(); ++i)
        if (app->m_tab->tabBar()->tabData(i).toString() == QLatin1String("Lighting Studio"))
        {
            app->m_tab->setCurrentIndex(i);
            break;
        }
    settle();
    return Monitor::instance();
}

PlotSheet::Context contextOf(Monitor *mon)
{
    PlotSheet::Context c;
    c.view = mon->m_graphicsView;
    return c;
}

PlotSheet::Options optionsFor(PlotSheet::View view)
{
    PlotSheet::Options o;
    o.title = QStringLiteral("Structures Demo Plot");
    o.venue = QStringLiteral("Pine Mountain School");
    o.drawnBy = QStringLiteral("Test LD");
    o.revision = QStringLiteral("B");
    o.view = view;
    o.page = QPageSize::Letter;
    return o;
}

/* Feet and whole inches as formatLength() writes them: 10', 2' 6". */
const QRegularExpression kFeet(QStringLiteral("^\\d+'( \\d+( \\d/\\d)?\")?$"));
}

void PlotSheet_Test::initTestCase()
{
    MU::setPreferredLength(MU::Feet);
    m_app = new App();
    m_app->init();
    m_app->resize(1440, 900);
    m_app->show();
    settle();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString src = QStringLiteral(PLOTSHEET_WORKSPACES "stage-structures-demo.qxw");
    QVERIFY2(QFileInfo::exists(src), qPrintable(src));
    const QString copy = dir.filePath(QStringLiteral("stage-structures-demo.qxw"));
    QVERIFY(QFile::copy(src, copy));
    QTimer closer;
    closer.setInterval(50);
    connect(&closer, &QTimer::timeout, []() {
        QWidget *w = modalOrFixtureEditor();
        if (QDialog *d = qobject_cast<QDialog *>(w))
            if (qobject_cast<QProgressDialog *>(d) == nullptr)
                d->reject();
    });
    closer.start();
    m_app->clearDocument();
    QCOMPARE(m_app->loadXML(copy), QFile::NoError);
    closer.stop();
    closeModalsFor(60);
    m_app->doc()->setMode(Doc::Design);
    QVERIFY(!m_app->doc()->fixtures().isEmpty());
    Monitor *mon = studio(m_app);
    QVERIFY(mon != nullptr);
    settle(200);
    QVERIFY(!mon->m_graphicsView->fixtureItems().isEmpty());
}

void PlotSheet_Test::cleanupTestCase()
{
    delete m_app;
    m_app = nullptr;
}

void PlotSheet_Test::thePlanPdfHasTitleKeyCountsAndScaleBar()
{
    MU::setPreferredLength(MU::Feet);
    Monitor *mon = studio(m_app);
    const PlotSheet sheet(m_app->doc(), contextOf(mon), optionsFor(PlotSheet::TopView));

    QVERIFY(sheet.fixtureCount() > 0);
    QVERIFY(!sheet.symbolKey().isEmpty());
    const QString bar = sheet.scaleBarLabel();
    QVERIFY2(kFeet.match(bar).hasMatch(), qPrintable(bar));   // feet by default
    QVERIFY(sheet.scaleBarMm() > 0);

    // The PDF: one page, written offscreen.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("plot.pdf"));
    QString error;
    QVERIFY2(sheet.writePdf(path, &error), qPrintable(error));
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray pdf = f.readAll();
    QVERIFY(pdf.startsWith("%PDF"));
    QVERIFY2(pdf.size() > 20000, qPrintable(QString::number(pdf.size())));   // a drawing, not a blank page
    QVERIFY(pdf.contains("/Count 1"));

    // What it says: the title block, the key's counts, the scale bar.
    const QStringList texts = sheetTexts(sheet);
    const QString all = texts.join(QLatin1Char('\n'));
    for (const QString &want : { QStringLiteral("Structures Demo Plot"), QStringLiteral("Pine Mountain School"),
                                 QStringLiteral("Test LD"), QStringLiteral("Symbol key"),
                                 QStringLiteral("Total fixtures"), QStringLiteral("Layers and owners"),
                                 QStringLiteral("Plan (top)"), bar, QStringLiteral("0") })
        QVERIFY2(texts.contains(want), qPrintable(QStringLiteral("no \"%1\" on the sheet:\n%2").arg(want, all)));
    QVERIFY2(texts.contains(MU::formatLength(sheet.scaleBarMm() / 2)), qPrintable(all));
    QVERIFY2(texts.contains(QLocale().toString(sheet.fixtureCount())), qPrintable(all));
    for (const PlotSheet::KeyRow &r : sheet.symbolKey())
        QVERIFY2(texts.contains(QLocale().toString(r.count)), qPrintable(r.type));

    // And the PDF's own text, where Qt can read a PDF back.
    int pages = 0;
    const QString text = pdfText(path, &pages);
    if (pages != 0)
    {
        QCOMPARE(pages, 1);
        for (const QString &want : { QStringLiteral("Structures Demo Plot"), QStringLiteral("Symbol key"),
                                     QStringLiteral("Total fixtures"), bar,
                                     QLocale().toString(sheet.fixtureCount()) })
            QVERIFY2(text.contains(want), qPrintable(QStringLiteral("no \"%1\" in the PDF:\n%2").arg(want, text)));
    }
    else
    {
        qInfo("Qt PDF module not here: checked what the sheet painted");
    }

    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    if (!shots.isEmpty())
    {
        sheet.preview(1650).save(QDir(shots).filePath(QStringLiteral("plotsheet-plan-letter.png")));
        QVERIFY(QFile::copy(path, QDir(shots).filePath(QStringLiteral("plotsheet-plan-letter.pdf"))));
    }
}

void PlotSheet_Test::theKeyCountsWhatIsDrawn()
{
    Monitor *mon = studio(m_app);
    Doc *doc = m_app->doc();
    MonitorGraphicsView *view = mon->m_graphicsView;
    const PlotSheet sheet(doc, contextOf(mon), optionsFor(PlotSheet::TopView));

    // Every fixture the plot shows, once; by definition, as many as the key says.
    QHash<QString, int> want;
    int total = 0;
    for (quint32 fid : view->fixtureItems().keys())
    {
        Fixture *fx = doc->fixture(fid);
        if (fx == nullptr)
            continue;
        // Drawn, or drawn as the count on its structure (mounted, buried).
        const bool mounted = doc->monitorProperties()->fixtureRigProps(fid).primaryMount()
                             != FixtureRigProps::NoMount;
        if (!view->fixtureItemShouldBeVisible(fid) && !mounted)
            continue;
        ++total;
        // The key's name holds the model, or the kind for a generic.
        QString model = fx->fixtureDef() != nullptr ? fx->fixtureDef()->model() : QString();
        if (model.isEmpty() || model.compare(QLatin1String("Generic"), Qt::CaseInsensitive) == 0)
            model = fx->typeString();
        want[model]++;
    }
    QCOMPARE(sheet.fixtureCount(), total);
    // stage-structures-demo's trusses are covered by their lights: the plot
    // draws them as a count, and the key counts them and says so.
    QVERIFY2(sheet.asCountCount() > 0, "the demo's buried lights were not counted");
    QVERIFY(sheet.fixtureCount() > sheet.asCountCount());
    QVERIFY2(sheetTexts(sheet).contains(sheet.asCountNote()),
             qPrintable(sheetTexts(sheet).join(" | ")));
    QHash<QString, int> got;
    for (const PlotSheet::KeyRow &r : sheet.symbolKey())
    {
        QVERIFY(r.count > 0);
        QVERIFY(view->fixtureItemForId(r.sampleFid) != nullptr);
        bool matched = false;
        for (auto it = want.constBegin(); it != want.constEnd(); ++it)
            if (!it.key().isEmpty() && r.type.contains(it.key()))
            {
                got[it.key()] += r.count;
                matched = true;
                break;
            }
        QVERIFY2(matched, qPrintable(r.type));
    }
    QCOMPARE(got, want);

    // Sorted most first; the layer key adds up to the same total.
    for (int i = 1; i < sheet.symbolKey().size(); ++i)
        QVERIFY(sheet.symbolKey().at(i - 1).count >= sheet.symbolKey().at(i).count);
    int inLayers = 0;
    for (const PlotSheet::LayerRow &l : sheet.layerKey())
        inLayers += l.fixtures;
    QCOMPARE(inLayers, total);
    int owned = 0;
    for (const PlotSheet::OwnerRow &o : sheet.ownerKey())
        owned += o.count;
    QCOMPARE(owned, total);
}

void PlotSheet_Test::aHiddenLayerIsLeftOffAndSaidSo()
{
    Monitor *mon = studio(m_app);
    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    const PlotSheet before(doc, contextOf(mon), optionsFor(PlotSheet::TopView));
    QVERIFY(!before.layerKey().isEmpty());
    PlotSheet::LayerRow most = before.layerKey().first();
    for (const PlotSheet::LayerRow &l : before.layerKey())
        if (l.fixtures > most.fixtures)
            most = l;
    QVERIFY(most.fixtures > 0);
    quint32 id = UINT_MAX;
    for (const MonitorProperties::MonitorLayer &l : props->layers())
        if (l.name == most.name)
            id = l.id;
    QVERIFY(id != UINT_MAX);

    props->setLayerVisible(id, false);
    auto restore = qScopeGuard([props, id]() { props->setLayerVisible(id, true); });
    const PlotSheet after(doc, contextOf(mon), optionsFor(PlotSheet::TopView));
    QCOMPARE(after.hiddenCount(), most.fixtures);
    QCOMPARE(after.fixtureCount(), before.fixtureCount() - most.fixtures);
    const QStringList texts = sheetTexts(after);
    QVERIFY2(texts.contains(QStringLiteral("Not drawn (on a hidden layer)")), qPrintable(texts.join(" | ")));
    QVERIFY(texts.contains(QStringLiteral("hidden")));
}

void PlotSheet_Test::frontAndOverviewPrintAndTheStudioIsUnchanged()
{
    MU::setPreferredLength(MU::Feet);
    Monitor *mon = studio(m_app);
    MonitorGraphicsView *view = mon->m_graphicsView;
    QCOMPARE(view->viewPOV(), MonitorGraphicsView::PovTop);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");

    // The front elevation: to scale, from the same plot.
    const PlotSheet front(m_app->doc(), contextOf(mon), optionsFor(PlotSheet::FrontView));
    QVERIFY2(kFeet.match(front.scaleBarLabel()).hasMatch(), qPrintable(front.scaleBarLabel()));
    QString error;
    const QString frontPdf = dir.filePath(QStringLiteral("front.pdf"));
    QVERIFY2(front.writePdf(frontPdf, &error), qPrintable(error));
    QVERIFY(QFileInfo(frontPdf).size() > 10000);
    QVERIFY(sheetTexts(front).contains(QStringLiteral("Front elevation")));
    QCOMPARE(view->viewPOV(), MonitorGraphicsView::PovTop);    // printing changed nothing

    // The Rig Overview's angled look: a picture, said to be not to scale.
    const PlotSheet angled(m_app->doc(), contextOf(mon), optionsFor(PlotSheet::OverviewView));
    QCOMPARE(angled.scaleBarMm(), 0.0);
    QVERIFY(angled.scaleBarLabel().isEmpty());
    const QStringList texts = sheetTexts(angled);
    QVERIFY2(texts.contains(QStringLiteral("Angled view: not to scale")), qPrintable(texts.join(" | ")));
    QCOMPARE(angled.fixtureCount(), front.fixtureCount());
    const QString angledPdf = dir.filePath(QStringLiteral("angled.pdf"));
    QVERIFY2(angled.writePdf(angledPdf, &error), qPrintable(error));
    QVERIFY(QFileInfo(angledPdf).size() > 10000);

    // An unwritable place says why.
    QVERIFY(!front.writePdf(QStringLiteral("/nonexistent-dir/x.pdf"), &error));
    QVERIFY(!error.isEmpty());

    if (!shots.isEmpty())
    {
        front.preview(1650).save(QDir(shots).filePath(QStringLiteral("plotsheet-front-letter.png")));
        angled.preview(1650).save(QDir(shots).filePath(QStringLiteral("plotsheet-overview-letter.png")));
    }
}

void PlotSheet_Test::theScaleBarFollowsSettingsUnits()
{
    Monitor *mon = studio(m_app);
    auto back = qScopeGuard([]() { MU::setPreferredLength(MU::Feet); });
    MU::setPreferredLength(MU::Metres);
    const PlotSheet metric(m_app->doc(), contextOf(mon), optionsFor(PlotSheet::TopView));
    const QString bar = metric.scaleBarLabel();
    QVERIFY2(bar.endsWith(QStringLiteral(" m")), qPrintable(bar));
    QVERIFY(sheetTexts(metric).contains(bar));
    // A round length: 1-2-5 steps of metres (or tenths).
    const double m = metric.scaleBarMm() / 1000.0;
    QVERIFY2(qFuzzyCompare(m * 10, qRound(m * 10) + 0.0), qPrintable(QString::number(m)));

    MU::setPreferredLength(MU::Feet);
    const PlotSheet imperial(m_app->doc(), contextOf(mon), optionsFor(PlotSheet::TopView));
    const double feet = imperial.scaleBarMm() / 304.8;          // whole feet
    QVERIFY2(qAbs(feet - qRound(feet)) < 1e-6 && feet >= 1, qPrintable(QString::number(feet)));
}

void PlotSheet_Test::theStudioMenuOpensTheDialog()
{
    MU::setPreferredLength(MU::Feet);
    Monitor *mon = studio(m_app);
    QVERIFY(mon->m_moreBtn != nullptr && mon->m_moreBtn->menu() != nullptr);

    // More > Print & Reports > Print Plot / Save PDF… (one submenu for the
    // plot's paperwork: the More menu itself got shorter).
    QMenu *print = mon->m_moreBtn->menu()->findChild<QMenu *>(QStringLiteral("plotPrintMenu"));
    QVERIFY(print != nullptr);
    QStringList inMore;
    for (QAction *a : mon->m_moreBtn->menu()->actions())
        inMore << a->text();
    QVERIFY2(!inMore.contains(QStringLiteral("Network Report…")) && !inMore.contains(QStringLiteral("Patch List…")),
             qPrintable(inMore.join(" | ")));
    QStringList inPrint;
    for (QAction *a : print->actions())
        inPrint << a->text();
    QCOMPARE(inPrint, QStringList({ QStringLiteral("Print Plot / Save PDF…"), QStringLiteral("Patch List…"),
                                    QStringLiteral("Network Report…") }));
    QAction *act = print->findChild<QAction *>(QStringLiteral("plotPdfAction"));
    QVERIFY(act == nullptr || act->isEnabled());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("menu.pdf"));
    const QString shots = qEnvironmentVariable("QLC_SHOT_DIR");
    bool seen = false;
    QString summary, title, error;
    bool saved = false;
    QTimer poll;
    poll.setInterval(10);
    connect(&poll, &QTimer::timeout, this, [&]() {
        PlotPrintDialog *dlg = qobject_cast<PlotPrintDialog *>(modalOrFixtureEditor());
        if (dlg == nullptr)
            return;
        poll.stop();
        seen = true;
        title = dlg->windowTitle();
        QLabel *sum = dlg->findChild<QLabel *>(QStringLiteral("plotSummary"));
        summary = sum != nullptr ? sum->text() : QString();
        dlg->findChild<QLineEdit *>(QStringLiteral("plotTitle"))->setText(QStringLiteral("From the menu"));
        saved = dlg->savePdf(path, &error);
        if (!shots.isEmpty())
        {
            for (const QSize &sz : { QSize(1024, 640), QSize(1440, 900), QSize(1920, 1080) })
            {
                dlg->resize(qMin(sz.width() - 40, 1100), qMin(sz.height() - 60, 720));
                dlg->slotRefresh();
                settle(30);
                QPixmap canvas(sz);
                canvas.fill(QColor(40, 40, 44));
                QPainter p(&canvas);
                p.drawPixmap(QPoint((sz.width() - dlg->width()) / 2, (sz.height() - dlg->height()) / 2), dlg->grab());
                p.end();
                canvas.save(QDir(shots).filePath(QStringLiteral("plotprintdialog-%1x%2.png")
                                                     .arg(sz.width()).arg(sz.height())));
            }
        }
        dlg->reject();
    });
    poll.start();
    QVERIFY(act != nullptr);
    act->trigger();
    poll.stop();
    QVERIFY(seen);
    QCOMPARE(title, QStringLiteral("Print Plot"));
    // The summary says how many and the scale, in feet.
    QVERIFY2(summary.contains(QStringLiteral("fixture")) && summary.contains(QStringLiteral("scale bar ")),
             qPrintable(summary));
    QVERIFY2(summary.contains(QLatin1Char('\'')), qPrintable(summary));
    QVERIFY2(saved, qPrintable(error));
    QVERIFY(QFileInfo(path).size() > 10000);
}

QTEST_MAIN(PlotSheet_Test)
