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

// compile with gcc -Wall -c "%f" -DUSE_OPENGL -DUSE_EGL -DIS_RPI -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -fPIC -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -g -ftree-vectorize -pipe -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -I/opt/vc/include/ -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -Wno-deprecated-declarations -DIO_READ -DIO_MMAP -DIO_USERPTR $(pkg-config --cflags gtk+-3.0)
// link with gcc -Wall -o "%e" "%f" -D_POSIX_C_SOURCE=199309L -DIO_READ -DIO_MMAP -DIO_USERPTR $(pkg-config --cflags gtk+-3.0) -Wl,--whole-archive -I/opt/vc/include -L/opt/vc/lib/ -lGLESv2 -lEGL -lbcm_host -lvchiq_arm -lpthread -lrt -ldl -lm -Wl,--no-whole-archive -rdynamic $(pkg-config --libs gtk+-3.0) $(pkg-config --cflags gtk+-3.0) $(pkg-config --libs gtk+-3.0) -ljpeg -lm

#define _GNU_SOURCE

#if !defined(IO_READ) && !defined(IO_MMAP) && !defined(IO_USERPTR)
#error You have to include one of IO_READ, IO_MMAP oder IO_USERPTR!
#endif

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

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

// global settings
static unsigned int width = 640;
static unsigned int height = 480;
static unsigned char jpegQuality = 70;
static char* jpegFilename = NULL;
static char* deviceName = "/dev/video0";

pthread_t tid[2];
int retval_wait, retval_capture, retval_play;
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

typedef struct
{
    // Handle to a program object
   GLuint programObject;

   // Attribute locations
   GLint  positionLoc;
   GLint  texCoordLoc;

   // Sampler location
   GLint samplerLoc;
   
   // Image size vector location vec2(WIDTH, HEIGHT)
   GLint sizeLoc;

   // Input texture
   GLuint tex;

   // YUV->RGB conversion matrix
   GLuint cmatrixLoc;

	// Frame & Render buffers
	GLuint canvasFrameBuffer;
	GLuint canvasRenderBuffer;

	unsigned char *outrgb;

	// Output texture
	GLuint outtex;
} UserData;

typedef struct
{
    uint32_t screen_width;
    uint32_t screen_height;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

	UserData *user_data;

} CUBE_STATE_T;

CUBE_STATE_T state, *p_state = &state;

GLfloat vVertices[] = { -1.0f,  1.0f, 0.0f, // Position 0
                        -1.0f, -1.0f, 0.0f, // Position 1
                         1.0f, -1.0f, 0.0f, // Position 2
                         1.0f,  1.0f, 0.0f  // Position 3
                      };


/*// Upside down with GTK
GLfloat tVertices[] = {  0.0f,  0.0f,        // TexCoord 0 
                         0.0f,  1.0f,        // TexCoord 1
                         1.0f,  1.0f,        // TexCoord 2
                         1.0f,  0.0f         // TexCoord 3
                      };
*/
GLfloat tVertices[] = {  0.0f,  1.0f,        // TexCoord 0 
                         0.0f,  0.0f,        // TexCoord 1
                         1.0f,  0.0f,        // TexCoord 2
                         1.0f,  1.0f         // TexCoord 3
                      };
                      
GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

void checkNoGLES2Error() 
{
    int error;
    error = glGetError();
    if (error != GL_NO_ERROR)
    {
		printf("GLES20 error: %d", error);
	}
}
  
void init_ogl(CUBE_STATE_T *state)
{
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    static EGL_DISPMANX_WINDOW_T nativewindow;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	    EGL_NONE
	};

    static const EGLint context_attributes[] =
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};

    EGLConfig config;

    // get an EGL display connection
    p_state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // initialize the EGL display connection
    result = eglInitialize(p_state->display, NULL, NULL);

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(p_state->display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);

    // create an EGL rendering context
    p_state->context = eglCreateContext(p_state->display, config, EGL_NO_CONTEXT, context_attributes);
    assert(p_state->context!=EGL_NO_CONTEXT);

    // create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, &p_state->screen_width, &p_state->screen_height);
    assert( success >= 0 );

    printf("Screen size = %d * %d\n", p_state->screen_width, p_state->screen_height);

    p_state->screen_width = width;
    p_state->screen_height = height;

    dst_rect.x = 0;
    dst_rect.y = 0;

    dst_rect.width = p_state->screen_width;
    dst_rect.height = p_state->screen_height;

    src_rect.x = 0;
    src_rect.y = 0;

    src_rect.width = p_state->screen_width << 16;
    src_rect.height = p_state->screen_height << 16;

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_element = 
	vc_dispmanx_element_add(dispman_update, dispman_display,
				0/*layer*/, &dst_rect, 0/*src*/,
				&src_rect, DISPMANX_PROTECTION_NONE, 
				0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

    nativewindow.element = dispman_element;
    nativewindow.width = p_state->screen_width;
    nativewindow.height = p_state->screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );

    state->surface = eglCreateWindowSurface( p_state->display, config, &nativewindow, NULL );
    assert(p_state->surface != EGL_NO_SURFACE);

    // connect the context to the surface
    result = eglMakeCurrent(p_state->display, p_state->surface, p_state->surface, p_state->context);
    assert(EGL_FALSE != result);
}

