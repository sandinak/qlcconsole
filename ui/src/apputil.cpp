/*
  Q Light Controller
  apputil.cpp

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

#include <QComboBox>
#include <QStyleFactory>
#include <QApplication>
#include <QCoreApplication>
#include <QMessageBox>
#include <QTextStream>
#include <QSettings>
#include <QProcess>
#include <QLocale>
#include <QWidget>
#include <QScreen>
#include <QDebug>
#include <QStyle>
#include <QRect>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QFont>
#include <QColor>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QMap>
#include <algorithm>

#if defined(WIN32) || defined(Q_OS_WIN)
#include <windows.h>
#include <lmcons.h>
#endif

#include "apputil.h"
#include <QSet>
#include "qlcfile.h"
#include "qlcconfig.h"
#include "qlcpalette.h"
#include "scene.h"
#include "doc.h"
#include "fixture.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"
#include "showstatus.h"
#include "qlcioplugin.h"
#include "ioplugincache.h"
#include "fixturedefcheck.h"
#include "inputoutputmap.h"
#include "outputpatch.h"
#include "universe.h"
#include "qlcfixturedefcache.h"

/****************************************************************************
 * Application identity
 ****************************************************************************/

QIcon AppUtil::applicationIcon(const QString &name)
{
    // The sizes resources/icons/hicolor/render.sh produces (appicons.qrc).
    static const int sizes[] = { 16, 22, 24, 32, 48, 64, 128, 256 };

    QIcon icon;
    for (int size : sizes)
    {
        const QString path = QString(":/appicon/%1x%1/apps/%2.png").arg(size).arg(name);
        if (QFile::exists(path))
            icon.addFile(path, QSize(size, size));
    }
    return icon;
}

void AppUtil::setApplicationIdentity(const QString &name)
{
    QGuiApplication::setDesktopFileName(name);
#if !defined(Q_OS_MACOS)
    QApplication::setWindowIcon(applicationIcon(name));
#endif
}

/****************************************************************************
 * Widget visibility helper
 ****************************************************************************/

void AppUtil::ensureWidgetIsVisible(QWidget *widget)
{
    if (widget == NULL)
        return;

    QWidget *parent = widget->parentWidget();
    if (widget->windowFlags() & Qt::Window)
    {
        // The widget is a top-level window (a dialog, for instance)
        // @todo Use the screen where the main application currently is?
        QScreen *screen = QGuiApplication::screens().first();
        if (screen != NULL)
        {
            // Move the widget to the center of the default screen
            const QRect screenRect(screen->availableGeometry());
            if (screenRect.contains(widget->pos()) == false)
            {
                QRect widgetRect(widget->rect());
                widgetRect.moveCenter(screenRect.center());
                widget->setGeometry(widgetRect);
            }
        }
        else
        {
            // Last resort: move to top left and hope the widget is visible
            widget->move(0, 0);
        }
    }
    else if (parent != NULL)
    {
        // The widget's placement is bounded by a parent
        const QRect parentRect(parent->rect());
        if (parentRect.contains(widget->pos()) == false)
        {
            // Move the widget to the center of the parent if wouldn't
            // otherwise be visible
            QRect widgetRect(widget->rect());
            widgetRect.moveCenter(parentRect.center());
            widget->setGeometry(widgetRect);
        }
    }
}

void AppUtil::ensureWindowOnScreen(QWidget *window, QSize minSize)
{
    if (window == NULL)
        return;

    QScreen *scr = QGuiApplication::screenAt(window->frameGeometry().center());
    if (scr == NULL)
        scr = QGuiApplication::primaryScreen();
    if (scr == NULL)
        return;

    const QRect avail = scr->availableGeometry();
    QRect g = window->geometry();

    // Floor first: a restoreGeometry() that silently failed (Qt version/
    // format mismatch, corruption) leaves whatever tiny default size the
    // widget had right after construction -- present and "on screen" by
    // coordinate, but unusable. Only widen up to what the screen can hold.
    QSize size = g.size().expandedTo(minSize).boundedTo(avail.size());
    g.setSize(size);

    if (g.right()  > avail.right())  g.moveRight(avail.right());
    if (g.bottom() > avail.bottom()) g.moveBottom(avail.bottom());
    if (g.left()   < avail.left())   g.moveLeft(avail.left());
    if (g.top()    < avail.top())    g.moveTop(avail.top());

    window->setGeometry(g);
}

