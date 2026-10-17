/*
  Q Light Controller Plus - Unit test
  nativecursor_test.cpp
*/

#include <QtTest>
#include <QDirIterator>
#include <QRegularExpression>

#include "nativecursor_test.h"
#include "apputil.h"

// The shapes Qt 6.11's cocoa plugin builds from sizeallcursor.png,
// spincursor.png and waitcursor.png -- each carries an iCCP chunk, and
// QImage::toCGImage() frees the resulting colour space too early.
static const QList<Qt::CursorShape> kPngBacked = {
    Qt::SizeAllCursor, Qt::WaitCursor, Qt::BusyCursor
};

void NativeCursor_Test::macMapping()
{
    QCOMPARE(AppUtil::macSafeCursorShape(Qt::SizeAllCursor), Qt::OpenHandCursor);
    QCOMPARE(AppUtil::macSafeCursorShape(Qt::SizeAllCursor, true), Qt::ClosedHandCursor);
    QCOMPARE(AppUtil::macSafeCursorShape(Qt::WaitCursor), Qt::ArrowCursor);
    QCOMPARE(AppUtil::macSafeCursorShape(Qt::BusyCursor), Qt::ArrowCursor);

    // Native NSCursors (or profile-less pixmaps) pass through.
    const QList<Qt::CursorShape> unchanged = {
        Qt::ArrowCursor, Qt::CrossCursor, Qt::IBeamCursor, Qt::PointingHandCursor,
        Qt::OpenHandCursor, Qt::ClosedHandCursor, Qt::SizeHorCursor,
        Qt::SizeVerCursor, Qt::SizeBDiagCursor, Qt::SizeFDiagCursor,
        Qt::SplitHCursor, Qt::SplitVCursor, Qt::ForbiddenCursor,
        Qt::WhatsThisCursor, Qt::DragMoveCursor, Qt::DragCopyCursor,
        Qt::DragLinkCursor, Qt::BlankCursor
    };
    for (Qt::CursorShape s : unchanged)
    {
        QCOMPARE(AppUtil::macSafeCursorShape(s), s);
        QCOMPARE(AppUtil::macSafeCursorShape(s, true), s);
    }
}

void NativeCursor_Test::macMappingNeverImageBased()
{
    for (int i = Qt::ArrowCursor; i <= Qt::LastCursor; i++)
    {
        const Qt::CursorShape s = Qt::CursorShape(i);
        for (bool dragging : { false, true })
            QVERIFY2(!kPngBacked.contains(AppUtil::macSafeCursorShape(s, dragging)),
                     qPrintable(QString("shape %1 maps to a PNG-backed cursor").arg(i)));
    }
}

void NativeCursor_Test::platformMapping()
{
    for (int i = Qt::ArrowCursor; i <= Qt::LastCursor; i++)
    {
        const Qt::CursorShape s = Qt::CursorShape(i);
        for (bool dragging : { false, true })
        {
#if defined(Q_OS_MACOS)
            QCOMPARE(AppUtil::nativeCursorShape(s, dragging),
                     AppUtil::macSafeCursorShape(s, dragging));
#else
            // Linux / Windows keep exactly what they had.
            QCOMPARE(AppUtil::nativeCursorShape(s, dragging), s);
#endif
        }
    }
}

// Guard: no setCursor() in the widgets UI names a PNG-backed shape directly.
// Every use must go through AppUtil::nativeCursorShape().
void NativeCursor_Test::noRawImageCursorsInSources()
{
    QDir root(QStringLiteral(QLC_TEST_RESOURCE_ROOT));
    QVERIFY(root.cdUp());
    const QStringList dirs = { "ui/src", "main", "fixtureeditor", "webaccess/src" };
    const QRegularExpression raw("Qt::(SizeAll|Wait|Busy)Cursor");
    const QRegularExpression wrapped("nativeCursorShape\\(\\s*Qt::(SizeAll|Wait|Busy)Cursor");
    int scanned = 0;
    QStringList offenders;

    for (const QString &d : dirs)
    {
        if (!root.exists(d))
            continue;
        QDirIterator it(root.filePath(d), { "*.cpp", "*.h", "*.mm" },
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString path = it.next();
            if (path.endsWith("/apputil.cpp") || path.endsWith("/apputil.h"))
                continue;
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
            scanned++;
            int lineNo = 0;
            while (!f.atEnd())
            {
                const QString line = QString::fromUtf8(f.readLine());
                lineNo++;
                const int rawCount = int(line.count(raw));
                if (rawCount == 0)
                    continue;
                if (rawCount != int(line.count(wrapped)))
                    offenders << QString("%1:%2: %3").arg(path).arg(lineNo).arg(line.trimmed());
            }
        }
    }
    QVERIFY2(scanned > 100, "source tree not found");
    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join('\n')));
}

QTEST_MAIN(NativeCursor_Test)
