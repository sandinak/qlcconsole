/*
  Q Light Controller Plus
  plotprintdialog.cpp

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

#include <QDialogButtonBox>
#include <QApplication>
#include <QPrintDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QFileInfo>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSettings>
#include <QPrinter>
#include <QPixmap>
#include <QLabel>
#include <QTimer>

#include "plotprintdialog.h"
#include "programmercontroller.h"
#include "venuecorrections.h"
#include "measurementunits.h"
#include "app.h"
#include "doc.h"

namespace
{
const char *const kDrawnByKey = "plotpdf/drawnBy";
const char *const kPageKey = "plotpdf/page";
const char *const kViewKey = "plotpdf/view";
const char *const kKeysKey = "plotpdf/keys";
const char *const kPaperKey = "plotpdf/paperBackground";

/* The show's file name, as the patch list finds it. */
QString showFileName()
{
    foreach (QWidget *w, qApp->topLevelWidgets())
        if (App *app = qobject_cast<App *>(w))
            return QFileInfo(app->fileName()).completeBaseName();
    return QString();
}
}

PlotPrintDialog::PlotPrintDialog(Doc *doc, const PlotSheet::Context &context, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_context(context)
{
    setObjectName(QStringLiteral("plotPrintDialog"));
    setProperty("helpTopic", QStringLiteral("plot-drawing.md"));
    setWindowTitle(tr("Print Plot"));
    resize(1000, 600);

    QSettings settings;
    QHBoxLayout *root = new QHBoxLayout;

    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_title = new QLineEdit(showFileName(), this);
    m_title->setObjectName(QStringLiteral("plotTitle"));
    m_title->setPlaceholderText(tr("Lighting plot"));
    form->addRow(tr("Title"), m_title);

    QString venue;
    if (m_doc != nullptr && m_doc->programmer() != nullptr && m_doc->programmer()->venueCorrections() != nullptr)
        venue = m_doc->programmer()->venueCorrections()->activeVenue();
    m_venue = new QLineEdit(venue, this);
    m_venue->setObjectName(QStringLiteral("plotVenue"));
    form->addRow(tr("Venue"), m_venue);

    m_drawnBy = new QLineEdit(settings.value(kDrawnByKey).toString(), this);
    m_drawnBy->setObjectName(QStringLiteral("plotDrawnBy"));
    m_drawnBy->setPlaceholderText(tr("Your name"));
    form->addRow(tr("Drawn by"), m_drawnBy);

    m_revision = new QLineEdit(this);
    m_revision->setObjectName(QStringLiteral("plotRevision"));
    m_revision->setPlaceholderText(tr("e.g. A, or 2"));
    form->addRow(tr("Revision"), m_revision);

    m_view = new QComboBox(this);
    m_view->setObjectName(QStringLiteral("plotView"));
    for (PlotSheet::View v : { PlotSheet::TopView, PlotSheet::FrontView, PlotSheet::SideView,
                               PlotSheet::OverviewView })
        if (PlotSheet::canDraw(m_context, v))
            m_view->addItem(PlotSheet::viewName(v), int(v));
    const int view = m_view->findData(settings.value(kViewKey, int(PlotSheet::TopView)).toInt());
    m_view->setCurrentIndex(qMax(0, view));
    form->addRow(tr("View"), m_view);

    m_page = new QComboBox(this);
    m_page->setObjectName(QStringLiteral("plotPage"));
    for (QPageSize::PageSizeId id : { QPageSize::Letter, QPageSize::Legal, QPageSize::Tabloid,
                                      QPageSize::A4, QPageSize::A3 })
        m_page->addItem(QPageSize(id).name(), int(id));
    const int defPage = MeasurementUnits::isImperial(MeasurementUnits::preferredLength())
                        ? int(QPageSize::Letter) : int(QPageSize::A4);
    m_page->setCurrentIndex(qMax(0, m_page->findData(settings.value(kPageKey, defPage).toInt())));
    form->addRow(tr("Paper"), m_page);

    m_background = new QComboBox(this);
    m_background->setObjectName(QStringLiteral("plotBackground"));
    m_background->addItem(tr("White (for print)"), true);
    m_background->addItem(tr("As on screen"), false);
    m_background->setToolTip(tr("White saves ink and leaves room for notes; the plot's colours keep their meaning"));
    m_background->setCurrentIndex(settings.value(kPaperKey, true).toBool() ? 0 : 1);
    form->addRow(tr("Background"), m_background);

    const int keys = settings.value(kKeysKey, 3).toInt();
    m_symbolKey = new QCheckBox(tr("Symbol key with counts"), this);
    m_symbolKey->setObjectName(QStringLiteral("plotSymbolKey"));
    m_symbolKey->setChecked(keys & 1);
    m_layerKey = new QCheckBox(tr("Layer and owner key"), this);
    m_layerKey->setObjectName(QStringLiteral("plotLayerKey"));
    m_layerKey->setChecked(keys & 2);
    form->addRow(tr("Show"), m_symbolKey);
    form->addRow(QString(), m_layerKey);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("plotSummary"));
    m_summary->setWordWrap(true);
    form->addRow(m_summary);

    QWidget *left = new QWidget(this);
    left->setLayout(form);
    left->setMinimumWidth(280);
    left->setMaximumWidth(360);
    root->addWidget(left);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("plotPreview"));
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumSize(360, 240);
    m_preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_preview->setStyleSheet(QStringLiteral("QLabel { background: #5a5d63; }"));
    m_preview->setToolTip(tr("The page as it will print"));
    root->addWidget(m_preview, 1);

    QDialogButtonBox *btns = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_saveBtn = btns->addButton(tr("Save PDF…"), QDialogButtonBox::ActionRole);
    m_saveBtn->setObjectName(QStringLiteral("plotSavePdf"));
    m_saveBtn->setToolTip(tr("Save the sheet as a PDF"));
    m_printBtn = btns->addButton(tr("Print…"), QDialogButtonBox::ActionRole);
    m_printBtn->setObjectName(QStringLiteral("plotPrint"));
    m_printBtn->setToolTip(tr("Send the sheet to a printer"));
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_saveBtn, &QPushButton::clicked, this, &PlotPrintDialog::slotSavePdf);
    connect(m_printBtn, &QPushButton::clicked, this, &PlotPrintDialog::slotPrint);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->addLayout(root, 1);
    outer->addWidget(btns);

    m_refresh = new QTimer(this);
    m_refresh->setSingleShot(true);
    m_refresh->setInterval(250);
    connect(m_refresh, &QTimer::timeout, this, &PlotPrintDialog::slotRefresh);
    for (QLineEdit *e : { m_title, m_venue, m_drawnBy, m_revision })
        connect(e, &QLineEdit::textChanged, m_refresh, QOverload<>::of(&QTimer::start));
    connect(m_view, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlotPrintDialog::slotRefresh);
    connect(m_page, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlotPrintDialog::slotRefresh);
    connect(m_background, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PlotPrintDialog::slotRefresh);
    connect(m_symbolKey, &QCheckBox::toggled, this, &PlotPrintDialog::slotRefresh);
    connect(m_layerKey, &QCheckBox::toggled, this, &PlotPrintDialog::slotRefresh);

    slotRefresh();
}