/*****************************************************************************
 * Sane style
 *****************************************************************************/

#define SETTINGS_SLIDERSTYLE "workspace/sliderstyle"

static QStyle* s_saneStyle = NULL;

QStyle* AppUtil::saneStyle()
{
    if (s_saneStyle == NULL)
    {
        QSettings settings;
        QVariant var = settings.value(SETTINGS_SLIDERSTYLE, QString("Fusion"));
        QStringList keys(QStyleFactory::keys());

        if (keys.contains(var.toString()) == true)
            s_saneStyle = QStyleFactory::create(var.toString());
        else
            s_saneStyle = QApplication::style();
    }

    return s_saneStyle;
}

/*********************************************************************
 * Stylesheets
 *********************************************************************/

#define USER_STYLESHEET_FILE "qlcplusStyle.qss"

bool styleCached = false;
QMap<QString,QString> styleCache;

// Bundled default chrome QSS (resources/qss/default.qss) — read once, applied
// only to the "MAIN" component, ahead of the user's own override block below
// so ~/.qlcconsole/qlcplusStyle.qss still cascades on top and can override it.
static QString bundledMainStyleSheet()
{
    static QString bundled;
    static bool loaded = false;
    if (loaded == false)
    {
        QFile defFile(":/style/default.qss");
        if (defFile.open(QIODevice::ReadOnly))
            bundled = QTextStream(&defFile).readAll();
        loaded = true;
    }
    return bundled;
}

static QString withBundledDefault(const QString &component, const QString &userBlock)
{
    if (component != "MAIN")
        return userBlock;
    return bundledMainStyleSheet() + "\n" + userBlock;
}

QString AppUtil::getStyleSheet(QString component)
{
    QString block;

    if (styleCached == false)
    {
#if defined(WIN32) || defined(Q_OS_WIN)
        /* User's input profile directory on Windows */
        LPTSTR home = (LPTSTR) malloc(256 * sizeof(TCHAR));
        GetEnvironmentVariable(TEXT("UserProfile"), home, 256);
        QString ssDir = QString("%1/%2").arg(QString::fromUtf16(reinterpret_cast<char16_t*> (home)))
                    .arg(USERQLCPLUSDIR);
        free(home);
#else
        /* User's input profile directory on *NIX systems */
        QString ssDir = QString("%1/%2").arg(getenv("HOME")).arg(USERQLCPLUSDIR);
#endif

        styleCached = true;

        QFile ssFile(ssDir + QDir::separator() + USER_STYLESHEET_FILE);
        if (ssFile.exists() == false)
            return withBundledDefault(component, block);

        if (ssFile.open(QIODevice::ReadOnly) == false)
            return withBundledDefault(component, block);

        bool found = false;
        QString compName;
        QTextStream in(&ssFile);

        while (!in.atEnd())
        {
            QString line = in.readLine();
            if (line.startsWith("====="))
            {
                if (found == true)
                {
                    styleCache.insert(compName, block);
                    block = "";
                    found = false;
                }

                compName = line.replace("=", "").simplified();
                qDebug() << "[AppUtil] found user style component:" << compName;
                found = true;
            }
            else if (found == true)
            {
                block.append(line);
            }
        }
        ssFile.close();
        if (found == true)
            styleCache.insert(compName, block);
    }

    return withBundledDefault(component, styleCache.value(component, QString()));
}

/*****************************************************************************
 * Digits
 *****************************************************************************/

unsigned int AppUtil::digits(unsigned int n)
{
    unsigned int res = 1;
    while (n /= 10)
        ++res;
    return res;
}

QColor AppUtil::sceneSwatchColor(Doc *doc, Scene *scene)
{
    if (doc == NULL || scene == NULL)
        return QColor();

    for (quint32 pid : scene->palettes())
    {
        QLCPalette *p = doc->palette(pid);
        if (p != NULL && p->type() == QLCPalette::Color)
            return p->rgbValue();
    }
    return QColor();
}

