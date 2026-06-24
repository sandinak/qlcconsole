/*
  Q Light Controller Plus
  hidprofileeditor.h

  Dialog for editing a HID device profile (.qxhid) — axis/button names,
  semantic slot assignments (pan/tilt), and button action bindings.

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef HIDPROFILEEDITOR_H
#define HIDPROFILEEDITOR_H

#include <QDialog>

class HIDProfile;
class QTableWidget;
class QDoubleSpinBox;
class QLabel;

class HIDProfileEditor : public QDialog
{
    Q_OBJECT

public:
    explicit HIDProfileEditor(HIDProfile *profile, QWidget *parent = nullptr);

private slots:
    void slotAccepted();

private:
    void buildAxisTable();
    void buildButtonTable();
    void applyTableToProfile();

    HIDProfile      *m_profile;
    QTableWidget    *m_axisTable    = nullptr;
    QTableWidget    *m_buttonTable  = nullptr;
    QDoubleSpinBox  *m_sensSpin     = nullptr;
    QDoubleSpinBox  *m_dzSpin       = nullptr;
};

#endif // HIDPROFILEEDITOR_H
