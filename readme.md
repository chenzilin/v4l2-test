Simplistic v4l2 camera test example, use mmap

Usage:
	cmake . && make -j4

Sunxi A20/T3 Format:
	ffmpeg -s 720x576 -pix_fmt nv12 -i capture0.yuv -f image2 -pix_fmt rgb24 captdump.png

Raw image viewer:
	RawViewer.exe

Reference:
	https://my.oschina.net/luckysym/blog/224933
