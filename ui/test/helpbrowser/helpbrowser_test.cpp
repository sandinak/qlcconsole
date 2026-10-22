/*
  Q Light Controller Plus - Test Unit
  helpbrowser_test.cpp

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

#include <QtTest>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QToolButton>
#include <QScrollBar>
#include <QListWidget>
#include <QLineEdit>
#include <QFile>
#include <QTextBlock>
#include <QTextDocumentFragment>

#define private public
#include "helpbrowser.h"
#undef private

#include "helpbrowser_test.h"

static QString guideText(const QString &file)
{
    QFile f(HelpBrowser::resourcePath(file));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

/* Every guide the help lists is in the app, has a title, and renders to
   something -- headings and all. */
void HelpBrowser_Test::everyGuideIsBundledAndLoads()
{
    const auto topics = HelpBrowser::topics();
    QVERIFY2(topics.size() >= 13, qPrintable(QString("only %1 guides bundled").arg(topics.size())));
    HelpBrowser help;
    for (const auto &t : topics)
    {
        QVERIFY2(!t.second.isEmpty() && t.second != t.first, qPrintable(t.first + " has no title"));
        help.showTopic(t.first);
        QCOMPARE(help.currentFile(), t.first);
        QVERIFY2(help.browser()->document()->characterCount() > 200, qPrintable(t.first + " renders empty"));
        /* All of it: an HTML-looking word in the text ("<effect>") makes the
           Markdown reader swallow the rest of the page. Its last heading must
           be there. */
        QString last;
        bool inFence = false;
        for (const QString &line : guideText(t.first).split(QLatin1Char('\n')))
        {
            if (line.startsWith(QLatin1String("```")))
                inFence = !inFence;
            else if (!inFence && line.startsWith(QLatin1String("## ")))
                last = line.mid(3).trimmed();
        }
        if (!last.isEmpty())
        {
            help.showTopic(t.first + QLatin1Char('#') + HelpBrowser::headingSlug(
                               QTextDocumentFragment::fromHtml(last.toHtmlEscaped().replace("&amp;", "&")).toPlainText()));
            QVERIFY2(!help.currentHeading().isEmpty(),
                     qPrintable(QString("%1 is cut short: its last heading \"%2\" isn't there").arg(t.first, last)));
        }
    }
}

/* The docs are written for the repository and read in the app: every link
   between guides must land on a bundled guide -- and on a heading in it,
   when it names one -- and every picture must be bundled. A guide edited
   with a broken link fails here, not in front of someone reading it. */
void HelpBrowser_Test::everyLinkInTheGuidesGoesSomewhere()
{
    const QRegularExpression link(QStringLiteral("!?\\[[^\\]]*\\]\\(([^)\\s]+)\\)"));
    const QRegularExpression fence(QStringLiteral("```.*?```"),
                                   QRegularExpression::DotMatchesEverythingOption);
    HelpBrowser help;
    int checked = 0;
    for (const auto &t : HelpBrowser::topics())
    {
        QString text = guideText(t.first);
        text.remove(fence);
        auto it = link.globalMatch(text);
        while (it.hasNext())
        {
            const QString target = it.next().captured(1);
            if (target.startsWith(QLatin1String("http")) || target.startsWith(QLatin1String("mailto:")))
                continue;
            const QString path = target.section(QLatin1Char('#'), 0, 0);
            const QString fragment = target.section(QLatin1Char('#'), 1);
            const QString where = QStringLiteral("%1 -> %2").arg(t.first, target);
            if (path.endsWith(QLatin1String(".png")))
            {
                const QUrl img = QUrl(QStringLiteral("qrc:/help/") + t.first).resolved(QUrl(path));
                QVERIFY2(QFile::exists(QLatin1Char(':') + img.path()), qPrintable("no picture: " + where));
                ++checked;
                continue;
            }
            const QString file = path.isEmpty() ? t.first : QUrl(path).fileName();
            QVERIFY2(QFile::exists(HelpBrowser::resourcePath(file)), qPrintable("no such guide: " + where));
            if (!fragment.isEmpty())
            {
                help.showTopic(file + QLatin1Char('#') + fragment);
                QVERIFY2(!help.currentHeading().isEmpty(), qPrintable("no such heading: " + where));
            }
            ++checked;
        }
    }
    QVERIFY2(checked > 50, qPrintable(QString("only %1 links found").arg(checked)));
}

/* Followed the way a reader does: a click on a link to another guide's
   heading opens that guide there; Back returns. */
void HelpBrowser_Test::aLinkToAHeadingScrollsToIt()
{
    HelpBrowser help;
    help.resize(900, 500);
    help.show();
    help.showTopic(QStringLiteral("getting-started.md"));
    help.followLink(QUrl(QStringLiteral("lighting-studio.md#mounting-a-fixture")));
    QCOMPARE(help.currentFile(), QString("lighting-studio.md"));
    QCOMPARE(help.currentHeading(), QString("Mounting a fixture"));
    QVERIFY2(help.browser()->verticalScrollBar()->value() > 0, "it should be scrolled down to the heading");
    // And it is the heading at the top of the view -- not just near it (a
    // long page is laid out lazily, and scrolling by its layout landed past).
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
    QCOMPARE(help.browser()->cursorForPosition(QPoint(20, 12)).block().text(), QString("Mounting a fixture"));
    help.m_back->click();
    QCOMPARE(help.currentFile(), QString("getting-started.md"));
    // A link written for the repository, from one folder to another.
    help.showTopic(QStringLiteral("effects.md"));
    help.followLink(QUrl(QStringLiteral("../../EFFECTSCRIPT_API.md")));
    QCOMPARE(help.currentFile(), QString("EFFECTSCRIPT_API.md"));
    // Unknown topics fall back to Getting Started.
    help.showTopic(QStringLiteral("nonsense.md"));
    QCOMPARE(help.currentFile(), QString("getting-started.md"));
    // A guide that moved: the Programming tab's is the Build tab's now, its
    // headings with it -- a topic or a link kept from before lands there.
    help.showTopic(QStringLiteral("programming-tab.md#the-effect-browser"));
    QCOMPARE(help.currentFile(), QString("build.md"));
    QCOMPARE(help.currentHeading(), QString("The effect browser"));
    help.followLink(QUrl(QStringLiteral("programming-tab.md#the-outline")));
    QCOMPARE(help.currentFile(), QString("build.md"));
    QCOMPARE(help.currentHeading(), QString("The outline"));
}

