/*
  Q Light Controller Plus
  helpshots.h

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

#ifndef HELPSHOTS_H
#define HELPSHOTS_H

#include <QPointer>
#include <QObject>
#include <QMargins>
#include <QImage>
#include <QList>
#include <QRect>
#include <QPoint>

#include <functional>

class QWidget;
class App;

/**
 * The pictures in the help. When QLC_HELP_SHOTS names a folder, the app runs
 * a fixed list of shots once it has loaded its show: each sets up a view,
 * grabs it (offscreen too) and marks the controls the guide talks about with
 * numbered outlines, then writes "<name>.png" there. The app quits after the
 * last one. QLC_HELP_SHOTS_ONLY=<name substring> runs only those that match.
 * tools/help-shots.sh runs it on the demo show into docs/images.
 */
class HelpShots final : public QObject
{
    Q_OBJECT

public:
    /** Where a callout's number sits; Auto picks by the control's size. */
    enum Place { Auto, Corner, Above, Left, Right };

    /** A numbered outline round a control. */
    struct Callout
    {
        QPointer<QWidget> widget;
        QRect rect;             // in the grabbed widget's coordinates, when no widget
        int number = 0;
        Place place = Auto;
    };

    /** What a shot's setup leaves for the grab. */
    struct Frame
    {
        QPointer<QWidget> grab;
        QList<Callout> callouts;
        QRect crop;             // part of the grab to keep; empty = all of it
        QMargins pad;           // room added round the grab, for numbers outside it
        bool saved = false;     // the setup took the picture itself (a modal dialog)
        /** A window of its own (a popup menu) drawn over the grab at
            @a at, in the grab's coordinates: the grab alone leaves it out. */
        QPointer<QWidget> overlay;
        QPoint overlayAt;
    };

    struct Shot
    {
        QString name;
        std::function<Frame()> setup;
        std::function<void()> cleanup;
    };

    /** With QLC_HELP_SHOTS set, points every QSettings at a fresh, empty
        folder for this run. Call before the QApplication is made. */
    static void isolateSettings();

    /** Starts the shots if QLC_HELP_SHOTS is set; does nothing otherwise. */
    static void runIfRequested(App *app);

    /** Draws @a frame's callouts onto @a image (a grab of frame.grab). */
    static QImage annotate(const QImage &image, const Frame &frame);

private:
    HelpShots(App *app, const QString &outDir, const QString &only);

    QList<Shot> shots();
    void next();
    void take(const Shot &shot);
    /** Grabs @a frame, marks it and writes "<name>.png"; false on a failure. */
    bool save(const QString &name, const Frame &frame);

private:
    App *m_app;
    QString m_outDir;
    QString m_only;
    QList<Shot> m_queue;
    int m_failed;
};

#endif
