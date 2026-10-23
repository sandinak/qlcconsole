/*
  Q Light Controller Plus - qlcconsole
  eosimportwizard_test.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef EOSIMPORTWIZARD_TEST_H
#define EOSIMPORTWIZARD_TEST_H

#include <QObject>

class Doc;

class EosImportWizard_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void needsAFile();
    void readsTheExport();
    void choicesGoToTheMapper();
    void loadsWithTheChoices();
    void specialCircuitsPage();
    void subFaders();
    void showFromSampleDir();

private:
    Doc *m_doc = nullptr;
};

#endif
