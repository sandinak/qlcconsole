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

    /* Make Cues... (Q237): the script's SCENE / LIGHTING lines as cues. */
    void makeCuesPreviewsThenMakesInOneStep();
    void cancellingTheCuesMakesNothing();
    void cuesGoInTheShowTreesCurrentList();
    /** Before/after pictures at 1024x640, 1280x800 and 1920x1080, only with
     *  QLC_SHOT_DIR set. */
    void pictures();

private:
    void settle(int ticks = 3);
    int offsetOfLine(int line) const;
    /** The excerpt (engine/test/callingscript/script-excerpt.txt) as text. */
    static QString excerpt();

    Doc *m_doc = nullptr;
    quint32 m_prod = 0, m_list = 0;
    quint32 m_cues[3] = { 0, 0, 0 };
};

#endif
