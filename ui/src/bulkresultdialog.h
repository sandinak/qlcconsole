/*
  Q Light Controller Plus - qlcconsole
  bulkresultdialog.h

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

#ifndef BULKRESULTDIALOG_H
#define BULKRESULTDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QList>
#include <QPair>

class QDialogButtonBox;
class QVBoxLayout;
class QLabel;

/**
 * What a bulk action did -- Update All of the fixture definitions, Undo
 * Last Update, switching or installing a show's definitions -- or what it
 * is about to do, as a proper window rather than an alert: a summary line,
 * then each section ("Updated", "Errors") as its own scrolling list, an
 * entry's details (what changed) under it, and Close (or Yes / Cancel to
 * ask), Copy and Save as Text.
 *
 * It opens no bigger than the screen allows, and can be resized: an alert
 * holding two hundred lines grew off the screen with nothing to scroll and
 * no Close button.
 */
class BulkResultDialog final : public QDialog
{
    Q_OBJECT

public:
    /** One line of a section, with what more there is to say about it. */
    struct Entry
    {
        QString text;
        QStringList details;
    };

    BulkResultDialog(const QString &title, const QString &summary, QWidget *parent = nullptr);

    /** A section: @p heading over its scrolling list; nothing when empty.
     *  @p name names the list (tests). */
    void addSection(const QString &heading, const QList<Entry> &entries,
                    const QString &name = QString());
    /** The same, from plain lines. */
    void addSection(const QString &heading, const QStringList &lines,
                    const QString &name = QString());
    /** A line under the sections ("The old files are in …"). */
    void addNote(const QString &text);
    /** Yes / Cancel in place of Close, the question above them. */
    void setQuestion(const QString &question);

    /** Everything it says, as text (Copy, Save as Text). */
    QString plainText() const;

    /** Show it (Close), unless there is nothing to say. */
    static void tell(QWidget *parent, const QString &title, const QString &summary,
                     const QList<QPair<QString, QList<Entry> > > &sections,
                     const QStringList &notes = QStringList());
    /** Ask over a list that may be long: @p intro, @p items under
     *  @p heading scrolling, @p question; true for Yes. */
    static bool confirm(QWidget *parent, const QString &title, const QString &intro,
                        const QString &heading, const QStringList &items,
                        const QString &question);

    /** Plain lines as entries. */
    static QList<Entry> entries(const QStringList &lines);

protected:
    void showEvent(QShowEvent *ev) override;

private slots:
    void copyText();
    void saveText();

private:
    /** At most nine tenths of the screen it opens on, centred over it. */
    void fitToScreen();

    QVBoxLayout *m_sections;
    QDialogButtonBox *m_buttons;
    QLabel *m_question = nullptr;
    QString m_summary;
    QStringList m_text;
    bool m_fitted = false;
};

#endif
