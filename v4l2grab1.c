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
 *   Modified 2020, İlker Çakır                                            *
 *   Added YUYV -> YUV420 GPU accelerated transcoder                       *
 *   Added ffmpeg encoder                                                  *
 ***************************************************************************/

// compile with gcc -Wall -c "%f" -DUSE_OPENGL -DUSE_EGL -DIS_RPI -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -g -ftree-vectorize -pipe -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -mcpu=cortex-a53 -mfloat-abi=hard -mfpu=neon-fp-armv8 -mneon-for-64bits -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -Wno-deprecated-declarations $(pkg-config --cflags gtk+-3.0)
// link with gcc -Wall -o "%e" "%f" YUYVYUV420gl.o v4l2.o VideoQueue.o encode.o -D_POSIX_C_SOURCE=199309L $(pkg-config --cflags gtk+-3.0) -Wl,--whole-archive -I/opt/vc/include -L/opt/vc/lib/ -lGLESv2 -lEGL -lbcm_host -lvchiq_arm -lpthread -lrt -ldl -lm -Wl,--no-whole-archive -rdynamic $(pkg-config --libs gtk+-3.0) $(pkg-config --cflags gtk+-3.0) $(pkg-config --libs gtk+-3.0) $(pkg-config --libs libavcodec libavformat libavutil libswscale) -ljpeg -lm

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
#include "encode.h"
#include "VideoQueue.h"

//static unsigned char jpegQuality = 70;
//static char* jpegFilename = NULL;

typedef struct
{
	vqstatus vqcapture;
	vqstatus vqencode;
	v4l2params p;
}queues;
queues q2;

CUBE_STATE_T state, *p_state = &state;

pthread_t tid[4];
int retval_wait, retval_capture, retval_play, retval_encode;
cpu_set_t cpu[4];

GtkWidget *window;
GtkWidget *capturebox;
GtkWidget *vcapturebox;
GtkWidget *imagebox;
GtkWidget *dwgarea;
GdkPixbuf *pixbuf;
GMutex pixbufmutex;
GtkWidget *window2;
GtkWidget *controlbox;
GtkWidget *buttonbox1;
GtkWidget *buttonbox2;
GtkWidget *videodev;
GtkWidget *bitrate;
GtkWidget *scale;
GtkWidget *iomethod;
GtkWidget *button1;
GtkWidget *button2;
GtkWidget *hbox;
GtkWidget *glcheckbox;
GtkWidget *buttonbox3;
GtkWidget *button3;

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

