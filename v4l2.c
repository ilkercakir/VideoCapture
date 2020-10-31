#include "v4l2.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

int enumerate_video_devices(GtkWidget *combovideodev)
{
	struct stat st;
	int fd;
	struct v4l2_capability video_cap;
	char videoDev[] = "/dev/video";
	char deviceName[20];
	char devicenamedisplayed[50];
	int i, count;

	for(count=0,i=0;i<64;i++)
	{
		sprintf(deviceName, "%s%d", videoDev, i);
		// stat file
		if (-1 == stat(deviceName, &st))
			continue;

		if ((fd = open(deviceName, O_RDWR | O_NONBLOCK)) == -1)
		{
			printf("cam_info: Can't open device %s\n", deviceName);
		}
		else
		{
			if (ioctl(fd, VIDIOC_QUERYCAP, &video_cap) == -1)
				printf("cam_info: Can't get capabilities\n");
			else
			{
				//printf("Name: %s %s %d.%d.%d\n", video_cap.card, video_cap.driver, (video_cap.version>>16)&0xFF, (video_cap.version>>8)&0xFF, video_cap.version&0xFF);
				sprintf(devicenamedisplayed, "%s (%s %d.%d.%d)", video_cap.card, video_cap.driver, (video_cap.version>>16)&0xFF, (video_cap.version>>8)&0xFF, video_cap.version&0xFF);
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combovideodev), deviceName, devicenamedisplayed);
			}
			close(fd);
			count++;
		}
	}

/*
	if(ioctl(fd, VIDIOCGWIN, &video_win) == -1)
		printf("cam_info: Can't get window information\n");
    else
        printf("Current size:\t%d x %d\n", video_win.width, video_win.height);

    if(ioctl(fd, VIDIOCGPICT, &video_pic) == -1)
        printf("cam_info: Can't get picture information\n");
    else
        printf("Current depth:\t%d\n", video_pic.depth);
*/

	return count;
}

void enumerate_bitrates(GtkWidget *combobitrate)
{
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "400000", "400 Kbps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "800000", "800 Kbps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "1000000", "1 Mbps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "2500000", "2.5 Mbps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "5000000", "5 Mbps");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobitrate), "8000000", "8 Mbps");
}

void enumerate_scale_values(GtkWidget *comboscale)
{
	char s[10];

	sprintf(s, "%1.2f", 0.25);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "0.25");
	sprintf(s, "%1.2f", 0.33);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "0.33");
	sprintf(s, "%1.2f", 0.50);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "0.50");
	sprintf(s, "%1.2f", 0.75);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "0.75");
	sprintf(s, "%1.2f", 1.00);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "1.00");
	sprintf(s, "%1.2f", 1.25);
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboscale), s, "1.25");
}

void init_v4l2params(v4l2params *p, io_method io)
{
	p->io = io;
	p->fd = -1;
	p->buffers = NULL;
	p->n_buffers = 0;
	p->width = 1080;
	p->height = 720;

	p->framesizeready = 0;

	p->sizemutex = malloc(sizeof(pthread_mutex_t));
	p->sizeready = malloc(sizeof(pthread_cond_t));

	int ret;
	if((ret=pthread_mutex_init(p->sizemutex, NULL))!=0 )
		printf("sizemutex init failed, %d\n", ret);
		
	if((ret=pthread_cond_init(p->sizeready, NULL))!=0 )
		printf("sizeready init failed, %d\n", ret);
}

void close_v4l2params(v4l2params *p)
{
	pthread_mutex_destroy(p->sizemutex);
	free(p->sizemutex);
	pthread_cond_destroy(p->sizeready);
	free(p->sizeready);
}

/**
  Print error message and terminate programm with EXIT_FAILURE return code.
  \param s error message to print
*/
void errno_exit(const char* s)
{
  fprintf(stderr, "%s error %d, %s\n", s, errno, strerror (errno));
  exit(EXIT_FAILURE);
}

/**
  Do ioctl and retry if error was EINTR ("A signal was caught during the ioctl() operation."). Parameters are the same as on ioctl.

  \param fd file descriptor
  \param request request
  \param argp argument
  \returns result from ioctl
*/
int xioctl(int fd, int request, void* argp)
{
  int r;

  do r = ioctl(fd, request, argp);
  while (-1 == r && EINTR == errno);

  return r;
}

