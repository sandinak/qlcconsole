#ifndef CUESHEET_TEST_H
#define CUESHEET_TEST_H

#include <QObject>
#include <QList>

class Doc;

class CueSheet_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void cueSheetRowsFollowTheRunningOrder();
    void subCuesHitsAndTimes();
    void trackingMarksWhatChanges();
    void runningOrderMovesSongsNotTracking();
    void csvAndXlsxAndPdf();
    void anEmptyShowMakesEmptySheets();

private:
    /** Preshow 0.0.1 / 0.0.2, song 1.0.1 / 1.0.2 (see the .cpp). */
    void buildShow();

    Doc *m_doc = nullptr;
    quint32 m_front = 0, m_back = 0;
    quint32 m_half = 0, m_full = 0, m_red = 0;
    quint32 m_prod = 0, m_list = 0;
    quint32 m_cues[4] = { 0, 0, 0, 0 };
};

#endif
