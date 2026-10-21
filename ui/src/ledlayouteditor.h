/*
  Q Light Controller Plus
  ledlayouteditor.h

  The LED Layout editor: a multi-head fixture (a pixel bar, a pixel box --
  the Warmdance XL-450 is 15 x 5, rows of RGB and of white) drawn as it is
  built, every head in its cell, coloured by its kind and numbered. The
  rows can be typed ("RGB, W, RGB, W, RGB"), heads swapped or moved by
  clicking, the grid flipped or resized, and a preview walks the heads in
  number order -- on the real fixture too -- so the map can be checked
  against the thing itself.

  LedLayoutEditor is the editor alone, shared by LedLayoutDialog (one
  model, reached from a fixture's menu and Fixture Properties) and the
  Split by Head Type wizard (HeadTypesDialog).

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef LEDLAYOUTEDITOR_H
#define LEDLAYOUTEDITOR_H

#include <QDialog>
#include <QWidget>
#include <QPointer>

#include "headlayout.h"

class GenericDMXSource;
class QPushButton;
class QToolButton;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QLabel;
class QTimer;
class Fixture;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/** The colour a head of @p kind is drawn in. @p shade (0..1) runs a kind
 *  that is colour (RGB, RGBW...) along a gradient, so the heads of a kind
 *  read in number order; a white, amber or UV head is that colour. */
QColor ledKindColour(const QString &kind, double shade = 0.0);

/** The LED grid of one model: a cell per head, coloured by its kind and
 *  numbered, empty cells shown. Click one head, then another to swap them,
 *  or an empty cell to move it there. */
class LedMapWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit LedMapWidget(QWidget *parent = nullptr);
    void setMap(const Fixture *like, const HeadLayout &map);
    HeadLayout map() const { return m_map; }
    /** Light @p head (the preview), with a fading trail behind it; -1: none. */
    void setLit(int head);
    int lit() const { return m_lit; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /** The cell under @p pos, or (-1,-1). */
    QPoint cellAt(const QPointF &pos) const;
    /** The head in @p cell, or -1 for an empty one. */
    int headIn(const QPoint &cell) const;
    /** Where @p cell is drawn. */
    QRectF cellRect(const QPoint &cell) const;

signals:
    void mapEdited();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    double cellSize() const;
    QPointF origin() const;

    const Fixture *m_like = nullptr;
    HeadLayout m_map;
    QVector<double> m_shade;    ///< per head: its place among its kind, 0..1
    int m_picked = -1;          ///< head clicked first, waiting for a swap
    int m_lit = -1;             ///< the preview's head
};

/** The heads in number order, one tile each in its kind's colour: what the
 *  preview walks along, and where to click to light one head. */
class LedOrderStrip final : public QWidget
{
    Q_OBJECT
public:
    explicit LedOrderStrip(QWidget *parent = nullptr);
    void setFixture(const Fixture *like);
    void setLit(int head) { m_lit = head; update(); }
    QSize sizeHint() const override { return QSize(300, 22); }

signals:
    void headClicked(int head);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QStringList m_kinds;
    QVector<double> m_shade;
    int m_lit = -1;
};

class LedLayoutEditor final : public QWidget
{
    Q_OBJECT
public:
    /** @param doc  for lighting the real fixture during the preview; may be null */
    explicit LedLayoutEditor(Doc *doc, QWidget *parent = nullptr);
    ~LedLayoutEditor() override;

    /** Edit @p layout, the map of @p like's model and mode. */
    void setFixture(const Fixture *like, const HeadLayout &layout);
    const Fixture *fixture() const { return m_like; }
    HeadLayout layout() const;

    /** The rows as typed ("RGB, W, RGB, W, RGB"). Applied when they fit:
     *  each row is filled with the next heads of its kind, in head order.
     *  When they don't, says why under the field and returns false. */
    bool applyRowPattern(const QString &text);
    /** The kind of each row of the current map, as the field shows it
     *  ("?" for a row of mixed heads). */
    QString rowPatternText() const;
    /** The reason the last row pattern was refused, or empty. */
    QString rowProblem() const;

    void flipHorizontally();
    void flipVertically();
    /** Back to the heads in number order, row by row. */
    void resetHeadOrder();
    /** The same heads on a grid of @p size (see HeadLayout::resized()). */
    bool setGridSize(const QSize &size);

    /** The order preview: walks the heads 1, 2, 3... */
    void setPreviewRunning(bool on);
    bool previewRunning() const;
    /** One step of the preview (the next head). */
    void stepPreview();
    /** Light @p head now (and on the fixture, when that is on). */
    void lightHead(int head);
    int litHead() const;

signals:
    /** The map was edited (not merely shown). */
    void layoutChanged();

private slots:
    void slotApplyRows();
    void slotGridSize();
    void slotMapEdited();

private:
    void showLayout(const HeadLayout &l, bool edited);
    void refreshControls();
    void setRowProblem(const QString &why);
    void lightOnFixture(int head);
    void stopFixtureLight();

private:
    Doc *m_doc;
    const Fixture *m_like = nullptr;
    LedMapWidget *m_grid;
    LedOrderStrip *m_strip;
    QLabel *m_legend;
    QLineEdit *m_rowsEdit;
    QLabel *m_rowNote;
    QSpinBox *m_colsSpin;
    QSpinBox *m_rowsSpin;
    QPushButton *m_previewButton;
    QCheckBox *m_lightFixture;
    QTimer *m_previewTimer;
    GenericDMXSource *m_source = nullptr;
    bool m_updating = false;
};

/** "LED Layout…": one fixture's model and mode, its map edited and kept in
 *  the show -- and, on request, as the operator's default for the model. */
class LedLayoutDialog final : public QDialog
{
    Q_OBJECT
public:
    LedLayoutDialog(Doc *doc, quint32 fixtureId, QWidget *parent = nullptr);

    /** True when @p fixtureId has a layout to edit: a real definition with
     *  more than one head. */
    static bool canEdit(Doc *doc, quint32 fixtureId);
    /** Open the editor for @p fixtureId; true when it was saved to the show. */
    static bool edit(Doc *doc, quint32 fixtureId, QWidget *parent);

    LedLayoutEditor *editor() const { return m_editor; }

public slots:
    void accept() override;
    /** Keep the map being edited as the default for this model. */
    void saveAsDefault();
    /** Load the default for this model (or head order) into the editor. */
    void useDefault();

private:
    void refreshSource();

private:
    Doc *m_doc;
    quint32 m_fid;
    QString m_key;
    LedLayoutEditor *m_editor;
    QLabel *m_source;
    QPushButton *m_saveDefault;
    QPushButton *m_useDefault;
};

/** @} */

#endif
