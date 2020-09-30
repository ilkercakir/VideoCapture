#include "YUYVYUV420gl.h"

void checkNoGLES2Error() 
{
    int error;
    while ((error=glGetError()) != GL_NO_ERROR)
    {
		printf("GLES20 error: %d", error);
	}
}

/*
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
    success = graphics_get_display_size(0 / * LCD * /, &p_state->screen_width, &p_state->screen_height);
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

    dispman_display = vc_dispmanx_display_open( 0 /  * LCD * /);
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_element = 
	vc_dispmanx_element_add(dispman_update, dispman_display,
				0/ *layer* /, &dst_rect, 0/ *src* /,
				&src_rect, DISPMANX_PROTECTION_NONE, 
				0 / *alpha* /, 0/ *clamp* /, 0/ *transform* /);

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
*/

void init_ogl2(CUBE_STATE_T *p_state, int playerwidth, int playerheight)
{
    EGLBoolean result;
    EGLint num_config;

GLfloat lvVertices[] = { -1.0f,  1.0f, 0.0f, // Position 0
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
GLfloat ltVertices[] = {  0.0f,  1.0f,        // TexCoord 0 
                         0.0f,  0.0f,        // TexCoord 1
                         1.0f,  0.0f,        // TexCoord 2
                         1.0f,  1.0f         // TexCoord 3
                      };
                      
GLushort lindices[] = { 0, 1, 2, 0, 2, 3 };

	int i;
	for(i=0;i<12;i++)
		p_state->vVertices[i] = lvVertices[i];

	for(i=0;i<8;i++)
		p_state->tVertices[i] = ltVertices[i];

	for(i=0;i<6;i++)
		p_state->indices[i] = lindices[i];

    static const EGLint attribute_list[] =
	{
	    EGL_RED_SIZE, 8,
	    EGL_GREEN_SIZE, 8,
	    EGL_BLUE_SIZE, 8,
	    EGL_ALPHA_SIZE, 8,
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
    p_state->screen_width = playerwidth;
    p_state->screen_height = playerheight;

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

void exit_func(CUBE_STATE_T *p_state) // Function to be passed to atexit().
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
	free(userData);
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

char* readShader(char* filename)
{
	FILE *f;
	int filesize, pos;
	char line[1024];
	char *shader;

	line[0] = '\0';

	if ((f = fopen(filename , "r")) == NULL)
	{
		printf("Error opening shader file %s\n", filename);
		return "";
	}

	fseek(f, 0L, SEEK_END);
	filesize = ftell(f);
	rewind(f);

	shader = malloc((filesize+1)*sizeof(char));

	for(pos=0;fgets(line, sizeof(line)-1, f)!=NULL;pos+=strlen(line))
	{
		sprintf(shader+pos, "%s", line);
	}

	fclose(f);

	return shader;
}

//
// Initialize the shader and program object
//
int Init(CUBE_STATE_T *p_state)
{
   p_state->user_data = malloc(sizeof(UserData));      
   UserData *userData = p_state->user_data;

/*
   GLbyte vShaderStr[] =  
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

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
*/
	GLbyte *vShaderStr = (GLbyte *)readShader("shader.vert");
	GLbyte *fShaderStr_stpq = (GLbyte *)readShader("shader.frag");
/*
   GLbyte fShaderStr_stpq[] =  
      "precision mediump float;                              \n"
      "varying vec2 v_texCoord;                              \n"
      "uniform vec2 texSize;                                 \n"
      "uniform sampler2D texture;                            \n"
      "void main()                                           \n"
      "{                                                     \n"
      "  vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y); \n"
      "  float oe=floor(mod(texCoord.x, 2.0));               \n"

      "  vec2 tc = vec2(v_texCoord.x, v_texCoord.y*3.0/2.0); \n"
      "  vec4 yuyv=texture2D(texture, tc);           \n"
      "  float y=yuyv.p*oe+yuyv.s*(1.0-oe);                        \n"
      "  y = float(v_texCoord.y<4.0/6.0) * y;   \n"
      
      "  float yu = (v_texCoord.y*3.0/2.0 - 1.0)*4.0 + float(v_texCoord.x>=0.5)*3.0/texCoord.y;    \n"
      "  tc = vec2(mod(v_texCoord.x*2.0, 1.0), yu);  \n"
      "  yuyv=texture2D(texture, tc);    \n"
      "  float u = yuyv.t;   \n"
      "  u = float(v_texCoord.y>=4.0/6.0 && v_texCoord.y<5.0/6.0) * u;    \n"

      "  y += u;    \n"

      "  float yv = (v_texCoord.y*3.0/2.0 - 5.0/4.0)*4.0 + float(v_texCoord.x>=0.5)*3.0/texCoord.y;    \n"
      "  tc = vec2(mod(v_texCoord.x*2.0, 1.0), yv);  \n"
      "  yuyv=texture2D(texture, tc);    \n"
      "  float v = yuyv.q;   \n"
      "  v = float(v_texCoord.y>=5.0/6.0) * v;    \n"

      "  y += v;    \n"

      "  gl_FragColor=vec4(y, y, y, y);                    \n"
      "}                                                     \n";
*/

   // Load the shaders and get a linked program object
   userData->programObject = LoadProgram ((char *)vShaderStr, (char *)fShaderStr_stpq );
   if (!userData->programObject)
   {
     printf("Load Program %d\n", userData->programObject);
     free(vShaderStr);
     free(fShaderStr_stpq);
     return GL_FALSE;
   }

	free(vShaderStr);
	free(fShaderStr_stpq);

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
                           GL_FALSE, 3 * sizeof(GLfloat), p_state->vVertices );
                           
   // Load the texture coordinate
   glVertexAttribPointer ( userData->texCoordLoc, 2, GL_FLOAT,
                           GL_FALSE, 2 * sizeof(GLfloat), p_state->tVertices );

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

void setSize(CUBE_STATE_T *p_state, int width, int height)
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

void texImage2D(CUBE_STATE_T *p_state, unsigned char* buf, int width, int height)
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

void redraw_scene(CUBE_STATE_T *p_state)
{
	UserData *userData = p_state->user_data;

   glUniform1i(userData->samplerLoc, 0);
   // Set the viewport
   glViewport(0, 0, p_state->screen_width, p_state->screen_height);
   // Clear the color buffer
   glClear(GL_COLOR_BUFFER_BIT);
   glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, p_state->indices);
   glFinish();
   eglSwapBuffers(p_state->display, p_state->surface);
}