/**
  process image read
*/
void imageProcess(const void* p, int width, int height, vqstatus *vqs)
{
	int buffersize = 2*width*height*sizeof(char);
	unsigned char *yuyvbuf = malloc(buffersize);
	memcpy(yuyvbuf, p, buffersize);
	vq_add(vqs, yuyvbuf);
}

/**
  read single frame
*/

int frameRead(vqstatus *vqs, v4l2params *p)
{
	struct v4l2_buffer buf;

	switch (p->io) 
	{
		case IO_METHOD_READ:
			if (-1 == read (p->fd, p->buffers[0].start, p->buffers[0].length))
			{
				switch (errno) 
				{
					case EAGAIN:
						return 0;
					case EIO:
						// Could ignore EIO, see spec.
						// fall through
					default:
						errno_exit("read");
				}
			}
			imageProcess(p->buffers[0].start, p->width, p->height, vqs);
			break;
		case IO_METHOD_MMAP:
			CLEAR (buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			if (-1 == xioctl(p->fd, VIDIOC_DQBUF, &buf))
			{
				switch (errno)
				{
					case EAGAIN:
						return 0;
					case EIO:
						// Could ignore EIO, see spec
						// fall through
					default:
						errno_exit("VIDIOC_DQBUF");
				}
			}

			assert(buf.index < p->n_buffers);

			imageProcess(p->buffers[buf.index].start, p->width, p->height, vqs);

			if (-1 == xioctl(p->fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");

			break;
		case IO_METHOD_USERPTR:
			CLEAR (buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;

			if (-1 == xioctl(p->fd, VIDIOC_DQBUF, &buf))
			{
				switch (errno)
				{
					case EAGAIN:
						return 0;
					case EIO:
						// Could ignore EIO, see spec.
						// fall through
					default:
						errno_exit("VIDIOC_DQBUF");
				}
			}

			unsigned int i;
			for (i = 0; i < p->n_buffers; ++i)
				if (buf.m.userptr == (unsigned long)p->buffers[i].start && buf.length == p->buffers[i].length)
					break;

			assert(i < p->n_buffers);

			imageProcess((void *) buf.m.userptr, p->width, p->height, vqs);

			if (-1 == xioctl(p->fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
			break;
	}

	return 1;
}

long get_first_time_microseconds(long long *usecs)
{
	long long micros;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

	micros = spec.tv_sec * 1.0e6 + round(spec.tv_nsec / 1000.0); // Convert nanoseconds to microseconds
	*usecs = micros;
	return(0L);
}

long get_next_time_microseconds(long long *usecs)
{
    long delta;
    long long micros;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    micros = spec.tv_sec * 1.0e6 + round(spec.tv_nsec / 1000.0); // Convert nanoseconds to microseconds
    delta = micros - *usecs;
    *usecs = micros;
    return(delta);
}

/** 
  mainloop: read frames and process them
*/
void mainLoop(vqstatus *vqs, int *running, v4l2params *p)
{
	unsigned int count;
	long long usecs; // microseconds

	get_first_time_microseconds(&usecs);

	count = 0;
	while (*running)
	{
		for (;;) 
		{
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(p->fd, &fds);

			/* Timeout. */
			tv.tv_sec = 2;
			tv.tv_usec = 0;

			r = select(p->fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r)
			{
				if (EINTR == errno)
					continue;

				errno_exit("select");
			}

			if (0 == r)
			{
				fprintf(stderr, "select timeout\n");
				exit(EXIT_FAILURE);
			}

			count++;
//printf("reading %d\n", count);
			if (frameRead(vqs, p))
				break;

			/* EAGAIN - continue select loop. */
		}
	}
  
	float secs = get_next_time_microseconds(&usecs)/1000000.0;
	printf("%3.2f fps\n", (float)count/secs);
}

/**
  stop capturing
*/
void captureStop(v4l2params *p)
{
	enum v4l2_buf_type type;

	switch (p->io)
	{
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(p->fd, VIDIOC_STREAMOFF, &type))
				errno_exit("VIDIOC_STREAMOFF");
			break;
   }
}

/**
  start capturing
*/
void captureStart(v4l2params *p)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (p->io) 
	{   
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
		case IO_METHOD_MMAP:
			for (i = 0; i < p->n_buffers; ++i)
			{
				struct v4l2_buffer buf;

				CLEAR (buf);

				buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory      = V4L2_MEMORY_MMAP;
				buf.index       = i;

				if (-1 == xioctl(p->fd, VIDIOC_QBUF, &buf))
					errno_exit("VIDIOC_QBUF");
			}
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(p->fd, VIDIOC_STREAMON, &type))
				errno_exit("VIDIOC_STREAMON");
			break;
		case IO_METHOD_USERPTR:
			for (i = 0; i < p->n_buffers; ++i)
			{
				struct v4l2_buffer buf;

				CLEAR (buf);

				buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory      = V4L2_MEMORY_USERPTR;
				buf.index       = i;
				buf.m.userptr   = (unsigned long) p->buffers[i].start;
				buf.length      = p->buffers[i].length;

				if (-1 == xioctl(p->fd, VIDIOC_QBUF, &buf))
					errno_exit("VIDIOC_QBUF");
			}
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(p->fd, VIDIOC_STREAMON, &type))
				errno_exit("VIDIOC_STREAMON");
			break;
	}
}

void deviceUninit(v4l2params *p)
{
	unsigned int i;

	switch (p->io) 
	{
		case IO_METHOD_READ:
			free(p->buffers[0].start);
			break;
		case IO_METHOD_MMAP:
			for (i = 0; i < p->n_buffers; ++i)
			if (-1 == munmap (p->buffers[i].start, p->buffers[i].length))
				errno_exit("munmap");
			break;
		case IO_METHOD_USERPTR:
			for (i = 0; i < p->n_buffers; ++i)
			free(p->buffers[i].start);
			break;
	}

	free(p->buffers);
}

void readInit(unsigned int buffer_size, v4l2params *p)
{
	p->buffers = calloc(1, sizeof(*(p->buffers)));

	if (!p->buffers)
	{
		fprintf (stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	p->buffers[0].length = buffer_size;
	p->buffers[0].start = malloc(buffer_size);

	if (!p->buffers[0].start)
	{
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
}

void mmapInit(v4l2params *p)
{
	struct v4l2_requestbuffers req;
	char* deviceName = p->devicename;

	CLEAR (req);

	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(p->fd, VIDIOC_REQBUFS, &req)) 
	{
		if (EINVAL == errno) 
		{
			fprintf(stderr, "%s does not support memory mapping\n", deviceName);
			exit(EXIT_FAILURE);
		} 
		else 
		{
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) 
	{
		fprintf(stderr, "Insufficient buffer memory on %s\n", deviceName);
		exit(EXIT_FAILURE);
	}

	p->buffers = calloc(req.count, sizeof(*(p->buffers)));

	if (!p->buffers) 
	{
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (p->n_buffers = 0; p->n_buffers < req.count; ++p->n_buffers) 
	{
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = p->n_buffers;

		if (-1 == xioctl(p->fd, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		p->buffers[p->n_buffers].length = buf.length;
		p->buffers[p->n_buffers].start =
		mmap (NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, p->fd, buf.m.offset);

		if (MAP_FAILED == p->buffers[p->n_buffers].start)
			errno_exit("mmap");
	}
}

void userptrInit(unsigned int buffer_size, v4l2params *p)
{
	struct v4l2_requestbuffers req;
	unsigned int page_size;
	char *deviceName = p->devicename;

	page_size = getpagesize ();
	buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

	CLEAR(req);

	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(p->fd, VIDIOC_REQBUFS, &req))
	{
		if (EINVAL == errno) 
		{
			fprintf(stderr, "%s does not support user pointer i/o\n", deviceName);
			exit(EXIT_FAILURE);
		} 
		else 
		{
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	p->buffers = calloc(4, sizeof (*(p->buffers)));

	if (!p->buffers)
	{
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	for (p->n_buffers = 0; p->n_buffers < 4; ++p->n_buffers)
	{
		p->buffers[p->n_buffers].length = buffer_size;
		p->buffers[p->n_buffers].start = memalign (/* boundary */ page_size, buffer_size);

		if (!p->buffers[p->n_buffers].start) 
		{
			fprintf(stderr, "Out of memory\n");
			exit(EXIT_FAILURE);
		}
	}
}

/**
  initialize device
*/
void deviceInit(v4l2params *p)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_standard standard;
	unsigned int min;
	char *deviceName = p->devicename;

	if (-1 == xioctl(p->fd, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			fprintf(stderr, "%s is no V4L2 device\n", deviceName);
			exit(EXIT_FAILURE);
		} 
		else
		{
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "%s is no video capture device\n", deviceName);
		exit(EXIT_FAILURE);
	}

	switch (p->io)
	{
		case IO_METHOD_READ:
			if (!(cap.capabilities & V4L2_CAP_READWRITE)) 
			{
				fprintf(stderr, "%s does not support read i/o\n", deviceName);
				exit(EXIT_FAILURE);
			}
			break;
		case IO_METHOD_MMAP:
		case IO_METHOD_USERPTR:
			if (!(cap.capabilities & V4L2_CAP_STREAMING))
			{
				fprintf(stderr, "%s does not support streaming i/o\n", deviceName);
				exit(EXIT_FAILURE);
			}
			break;
	}

	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(p->fd, VIDIOC_CROPCAP, &cropcap))
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(p->fd, VIDIOC_S_CROP, &crop))
		{
			switch (errno)
			{
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	}
	else
	{        
		/* Errors ignored. */
	}

	// set video standard
	standard.id = V4L2_STD_PAL_BG;
	if (-1 == xioctl(p->fd, VIDIOC_S_STD, &standard.id))
		errno_exit("VIDIOC_S_STD");

	CLEAR (fmt);

	// v4l2_format
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = p->width; 
	fmt.fmt.pix.height      = p->height;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

	if (-1 == xioctl(p->fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

	/* Note VIDIOC_S_FMT may change width and height. */
	if (p->width != fmt.fmt.pix.width)
	{
		p->width = fmt.fmt.pix.width;
		fprintf(stderr,"Image width set to %i by device %s.\n", p->width, deviceName);
	}
	if (p->height != fmt.fmt.pix.height)
	{
		p->height = fmt.fmt.pix.height;
		fprintf(stderr,"Image height set to %i by device %s.\n", p->height, deviceName);
	}

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (p->io)
	{
		case IO_METHOD_READ:
			readInit(fmt. fmt.pix.sizeimage, p);
			break;
		case IO_METHOD_MMAP:
			mmapInit(p);
			break;
		case IO_METHOD_USERPTR:
			userptrInit(fmt. fmt.pix.sizeimage, p);
			break;
	}

	p->playerwidth = p->width * p->scale;
	p->playerheight = p->height * 3/2 * p->scale;

	pthread_mutex_lock(p->sizemutex);
	p->framesizeready = 1;
	pthread_cond_broadcast(p->sizeready); // wake up all threads
	pthread_mutex_unlock(p->sizemutex);
}

/**
  close device
*/
void deviceClose(v4l2params *p)
{
	if (-1 == close(p->fd))
		errno_exit("close");

	p->fd = -1;
}

/**
  open device
*/
void deviceOpen(v4l2params *p)
{
	struct stat st;
	char *deviceName = p->devicename;

	// stat file
	if (-1 == stat(deviceName, &st))
	{
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", deviceName, errno, strerror (errno));
		exit(EXIT_FAILURE);
	}

	// check if its device
	if (!S_ISCHR (st.st_mode))
	{
		fprintf(stderr, "%s is no device\n", deviceName);
		exit(EXIT_FAILURE);
	}

	// open device
	p->fd = open(deviceName, O_RDWR /* required */ | O_NONBLOCK, 0);

	// check if opening was successfull
	if (-1 == p->fd)
	{
		fprintf(stderr, "Cannot open '%s': %d, %s\n", deviceName, errno, strerror (errno));
		exit(EXIT_FAILURE);
	}
}
