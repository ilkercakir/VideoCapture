/***************************************************************************
 *   v4l2grab Version 0.1                                                  *
 *   Copyright (C) 2009 by Tobias Müller                                   *
 *   Tobias_Mueller@twam.info                                              *
 *                                                                         *
 *   based on V4L2 Specification, Appendix B: Video Capture Example        *
 *   (http://v4l2spec.bytesex.org/spec/capture-example.html)               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
/***************************************************************************
 *   Modified 2017, İlker Çakır                                            *
 *   Added GPU support using open GLES 2.0                                 *
 ***************************************************************************/

// compile with gcc -Wall -c "%f" -DUSE_OPENGL -DUSE_EGL -DIS_RPI -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -g -ftree-vectorize -pipe -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -mcpu=cortex-a53 -mfloat-abi=hard -mfpu=neon-fp-armv8 -mneon-for-64bits -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -Wno-deprecated-declarations -DIO_USERPTR $(pkg-config --cflags gtk+-3.0)
// link with gcc -Wall -o "%e" "%f" -D_POSIX_C_SOURCE=199309L -DIO_READ -DIO_MMAP -DIO_USERPTR $(pkg-config --cflags gtk+-3.0) -Wl,--whole-archive -I/opt/vc/include -L/opt/vc/lib/ -lGLESv2 -lEGL -lbcm_host -lvchiq_arm -lpthread -lrt -ldl -lm -Wl,--no-whole-archive -rdynamic $(pkg-config --libs gtk+-3.0) $(pkg-config --cflags gtk+-3.0) $(pkg-config --libs gtk+-3.0) -ljpeg -lm

#define _GNU_SOURCE

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
#include <linux/videodev2.h>
#include <jpeglib.h>

#include <math.h>
#include <pthread.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "YUYVYUV420gl.h"
#include "v4l2.h"

// global settings
int playerwidth = 720;
int playerheight = 576 * 3/2;
CUBE_STATE_T state, *p_state = &state;

// video
unsigned int width = 720;
unsigned int height = 576;
//static unsigned char jpegQuality = 70;
//static char* jpegFilename = NULL;
static char* deviceName = "/dev/video0";

typedef struct
{
	vqstatus vqcapture;
	vqstatus vqencode;
}queues;
queues q2;

pthread_t tid[4];
int retval_wait, retval_capture, retval_play, retval_encode;
cpu_set_t cpu[4];

GtkWidget *window;
GtkWidget *capturebox;
GtkWidget *imagebox;
GtkWidget *dwgarea;
GdkPixbuf *pixbuf;
GMutex pixbufmutex;
GtkWidget *buttonbox1;
GtkWidget *button1;
GtkWidget *button2;
GtkWidget *hbox;
GtkWidget *glcheckbox;

int running = 0;
int useGL = 1;

void queues_init(queues *q)
{
	vqs_init(&(q->vqcapture));
	q->vqcapture.playerstatus = PLAYING;

	vqs_init(&(q->vqencode));
	q->vqencode.playerstatus = PLAYING;
}

gboolean invalidate(gpointer data)
{
	GdkWindow *dawin = gtk_widget_get_window(dwgarea);
	cairo_region_t *region = gdk_window_get_clip_region(dawin);
	gdk_window_invalidate_region (dawin, region, TRUE);
	//gdk_window_process_updates (dawin, TRUE);
	cairo_region_destroy(region);
	return FALSE;
}

void destroynotify(guchar *pixels, gpointer data)
{
//printf("destroy notify\n");
	free(pixels);
}

void initPixbuf(int width, int height)
{
	int i;

	guchar *imgdata = malloc(width*height*4); // RGBA
	for(i=0;i<width*height;i++)
	{
		((unsigned int *)imgdata)[i]=0xFF000000; // ABGR
	}
	g_mutex_lock(&pixbufmutex);
	if (pixbuf)
		g_object_unref(pixbuf);
    pixbuf = gdk_pixbuf_new_from_data(imgdata, GDK_COLORSPACE_RGB, TRUE, 8, width, height, width*4, destroynotify, NULL);
	g_mutex_unlock(&pixbufmutex);
	gdk_threads_add_idle(invalidate, NULL);
}

