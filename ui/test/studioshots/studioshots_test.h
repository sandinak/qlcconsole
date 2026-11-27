#ifndef STUDIOSHOTS_TEST_H
#define STUDIOSHOTS_TEST_H

#include <QObject>

/* Before / after pictures of the Lighting Studio small wins at three sizes
   (QLC_SHOT_DIR): the plot's labels, the DMX overlay with a patch clash, and
   the Rig Overview's footer. Only the old API is used unless STUDIO_NEW. */
class StudioShots_Test final : public QObject
{
    Q_OBJECT

private slots:
    void picturesForReview();
};

#endif