bool AppUtil::launchFixtureEditor(const QString &fixtureFilePath, QWidget *parentForError)
{
    const QString binName =
#if defined(WIN32) || defined(Q_OS_WIN)
        QStringLiteral("qlcconsole-fixtureeditor.exe");
#else
        QStringLiteral("qlcconsole-fixtureeditor");
#endif

    const QDir appDir(QCoreApplication::applicationDirPath());
    // Installed layout: a flat bindir holding every qlcconsole binary side by
    // side. Checked first since that's the real deployment target.
    QString editorPath = appDir.absoluteFilePath(binName);
    if (QFile::exists(editorPath) == false)
    {
        // Dev-build layout: build/main/qlcconsole -> build/fixtureeditor/...
        // (see CLAUDE.md's documented build/run layout).
        editorPath = appDir.absoluteFilePath(QStringLiteral("../fixtureeditor/") + binName);
    }

    if (QFile::exists(editorPath) == false)
    {
        QMessageBox::warning(parentForError, QObject::tr("Fixture Editor not found"),
            QObject::tr("Could not find the Fixture Editor (%1) next to this "
                        "application.").arg(binName));
        return false;
    }

    QStringList args;
    // Opened for one file: the editor goes away again when it is closed.
    if (fixtureFilePath.isEmpty() == false)
        args << QStringLiteral("--open") << fixtureFilePath << QStringLiteral("--close-when-done");

    return QProcess::startDetached(editorPath, args);
}

/*****************************************************************************
 * ComboBoxDelegate
 *****************************************************************************/

ComboBoxDelegate::ComboBoxDelegate(const QStringList &strings, QWidget *parent)
    : QStyledItemDelegate(parent)
    , m_strings(strings)
{
}

QWidget *ComboBoxDelegate::createEditor(QWidget *parent,
        const QStyleOptionViewItem &/*option*/,
        const QModelIndex &/*index*/) const
{
    QComboBox *comboBox = new QComboBox(parent);
    comboBox->addItems(m_strings);
    return comboBox;
}

void ComboBoxDelegate::setEditorData(QWidget *editor,
        const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::UserRole).toInt();
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    comboBox->setCurrentIndex(value);
}

void ComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
        const QModelIndex &index) const
{
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    int value = comboBox->currentIndex();
    model->setData(index, value, Qt::UserRole);
    model->setData(index, comboBox->currentText(), Qt::DisplayRole);
}

void ComboBoxDelegate::updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    editor->setGeometry(option.rect);
}

bool AppUtil::refuseTakenFixtureName(QWidget *parent, Doc *doc, quint32 fid, const QString &name)
{
    if (doc == nullptr || name.isEmpty() || !doc->fixtureNameTaken(name, fid))
        return false;
    QMessageBox::warning(parent, QObject::tr("Rename Fixture"),
        QObject::tr("Another fixture is already called “%1”. Fixture names are unique, so "
                    "every view, palette and cue can tell them apart -- pick another name "
                    "(for example “%2”).").arg(name, doc->uniqueFixtureName(name, fid)));
    return true;
}