/**
  Convert from YUV422 format to RGB888. Formulae are described on http://en.wikipedia.org/wiki/YUV

  \param width width of image
  \param height height of image
  \param src source
  \param dst destination
*/
static void YUV422toRGB8888(int width, int height, unsigned char *src, unsigned char *dst)
{
  int line, column;
  unsigned char *py, *pu, *pv;
  unsigned char *tmp = dst;

  /* In this format each four bytes is two pixels. Each four bytes is two Y's, a Cb and a Cr. 
     Each Y goes to one of the pixels, and the Cb and Cr belong to both pixels. */
  py = src;
  pu = src + 1;
  pv = src + 3;

  #define swCLIP(x) ( (x)>=0xFF ? 0xFF : ( (x) <= 0x00 ? 0x00 : (x) ) )

  for (line = 0; line < height; ++line) {
    for (column = 0; column < width; ++column) {
      *tmp++ = swCLIP((double)*py + 1.402*((double)*pv-128.0));
      *tmp++ = swCLIP((double)*py - 0.344*((double)*pu-128.0) - 0.714*((double)*pv-128.0));      
      *tmp++ = swCLIP((double)*py + 1.772*((double)*pu-128.0));
      *tmp++ = 0xFF; // A

      // increase py every time
      py += 2;
      // increase pu,pv every second time
      if ((column & 1)==1) {
        pu += 4;
        pv += 4;
      }
    }
  }
}

void* playThread(void *arg)
{
	queues *q = (queues *)arg;
	vqstatus *vqs = &(q->vqcapture);
	vqstatus *vqe = &(q->vqencode);
	struct videoqueue *p;
	UserData *userData;
	unsigned char *src;

	int ctype = PTHREAD_CANCEL_ASYNCHRONOUS;
	int ctype_old;
	pthread_setcanceltype(ctype, &ctype_old);

	if (useGL)
	{
		bcm_host_init();
		init_ogl2(p_state, playerwidth, playerheight); // Render to Frame Buffer
		if (Init(p_state) == GL_FALSE)
		{
			printf("Init\n");
			exit(0);
		}
		userData = p_state->user_data;
		setSize(p_state, width/2, height);

		initPixbuf(p_state->screen_width, p_state->screen_height);
		userData->outrgb = (unsigned char*)gdk_pixbuf_get_pixels(pixbuf);

		GLfloat picSize[2] = { (GLfloat)width, (GLfloat)height };
		glUniform2fv(userData->sizeLoc, 1, picSize);
	}
	else
	{
		p_state->user_data = malloc(sizeof(UserData));
		userData = p_state->user_data;
		userData->outrgb = (unsigned char*)gdk_pixbuf_get_pixels(pixbuf);
	}

	while (1)
	{
		if ((p = vq_remove(vqs)) == NULL)
			break;
		src = p->yuyv;

		if (useGL)
		{
			texImage2D(p_state, p->yuyv, width/2, height);
			redraw_scene(p_state);
			g_mutex_lock(&pixbufmutex);
			//glReadPixels(0, 0, p_state->screen_width, p_state->screen_height*4/3, GL_ALPHA, GL_UNSIGNED_BYTE, userData->outrgb);
			glReadPixels(0, 0, p_state->screen_width, p_state->screen_height, GL_RGBA, GL_UNSIGNED_BYTE, userData->outrgb);
			checkNoGLES2Error();
//printf("wh %d %d\n", p_state->screen_width, p_state->screen_height);
/*
			//stub
			int framecount = 0;
			FILE *f;
			if (++framecount == 100)
			{
			const int yuvsize = width*height*3/2;
			int i;
			unsigned char yuv[yuvsize];
			for (i=0;i<yuvsize;i++)
				yuv[i] = ((int32_t*)userData->outrgb)[i];
			f=fopen("image.yuv420", "wb");
			fwrite(yuv,1,yuvsize,f);
			fclose(f);
			}
*/
			g_mutex_unlock(&pixbufmutex);
			gdk_threads_add_idle(invalidate, NULL);

			//stub
			uint32_t *rgba = (uint32_t *)userData->outrgb;
			uint8_t *i8 = malloc(p_state->screen_width * p_state->screen_height);
			int i = p_state->screen_width*p_state->screen_height;
			do
			{
				i--;
				i8[i] = rgba[i];
			}while (i);
			vq_add(vqe, i8);

		}
		else
		{
			g_mutex_lock(&pixbufmutex);
//printf("convert from YUV422 to RGB888\n");
			YUV422toRGB8888(width, height, src, userData->outrgb);
//printf("YUV422->RGB conversion done\n");
			g_mutex_unlock(&pixbufmutex);
			gdk_threads_add_idle(invalidate, NULL);
		}
		free(p->yuyv);
		free(p);

  // write jpeg
//  jpegWrite(dst);
	}
	
	if (useGL)
		exit_func(p_state);
	else
		free(userData);

	retval_play = 0;
	pthread_exit((void*)&retval_play);
}



