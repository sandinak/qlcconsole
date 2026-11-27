/*
  Q Light Controller Plus - test support
  painttimer.h

  How long each paint of a widget takes on the wall clock: the paint event
  is delivered from inside an event filter, timed, and then not again.
  Used by ui/test/showscale and ui/test/renderperf (board Q360).

  Licensed under the Apache License, Version 2.0
*/

#ifndef QLC_TEST_PAINTTIMER_H
#define QLC_TEST_PAINTTIMER_H

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QWidget>
#include <algorithm>
#include <vector>

#include "hostspeed.h"

class PaintTimer final : public QObject
{
public:
    explicit PaintTimer(QWidget *w) : QObject(w) { w->installEventFilter(this); }
    std::vector<double> ms;
    double p95() const { return HostSpeed::quantile(ms, 0.95); }
    double worst() const { return ms.empty() ? 0 : *std::max_element(ms.begin(), ms.end()); }

protected:
    bool eventFilter(QObject *o, QEvent *e) override
    {
        if (e->type() != QEvent::Paint || m_inside)
            return false;
        m_inside = true;
        QElapsedTimer t;
        t.start();
        QCoreApplication::sendEvent(o, e);
        ms.push_back(t.nsecsElapsed() / 1e6);
        m_inside = false;
        return true;
    }

private:
    bool m_inside = false;
};

#endif
