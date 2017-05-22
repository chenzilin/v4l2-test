#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define CAPTURE_WIDTH 720
#define CAPTURE_HEIGHT 576
#define CAPTURE_PIX_FMT V4L2_PIX_FMT_MJPEG

#define CAMERA_DEVICE "/dev/video0"

#define FRAME_BUFFER_COUNT 5
struct {
    void *start;
    size_t length;
} FrameBuffer[FRAME_BUFFER_COUNT];


int main()
{
    int cam_fd;

    // open camera device
    if ((cam_fd = open(CAMERA_DEVICE, O_RDWR)) < 0) {
        fprintf(stderr, "Fail to open camera device %s .\n", CAMERA_DEVICE);
        return -1;
    }

    // query device driver information
    struct v4l2_capability v4l2_cap;
    if (ioctl(cam_fd, VIDIOC_QUERYCAP, &v4l2_cap) < 0) {
        fprintf(stderr, "Fail to query camera device information!\n");
        return -1;
    }

#ifdef DEBUG
    // print camera capability information
    printf("\033[32mCamera Capability Information:\033[0m\n");
    printf("  Driver: %s\n", v4l2_cap.driver);
    printf("  Card: %s\n", v4l2_cap.card);
    printf("  Bus_Info: %s\n", v4l2_cap.bus_info);
    printf("  Kernel Version: 0x%08X\n", v4l2_cap.version);
    printf("  Capabilities: 0x%08X\n", v4l2_cap.capabilities);
    printf("  Device Capabilities: 0x%08X\n", v4l2_cap.device_caps);
    printf("  Capable of capture: %s\n", v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ? "\033[32mYes\033[0m":"\033[33mNo\033[0m");
    printf("  Capable of streaming: %s\n", v4l2_cap.capabilities & V4L2_CAP_STREAMING ? "\033[32mYes\033[0m":"\033[33mNo\033[0m");
#endif

#ifdef DEBUG
    // query fps
    struct v4l2_streamparm streamparm;
    memset(&streamparm, 0, sizeof(struct v4l2_streamparm));
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(cam_fd, VIDIOC_G_PARM, &streamparm) == -1) {
        fprintf(stderr, "Fail to get stream parameters!\n");
        return -1;
    }

    printf("  Current Fps: %d fps\n", streamparm.parm.capture.timeperframe.denominator/streamparm.parm.capture.timeperframe.numerator);
    if ((streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) == V4L2_CAP_TIMEPERFRAME) {
        printf("  Capabilities: support pragrammable frame rates!\n");
    }
#endif

    // query camera can capture image type
    struct v4l2_fmtdesc ffmt;
    memset(&ffmt, 0, sizeof(struct v4l2_fmtdesc));
    ffmt.index = 0;
    ffmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    printf("\033[32mCamera can capture PixelFormat:\033[0m\n");
    while (ioctl(cam_fd, VIDIOC_ENUM_FMT, &ffmt) == 0) {
        printf("  %d: \033[32m0x%08X, %s\033[0m\n", ffmt.index, ffmt.pixelformat, (char *)ffmt.description);
        ffmt.index++;
    }

    // query camera can capture image size by CAPTURE_PIX_FMT
    struct v4l2_frmsizeenum fsize;
    memset(&fsize, 0, sizeof(struct v4l2_frmsizeenum));
    fsize.index = 0;
    fsize.pixel_format = CAPTURE_PIX_FMT;

    printf("\033[32mCamera can capture FrameSize by PixelFormat: 0x%08X\033[0m\n", CAPTURE_PIX_FMT);
    while (ioctl(cam_fd, VIDIOC_ENUM_FRAMESIZES, &fsize) == 0) {
        printf("  %d: \033[32m%dx%d\033[0m\n", fsize.index, fsize.discrete.width, fsize.discrete.height);
            fsize.index++;
   }

    // settings for video format
    struct v4l2_format v4l2_fmt;
    memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
    v4l2_fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.width       = CAPTURE_WIDTH;
    v4l2_fmt.fmt.pix.height      = CAPTURE_HEIGHT;
    v4l2_fmt.fmt.pix.pixelformat = CAPTURE_PIX_FMT;
    v4l2_fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
    if (ioctl(cam_fd, VIDIOC_S_FMT, &v4l2_fmt) < 0) {
        fprintf(stderr, "Fail to set capture image format!\n");
        return -1;
    }

