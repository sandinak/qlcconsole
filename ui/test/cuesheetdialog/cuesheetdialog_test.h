#ifndef CUESHEETDIALOG_TEST_H
#define CUESHEETDIALOG_TEST_H

#include <QObject>

class Doc;

class CueSheetDialog_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void showsTheCueSheetAndTheTrackingSheet();
    void savesWhatIsShown();
    void theShowTreeMoreMenuOpensIt();

private:
    Doc *m_doc = nullptr;
    quint32 m_prod = 0;
};

#endif
