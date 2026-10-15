/*
  Q Light Controller
  main.cpp

  Copyright (c) Heikki Junnila

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

#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QTimer>
#include <QDir>

#include "qlcconfig.h"
#include "launcher.h"

void loadTranslation(const QString& locale, QApplication& app)
{
    QString file(QString("launcher_%1").arg(locale));
#if defined(__APPLE__) || defined(Q_OS_MAC)
    QString path(QString("%1/../%2").arg(QApplication::applicationDirPath())
                 .arg(TRANSLATIONDIR));
#else
    QString path(TRANSLATIONDIR);
#endif

    QTranslator* translator = new QTranslator(&app);
    if (translator->load(file, path) == true)
    {
        qDebug() << "Using translation for" << locale;
        app.installTranslator(translator);
    }
    else
    {
        qDebug() << "Unable to find translation for" << locale
        << "in" << path;
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    /* Load plugins from within the bundle ONLY */
    QDir dir(QApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("PlugIns");
    QApplication::setLibraryPaths(QStringList(dir.absolutePath()));

    loadTranslation(QLocale::system().name(), app);

    Launcher launcher;
    app.installEventFilter(&launcher);

    // Started by the system for a double-clicked show or fixture, the file
    // arrives as an event once app.exec() runs, and the event filter starts
    // the right program for it. Otherwise -- a plain start -- qlcconsole,
    // after a grace for that event to arrive.
    QTimer::singleShot(500, &launcher, SLOT(slotStartDefault()));
    return app.exec();
}
