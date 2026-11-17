/*
  Q Light Controller Plus
  universeoutputview.h

  Copyright (C) Branson Matheson

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef UNIVERSEOUTPUTVIEW_H
#define UNIVERSEOUTPUTVIEW_H

#include <QByteArray>
#include <QColor>
#include <QRect>
#include <QWidget>

class QLabel;
class QTimer;
class Doc;

/** @addtogroup ui UI
 * @{
 */

/**
 * What one universe is sending, live ("it would be nice to see what's being
 * sent out on a U if selected"): its 512 channels as a grid, each with its
 * value and a bar as high, each fixture's channels shaded together and named
 * where they start. Values are what goes out -- after the grand master -- as
 * the universe writes them; a hover says whose channel it is and what.
 */
class UniverseOutputView final : public QWidget
{
    Q_OBJECT

public:
    explicit UniverseOutputView(Doc *doc, QWidget *parent = nullptr);

    /** What a channel is, for the colour of its block: an emitter colour, a
     *  dimmer, pan, tilt, or one of the other groups (read from the
     *  definition's group and colour as the Simple Desk reads them). */
    enum Kind { Plain = 0, Red, Green, Blue, Amber, White, UV, Cyan, Magenta, Yellow, Lime, Indigo,
                Dimmer, Pan, Tilt, ColourWheel, Gobo, Shutter, Speed, PrismKind, BeamKind,
                EffectKind, OtherKind };
    static Kind kindOf(const class QLCChannel *channel);
    /** The block's fill: a dull shade at 0 rising to the bright colour at 255. */
    static QColor fillFor(Kind kind, int value);
    static QColor emitterColour(Kind kind);

    /** Show @p universe (an index; invalid shows nothing). */
    void setUniverse(quint32 universe);
    quint32 universe() const { return m_universe; }

    /** The values as last seen (for tests). */
    QByteArray values() const { return m_values; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /** Lay the channels out as many to a row (4-16) as @p width holds. */
    void fitWidth(int width);
    int columns() const { return m_cols; }
    /** The width that shows @p columns channels a row. */
    static int widthFor(int columns);

    /** Where @p channel (0-based) is, in words: its address and, for a
     *  used one, the fixture, its address range and which of its channels
     *  this is ("121 · Front Wash 3 (121-136) · ch 4 Blue"); for a free one,
     *  the free run it sits in ("121 · free 121-140, 20 channels"). What
     *  the header line says while the pointer is over that cell. */
    QString describe(int channel) const;
    /** The address the label at the left of grid row @p row reads (the
     *  row's first channel, from the live column count). */
    int rowStart(int row) const { return row * m_cols + 1; }
    /** Test mode on the map (Doc::testChannels()): a click or a drag on a
     *  block sets that channel's value (top of the block 255, bottom 0), a
     *  double-click asks for a number 0-255, a right-click lets that channel
     *  go. Off: the map only looks. */
    void setTestMode(bool on);
    bool testMode() const { return m_testMode; }
    /** The value a press at @p y gives in @p cell. */
    static int valueAtY(const QRect &cell, int y);
    /** The channel (0-based) the header line is describing, or -1. */
    int hovered() const { return m_hovered; }

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    static void paintMark(class QPainter &p, Kind kind, const QRect &cell, int value);
    void paintLegend(class QPainter &p, int top);
    int legendHeight() const;

private slots:
    void slotUniverseWritten(quint32 index, const QByteArray &data);

private:
    void showHeader(const QString &text);
    /** The channel (0-based) under @p pos, or -1. */
    int channelAt(const QPoint &pos) const;
    QRect cellRect(int channel) const;
    void updateHeader();
    /** The header line on @p channel (describe()), or back to its own
     *  text (-1). */
    void hoverChannel(int channel);
    int m_hovered = -1;
    bool m_testMode = false;
    int m_dragChannel = -1;
    void applyTest(int channel, int y);

    Doc *m_doc;
    quint32 m_universe;
    QByteArray m_values;
    QLabel *m_header;
    QString m_headerText;
    QTimer *m_repaint;
    int m_cols = 16;
    int rows() const;
    bool m_dirty = false;
};

/** @} */

#endif
