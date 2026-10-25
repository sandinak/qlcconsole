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
#include <QTextDocumentFragment>
#include <QHash>
#include <QTimer>
#include <QImageReader>
#include <QScrollArea>
#include <QPixmap>
#include <QScreen>
#include <QDialog>
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
    "build.md", "show-tree.md", "fixture-manager.md", "definition-updates.md",
    "stage-targets.md", "focus-calls.md", "venues.md", "refine.md", "shows.md",
    "show-control.md",
    "other-improvements.md", "effects.md", "shortcuts.md", "troubleshooting.md", "updates.md",
    "EFFECTSCRIPT_API.md"
};

/* A guide that moved: its old name, from a link or a topic kept from
   before, opens the new one (the headings came along). */
QString movedGuide(const QString &file)
{
    if (file == QLatin1String("programming-tab.md"))     // the Programming tab is Build now
        return QStringLiteral("build.md");
    return file;
}

QString readGuide(const QString &file)
{
    QFile f(HelpBrowser::resourcePath(file));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
#if defined(Q_OS_MACOS)
    const bool mac = true;
#else
    const bool mac = false;
#endif
    return HelpBrowser::keysForPlatform(QString::fromUtf8(f.readAll()), mac);
}

QString titleOf(const QString &file)
{
    const QString text = readGuide(file);
    for (const QString &line : text.split(QLatin1Char('\n')))
        if (line.startsWith(QLatin1String("# ")))   // as written: "Stage Targets &amp; …"
            return QTextDocumentFragment::fromHtml(line.mid(2).trimmed()).toPlainText();
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
    m_browser->viewport()->installEventFilter(this);
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
    QString file = movedGuide(topic.section(QLatin1Char('#'), 0, 0));
    if (file.isEmpty() || !QFile::exists(resourcePath(file)))
        file = QStringLiteral("getting-started.md");
    load(file, topic.section(QLatin1Char('#'), 1), true);
}

QString HelpBrowser::movedHeading(const QString &file, const QString &fragment)
{
    /* Headings renamed with the focus tools (board Q149): an old link
       (a bookmark, a course page) still lands on the heading. */
    static const QHash<QString, QString> moved = {
        { QStringLiteral("lighting-studio.md#focus-mode"), QStringLiteral("quick-focus") },
        { QStringLiteral("focus-calls.md#running-them"), QStringLiteral("running-venue-focus") },
        { QStringLiteral("focus-calls.md#a-focus-call-cue-pre-show"), QStringLiteral("a-venue-focus-cue-pre-show") },
        { QStringLiteral("refine.md#on-a-build-scene"), QStringLiteral("focus-this-look-on-a-build-scene") },
        { QStringLiteral("refine.md#refine-during-the-show"), QStringLiteral("focusing-during-the-show") },
    };
    return moved.value(file + QLatin1Char('#') + fragment, fragment);
}

void HelpBrowser::load(const QString &file, const QString &wanted, bool record)
{
    const QString fragment = movedHeading(file, wanted);
    if (file != m_file)
    {
        QTextDocument *doc = m_browser->document();
        doc->setBaseUrl(QUrl(QStringLiteral("qrc:/help/")));
        m_browser->setMarkdown(readGuide(file));
        m_file = file;

        m_fittedTo = 0;
        fitImages();

        /* Each heading an anchor, named as GitHub names it, for
           scrollToHeading(). */
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
        {
            if (b.blockFormat().headingLevel() <= 0 || b.length() <= 1)
                continue;
            QTextCursor c(b);
            c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            QTextCharFormat anchor;
            anchor.setAnchor(true);
            anchor.setAnchorNames({ headingSlug(b.text()) });
            c.mergeCharFormat(anchor);
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

/* The pictures are taken at twice their size (tools: help-shots.sh), as a
   Retina screen shows them: each is shown at half its pixels, or narrower to
   fit the page. Every one is a link to itself at full size -- a whole window
   fitted into a narrow page is a map, not something to read. */
void HelpBrowser::fitImages()
{
    const int room = qMax(240, m_browser->viewport()->width() - 40);
    if (qAbs(room - m_fittedTo) < 8)
        return;                         // a scroll bar coming and going
    m_fittedTo = room;

    QTextDocument *doc = m_browser->document();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
        for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it)
        {
            QTextFragment frag = it.fragment();
            if (!frag.isValid() || !frag.charFormat().isImageFormat())
                continue;
            QTextImageFormat img = frag.charFormat().toImageFormat();
            const QString path = QLatin1Char(':') + doc->baseUrl().resolved(QUrl(img.name())).path();
            const QSize size = QImageReader(path).size() / 2;
            if (!size.isValid() || size.isEmpty())
                continue;
            const int w = qMin(size.width(), room);
            img.setWidth(w);
            img.setHeight(double(size.height()) * w / size.width());
            img.setAnchor(true);
            img.setAnchorHref(QStringLiteral("picture:") + path);
            img.setToolTip(tr("Click to see it full size"));
            QTextCursor cur(doc);
            cur.setPosition(frag.position());
            cur.setPosition(frag.position() + frag.length(), QTextCursor::KeepAnchor);
            cur.setCharFormat(img);
        }
}

bool HelpBrowser::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_browser->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, &HelpBrowser::fitImages);
    return QWidget::eventFilter(watched, event);
}