// print usage information
/*
static void usage(FILE* fp, int argc, char** argv)
{
  fprintf (fp,
    "Usage: %s [options]\n\n"
    "Options:\n"
    "-d | --device name   Video device name [/dev/video0]\n"
    "-h | --help          Print this message\n"
    "-o | --output        JPEG output filename\n"
    "-q | --quality       JPEG quality (0-100)\n"
    "-m | --mmap          Use memory mapped buffers\n"
    "-r | --read          Use read() calls\n"
    "-u | --userptr       Use application allocated buffers\n"
    "-W | --width         width\n"
    "-H | --height        height\n"
    "",
    argv[0]);
}

static const char short_options [] = "d:ho:q:mruW:H:";

static const struct option
long_options [] = {
        { "device",     required_argument,      NULL,           'd' },
        { "help",       no_argument,            NULL,           'h' },
        { "output",     required_argument,      NULL,           'o' },
        { "quality",    required_argument,      NULL,           'q' },
        { "mmap",       no_argument,            NULL,           'm' },
        { "read",       no_argument,            NULL,           'r' },
        { "userptr",    no_argument,            NULL,           'u' },
        { "width",      required_argument,      NULL,           'W' },
        { "height",     required_argument,      NULL,           'H' },
        { 0, 0, 0, 0 }
};
*/

void* captureThread(void *arg)
{
	queues *q = (queues *)arg;
	vqstatus *vqs = &(q->vqcapture);
	v4l2params p;
	
	init_v4l2params(&p, IO_METHOD_USERPTR);
/*
  for (;;) {
    int index, c = 0;
                
    c = getopt_long(argc, argv, short_options, long_options, &index);

    if (-1 == c)
      break;

    switch (c) {
      case 0: // getopt_long() flag
        break;

      case 'd':
        deviceName = optarg;
        break;

      case 'h':
        // print help
        usage (stdout, argc, argv);
        exit(EXIT_SUCCESS);

      case 'o':
        // set jpeg filename
        jpegFilename = optarg;
        break;

      case 'q':
        // set jpeg filename
        jpegQuality = atoi(optarg);
        break;

      case 'm':
#ifdef IO_MMAP
        io = IO_METHOD_MMAP;
#else
        fprintf(stderr, "You didn't compile for mmap support.\n");
        exit(EXIT_FAILURE);         
#endif
        break;

      case 'r':
#ifdef IO_READ
        io = IO_METHOD_READ;
#else
        fprintf(stderr, "You didn't compile for read support.\n");
        exit(EXIT_FAILURE);         
#endif
        break;

      case 'u':
#ifdef IO_USERPTR
        io = IO_METHOD_USERPTR;
#else
        fprintf(stderr, "You didn't compile for userptr support.\n");
        exit(EXIT_FAILURE);         
#endif
        break;

      case 'W':
        // set width
        width = atoi(optarg);
        break;

      case 'H':
        // set height
        height = atoi(optarg);
        break;

      default:
        usage(stderr, argc, argv);
        exit(EXIT_FAILURE);
    }
  }

  // check for need parameters
  if (!jpegFilename) {
        fprintf(stderr, "You have to specify JPEG output filename!\n\n");
        usage(stdout, argc, argv);
        exit(EXIT_FAILURE); 
  }
*/

	// open and initialize device
	deviceOpen(deviceName, &p);
	deviceInit(deviceName, width, height, &p);

	// start capturing
	captureStart(&p);

	// process frames
	mainLoop(width, height, vqs, &running, &p);

	// stop capturing
	captureStop(&p);

	// close device
	deviceUninit(&p);
	deviceClose(&p);

	retval_capture = 0;
	pthread_exit((void*)&retval_capture);
}