void init_ogl2(CUBE_STATE_T *state)
{
    EGLBoolean result;
    EGLint num_config;

/*
    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 0,
	    EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
	    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT | EGL_OPENVG_BIT,
	    EGL_NONE
	};
*/
    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 0,
	    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	    EGL_NONE
	};

    static const EGLint context_attributes[] =
	{
	    EGL_CONTEXT_CLIENT_VERSION, 2,
	    EGL_NONE
	};

    EGLConfig config;

    // get an EGL display connection
    p_state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // initialize the EGL display connection
    result = eglInitialize(p_state->display, NULL, NULL);

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(p_state->display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);

    // create an EGL rendering context
    p_state->context = eglCreateContext(p_state->display, config, EGL_NO_CONTEXT, context_attributes);
    assert(p_state->context!=EGL_NO_CONTEXT);

    // create an EGL window surface
    p_state->screen_width = width; //playerwidth
    p_state->screen_height = height; //playerheight

/*
    state->surface = eglCreateWindowSurface( p_state->display, config, NULL, NULL);
    assert(p_state->surface != EGL_NO_SURFACE);
*/
    EGLint pbuffer_attributes[] = 
    {
		EGL_WIDTH, p_state->screen_width,
		EGL_HEIGHT, p_state->screen_height,
		EGL_NONE
    };
    p_state->surface = eglCreatePbufferSurface(p_state->display, config, pbuffer_attributes);
    assert (p_state->surface != EGL_NO_SURFACE);

    // connect the context to the surface
    result = eglMakeCurrent(p_state->display, p_state->surface, p_state->surface, p_state->context);
    assert(EGL_FALSE != result);
}

