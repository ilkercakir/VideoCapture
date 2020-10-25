#ifndef v4l2H
#define v4l2H

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
#include <asm/types.h>
#include <math.h>
#include <linux/videodev2.h>
#include <jpeglib.h>
#include <math.h>
#include <gtk/gtk.h>

#include "VideoQueue.h"

typedef enum
{
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
}io_method;

typedef struct
{
	io_method io;
	int fd;
	struct buffer *buffers;
	unsigned int n_buffers;
	unsigned int width;
	unsigned int height;
	int framesizeready;
	pthread_mutex_t *sizemutex;
	pthread_cond_t *sizeready;
	char devicename[20];
}v4l2params;

struct buffer 
{
	void *start;
	size_t length;
};

int enumerate_video_devices(GtkWidget *combovideodev);
void init_v4l2params(v4l2params *p, io_method io);
void close_v4l2params(v4l2params *p);
void errno_exit(const char* s);
int xioctl(int fd, int request, void* argp);
void imageProcess(const void* p, int width, int height, vqstatus *vqs);
int frameRead(vqstatus *vqs, v4l2params *p);
long get_first_time_microseconds(long long *usecs);
long get_next_time_microseconds(long long *usecs);
void mainLoop(vqstatus *vqs, int *running, v4l2params *p);
void captureStop(v4l2params *p);
void captureStart(v4l2params *p);
void deviceUninit(v4l2params *p);
void readInit(unsigned int buffer_size, v4l2params *p);
void mmapInit(v4l2params *p);
void userptrInit(unsigned int buffer_size, v4l2params *p);
void deviceInit(v4l2params *p);
void deviceClose(v4l2params *p);
void deviceOpen(v4l2params *p);
#endif
