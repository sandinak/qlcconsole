/*
  Q Light Controller Plus
  menuwords_test.cpp

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

#include <QDirIterator>
#include <QRegularExpression>
#include <QFile>
#include <QtTest>

#include "menuwords_test.h"
#include "contextmenus.h"

namespace
{

struct Label
{
    QString file;   ///< relative to ui/src
    int line;
    QString text;
};

/* A C string literal's body as the compiler reads it: \" \\ \n and the
   octal escapes (\342\200\246 is "…") */
QString unescape(const QString &raw)
{
    QByteArray out;
    const QByteArray in = raw.toUtf8();
    for (int i = 0; i < in.size(); ++i)
    {
        const char c = in.at(i);
        if (c != '\\' || i + 1 >= in.size())
        {
            out.append(c);
            continue;
        }
        const char n = in.at(i + 1);
        if (n >= '0' && n <= '7')
        {
            int v = 0, k = 0;
            while (k < 3 && i + 1 + k < in.size() && in.at(i + 1 + k) >= '0' && in.at(i + 1 + k) <= '7')
            {
                v = v * 8 + (in.at(i + 1 + k) - '0');
                ++k;
            }
            out.append(char(v));
            i += k;
            continue;
        }
        out.append(n == 'n' ? '\n' : n == 't' ? '\t' : n);
        ++i;
    }
    return QString::fromUtf8(out);
}

const QRegularExpression &trRe()
{
    static const QRegularExpression re(QStringLiteral("tr\\(\"((?:[^\"\\\\]|\\\\.)*)\""));
    return re;
}

QList<QPair<QString, QStringList>> sources()
{
    QList<QPair<QString, QStringList>> out;
    const QString root = QStringLiteral(UI_SRC_DIR);
    QDirIterator it(root, { QStringLiteral("*.cpp") }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        out << qMakePair(QDir(root).relativeFilePath(path),
                         QString::fromUtf8(f.readAll()).split(QLatin1Char('\n')));
    }
    return out;
}

/* Every label given to a menu in ui/src: the tr() texts in the arguments of
   addAction(), addMenu(), ContextMenu's add() and a new QAction (a ternary's both sides),
   up to a lambda or a tooltip. A disabled line of information (a note in a
   menu, "(no folder)") is not an item to say. */
QList<Label> menuLabels()
{
    static const QRegularExpression call(QStringLiteral("(addAction|addMenu|\\.add|new QAction)\\("));
    static const QStringList stops = { QStringLiteral("["), QStringLiteral("connect"),
                                       QStringLiteral("setToolTip"), QStringLiteral("setStatusTip"),
                                       QStringLiteral("setWhatsThis") };
    QList<Label> out;
    for (const auto &src : sources())
    {
        const QStringList &lines = src.second;
        for (int i = 0; i < lines.size(); ++i)
        {
            const QString &l = lines.at(i);
            const QRegularExpressionMatch m = call.match(l);
            if (!m.hasMatch())
                continue;
            QString rest = l.mid(m.capturedEnd());
            if (m.captured(1) == QLatin1String(".add") && !rest.contains(QLatin1String("ContextMenu::")))
                continue;
            if (rest.trimmed().isEmpty() && i + 1 < lines.size())
                rest = lines.at(i + 1);
            int cut = rest.size();
            for (const QString &s : stops)
            {
                const int k = rest.indexOf(s);
                if (k >= 0)
                    cut = qMin(cut, k);
            }
            const QString next = i + 1 < lines.size() ? lines.at(i + 1) : QString();
            const bool note = l.count(QLatin1String("tr(")) == 1
                              && (l + next).contains(QLatin1String("setEnabled(false)"));
            if (note)
                continue;
            QRegularExpressionMatchIterator t = trRe().globalMatch(rest.left(cut));
            while (t.hasNext())
                out << Label{ src.first, i + 1, unescape(t.next().captured(1)) };
        }
    }
    return out;
}

/* What is wrong with @p text as a menu label, by the canon's words */
QStringList problemsOf(const QString &text)
{
    QStringList p;
    // A line of information, a question or a prompt: not an item's name.
    if (text.isEmpty() || text.at(0).isSpace() || text.startsWith(QLatin1Char('('))
        || text.startsWith(QStringLiteral("⚠")) || text.startsWith(QStringLiteral("…"))
        || text.endsWith(QLatin1Char('?')) || text.endsWith(QLatin1Char(':')))
        return p;
    if (text.contains(QLatin1String("...")))
        p << QStringLiteral("\"...\" instead of \"…\"");
    if (text.contains(QRegularExpression(QStringLiteral("\\((Ctrl|Shift|Alt|⌘)"))))
        p << QStringLiteral("a shortcut typed into the label (set it on the action)");
    // The app's own name is written as it is.
    QString words = text;
    words.remove(QStringLiteral("qlcconsole"));
    const QString tc = ContextMenu::checkTitleCase(words);
    if (!tc.isEmpty())
        p << tc;
    return p;
}

} // namespace

