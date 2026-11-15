#ifndef CALLINGSCRIPT_TEST_H
#define CALLINGSCRIPT_TEST_H

#include <QObject>

class Doc;

class CallingScript_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void aCueIsCalledOnALine();
    void anchorsSurviveRenumberMoveAndDelete();
    void editingTheTextKeepsCuesOnTheirWords();
    void importReadsTextAndMarkdown();
    void savedWithTheShowAbsentMeansNone();
    void headingsFromSectionsAndMarkdown();
    void pdfAndHtml();

    /* Cues from the script (Q237), on script-excerpt.txt: a copy of the
       format of a real script's excerpt (a Google Docs plain-text export:
       BOM, CRLF, zero-width spaces), never the real file. */
    void scriptDirectionsAreFoundInAnExport();
    void scriptCuesAreNumberedSongBySong();
    void scriptLinesWithACueAreSkipped();
    void scriptCuesNumberAroundCuesOnTheScript();
    void untickedLinesAreLeftOutAndTheRestRenumbered();
    void scriptCuesAreMadeInOneUndoStep();
    void scriptCuesWithoutASchemeFollowOn();

private:
    Doc *m_doc = nullptr;
    quint32 m_prod = 0, m_list = 0;
    quint32 m_cues[4] = { 0, 0, 0, 0 };

    /** The excerpt as the calling script (Import...), and song 2 "Lets
     *  Dance" with its first cue 2.0.1 in the list. */
    void importExcerpt();
};

#endif
