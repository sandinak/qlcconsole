/*
  Q Light Controller Plus - qlcconsole
  qlceventpos.h

  Qt6 deprecated the integer/point accessors on input events in favour of
  position()/globalPosition(), which return QPointF and do not exist in Qt5.
  This fork still builds against both, and the Linux CI job compiles with
  -Werror, so every remaining call is a build failure there.

  These helpers give one spelling that compiles on both. They are deliberately
  templates: QDropEvent, QMouseEvent and QWheelEvent don't share a base that
  declares these, but they all spell them the same way.

  Licensed under the Apache License, Version 2.0.
*/

#ifndef QLCEVENTPOS_H
#define QLCEVENTPOS_H

#include <QtGlobal>
#include <QPoint>
#include <QPointF>

/** Event position in widget coordinates, rounded to whole pixels. */
template <typename Event>
inline QPoint qlcEventPos(const Event *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

/** Event position in widget coordinates, full precision. */
template <typename Event>
inline QPointF qlcEventPosF(const Event *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return e->posF();
#endif
}

/** Event position in screen coordinates, rounded to whole pixels. */
template <typename Event>
inline QPoint qlcEventGlobalPos(const Event *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}

#endif
