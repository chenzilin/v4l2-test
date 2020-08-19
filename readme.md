Simplistic v4l2 camera test example, use mmap

Usage:
	cd v4l2-test

	1. cmake . && make -j4

	2. or cross compile:
		cmake \
		  -D CMAKE_C_COMPILER="sdk-path/bin/arm-linux-gnueabi-gcc" \
		  -D CMAKE_CXX_COMPILER="sdk-path/bin/arm-linux-gnueabi-g++" . \
		  make -j4

Sunxi A20/T3 Format:
	ffmpeg -s 720x576 -pix_fmt nv12 -i capture0.yuv -f image2 -pix_fmt rgb24 captdump.png

	for program_guide.txt and tvd-test under linux/drivers/media/video/sunxi_tvd

Raw image viewer:
	RawViewer.exe

Reference:
	https://my.oschina.net/luckysym/blog/224933
