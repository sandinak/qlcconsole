/*
  Q Light Controller Plus
  helpbrowser.cpp

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

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QApplication>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextBlock>
#include <QImageReader>
#include <QListWidget>
#include <QToolButton>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSplitter>
#include <QPointer>
#include <QLabel>
#include <QFile>

#include "helpbrowser.h"

namespace
{
/* The guides, in the order to read them. */
const char *const kTopics[] = {
    "getting-started.md", "whats-new.md", "README.md", "lighting-studio.md",
    "programming-tab.md", "fixture-manager.md", "stage-targets.md",
    "other-improvements.md", "effects.md", "shortcuts.md", "troubleshooting.md",
    "EFFECTSCRIPT_API.md"
};

QString readGuide(const QString &file)
{
    QFile f(HelpBrowser::resourcePath(file));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

QString titleOf(const QString &file)
{
    const QString text = readGuide(file);
    for (const QString &line : text.split(QLatin1Char('\n')))
        if (line.startsWith(QLatin1String("# ")))
            return line.mid(2).trimmed();
    return file;
}

QPointer<HelpBrowser> s_main;
}

HelpBrowser::HelpBrowser(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("helpBrowser"));
    setWindowTitle(tr("qlcconsole Help"));
    resize(1100, 760);

    QVBoxLayout *root = new QVBoxLayout(this);
    QHBoxLayout *bar = new QHBoxLayout;
    m_back = new QToolButton(this);
    m_back->setText(QStringLiteral("◀"));
    m_back->setToolTip(tr("Back"));
    m_forward = new QToolButton(this);
    m_forward->setText(QStringLiteral("▶"));
    m_forward->setToolTip(tr("Forward"));
    QToolButton *home = new QToolButton(this);
    home->setText(tr("Getting Started"));
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("helpSearch"));
    m_search->setPlaceholderText(tr("Search the help…"));
    m_search->setClearButtonEnabled(true);
    m_found = new QLabel(this);
    m_found->setStyleSheet(QStringLiteral("QLabel { color: #8a8e99; }"));
    bar->addWidget(m_back);
    bar->addWidget(m_forward);
    bar->addWidget(home);
    bar->addSpacing(12);
    bar->addWidget(m_search, 1);
    bar->addWidget(m_found);
    root->addLayout(bar);

    QSplitter *split = new QSplitter(Qt::Horizontal, this);
    m_topics = new QListWidget(split);
    m_topics->setObjectName(QStringLiteral("helpTopics"));
    m_browser = new QTextBrowser(split);
    m_browser->setObjectName(QStringLiteral("helpText"));
    m_browser->setOpenLinks(false);       // followed here: between guides, to headings
    split->addWidget(m_topics);
    split->addWidget(m_browser);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({ 240, 860 });
    root->addWidget(split, 1);

    search(QString());

    connect(m_topics, &QListWidget::currentRowChanged, this, [this](int row) {
        QListWidgetItem *it = m_topics->item(row);
        if (it == nullptr)
            return;
        const QString file = it->data(Qt::UserRole).toString();
        if (file != m_file)
            load(file, QString(), true);
        if (!m_search->text().trimmed().isEmpty())
        {
            // Show where it was found.
            m_browser->moveCursor(QTextCursor::Start);
            m_browser->find(m_search->text().trimmed());
        }
    });
    connect(m_search, &QLineEdit::textChanged, this, &HelpBrowser::search);
    connect(m_browser, &QTextBrowser::anchorClicked, this, &HelpBrowser::followLink);
    connect(home, &QToolButton::clicked, this, [this]() { showTopic(QStringLiteral("getting-started.md")); });
    connect(m_back, &QToolButton::clicked, this, [this]() {
        if (m_at <= 0)
            return;
        const QString t = m_history.at(--m_at);
        load(t.section(QLatin1Char('#'), 0, 0), t.section(QLatin1Char('#'), 1), false);
    });
    connect(m_forward, &QToolButton::clicked, this, [this]() {
        if (m_at + 1 >= m_history.size())
            return;
        const QString t = m_history.at(++m_at);
        load(t.section(QLatin1Char('#'), 0, 0), t.section(QLatin1Char('#'), 1), false);
    });
    m_back->setEnabled(false);
    m_forward->setEnabled(false);
}

QList<QPair<QString, QString> > HelpBrowser::topics()
{
    QList<QPair<QString, QString> > out;
    for (const char *t : kTopics)
    {
        const QString file = QLatin1String(t);
        if (QFile::exists(resourcePath(file)))
            out << qMakePair(file, titleOf(file));
    }
    return out;
}

QString HelpBrowser::resourcePath(const QString &file)
{
    return QStringLiteral(":/help/") + file;
}