void HelpBrowser::showImage(const QString &path)
{
    QPixmap pm(path);
    if (pm.isNull())
        return;
    pm.setDevicePixelRatio(2);

    QDialog *d = new QDialog(this);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setObjectName(QStringLiteral("helpPicture"));
    d->setWindowTitle(windowTitle());
    QVBoxLayout *lay = new QVBoxLayout(d);
    lay->setContentsMargins(0, 0, 0, 0);
    QScrollArea *sa = new QScrollArea(d);
    QLabel *pic = new QLabel(sa);
    pic->setPixmap(pm);
    sa->setWidget(pic);
    sa->setAlignment(Qt::AlignCenter);
    lay->addWidget(sa);

    // As big as the picture, as far as the screen allows.
    QSize want = pm.size() / 2 + QSize(4, 4);
    if (QScreen *scr = screen())
        want = want.boundedTo(scr->availableSize() * 0.95);
    d->resize(want);
    d->show();
}

void HelpBrowser::scrollToHeading(const QString &fragment)
{
    QTextDocument *doc = m_browser->document();
    for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
    {
        if (b.blockFormat().headingLevel() <= 0 || headingSlug(b.text()) != fragment)
            continue;
        m_heading = b.text();
        m_browser->setTextCursor(QTextCursor(b));
        /* By its anchor (load() names each heading): scrollToAnchor() lays
           the page out as far as it needs -- a long page is otherwise laid
           out lazily, and a heading not reached yet has no place to scroll
           to. Again once the window is laid out at its real width. */
        m_browser->scrollToAnchor(fragment);
        const QString file = m_file, heading = m_heading;
        QTimer::singleShot(0, this, [this, file, heading, fragment]() {
            if (m_file == file && m_heading == heading)
                m_browser->scrollToAnchor(fragment);
        });
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
    if (scheme == QLatin1String("picture"))
    {
        showImage(url.path());
        return;
    }
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
    const QString file = movedGuide(QUrl(QStringLiteral("qrc:/help/") + m_file).resolved(url).fileName());
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

QString HelpBrowser::keysForPlatform(const QString &text, bool mac)
{
    if (mac)
        return text;

    static const QString glyphs = QStringLiteral("\u2303\u2325\u21E7\u2318");  // ⌃ ⌥ ⇧ ⌘
    QString out;
    out.reserve(text.size() + text.size() / 8);
    int i = 0;
    while (i < text.size())
    {
        if (glyphs.contains(text.at(i)) == false)
        {
            out += text.at(i++);
            continue;
        }

        /* A run of modifiers, in whatever order the Mac writes them (⇧⌘),
           said in the order Windows and Linux do (Ctrl+Alt+Shift). Control
           and Command both come out as Ctrl: Qt maps ⌘ to Ctrl there. */
        bool ctrl = false, alt = false, shift = false;
        while (i < text.size() && glyphs.contains(text.at(i)))
        {
            const QChar g = text.at(i++);
            if (g == QChar(0x2325))
                alt = true;
            else if (g == QChar(0x21E7))
                shift = true;
            else
                ctrl = true;
        }
        QStringList mods;
        if (ctrl)
            mods << QStringLiteral("Ctrl");
        if (alt)
            mods << QStringLiteral("Alt");
        if (shift)
            mods << QStringLiteral("Shift");
        out += mods.join(QLatin1Char('+'));

        /* What follows: the key ("⌘Z", "⇧F1", "⌘↩", "⌥→", "⌘,"), or nothing
           -- "⌘-click", "⌘+Shift+R", "⇧ snaps", the end of a cell. */
        if (i >= text.size())
            break;
        const QChar next = text.at(i);
        if (next.isSpace() || next == QLatin1Char('-') || next == QLatin1Char('+')
            || next == QLatin1Char(')') || next == QLatin1Char('|') || next == QLatin1Char('/'))
            continue;
        out += QLatin1Char('+');
        if (next.isLetterOrNumber())
        {
            while (i < text.size() && text.at(i).isLetterOrNumber())
                out += text.at(i++);
        }
        else
        {
            if (next == QChar(0x21A9))          // ↩
                out += QStringLiteral("Enter");
            else if (next == QChar(0x238B))     // ⎋
                out += QStringLiteral("Esc");
            else if (next == QChar(0x232B))     // ⌫
                out += QStringLiteral("Backspace");
            else
                out += next;
            i++;
        }
    }

    /* Written for both already ("Ctrl/⌘+Z"), and the one line that explains
       the symbols, which off a Mac has nothing left to explain. */
    out.replace(QStringLiteral("Ctrl/Ctrl+"), QStringLiteral("Ctrl+"));
    out.replace(QStringLiteral("Ctrl is Ctrl on Windows and Linux; Alt is Alt."),
                QStringLiteral("On a Mac, Ctrl is \u2318 and Alt is \u2325."));
    return out;
}
