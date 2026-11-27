/*
  Q Light Controller Plus - qlcconsole
  surfacemap.h

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

#ifndef SURFACEMAP_H
#define SURFACEMAP_H

#include <QWidget>
#include <QHash>
#include <QList>
#include <QMap>
#include <QElapsedTimer>

#include "surfacelayout.h"
#include "qlcui_export.h"

class QVBoxLayout;
class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui_io
 * @{
 */

/**
 * One control surface drawn as it is (docs/design/CONTROL_SCOPES.md section
 * 3, Q311): each control of its SurfaceLayout where it sits on the board,
 * labelled with its job now, in one of four looks --
 *  - Desk: this computer's STOP, Grand Master, GO or Back (DeskRecord), the
 *    strongest look, the role's name on it;
 *  - Board: what the board's own overlay does (the PMJ's), locked;
 *  - Show: what this show gives it (ControlOwners::ownersOn(): a trigger, a
 *    hit by place, a Virtual Console input, the encoder bank, a speed
 *    master, the show's Grand Master or emergency stop); a trigger whose
 *    place is not live now (a hit armed only in another cue) dimmer, its
 *    place in the tooltip;
 *  - Page: in Design, what the board's surface page on now gives it
 *    (SurfacePages: "Pan fine", a colour fader in its colour, the page
 *    keys); a show job on it is named as kept from it until Operate;
 *  - Free.
 * Live: a control pressed or moved on the board lights here, a fader or a
 * knob with its value; each lamp shows what the app last sent the board
 * (InputOutputMap::lastFeedback()). Read only. GUI thread.
 */
class QLC_UI_EXPORT SurfaceMapView final : public QWidget
{
    Q_OBJECT

public:
    enum Group { Free, Desk, Board, Show, Page };
    Q_ENUM(Group)

    SurfaceMapView(Doc *doc, quint32 universe, const SurfaceLayout &layout, QWidget *parent = nullptr);

    quint32 universe() const { return m_universe; }
    const SurfaceLayout &layout() const { return m_layout; }

    /* What each control shows (by any channel it hears); for the tests. */
    Group group(quint32 channel) const;
    /** "GO", "Scene: Warm", "" (free) */
    QString jobText(quint32 channel) const;
    QString toolTipFor(quint32 channel) const;
    /** The desk role key on it ("go"), shown in the role's look; empty: none. */
    QString role(quint32 channel) const;
    /** A show job whose place is not live now (drawn dimmer). */
    bool isDim(quint32 channel) const;
    /** Pressed or moved in the last moments. */
    bool isHighlighted(quint32 channel) const;
    /** The last value the board sent (0..255); -1: none yet. */
    int value(quint32 channel) const;
    /** The last value the app sent its light (0..255); -1: none. */
    int lamp(quint32 channel) const;
    /** The control's rectangle on the widget now. */
    QRect cellRect(quint32 channel) const;
    /** A label of the active surface page (the surface pages design), in
     *  place of a free control's own name; empty clears it. */
    void setPageLabels(const QHash<quint32, QString> &labels);
    /** The colour a page control is drawn in (a colour fader's); invalid:
     *  the page's own. For the tests. */
    QColor pageColour(quint32 channel) const;
    /** Pixels per grid unit now. */
    double scale() const;

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /** The smallest and largest grid unit drawn (px): below the smallest
     *  the map asks for its width and its page scrolls, never clipped. */
    static constexpr int kMinCell = 40;
    static constexpr int kMaxCell = 76;

public slots:
    /** Read every control's job again. */
    void refresh();
    /** A value from the board (InputOutputMap::inputValueChanged). */
    void noteInput(quint32 universe, quint32 channel, uchar value);
    /** A value sent to the board's light (InputOutputMap::feedbackSent). */
    void noteLamp(quint32 universe, quint32 channel, uchar value);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Cell
    {
        Group group = Free;
        QString role;
        QString job;
        QString tip;
        bool dim = false;
        QColor colour;      //!< a page's colour control: its colour
        int value = -1;
        int lamp = -1;
        qint64 movedAt = -1;
    };
    int cellIndex(quint32 channel) const { return m_layout.indexOf(channel); }
    QRectF toWidget(const QRectF &grid) const;
    QPointF origin() const;
    void paintCell(QPainter &p, int i);

private:
    Doc *m_doc;
    quint32 m_universe;
    SurfaceLayout m_layout;
    QList<Cell> m_cells;
    QHash<quint32, QString> m_pageLabels;
    QElapsedTimer m_clock;
    QTimer *m_fade;
};