void AppUtil::updateFixtureReadiness(Doc *doc)
{
    if (doc == NULL)
        return;

    QList<Fixture *> unpatched;
    struct SilentType { QString file; int fixtures = 0; };
    QMap<QString, SilentType> silentTypes;      // "Maker Model (mode)" -> its file, fixtures
    foreach (Fixture *fx, doc->fixtures())
    {
        if (fx == NULL)
            continue;
        // A clash is listed as a clash (below), not as merely unpatched.
        if (fx->isPatched() == false && fx->addressClash() == false)
            unpatched << fx;
        const QLCFixtureMode *mode = fx->fixtureMode();
        if (mode != NULL && mode->physical().declaresOutput() == false)
        {
            const QLCFixtureDef *def = fx->fixtureDef();
            const QString type = (def != NULL)
                ? QString("%1 %2 (%3) · %4").arg(def->manufacturer(), def->model(), mode->name(),
                                                  def->sourceDescription())
                : mode->name();
            SilentType &st = silentTypes[type];
            st.file = (def != NULL) ? def->definitionSourceFile() : QString();
            st.fixtures++;
        }
    }

    ShowStatus *status = ShowStatus::instance();
    if (unpatched.isEmpty())
    {
        status->clearStatus("fixtures.unpatched");
    }
    else
    {
        std::sort(unpatched.begin(), unpatched.end(), [](Fixture *a, Fixture *b) {
            return QString::localeAwareCompare(a->name(), b->name()) < 0;
        });
        QStringList items;
        QVariantList ids;
        foreach (Fixture *fx, unpatched)
        {
            items << fx->name();
            ids << fx->id();
        }
        status->setStatus("fixtures.unpatched", ShowStatus::Warning,
            QCoreApplication::translate("AppUtil", "%n fixture(s) not patched: they send no DMX",
                                        "", unpatched.count()),
            QCoreApplication::translate("AppUtil", "They show in the Lighting Studio only. Select "
                                        "them and Patch."),
            items, ids);
    }

    if (silentTypes.isEmpty())
    {
        status->clearStatus("fixtures.output");
    }
    else
    {
        QStringList items;
        QVariantList files;
        int fixtures = 0;
        for (auto it = silentTypes.constBegin(); it != silentTypes.constEnd(); ++it)
        {
            items << QCoreApplication::translate("AppUtil", "%1 — %n fixture(s)", "", it.value().fixtures)
                         .arg(it.key());
            files << it.value().file;
            fixtures += it.value().fixtures;
        }
        status->setStatus("fixtures.output", ShowStatus::Warning,
            QCoreApplication::translate("AppUtil", "%n fixture type(s) declare no light output",
                                        "", silentTypes.count()),
            QCoreApplication::translate("AppUtil", "The Lighting Studio and the Rig Overview draw these %n fixture(s) at a "
                                        "guess. Edit the definition and set its lumens (or "
                                        "power) on the Physical tab.", "", fixtures),
            items, files);
    }

    /* Fixtures that loaded unpatched because their address was taken. */
    {
        QStringList items;
        QVariantList ids;
        foreach (quint32 fid, doc->addressClashes())
        {
            Fixture *fx = doc->fixture(fid);
            if (fx == NULL)
                continue;
            items << QCoreApplication::translate("AppUtil", "%1 — wanted universe %2, address %3")
                         .arg(fx->name()).arg(fx->universe() + 1).arg(fx->address() + 1);
            ids << fid;
        }
        if (items.isEmpty())
            status->clearStatus("fixtures.clash");
        else
            status->setStatus("fixtures.clash", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n fixture(s) clash with another's address",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "They loaded unpatched, so they send no "
                                            "DMX. Re-patch them somewhere free."),
                items, ids);
    }

    /* Two fixtures with one name cannot be told apart where a fixture is
       picked by name. */
    {
        const QMap<QString, QList<quint32> > dups = doc->duplicateFixtureNames();
        QStringList items;
        QVariantList names;
        for (auto it = dups.constBegin(); it != dups.constEnd(); ++it)
        {
            items << QCoreApplication::translate("AppUtil", "%1 — %n fixture(s)", "", it.value().count())
                         .arg(it.key());
            names << it.key();
        }
        if (items.isEmpty())
            status->clearStatus("fixtures.duplicates");
        else
            status->setStatus("fixtures.duplicates", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n fixture name(s) used more than once",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Numbering keeps the name on the patched "
                                            "one and gives the others \"#2\", \"#3\"..."),
                items, names);
    }

    /* Definition files that could not be read: missing from the library. */
    {
        const QMap<QString, QString> failed = doc->fixtureDefCache()->failedDefinitions();
        const QHash<QString, QLCFixtureDef *> defsByName = definitionsByName(doc->fixtureDefCache());
        QStringList items, groups;
        QVariantList files;
        QHash<QByteArray, QString> firstWithContent;     // a "X 2.qxf" that is X.qxf again
        // Originals before their numbered copies ("X.qxf" before "X 2.qxf"),
        // so it is the copy that says so.
        QStringList paths = failed.keys();
        static const QRegularExpression numbered(QStringLiteral(" \\d+\\.[^.]+$"));
        std::stable_sort(paths.begin(), paths.end(), [](const QString &a, const QString &b) {
            const bool ca = QFileInfo(a).fileName().contains(numbered);
            const bool cb = QFileInfo(b).fileName().contains(numbered);
            return ca != cb ? !ca : QFileInfo(a).fileName() < QFileInfo(b).fileName();
        });
        QHash<QString, QString> whyOf;
        foreach (const QString &path, paths)
        {
            QString why = QCoreApplication::translate("AppUtil", "read error");
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
            {
                const QByteArray content = f.readAll();
                FixtureDefCheck::ParseProblem p;
                if (FixtureDefCheck::diagnose(content, &p) && !p.brief().isEmpty())
                    why += QStringLiteral(", ") + p.brief();
                const QString twin = firstWithContent.value(content);
                if (twin.isEmpty())
                    firstWithContent.insert(content, QFileInfo(path).fileName());
                else
                    why += QCoreApplication::translate("AppUtil", " (copy of %1)").arg(twin);
            }
            whyOf.insert(path, why);
        }
        for (auto it = failed.constBegin(); it != failed.constEnd(); ++it)
        {
            const bool local = QFileInfo(it.key()).absolutePath()
                == QLCFixtureDefCache::userDefinitionDirectory().absolutePath();
            // "read error", and why when the file says.
            const QString why = whyOf.value(it.key());
            QString item = QString("%1 — %2").arg(QFileInfo(it.key()).fileName(), why);
            /* A definition of the same fixture that DID load is used in its
               place: say which, so a broken local copy of an included
               fixture is not mistaken for the fixture going missing. */
            QFile src(it.key());
            if (src.open(QIODevice::ReadOnly))
            {
                const QString text = QString::fromUtf8(src.readAll());
                static const QRegularExpression man(QStringLiteral("<Manufacturer>([^<]*)</Manufacturer>"));
                static const QRegularExpression mod(QStringLiteral("<Model>([^<]*)</Model>"));
                const QString key = man.match(text).captured(1).trimmed() + QLatin1Char('\n')
                                  + mod.match(text).captured(1).trimmed();
                if (QLCFixtureDef *used = defsByName.value(key))
                    item += (used->source() == QLCFixtureDef::Local)
                        ? QCoreApplication::translate("AppUtil", " · %1 is used").arg(QFileInfo(used->definitionSourceFile()).fileName())
                        : QCoreApplication::translate("AppUtil", " · the included one is used");
            }
            items << item;
            groups << QLCFixtureDef::sourceToString(local ? QLCFixtureDef::Local : QLCFixtureDef::Included);
            files << it.key();
        }
        if (items.isEmpty())
            status->clearStatus("definitions.failed");
        else
            status->setStatus("definitions.failed", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n fixture definition(s) could not be read",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Missing from the fixture library "
                                            "until repaired."),
                items, files, groups);
    }

    /* Two LOCAL files for one fixture: only one is used (the newest), the
       other is clutter -- or a different version nobody knows is ignored.
       (A local one replacing an included one is by design: not listed.) */
    {
        QStringList items, groups;
        QVariantList files;
        foreach (const QLCFixtureDefCache::Duplicate &d, doc->fixtureDefCache()->duplicates())
        {
            if (!d.keptIsLocal || !d.ignoredIsLocal || !QFile::exists(d.ignored))
                continue;
            const QString kept = QFileInfo(d.kept).fileName();
            items << (d.identical
                ? QCoreApplication::translate("AppUtil", "%1 — same as %2").arg(QFileInfo(d.ignored).fileName(), kept)
                : QCoreApplication::translate("AppUtil", "%1 — older than %2, not used").arg(QFileInfo(d.ignored).fileName(), kept));
            files << d.ignored;
            QFile f(d.kept);
            QString name = kept;
            if (f.open(QIODevice::ReadOnly))
            {
                const QString text = QString::fromUtf8(f.readAll());
                static const QRegularExpression man(QStringLiteral("<Manufacturer>([^<]*)</Manufacturer>"));
                static const QRegularExpression mod(QStringLiteral("<Model>([^<]*)</Model>"));
                name = QString("%1 %2").arg(man.match(text).captured(1).trimmed(),
                                            mod.match(text).captured(1).trimmed());
            }
            groups << name;
        }
        if (items.isEmpty())
            status->clearStatus("definitions.duplicates");
        else
            status->setStatus("definitions.duplicates", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n local definition file(s) not used: another "
                                            "file defines the same fixture", "", items.count()),
                QCoreApplication::translate("AppUtil", "The most recently changed file is the one used."),
                items, files, groups);
    }

    /* WLED lanterns that do not answer, or answer on a weak signal -- asked
       by the WLED output itself, every few seconds. */
    {
        QStringList items;
        QVariantList ips;
        QLCIOPlugin *wled = doc->ioPluginCache() != NULL
                          ? doc->ioPluginCache()->plugin(QStringLiteral("WLED")) : NULL;
        if (wled != NULL)
        {
            foreach (const QString &problem, wled->targetProblems())
            {
                const QString ip = problem.section(QStringLiteral(" — "), 0, 0);
                QStringList names;
                foreach (Fixture *fx, doc->fixtures())
                    if (fx != NULL && fx->connection() == Fixture::Network && fx->ipAddress() == ip)
                        names << fx->name();
                items << (names.isEmpty() ? problem
                                          : QString("%1 (%2)").arg(names.join(", "), problem));
                ips << ip;
            }
        }
        if (items.isEmpty())
            status->clearStatus("wled.targets");
        else
            status->setStatus("wled.targets", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n WLED device(s) not answering or on weak Wi-Fi",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Checked every few seconds. Weaker than -75 dBm "
                                            "drops on stage Wi-Fi."),
                items, ips);
    }

    /* Network outputs in trouble, as their plugins judge it -- sends that
       keep failing, Art-Net nodes that stopped answering, node ports that
       report a fault (QLCIOPlugin::outputProblems()). Asked per patch so
       each line can say which universes it hits and the fix can open the
       first of them in Connections; one line per problem, not per universe,
       because fifty universes behind one dead node are one problem. */
    {
        InputOutputMap *io = doc->inputOutputMap();
        QStringList order;
        QMap<QString, QList<quint32> > hit;
        foreach (Universe *u, io->universes())
        {
            if (u == NULL)
                continue;
            for (int i = 0; i < u->outputPatchesCount(); i++)
            {
                OutputPatch *op = u->outputPatch(i);
                if (op == NULL || op->plugin() == NULL)
                    continue;
                foreach (const QString &problem, op->plugin()->outputProblems(op->output(), u->id()))
                {
                    if (hit.contains(problem) == false)
                        order << problem;
                    if (hit[problem].contains(u->id()) == false)
                        hit[problem] << u->id();
                }
            }
        }
        QStringList items;
        QVariantList universes;
        foreach (const QString &problem, order)
        {
            const QList<quint32> &unis = hit[problem];
            QStringList nums;
            foreach (quint32 id, unis)
                nums << QString::number(id + 1);
            items << (unis.count() == 1
                ? QCoreApplication::translate("AppUtil", "%1 (universe %2)").arg(problem, nums.first())
                : QCoreApplication::translate("AppUtil", "%1 (universes %2)").arg(problem, nums.join(", ")));
            universes << unis.first();
        }
        if (items.isEmpty())
            status->clearStatus("outputs.problems");
        else
            status->setStatus("outputs.problems", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n network output problem(s)",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Outputs that can't send, Art-Net nodes that "
                                            "stopped answering, and node ports reporting a "
                                            "fault. Each shows once it has lasted, and clears "
                                            "once it has stayed fixed."),
                items, universes);
    }

    /* A universe fixtures are patched on, with nowhere to send it. */
    {
        InputOutputMap *io = doc->inputOutputMap();
        QMap<quint32, int> used;
        foreach (Fixture *fx, doc->fixtures())
            if (fx != NULL && fx->isPatched() && fx->universe() < io->universesCount())
                used[fx->universe()]++;
        QStringList items;
        QVariantList universes;
        for (auto it = used.constBegin(); it != used.constEnd(); ++it)
        {
            if (io->outputPatchesCount(it.key()) > 0)
                continue;
            const QString name = io->getUniverseNameByIndex(it.key());
            items << QCoreApplication::translate("AppUtil", "Universe %1%2 — %n fixture(s)", "", it.value())
                         .arg(it.key() + 1)
                         .arg(name.isEmpty() ? QString() : QString(" (%1)").arg(name));
            universes << it.key();
        }
        if (items.isEmpty())
            status->clearStatus("universes.nooutput");
        else
            status->setStatus("universes.nooutput", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n universe(s) with fixtures but no output",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "What they send goes nowhere. Give each "
                                            "an output in Connections."),
                items, universes);
    }

    /* Effect looks that cannot run: no effect chosen, a script that is not
       on this computer (checked here, before anything plays), or one that
       would not start when its scene ran (the runner's failures) -- an effect
       look like that otherwise just does nothing. */
    {
        QStringList items;
        QVariantList ids;
        QHash<quint32, QString> why = doc->effectFailures();
        QSet<quint32> inScenes;
        foreach (Function *f, doc->functionsByType(Function::SceneType))
        {
            if (Scene *sc = qobject_cast<Scene *>(f))
            {
                foreach (quint32 pid, sc->palettes())
                    inScenes.insert(pid);
            }
        }
        foreach (QLCPalette *p, doc->palettes())
        {
            if (p == nullptr || p->type() != QLCPalette::Effect || !inScenes.contains(p->id()))
                continue;
            if (p->scriptPath().isEmpty())
                why.insert(p->id(), QCoreApplication::translate("AppUtil", "no effect is chosen"));
            else if (!QFile::exists(p->scriptPath()) && !why.contains(p->id()))
                why.insert(p->id(), QCoreApplication::translate("AppUtil", "its script is not on this computer (%1)")
                                        .arg(QFileInfo(p->scriptPath()).fileName()));
        }
        for (auto it = why.constBegin(); it != why.constEnd(); ++it)
        {
            QLCPalette *p = doc->palette(it.key());
            items << QStringLiteral("%1 — %2").arg(p != nullptr ? p->name() : QString::number(it.key()),
                                                  it.value());
            ids << it.key();
        }
        if (items.isEmpty())
            status->clearStatus("effects.failed");
        else
            status->setStatus("effects.failed", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n effect look(s) cannot run", "", items.count()),
                QCoreApplication::translate("AppUtil", "They do nothing until fixed: choose an effect "
                                            "in the look, or put its script back."),
                items, ids);
    }

    /* Fixtures standing in as dimmers for a definition that was not here
       when the show loaded: they keep their real make and model, but cannot
       be programmed as what they are until it is back. */
    {
        QStringList items, groups;
        QVariantList ids;
        foreach (Fixture *fx, doc->fixtures())
        {
            if (fx == nullptr || !fx->definitionUnavailable())
                continue;
            items << fx->name();
            groups << fx->unavailableManufacturer() + QLatin1Char(' ') + fx->unavailableModel();
            ids << fx->id();
        }
        if (items.isEmpty())
            status->clearStatus("fixtures.nodefinition");
        else
            status->setStatus("fixtures.nodefinition", ShowStatus::Error,
                QCoreApplication::translate("AppUtil", "%n fixture(s) have no definition on this computer",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Standing in as dimmers. Saved as what they "
                                            "really are, but not programmable as that until "
                                            "their definitions are back."),
                items, ids, groups);
    }

    /* Files in cloud storage that are not on this machine -- an online-only
       placeholder (Synology Drive, iCloud, OneDrive...). Reading one waits on
       the cloud, and with it out of reach that wait was the whole startup, so
       they were skipped ("that's burned me before"): say so, and which. */
    {
        QStringList items, groups;
        QVariantList files;
        QString provider;
        foreach (const QString &path, QLCFile::skippedOnlineOnly())
        {
            items << QFileInfo(path).fileName();
            groups << QFileInfo(path).dir().dirName();      // Fixtures, RGBScripts, ...
            files << path;
            // Whose cloud: ~/Library/CloudStorage/<Provider>-<Account>/...
            QString real = QFileInfo(path).canonicalFilePath();   // through the links
            if (real.isEmpty())
                real = path;
            const int at = real.indexOf(QLatin1String("/CloudStorage/"));
            if (provider.isEmpty() && at >= 0)
                provider = real.mid(at + 14).section(QLatin1Char('/'), 0, 0).section(QLatin1Char('-'), 0, 0);
        }
        if (items.isEmpty())
            status->clearStatus("storage.onlineonly");
        else
            status->setStatus("storage.onlineonly", ShowStatus::Warning,
                QCoreApplication::translate("AppUtil", "%n file(s) in cloud storage are not on this computer",
                                            "", items.count()),
                QCoreApplication::translate("AppUtil", "Not loaded -- reading them waits on %1. Keep "
                                            "the folder on this computer (pin it), or download "
                                            "them now.")
                    .arg(provider.isEmpty() ? QCoreApplication::translate("AppUtil", "the cloud") : provider),
                items, files, groups);
    }
}

