/*
  Q Light Controller Plus
  headtypesdialog.h

  "Split by head type": for fixtures with more than one kind of head (the
  Warmdance XL-450: rows of RGB and of white), show where each head really is
  -- the model's LED map -- let it be put right, and make a fixture group per
  kind of head, laid out as the fixtures stand on stage.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef HEADTYPESDIALOG_H
#define HEADTYPESDIALOG_H

#include <QDialog>
#include <QHash>
#include <QList>

#include "headlayout.h"

class QComboBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QHBoxLayout;
class FixtureGroup;
class Fixture;
class Doc;

/** @addtogroup ui_fixtures
 * @{
 */

/** The LED grid of one model: a cell per head, coloured by its kind and
 *  numbered. Click one cell, then another, to swap the heads in them. */
class LedMapWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit LedMapWidget(QWidget *parent = nullptr);
    void setMap(const Fixture *like, const HeadLayout &map);
    HeadLayout map() const { return m_map; }
    QSize sizeHint() const override;

signals:
    void mapEdited();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;

private:
    QRectF cellRect(const QPoint &cell) const;
    int headAt(const QPointF &pos) const;

    const Fixture *m_like = nullptr;
    HeadLayout m_map;
    int m_picked = -1;          ///< head clicked first, waiting for a swap
};

class HeadTypesDialog final : public QDialog
{
    Q_OBJECT
public:
    /** @param fixtureIds  the fixtures to group (any may be single-kind)
     *  @param baseName    the groups are named "<baseName> <kind>" */
    HeadTypesDialog(Doc *doc, const QList<quint32> &fixtureIds, const QString &baseName,
                    QWidget *parent = nullptr);

    /** The groups made by accept(), for the caller to show. */
    QList<FixtureGroup *> createdGroups() const { return m_created; }

    /** True when any of @p fixtureIds has heads of more than one kind. */
    static bool anyMixed(Doc *doc, const QList<quint32> &fixtureIds);

    /** A name for groups of @p fixtureIds: what their names share, less a
     *  trailing number ("FART-SR-1" .. "FART-SR-16" -> "FART-SR"). */
    static QString suggestName(Doc *doc, const QList<quint32> &fixtureIds);

    /** When @p fixtureIds include mixed fixtures, ask whether to split them
     *  by head type. 1 = they chose to split, and did (the groups are in
     *  @p made); 0 = they want one group as usual; -1 = cancelled. */
    static int offerSplit(Doc *doc, const QList<quint32> &fixtureIds, const QString &baseName,
                          QWidget *parent, QList<FixtureGroup *> *made = nullptr);

public slots:
    void accept() override;

private slots:
    void slotModelChanged();
    void slotApplyRows();
    void slotHeadOrder();
    void slotMapEdited();

private:
    void showModel(const QString &key);
    void fillRowCombos();
    const Fixture *likeFor(const QString &key) const;

private:
    Doc *m_doc;
    QList<quint32> m_ids;
    QHash<QString, HeadLayout> m_maps;     ///< per model key, as edited
    QHash<QString, quint32> m_like;        ///< per model key: one of its fixtures
    QComboBox *m_modelCombo;
    QLabel *m_modelInfo;
    LedMapWidget *m_grid;
    QHBoxLayout *m_rowsBox;
    QList<QComboBox *> m_rowCombos;
    QLabel *m_rowNote;
    QList<QCheckBox *> m_kindChecks;
    QList<QLineEdit *> m_kindNames;     ///< the exact name of each kind's group
    QComboBox *m_viewCombo;             ///< from the front / from above / auto
    QCheckBox *m_closeGaps;
    QList<FixtureGroup *> m_created;
};

/** @} */

#endif
