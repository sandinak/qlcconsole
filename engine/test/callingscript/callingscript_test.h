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

private:
    Doc *m_doc = nullptr;
    quint32 m_prod = 0, m_list = 0;
    quint32 m_cues[4] = { 0, 0, 0, 0 };
};

#endif
