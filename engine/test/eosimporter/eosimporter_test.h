/*
  Q Light Controller Plus - qlcconsole
  eosimporter_test.h

  Licensed under the Apache License, Version 2.0.
*/

#ifndef EOSIMPORTER_TEST_H
#define EOSIMPORTER_TEST_H

#include <QObject>

class Doc;

class EosImporter_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void mapperWords();
    void mapperExactAndSuggested();
    /** ETC ColorSource Spot, Spot jr and CYC: each has 5-Channel, Direct,
     *  1-Channel and RGB with ETC's footprints and a strobe that is open at
     *  0 and 255; Eos's _RGB personalities resolve Exact to RGB. */
    void colorSourceDefinitionsAndTheirRgbPersonalities();
    void mapperSavedChoice();
    void fixtures();
    void groups();
    void palettes();
    void cueLists();
    void presetsAndSubs();
    void warnings();
    void circuitClassifier();
    void specialCircuits();
    void subsForTheConsole();
    void realExportIntoShowTree();
    void magicSheet();
    void showFromSampleDir();

private:
    Doc *m_doc = nullptr;
};

#endif