void HelpBrowser_Test::searchFindsTheGuidesThatSayIt()
{
    HelpBrowser help;
    const int all = help.m_topics->count();
    help.m_search->setText(QStringLiteral("Shift"));
    QVERIFY(help.m_topics->count() > 0 && help.m_topics->count() < all);
    help.m_search->setText(QStringLiteral("zzzqqq-no-such-word"));
    QCOMPARE(help.m_topics->count(), 0);
    help.m_search->clear();
    QCOMPARE(help.m_topics->count(), all);
}

/* "windows shift r doesn't work": the guides said ⇧⌘R, and on Windows the
   only key with a symbol like that is the Windows key. Off a Mac they say
   Ctrl+Shift+R. */
void HelpBrowser_Test::keysAreSpelledForTheComputerYouAreOn()
{
    const QString mac = QString::fromUtf8(
        "**View \u2192 Rig Overview** (\u21E7\u2318R) opens it; \u2318Z, \u2318F12, \u2318, | "
        "\u2318\u21E7\u21A9 \u00B7 \u2325\u2190 \u00B7 \u2318- or \u21E7-click \u00B7 "
        "Ctrl/\u2318+Shift+R \u00B7 \u21E7 snaps \u00B7 (\u2318O, or");
    QCOMPARE(HelpBrowser::keysForPlatform(mac, true), mac);
    QCOMPARE(HelpBrowser::keysForPlatform(mac, false), QString::fromUtf8(
        "**View \u2192 Rig Overview** (Ctrl+Shift+R) opens it; Ctrl+Z, Ctrl+F12, Ctrl+, | "
        "Ctrl+Shift+Enter \u00B7 Alt+\u2190 \u00B7 Ctrl- or Shift-click \u00B7 "
        "Ctrl+Shift+R \u00B7 Shift snaps \u00B7 (Ctrl+O, or"));

    // Every guide, read the way Windows reads it, has no Mac symbol left.
    for (const auto &t : HelpBrowser::topics())
    {
        QFile f(HelpBrowser::resourcePath(t.first));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = HelpBrowser::keysForPlatform(QString::fromUtf8(f.readAll()), false);
        QVERIFY2(text.contains(QChar(0x2318)) == false || t.first == QLatin1String("shortcuts.md"),
                 qPrintable(t.first));
    }
}

void HelpBrowser_Test::headingsAreSluggedTheWayGitHubDoes()
{
    QCOMPARE(HelpBrowser::headingSlug("Connections & patch"), QString("connections--patch"));
    QCOMPARE(HelpBrowser::headingSlug("Angled — the free 3D view"), QString("angled--the-free-3d-view"));
    QCOMPARE(HelpBrowser::headingSlug("A fixture's maximum draw"), QString("a-fixtures-maximum-draw"));
    QCOMPARE(HelpBrowser::headingSlug("Patching: the Patch dialog"), QString("patching-the-patch-dialog"));
}

/* A picture is shown no wider than the page -- a narrow page shrinks it,
   a wider one gives it back, up to its own size (taken at 2x, shown at
   half) -- and a click on it opens it full size. */
void HelpBrowser_Test::aPictureFitsThePageAndOpensFullSize()
{
    HelpBrowser help;
    help.resize(560, 600);
    help.show();
    QVERIFY(QTest::qWaitForWindowExposed(&help));
    help.showTopic(QStringLiteral("getting-started.md"));

    auto picture = [&help]() {
        QTextDocument *doc = help.browser()->document();
        for (QTextBlock b = doc->begin(); b.isValid(); b = b.next())
            for (QTextBlock::iterator it = b.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().isImageFormat())
                    return it.fragment().charFormat().toImageFormat();
        return QTextImageFormat();
    };

    QTextImageFormat img = picture();
    QVERIFY(img.isValid());
    const QString href = img.anchorHref();
    QVERIFY2(href.startsWith(QLatin1String("picture::/images/")), qPrintable(href));
    const QSize full = QImage(href.mid(8)).size() / 2;
    QVERIFY(!full.isEmpty());
    QVERIFY2(img.width() <= help.browser()->viewport()->width(),
             qPrintable(QString("%1 wide on a %2 page").arg(img.width()).arg(help.browser()->viewport()->width())));
    const qreal narrow = img.width();

    help.resize(1800, 900);
    QTRY_VERIFY(picture().width() > narrow);
    QVERIFY(picture().width() <= full.width());

    help.followLink(QUrl(href));
    QWidget *shown = help.findChild<QWidget *>(QStringLiteral("helpPicture"));
    QVERIFY(shown != nullptr);
    QVERIFY(shown->isVisible());
}

QTEST_MAIN(HelpBrowser_Test)