void* encodeThread(void *arg)
{
	queues *q = (queues *)arg;
	vqstatus *vqe = &(q->vqencode);
	struct videoqueue *p;

	while (1)
	{
		if ((p = vq_remove(vqe)) == NULL)
			break;
			
printf("encode\n");

		free(p->yuyv);
		free(p);
	}
	retval_encode = 0;
	pthread_exit((void*)&retval_encode);
}

void* waitThread(void *arg)
{
	// Init Video Queue
	queues_init(&q2);

	int err;

	err = pthread_create(&(tid[1]), NULL, &captureThread, (void *)&q2);
	if (err)
	{}
	CPU_ZERO(&(cpu[1]));
	CPU_SET(1, &(cpu[1]));
	err = pthread_setaffinity_np(tid[1], sizeof(cpu_set_t), &(cpu[1]));
	if (err)
	{}

	err = pthread_create(&(tid[2]), NULL, &playThread, (void *)&q2);
	if (err)
	{}
	CPU_ZERO(&(cpu[2]));
	CPU_SET(2, &(cpu[2]));
	err = pthread_setaffinity_np(tid[2], sizeof(cpu_set_t), &(cpu[2]));
	if (err)
	{}

	err = pthread_create(&(tid[3]), NULL, &encodeThread, (void *)&q2);
	if (err)
	{}
	CPU_ZERO(&(cpu[3]));
	CPU_SET(3, &(cpu[3]));
	err = pthread_setaffinity_np(tid[3], sizeof(cpu_set_t), &(cpu[3]));
	if (err)
	{}

	int i;
	if ((i=pthread_join(tid[1], NULL)))
		printf("pthread_join error, tid[1], %d\n", i);

	vq_drain(&(q2.vqcapture));

	if ((i=pthread_join(tid[2], NULL)))
		printf("pthread_join error, tid[2], %d\n", i);

	vq_drain(&(q2.vqencode));

	if ((i=pthread_join(tid[3], NULL)))
		printf("pthread_join error, tid[3], %d\n", i);

	retval_wait = 0;
	pthread_exit((void*)&retval_wait);
}

void create_threads()
{
	int err;
	err = pthread_create(&(tid[0]), NULL, &waitThread, NULL);
	if (err)
	{}
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
//g_print ("delete event occurred\n");
	queues *q = (queues *)data;
	vqstatus *vqs = &(q->vqcapture);

	running = 0;
	while (vqs->playerstatus!=IDLE)
	{
		sleep(1);
	}
    return FALSE; // return FALSE to emit destroy signal
}

static void destroy(GtkWidget *widget, gpointer data)
{
//printf("gtk_main_quit\n");
    gtk_main_quit();
}

static void realize_cb(GtkWidget *widget, gpointer data)
{
}

static gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
{
//printf("Draw %d\n", gettid());
	g_mutex_lock(&pixbufmutex);
	//cr = gdk_cairo_create (gtk_widget_get_window(dwgarea));
	gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
	cairo_paint(cr);
	//cairo_destroy(cr);
	g_mutex_unlock(&pixbufmutex);

	return FALSE;
}