void exit_func(void) // Function to be passed to atexit().
{
	UserData *userData = p_state->user_data;

/*
// unbind frame buffer
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteRenderbuffers(1, &(userData->canvasRenderBuffer));
	glDeleteFramebuffers(1, &(userData->canvasFrameBuffer));
*/

	// Delete allocated objects
	glDeleteProgram(userData->programObject);
	glDeleteTextures(1, &userData->tex);

	// Release OpenGL resources
	eglMakeCurrent(p_state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(p_state->display, p_state->surface);
	eglDestroyContext(p_state->display, p_state->context);
	eglTerminate(p_state->display);
}

// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader(GLenum type, const char *shaderSrc)
{
    GLuint shader;
    GLint compiled;
    // Create the shader object
    shader = glCreateShader(type);
    if(shader == 0)
    return 0;
    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, NULL);
    // Compile the shader
    glCompileShader(shader);
    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if(!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if(infoLen > 1)
        {
            char* infoLog = malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            fprintf(stderr, "Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LoadProgram ( const char *vertShaderSrc, const char *fragShaderSrc )
{
   GLuint vertexShader;
   GLuint fragmentShader;
   GLuint programObject;
   GLint linked;

   // Load the vertex/fragment shaders
   vertexShader = LoadShader ( GL_VERTEX_SHADER, vertShaderSrc );
   if ( vertexShader == 0 )
      return 0;

   fragmentShader = LoadShader ( GL_FRAGMENT_SHADER, fragShaderSrc );
   if ( fragmentShader == 0 )
   {
      glDeleteShader( vertexShader );
      return 0;
   }

   // Create the program object
   programObject = glCreateProgram ( );

   if ( programObject == 0 )
      return 0;

   glAttachShader ( programObject, vertexShader );
   glAttachShader ( programObject, fragmentShader );

   // Link the program
   glLinkProgram ( programObject );

   // Check the link status
   glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );

   if ( !linked ) 
   {
      GLint infoLen = 0;
      glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );

      if ( infoLen > 1 )
      {
         char* infoLog = malloc (sizeof(char) * infoLen );

         glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
         fprintf (stderr, "Error linking program:\n%s\n", infoLog );            

         free(infoLog);
      }

      glDeleteProgram ( programObject );
      return 0;
   }

   // Free up no longer needed shader resources
   glDeleteShader ( vertexShader );
   glDeleteShader ( fragmentShader );

   return programObject;
}

//
// Initialize the shader and program object
//
int Init(CUBE_STATE_T *p_state)
{
   p_state->user_data = malloc(sizeof(UserData));      
   UserData *userData = p_state->user_data;

   GLbyte vShaderStr[] =  
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

/*
   GLbyte fShaderStr_stpq[] =  
      "precision mediump float;                              \n"
      "varying vec2 v_texCoord;                              \n"
      "uniform vec2 texSize;                                 \n"
      "uniform mat3 yuv2rgb;                                 \n"
      "uniform sampler2D texture;                            \n"
      "void main()                                           \n"
      "{                                                     \n"
      "  float y, u, v, r, g, b;                             \n"
      "  float yx, yy, ux, uy, vx, vy;                       \n"

      "  vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y);\n"
      "  float oe=floor(mod(texCoord.y/2.0, 2.0));           \n"

      "  yx=v_texCoord.x;                                    \n"
      "  yy=v_texCoord.y*2.0/3.0;                            \n"
      "  ux=oe*0.5+v_texCoord.x/2.0;                         \n"
      "  uy=4.0/6.0+v_texCoord.y/6.0;                        \n"
      "  vx=ux;                                              \n"
      "  vy=5.0/6.0+v_texCoord.y/6.0;                        \n"

      "  int x=int(mod(texCoord.x, 8.0));                    \n"

      "  vec4 y4 = vec4(float((x==0)||(x==4)), float((x==1)||(x==5)), float((x==2)||(x==6)), float((x==3)||(x==7))); \n"
      "  vec4 uv4 = vec4(float((x==0)||(x==1)), float((x==2)||(x==3)), float((x==4)||(x==5)), float((x==6)||(x==7))); \n"
      "  y=dot(y4,  texture2D(texture, vec2(yx, yy))); \n"
      "  u=dot(uv4, texture2D(texture, vec2(ux, uy))); \n"
      "  v=dot(uv4, texture2D(texture, vec2(vx, vy))); \n"

      "  vec3 yuv=vec3(1.1643*(y-0.0625), u-0.5, v-0.5);     \n"
      "  vec3 rgb=yuv*yuv2rgb;                               \n"

      "  gl_FragColor=vec4(rgb, 1.0);                        \n"
      "}                                                     \n";
*/

   GLbyte fShaderStr_stpq[] =  
      "precision mediump float;                              \n"
      "varying vec2 v_texCoord;                              \n"
      "uniform vec2 texSize;                                 \n"
      "uniform mat3 yuv2rgb;                                 \n"
      "uniform sampler2D texture;                            \n"
      "void main()                                           \n"
      "{                                                     \n"
      "  float y, u, v, r, g, b;                             \n"
      "  float yx, yy, ux, uy, vx, vy;                       \n"

      "  vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y); \n"
      "  float oe=floor(mod(texCoord.x, 2.0));               \n"

      "  vec4 yuyv=texture2D(texture, v_texCoord);           \n"
      "  y=yuyv.p*oe+yuyv.s*(1.0-oe);                        \n"
      "  u=yuyv.t;                                           \n"
      "  v=yuyv.q;                                           \n"

      "  vec3 yuv=vec3(y, u-0.5, v-0.5);                     \n"
      "  vec3 rgb=yuv*yuv2rgb;                               \n"

      "  gl_FragColor=vec4(rgb, 1.0);                        \n"
      "}                                                     \n";

   // Load the shaders and get a linked program object
   userData->programObject = LoadProgram ((char *)vShaderStr, (char *)fShaderStr_stpq );
   if (!userData->programObject)
   {
     printf("Load Program %d\n",userData->programObject);
     return GL_FALSE;
   }

   userData->samplerLoc = glGetUniformLocation ( userData->programObject, "texture" );

   // Get the attribute locations
   userData->positionLoc = glGetAttribLocation ( userData->programObject, "a_position" ); // Query only
   userData->texCoordLoc = glGetAttribLocation ( userData->programObject, "a_texCoord" );
   
   userData->sizeLoc = glGetUniformLocation ( userData->programObject, "texSize" );
   userData->cmatrixLoc = glGetUniformLocation ( userData->programObject, "yuv2rgb" );
   
   // Use the program object
   glUseProgram ( userData->programObject ); 

   // Load the vertex position
   glVertexAttribPointer ( userData->positionLoc, 3, GL_FLOAT, 
                           GL_FALSE, 3 * sizeof(GLfloat), vVertices );
                           
   // Load the texture coordinate
   glVertexAttribPointer ( userData->texCoordLoc, 2, GL_FLOAT,
                           GL_FALSE, 2 * sizeof(GLfloat), tVertices );

   glEnableVertexAttribArray(userData->positionLoc);
   glEnableVertexAttribArray(userData->texCoordLoc);
   
   glUniform1i(userData->samplerLoc, 0);

/*
	// Create framebuffer
	glGenFramebuffers(1, &(userData->canvasFrameBuffer));
	glBindFramebuffer(GL_RENDERBUFFER, userData->canvasFrameBuffer);

	// Attach renderbuffer
	glGenRenderbuffers(1, &(userData->canvasRenderBuffer));
	glBindRenderbuffer(GL_RENDERBUFFER, userData->canvasRenderBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, pCodecCtx->width, pCodecCtx->height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, userData->canvasRenderBuffer);

	glGenTextures(1, &(userData->outtex));
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, userData->outtex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLfloat)GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLfloat)GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  pCodecCtx->width, pCodecCtx->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, userData->outtex, 0);
*/
   glClearColor ( 0.0f, 0.0f, 0.0f, 1.0f );
   glClear(GL_COLOR_BUFFER_BIT);
   return GL_TRUE;
}

void setSize(int width, int height)
{
	UserData *userData = p_state->user_data;
	int w, h;

	w = width;
	h = height;
	glGenTextures(1, &userData->tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, userData->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLfloat)GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLfloat)GL_CLAMP_TO_EDGE);
}

