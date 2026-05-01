#ifndef AUTOSAVEMANAGER_H
#define AUTOSAVEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QSettings>
#include <QDateTime>

class Doc;

/**
 * AutoSaveManager handles automatic saving of QLC+ workspaces at configurable intervals.
 * It monitors document modification state and performs saves when the document has been
 * modified and the autosave interval has elapsed.
 */
class AutoSaveManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoSaveManager(Doc* doc, QObject* parent = nullptr);
    ~AutoSaveManager();

    /** Configuration keys for QSettings */
    static const QString SETTINGS_AUTOSAVE_ENABLED;
    static const QString SETTINGS_AUTOSAVE_INTERVAL;
    static const QString SETTINGS_AUTOSAVE_USE_BACKUP;
    static const QString SETTINGS_AUTOSAVE_MAX_BACKUPS;

    /** Default configuration values */
    static const bool DEFAULT_AUTOSAVE_ENABLED;
    static const int DEFAULT_AUTOSAVE_INTERVAL;
    static const bool DEFAULT_AUTOSAVE_USE_BACKUP;
    static const int DEFAULT_AUTOSAVE_MAX_BACKUPS;

    /** Get/Set autosave enabled state */
    bool isEnabled() const;
    void setEnabled(bool enabled);

    /** Get/Set autosave interval in minutes */
    int intervalMinutes() const;
    void setIntervalMinutes(int minutes);

    /** Get/Set whether to use backup files */
    bool useBackupFiles() const;
    void setUseBackupFiles(bool useBackup);

    /** Get/Set maximum number of backup files to keep */
    int maxBackupFiles() const;
    void setMaxBackupFiles(int maxFiles);

    /** Load settings from QSettings */
    void loadSettings();

    /** Save settings to QSettings */
    void saveSettings();

    /** Start the autosave timer */
    void start();

    /** Stop the autosave timer */
    void stop();

    /** Force an immediate autosave if document is modified */
    void forceAutosave();

    /** Get the last autosave timestamp */
    QDateTime lastAutosaveTime() const;

signals:
    /** Emitted when an autosave operation should be performed */
    void autosaveRequested(const QString& filePath);

    /** Emitted when an autosave operation completes */
    void autosaveCompleted(bool success, const QString& filePath);

    /** Emitted when autosave settings change */
    void settingsChanged();

public slots:
    /** Called by the UI to report autosave completion */
    void onAutosaveResult(bool success, const QString& filePath);

private slots:
    /** Called when the autosave timer expires */
    void onAutosaveTimer();

    /** Called when document modification state changes */
    void onDocumentModified(bool modified);

private:
    /** Perform the actual autosave operation */
    bool performAutosave();

    /** Generate backup file path */
    QString generateBackupFilePath(const QString& originalPath);

    /** Clean up old backup files */
    void cleanupOldBackups(const QString& basePath);

    /** Update timer interval based on current settings */
    void updateTimerInterval();

private:
    Doc* m_doc;
    QTimer* m_autosaveTimer;
    QSettings* m_settings;
    
    bool m_enabled;
    int m_intervalMinutes;
    bool m_useBackupFiles;
    int m_maxBackupFiles;
    
    bool m_documentModified;
    QDateTime m_lastAutosaveTime;
};

#endif // AUTOSAVEMANAGER_H
