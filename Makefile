CC ?= arm-linux-gnueabi-gcc

TARGET := sunxi_tvd

.PHONY: all clean

all: $(TARGET)

sunxi_tvd: sunxi_tvd.c video.c
	$(CC) sunxi_tvd.c video.c -o $(TARGET) -lpthread

clean:
	rm -rf $(TARGET)