void texImage2D(unsigned char* buf, int width, int height)
{
   UserData *userData = p_state->user_data;

   int w, h;

   w = width;
   h = height;
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, userData->tex);
   //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);

   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLfloat)GL_CLAMP_TO_EDGE);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLfloat)GL_CLAMP_TO_EDGE);

    //checkNoGLES2Error();
}

void redraw_scene(CUBE_STATE_T *state)
{
	UserData *userData = p_state->user_data;

   glUniform1i(userData->samplerLoc, 0);
   // Set the viewport
   glViewport(0, 0, state->screen_width, state->screen_height);
   // Clear the color buffer
   glClear(GL_COLOR_BUFFER_BIT);
   glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
   glFinish();
   eglSwapBuffers(p_state->display, p_state->surface);
}

long long usecs; // microseconds

long get_first_time_microseconds()
{
	long long micros;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

	micros = spec.tv_sec * 1.0e6 + round(spec.tv_nsec / 1000.0); // Convert nanoseconds to microseconds
	usecs = micros;
	return(0L);
}

long get_next_time_microseconds()
{
    long delta;
    long long micros;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    micros = spec.tv_sec * 1.0e6 + round(spec.tv_nsec / 1000.0); // Convert nanoseconds to microseconds
    delta = micros - usecs;
    usecs = micros;
    return(delta);
}

#define idle 0
#define playing 1
#define paused 2
#define draining 3

// Video
int playerstatus = idle;

struct videoqueue
{
	struct videoqueue *prev;
	struct videoqueue *next;
	unsigned char *yuyv; // YUV422 byte array
};
struct videoqueue *vq = NULL;
int vqLength;
int vqMaxLength = 20;
pthread_mutex_t vqmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t vqlowcond = PTHREAD_COND_INITIALIZER;
pthread_cond_t vqhighcond = PTHREAD_COND_INITIALIZER;

void vq_init(struct videoqueue **q, pthread_mutex_t *m, pthread_cond_t *cl, pthread_cond_t *ch)
{
	int ret;

	*q = NULL;
	vqLength = 0;

	pthread_mutex_destroy(m);
	pthread_cond_destroy(cl);
	pthread_cond_destroy(ch);

	if((ret=pthread_mutex_init(m, NULL))!=0 )
		printf("mutex init failed, %d\n", ret);

	if((ret=pthread_cond_init(ch, NULL))!=0 )
		printf("cond init failed, %d\n", ret);

	if((ret=pthread_cond_init(cl, NULL))!=0 )
		printf("cond init failed, %d\n", ret);
}