gboolean resizewindow(gpointer data)
{
	queues *q = (queues *)data;

	gtk_widget_set_size_request(dwgarea, q->p.playerwidth, q->p.playerheight);
	gtk_window_resize(GTK_WINDOW(window), 1, 1);
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

// Write image to jpeg file.
/*
static void jpegWrite(unsigned char* img)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
	
  JSAMPROW row_pointer[1];
  FILE *outfile = fopen( jpegFilename, "wb" );

  // try to open file for saving
  if (!outfile) {
    errno_exit("jpeg");
  }

  // create jpeg data
  cinfo.err = jpeg_std_error( &jerr );
  jpeg_create_compress(&cinfo);
  jpeg_stdio_dest(&cinfo, outfile);

  // set image parameters
  cinfo.image_width = width;	
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;

  // set jpeg compression parameters to default
  jpeg_set_defaults(&cinfo);
  // and then adjust quality setting
  jpeg_set_quality(&cinfo, jpegQuality, TRUE);

  // start compress 
  jpeg_start_compress(&cinfo, TRUE);

  // feed data
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &img[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
    jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  // finish compression
  jpeg_finish_compress(&cinfo);

  // destroy jpeg data
  jpeg_destroy_compress(&cinfo);

  // close output file
  fclose(outfile);
}
*/

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

	pthread_mutex_lock(q->p.sizemutex);
	while (!q->p.framesizeready)
	{
		pthread_cond_wait(q->p.sizeready, q->p.sizemutex);
	}
	pthread_mutex_unlock(q->p.sizemutex);

	gdk_threads_add_idle(resizewindow, (void *)q);

	if (useGL)
	{
		bcm_host_init();
		init_ogl2(p_state, q->p.playerwidth, q->p.playerheight); // Render to Frame Buffer
		if (Init(p_state) == GL_FALSE)
		{
			printf("Init\n");
			exit(0);
		}
		userData = p_state->user_data;
		setSize(p_state, q->p.width/2, q->p.height);

		initPixbuf(p_state->screen_width, p_state->screen_height);
		userData->outrgb = (unsigned char*)gdk_pixbuf_get_pixels(pixbuf);

		GLfloat picSize[2] = { (GLfloat)q->p.width, (GLfloat)q->p.height };
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
			texImage2D(p_state, p->yuyv, q->p.width/2, q->p.height);
			redraw_scene(p_state);
			g_mutex_lock(&pixbufmutex);
			//glReadPixels(0, 0, p_state->screen_width, p_state->screen_height*4/3, GL_ALPHA, GL_UNSIGNED_BYTE, userData->outrgb);
			glReadPixels(0, 0, p_state->screen_width, p_state->screen_height, GL_RGBA, GL_UNSIGNED_BYTE, userData->outrgb);
			//checkNoGLES2Error();
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

			// 32 -> 8
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
			YUV422toRGB8888(q->p.width, q->p.height, src, userData->outrgb);
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

void* captureThread(void *arg)
{
	queues *q = (queues *)arg;
	vqstatus *vqs = &(q->vqcapture);

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
	deviceOpen(&(q->p));
	deviceInit(&(q->p));

	// start capturing
	captureStart(&(q->p));

	// process frames
	mainLoop(vqs, &running, &(q->p));

	// stop capturing
	captureStop(&(q->p));

	// close device
	deviceUninit(&(q->p));
	deviceClose(&(q->p));

	retval_capture = 0;
	pthread_exit((void*)&retval_capture);
}

void* encodeThread(void *arg)
{
	queues *q = (queues *)arg;
	vqstatus *vqe = &(q->vqencode);
	struct videoqueue *p;
	videoencoder enc;
	long long int pts = 0;

	pthread_mutex_lock(q->p.sizemutex);
	while (!q->p.framesizeready)
	{
		pthread_cond_wait(q->p.sizeready, q->p.sizemutex);
	}
	pthread_mutex_unlock(q->p.sizemutex);

	init_encoder(&enc, q->p.filename, q->p.bitrate, q->p.width * q->p.scale, q->p.height * q->p.scale);

	while (1)
	{
		if ((p = vq_remove(vqe)) == NULL)
			break;

//printf("encode\n");
		encode(&enc, p->yuyv, ++pts);

		free(p->yuyv);
		free(p);
	}

	close_encoder(&enc);

	retval_encode = 0;
	pthread_exit((void*)&retval_encode);
}

void* waitThread(void *arg)
{
	queues *q = (queues *)arg;
	v4l2params *p = &(q->p);
	
	// Init Video Queue
	queues_init(q);
	init_v4l2params(p);

	int err;

	err = pthread_create(&(tid[1]), NULL, &captureThread, (void *)q);
	if (err)
	{}
	CPU_ZERO(&(cpu[1]));
	CPU_SET(1, &(cpu[1]));
	err = pthread_setaffinity_np(tid[1], sizeof(cpu_set_t), &(cpu[1]));
	if (err)
	{}

	err = pthread_create(&(tid[2]), NULL, &playThread, (void *)q);
	if (err)
	{}
	CPU_ZERO(&(cpu[2]));
	CPU_SET(2, &(cpu[2]));
	err = pthread_setaffinity_np(tid[2], sizeof(cpu_set_t), &(cpu[2]));
	if (err)
	{}

	err = pthread_create(&(tid[3]), NULL, &encodeThread, (void *)q);
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

	vq_drain(&(q->vqcapture));

	if ((i=pthread_join(tid[2], NULL)))
		printf("pthread_join error, tid[2], %d\n", i);

	vq_drain(&(q->vqencode));

	if ((i=pthread_join(tid[3], NULL)))
		printf("pthread_join error, tid[3], %d\n", i);

	close_v4l2params(p);

	retval_wait = 0;
	pthread_exit((void*)&retval_wait);
}

void create_threads(queues *q)
{
	int err;
	err = pthread_create(&(tid[0]), NULL, &waitThread, (void *)q);
	if (err)
	{}
}

void terminate_threads()
{
	int i;
	if ((i=pthread_join(tid[0], NULL)))
		printf("pthread_join error, tid[0], %d\n", i);
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
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

void destroy(GtkWidget *widget, gpointer data)
{
//printf("gtk_main_quit\n");
    gtk_main_quit();
}

void realize_cb(GtkWidget *widget, gpointer data)
{
}

gboolean draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data)
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

gboolean delete_event2(GtkWidget *widget, GdkEvent *event, gpointer data)
{
//g_print ("delete event occurred\n");
    return TRUE; // return FALSE to emit destroy signal
}

void destroy2(GtkWidget *widget, gpointer data)
{
//printf("gtk_main_quit\n");
}

void realize_cb2(GtkWidget *widget, gpointer data)
{
}

void videodev_changed(GtkWidget *combo, gpointer data)
{
	queues *q = (queues *)data;

	gchar *device;
	g_object_get((gpointer)combo, "active-id", &device, NULL);
//printf("%s\n", device);
	strcpy(q->p.devicename, device);
	g_free(device);
}

void bitrate_changed(GtkWidget *combo, gpointer data)
{
	queues *q = (queues *)data;

	gchar *bitrate;
	g_object_get((gpointer)combo, "active-id", &bitrate, NULL);
//printf("%s\n", bitrate);
	q->p.bitrate = atoi(bitrate);
	g_free(bitrate);
}

void scale_changed(GtkWidget *combo, gpointer data)
{
	queues *q = (queues *)data;

	gchar *scaleval;
	g_object_get((gpointer)combo, "active-id", &scaleval, NULL);
	q->p.scale = atof(scaleval);
//printf("%5.2f\n", q->p.scale);
	g_free(scaleval);
}

void iomethod_changed(GtkWidget *combo, gpointer data)
{
	queues *q = (queues *)data;

	gchar *ioval;
	g_object_get((gpointer)combo, "active-id", &ioval, NULL);
	q->p.io = atoi(ioval);
//printf("%d\n", q->p.io);
	g_free(ioval);
}

void button1_clicked(GtkWidget *button, gpointer data)
{
	queues *q = (queues *)data;

	gtk_widget_set_sensitive(glcheckbox, FALSE);
	gtk_widget_set_sensitive(button1, FALSE);
	gtk_widget_set_sensitive(button2, TRUE);
	running = 1;
	create_threads(q);
}

void button2_clicked(GtkWidget *button, gpointer data)
{
	running = 0;
	terminate_threads();
	gtk_widget_set_sensitive(glcheckbox, TRUE);
	gtk_widget_set_sensitive(button1, TRUE);
	gtk_widget_set_sensitive(button2, FALSE);
}

void usegl_toggled(GtkWidget *togglebutton, gpointer data)
{
	useGL = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));
	//printf("toggle state %d\n", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlyenable)));
}

