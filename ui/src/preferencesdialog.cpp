#include "preferencesdialog.h"
#include "autosavemanager.h"
#include <QSettings>
#include <QIcon>
#include <QColorDialog>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QTabWidget>

#include "monitor/monitor.h"

#define SETTINGS_GEOMETRY "preferencesdialog/geometry"

PreferencesDialog::PreferencesDialog(AutoSaveManager* autoSaveManager, QWidget* parent)
    : QDialog(parent)
    , m_autoSaveManager(autoSaveManager)
    , m_tabWidget(nullptr)
    , m_autosaveTab(nullptr)
    , m_autosaveGroup(nullptr)
    , m_autosaveEnabledCheck(nullptr)
    , m_autosaveIntervalSpin(nullptr)
    , m_autosaveUseBackupCheck(nullptr)
    , m_autosaveMaxBackupsSpin(nullptr)
    , m_restoreDefaultsButton(nullptr)
    , m_buttonBox(nullptr)
{
    Q_ASSERT(autoSaveManager != nullptr);
    
    setWindowTitle(tr("QLC+ Preferences"));
    setWindowIcon(QIcon(":/configure.png"));
    setModal(true);
    
    // Backup current settings for cancel operation
    m_originalEnabled = m_autoSaveManager->isEnabled();
    m_originalInterval = m_autoSaveManager->intervalMinutes();
    m_originalUseBackup = m_autoSaveManager->useBackupFiles();
    m_originalMaxBackups = m_autoSaveManager->maxBackupFiles();
    
    setupUI();
    loadSettings();
    
    // Restore geometry
    QSettings settings;
    QVariant var = settings.value(SETTINGS_GEOMETRY);
    if (var.isValid())
        restoreGeometry(var.toByteArray());
    else
        resize(500, 400);
}

PreferencesDialog::~PreferencesDialog()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

void PreferencesDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);
    
    // Setup tabs
    setupAutosaveTab();
    setupLocateTab();
    
    // Create button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

void PreferencesDialog::setupAutosaveTab()
{
    m_autosaveTab = new QWidget();
    m_tabWidget->addTab(m_autosaveTab, QIcon(":/filesave.png"), tr("Autosave"));
    
    QVBoxLayout* tabLayout = new QVBoxLayout(m_autosaveTab);
    
    // Autosave group box
    m_autosaveGroup = new QGroupBox(tr("Autosave Settings"), m_autosaveTab);
    tabLayout->addWidget(m_autosaveGroup);
    
    QGridLayout* groupLayout = new QGridLayout(m_autosaveGroup);
    
    // Enable autosave
    m_autosaveEnabledCheck = new QCheckBox(tr("Enable autosave"), m_autosaveGroup);
    m_autosaveEnabledCheck->setToolTip(tr("Automatically save the workspace at regular intervals"));
    connect(m_autosaveEnabledCheck, &QCheckBox::toggled, 
            this, &PreferencesDialog::slotAutosaveEnabledChanged);
    groupLayout->addWidget(m_autosaveEnabledCheck, 0, 0, 1, 2);
    
    // Autosave interval
    QLabel* intervalLabel = new QLabel(tr("Autosave interval:"), m_autosaveGroup);
    groupLayout->addWidget(intervalLabel, 1, 0);
    
    m_autosaveIntervalSpin = new QSpinBox(m_autosaveGroup);
    m_autosaveIntervalSpin->setRange(1, 60);
    m_autosaveIntervalSpin->setSuffix(tr(" minutes"));
    m_autosaveIntervalSpin->setToolTip(tr("Time between automatic saves (1-60 minutes)"));
    connect(m_autosaveIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PreferencesDialog::slotAutosaveIntervalChanged);
    groupLayout->addWidget(m_autosaveIntervalSpin, 1, 1);
    
    // Use backup files
    m_autosaveUseBackupCheck = new QCheckBox(tr("Create backup files"), m_autosaveGroup);
    m_autosaveUseBackupCheck->setToolTip(tr("Save to timestamped backup files instead of overwriting the current file"));
    connect(m_autosaveUseBackupCheck, &QCheckBox::toggled,
            this, &PreferencesDialog::slotAutosaveUseBackupChanged);
    groupLayout->addWidget(m_autosaveUseBackupCheck, 2, 0, 1, 2);
    
    // Maximum backup files
    QLabel* maxBackupsLabel = new QLabel(tr("Maximum backup files:"), m_autosaveGroup);
    groupLayout->addWidget(maxBackupsLabel, 3, 0);
    
    m_autosaveMaxBackupsSpin = new QSpinBox(m_autosaveGroup);
    m_autosaveMaxBackupsSpin->setRange(1, 20);
    m_autosaveMaxBackupsSpin->setToolTip(tr("Number of backup files to keep (1-20)"));
    connect(m_autosaveMaxBackupsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PreferencesDialog::slotAutosaveMaxBackupsChanged);
    groupLayout->addWidget(m_autosaveMaxBackupsSpin, 3, 1);
    
    // Restore defaults button
    m_restoreDefaultsButton = new QPushButton(tr("Restore Defaults"), m_autosaveGroup);
    m_restoreDefaultsButton->setToolTip(tr("Reset autosave settings to default values"));
    connect(m_restoreDefaultsButton, &QPushButton::clicked,
            this, &PreferencesDialog::slotRestoreDefaults);
    groupLayout->addWidget(m_restoreDefaultsButton, 4, 0, 1, 2);
    
    // Add stretch to push everything to the top
    tabLayout->addStretch();
    
    // Update control states
    updateAutosaveControls();
}

