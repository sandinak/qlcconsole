/*
  Q Light Controller Plus
  helpbrowser.h

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

#ifndef HELPBROWSER_H
#define HELPBROWSER_H

#include <QWidget>
#include <QUrl>

class QTextBrowser;
class QListWidget;
class QLineEdit;
class QToolButton;
class QLabel;

/** @addtogroup ui UI
 * @{
 */

/**
 * The app's own help: the feature guides (docs/features), bundled into the
 * app, read in a window of their own. Topics on the left -- or what a search
 * finds in them -- the guide on the right, links between guides followed,
 * a link to a heading ("lighting-studio.md#mounting-a-fixture") scrolled
 * to, web links handed to the browser.
 *
 * F1 opens it on the topic for what is in front (App::helpTopicFor()); a
 * widget names its topic with the "helpTopic" property, the nearest one up
 * its parents winning.
 */
class HelpBrowser final : public QWidget
{
    Q_OBJECT

public:
    explicit HelpBrowser(QWidget *parent = nullptr);

    /** Show @p topic: "getting-started.md", "lighting-studio.md#mounting-a-fixture". */
    void showTopic(const QString &topic);

    /** Every guide the help has, in reading order: file name and title. */
    static QList<QPair<QString, QString> > topics();
    /** Where a guide is bundled (":/help/<file>"). */
    static QString resourcePath(const QString &file);
    /** A heading's anchor, as GitHub writes it ("Connections & patch" ->
     *  "connections--patch"), so links written for the repository work here. */
    static QString headingSlug(const QString &heading);

    /** The guide on screen, and the heading it was scrolled to (tests). */
    QString currentFile() const { return m_file; }
    QString currentHeading() const { return m_heading; }
    QTextBrowser *browser() const { return m_browser; }

    /** The window that shows help -- one, reused; inside a modal dialog, one
     *  of its own over it (a window the dialog's modality would block is no
     *  help). */
    static HelpBrowser *open(const QString &topic);

private:
    void load(const QString &file, const QString &fragment, bool record);
    void scrollToHeading(const QString &fragment);
    void search(const QString &text);
    void followLink(const QUrl &url);
    void fitImages();
    void showImage(const QString &path);

    bool eventFilter(QObject *watched, QEvent *event) override;

    QTextBrowser *m_browser;
    QListWidget *m_topics;
    QLineEdit *m_search;
    QToolButton *m_back, *m_forward;
    QLabel *m_found;
    QString m_file, m_heading;
    QStringList m_history;
    int m_at = -1;
    int m_fittedTo = 0;                 ///< page width the pictures were fitted to
};

/** @} */

#endif
