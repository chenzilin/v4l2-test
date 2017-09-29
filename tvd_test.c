#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include "video.h"

int main(int argc, char** argv)
{
	Video_st2 vst={
		.channel = 0,
		.show_rect = {
			.x = 0, 
			.y = 0,
			.width = 720,
			.height = 576,
		},
		.crop_rect = {
			.x = 0,
			.y = 0,
			.width = 720,
			.height = 576,
		},
	};

	video_init(stdout);
	video_start(&vst);
	getchar();
	video_stop();

	return 0;
}
