/*
  Q Light Controller Plus - Unit test
  dllsignals_test.cpp

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
#include <QDirIterator>
#include <QRegularExpression>
#include <QFile>

#include "dllsignals_test.h"

namespace
{

/* @p text with its comments blanked (newlines kept, so line numbers hold)
   and string and character literals left alone. */
QString stripComments(const QString &text)
{
    QString out = text;
    enum State { Code, Line, Block, String, Char } st = Code;
    for (int i = 0; i < out.size(); i++)
    {
        const QChar c = out.at(i);
        const QChar n = i + 1 < out.size() ? out.at(i + 1) : QChar();
        switch (st)
        {
            case Code:
                if (c == '/' && n == '/')
                {
                    st = Line;
                    out[i] = ' ';
                }
                else if (c == '/' && n == '*')
                {
                    st = Block;
                    out[i] = ' ';
                    out[++i] = ' ';
                }
                else if (c == '"')
                {
                    st = String;
                }
                else if (c == '\'')
                {
                    st = Char;
                }
            break;
            case Line:
                if (c == '\n')
                    st = Code;
                else
                    out[i] = ' ';
            break;
            case Block:
                if (c == '*' && n == '/')
                {
                    out[i] = ' ';
                    out[++i] = ' ';
                    st = Code;
                }
                else if (c != '\n')
                {
                    out[i] = ' ';
                }
            break;
            case String:
            case Char:
                if (c == '\\')
                    i++;
                else if ((st == String && c == '"') || (st == Char && c == '\'') || c == '\n')
                    st = Code;
            break;
        }
    }
    return out;
}

/* The bracket that encloses position @p at of @p text: '(', '[', '{' or
   0 when none is found nearby. */
QChar enclosingBracket(const QString &text, int at)
{
    int depth = 0;
    for (int i = at - 1; i >= 0 && i > at - 4000; i--)
    {
        const QChar c = text.at(i);
        if (c == ')' || c == ']' || c == '}')
        {
            depth++;
        }
        else if (c == '(' || c == '[' || c == '{')
        {
            if (depth == 0)
                return c;
            depth--;
        }
    }
    return QChar();
}

} // namespace

QString DllSignals_Test::moduleOf(const QString &rel)
{
    const QStringList parts = rel.split('/');
    const int t = parts.indexOf(QStringLiteral("test"));
    if (t >= 0 && t + 1 < parts.size())
        return QStringLiteral("test:") + parts.mid(0, qMin(t + 2, int(parts.size()) - 1)).join('/');
    if (rel == QLatin1String("plugins/interfaces/qlcioplugin.h"))
        return QStringLiteral("qlcioplugin");     // built into every plugin, and the engine
    if (rel.startsWith(QLatin1String("engine/src/")) || rel.startsWith(QLatin1String("engine/audio/src/")))
        return QStringLiteral("engine");
    if (rel.startsWith(QLatin1String("ui/src/")))
        return QStringLiteral("ui");
    if (rel.startsWith(QLatin1String("webaccess/src/")))
        return QStringLiteral("webaccess");
    if (parts.size() > 2 && parts.at(0) == QLatin1String("plugins"))
        return QStringLiteral("plugin:") + parts.at(1);
    return parts.value(0);
}

bool DllSignals_Test::isDllModule(const QString &module)
{
    return module == QLatin1String("engine") || module == QLatin1String("ui")
           || module == QLatin1String("webaccess") || module.startsWith(QLatin1String("plugin:"));
}

const DllSignals_Test::ClassInfo *DllSignals_Test::signalOwner(const QString &cls, const QString &member,
                                                                const QString &fromModule) const
{
    QString name = cls;
    QString nearPt = fromModule;
    for (int hops = 0; hops < 16 && !name.isEmpty(); hops++)
    {
        const auto found = m_classes.constFind(name);
        if (found == m_classes.constEnd() || found.value().isEmpty())
            return nullptr;
        const QList<ClassInfo> &cands = found.value();
        // A name used by two apps (App): the one in the referring module.
        const ClassInfo *ci = &cands.first();
        for (const ClassInfo &c : cands)
        {
            if (c.module == nearPt)
            {
                ci = &c;
                break;
            }
        }
        if (ci->signalNames.contains(member))
            return ci;
        name = ci->base.section(QStringLiteral("::"), -1);
        nearPt = ci->module;
    }
    return nullptr;
}

QString DllSignals_Test::describe(const Ref &r) const
{
    return QStringLiteral("%1:%2: &%3::%4 (%5, declared in %6 [%7])")
        .arg(r.file).arg(r.line).arg(r.cls, r.member, r.decl->name, r.decl->file, r.decl->module);
}

