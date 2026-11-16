/*
  Q Light Controller Plus - test helpers
  Licensed under the Apache License, Version 2.0
*/

#ifndef PADFRAME_H
#define PADFRAME_H

#include <QWidget>
#include <QImage>
#include <QColor>

/** The Run tab's Pad Run frame, read back as DRAWN: the colour of the
 *  pixels at the middle of each edge of a grab() of the CueRunner, or an
 *  invalid colour when the four edges do not agree on one (no frame).
 *  A style string cannot tell whether the platform repainted it away. */
inline QColor padFrameDrawn(QWidget *runner)
{
    if (runner == nullptr || runner->width() < 20 || runner->height() < 20)
        return QColor();
    const QImage img = runner->grab().toImage();
    if (img.isNull())
        return QColor();
    const qreal r = img.devicePixelRatio();
    const int w = int(img.width() / r), h = int(img.height() / r);
    auto at = [&](int x, int y) { return img.pixelColor(int(x * r), int(y * r)); };
    const QColor top = at(w / 2, 2), bottom = at(w / 2, h - 3);
    const QColor left = at(2, h / 2), right = at(w - 3, h / 2);
    return (top == bottom && top == left && top == right) ? top : QColor();
}

/** The frame child itself: visible (shown to the user) or not. */
inline bool padFrameShown(QWidget *runner)
{
    QWidget *f = runner->findChild<QWidget *>(QStringLiteral("crPadFrame"));
    return f != nullptr && !f->isHidden();
}

#endif
