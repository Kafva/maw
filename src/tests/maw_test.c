#ifdef MAW_TEST
#include "maw.h"


static int test_maw_update(void) {
    // file with video stream has the video stream stripped
    // More than one video/audio stream is rejected
    //
    // KEEP_CORE_FIELDS    = 0x1,
    // KEEP_ALL_FIELDS     = 0x1 << 1,
    // KEEP_COVER          = 0x1 << 2,
    // CROP_COVER          = 0x1 << 3,
    

    const char *path = ".testenv/albums/blue/audio_blue_0.mp4";

    maw_update()
}

#endif