void DllSignals_Test::initTestCase()
{
    m_root = QStringLiteral(DLLSIGNALS_SOURCE_DIR);
    const QStringList roots = { "engine", "ui", "webaccess", "plugins", "main", "fixtureeditor", "launcher" };
    for (const QString &top : roots)
    {
        QDirIterator it(m_root + '/' + top, { "*.h", "*.cpp" }, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            const QString rel = QDir(m_root).relativeFilePath(it.next());
            if (rel.contains(QLatin1String("CMakeFiles")) || rel.contains(QLatin1String("_autogen/")))
                continue;
            m_files << rel;
        }
    }
    m_files.sort();
    QVERIFY2(m_files.size() > 500, qPrintable(QString("only %1 sources under %2").arg(m_files.size()).arg(m_root)));

    QHash<QString, QString> texts;
    for (const QString &rel : std::as_const(m_files))
    {
        QFile f(m_root + '/' + rel);
        QVERIFY(f.open(QIODevice::ReadOnly));
        texts.insert(rel, stripComments(QString::fromUtf8(f.readAll())));
    }

    // The classes, their export macro, base and signals.
    const QRegularExpression classRe(QStringLiteral(
        "^\\s*class\\s+(?:([A-Z][A-Z0-9_]*EXPORT|QLC_DECLSPEC)\\s+)?(\\w+)(?:\\s+final)?\\s*"
        "(?::\\s*(?:public|protected|private)?\\s*([\\w:]+))?[^;]*$"));
    // "class QLC_ENGINE_EXPORT Function;" exports the class defined later.
    const QRegularExpression fwdRe(QStringLiteral(
        "^\\s*class\\s+([A-Z][A-Z0-9_]*EXPORT|QLC_DECLSPEC)\\s+(\\w+)\\s*;"));
    QSet<QString> fwdExported;
    const QRegularExpression sigStart(QStringLiteral("^(?:signals|Q_SIGNALS)\\s*:"));
    const QRegularExpression access(QStringLiteral("^(?:public|private|protected)\\b[^:]*:"));
    const QRegularExpression sigDecl(QStringLiteral("^void\\s+(\\w+)\\s*\\("));
    for (const QString &rel : std::as_const(m_files))
    {
        if (!rel.endsWith(QLatin1String(".h")))
            continue;
        ClassInfo *cur = nullptr;
        bool inSignals = false;
        const QStringList lines = texts.value(rel).split('\n');
        for (const QString &line : lines)
        {
            const QRegularExpressionMatch fwd = fwdRe.match(line);
            if (fwd.hasMatch())
                fwdExported.insert(moduleOf(rel) + '/' + fwd.captured(2));
            const QRegularExpressionMatch m = classRe.match(line);
            if (m.hasMatch())
            {
                ClassInfo ci;
                ci.name = m.captured(2);
                ci.module = moduleOf(rel);
                ci.file = rel;
                ci.base = m.captured(3);
                ci.exported = !m.captured(1).isEmpty();
                m_classes[ci.name].append(ci);
                cur = &m_classes[ci.name].last();
                inSignals = false;
                continue;
            }
            const QString s = line.trimmed();
            if (sigStart.match(s).hasMatch())
            {
                inSignals = true;
                continue;
            }
            if (access.match(s).hasMatch())
            {
                inSignals = false;
                continue;
            }
            if (inSignals && cur != nullptr)
            {
                const QRegularExpressionMatch d = sigDecl.match(s);
                if (d.hasMatch())
                    cur->signalNames.insert(d.captured(1));
            }
        }
    }

    for (auto it = m_classes.begin(); it != m_classes.end(); ++it)
    {
        for (ClassInfo &ci : it.value())
            if (fwdExported.contains(ci.module + '/' + ci.name))
                ci.exported = true;
    }

    /* A test that compiles a class's own source (qlcioplugin.cpp, a
       plugin's) has that class in its own binary: no DLL in between. */
    QHash<QString, QString> testSources;   // test module -> its CMakeLists.txt
    for (const QString &rel : std::as_const(m_files))
    {
        const QString mod = moduleOf(rel);
        if (!mod.startsWith(QLatin1String("test:")) || testSources.contains(mod))
            continue;
        QFile cm(m_root + '/' + mod.mid(5) + QStringLiteral("/CMakeLists.txt"));
        testSources.insert(mod, cm.open(QIODevice::ReadOnly) ? QString::fromUtf8(cm.readAll()) : QString());
    }

    // Every &Class::signal outside the module that declares it.
    const QRegularExpression refRe(QStringLiteral("&\\s*(\\w+)::(\\w+)\\b"));
    for (const QString &rel : std::as_const(m_files))
    {
        const QString text = texts.value(rel);
        const QString mod = moduleOf(rel);
        QRegularExpressionMatchIterator it = refRe.globalMatch(text);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            const ClassInfo *decl = signalOwner(m.captured(1), m.captured(2), mod);
            if (decl == nullptr || decl->module == mod)
                continue;
            const QString source = QFileInfo(decl->file).completeBaseName() + QStringLiteral(".cpp");
            if (testSources.value(mod).contains(QLatin1Char('/') + source))
                continue;
            Ref r;
            r.file = rel;
            r.line = text.left(m.capturedStart()).count('\n') + 1;
            r.cls = m.captured(1);
            r.member = m.captured(2);
            r.decl = decl;
            r.inBracedList = enclosingBracket(text, m.capturedStart()) == QLatin1Char('{');
            m_refs << r;
        }
    }
}

