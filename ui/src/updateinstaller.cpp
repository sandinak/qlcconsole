/*
  Q Light Controller Plus
  updateinstaller.cpp

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

#include <QCoreApplication>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QDir>

#include "updateinstaller.h"
#include "updatechecker.h"
#include "mastertimer.h"
#include "doc.h"

UpdateInstaller::UpdateInstaller(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
{
}

UpdateInstaller::~UpdateInstaller()
{
    if (m_process != nullptr)
    {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    if (m_helper.isEmpty() == false)
        QFile::remove(m_helper);
}

bool UpdateInstaller::safeToInstall(const Doc *doc, QString *why)
{
    QString reason;
    if (doc != nullptr)
    {
        const int running = doc->masterTimer()->runningFunctions();
        if (doc->isKiosk())
            reason = tr("qlcconsole is running as a kiosk.");
        else if (doc->mode() == Doc::Operate)
            reason = tr("qlcconsole is in Perform (Operate) mode.");
        else if (running > 0)
            reason = tr("%n function(s) running.", "", running);
    }
    if (why != nullptr)
        *why = reason;
    return reason.isEmpty();
}

QString UpdateInstaller::runningBundle()
{
    // <bundle>.app/Contents/MacOS/qlcconsole
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.dirName() != QLatin1String("MacOS"))
        return QString();
    dir.cdUp();
    if (dir.dirName() != QLatin1String("Contents"))
        return QString();
    dir.cdUp();
    return dir.absolutePath().endsWith(QLatin1String(".app")) ? dir.absolutePath() : QString();
}

QString UpdateInstaller::installFolder()
{
    return QCoreApplication::applicationDirPath();
}

bool UpdateInstaller::canReplaceBundle(const QString &bundle, QString *why)
{
    QString reason;
    const QFileInfo fi(bundle);
    const QFileInfo dir(fi.absolutePath());
    if (bundle.isEmpty() || fi.isDir() == false)
        reason = tr("qlcconsole is not running from an app bundle.");
    else if (bundle.contains(QLatin1String("/AppTranslocation/")))
        reason = tr("macOS is running this copy from a protected location; move qlcconsole "
                    "to Applications first.");
    else if (bundle.startsWith(QLatin1String("/Volumes/")) && dir.isWritable() == false)
        reason = tr("qlcconsole is running from its disk image.");
    else if (dir.isWritable() == false || fi.isWritable() == false)
        reason = tr("%1 cannot be written without an administrator.").arg(dir.absoluteFilePath());
    if (why != nullptr)
        *why = reason;
    return reason.isEmpty();
}

UpdateInstaller::Method UpdateInstaller::method(QString *why)
{
    const QString os = UpdateChecker::currentOs();
    if (os == QLatin1String("macos"))
    {
        return canReplaceBundle(runningBundle(), why) ? InPlace : OpenDownload;
    }
    if (os == QLatin1String("windows"))
    {
        const QFileInfo dir(installFolder());
        if (dir.isWritable())
            return InPlace;
        if (why != nullptr)
            *why = tr("%1 cannot be written without an administrator.").arg(dir.absoluteFilePath());
        return OpenDownload;
    }
    if (why != nullptr)
        *why = tr("On Linux and Raspberry Pi, updates are not installed automatically.");
    return NoInstall;
}

QString UpdateInstaller::stagedPathFor(const QString &bundle)
{
    return QFileInfo(bundle).absolutePath() + QStringLiteral("/.qlcconsole-update.app");
}

QString UpdateInstaller::backupPathFor(const QString &bundle)
{
    return QFileInfo(bundle).absolutePath() + QStringLiteral("/.qlcconsole-previous.app");
}

QString UpdateInstaller::writeMacHelper()
{
    QFile res(QStringLiteral(":/update/update-helper.sh"));
    if (res.open(QIODevice::ReadOnly) == false)
        return QString();
    QTemporaryFile out(QDir::temp().filePath(QStringLiteral("qlcconsole-update-helper-XXXXXX.sh")));
    out.setAutoRemove(false);
    if (out.open() == false)
        return QString();
    const QByteArray script = res.readAll();
    if (out.write(script) != script.size())
    {
        out.remove();
        return QString();
    }
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    out.close();
    return out.fileName();
}

QString UpdateInstaller::windowsScript(const QString &installer, qint64 pid,
                                       const QString &installDir, const QString &exe,
                                       bool relaunch, const QString &show)
{
    // Windows paths whatever this was built on (the test runs everywhere).
    auto win = [](QString p) { return p.replace(QLatin1Char('/'), QLatin1Char('\\')); };
    const QString inst = win(installer);
    const QString dir = win(installDir);
    const QString app = win(exe);
    QStringList lines;
    lines << QStringLiteral("@echo off")
          << QStringLiteral("setlocal")
          << QStringLiteral("set /a waited=0")
          // Wait for qlcconsole (this pid) to end -- five minutes at most.
          << QStringLiteral(":wait")
          << QStringLiteral("tasklist /FI \"PID eq %1\" 2>NUL | find \"%1\" >NUL").arg(pid)
          << QStringLiteral("if errorlevel 1 goto install")
          << QStringLiteral("set /a waited+=1")
          << QStringLiteral("if %waited% GEQ 300 exit /b 1")
          << QStringLiteral("ping -n 2 127.0.0.1 >NUL")
          << QStringLiteral("goto wait")
          << QStringLiteral(":install")
          // NSIS: /S silent; /D= the folder, last and unquoted by its rules.
          << QStringLiteral("\"%1\" /S /D=%2").arg(inst, dir)
          << QStringLiteral("if errorlevel 1 exit /b 1");
    if (relaunch)
    {
        if (show.isEmpty())
            lines << QStringLiteral("start \"\" \"%1\"").arg(app);
        else
            lines << QStringLiteral("start \"\" \"%1\" -o \"%2\"")
                         .arg(app, win(show));
    }
    lines << QStringLiteral("exit /b 0");
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

void UpdateInstaller::prepare(const QString &dmg, const QString &bundle, bool allowAdhoc)
{
    m_staged.clear();
    m_team.clear();
    if (m_process != nullptr)
        return;

    const QString running = bundle.isEmpty() ? runningBundle() : bundle;
    QString why;
    if (canReplaceBundle(running, &why) == false)
    {
        emit prepared(false, why);
        return;
    }
    if (m_helper.isEmpty() || QFile::exists(m_helper) == false)
        m_helper = writeMacHelper();
    if (m_helper.isEmpty())
    {
        emit prepared(false, tr("Could not write the update helper."));
        return;
    }

    QStringList args;
    args << m_helper << QStringLiteral("prepare")
         << QStringLiteral("--dmg") << dmg
         << QStringLiteral("--dest-dir") << QFileInfo(running).absolutePath()
         << QStringLiteral("--running-app") << running
         << QStringLiteral("--staged-name") << QFileInfo(stagedPathFor(running)).fileName();
    if (allowAdhoc)
        args << QStringLiteral("--insecure-test-allow-adhoc");

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        QString error;
        for (const QString &line : out.split(QLatin1Char('\n')))
        {
            if (line.startsWith(QLatin1String("STAGED: ")))
                m_staged = line.mid(8).trimmed();
            else if (line.startsWith(QLatin1String("TEAM: ")))
                m_team = line.mid(6).trimmed();
            else if (line.startsWith(QLatin1String("ERROR: ")))
                error = line.mid(7).trimmed();
        }
        if (status != QProcess::NormalExit || code != 0 || m_staged.isEmpty())
        {
            m_staged.clear();
            qWarning() << "[updates] prepare failed:" << out;
            emit prepared(false, error.isEmpty() ? tr("The new app could not be checked.") : error);
            return;
        }
        emit prepared(true, QString());
    });
    m_process->start(QStringLiteral("/bin/bash"), args);
}

bool UpdateInstaller::launchOnQuit(const QString &file, bool relaunch, const QString &show)
{
    const QString os = UpdateChecker::currentOs();
    if (os == QLatin1String("macos"))
    {
        const QString bundle = runningBundle();
        if (m_staged.isEmpty() || QFileInfo(m_staged).isDir() == false || bundle.isEmpty())
            return false;
        if (m_helper.isEmpty() || QFile::exists(m_helper) == false)
            m_helper = writeMacHelper();
        if (m_helper.isEmpty())
            return false;
        QStringList args;
        args << m_helper << QStringLiteral("install")
             << QStringLiteral("--pid") << QString::number(QCoreApplication::applicationPid())
             << QStringLiteral("--current") << bundle
             << QStringLiteral("--new") << m_staged
             << QStringLiteral("--backup") << backupPathFor(bundle)
             << QStringLiteral("--log") << QDir::temp().filePath(QStringLiteral("qlcconsole-update.log"));
        if (m_team.isEmpty() == false)
            args << QStringLiteral("--team-id") << m_team;
        if (relaunch)
            args << QStringLiteral("--relaunch");
        if (relaunch && show.isEmpty() == false)
            args << QStringLiteral("--show") << show;
        const bool ok = QProcess::startDetached(QStringLiteral("/bin/bash"), args);
        if (ok)
            m_helper.clear();   // the helper runs it after we are gone
        return ok;
    }

    if (os == QLatin1String("windows"))
    {
        if (QFile::exists(file) == false)
            return false;
        const QString script = windowsScript(file, QCoreApplication::applicationPid(),
                                             installFolder(), QCoreApplication::applicationFilePath(),
                                             relaunch, show);
        const QString path = QDir::temp().filePath(QStringLiteral("qlcconsole-update.cmd"));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate) == false
            || f.write(script.toLocal8Bit()) < 0)
            return false;
        f.close();
        QProcess p;
        p.setProgram(QStringLiteral("cmd.exe"));
        p.setArguments({ QStringLiteral("/c"), QDir::toNativeSeparators(path) });
#if defined(Q_OS_WIN)
        p.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
            a->flags |= 0x08000000;     // CREATE_NO_WINDOW
        });
#endif
        return p.startDetached();
    }
    return false;
}