#ifdef DEBUG
    // print capture image format information
    printf("\033[32mCapture Image Format Set Information:\033[0m\n");
    printf("  Type: %d\n", v4l2_fmt.type);
    printf("  Width: \033[32m%d\033[0m\n", v4l2_fmt.fmt.pix.width);
    printf("  Height: \033[32m%d\033[0m\n", v4l2_fmt.fmt.pix.height);
    printf("  PixelFormat: \033[32m0x%08X\033[0m\n", v4l2_fmt.fmt.pix.pixelformat);
    printf("  Field: %d\n", v4l2_fmt.fmt.pix.field);
    printf("  BytesPerLine: %d\n", v4l2_fmt.fmt.pix.bytesperline);
    printf("  SizeImage: %d\n", v4l2_fmt.fmt.pix.sizeimage);
    printf("  ColorSpace: %d\n", v4l2_fmt.fmt.pix.colorspace);
    printf("  Priv: %d\n", v4l2_fmt.fmt.pix.priv);
    printf("  RawDate: %s\n", v4l2_fmt.fmt.raw_data);
#endif

    // request buffer memory
    struct v4l2_requestbuffers req_buffer;
    req_buffer.count = FRAME_BUFFER_COUNT;
    req_buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_buffer.memory = V4L2_MEMORY_MMAP;

    if(ioctl(cam_fd, VIDIOC_REQBUFS, &req_buffer) < 0) {
        fprintf(stderr, "Fail to request frame buffer!\n");
        return -1;
    }


    int i = 0;
    struct v4l2_buffer buf;
    for (i = 0; i < req_buffer.count; ++i) {

        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(ioctl(cam_fd , VIDIOC_QUERYBUF, &buf) < 0) {
            fprintf(stderr, "Fail to query frame buffer!\n");
            return -1;
        }

        // mmap buffer
        FrameBuffer[i].length = buf.length;
        FrameBuffer[i].start = (char *) mmap(0, buf.length, PROT_READ|PROT_WRITE, MAP_SHARED, cam_fd, buf.m.offset);
        if (FrameBuffer[i].start == MAP_FAILED) {
            fprintf(stderr, "Fail to mmap (%d) : %s !\n", i, strerror(errno));
            return -1;
        }

        // queen buffer
        if (ioctl(cam_fd, VIDIOC_QBUF, &buf) < 0) {
            fprintf(stderr, "Fail to queen buffer!\n");
            return -1;
        }

        printf("Frame buffer %d: address=0x%x, length=%d\n", i, (unsigned int)FrameBuffer[i].start, FrameBuffer[i].length);
    }

    // start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(cam_fd, VIDIOC_STREAMON, &type) < 0) {
        printf("VIDIOC_STREAMON failed!\n");
        return -1;
    }

    for (i = 0; i < FRAME_BUFFER_COUNT; ++i) {
        // Get frame
        if (ioctl(cam_fd, VIDIOC_DQBUF, &buf) < 0) {
            printf("VIDIOC_DQBUF failed!\n");
            return -1;
        }

        char filename[64];
        sprintf(filename, "%d.jpg", i);
        FILE *fp = fopen(filename, "wb");
        if (fp < 0) {
            printf("open frame data file failed\n");
            return -1;
        }
        fwrite(FrameBuffer[buf.index].start, 1, buf.bytesused, fp);
        fclose(fp);
        printf("Capture one frame saved in %s\n", filename);

        // requeue buffer
        if (ioctl(cam_fd, VIDIOC_QBUF, &buf) < 0) {
            printf("VIDIOC_QBUF failed!\n");
            return -1;
        }
    }

    // stop streaming
    if (ioctl(cam_fd, VIDIOC_STREAMOFF, &type) < 0) {
        printf("VIDIOC_STREAMON failed!\n");
        return -1;
    }

    // release the resource
    for (i = 0; i < FRAME_BUFFER_COUNT; i++)
    {
        munmap(FrameBuffer[i].start, FrameBuffer[i].length);
    }

    close(cam_fd);

    return 0;
}
