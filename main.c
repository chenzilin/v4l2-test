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
#define CAPTURE_PIX_FMT V4L2_PIX_FMT_JPEG

#define CAMERA_DEVICE "/dev/video0"

#define CAPTURED_IMAGE_PATH "./capture.jpg"

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
    printf("Camera Capability Information:\n");
    printf("  Driver: %s\n", v4l2_cap.driver);
    printf("  Card: %s\n", v4l2_cap.card);
    printf("  Bus_Info: %s\n", v4l2_cap.bus_info);
    printf("  Kernel Version: 0x%08X\n", v4l2_cap.version);
    printf("  Capabilities: 0x%08X\n", v4l2_cap.capabilities);
    printf("  Device Capabilities: 0x%08X\n", v4l2_cap.device_caps);
#endif

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
    printf("Capture Image Format Information:\n");
    printf("  Type: %d\n", v4l2_fmt.type);
    printf("  Width: %d\n", v4l2_fmt.fmt.pix.width);
    printf("  Height: %d\n", v4l2_fmt.fmt.pix.height);
    printf("  PixelFormat: %d\n", v4l2_fmt.fmt.pix.pixelformat);
    printf("  Field: %d\n", v4l2_fmt.fmt.pix.field);
    printf("  BytesPerLine: %d\n", v4l2_fmt.fmt.pix.bytesperline);
    printf("  SizeImage: %d\n", v4l2_fmt.fmt.pix.sizeimage);
    printf("  ColorSpace: %d\n", v4l2_fmt.fmt.pix.colorspace);
    printf("  Priv: %d\n", v4l2_fmt.fmt.pix.priv);
    printf("  RawDate: %s\n", v4l2_fmt.fmt.raw_data);

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

    // 最简单的Capture Image例子，不使用mmap那种复杂的方法
    void *buffer = 0;
    buffer = malloc(CAPTURE_WIDTH*CAPTURE_HEIGHT*4);

    ssize_t size = -1;
    if ((size = read(cam_fd, buffer, CAPTURE_WIDTH*CAPTURE_HEIGHT*4)) >= 0) {
        int file_fd = open(CAPTURED_IMAGE_PATH, O_RDWR | O_CREAT);
        write(file_fd, buffer, size);
        close(file_fd);
    }
    else {
        fprintf(stderr, "Fail to capture one image from camera!");
    }

    if (buffer != 0) free(buffer);

    close(cam_fd);
}
