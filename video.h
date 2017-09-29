#ifndef __MON_VIDEO_H__
#define __MON_VIDEO_H__

#include "mon-server.h"

int video_start(PVideo_st2 pvst);
int video_stop(void);
int video_pause(void);
int video_run(void);
int video_init(FILE *log);

#endif