QHash<QString, QLCFixtureDef *> AppUtil::definitionsByName(QLCFixtureDefCache *cache)
{
    QHash<QString, QLCFixtureDef *> out;
    if (cache == NULL)
        return out;
    foreach (QLCFixtureDef *def, cache->definitions())
        if (def != NULL)
            out.insert(def->manufacturer() + QLatin1Char('\n') + def->model(), def);
    return out;
}

void AppUtil::showDefinitionSource(QTreeWidgetItem *item, int column, const QLCFixtureDef *def)
{
    if (item == NULL || def == NULL)
        return;
    item->setText(column, def->sourceDescription());
    QFont f = item->font(column);
    switch (def->source())
    {
    case QLCFixtureDef::Local:
        item->setForeground(column, QColor(70, 140, 230));
        item->setToolTip(column, QCoreApplication::translate("AppUtil",
            "Your own definition: %1").arg(def->definitionSourceFile()));
        break;
    case QLCFixtureDef::Downloadable:
        f.setItalic(true);
        item->setForeground(column, QColor(150, 150, 150));
        item->setToolTip(column, QCoreApplication::translate("AppUtil",
            "From the online library: fetched when it is used."));
        break;
    case QLCFixtureDef::Included:
    default:
        item->setForeground(column, QColor(140, 140, 140));
        item->setToolTip(column, QCoreApplication::translate("AppUtil",
            "Ships with the app. Edits are saved as a local copy."));
        break;
    }
    item->setFont(column, f);
}

