#ifndef YUYVYUV420glH
#define YUYVYUV420glH

#include <stdio.h>
#include <assert.h>

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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
	GLfloat vVertices[12];
	GLfloat tVertices[8];
	GLushort indices[6];
} CUBE_STATE_T;

void checkNoGLES2Error();
void init_ogl2(CUBE_STATE_T *state, int playerwidth, int playerheight);
void exit_func(CUBE_STATE_T *p_state);
GLuint LoadShader(GLenum type, const char *shaderSrc);
GLuint LoadProgram( const char *vertShaderSrc, const char *fragShaderSrc);
char* readShader(char* filename);
int Init(CUBE_STATE_T *p_state);
void setSize(CUBE_STATE_T *p_state, int width, int height);
void texImage2D(CUBE_STATE_T *p_state, unsigned char* buf, int width, int height);
void redraw_scene(CUBE_STATE_T *state);
#endif
