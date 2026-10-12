/*
  Q Light Controller Plus
  rigeditsession.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef RIGEDITSESSION_H
#define RIGEDITSESSION_H

#include <QByteArray>
#include "monitor.h"

/** @addtogroup ui_monitor
 * @{
 */

/**
 * How every rig dialog behaves: it edits the rig live as its fields change,
 * Cancel puts back everything it did, and OK is one Ctrl+Z step on the plot.
 * Open one as the dialog opens:
 *
 *     RigEditSession edit(this);
 *     ... connect the fields to apply as they change ...
 *     if (dlg.exec() != QDialog::Accepted)
 *     {
 *         edit.cancel();
 *         return;
 *     }
 *     ...
 *
 * A session left without cancel() keeps what was done, as one undo step: an
 * OK path that returns early must never lose the user's edit.
 */
class RigEditSession final
{
public:
    explicit RigEditSession(Monitor *monitor)
        : m_monitor(monitor)
        , m_before(monitor->rigSnapshot())
    {
    }

    ~RigEditSession()
    {
        if (!m_done)
            accept();
    }

    /** Keep what the dialog did, as one undo step. */
    void accept()
    {
        m_done = true;
        m_monitor->pushRigUndo(m_before);
    }

    /** Put the rig back as it was when the dialog opened. */
    void cancel()
    {
        m_done = true;
        if (m_monitor->rigSnapshot() != m_before)
            m_monitor->restoreRig(m_before);
    }

private:
    Q_DISABLE_COPY(RigEditSession)
    Monitor *m_monitor;
    QByteArray m_before;
    bool m_done = false;
};

/** @} */

#endif
