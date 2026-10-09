/*
  Q Light Controller Plus
  lightcontrols.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef LIGHTCONTROLS_H
#define LIGHTCONTROLS_H

#include <QObject>
#include <QList>

class QComboBox;
class QSpinBox;
class QWidget;
class Fixture;
class Doc;
struct FixtureRigProps;

/** @addtogroup ui_monitor
 * @{
 */

/**
 * What makes a fixture's light and what it is seen through, for the fixture
 * itself: a lantern built round one bulb, a hanging LED cluster or LED tape,
 * behind diffusing or clear glass. The definition says what the model
 * usually is (QLCPhysical); these say what THIS one is, so the rig view draws
 * it -- and runs its effects -- as it was built.
 *
 * The same two rows in the studio inspector and in Fixture Properties. The
 * definition's choice is marked "(as defined)"; choosing it stores nothing,
 * so a fixture follows its definition (and a WLED's own LED count) until it
 * is told otherwise.
 */
class LightControls final : public QObject
{
    Q_OBJECT

public:
    /** The rows' widgets, parented to @p parent: lightRow() holds the source
     *  and its LED count, glassCombo() the glass. */
    explicit LightControls(QWidget *parent);

    QWidget *lightRow() const { return m_lightRow; }
    QComboBox *glassCombo() const { return m_glass; }

    /** Whether @p fx is a fixture this is a question for: its definition
     *  makes light all round, out of LEDs, or is a lantern. */
    static bool relevant(const Fixture *fx);

    /** Show what @p fx is now, without signalling. */
    void load(Doc *doc, const Fixture *fx);
    /** Show what the relevant ones of @p fixtures are now: a row they differ
     *  on reads "mixed" (shown for the first), and is only written once the
     *  user sets it. */
    void load(Doc *doc, const QList<const Fixture *> &fixtures);

    /** Write the rows' choice into @p rp, for @p fx: an override only where
     *  it differs from the definition, and only the rows the user has set
     *  since the last load(). */
    void store(const Fixture *fx, FixtureRigProps &rp) const;

    enum Kind { PointSource = 0, HangingCluster, LedTape };

signals:
    /** The user changed something. */
    void edited();

private:
    QWidget *m_lightRow = nullptr;
    QComboBox *m_kind = nullptr;
    QSpinBox *m_count = nullptr;
    QComboBox *m_glass = nullptr;
    bool m_loading = false;
    bool m_kindSet = false;         ///< kind or count set since load()
    bool m_glassSet = false;        ///< glass set since load()
    /** Mark @p cb "mixed" (an extra item, data -1) or not. */
    void showMixed(QComboBox *cb, bool mixed);
};

/** @} */

#endif
