#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "mon-server.h"
#include "video.h"
#include "sunxi_disp_ioctl.h"

#define VIN_SYSTEM_NTSC	0
#define VIN_SYSTEM_PAL	1

#define VIN_ROW_NUM	1
#define VIN_COL_NUM	1
#define VIN_SYSTEM	VIN_SYSTEM_PAL

//#define LOGTIME

#define DEFAULT_FB_DEVICE	"/dev/fb0"
#define VIDEO_CHANNEL_NUM	2

//#define DEBUG
#ifdef DEBUG
#define DPRINTF(fmt, x...)	fprintf(logfile, "Debug:"fmt, ## x);
#else
#define DPRINTF(fmt, x...)
#endif

#define LOG(fmt, x...)	if(logfile) fprintf(logfile, fmt, ## x)
static FILE *logfile=NULL;

#define min(a,b)	((a)<(b)?(a):(b))
#define max(a,b)	((a)>(b)?(a):(b))

/* structure used to store information of the buffers */
struct buf_info{
	int index;
	unsigned int length;
	char *start;
};

#define CAPTURE_MAX_BUFFER		4

/* device to be used for capture */
#define CAPTURE_DEVICE		"/dev/video1"
#define CAPTURE_NAME		"Capture"
/* device to be used for display */
#define DISPLAYCTRL_DEVICE		"/dev/disp"
#define DISPLAYCTRL_NAME		"Display ctrl"

#define DEF_PIX_FMT		V4L2_PIX_FMT_YUV420	//V4L2_PIX_FMT_NV12

struct display_dev
{
	int fd;	//for frame buffer overlayer device
	rect_st scrn;
	rect_st crop;

	unsigned int hlay;
	int scn_index;//which screen 0/1
};

struct fb_info
{
	int fb_width, fb_height, fb_line_len, fb_size;
	int fb_bpp;
};
static struct fb_info fbinfo;

static struct display_dev dispdev={.fd=-1,};

struct video_dev
{
	int fd;	//for video device
	int channel;
	unsigned int offset[2];	//capture data C offset
	int cap_width, cap_height;

	struct v4l2_buffer capture_buf;
	struct buf_info buff_info[CAPTURE_MAX_BUFFER];
	int numbuffer;

};
static struct video_dev videodev={.fd=-1, };

static int rect_and(Prect_st win, Prect_st bound)
{
	unsigned int left, top, right, down;
	
	left = max(win->x, bound->x);
	top = max(win->y, bound->y);
	right = min(win->x+win->width, bound->x+bound->width);
	down = min(win->y+win->height, bound->y+bound->height);

	if(left>=right || top>=down){
		LOG("invalid rect\n");
		return -1;
	}

	if(win->x != left || win->y!=top || 
		win->width != right-left || win->height != down-top){
		win->x = left;
		win->y = top;
		win->width = right-left;
		win->height = down-top;
		LOG("regulate crop: (%d,%d)-%dx%d\n", 
			win->x, win->y, win->width, win->height);
	}

	return 0;
}

static void disp_start(struct display_dev *pddev)
{
	unsigned long arg[4];
	arg[0] = pddev->scn_index;
	arg[1] = pddev->hlay;
	ioctl(pddev->fd, DISP_CMD_VIDEO_START,	(void*)arg);
}

static void disp_stop(struct display_dev *pddev)
{
	unsigned long arg[4];
	arg[0] = pddev->scn_index;
	arg[1] = pddev->hlay;
	ioctl(pddev->fd, DISP_CMD_VIDEO_STOP,  (void*)arg);
}

static int disp_on(struct display_dev *pddev)
{
	unsigned long arg[4];
	arg[0] = 0;
	ioctl(pddev->fd, DISP_CMD_LCD_ON, (void*)arg);
}