void PreferencesDialog::setupLocateTab()
{
    m_locateTab = new QWidget();
    m_tabWidget->addTab(m_locateTab, QIcon(":/fixture.png"), tr("Locate"));

    QVBoxLayout *tabLayout = new QVBoxLayout(m_locateTab);
    QGroupBox *box = new QGroupBox(tr("Locate Flash"), m_locateTab);
    tabLayout->addWidget(box);
    QGridLayout *g = new QGridLayout(box);

    QLabel *why = new QLabel(
        tr("How a fixture identifies itself when you ask where it is.\n"
           "Three white flashes at full is a lot of light to stand next to, "
           "and in a dark house it lights the room."), box);
    why->setWordWrap(true);
    g->addWidget(why, 0, 0, 1, 2);

    g->addWidget(new QLabel(tr("Colour:"), box), 1, 0);
    m_locateColor = new QColor(Monitor::locateColor());
    m_locateColorBtn = new QPushButton(box);
    m_locateColorBtn->setStyleSheet(
        QString("background-color: %1").arg(m_locateColor->name()));
    connect(m_locateColorBtn, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(*m_locateColor, this, tr("Locate colour"));
        if (c.isValid())
        {
            *m_locateColor = c;
            m_locateColorBtn->setStyleSheet(
                QString("background-color: %1").arg(c.name()));
        }
    });
    g->addWidget(m_locateColorBtn, 1, 1);

    g->addWidget(new QLabel(tr("Intensity:"), box), 2, 0);
    m_locateIntensitySpin = new QSpinBox(box);
    m_locateIntensitySpin->setRange(1, 255);
    m_locateIntensitySpin->setToolTip(
        tr("DMX level for the flash. Fixtures with no dimmer channel are "
           "scaled through their colour emitters instead, so this dims them too."));
    g->addWidget(m_locateIntensitySpin, 2, 1);

    g->addWidget(new QLabel(tr("Flashes:"), box), 3, 0);
    m_locateFlashesSpin = new QSpinBox(box);
    m_locateFlashesSpin->setRange(1, 10);
    g->addWidget(m_locateFlashesSpin, 3, 1);

    tabLayout->addStretch(1);
}

void PreferencesDialog::loadSettings()
{
    m_locateIntensitySpin->setValue(Monitor::locateIntensity());
    m_locateFlashesSpin->setValue(Monitor::locateFlashes());

    if (m_autoSaveManager)
    {
        m_autosaveEnabledCheck->setChecked(m_autoSaveManager->isEnabled());
        m_autosaveIntervalSpin->setValue(m_autoSaveManager->intervalMinutes());
        m_autosaveUseBackupCheck->setChecked(m_autoSaveManager->useBackupFiles());
        m_autosaveMaxBackupsSpin->setValue(m_autoSaveManager->maxBackupFiles());
    }
}

void PreferencesDialog::saveSettings()
{
    {
        QSettings s;
        s.setValue("monitor/locate/color", m_locateColor->name());
        s.setValue("monitor/locate/intensity", m_locateIntensitySpin->value());
        s.setValue("monitor/locate/flashes", m_locateFlashesSpin->value());
    }

    if (m_autoSaveManager)
    {
        m_autoSaveManager->setEnabled(m_autosaveEnabledCheck->isChecked());
        m_autoSaveManager->setIntervalMinutes(m_autosaveIntervalSpin->value());
        m_autoSaveManager->setUseBackupFiles(m_autosaveUseBackupCheck->isChecked());
        m_autoSaveManager->setMaxBackupFiles(m_autosaveMaxBackupsSpin->value());
        m_autoSaveManager->saveSettings();
    }
}

void PreferencesDialog::updateAutosaveControls()
{
    bool enabled = m_autosaveEnabledCheck->isChecked();
    m_autosaveIntervalSpin->setEnabled(enabled);
    m_autosaveUseBackupCheck->setEnabled(enabled);
    m_autosaveMaxBackupsSpin->setEnabled(enabled && m_autosaveUseBackupCheck->isChecked());
}

void PreferencesDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void PreferencesDialog::reject()
{
    // Restore original settings
    if (m_autoSaveManager)
    {
        m_autoSaveManager->setEnabled(m_originalEnabled);
        m_autoSaveManager->setIntervalMinutes(m_originalInterval);
        m_autoSaveManager->setUseBackupFiles(m_originalUseBackup);
        m_autoSaveManager->setMaxBackupFiles(m_originalMaxBackups);
    }
    QDialog::reject();
}

void PreferencesDialog::slotAutosaveEnabledChanged(bool enabled)
{
    Q_UNUSED(enabled)
    updateAutosaveControls();
}

void PreferencesDialog::slotAutosaveIntervalChanged(int minutes)
{
    Q_UNUSED(minutes)
    // Real-time preview could be added here if desired
}

void PreferencesDialog::slotAutosaveUseBackupChanged(bool useBackup)
{
    Q_UNUSED(useBackup)
    updateAutosaveControls();
}

void PreferencesDialog::slotAutosaveMaxBackupsChanged(int maxBackups)
{
    Q_UNUSED(maxBackups)
    // Real-time preview could be added here if desired
}

void PreferencesDialog::slotRestoreDefaults()
{
    m_autosaveEnabledCheck->setChecked(AutoSaveManager::DEFAULT_AUTOSAVE_ENABLED);
    m_autosaveIntervalSpin->setValue(AutoSaveManager::DEFAULT_AUTOSAVE_INTERVAL);
    m_autosaveUseBackupCheck->setChecked(AutoSaveManager::DEFAULT_AUTOSAVE_USE_BACKUP);
    m_autosaveMaxBackupsSpin->setValue(AutoSaveManager::DEFAULT_AUTOSAVE_MAX_BACKUPS);
}
