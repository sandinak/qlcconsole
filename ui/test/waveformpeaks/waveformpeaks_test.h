#ifndef WAVEFORMPEAKS_TEST_H
#define WAVEFORMPEAKS_TEST_H

#include <QObject>

class WaveformPeaks_Test final : public QObject
{
    Q_OBJECT

private slots:
    void peaksFollowTheSamples();
    void aCancelledBuildIsEmpty();
    void drawsWithNoFile();
    void drawsAtAnyZoom();
    void cacheIsKeyedByPathSizeAndMtime();
    void serviceBuildsOnceAndCaches();
    void serviceRebuildsWhenTheFileChanges();
    void unreadableFileIsRememberedNotRetried();
};

#endif
