#include "autosavemanager.h"
#include "doc.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

// Settings keys
const QString AutoSaveManager::SETTINGS_AUTOSAVE_ENABLED = "autosave/enabled";
const QString AutoSaveManager::SETTINGS_AUTOSAVE_INTERVAL = "autosave/interval";
const QString AutoSaveManager::SETTINGS_AUTOSAVE_USE_BACKUP = "autosave/useBackup";
const QString AutoSaveManager::SETTINGS_AUTOSAVE_MAX_BACKUPS = "autosave/maxBackups";

// Default values
const bool AutoSaveManager::DEFAULT_AUTOSAVE_ENABLED = false;
const int AutoSaveManager::DEFAULT_AUTOSAVE_INTERVAL = 5; // minutes
const bool AutoSaveManager::DEFAULT_AUTOSAVE_USE_BACKUP = true;
const int AutoSaveManager::DEFAULT_AUTOSAVE_MAX_BACKUPS = 3;

AutoSaveManager::AutoSaveManager(Doc* doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
    , m_autosaveTimer(new QTimer(this))
    , m_settings(new QSettings(this))
    , m_enabled(DEFAULT_AUTOSAVE_ENABLED)
    , m_intervalMinutes(DEFAULT_AUTOSAVE_INTERVAL)
    , m_useBackupFiles(DEFAULT_AUTOSAVE_USE_BACKUP)
    , m_maxBackupFiles(DEFAULT_AUTOSAVE_MAX_BACKUPS)
    , m_documentModified(false)
{
    Q_ASSERT(doc != nullptr);

    // Connect to document modification signals
    connect(m_doc, &Doc::modified, this, &AutoSaveManager::onDocumentModified);

    // Setup autosave timer
    m_autosaveTimer->setSingleShot(false);
    connect(m_autosaveTimer, &QTimer::timeout, this, &AutoSaveManager::onAutosaveTimer);

    // Load settings
    loadSettings();
    updateTimerInterval();
}

AutoSaveManager::~AutoSaveManager()
{
    stop();
}

bool AutoSaveManager::isEnabled() const
{
    return m_enabled;
}

void AutoSaveManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled)
    {
        m_enabled = enabled;
        
        if (m_enabled)
            start();
        else
            stop();
            
        emit settingsChanged();
    }
}

int AutoSaveManager::intervalMinutes() const
{
    return m_intervalMinutes;
}

void AutoSaveManager::setIntervalMinutes(int minutes)
{
    if (minutes < 1)
        minutes = 1; // Minimum 1 minute
        
    if (m_intervalMinutes != minutes)
    {
        m_intervalMinutes = minutes;
        updateTimerInterval();
        emit settingsChanged();
    }
}

bool AutoSaveManager::useBackupFiles() const
{
    return m_useBackupFiles;
}

void AutoSaveManager::setUseBackupFiles(bool useBackup)
{
    if (m_useBackupFiles != useBackup)
    {
        m_useBackupFiles = useBackup;
        emit settingsChanged();
    }
}

int AutoSaveManager::maxBackupFiles() const
{
    return m_maxBackupFiles;
}

void AutoSaveManager::setMaxBackupFiles(int maxFiles)
{
    if (maxFiles < 1)
        maxFiles = 1; // Minimum 1 backup
        
    if (m_maxBackupFiles != maxFiles)
    {
        m_maxBackupFiles = maxFiles;
        emit settingsChanged();
    }
}

void AutoSaveManager::loadSettings()
{
    m_enabled = m_settings->value(SETTINGS_AUTOSAVE_ENABLED, DEFAULT_AUTOSAVE_ENABLED).toBool();
    m_intervalMinutes = m_settings->value(SETTINGS_AUTOSAVE_INTERVAL, DEFAULT_AUTOSAVE_INTERVAL).toInt();
    m_useBackupFiles = m_settings->value(SETTINGS_AUTOSAVE_USE_BACKUP, DEFAULT_AUTOSAVE_USE_BACKUP).toBool();
    m_maxBackupFiles = m_settings->value(SETTINGS_AUTOSAVE_MAX_BACKUPS, DEFAULT_AUTOSAVE_MAX_BACKUPS).toInt();
    
    // Validate loaded values
    if (m_intervalMinutes < 1)
        m_intervalMinutes = DEFAULT_AUTOSAVE_INTERVAL;
    if (m_maxBackupFiles < 1)
        m_maxBackupFiles = DEFAULT_AUTOSAVE_MAX_BACKUPS;
}