static void video_close(struct video_dev *pvdev)
{
	int i;
	struct buf_info *buff_info;

	/* Un-map the buffers */
	for (i = 0; i < CAPTURE_MAX_BUFFER; i++){
		buff_info = &pvdev->buff_info[i];
		if(buff_info->start){
			munmap(buff_info->start, buff_info->length);
			buff_info->start = NULL;
		}
	}

	if(pvdev->fd>=0){
		close(pvdev->fd);
		pvdev->fd = -1;
	}
}

static void display_close(struct display_dev *pddev)
{
	unsigned long arg[4];

	disp_stop(pddev);

//	arg[0] = 0;
//	ioctl(pddev->fd, DISP_CMD_LCD_OFF, (void*)arg);

	arg[0] = pddev->scn_index;
	arg[1] = pddev->hlay;
	ioctl(pddev->fd, DISP_CMD_LAYER_CLOSE,	(void*)arg);

	arg[0] = pddev->scn_index;
	arg[1] = pddev->hlay;
	ioctl(pddev->fd, DISP_CMD_LAYER_RELEASE,  (void*)arg);

	if (pddev->fd >= 0){
		close(pddev->fd);
		pddev->fd = -1;
	}
}

/*===============================initCapture==================================*/
/* This function initializes capture device. It selects an active input
 * and detects the standard on that input. It then allocates buffers in the
 * driver's memory space and mmaps them in the application space.
 */
static int initCapture(const char *dev, struct video_dev *pvdev)
{
	int fd;
	struct v4l2_format fmt;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	int ret, i, w, h;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	struct v4l2_capability capability;
	int index;

	/* Open the capture device */
	fd	= open(dev, O_RDWR);
	DPRINTF("%s, %d\n", __FUNCTION__, __LINE__);

	if (fd	<= 0) {
		DPRINTF("%s, %d\n", __FUNCTION__, __LINE__);
		LOG("Cannot open = %s device\n", CAPTURE_DEVICE);
		return -1;
	}
	pvdev->fd = fd;

	/* Check if the device is capable of streaming */
	if (ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) {
		LOG("VIDIOC_QUERYCAP error\n");
		goto ERROR;
	}

	if (capability.capabilities & V4L2_CAP_STREAMING){
		LOG("%s: Capable of streaming\n", CAPTURE_NAME);
	}
	else {
		LOG("%s: Not capable of streaming\n", CAPTURE_NAME);
		goto ERROR;
	}

	//set position and auto calculate size
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	fmt.fmt.raw_data[0] =0;//interface
	fmt.fmt.raw_data[1] =VIN_SYSTEM;//system, 1=pal, 0=ntsc
	fmt.fmt.raw_data[8] =VIN_ROW_NUM;//row
	fmt.fmt.raw_data[9] =VIN_COL_NUM;//column
	fmt.fmt.raw_data[10] =1;//channel_index
	fmt.fmt.raw_data[11] =0;//channel_index
	fmt.fmt.raw_data[12] =0;//channel_index
	fmt.fmt.raw_data[13] =0;//channel_index
	if (-1 == ioctl (fd, VIDIOC_S_FMT, &fmt)){
		LOG("VIDIOC_S_FMT error!\n");
		return -1; 
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
	if (ret < 0) {
		LOG("VIDIOC_G_FMT error\n");
		goto ERROR;
	}

	w = pvdev->cap_width = fmt.fmt.pix.width;
	h = pvdev->cap_height = fmt.fmt.pix.height;
	pvdev->offset[0] = w * h;

	switch(fmt.fmt.pix.pixelformat){
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		pvdev->offset[1] = w*h*3/2;
		break;
	case V4L2_PIX_FMT_YUV420:
		pvdev->offset[1] = w*h*5/4;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_HM12:
		pvdev->offset[1] = pvdev->offset[0];
		break;

	default:
		LOG("csi_format is not found!\n");
		break;

	}

	LOG("cap size %d x %d offset: %x, %x\n", 
		fmt.fmt.pix.width, fmt.fmt.pix.height, pvdev->offset[0], pvdev->offset[1]);

	/* Buffer allocation
	 * Buffer can be allocated either from capture driver or
	 * user pointer can be used
	 */
	/* Request for MAX_BUFFER input buffers. As far as Physically contiguous
	 * memory is available, driver can allocate as many buffers as
	 * possible. If memory is not available, it returns number of
	 * buffers it has allocated in count member of reqbuf.
	 * HERE count = number of buffer to be allocated.
	 * type = type of device for which buffers are to be allocated.
	 * memory = type of the buffers requested i.e. driver allocated or
	 * user pointer */
	reqbuf.count = CAPTURE_MAX_BUFFER;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret < 0) {
		LOG("Cannot allocate memory\n");
		goto ERROR;
	}
	/* Store the number of buffers actually allocated */
	pvdev->numbuffer = reqbuf.count;
	LOG("%s: Number of requested buffers = %d\n", CAPTURE_NAME,
			pvdev->numbuffer);

	memset(&buf, 0, sizeof(buf));

	/* Mmap the buffers
	 * To access driver allocated buffer in application space, they have
	 * to be mmapped in the application space using mmap system call */
	for (i = 0; i < (pvdev->numbuffer); i++) {
		buf.type = reqbuf.type;
		buf.index = i;
		buf.memory = reqbuf.memory;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0) {
			LOG("VIDIOC_QUERYCAP error\n");
			pvdev->numbuffer = i;
			goto ERROR;
		}

		pvdev->buff_info[i].length = buf.length;
		pvdev->buff_info[i].index = i;
		pvdev->buff_info[i].start = mmap(NULL, buf.length,
				PROT_READ | PROT_WRITE, MAP_SHARED, fd,
				buf.m.offset);

		if (pvdev->buff_info[i].start == MAP_FAILED) {
			LOG("Cannot mmap = %d buffer\n", i);
			pvdev->numbuffer = i;
			goto ERROR;
		}

		memset((void *) pvdev->buff_info[i].start, 0x80,
				pvdev->buff_info[i].length);
		/* Enqueue buffers
		 * Before starting streaming, all the buffers needs to be
		 * en-queued in the driver incoming queue. These buffers will
		 * be used by thedrive for storing captured frames. */
		ret = ioctl(fd, VIDIOC_QBUF, &buf);
		if (ret < 0) {
			LOG("VIDIOC_QBUF error\n");
			pvdev->numbuffer = i + 1;
			goto ERROR;
		}
	}

	LOG("%s: Init done successfully\n\n", CAPTURE_NAME);
	return 0;

