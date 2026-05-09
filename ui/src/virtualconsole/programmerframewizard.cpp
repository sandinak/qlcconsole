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
#include "inputpatch.h"
#include "outputpatch.h"
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
    connect(m_universeSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotProfileChanged(int)));

    populateProfiles();
    populateMaps();
    // Auto-sync profile to the initially-selected map's <Profile> ref.
    slotMapChanged(0);
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
    // Auto-pick the input profile referenced by the selected map's
    // <Profile> element, if a matching profile is loaded. Saves the
    // user from having to remember which profile pairs with which
    // map, and avoids the foot-gun where two similar profiles (e.g.
    // "APC40 mkII" vs "APC Mini mk2") sit next to each other in the
    // dropdown.
    if (m_mapCombo->count() > 0 && m_doc != nullptr)
    {
        const QString mapPath = m_mapCombo->currentData().toString();
        ProgrammerMap probe;
        if (probe.load(mapPath))
        {
            const QString wantedFile = probe.matchingProfile();
            if (!wantedFile.isEmpty())
            {
                InputOutputMap *iom = m_doc->inputOutputMap();
                for (const QString &name : iom->profileNames())
                {
                    QLCInputProfile *p = iom->profile(name);
                    if (p == nullptr)
                        continue;
                    const QString path = p->path();
                    // Match on either basename (case-insensitive) or
                    // exact name fallback.
                    const QString baseName = path.section('/', -1);
                    if (baseName.compare(wantedFile, Qt::CaseInsensitive) == 0
                        || name.compare(wantedFile, Qt::CaseInsensitive) == 0)
                    {
                        const int idx = m_profileCombo->findText(name);
                        if (idx >= 0 && idx != m_profileCombo->currentIndex())
                            m_profileCombo->setCurrentIndex(idx);
                        break;
                    }
                }
            }
        }
    }
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

    // Patch-level diagnostics for the chosen Input universe. The wizard
    // can wire widgets to channels, but it can't configure the universe's
    // patches — and the bidirectional feedback path silently breaks if
    // the profile isn't attached or the Feedback patch is missing. Warn
    // the user up-front instead of letting them chase phantom bugs.
    InputOutputMap* iom = m_doc->inputOutputMap();
    const quint32 universe = static_cast<quint32>(m_universeSpin->value() - 1);
    if (iom != nullptr)
    {
        QStringList warnings;

        InputPatch* ip = iom->inputPatch(universe);
        if (ip == nullptr || !ip->isPatched())
        {
            warnings << tr("• Universe %1 has no Input patch — hardware "
                           "presses won't reach the VC.")
                            .arg(m_universeSpin->value());
        }
        else if (profile != nullptr
                 && (ip->profile() == nullptr
                     || ip->profile()->name() != m_profileCombo->currentText()))
        {
            warnings << tr("• Universe %1 input patch isn't using profile "
                           "\"%2\". Per-channel feedback values won't be "
                           "applied; LEDs may stay dark even with a "
                           "Feedback patch.")
                            .arg(m_universeSpin->value())
                            .arg(m_profileCombo->currentText());
        }

        if (iom->feedbackPatch(universe) == nullptr
            || !iom->feedbackPatch(universe)->isPatched())
        {
            warnings << tr("• Universe %1 has no Feedback patch — VC "
                           "state changes won't light controller LEDs. "
                           "Add one in Tools → Inputs/Outputs (point it "
                           "at the same MIDI device as the Input patch).")
                            .arg(m_universeSpin->value());
        }

        if (!warnings.isEmpty())
            text += tr("\n\nHeads-up:\n") + warnings.join(QStringLiteral("\n"));
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
    const int TOP_MARGIN = 40;       // clear VCFrame title bar / header
    const int COL_W = 90;            // column pitch
    const int ROW_GAP = 8;           // vertical gap between rows
    const int W_INSET = 6;           // widget width inset within column
    const int SLIDER_H = 220;        // slider row height (tall fader)
    const int KNOB_H = 80;           // knob row height (compact rotary)
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
        const bool isSlider = (e.kind == ProgrammerMap::ParameterSlider
                               || e.kind == ProgrammerMap::GrandMasterSlider);
        const bool isKnob = isSlider && (e.style == VCSlider::WKnob);
        const bool isPadCell = (e.kind == ProgrammerMap::FixturePadCellButton);
        int h;
        if (isKnob)         h = KNOB_H;
        else if (isSlider)  h = SLIDER_H;
        else if (isPadCell) h = 35;       // compact 30×30 cell + margin
        else                h = BUTTON_H;
        // Spanning widgets stretch across rowSpan rows; they don't
        // contribute to any row's height (so e.g. a tall knob spanning
        // two pad rows doesn't inflate those pad rows). Just register
        // the spanned rows so they get Y positions.
        if (e.rowSpan > 1)
        {
            for (int r = row; r < row + e.rowSpan; ++r)
                rowHeights.insert(r, rowHeights.value(r, 0));
        }
        else
        {
            rowHeights[row] = qMax(rowHeights.value(row, 0), h);
        }
        maxColumn = qMax(maxColumn, col);
    }

    // Cumulative row Y offsets.
    QList<int> rowsSorted = rowHeights.keys();
    std::sort(rowsSorted.begin(), rowsSorted.end());
    QHash<int, int> rowY;
    int yCursor = TOP_MARGIN;
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
        const int y = rowY.value(row, TOP_MARGIN);
        const int w = COL_W - W_INSET;

        // Resolve the rendered height. With rowSpan > 1 the widget
        // can use up to (sum of spanned row heights + intervening
        // gaps); we render at min(natural, spanned) so the widget
        // never overflows past its span (which would clip into the
        // next row), and so non-stretching widgets like buttons keep
        // their natural compact size in shared rows.
        auto spannedHeight = [&](int naturalH) {
            if (e.rowSpan <= 1)
                return naturalH;
            int total = 0;
            for (int r = row; r < row + e.rowSpan; ++r)
                total += rowHeights.value(r, 0);
            total += (e.rowSpan - 1) * ROW_GAP;
            return qMin(naturalH, total);
        };

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
            const int natH = (e.style == VCSlider::WKnob) ? KNOB_H : SLIDER_H;
            slider->setGeometry(x, y, w, spannedHeight(natH));
            if (e.style == VCSlider::WKnob)
                slider->setWidgetStyle(VCSlider::WKnob);
            widget = slider;
            ++slidersMade;
        }
        else if (e.kind == ProgrammerMap::GrandMasterSlider)
        {
            VCSlider* slider = new VCSlider(frame, doc);
            slider->setCaption(ProgrammerFrameWizard::tr("Master"));
            slider->setSliderMode(VCSlider::GrandMaster);
            const int natH = (e.style == VCSlider::WKnob) ? KNOB_H : SLIDER_H;
            slider->setGeometry(x, y, w, spannedHeight(natH));
            if (e.style == VCSlider::WKnob)
                slider->setWidgetStyle(VCSlider::WKnob);
            widget = slider;
            ++slidersMade;
        }
        else if (e.kind == ProgrammerMap::SaveProgrammerButton)
        {
            VCButton* button = new VCButton(frame, doc);
            button->setCaption(ProgrammerFrameWizard::tr("Save"));
            button->setAction(VCButton::SaveProgrammer);
            // Mirror the APC40 mk2's RECORD key, which is red.
            button->setBackgroundColor(QColor(180, 30, 30));
            button->setGeometry(x, y, w, spannedHeight(BUTTON_H));
            widget = button;
            ++buttonsMade;
        }
        else if (e.kind == ProgrammerMap::RevertProgrammerButton)
        {
            VCButton* button = new VCButton(frame, doc);
            button->setCaption(ProgrammerFrameWizard::tr("Revert"));
            button->setAction(VCButton::RevertProgrammer);
            button->setGeometry(x, y, w, spannedHeight(BUTTON_H));
            widget = button;
            ++buttonsMade;
        }
        else if (e.kind == ProgrammerMap::PadModeButton)
        {
            VCButton* button = new VCButton(frame, doc);
            QString cap;
            switch (e.padMode)
            {
            case 1: cap = ProgrammerFrameWizard::tr("Fixt\nselect"); break;
            case 2: cap = ProgrammerFrameWizard::tr("Gobo\nselect"); break;
            case 3: cap = ProgrammerFrameWizard::tr("Color\npalette"); break;
            default: cap = ProgrammerFrameWizard::tr("Pad\noff"); break;
            }
            button->setCaption(cap);
            button->setPadMode(static_cast<Doc::PadMode>(e.padMode));
            button->setAction(VCButton::PadModeSelect);
            button->setGeometry(x, y, w, spannedHeight(BUTTON_H));
            widget = button;
            ++buttonsMade;
        }
        else if (e.kind == ProgrammerMap::FixturePadCellButton)
        {
            VCButton* button = new VCButton(frame, doc);
            // No caption — pad cells are LED-only on hardware. The
            // VC widget shows up as a small empty bar so the user
            // can also click via mouse. Width matches slider width so
            // the pad columns line up with the fader columns below.
            button->setCaption(QString());
            button->setPadCell(e.padRow, e.padCol);
            button->setAction(VCButton::FixturePadCell);
            button->setGeometry(x, y, w, spannedHeight(30));
            widget = button;
            ++buttonsMade;
        }
        else if (e.kind == ProgrammerMap::ChaserStepNextButton
                 || e.kind == ProgrammerMap::ChaserStepPrevButton)
        {
            VCButton* button = new VCButton(frame, doc);
            const bool isNext = (e.kind == ProgrammerMap::ChaserStepNextButton);
            button->setCaption(isNext ? ProgrammerFrameWizard::tr("Step\n›")
                                       : ProgrammerFrameWizard::tr("Step\n‹"));
            button->setAction(isNext ? VCButton::ChaserStepNext
                                      : VCButton::ChaserStepPrev);
            button->setGeometry(x, y, w, spannedHeight(BUTTON_H));
            widget = button;
            ++buttonsMade;
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
                // Two-line caption: group on top, mode word on bottom.
                // Spelled out instead of symbols so the user doesn't
                // have to decode the action without a legend.
                const int g = col + 1;
                switch (e.selectionMode)
                {
                case VCButton::SelectAdd:
                    caption = ProgrammerFrameWizard::tr("G%1\nadd").arg(g);
                    break;
                case VCButton::SelectRemove:
                    caption = ProgrammerFrameWizard::tr("G%1\nrem").arg(g);
                    break;
                case VCButton::SelectToggle:
                    caption = ProgrammerFrameWizard::tr("G%1\ntog").arg(g);
                    break;
                case VCButton::SelectReplace:
                default:
                    caption = ProgrammerFrameWizard::tr("G%1\nset").arg(g);
                    break;
                }
                button->setSelectionMode(e.selectionMode);
            }
            button->setCaption(caption);
            button->setAction(VCButton::SelectFixtures);
            button->setGeometry(x, y, w, spannedHeight(BUTTON_H));
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
        // For pad cells on RGB controllers (e.g., APC40 mk2): use
        // DMX values that map to distinct + visible MIDI velocities
        // after QLC's DMX2MIDI = (val >> 1) shift.
        //   DMX 0   → velocity 0   = off
        //   DMX 26  → velocity 13  = orange (palette entry on mk2)
        //   DMX 74  → velocity 37  = green
        // Both > 0 so both produce LED output; different palette
        // entries so candidate vs selected look different.
        if (e.kind == ProgrammerMap::FixturePadCellButton)
        {
            src->setFeedbackValue(QLCInputFeedback::LowerValue,   0);
            src->setFeedbackValue(QLCInputFeedback::MonitorValue, 26);
            src->setFeedbackValue(QLCInputFeedback::UpperValue,   74);
        }
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