PlotSheet::Options PlotPrintDialog::options() const
{
    PlotSheet::Options o;
    o.title = m_title->text().trimmed();
    o.venue = m_venue->text().trimmed();
    o.drawnBy = m_drawnBy->text().trimmed();
    o.revision = m_revision->text().trimmed();
    o.view = PlotSheet::View(m_view->currentData().toInt());
    o.page = QPageSize::PageSizeId(m_page->currentData().toInt());
    o.symbolKey = m_symbolKey->isChecked();
    o.layerKey = m_layerKey->isChecked();
    o.paper = m_background->currentData().toBool();
    return o;
}

PlotSheet PlotPrintDialog::sheet() const
{
    return PlotSheet(m_doc, m_context, options());
}

QString PlotPrintDialog::suggestedName() const
{
    const QString show = showFileName();
    QString view;
    switch (options().view)
    {
        case PlotSheet::FrontView:    view = QStringLiteral("front"); break;
        case PlotSheet::SideView:     view = QStringLiteral("side"); break;
        case PlotSheet::OverviewView: view = QStringLiteral("overview"); break;
        default:                      view = QStringLiteral("plan"); break;
    }
    return show.isEmpty() ? QStringLiteral("plot-%1.pdf").arg(view)
                          : QStringLiteral("%1-plot-%2.pdf").arg(show, view);
}

void PlotPrintDialog::slotRefresh()
{
    const PlotSheet s = sheet();
    const int types = s.symbolKey().size();
    const int n = s.fixtureCount();
    QString text = n == 1 ? tr("1 fixture on the sheet") : tr("%1 fixtures on the sheet").arg(n);
    text += QStringLiteral(" · ") + (types == 1 ? tr("1 type") : tr("%1 types").arg(types));
    const QString bar = s.scaleBarLabel();
    text += QStringLiteral(" · ") + (bar.isEmpty() ? tr("not to scale") : tr("scale bar %1").arg(bar));
    if (s.hiddenCount() > 0)
        text += QStringLiteral("\n") + tr("%1 on a hidden layer: not drawn. Show the layer in "
                                          "Plot layers to print it.").arg(s.hiddenCount());
    m_summary->setText(text);

    const QSize room = m_preview->size().boundedTo(QSize(1400, 1000)) - QSize(16, 16);
    const QSizeF page = s.pageSizePoints();
    const double aspect = page.height() / page.width();
    int w = qMax(200, room.width());
    if (w * aspect > room.height())
        w = qMax(200, int(room.height() / aspect));
    const qreal dpr = devicePixelRatioF();
    QImage img = s.preview(int(w * dpr));
    img.setDevicePixelRatio(dpr);
    m_preview->setPixmap(QPixmap::fromImage(img));
}

void PlotPrintDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    m_refresh->start();     // the preview follows the window's size
}

bool PlotPrintDialog::savePdf(const QString &path, QString *error)
{
    remember();
    return sheet().writePdf(path, error);
}

void PlotPrintDialog::remember() const
{
    QSettings settings;
    settings.setValue(kDrawnByKey, m_drawnBy->text().trimmed());
    settings.setValue(kPageKey, m_page->currentData().toInt());
    settings.setValue(kViewKey, m_view->currentData().toInt());
    settings.setValue(kPaperKey, m_background->currentData().toBool());
    settings.setValue(kKeysKey, (m_symbolKey->isChecked() ? 1 : 0) | (m_layerKey->isChecked() ? 2 : 0));
}

void PlotPrintDialog::slotSavePdf()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Plot"), suggestedName(),
                                                      tr("PDF files (*.pdf)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!savePdf(path, &error))
        QMessageBox::warning(this, tr("Save Plot"), tr("Could not save %1:\n%2").arg(path, error));
}

void PlotPrintDialog::slotPrint()
{
    const PlotSheet s = sheet();
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(s.options().title.isEmpty() ? tr("Lighting plot") : s.options().title);
    printer.setPageSize(QPageSize(s.options().page));
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    QPrintDialog dlg(&printer, this);
    dlg.setWindowTitle(tr("Print Plot"));
    if (dlg.exec() != QDialog::Accepted)
        return;
    remember();
    s.paintTo(&printer);
}