ERROR:
	video_close(pvdev);

	return -1;
}

/*===============================initDisplay==================================*/
/* This function initializes display device. It sets output and standard for
 * LCD. These output and standard are same as those detected in capture device.
 * It, then, allocates buffers in the driver's memory space and mmaps them in
 * the application space */
static int initDisplay(const char *dev, struct display_dev *pddev, struct video_dev *pvdev)
{
	int fd;
	int ret=0, i;
	unsigned int hlay;
	unsigned long arg[4];
	int fb_fd;
	unsigned long fb_layer;
	__disp_layer_info_t layer_para;
	rect_st bound={.x=0, .y=0, .width=pvdev->cap_width, .height=pvdev->cap_height};

	pddev->scn_index = 0;

	if(rect_and(&pddev->crop, &bound)<0)
		return -1;

	/* Open the video display device */
	fd = open(dev, O_RDWR);
	if (fd < 0) {
		LOG("Cannot open = %s device\n", DISPLAYCTRL_DEVICE);
		return -1;
	}
	LOG("\n%s: Opened\n", DISPLAYCTRL_DEVICE);
	pddev->fd = fd;

	arg[0] = 0;
	ioctl(fd, DISP_CMD_LCD_ON, (void*)arg);

	//layer0
	arg[0] = 0;
	arg[1] = DISP_LAYER_WORK_MODE_SCALER;
	hlay = ioctl(fd, DISP_CMD_LAYER_REQUEST, (void*)arg);
	if(hlay == 0)
	{
		LOG("request layer0 fail\n");
		goto ERROR;
	}
	LOG("video layer hdl:%d\n", hlay);
	pddev->hlay = hlay;

	layer_para.mode = DISP_LAYER_WORK_MODE_SCALER;
	layer_para.pipe = 0;
	layer_para.fb.addr[0] = 0;//your Y address,modify this
	layer_para.fb.addr[1] = pvdev->offset[0]; //your C address,modify this
	layer_para.fb.addr[2] = pvdev->offset[1];
	layer_para.fb.size.width    = pvdev->cap_width;
	layer_para.fb.size.height   = pvdev->cap_height;
	layer_para.fb.mode		   = DISP_MOD_NON_MB_UV_COMBINED;//DISP_MOD_INTERLEAVED;//DISP_MOD_NON_MB_PLANAR;//DISP_MOD_NON_MB_UV_COMBINED;
	layer_para.fb.format 	   = DISP_FORMAT_YUV420;//DISP_FORMAT_YUV422;//DISP_FORMAT_YUV420;
	layer_para.fb.br_swap	   = 0;
	layer_para.fb.seq		   = DISP_SEQ_UVUV;//DISP_SEQ_UVUV;//DISP_SEQ_YUYV;//DISP_SEQ_YVYU;//DISP_SEQ_UYVY;//DISP_SEQ_VYUY//DISP_SEQ_UVUV
	layer_para.ck_enable 	   = 0;
	layer_para.alpha_en		   = 1;
	layer_para.alpha_val 	   = 0xff;
	layer_para.src_win.x 	   = pddev->crop.x;
	layer_para.src_win.y 	   = pddev->crop.y;
	layer_para.src_win.width    = pddev->crop.width;
	layer_para.src_win.height   = pddev->crop.height;
	layer_para.scn_win.x 	   = pddev->scrn.x;
	layer_para.scn_win.y 	   = pddev->scrn.y;
	layer_para.scn_win.width    = pddev->scrn.width;
	layer_para.scn_win.height   = pddev->scrn.height;
	arg[0] = pddev->scn_index;
	arg[1] = hlay;
	arg[2] = (unsigned long)&layer_para;
	ioctl(fd,DISP_CMD_LAYER_SET_PARA,(void*)arg);
#if 0
	arg[0] = pddev->scn_index;
	arg[1] = hlay;
	ioctl(disphd,DISP_CMD_LAYER_TOP,(void*)arg);
#endif
	arg[0] = pddev->scn_index;
	arg[1] = hlay;
	ioctl(fd,DISP_CMD_LAYER_OPEN,(void*)arg);

#if 1
	fb_fd = open(DEFAULT_FB_DEVICE, O_RDWR);
	if (ioctl(fb_fd, FBIOGET_LAYER_HDL_0, &fb_layer) == -1) {
		LOG("get fb layer handel\n");
	}

	arg[0] = 0;
	arg[1] = fb_layer;
	ioctl(fd, DISP_CMD_LAYER_BOTTOM, (void *)arg);

	disp_start(pddev);
	return 0;
#endif

ERROR:
	display_close(pddev);

	return -1;
}