void AutoSaveManager::saveSettings()
{
    m_settings->setValue(SETTINGS_AUTOSAVE_ENABLED, m_enabled);
    m_settings->setValue(SETTINGS_AUTOSAVE_INTERVAL, m_intervalMinutes);
    m_settings->setValue(SETTINGS_AUTOSAVE_USE_BACKUP, m_useBackupFiles);
    m_settings->setValue(SETTINGS_AUTOSAVE_MAX_BACKUPS, m_maxBackupFiles);
    m_settings->sync();
}

void AutoSaveManager::start()
{
    if (m_enabled && !m_autosaveTimer->isActive())
    {
        m_autosaveTimer->start();
        qDebug() << "AutoSave: Started with interval" << m_intervalMinutes << "minutes";
    }
}

void AutoSaveManager::stop()
{
    if (m_autosaveTimer->isActive())
    {
        m_autosaveTimer->stop();
        qDebug() << "AutoSave: Stopped";
    }
}

void AutoSaveManager::forceAutosave()
{
    if (m_documentModified)
    {
        performAutosave();
    }
}

QDateTime AutoSaveManager::lastAutosaveTime() const
{
    return m_lastAutosaveTime;
}

void AutoSaveManager::onAutosaveTimer()
{
    if (m_enabled && m_documentModified)
    {
        performAutosave();
    }
}

void AutoSaveManager::onDocumentModified(bool modified)
{
    m_documentModified = modified;
    
    // If document is no longer modified (e.g., user saved manually), 
    // we can reset our tracking
    if (!modified)
    {
        qDebug() << "AutoSave: Document saved manually, resetting autosave tracking";
    }
}

void AutoSaveManager::updateTimerInterval()
{
    int intervalMs = m_intervalMinutes * 60 * 1000; // Convert minutes to milliseconds
    m_autosaveTimer->setInterval(intervalMs);

    if (m_enabled && !m_autosaveTimer->isActive())
    {
        start();
    }
}

bool AutoSaveManager::performAutosave()
{
    QString currentFile = m_doc->currentWorkspaceFile();

    // If no current file is set, we can't autosave
    if (currentFile.isEmpty())
    {
        qDebug() << "AutoSave: No current workspace file set, skipping autosave";
        return false;
    }

    QString saveFilePath;

    if (m_useBackupFiles)
    {
        saveFilePath = generateBackupFilePath(currentFile);
    }
    else
    {
        saveFilePath = currentFile;
    }

    qDebug() << "AutoSave: Requesting save to" << saveFilePath;

    // Emit signal for the UI to handle the actual save operation
    emit autosaveRequested(saveFilePath);

    return true; // Return true since we've initiated the save request
}

void AutoSaveManager::onAutosaveResult(bool success, const QString& filePath)
{
    if (success)
    {
        m_lastAutosaveTime = QDateTime::currentDateTime();

        if (m_useBackupFiles)
        {
            cleanupOldBackups(m_doc->currentWorkspaceFile());
        }

        qDebug() << "AutoSave: Successfully saved to" << filePath;
    }
    else
    {
        qDebug() << "AutoSave: Failed to save to" << filePath;
    }

    emit autosaveCompleted(success, filePath);
}

QString AutoSaveManager::generateBackupFilePath(const QString& originalPath)
{
    QFileInfo fileInfo(originalPath);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QString dir = fileInfo.absolutePath();

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString backupFileName = QString("%1_autosave_%2.%3").arg(baseName, timestamp, suffix);

    return QDir(dir).absoluteFilePath(backupFileName);
}

void AutoSaveManager::cleanupOldBackups(const QString& basePath)
{
    QFileInfo fileInfo(basePath);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QDir dir(fileInfo.absolutePath());

    // Find all autosave backup files for this workspace
    QString pattern = QString("%1_autosave_*.%2").arg(baseName, suffix);
    QStringList backupFiles = dir.entryList(QStringList() << pattern, QDir::Files, QDir::Time);

    // Remove oldest files if we exceed the maximum
    while (backupFiles.size() > m_maxBackupFiles)
    {
        QString oldestFile = backupFiles.takeLast();
        QString fullPath = dir.absoluteFilePath(oldestFile);

        if (QFile::remove(fullPath))
        {
            qDebug() << "AutoSave: Removed old backup file" << fullPath;
        }
        else
        {
            qDebug() << "AutoSave: Failed to remove old backup file" << fullPath;
        }
    }
}
