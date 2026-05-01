/*
  Q Light Controller Plus
  programmerframewizard.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef PROGRAMMERFRAMEWIZARD_H
#define PROGRAMMERFRAMEWIZARD_H

#include <QDialog>

#include "programmermap.h"

class Doc;
class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;
class VCFrame;
class QLCInputProfile;

/**
 * Dialog: pick an input profile + a programmer map + a target input
 * universe; click Generate; a new VCFrame is appended to the Virtual
 * Console populated with parameter sliders + select-fixtures buttons
 * pre-bound to the device controls.
 */
class ProgrammerFrameWizard : public QDialog
{
    Q_OBJECT

public:
    ProgrammerFrameWizard(Doc* doc, QWidget* parent = nullptr);

    /**
     * Static helper that does the actual generation. Public so callers
     * (CLI, scripts, tests) can invoke it without going through the
     * dialog UI.
     *
     * Returns the newly-created VCFrame, or nullptr on failure.
     */
    static VCFrame* generateFrame(Doc* doc,
                                  VCFrame* parent,
                                  QLCInputProfile* profile,
                                  const ProgrammerMap& map,
                                  quint32 inputUniverse);

private slots:
    void slotGenerate();
    void slotProfileChanged(int idx);
    void slotMapChanged(int idx);

private:
    void populateProfiles();
    void populateMaps();
    void updateSummary();

private:
    Doc* m_doc;
    QComboBox* m_profileCombo;
    QComboBox* m_mapCombo;
    QSpinBox* m_universeSpin;
    QLabel* m_summary;
    QPushButton* m_generateButton;
};

#endif