void button3_clicked(GtkWidget *button, gpointer data)
{
	queues *q = (queues *)data;

	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	GSList *chosenfile;

	dialog = gtk_file_chooser_dialog_new("Save File", GTK_WINDOW(window), action, "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
	gtk_file_chooser_set_select_multiple(chooser, FALSE);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		GSList *filelist = gtk_file_chooser_get_filenames(chooser);
		for(chosenfile=filelist;chosenfile;chosenfile=chosenfile->next)
		{
			strcpy(q->p.filename, (char*)chosenfile->data);
		}
		g_slist_free(filelist);
	}
	gtk_widget_destroy(dialog);
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
	q2.p.playerwidth = 640;
	q2.p.playerheight = 360; // dummy size

	setup_default_icon("./v4l2.png");

	/* This is called in all GTK applications. Arguments are parsed
	* from the command line and are returned to the application. */
	gtk_init(&argc, &argv);
    
	/* create a new window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	/* Sets the border width of the window. */
	gtk_container_set_border_width(GTK_CONTAINER(window), 2);
	//gtk_widget_set_size_request(window, 100, 100);
	gtk_window_set_title(GTK_WINDOW(window), "Video Capture");
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

	/* When the window is given the "delete-event" signal (this is given
	* by the window manager, usually by the "close" option, or on the
	* titlebar), we ask it to call the delete_event () function
	* as defined above. The data passed to the callback
	* function is NULL and is ignored in the callback function. */
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), (void *)&q2);
    
	/* Here we connect the "destroy" event to a signal handler.  
	* This event occurs when we call gtk_widget_destroy() on the window,
	* or if we return FALSE in the "delete-event" callback. */
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);

	g_signal_connect(window, "realize", G_CALLBACK(realize_cb), NULL);

// vertical box
	vcapturebox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(window), vcapturebox);

// vertical box
	capturebox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	//gtk_box_pack_start(GTK_BOX(vcapturebox), capturebox, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(vcapturebox), capturebox);

// horizontal box
	imagebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	//gtk_box_pack_start(GTK_BOX(capturebox), imagebox, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(capturebox), imagebox);

