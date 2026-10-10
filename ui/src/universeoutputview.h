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

    /** Show @p universe (an index; invalid shows nothing). */
    void setUniverse(quint32 universe);
    quint32 universe() const { return m_universe; }

    /** The values as last seen (for tests). */
    QByteArray values() const { return m_values; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *e) override;

private slots:
    void slotUniverseWritten(quint32 index, const QByteArray &data);

private:
    /** The channel (0-based) under @p pos, or -1. */
    int channelAt(const QPoint &pos) const;
    QRect cellRect(int channel) const;
    void updateHeader();

    Doc *m_doc;
    quint32 m_universe;
    QByteArray m_values;
    QLabel *m_header;
    QTimer *m_repaint;
    bool m_dirty = false;
};

/** @} */

#endif
