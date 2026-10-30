/*
  qlcconsole - Unit tests
  legacysettings_test.h

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef LEGACYSETTINGS_TEST_H
#define LEGACYSETTINGS_TEST_H

#include <QObject>
#include <QTemporaryDir>

class LegacySettings_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void theStoreIsQlcPlus4s();
    void recentShowsComeAfterQlcconsolesOwn();
    void aFullRecentListTakesNoMore();
    void preferencesAreCopiedOnlyWhereMissing();
    void windowLayoutsAndOtherKeysAreLeftBehind();
    void qlcPlusIsNeverWritten();
    void theOfferIsMadeOnce();
    void thePlanIsDescribedInAFewLines();

private:
    QTemporaryDir *m_dir = nullptr;
    QString m_from;
    QString m_to;
    QString file(const QString &name);
};

#endif