// drawing area
	dwgarea = gtk_drawing_area_new();
	gtk_widget_set_size_request(dwgarea, q2.p.playerwidth, q2.p.playerheight);
	//gtk_box_pack_start(GTK_BOX(imagebox), dwgarea, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(imagebox), dwgarea);

	// Signals used to handle the backing surface
	g_signal_connect(dwgarea, "draw", G_CALLBACK(draw_cb), NULL);
	gtk_widget_set_app_paintable(dwgarea, TRUE);

	pixbuf = NULL;
	initPixbuf(q2.p.playerwidth, q2.p.playerheight);

	gtk_widget_show_all(window);


	/* create a new window */
	window2 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window2), GTK_WIN_POS_CENTER);
	/* Sets the border width of the window. */
	gtk_container_set_border_width(GTK_CONTAINER(window2), 2);
	//gtk_widget_set_size_request(window, 100, 100);
	gtk_window_set_title(GTK_WINDOW(window2), "Video Capture Control");
	gtk_window_set_resizable(GTK_WINDOW(window2), FALSE);

	/* When the window is given the "delete-event" signal (this is given
	* by the window manager, usually by the "close" option, or on the
	* titlebar), we ask it to call the delete_event () function
	* as defined above. The data passed to the callback
	* function is NULL and is ignored in the callback function. */
	g_signal_connect(window2, "delete-event", G_CALLBACK(delete_event2), NULL);
    
	/* Here we connect the "destroy" event to a signal handler.  
	* This event occurs when we call gtk_widget_destroy() on the window,
	* or if we return FALSE in the "delete-event" callback. */
	g_signal_connect(window2, "destroy", G_CALLBACK(destroy2), NULL);

	g_signal_connect(window2, "realize", G_CALLBACK(realize_cb2), NULL);

// vertical box
	controlbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	//gtk_box_pack_start(GTK_BOX(vcapturebox), capturebox, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(window2), controlbox);

// horizontal button box
	buttonbox1 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout((GtkButtonBox *)buttonbox1, GTK_BUTTONBOX_START);
	gtk_container_add(GTK_CONTAINER(controlbox), buttonbox1);

// video devices
	videodev = gtk_combo_box_text_new();
	enumerate_video_devices(videodev);
	gtk_combo_box_set_active((gpointer)videodev, 0);
	g_signal_connect(GTK_COMBO_BOX(videodev), "changed", G_CALLBACK(videodev_changed), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(buttonbox1), videodev);
	gtk_button_box_set_child_non_homogeneous((GtkButtonBox *)buttonbox1, videodev, TRUE);
	videodev_changed(videodev, (void *)&q2);

// bit rate
	bitrate = gtk_combo_box_text_new();
	enumerate_bitrates(bitrate);
	gtk_combo_box_set_active((gpointer)bitrate, 0);
	g_signal_connect(GTK_COMBO_BOX(bitrate), "changed", G_CALLBACK(bitrate_changed), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(buttonbox1), bitrate);
	gtk_button_box_set_child_non_homogeneous((GtkButtonBox *)buttonbox1, bitrate, TRUE);
	bitrate_changed(bitrate, (void *)&q2);

// horizontal button box
	buttonbox2 = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout((GtkButtonBox *)buttonbox2, GTK_BUTTONBOX_START);
	gtk_container_add(GTK_CONTAINER(controlbox), buttonbox2);

// button capture
	button1 = gtk_button_new_with_label("Start");
	g_signal_connect(GTK_BUTTON(button1), "clicked", G_CALLBACK(button1_clicked), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(buttonbox2), button1);

// button stop
	button2 = gtk_button_new_with_label("Stop");
	g_signal_connect(GTK_BUTTON(button2), "clicked", G_CALLBACK(button2_clicked), NULL);
	gtk_container_add(GTK_CONTAINER(buttonbox2), button2);
	gtk_widget_set_sensitive(button2, FALSE);

// scale
	scale = gtk_combo_box_text_new();
	enumerate_scale_values(scale);
	gtk_combo_box_set_active((gpointer)scale, 2);
	g_signal_connect(GTK_COMBO_BOX(scale), "changed", G_CALLBACK(scale_changed), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(buttonbox2), scale);
	scale_changed(scale, (void *)&q2);

// horizontal button box
	buttonbox3 = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout((GtkButtonBox *)buttonbox3, GTK_BUTTONBOX_START);
	gtk_container_add(GTK_CONTAINER(controlbox), buttonbox3);

// button browse
	button3 = gtk_button_new_with_label("Browse");
	g_signal_connect(GTK_BUTTON(button3), "clicked", G_CALLBACK(button3_clicked), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(buttonbox3), button3);
	strcpy(q2.p.filename, "/media/pi/Ilker/v4l2grab1.mp4");

// horizontal box
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(controlbox), hbox);

// io methods
	iomethod = gtk_combo_box_text_new();
	enumerate_io_methods(iomethod);
	gtk_combo_box_set_active((gpointer)iomethod, 0);
	g_signal_connect(GTK_COMBO_BOX(iomethod), "changed", G_CALLBACK(iomethod_changed), (void *)&q2);
	gtk_container_add(GTK_CONTAINER(hbox), iomethod);
	iomethod_changed(iomethod, (void *)&q2);

// checkbox
	useGL = TRUE;
	glcheckbox = gtk_check_button_new_with_label("Use GL");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glcheckbox), useGL);
	g_signal_connect(GTK_TOGGLE_BUTTON(glcheckbox), "toggled", G_CALLBACK(usegl_toggled), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), glcheckbox);

	gtk_widget_show_all(window2);

	gtk_main();

	return 0;
}