static void button1_clicked(GtkWidget *button, gpointer data)
{
	gtk_widget_set_sensitive(glcheckbox, FALSE);
	gtk_widget_set_sensitive(button1, FALSE);
	gtk_widget_set_sensitive(button2, TRUE);
	running = 1;
	create_threads();
}

static void button2_clicked(GtkWidget *button, gpointer data)
{
	running = 0;
	gtk_widget_set_sensitive(glcheckbox, TRUE);
	gtk_widget_set_sensitive(button1, TRUE);
	gtk_widget_set_sensitive(button2, FALSE);
}

static void usegl_toggled(GtkWidget *togglebutton, gpointer data)
{
	useGL = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));
	//printf("toggle state %d\n", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlyenable)));
}

void setup_default_icon(char *filename)
{
	GdkPixbuf *pixbuf;
	GError *err;

	err = NULL;
	pixbuf = gdk_pixbuf_new_from_file(filename, &err);

	if (pixbuf)
	{
		GList *list;      

		list = NULL;
		list = g_list_append(list, pixbuf);
		gtk_window_set_default_icon_list(list);
		g_list_free(list);
		g_object_unref(pixbuf);
    }
}

int main(int argc, char **argv)
{
	setup_default_icon("./v4l2.png");

	


     /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init (&argc, &argv);
    
    /* create a new window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (window), 2);
	gtk_widget_set_size_request(window, 100, 100);
	gtk_window_set_title(GTK_WINDOW(window), "Video Capture");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    /* When the window is given the "delete-event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    g_signal_connect (window, "delete-event", G_CALLBACK (delete_event), (void *)&q2);
    
    /* Here we connect the "destroy" event to a signal handler.  
     * This event occurs when we call gtk_widget_destroy() on the window,
     * or if we return FALSE in the "delete-event" callback. */
    g_signal_connect (window, "destroy", G_CALLBACK (destroy), NULL);

    g_signal_connect (window, "realize", G_CALLBACK (realize_cb), NULL);
//printf("realized\n");

// vertical box
    capturebox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), capturebox);

// horizontal box
    imagebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(capturebox), imagebox);

// drawing area
    dwgarea = gtk_drawing_area_new();
    gtk_widget_set_size_request (dwgarea, playerwidth, playerheight);
    //gtk_widget_set_size_request (dwgarea, 1280, 720);
    gtk_container_add(GTK_CONTAINER(imagebox), dwgarea);

    // Signals used to handle the backing surface
    g_signal_connect (dwgarea, "draw", G_CALLBACK(draw_cb), NULL);
    gtk_widget_set_app_paintable(dwgarea, TRUE);

// horizontal button box
    buttonbox1 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout((GtkButtonBox *)buttonbox1, GTK_BUTTONBOX_START);
    gtk_container_add(GTK_CONTAINER(capturebox), buttonbox1);

// button capture
    button1 = gtk_button_new_with_label("Start");
    g_signal_connect(GTK_BUTTON(button1), "clicked", G_CALLBACK(button1_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(buttonbox1), button1);

// button stop
    button2 = gtk_button_new_with_label("Stop");
    g_signal_connect(GTK_BUTTON(button2), "clicked", G_CALLBACK(button2_clicked), NULL);
    gtk_container_add(GTK_CONTAINER(buttonbox1), button2);
	gtk_widget_set_sensitive(button2, FALSE);

// horizontal box
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(capturebox), hbox);
    
// checkbox
	useGL = TRUE;
	glcheckbox = gtk_check_button_new_with_label("Use GL");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glcheckbox), useGL);
	g_signal_connect(GTK_TOGGLE_BUTTON(glcheckbox), "toggled", G_CALLBACK(usegl_toggled), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), glcheckbox);

	initPixbuf(playerwidth, playerheight);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