QString AppUtil::nthAddress(const QString &ip, int n)
{
    const QStringList parts = ip.trimmed().split(QLatin1Char('.'));
    if (parts.size() != 4)
        return QString();
    int o[4];
    for (int i = 0; i < 4; ++i)
    {
        bool ok = false;
        o[i] = parts.at(i).toInt(&ok);
        if (!ok || o[i] < 0 || o[i] > 255)
            return QString();
    }
    const int low = o[2] * 256 + o[3] + n;
    if (n < 0 || low > 255 * 256 + 254)
        return QString();
    return QString("%1.%2.%3.%4").arg(o[0]).arg(o[1]).arg(low / 256).arg(low % 256);
}

void AppUtil::updateLoadProblems(const QString &errorLog)
{
    ShowStatus *status = ShowStatus::instance();
    QStringList items;
    static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
    foreach (QString line, errorLog.split(QStringLiteral("<br>"), Qt::SkipEmptyParts))
    {
        line.remove(tags);
        line = line.trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1String("No fixture definition found")))
            items << line;
    }
    if (items.isEmpty())
        status->clearStatus("show.load");
    else
        status->setStatus("show.load", ShowStatus::Warning,
            QCoreApplication::translate("AppUtil", "%n problem(s) loading the show", "", items.count()),
            QCoreApplication::translate("AppUtil", "What the show file asked for that could not be done as it was."),
            items);
}

QString AppUtil::patchActionText(int count)
{
    return count > 1 ? QCoreApplication::translate("AppUtil", "Patch %1 Fixtures…").arg(count)
                     : QCoreApplication::translate("AppUtil", "Patch…");
}

bool AppUtil::patchAllowed(Doc *doc, const QList<quint32> &fixtureIds)
{
    if (doc == nullptr || fixtureIds.isEmpty())
        return false;
    if (doc->mode() == Doc::Design)
        return true;
    foreach (quint32 fid, fixtureIds)
    {
        Fixture *fx = doc->fixture(fid);
        if (fx != nullptr && fx->isPatched())
            return false;
    }
    return true;
}
