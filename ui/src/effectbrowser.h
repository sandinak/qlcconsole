/*
  Q Light Controller Plus
  effectbrowser.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef EFFECTBROWSER_H
#define EFFECTBROWSER_H

#include <QSharedPointer>
#include <QElapsedTimer>
#include <QWidget>
#include <QPointF>
#include <QSet>
#include <QVector>
#include <QColor>
#include <QColor>
#include <QHash>

class QListWidgetItem;
class QListWidget;
class QComboBox;
class QLineEdit;
class QTabBar;
class QTimer;
class QToolButton;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * The look editor's effect picker, in place of a dropdown a hundred and
 * fifty lines long ("too long and too disjointed"): a search box, a tab per
 * category (Recent first, WLED last), and a list where each effect shows its
 * name, a line saying what it does and a strip of sixteen lights running it.
 * Presets sit under the effect they are made from.
 *
 * It MIRRORS the look editor's effect combo -- the combo stays the one
 * source of truth, and a pick here just sets its index -- so everything that
 * listens to the combo, and the Browse dialog, carries on as it was.
 */
class EffectBrowser final : public QWidget
{
    Q_OBJECT

public:
    EffectBrowser(Doc *doc, QComboBox *source, QWidget *parent = nullptr);
    ~EffectBrowser() override;

    /** Rebuild from the combo (after the look editor repopulates it). */
    void reload();

    /** Colours the previews run in: the look's own, else a default set. */
    void setPreviewColours(const QList<QColor> &colours);

    /** The tab names, in order (for tests). */
    QStringList tabNames() const;
    QListWidget *list() const { return m_list; }
    QLineEdit *search() const { return m_search; }
    QTabBar *tabs() const { return m_tabs; }
    /** Where effects came from: Built-in, Mine, This show (EffectOrigin::Kind
     *  order). None checked lists them all; checking some lists only those,
     *  from every tab, as a search does. */
    QToolButton *originFilter(int kind) const { return m_origin[qBound(0, kind, 2)]; }

    /** Most recently picked first; remembered between sessions. */
    static QStringList recent();

    /** Moving through the list with the arrow keys picks as it goes (the
     *  picker window: browse and watch each one on the rig). Off, only a
     *  click or Enter picks. */
    void setPickWhileBrowsing(bool on) { m_pickWhileBrowsing = on; }

    /** Whether the look has a WLED device to run on. Without one, a WLED
     *  effect that has no port (it runs on the device only) isn't offered.
     *  Default true: nothing known, everything listed. */
    void setWledTargets(bool any)
    {
        if (any == m_wledTargets)
            return;
        m_wledTargets = any;
        reload();
    }
    bool wledTargets() const { return m_wledTargets; }

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    struct Entry
    {
        int comboIndex = -1;
        int kind = 0;           ///< the look editor's EffectKind*
        QString name;           ///< as the combo stores it (UserRole)
        QString label;          ///< as shown
        QString category;
        QString description;
        QString scriptPath;     ///< for the preview: the engine script, or a WLED port
        bool isPreset = false;
        int origin = 0;         ///< EffectOrigin::Kind
        QString basedOn;        ///< "based on Fire" (mine, this show)
        QString diffTip;        ///< what differs from that, for the hover
        /** What it is listed by: its own label -- a preset, its generator's
         *  then its own, so it stays under the effect it came from. */
        QString sortName;
        QString key() const;    ///< for Recent: kind and name
    };

    bool m_pickWhileBrowsing = false;
    bool m_wledTargets = true;
    void refill();
    void pick(QListWidgetItem *item);
    void tickPreviews();
public:
    /** What a row's preview shows: what the effect does, now. */
    struct Preview
    {
        enum Kind { Lights, Position, Gobo, Beam, Matrix };
        Kind kind = Lights;
        QVector<QColor> lights;     ///< Lights: the strip of 16; Matrix: the box, row by row
        int cols = 0, rows = 0;     ///< Matrix: the box's size
        QVector<QPointF> spots;     ///< Position: where each head points, 0..1 (pan, tilt)
        QVector<double> beams;      ///< Beam: each head's beam size 0..1; Gobo: the slot
    };
    /** A preview for @p scriptPath, now (and its trail, for a position one). */
    Preview previewFrame(const QString &scriptPath);
    Preview frame(const QString &scriptPath) const { return m_frames.value(scriptPath); }

private:
    /** A preview's script and its run (state), defined in the .cpp. */
    struct Run;
    /** The preview of @p run at @p t seconds. */
    Preview previewWith(Run *run, double t, bool steer = false);
    /** A matrix effect's preview at @p t: on a pixel box (previewWith()
     *  hands those over here). */
    Preview previewMatrix(Run *run, double t);
    /** A position effect's shape: four heads' paths over eight seconds. */
    QVector<QVector<QPointF> > samplePaths(const QString &scriptPath);

    Doc *m_doc;
    QComboBox *m_source;
    QLineEdit *m_search = nullptr;
    QTabBar *m_tabs = nullptr;
    QToolButton *m_origin[3] = { nullptr, nullptr, nullptr };
    QListWidget *m_list = nullptr;
    QTimer *m_previewTimer = nullptr;
    QElapsedTimer m_clock;
    QList<Entry> m_entries;
    QList<QColor> m_previewColours;

    QHash<QString, QSharedPointer<Run> > m_runs;   ///< by script path; null = would not load
    QHash<QString, Preview> m_frames;             ///< last frame per script path
    /** A position effect's shape, sampled once (samplePaths()). */
    QHash<QString, QVector<QVector<QPointF> > > m_paths;
    /** Position effects that only follow a joystick: previewed steered. */
    QSet<QString> m_steered;
    friend class EffectRowDelegate;
    bool m_filling = false;
};

/** @} */

#endif
