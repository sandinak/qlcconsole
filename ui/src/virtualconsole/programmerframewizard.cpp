/*
  Q Light Controller Plus
  programmerframewizard.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
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
    resize(560, 240);

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* intro = new QLabel(tr(
        "Pick an input profile (one already loaded in this workspace, "
        "or browse-on-disk via Tools → Inputs/Outputs first) and a "
        "Programmer Map that says which controls play which roles. "
        "A new VCFrame will be appended to the Virtual Console with "
        "every widget pre-configured and pre-bound to your surface — "
        "no MIDI-learn, no manual properties."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    QFormLayout* form = new QFormLayout();
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
    layout->addWidget(m_summary, 1);

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

    // System dir + user dir, both following the QLC+ convention.
    QStringList searchDirs;
    searchDirs << QLCFile::systemDirectory(QStringLiteral("ProgrammerMaps"))
                  .absolutePath();
    searchDirs << QLCFile::userDirectory(QStringLiteral("ProgrammerMaps"),
                                         QString(),
                                         QStringList() << QStringLiteral("*.qxpm"))
                  .absolutePath();
    // Also pick up the in-tree resource dir during development.
    searchDirs << QStringLiteral("%1/../resources/programmermaps")
                      .arg(QCoreApplication::applicationDirPath());

    QSet<QString> seen;
    for (const QString& dirPath : searchDirs)
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        for (const QFileInfo& fi : dir.entryInfoList(QStringList()
                                                     << QStringLiteral("*.qxpm"),
                                                     QDir::Files,
                                                     QDir::Name))
        {
            const QString abs = fi.absoluteFilePath();
            if (seen.contains(abs))
                continue;
            seen.insert(abs);
            m_mapCombo->addItem(fi.baseName(), abs);
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

    // Layout constants
    const int CELL_W = 60;
    const int CELL_H = 110;
    const int MARGIN = 10;

    int autoIdx = 0; // for entries without explicit row/column

    for (const ProgrammerMap::Entry& e : map.entries())
    {
        QLCInputChannel* ich = profile->channels().value(e.channel);
        QString caption = ich ? ich->name() : QString::number(e.channel + 1);

        int row = e.row;
        int col = e.column;
        if (row < 0 || col < 0)
        {
            row = autoIdx / map.gridColumns();
            col = autoIdx % map.gridColumns();
            ++autoIdx;
        }

        const int x = MARGIN + col * CELL_W;
        const int y = MARGIN + row * CELL_H;

        VCWidget* w = nullptr;
        if (e.kind == ProgrammerMap::ParameterSlider)
        {
            VCSlider* slider = new VCSlider(frame, doc);
            slider->setCaption(caption);
            slider->setParameterRole(e.role);
            slider->setParameterControlByte(e.controlByte);
            slider->setSliderMode(VCSlider::Parameter);
            slider->setGeometry(x, y, CELL_W - 4, CELL_H - 4);
            w = slider;
        }
        else if (e.kind == ProgrammerMap::SelectFixturesButton ||
                 e.kind == ProgrammerMap::ClearSelectionButton)
        {
            VCButton* button = new VCButton(frame, doc);
            button->setCaption(caption);
            button->setSelectionMode(e.selectionMode);
            // ClearSelectionButton == empty fixtures + Replace mode
            // (a SelectFixtures button with nothing checked, on Replace,
            // is precisely "clear the selection").
            if (e.kind == ProgrammerMap::ClearSelectionButton)
                button->setSelectionMode(VCButton::SelectReplace);
            button->setAction(VCButton::SelectFixtures);
            button->setGeometry(x, y, CELL_W - 4, 38);
            w = button;
        }

        if (w != nullptr)
        {
            frame->addWidgetToPageMap(w);
            QSharedPointer<QLCInputSource> src(
                new QLCInputSource(inputUniverse, e.channel));
            w->setInputSource(src);
            w->show();
        }
    }

    // Size the frame to fit its contents with a little padding.
    int needWidth = MARGIN * 2 + map.gridColumns() * CELL_W;
    int needHeight = MARGIN * 2 + ((autoIdx / map.gridColumns()) + 4) * CELL_H;
    frame->setGeometry(20, 20, needWidth, needHeight);
    frame->show();

    doc->setModified();
    return frame;
}
