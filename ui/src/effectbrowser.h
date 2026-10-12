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
#include <QColor>
#include <QHash>

class QListWidgetItem;
class QListWidget;
class QComboBox;
class QLineEdit;
class QTabBar;
class QTimer;
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

    /** Most recently picked first; remembered between sessions. */
    static QStringList recent();

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
        /** What it is listed by: its own label -- a preset, its generator's
         *  then its own, so it stays under the effect it came from. */
        QString sortName;
        QString key() const;    ///< for Recent: kind and name
    };

    void refill();
    void pick(QListWidgetItem *item);
    void tickPreviews();
    /** The colours of a strip of 16 for @p scriptPath, now. */
    QVector<QColor> previewFrame(const QString &scriptPath);

    Doc *m_doc;
    QComboBox *m_source;
    QLineEdit *m_search = nullptr;
    QTabBar *m_tabs = nullptr;
    QListWidget *m_list = nullptr;
    QTimer *m_previewTimer = nullptr;
    QElapsedTimer m_clock;
    QList<Entry> m_entries;
    QList<QColor> m_previewColours;

    /** A preview's script and its run (state), defined in the .cpp. */
    struct Run;
    QHash<QString, QSharedPointer<Run> > m_runs;   ///< by script path; null = would not load
    QHash<QString, QVector<QColor> > m_frames;     ///< last frame per script path
    friend class EffectRowDelegate;
    bool m_filling = false;
};

/** @} */

#endif