void vq_add(struct videoqueue **q, unsigned char *yuyv)
{
	struct videoqueue *p;

	pthread_mutex_lock(&vqmutex);
	while (vqLength>=vqMaxLength)
	{
		//printf("Video queue sleeping, overrun\n");
		pthread_cond_wait(&vqhighcond, &vqmutex);
	}

	p = malloc(sizeof(struct videoqueue));
//printf("malloc vq %d\n", sizeof(struct videoqueue));
	if (*q == NULL)
	{
		p->next = p;
		p->prev = p;
		*q = p;
	}
	else
	{
		p->next = *q;
		p->prev = (*q)->prev;
		(*q)->prev = p;
		p->prev->next = p;
	}
	p->yuyv = yuyv;

	vqLength++;

	//condition = true;
	pthread_cond_signal(&vqlowcond); // Should wake up *one* thread
	pthread_mutex_unlock(&vqmutex);
}

struct videoqueue* vq_remove_element(struct videoqueue **q)
{
	struct videoqueue *p;

	if ((*q)->next == (*q))
	{
		p=*q;
		*q = NULL;
	}
	else
	{
		p = (*q);
		(*q) = (*q)->next;
		(*q)->prev = p->prev;
		(*q)->prev->next = (*q);
	}
	return p;
}

struct videoqueue* vq_remove(struct videoqueue **q)
{
	struct videoqueue *p;

	pthread_mutex_lock(&vqmutex);
	while((*q)==NULL) // queue empty
	{
		if ((playerstatus==playing) || (playerstatus==paused))
		{
			//printf("Video queue sleeping, underrun\n");
			pthread_cond_wait(&vqlowcond, &vqmutex);
		}
		else
			break;
	}
	switch (playerstatus)
	{
		case playing:
		case paused:
			p = vq_remove_element(q);
			vqLength--;
			break;
		case draining:
			if (vqLength>0)
			{
				p = vq_remove_element(q);
				vqLength--;
			}
			else
				p=NULL;
			break;
		default:
			p = NULL;
			break;
	}

	//condition = true;
	pthread_cond_signal(&vqhighcond); // Should wake up *one* thread
	pthread_mutex_unlock(&vqmutex);

	return p;
}

void vq_drain(struct videoqueue **q)
{
	pthread_mutex_lock(&vqmutex);
	while (vqLength)
	{
		pthread_mutex_unlock(&vqmutex);
		usleep(100000); // 0.1s
//printf("vqLength=%d\n", vqLength);
		pthread_mutex_lock(&vqmutex);
	}
	pthread_cond_signal(&vqlowcond); // Should wake up *one* thread
	pthread_mutex_unlock(&vqmutex);
//printf("vq_drain exit\n");
}