void MenuWords_Test::theReaderFindsLabels()
{
    QCOMPARE(unescape(QStringLiteral("Focus This Cue\\342\\200\\246")), QStringLiteral("Focus This Cue…"));
    QCOMPARE(unescape(QStringLiteral("Say \\\"Hi\\\"")), QStringLiteral("Say \"Hi\""));

    const QList<Label> labels = menuLabels();
    // A few hundred labels: if the reader broke, it would find none and
    // pass everything.
    QVERIFY2(labels.size() > 400, qPrintable(QString::number(labels.size())));
    bool add = false, edit = false;
    for (const Label &l : labels)
    {
        add = add || l.text == QStringLiteral("Add Universe");
        edit = edit || l.text == QStringLiteral("Print Labels for the List Shown…");
    }
    QVERIFY(add);
    QVERIFY(edit);

    // And the words it holds them to.
    QVERIFY(!problemsOf(QStringLiteral("Delete universe entirely…")).isEmpty());
    QVERIFY(!problemsOf(QStringLiteral("Fixture Editor...")).isEmpty());
    QVERIFY(!problemsOf(QStringLiteral("Select All (Ctrl+A)")).isEmpty());
    QVERIFY(problemsOf(QStringLiteral("Duplicate Setup to…")).isEmpty());
    QVERIFY(problemsOf(QStringLiteral("Move Up (preferred)")).isEmpty());
    QVERIFY(problemsOf(QStringLiteral("(no folder)")).isEmpty());
}

void MenuWords_Test::everyMenuLabelIsTitleCase()
{
    QStringList bad;
    for (const Label &l : menuLabels())
    {
        const QStringList p = problemsOf(l.text);
        if (!p.isEmpty())
            bad << QStringLiteral("%1:%2  \"%3\"  %4").arg(l.file).arg(l.line).arg(l.text, p.join(QStringLiteral("; ")));
    }
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral("\n%1 menu label(s) off the canon "
                                                      "(docs/design/CONTEXT_MENUS.md §3.2):\n  ")
                                           .arg(bad.size()) + bad.join(QStringLiteral("\n  "))));
}

void MenuWords_Test::noLengthIsMetresOnly()
{
    /* "%1 m" or "(metres)" in a text the person reads, with no feet text
       beside it (a feet branch within a few lines): a show in feet would
       read metres there. Format with MonitorProperties::formatStageDistance()
       or MeasurementUnits::formatLength(), or use a LengthSpinBox. */
    static const QRegularExpression metres(QStringLiteral("%\\d+ ?m\\b(?!/)|\\((metres|meters)\\)"));
    static const QRegularExpression feet(QStringLiteral("tr\\(\"[^\"]*(\\bft\\b|feet|%\\d+ ?in\\b|\\(feet\\))"));
    QStringList bad;
    for (const auto &src : sources())
    {
        const QStringList &lines = src.second;
        for (int i = 0; i < lines.size(); ++i)
        {
            QRegularExpressionMatchIterator t = trRe().globalMatch(lines.at(i));
            while (t.hasNext())
            {
                const QString text = unescape(t.next().captured(1));
                if (!metres.match(text).hasMatch())
                    continue;
                bool paired = false;
                for (int k = qMax(0, i - 4); k < qMin(lines.size(), i + 5) && !paired; ++k)
                    paired = feet.match(lines.at(k)).hasMatch();
                if (!paired)
                    bad << QStringLiteral("%1:%2  \"%3\"").arg(src.first).arg(i + 1).arg(text);
            }
        }
    }
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral("\nmetres only (Settings > Units):\n  ")
                                           + bad.join(QStringLiteral("\n  "))));
}

void MenuWords_Test::theTabsHaveTheirNames()
{
    /* The Build tab was "Programming" (CLAUDE.md); a text that sends the
       person to "Programming" sends them to a tab that is not there. */
    static const QRegularExpression old(QStringLiteral("\\bProgramming\\b"));
    QStringList bad;
    for (const auto &src : sources())
    {
        const QStringList &lines = src.second;
        for (int i = 0; i < lines.size(); ++i)
        {
            QRegularExpressionMatchIterator t = trRe().globalMatch(lines.at(i));
            while (t.hasNext())
            {
                const QString text = unescape(t.next().captured(1));
                if (old.match(text).hasMatch())
                    bad << QStringLiteral("%1:%2  \"%3\"").arg(src.first).arg(i + 1).arg(text);
            }
        }
    }
    QVERIFY2(bad.isEmpty(), qPrintable(QStringLiteral("\nthe old tab name:\n  ") + bad.join(QStringLiteral("\n  "))));
}

QTEST_MAIN(MenuWords_Test)
