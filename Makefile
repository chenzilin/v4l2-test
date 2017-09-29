CC ?= arm-linux-gnueabi-gcc

TARGET := tvd_test

.PHONY: all clean

all: $(TARGET)

tvd_test: tvd_test.c video.c
	$(CC) tvd_test.c video.c -o $(TARGET) -lpthread

clean:
	rm -rf $(TARGET)