void vq_destroy(pthread_mutex_t *m, pthread_cond_t *cl, pthread_cond_t *ch)
{
	int ret;

	pthread_mutex_destroy(m);
	pthread_cond_destroy(cl);
	pthread_cond_destroy(ch);

	if((ret=pthread_mutex_init(m, NULL))!=0 )
		printf("destroy, mutex init failed, %d\n", ret);

	if((ret=pthread_cond_init(ch, NULL))!=0 )
		printf("destroy,cond init failed, %d\n", ret);

	if((ret=pthread_cond_init(cl, NULL))!=0 )
		printf("destroy,cond init failed, %d\n", ret);
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
	struct videoqueue *p;
	UserData *userData;
	unsigned char *src;

	int ctype = PTHREAD_CANCEL_ASYNCHRONOUS;
	int ctype_old;
	pthread_setcanceltype(ctype, &ctype_old);


	if (useGL)
	{
		bcm_host_init();
		init_ogl2(p_state); // Render to Frame Buffer
		if (Init(p_state) == GL_FALSE)
		{
			printf("Init\n");
			exit(0);
		}
		userData = p_state->user_data;
		setSize(width/2, height);

		initPixbuf(p_state->screen_width, p_state->screen_height);
		userData->outrgb = (unsigned char*)gdk_pixbuf_get_pixels(pixbuf);

		GLfloat picSize[2] = { (GLfloat)width, (GLfloat)height };
		glUniform2fv(userData->sizeLoc, 1, picSize);
		//GLfloat yuv2rgbmatrix[9] = { 1.0, 0.0, 1.5958, 1.0, -0.3917, -0.8129, 1.0, 2.017, 0.0 }; // YUV420
		GLfloat yuv2rgbmatrix[9] = { 1.0, 0.0, 1.402, 1.0, -0.344, -0.714, 1.0, 1.772, 0.0 }; // YUV422
		glUniformMatrix3fv(userData->cmatrixLoc, 1, FALSE, yuv2rgbmatrix);
	}
	else
	{
		p_state->user_data = malloc(sizeof(UserData));
		userData = p_state->user_data;
		userData->outrgb = (unsigned char*)gdk_pixbuf_get_pixels(pixbuf);
	}

	while (1)
	{
		if ((p = vq_remove(&vq)) == NULL)
			break;
		src = p->yuyv;

		if (useGL)
		{
			texImage2D(p->yuyv, width/2, height);
			redraw_scene(p_state);
			g_mutex_lock(&pixbufmutex);
			glReadPixels(0, 0, p_state->screen_width, p_state->screen_height, GL_RGBA, GL_UNSIGNED_BYTE, userData->outrgb);
			g_mutex_unlock(&pixbufmutex);

			gdk_threads_add_idle(invalidate, NULL);
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
		exit_func();

	retval_play = 0;
	pthread_exit((void*)&retval_play);
}

#define CLEAR(x) memset (&(x), 0, sizeof (x))

typedef enum {
#ifdef IO_READ
        IO_METHOD_READ,
#endif
#ifdef IO_MMAP
        IO_METHOD_MMAP,
#endif
#ifdef IO_USERPTR
        IO_METHOD_USERPTR,
#endif
} io_method;

struct buffer {
        void *                  start;
        size_t                  length;
};

static io_method        io              = IO_METHOD_MMAP;
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;

/**
  Print error message and terminate programm with EXIT_FAILURE return code.
  \param s error message to print
*/
static void errno_exit(const char* s)
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
static int xioctl(int fd, int request, void* argp)
{
  int r;

  do r = ioctl(fd, request, argp);
  while (-1 == r && EINTR == errno);

  return r;
}

/**
  Write image to jpeg file.

  \param img image to write
*/
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

/**
  process image read
*/
static void imageProcess(const void* p)
{
	int buffersize = 2*width*height*sizeof(char);
	unsigned char *yuyvbuf = malloc(buffersize);
	memcpy(yuyvbuf, p, buffersize);
	vq_add(&vq, yuyvbuf);
}

/**
  read single frame
*/

static int frameRead(void)
{
  struct v4l2_buffer buf;
#ifdef IO_USERPTR
  unsigned int i;
#endif

  switch (io) {
#ifdef IO_READ
    case IO_METHOD_READ:
      if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
            // Could ignore EIO, see spec.

            // fall through
          default:
            errno_exit("read");
        }
      }

      imageProcess(buffers[0].start);
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
      CLEAR (buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
             // Could ignore EIO, see spec

             // fall through
          default:
            errno_exit("VIDIOC_DQBUF");
        }
      }

      assert (buf.index < n_buffers);

      imageProcess(buffers[buf.index].start);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");

      break;
#endif

#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
      CLEAR (buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
            // Could ignore EIO, see spec.

            // fall through
          default:
            errno_exit("VIDIOC_DQBUF");
                        
        }
      }

      for (i = 0; i < n_buffers; ++i)
        if (buf.m.userptr == (unsigned long) buffers[i].start && buf.length == buffers[i].length)
          break;

      assert (i < n_buffers);

      imageProcess((void *) buf.m.userptr);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");
      break;
#endif

    }

  return 1;
}

/** 
  mainloop: read frames and process them
*/
static void mainLoop(void)
{
  unsigned int count;

	get_first_time_microseconds();

	count = 0;
  while (running)
  {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;

      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      /* Timeout. */
      tv.tv_sec = 2;
      tv.tv_usec = 0;

      r = select(fd + 1, &fds, NULL, NULL, &tv);

      if (-1 == r) {
        if (EINTR == errno)
          continue;

        errno_exit("select");
      }

      if (0 == r) {
        fprintf (stderr, "select timeout\n");
        exit(EXIT_FAILURE);
      }

	count++;
//printf("reading %d\n", count);
      if (frameRead())
        break;
        
      /* EAGAIN - continue select loop. */
    }
  }
  
 float secs = get_next_time_microseconds()/1000000.0;
 printf("%3.2f fps\n", (float)count/secs);
}

