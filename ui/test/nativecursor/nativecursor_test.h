/*
  Q Light Controller Plus - Unit test
  nativecursor_test.h

  AppUtil::nativeCursorShape(): on macOS the cursors Qt 6.11 draws from a
  colour-profiled PNG (SizeAll, Wait, Busy) crash inside CoreGraphics
  (QTBUG-147602), so they are swapped for native ones with the same meaning;
  everywhere else the shape passes through untouched.
*/

#ifndef NATIVECURSOR_TEST_H
#define NATIVECURSOR_TEST_H

#include <QObject>

class NativeCursor_Test : public QObject
{
    Q_OBJECT

private slots:
    void macMapping();
    void macMappingNeverImageBased();
    void platformMapping();
    void noRawImageCursorsInSources();
};

#endif