static int Getfb_info(char *dev)
{
	int fb;
	struct fb_var_screeninfo fb_vinfo;
	struct fb_fix_screeninfo fb_finfo;
	char* fb_dev_name=NULL;
	
	if (!(fb_dev_name = getenv("FRAMEBUFFER")))
		fb_dev_name=dev;

	fb = open (fb_dev_name, O_RDWR);
	if(fb<0){
		DPRINTF("device %s open failed\n", fb_dev_name);
		return -1;
	}
	
	if (ioctl(fb, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		DPRINTF("Can't get VSCREENINFO: %s\n");
		close(fb);
		return -1;
	}

	if (ioctl(fb, FBIOGET_FSCREENINFO, &fb_finfo)) {
		DPRINTF("Can't get FSCREENINFO: %s\n");
		return -1;
	}

	fbinfo.fb_bpp= fb_vinfo.red.length + fb_vinfo.green.length +
		fb_vinfo.blue.length + fb_vinfo.transp.length;

	fbinfo.fb_width = fb_vinfo.xres;
	fbinfo.fb_height = fb_vinfo.yres;
	fbinfo.fb_line_len = fb_finfo.line_length;
	fbinfo.fb_size = fb_finfo.smem_len;

	DPRINTF("frame buffer: %d(%d)x%d,  %dbpp, 0x%xbyte\n", 
		fbinfo.fb_width, fbinfo.fb_line_len, fbinfo.fb_height, fbinfo.fb_bpp, fbinfo.fb_size);
		
	close(fb);

	return 0;
}

static int disp_set_addr(struct display_dev *pddev, struct video_dev *pvdev, int *addr)
{
	unsigned long arg[4];

	__disp_video_fb_t  fb_addr;
	memset(&fb_addr, 0, sizeof(__disp_video_fb_t));

	fb_addr.interlace		= 0;
	fb_addr.top_field_first = 0;
	fb_addr.frame_rate		= 25;
	fb_addr.addr[0] = *addr; //your Y address
	fb_addr.addr[1] = *addr + pvdev->offset[0];//your C address
	fb_addr.addr[2] = *addr + pvdev->offset[1];//your C address

	fb_addr.id = 0;  //TODO

	arg[0] = pddev->scn_index;
	arg[1] = pddev->hlay;
	arg[2] = (unsigned long)&fb_addr;
	ioctl(pddev->fd, DISP_CMD_VIDEO_SET_FB, (void*)arg);
}

static int video_start_cap(struct display_dev *pddev, struct video_dev *pvdev)
{
	int a, ret;

	/* run section
	 * STEP2:
	 * Here display and capture channels are started for streaming. After
	 * this capture device will start capture frames into enqueued
	 * buffers and display device will start displaying buffers from
	 * the qneueued buffers */

	/* Start Streaming. on capture device */
	a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(pvdev->fd, VIDIOC_STREAMON, &a);
	if (ret < 0) {
		LOG("capture VIDIOC_STREAMON error fd=%d\n", pvdev->fd);
		return ret;
	}
	LOG("%s: Stream on...\n", CAPTURE_NAME);

	/* Set the capture buffers for queuing and dqueueing operation */
	pvdev->capture_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	pvdev->capture_buf.index = 0;
	pvdev->capture_buf.memory = V4L2_MEMORY_MMAP;

	return 0;
}

static inline int video_cap_frame(struct display_dev *pddev, struct video_dev *pvdev)
{
	int ret;
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

#ifdef LOGTIME
	struct timeval btime, ctime, ltime;

	gettimeofday( &btime, NULL );
	ltime = btime;
	LOG("Begin\n");
#endif

	pthread_testcancel();
	/* Dequeue capture buffer */
	ret = ioctl(pvdev->fd, VIDIOC_DQBUF, &buf);
	if (ret < 0) {
		LOG("Cap VIDIOC_DQBUF");
		return ret;
	}
#ifdef LOGTIME
	gettimeofday( &ctime, NULL );
	LOG("DQ cap: %ld(%ld)\n", 
		(ctime.tv_sec-btime.tv_sec)*1000000 + ctime.tv_usec-btime.tv_usec, 
		(ctime.tv_sec-ltime.tv_sec)*1000000 + ctime.tv_usec-ltime.tv_usec);
	ltime = ctime;
#endif

	disp_set_addr(pddev,pvdev,&buf.m.offset);

#ifdef LOGTIME
	gettimeofday( &ctime, NULL );
	LOG("Process done: %ld(%ld)\n", 
		(ctime.tv_sec-btime.tv_sec)*1000000 + ctime.tv_usec-btime.tv_usec, 
		(ctime.tv_sec-ltime.tv_sec)*1000000 + ctime.tv_usec-ltime.tv_usec);
	ltime = ctime;
#endif

	pthread_testcancel();
	ret = ioctl(pvdev->fd, VIDIOC_QBUF, &buf);
	if (ret < 0) {
		LOG("Cap VIDIOC_QBUF");
		return ret;
	}
#ifdef LOGTIME
	gettimeofday( &ctime, NULL );
	LOG("Q cap: %ld(%ld)\n", 
		(ctime.tv_sec-btime.tv_sec)*1000000 + ctime.tv_usec-btime.tv_usec, 
		(ctime.tv_sec-ltime.tv_sec)*1000000 + ctime.tv_usec-ltime.tv_usec);
	ltime = ctime;
#endif

	disp_on(pddev);
	return 0;
}

static int video_stop_cap(struct display_dev *pddev, struct video_dev *pvdev)
{
	int a, ret;
	
	LOG("\n%s: Stream off!!\n", CAPTURE_NAME);

	a = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(pvdev->fd, VIDIOC_STREAMOFF, &a);
	if (ret < 0) {
		LOG("VIDIOC_STREAMOFF");
		return ret;
	}

	return 0;
}

struct video_thread{
	pthread_t th_video;
	int video_running;
	pthread_mutex_t mutex;
	int channel;
};

static struct video_thread video_thd;

static void* Show_video(void* v)
{
	if (initCapture(CAPTURE_DEVICE,&videodev) < 0) {
		DPRINTF("Failed to open:%s\n", CAPTURE_DEVICE);
		return NULL;
	}

	if(initDisplay(DISPLAYCTRL_DEVICE, &dispdev, &videodev)<0){
		DPRINTF("Failed to open:%s\n", DISPLAYCTRL_DEVICE);
		return NULL;
	}

	//start capture
	if( video_start_cap(&dispdev, &videodev)<0){
		return NULL;
	}

	while(1){
		pthread_mutex_lock(&video_thd.mutex);
		pthread_mutex_unlock(&video_thd.mutex);

		if(video_cap_frame(&dispdev, &videodev)<0)
			break;
	}

	video_stop_cap(&dispdev, &videodev);
	return NULL;
}

int video_pause(void)
{
	pthread_mutex_lock(&video_thd.mutex);
	return 0;
}

int video_run(void)
{
	pthread_mutex_unlock(&video_thd.mutex);
	return 0;
}

int video_start(PVideo_st2 pvst)
{
	pthread_mutexattr_t attr;
	Prect_st pshow, pcrop;
	
	if(pvst->channel >= VIDEO_CHANNEL_NUM){
		LOG("valid channel %d\n", pvst->channel);
	}

	pshow = &pvst->show_rect;
	pcrop = &pvst->crop_rect;
	
	if(pshow->x + pshow->width > fbinfo.fb_width ||
		pshow->y + pshow->height > fbinfo.fb_height){
		LOG("Size %dx%d out of screen\n", pshow->x + pshow->width, pshow->y + pshow->height);
		return -1;
	}

	memcpy(&dispdev.scrn, pshow, sizeof(rect_st));
	memcpy(&dispdev.crop, pcrop, sizeof(rect_st));

	videodev.channel = video_thd.channel = pvst->channel;

	if(video_thd.video_running){
		video_stop();
	}

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&video_thd.mutex, NULL);

	pthread_create(&video_thd.th_video, NULL, (void*(*)(void*))Show_video, (void*)NULL);
	video_thd.video_running=1;

	return 0;
}

int video_stop(void)
{
	if(video_thd.video_running){
		video_run();
		pthread_cancel(video_thd.th_video);
		pthread_join(video_thd.th_video, NULL);
		video_thd.video_running = 0;
	}

	video_close(&videodev);
	display_close(&dispdev);
	pthread_mutex_destroy(&video_thd.mutex);

	return 0;
}

int video_init(FILE *log)
{
	logfile = log;
	video_thd.video_running = 0;
	if(Getfb_info(DEFAULT_FB_DEVICE)<0)
		return -1;

	return 0;
}