/**
  stop capturing
*/
static void captureStop(void)
{
  enum v4l2_buf_type type;

  switch (io) {
#ifdef IO_READ
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
#endif
#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
#endif
#if defined(IO_MMAP) || defined(IO_USERPTR)
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
        errno_exit("VIDIOC_STREAMOFF");

      break;
#endif 
   }
}

/**
  start capturing
*/
static void captureStart(void)
{
  unsigned int i;
  enum v4l2_buf_type type;

  switch (io) {
#ifdef IO_READ    
    case IO_METHOD_READ:
      /* Nothing to do. */
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
          errno_exit("VIDIOC_QBUF");
      }
                
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");

      break;
#endif

#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = i;
        buf.m.userptr   = (unsigned long) buffers[i].start;
        buf.length      = buffers[i].length;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
          errno_exit("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
         errno_exit("VIDIOC_STREAMON");

      break;
#endif
  }
}

static void deviceUninit(void)
{
  unsigned int i;

  switch (io) {
#ifdef IO_READ
    case IO_METHOD_READ:
      free(buffers[0].start);
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
      if (-1 == munmap (buffers[i].start, buffers[i].length))
        errno_exit("munmap");
      break;
#endif

#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i)
        free (buffers[i].start);
      break;
#endif
  }

  free(buffers);
}

#ifdef IO_READ
static void readInit(unsigned int buffer_size)
{
  buffers = calloc(1, sizeof(*buffers));

  if (!buffers) {
    fprintf (stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }

  buffers[0].length = buffer_size;
  buffers[0].start = malloc (buffer_size);

  if (!buffers[0].start) {
    fprintf (stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }
}
#endif

#ifdef IO_MMAP
static void mmapInit(void)
{
  struct v4l2_requestbuffers req;

  CLEAR (req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s does not support memory mapping\n", deviceName);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2) {
    fprintf(stderr, "Insufficient buffer memory on %s\n", deviceName);
    exit(EXIT_FAILURE);
  }

  buffers = calloc(req.count, sizeof(*buffers));

  if (!buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR (buf);

    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = n_buffers;

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
      errno_exit("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start =
    mmap (NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */, MAP_SHARED /* recommended */, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
      errno_exit("mmap");
  }
}
#endif

#ifdef IO_USERPTR
static void userptrInit(unsigned int buffer_size)
{
  struct v4l2_requestbuffers req;
  unsigned int page_size;

  page_size = getpagesize ();
  buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

  CLEAR(req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_USERPTR;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s does not support user pointer i/o\n", deviceName);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  buffers = calloc(4, sizeof (*buffers));

  if (!buffers) {
    fprintf(stderr, "Out of memory\n");
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
    buffers[n_buffers].length = buffer_size;
    buffers[n_buffers].start = memalign (/* boundary */ page_size, buffer_size);

    if (!buffers[n_buffers].start) {
      fprintf(stderr, "Out of memory\n");
      exit(EXIT_FAILURE);
    }
  }
}
#endif

/**
  initialize device
*/
static void deviceInit(void)
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s is no V4L2 device\n",deviceName);
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "%s is no video capture device\n",deviceName);
    exit(EXIT_FAILURE);
  }

  switch (io) {
#ifdef IO_READ
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        fprintf(stderr, "%s does not support read i/o\n",deviceName);
        exit(EXIT_FAILURE);
      }
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
#endif
#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
#endif
#if defined(IO_MMAP) || defined(IO_USERPTR)
      if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "%s does not support streaming i/o\n",deviceName);
        exit(EXIT_FAILURE);
      }
      break;
#endif
  }


  /* Select video input, video standard and tune here. */
  CLEAR(cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
      }
    }
  } else {        
    /* Errors ignored. */
  }

  CLEAR (fmt);

  // v4l2_format
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = width; 
  fmt.fmt.pix.height      = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
    errno_exit("VIDIOC_S_FMT");

  /* Note VIDIOC_S_FMT may change width and height. */
  if (width != fmt.fmt.pix.width) {
    width = fmt.fmt.pix.width;
    fprintf(stderr,"Image width set to %i by device %s.\n",width,deviceName);
  }
  if (height != fmt.fmt.pix.height) {
    height = fmt.fmt.pix.height;
    fprintf(stderr,"Image height set to %i by device %s.\n",height,deviceName);
  }

  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;

  switch (io) {
#ifdef IO_READ
    case IO_METHOD_READ:
      readInit(fmt.fmt.pix.sizeimage);
      break;
#endif

#ifdef IO_MMAP
    case IO_METHOD_MMAP:
      mmapInit();
      break;
#endif

#ifdef IO_USERPTR
    case IO_METHOD_USERPTR:
      userptrInit(fmt.fmt.pix.sizeimage);
      break;
#endif
  }
}