QString HelpBrowser::headingSlug(const QString &heading)
{
    QString out;
    for (const QChar &c : heading.trimmed().toLower())
    {
        if (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_'))
            out += c;
        else if (c == QLatin1Char(' '))
            out += QLatin1Char('-');
        // everything else (punctuation, &, ., —) is dropped, as GitHub does
    }
    return out;
}

void HelpBrowser::showTopic(const QString &topic)
{
    QString file = topic.section(QLatin1Char('#'), 0, 0);
    if (file.isEmpty() || !QFile::exists(resourcePath(file)))
        file = QStringLiteral("getting-started.md");
    load(file, topic.section(QLatin1Char('#'), 1), true);
}

void HelpBrowser::load(const QString &file, const QString &fragment, bool record)
{
    if (file != m_file)
    {
        QTextDocument *doc = m_browser->document();
        doc->setBaseUrl(QUrl(QStringLiteral("qrc:/help/")));
        m_browser->setMarkdown(readGuide(file));
        m_file = file;

        /* The screenshots are taken at full size: fit each to the page. */
        const int room = qMax(400, m_browser->viewport()->width() - 40);
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
            for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it)
            {
                QTextFragment frag = it.fragment();
                if (!frag.isValid() || !frag.charFormat().isImageFormat())
                    continue;
                QTextImageFormat img = frag.charFormat().toImageFormat();
                const QUrl src = doc->baseUrl().resolved(QUrl(img.name()));
                const QSize size = QImageReader(QLatin1Char(':') + src.path()).size();
                if (!size.isValid() || size.width() <= room)
                    continue;
                img.setWidth(room);
                img.setHeight(double(size.height()) * room / size.width());
                QTextCursor c(doc);
                c.setPosition(frag.position());
                c.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
                c.setCharFormat(img);
            }

        const QString title = titleOf(file);
        setWindowTitle(tr("qlcconsole Help — %1").arg(title));
        for (int i = 0; i < m_topics->count(); ++i)
            if (m_topics->item(i)->data(Qt::UserRole).toString() == file)
            {
                m_topics->blockSignals(true);
                m_topics->setCurrentRow(i);
                m_topics->blockSignals(false);
            }
    }
    m_heading.clear();
    if (fragment.isEmpty())
        m_browser->verticalScrollBar()->setValue(0);
    else
        scrollToHeading(fragment);

    if (record)
    {
        while (m_history.size() > m_at + 1)
            m_history.removeLast();
        m_history << (fragment.isEmpty() ? file : file + QLatin1Char('#') + fragment);
        m_at = m_history.size() - 1;
    }
    m_back->setEnabled(m_at > 0);
    m_forward->setEnabled(m_at + 1 < m_history.size());
}

void HelpBrowser::scrollToHeading(const QString &fragment)
{
    QTextDocument *doc = m_browser->document();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
    {
        if (b.blockFormat().headingLevel() <= 0 || headingSlug(b.text()) != fragment)
            continue;
        m_heading = b.text();
        QTextCursor c(b);
        m_browser->setTextCursor(c);
        const qreal top = doc->documentLayout()->blockBoundingRect(b).top();
        m_browser->verticalScrollBar()->setValue(int(top));
        return;
    }
}

void HelpBrowser::search(const QString &text)
{
    const QString want = text.trimmed();
    m_topics->blockSignals(true);
    m_topics->clear();
    int hits = 0;
    for (const auto &t : topics())
    {
        int count = 0;
        if (!want.isEmpty())
        {
            count = readGuide(t.first).count(want, Qt::CaseInsensitive);
            if (count == 0)
                continue;
        }
        QListWidgetItem *it = new QListWidgetItem(
            count > 0 ? tr("%1 (%2)").arg(t.second).arg(count) : t.second, m_topics);
        it->setData(Qt::UserRole, t.first);
        if (t.first == m_file)
            m_topics->setCurrentItem(it);
        hits += count > 0 ? 1 : 0;
    }
    m_topics->blockSignals(false);
    m_found->setText(want.isEmpty() ? QString()
                     : hits == 0 ? tr("Not found")
                     : tr("In %n guide(s)", "", hits));
}

void HelpBrowser::followLink(const QUrl &url)
{
    const QString scheme = url.scheme();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https")
        || scheme == QLatin1String("mailto"))
    {
        QDesktopServices::openUrl(url);
        return;
    }
    // "#heading" -- on this page.
    if (url.path().isEmpty())
    {
        load(m_file, url.fragment(), true);
        return;
    }
    // Another guide: by its name, wherever the link was written to find it
    // ("other-improvements.md", "../../EFFECTSCRIPT_API.md").
    const QString file = QUrl(QStringLiteral("qrc:/help/") + m_file).resolved(url).fileName();
    if (QFile::exists(resourcePath(file)))
        load(file, url.fragment(), true);
}

HelpBrowser *HelpBrowser::open(const QString &topic)
{
    HelpBrowser *help = nullptr;
    if (QWidget *modal = QApplication::activeModalWidget())
    {
        help = new HelpBrowser(modal);
        help->setAttribute(Qt::WA_DeleteOnClose);
    }
    else
    {
        if (s_main.isNull())
        {
            QWidget *main = nullptr;
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->inherits("App"))
                    main = w;
            s_main = new HelpBrowser(main);
        }
        help = s_main;
    }
    help->showTopic(topic);
    help->show();
    help->raise();
    help->activateWindow();
    return help;
}
