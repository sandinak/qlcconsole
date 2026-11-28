/*
  Q Light Controller Plus
  plotsheet.h

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

#ifndef PLOTSHEET_H
#define PLOTSHEET_H

#include <QCoreApplication>
#include <QPageSize>
#include <QString>
#include <QColor>
#include <QImage>
#include <QDate>
#include <QList>
#include <QRectF>

#include "structurestudioview.h"
#include "rigowner.h"

class MonitorGraphicsView;
class QPagedPaintDevice;
class QPainter;
class Doc;

/** @addtogroup ui_mon DMX Monitor
 * @{
 */

/**
 * The plot as a drawing on paper (capture study 2026-11-27, item 2): the
 * Lighting Studio's plan, front or side elevation, or the Rig Overview's
 * angled look, framed with
 *
 * - a title block (show, venue, drawn by, date, view, sheet),
 * - a symbol key: one row per kind of fixture on the plot, its symbol as the
 *   plot draws it and how many there are,
 * - a layer and owner key (fixtures per plot layer, per owner),
 * - a scale bar in Settings > Units (MeasurementUnits).
 *
 * One painter path for the PDF, the printer and the dialog's preview, laid
 * out in points (1/72 inch) on the page's printable area.
 */
class PlotSheet final
{
    Q_DECLARE_TR_FUNCTIONS(PlotSheet)

public:
    enum View { TopView = 0, FrontView = 1, SideView = 2, OverviewView = 3 };

    /** Where the drawing comes from. */
    struct Context
    {
        /** The Lighting Studio's plot (Top/Front/Side are drawn from it). */
        MonitorGraphicsView *view = nullptr;
        /** The plot shows the Owner overlay: the owner key gets its colours. */
        bool ownerColours = false;
        /** The open Rig Overview's camera, so the angled view prints as seen. */
        StructureStudioView::Camera camera;
        bool haveCamera = false;
    };

    /** What the person chose in the dialog. */
    struct Options
    {
        QString title;
        QString venue;
        QString drawnBy;
        QString revision;
        QDate date = QDate::currentDate();
        View view = TopView;
        QPageSize::PageSizeId page = QPageSize::Letter;
        bool symbolKey = true;
        bool layerKey = true;
    };

    struct KeyRow
    {
        QString type;       ///< "Manufacturer Model"
        QString look;       ///< the plot symbol's name, empty when drawn by kind
        int count = 0;
        quint32 sampleFid = 0;
    };
    struct LayerRow
    {
        QString name;
        int fixtures = 0;
        bool visible = true;
    };
    struct OwnerRow
    {
        RigOwner::Owner owner = RigOwner::Tour;
        int count = 0;
        QColor colour;
    };

    PlotSheet(Doc *doc, const Context &context, const Options &options);

    static QString viewName(View view);
    /** True when @p view can be drawn from @p context. */
    static bool canDraw(const Context &context, View view);

    const Options &options() const { return m_options; }
    QList<KeyRow> symbolKey() const { return m_symbols; }
    QList<LayerRow> layerKey() const { return m_layers; }
    QList<OwnerRow> ownerKey() const { return m_owners; }
    /** Fixtures drawn on the sheet (the symbol key's total). */
    int fixtureCount() const;
    /** Fixtures on the plot that a hidden layer keeps off the sheet. */
    int hiddenCount() const { return m_hidden; }
    /** Of fixtureCount(), those the plot draws as the count on their
     *  structure (they bury it, or Fixtures on the Rig is off). */
    int asCountCount() const { return m_asCount; }
    /** The key's line that says so ("12 are drawn as the count on their structure"). */
    QString asCountNote() const;

    /** The page, landscape, in points, less the margins. */
    QSizeF pageSizePoints() const;

    /** The scale bar on this page: its length in mm of stage and its label
     *  (MeasurementUnits::formatLength in Settings > Units). Zero / empty for
     *  the angled view, which is not to scale. */
    double scaleBarMm() const;
    QString scaleBarLabel() const;

    /** Paint the sheet with @p painter set up so that one unit is one point
     *  and (0,0) is the top-left of the printable area. */
    void paint(QPainter *painter) const;
    /** Paint on a PDF writer or a printer, one page. */
    void paintTo(QPagedPaintDevice *device) const;
    /** Write the PDF. False (with @p error set) when the file can't be written. */
    bool writePdf(const QString &path, QString *error = nullptr) const;
    /** The sheet as a picture @p width pixels wide (the dialog's preview). */
    QImage preview(int width) const;

private:
    struct Layout
    {
        QRectF drawing;     ///< the frame the plot is drawn in
        QRectF plot;        ///< where the plot itself lands inside it
        QRectF source;      ///< the scene rect drawn there
        QRectF side;        ///< the key and title column
        double pointsPerMm = 0;   ///< points of paper per mm of stage
        double barMm = 0;
    };
    Layout layout() const;
    void collect();
    QImage overviewImage(const QSize &size) const;
    void paintDrawing(QPainter *p, const Layout &l) const;
    void paintScaleBar(QPainter *p, const Layout &l) const;
    void paintSide(QPainter *p, const Layout &l) const;
    void paintSymbol(QPainter *p, const QRectF &cell, quint32 fid) const;

    Doc *m_doc;
    Context m_context;
    Options m_options;
    QList<KeyRow> m_symbols;
    QList<LayerRow> m_layers;
    QList<OwnerRow> m_owners;
    int m_hidden = 0;
    int m_asCount = 0;
    QRectF m_sceneRect;     ///< what the drawing frames, in scene units
    double m_sceneUnitsPerMm = 0;
};

/** @} */

#endif