void DllSignals_Test::theScanSeesTheTree()
{
    const QList<ClassInfo> docs = m_classes.value("Doc");
    QVERIFY(!docs.isEmpty());
    QCOMPARE(docs.first().module, QString("engine"));
    QVERIFY(docs.first().exported);
    QVERIFY(docs.first().signalNames.contains("functionAdded"));
    QVERIFY(docs.first().signalNames.contains("fixtureGroupChanged"));

    const QList<ClassInfo> plugin = m_classes.value("QLCIOPlugin");
    QVERIFY(!plugin.isEmpty());
    QCOMPARE(plugin.first().module, QString("qlcioplugin"));
    QVERIFY(plugin.first().signalNames.contains("inputDiagnostic"));

    // A signal inherited: Scene's changed() is Function's.
    const ClassInfo *owner = signalOwner("Scene", "changed", "ui");
    QVERIFY(owner != nullptr);
    QCOMPARE(owner->name, QString("Function"));

    // The UI names Doc's signals by member pointer a great many times.
    int docRefs = 0;
    for (const Ref &r : std::as_const(m_refs))
        if (r.decl->name == QLatin1String("Doc") && moduleOf(r.file) == QLatin1String("ui"))
            docRefs++;
    QVERIFY2(docRefs > 100, qPrintable(QString::number(docRefs)));

    QCOMPARE(moduleOf("ui/src/monitor/monitor.cpp"), QString("ui"));
    QCOMPARE(moduleOf("ui/test/monitor/monitor_test.cpp"), QString("test:ui/test/monitor"));
    QCOMPARE(moduleOf("plugins/artnet/test/artnet_test.cpp"), QString("test:plugins/artnet/test"));
    QCOMPARE(moduleOf("plugins/artnet/src/artnetplugin.cpp"), QString("plugin:artnet"));
    QCOMPARE(enclosingBracket("for (auto s : { &Doc::a, &Doc::b })", 15), QChar('{'));
    QCOMPARE(enclosingBracket("connect(d, &Doc::a, this, [this]() { f(); });", 11), QChar('('));
}

void DllSignals_Test::signalsAcrossADllAreExported()
{
    QStringList bad;
    for (const Ref &r : std::as_const(m_refs))
    {
        if (r.decl->module == QLatin1String("qlcioplugin") || r.decl->module.startsWith(QLatin1String("plugin:")))
            continue;       // pluginSignalsAreConnectedByName()
        if (!isDllModule(r.decl->module) || r.decl->exported)
            continue;
        bad << describe(r);
    }
    QVERIFY2(bad.isEmpty(), qPrintable("Not exported from their DLL, so these connect to nothing on Windows."
                                       " Mark the class QLC_ENGINE_EXPORT / QLC_UI_EXPORT:\n  "
                                       + bad.join("\n  ")));
}

void DllSignals_Test::pluginSignalsAreConnectedByName()
{
    QStringList bad;
    for (const Ref &r : std::as_const(m_refs))
    {
        if (r.decl->module == QLatin1String("qlcioplugin"))
        {
            // A plugin's own copy, compiled into it: its own address.
            if (!moduleOf(r.file).startsWith(QLatin1String("plugin:")))
                bad << describe(r);
        }
        else if (r.decl->module.startsWith(QLatin1String("plugin:")))
        {
            bad << describe(r);
        }
    }
    QVERIFY2(bad.isEmpty(), qPrintable("A plugin's signals from outside it: connect by name"
                                       " (SIGNAL(...)), every plugin DLL has its own QLCIOPlugin:\n  "
                                       + bad.join("\n  ")));
}

void DllSignals_Test::noBracedListsOfSignals()
{
    QStringList bad;
    for (const Ref &r : std::as_const(m_refs))
        if (r.inBracedList && isDllModule(r.decl->module))
            bad << describe(r);
    QVERIFY2(bad.isEmpty(), qPrintable("A braced list of another DLL's signals connects none of them on"
                                       " Windows: one connect() per signal:\n  " + bad.join("\n  ")));
}

QTEST_GUILESS_MAIN(DllSignals_Test)
