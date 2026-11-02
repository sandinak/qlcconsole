#ifndef CALLINGSCRIPTVIEW_TEST_H
#define CALLINGSCRIPTVIEW_TEST_H

#include <QObject>

class Doc;

class CallingScriptView_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void cueHereCallsTheSelectedWord();
    void droppingACueFromTheTreeOnAWord();
    void marginShowsNumberNameAndNote();
    void followsTheRunningCueAndMarksStandby();
    void followOffLeavesTheScrollAlone();
    void goGoesThroughTheRunPath();
    void importAndPdf();
    void typedTextKeepsCuesOnTheirWords();
    void theShowTreeMenuOpensIt();

private:
    void settle(int ticks = 3);
    int offsetOfLine(int line) const;

    Doc *m_doc = nullptr;
    quint32 m_prod = 0, m_list = 0;
    quint32 m_cues[3] = { 0, 0, 0 };
};

#endif
