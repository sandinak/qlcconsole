#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

class AutoSaveManager;

/**
 * PreferencesDialog provides a centralized dialog for configuring
 * QLC+ application preferences, including autosave settings.
 */
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(AutoSaveManager* autoSaveManager, QWidget* parent = nullptr);
    ~PreferencesDialog();

public slots:
    void accept() override;
    void reject() override;

private slots:
    void slotAutosaveEnabledChanged(bool enabled);
    void slotAutosaveIntervalChanged(int minutes);
    void slotAutosaveUseBackupChanged(bool useBackup);
    void slotAutosaveMaxBackupsChanged(int maxBackups);
    void slotRestoreDefaults();

private:
    void setupUI();
    void setupAutosaveTab();
    /** Locate appearance: how a fixture identifies itself on the rig. */
    void setupLocateTab();
    void loadSettings();
    void saveSettings();
    void updateAutosaveControls();

private:
    AutoSaveManager* m_autoSaveManager;
    
    // UI Components
    QTabWidget* m_tabWidget;
    QWidget* m_autosaveTab;
    QWidget* m_locateTab;
    
    // Autosave controls
    QGroupBox* m_autosaveGroup;
    QCheckBox* m_autosaveEnabledCheck;
    QSpinBox* m_autosaveIntervalSpin;
    QCheckBox* m_autosaveUseBackupCheck;
    QSpinBox* m_autosaveMaxBackupsSpin;

    // Locate controls
    class QPushButton* m_locateColorBtn;
    class QColor*      m_locateColor;
    QSpinBox*          m_locateIntensitySpin;
    QSpinBox*          m_locateFlashesSpin;
    QPushButton* m_restoreDefaultsButton;
    
    QDialogButtonBox* m_buttonBox;
    
    // Settings backup for cancel operation
    bool m_originalEnabled;
    int m_originalInterval;
    bool m_originalUseBackup;
    int m_originalMaxBackups;
};

#endif // PREFERENCESDIALOG_H