/**
 * The hub's surface maps (Connections ▸ Control surfaces, Q311): for each
 * universe whose input is a surface with a layout, its name, the legend,
 * where GO, Back, STOP and the Grand Master are on it ("GO is PLAY") with
 * Set Up… for a role it lacks, and its SurfaceMapView. A surface patched
 * without a layout says so in one line: its jobs stay in the hub's rows.
 */
class QLC_UI_EXPORT SurfaceMaps final : public QWidget
{
    Q_OBJECT

public:
    explicit SurfaceMaps(Doc *doc, QWidget *parent = nullptr);

    /** The drawn boards, in universe order. */
    QList<SurfaceMapView *> views() const { return m_views.values(); }
    SurfaceMapView *view(quint32 universe) const { return m_views.value(universe); }
    /** "GO is PLAY · Back is RECORD · ..." for @p universe's board. */
    QString roleLine(quint32 universe) const;
    /** "Pages: Build · on now: Move (Pan, Sends pick the page)" of
     *  @p universe's board; empty when it has no page set. */
    QString pageLine(quint32 universe) const;
    /** The offer of the show's own page set; empty when none. */
    QString offerLine(quint32 universe) const;
    /** Export @p universe's page set to @p path / import one (the Export…
     *  and Import… buttons, without their file dialogs: the tests). An
     *  import that passes is chosen for the board. False: @p error says why. */
    bool exportPageSet(quint32 universe, const QString &path, QString *error = nullptr) const;
    bool importPageSet(quint32 universe, const QString &path, QString *error = nullptr);
    /** The lines of surfaces patched with no layout. */
    QStringList fallbackLines() const;

    /** Every universe whose input is a surface with a layout. */
    static QList<quint32> surfaceUniverses(Doc *doc);

public slots:
    /** Build again (a patch, a profile, a load). */
    void rebuild();
    /** Read every map's jobs again. */
    void refresh();
    /** Scroll to @p universe's map (the footer's chip). */
    void showMap(quint32 universe);
    /** The footer chip's click: that board's map alone (the others hidden,
     *  an "All boards" button to bring them back); the filter ends when the
     *  page is left. Connections ▸ Control surfaces lists every board. */
    void showOnly(quint32 universe);
    /** The universe shown alone; invalidUniverse() while every board is. */
    quint32 onlyUniverse() const { return m_only; }
    void showAll();
    bool isBoardShown(quint32 universe) const;

signals:
    /** Set Up GO… (a DeskRecord::Role): the desk's own Learn. */
    void deskLearnRequested(int role);
    /** Set Up… in a board's options: its controller setup. */
    void controllerSetupRequested(quint32 universe);
    /** Lamp Test… in a board's options (a board that has one). */
    void lampTestRequested(quint32 universe);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    void applyFilter();
    QWidget *buildBlock(quint32 universe, const SurfaceLayout &layout);
    void addPagesRow(QVBoxLayout *v, QWidget *block, quint32 universe);
    void addOptionsRow(QVBoxLayout *v, QWidget *block, quint32 universe, const SurfaceLayout &layout);
    /** The pages row's words and the offer, as they are now. */
    void refreshPages();

private:
    Doc *m_doc;
    QVBoxLayout *m_box;
    QMap<quint32, SurfaceMapView *> m_views;
    QMap<quint32, QLabel *> m_roleLines;
    QMap<quint32, QLabel *> m_pageLines;
    QMap<quint32, QWidget *> m_offers;
    QMap<quint32, class QComboBox *> m_setBoxes;
    QMap<quint32, QPair<class QCheckBox *, class QCheckBox *>> m_guardBoxes;
    QMap<quint32, QWidget *> m_blocks;
    QList<QWidget *> m_parts;
    QStringList m_fallback;
    bool m_rebuildQueued = false;
    quint32 m_only = 0xFFFFFFFF;
    QMap<quint32, class QPushButton *> m_allButtons;
};

/** @} */

#endif
