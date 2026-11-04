/*
  Q Light Controller Plus
  blockingview.h

  Which blocking the app's views show (docs/design/BLOCKING.md, Q211): the
  cue in edit's (the default), or a set picked in the Lighting Studio. It
  sets the main thread's default blocking (BlockingScope::setThreadDefault)
  so the plot, the rig view, the plan views and Focus all read the same
  one; live output keeps reading the cue that went.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef BLOCKINGVIEW_H
#define BLOCKINGVIEW_H

#include <QObject>
#include <climits>
#include "qlcui_export.h"

class Doc;

class QLC_UI_EXPORT BlockingView final : public QObject
{
    Q_OBJECT

public:
    /** The one for @p doc (made on first use, a child of it). */
    static BlockingView *of(Doc *doc);
    ~BlockingView() override;

    /** "Follow the cue in edit": in Design the edited Show Tree cue's
     *  blocking, else the live one. */
    static constexpr quint32 followCue() { return UINT_MAX - 2; }

    /** What the Studio's picker says: followCue() or a set id (0 Default). */
    quint32 pick() const { return m_pick; }
    void setPick(quint32 pick);

    /** The blocking the views show now. */
    quint32 blocking() const { return m_blocking; }
    QString blockingName() const;
    /** A drag on the plot edits a blocking set, not Default. */
    bool editsSet() const { return m_blocking != 0; }

    /** "Save Positions as Blocking...": a new set @p name holding where
     *  every target stands in blocking @p from now (a copy of its
     *  overrides); its id. Undo is the set's removal by hand. */
    static quint32 savePositionsAs(Doc *doc, const QString &name, quint32 from);
    /** The menu/button text, the same everywhere. */
    static QString saveActionText();

    /** "Moving targets in: Apron number" (empty in Default). */
    QString bannerText() const;

public slots:
    /** Work it out again (a cue picked, a set renamed, the mode). */
    void refresh();

signals:
    /** blocking() or its name changed: views redraw their targets. */
    void changed();

private:
    explicit BlockingView(Doc *doc);
    Doc *m_doc;
    quint32 m_pick = followCue();
    quint32 m_blocking = 0;
    QString m_name;
};

#endif // BLOCKINGVIEW_H