/**
  close device
*/
static void deviceClose(void)
{
  if (-1 == close (fd))
    errno_exit("close");

  fd = -1;
}

/**
  open device
*/
static void deviceOpen(void)
{
  struct stat st;

  // stat file
  if (-1 == stat(deviceName, &st)) {
    fprintf(stderr, "Cannot identify '%s': %d, %s\n", deviceName, errno, strerror (errno));
    exit(EXIT_FAILURE);
  }

  // check if its device
  if (!S_ISCHR (st.st_mode)) {
    fprintf(stderr, "%s is no device\n", deviceName);
    exit(EXIT_FAILURE);
  }

  // open device
  fd = open(deviceName, O_RDWR /* required */ | O_NONBLOCK, 0);

  // check if opening was successfull
  if (-1 == fd) {
    fprintf(stderr, "Cannot open '%s': %d, %s\n", deviceName, errno, strerror (errno));
    exit(EXIT_FAILURE);
  }
}

/**
  print usage information
*/
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

void* captureThread(void *arg)
{

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

#ifdef IO_USERPTR
        io = IO_METHOD_USERPTR;
#else
        fprintf(stderr, "You didn't compile for userptr support.\n");
        exit(EXIT_FAILURE);         
#endif

  // open and initialize device
  deviceOpen();
  deviceInit();

  // start capturing
  captureStart();

  // process frames
  mainLoop();

  // stop capturing
  captureStop();

  // close device
  deviceUninit();
  deviceClose();

	retval_capture = 0;
	pthread_exit((void*)&retval_capture);
}

void* waitThread(void *arg)
{
	// Init Video Queue
	vq_init(&vq, &vqmutex, &vqlowcond, &vqhighcond);
	playerstatus = playing;

	int err;

	err = pthread_create(&(tid[1]), NULL, &captureThread, NULL);
	if (err)
	{}
	CPU_ZERO(&(cpu[1]));
	CPU_SET(1, &(cpu[1]));
	err = pthread_setaffinity_np(tid[1], sizeof(cpu_set_t), &(cpu[1]));
	if (err)
	{}

	err = pthread_create(&(tid[2]), NULL, &playThread, NULL);
	if (err)
	{}
	CPU_ZERO(&(cpu[2]));
	CPU_SET(2, &(cpu[2]));
	err = pthread_setaffinity_np(tid[2], sizeof(cpu_set_t), &(cpu[2]));
	if (err)
	{}
	
	int i;
	if ((i=pthread_join(tid[1], NULL)))
		printf("pthread_join error, tid[1], %d\n", i);

	pthread_mutex_lock(&vqmutex);
	playerstatus = draining;
	pthread_mutex_unlock(&vqmutex);
	vq_drain(&vq);
	playerstatus = idle;

	if ((i=pthread_join(tid[2], NULL)))
		printf("pthread_join error, tid[2], %d\n", i);

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
	running = 0;
	while (playerstatus!=idle)
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
	running = 1;
	create_threads();
}

static void button2_clicked(GtkWidget *button, gpointer data)
{
	running = 0;
	gtk_widget_set_sensitive(glcheckbox, TRUE);
}

static void usegl_toggled(GtkWidget *togglebutton, gpointer data)
{
	useGL = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton));
	//printf("toggle state %d\n", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dlyenable)));
}

int main(int argc, char **argv)
{

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
    g_signal_connect (window, "delete-event", G_CALLBACK (delete_event), NULL);
    
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
    gtk_widget_set_size_request (dwgarea, width, height);
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

// horizontal box
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(capturebox), hbox);
    
// checkbox
	useGL = TRUE;
	glcheckbox = gtk_check_button_new_with_label("Use GL");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glcheckbox), useGL);
	g_signal_connect(GTK_TOGGLE_BUTTON(glcheckbox), "toggled", G_CALLBACK(usegl_toggled), NULL);
	gtk_container_add(GTK_CONTAINER(hbox), glcheckbox);

	initPixbuf(width, height);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
