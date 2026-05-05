/*
  Q Light Controller Plus
  programmerframewizard.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QCoreApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>

#include "programmerframewizard.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"
#include "qlcinputsource.h"
#include "qlcfile.h"
#include "vcframe.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "virtualconsole.h"

ProgrammerFrameWizard::ProgrammerFrameWizard(Doc* doc, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
{
    setWindowTitle(tr("Generate Programmer Frame from Input Profile"));
    setMinimumWidth(620);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel* intro = new QLabel(tr(
        "Pick an input profile (one already loaded in this workspace, "
        "or browse-on-disk via Tools → Inputs/Outputs first) and a "
        "Programmer Map that says which controls play which roles. "
        "A new VCFrame will be appended to the Virtual Console with "
        "every widget pre-configured and pre-bound to your surface — "
        "no MIDI-learn, no manual properties."), this);
    intro->setWordWrap(true);
    intro->setMinimumHeight(intro->sizeHint().height());
    layout->addWidget(intro);

    QFormLayout* form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_profileCombo = new QComboBox(this);
    m_mapCombo = new QComboBox(this);
    m_universeSpin = new QSpinBox(this);
    m_universeSpin->setRange(1, 16);
    m_universeSpin->setValue(1);

    form->addRow(tr("Input profile"), m_profileCombo);
    form->addRow(tr("Programmer map"), m_mapCombo);
    form->addRow(tr("Input universe"), m_universeSpin);
    layout->addLayout(form);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::PlainText);
    m_summary->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    layout->addWidget(m_summary);
    layout->addStretch(1);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    m_generateButton = buttons->addButton(tr("Generate"),
                                          QDialogButtonBox::AcceptRole);
    QPushButton* cancel = buttons->addButton(tr("Cancel"),
                                             QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(m_generateButton, &QPushButton::clicked,
            this, &ProgrammerFrameWizard::slotGenerate);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_profileCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotProfileChanged(int)));
    connect(m_mapCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotMapChanged(int)));

    populateProfiles();
    populateMaps();
    updateSummary();
}

void ProgrammerFrameWizard::populateProfiles()
{
    m_profileCombo->clear();
    InputOutputMap* iom = m_doc->inputOutputMap();
    if (iom == nullptr)
        return;
    QStringList names = iom->profileNames();
    names.sort();
    for (const QString& name : names)
        m_profileCombo->addItem(name);
}

void ProgrammerFrameWizard::populateMaps()
{
    m_mapCombo->clear();

    QStringList searchDirs;
    // System dir per QLC+ convention.
    searchDirs << QLCFile::systemDirectory(QStringLiteral("ProgrammerMaps"))
                  .absolutePath();
    // User-level overrides.
    searchDirs << QLCFile::userDirectory(QStringLiteral("ProgrammerMaps"),
                                         QString(),
                                         QStringList() << QStringLiteral("*.qxpm"))
                  .absolutePath();
    // macOS bundle Resources dir (capital R).
    const QString appDir = QCoreApplication::applicationDirPath();
    searchDirs << QStringLiteral("%1/../Resources/programmermaps").arg(appDir);
    // In-tree dev: binary is in build/main/, source is two levels up.
    searchDirs << QStringLiteral("%1/../../resources/programmermaps").arg(appDir);
    // …and one level up, for build layouts that put binaries one deeper.
    searchDirs << QStringLiteral("%1/../resources/programmermaps").arg(appDir);

    QSet<QString> seen;
    for (const QString& dirPath : searchDirs)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QString canonical = dir.canonicalPath();
        if (seen.contains(canonical))
            continue;
        seen.insert(canonical);
        for (const QFileInfo& fi : dir.entryInfoList(QStringList()
                                                     << QStringLiteral("*.qxpm"),
                                                     QDir::Files,
                                                     QDir::Name))
        {
            m_mapCombo->addItem(fi.baseName(), fi.absoluteFilePath());
        }
    }
}

void ProgrammerFrameWizard::slotProfileChanged(int)
{
    updateSummary();
}

void ProgrammerFrameWizard::slotMapChanged(int)
{
    updateSummary();
}

void ProgrammerFrameWizard::updateSummary()
{
    QString text;
    QLCInputProfile* profile = nullptr;
    if (!m_profileCombo->currentText().isEmpty())
    {
        profile = m_doc->inputOutputMap()->profile(m_profileCombo->currentText());
    }
    if (profile != nullptr)
    {
        text += tr("Profile: %1 (%2 channels). ")
                    .arg(m_profileCombo->currentText())
                    .arg(profile->channels().size());
    }

    if (m_mapCombo->count() == 0)
    {
        text += tr("\nNo programmer maps found. Drop a .qxpm file into "
                   "resources/programmermaps/ in the source tree, or "
                   "into ~/Library/Application Support/QLC+/ProgrammerMaps "
                   "(macOS).");
        m_generateButton->setEnabled(false);
    }
    else
    {
        QString mapPath = m_mapCombo->currentData().toString();
        ProgrammerMap map;
        if (map.load(mapPath))
        {
            text += tr("\nMap: %1 %2 (%3 widgets)")
                        .arg(map.manufacturer())
                        .arg(map.model())
                        .arg(map.entries().size());
        }
        m_generateButton->setEnabled(profile != nullptr);
    }
    m_summary->setText(text);
}

void ProgrammerFrameWizard::slotGenerate()
{
    QLCInputProfile* profile =
        m_doc->inputOutputMap()->profile(m_profileCombo->currentText());
    if (profile == nullptr)
    {
        QMessageBox::warning(this, tr("No profile"),
                             tr("Select a loaded input profile first."));
        return;
    }

    ProgrammerMap map;
    if (!map.load(m_mapCombo->currentData().toString()))
    {
        QMessageBox::warning(this, tr("Invalid map"),
                             tr("Failed to load programmer map %1")
                                 .arg(m_mapCombo->currentText()));
        return;
    }

    VirtualConsole* vc = VirtualConsole::instance();
    if (vc == nullptr)
    {
        QMessageBox::warning(this, tr("No Virtual Console"),
                             tr("Virtual Console is not available."));
        return;
    }

    quint32 universe = static_cast<quint32>(m_universeSpin->value() - 1);

    VCFrame* parent = vc->contents();
    VCFrame* frame = generateFrame(m_doc, parent, profile, map, universe);
    if (frame == nullptr)
    {
        QMessageBox::warning(this, tr("Generation failed"),
                             tr("Could not generate the programmer frame."));
        return;
    }

    QMessageBox::information(this, tr("Done"),
                             tr("Generated %1 widgets into the new "
                                "Programmer frame. Assign your fixture "
                                "groups to the SelectFixtures buttons "
                                "via right-click → Properties.")
                                 .arg(frame->findChildren<VCWidget*>().size()));
    accept();
}

VCFrame* ProgrammerFrameWizard::generateFrame(Doc* doc,
                                              VCFrame* parent,
                                              QLCInputProfile* profile,
                                              const ProgrammerMap& map,
                                              quint32 inputUniverse)
{
    if (doc == nullptr || parent == nullptr || profile == nullptr)
        return nullptr;

    VCFrame* frame = new VCFrame(parent, doc, true);
    frame->setCaption(QString("%1 %2 Programmer")
                          .arg(map.manufacturer()).arg(map.model()).trimmed());
    parent->addWidgetToPageMap(frame);

    // Sizing: real, generously sized widgets so the surface mapping is
    // legible without zooming the VC.
    const int MARGIN = 12;
    const int COL_W = 90;            // column pitch
    const int ROW_GAP = 8;           // vertical gap between rows
    const int W_INSET = 6;           // widget width inset within column
    const int SLIDER_H = 220;        // slider row height
    const int BUTTON_H = 60;         // button row height

    // Pre-pass: determine each row's max widget height so mixed rows
    // (rare but possible) lay out cleanly.
    QHash<int, int> rowHeights;
    int autoIdx = 0;
    int maxColumn = 0;
    for (const ProgrammerMap::Entry& e : map.entries())
    {
        int row = e.row;
        int col = e.column;
        if (row < 0 || col < 0)
        {
            row = autoIdx / map.gridColumns();
            col = autoIdx % map.gridColumns();
            ++autoIdx;
        }
        int h = (e.kind == ProgrammerMap::ParameterSlider) ? SLIDER_H : BUTTON_H;
        rowHeights[row] = qMax(rowHeights.value(row, 0), h);
        maxColumn = qMax(maxColumn, col);
    }

    // Cumulative row Y offsets.
    QList<int> rowsSorted = rowHeights.keys();
    std::sort(rowsSorted.begin(), rowsSorted.end());
    QHash<int, int> rowY;
    int yCursor = MARGIN;
    for (int r : rowsSorted)
    {
        rowY[r] = yCursor;
        yCursor += rowHeights[r] + ROW_GAP;
    }
    const int frameHeight = yCursor + MARGIN;
    const int frameWidth = MARGIN * 2 + (maxColumn + 1) * COL_W;

    // Second pass: actually create widgets.
    autoIdx = 0;
    int slidersMade = 0;
    int buttonsMade = 0;
    int selectButtonIndex = 0;
    for (const ProgrammerMap::Entry& e : map.entries())
    {
        int row = e.row;
        int col = e.column;
        if (row < 0 || col < 0)
        {
            row = autoIdx / map.gridColumns();
            col = autoIdx % map.gridColumns();
            ++autoIdx;
        }

        const int x = MARGIN + col * COL_W;
        const int y = rowY.value(row, MARGIN);
        const int w = COL_W - W_INSET;

        VCWidget* widget = nullptr;
        if (e.kind == ProgrammerMap::ParameterSlider)
        {
            // Caption is role-based ("R", "Pan", "Pan ◐") — far more
            // useful on a programmer surface than the device-side
            // channel name ("Slider 1", "Knob 9", etc.).
            QString roleLabel = VCSlider::parameterRoleToString(e.role);
            if (e.controlByte == 1)
                roleLabel += QStringLiteral(" ◐"); // half-circle = LSB / fine
            VCSlider* slider = new VCSlider(frame, doc);
            slider->setCaption(roleLabel);
            slider->setParameterRole(e.role);
            slider->setParameterControlByte(e.controlByte);
            slider->setSliderMode(VCSlider::Parameter);
            slider->setGeometry(x, y, w, SLIDER_H);
            widget = slider;
            ++slidersMade;
        }
        else if (e.kind == ProgrammerMap::SelectFixturesButton ||
                 e.kind == ProgrammerMap::ClearSelectionButton)
        {
            VCButton* button = new VCButton(frame, doc);
            QString caption;
            if (e.kind == ProgrammerMap::ClearSelectionButton)
            {
                caption = ProgrammerFrameWizard::tr("Clear");
                button->setSelectionMode(VCButton::SelectReplace);
            }
            else
            {
                ++selectButtonIndex;
                caption = ProgrammerFrameWizard::tr("Group %1").arg(selectButtonIndex);
                button->setSelectionMode(e.selectionMode);
            }
            button->setCaption(caption);
            button->setAction(VCButton::SelectFixtures);
            button->setGeometry(x, y, w, BUTTON_H);
            widget = button;
            ++buttonsMade;
        }
        else
        {
            qWarning() << "ProgrammerFrameWizard: skipping unknown kind for channel"
                       << e.channel;
            continue;
        }

        frame->addWidgetToPageMap(widget);
        QSharedPointer<QLCInputSource> src(
            new QLCInputSource(inputUniverse, e.channel));
        widget->setInputSource(src);
        widget->show();
    }

    qDebug() << "ProgrammerFrameWizard: generated"
             << slidersMade << "sliders +" << buttonsMade << "buttons in a"
             << frameWidth << "x" << frameHeight << "frame";

    frame->setGeometry(20, 20, frameWidth, frameHeight);
    frame->show();

    doc->setModified();
    return frame;
}
