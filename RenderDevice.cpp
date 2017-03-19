#include <stdlib.h>
#include <stdio.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
//#include <GL/glx.h>
//#include <GL/glxext.h>

#include "Render.h"
#include "SDL.h"
#include <math.h>
#include <string.h>

#include "Globals.h"

#ifndef M_PI
#define M_PI 3.14159265
#endif


// externs for the various shader strings
extern string FRAG_LOGSCALE;
extern string FRAG_MINMAX;
extern string FRAG_MINMAXSTART;
extern string VERT_PSPRITE;
extern string FRAG_PSPRITE;


bool Render::Init()
{
    SDL_WM_SetCaption("Vis", "vis");

    // initial value, so we know it doesn't exist yet
    renderbuffer = 0;
    lastFPSCheck = 0;
    lastFPS = 0;
    curFPS = 0;

    viewBoxes = false;
    viewPoints = true;
    takeScreenshot = false;

    lastMinmaxCheck=-1000;
    curScoreScale = 1;

    autoScale = true;

    curScreenshotIndex = 10000;


    // setup display
    Resize(1600,1200);

    printf("OpenGL version: %s\n",glGetString(GL_VERSION));
    loadShaders();
    computeScaleFactors();
    loadColorTable();

    // setup some generic opengl options
    glClampColorARB(GL_CLAMP_VERTEX_COLOR_ARB, GL_FALSE);
    glClampColorARB(GL_CLAMP_FRAGMENT_COLOR_ARB, GL_FALSE);
    glClampColorARB(GL_CLAMP_READ_COLOR_ARB, GL_FALSE);
    // enable vertex shaders to set gl_PointSize param
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    return true;
}


void Render::Cleanup()
{
    
}

// saves screen data to a file
void Render::saveBuffer()
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glReadBuffer(GL_FRONT);

    int rowlength = 3*screen->w;
    char *data = (char*)malloc(rowlength*screen->h);

    glFlush();
    SDL_Surface* img = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h, 24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);

    glReadPixels(0, 0, screen->w, screen->h, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)data);

    // flip image upside-down
    SDL_LockSurface(img);
    for (int y=0; y<screen->h; y++)
	memcpy(((char*)img->pixels) + rowlength*y, data + rowlength*(screen->h-y-1), rowlength);
    SDL_UnlockSurface(img);

    SDL_SaveBMP(img, ("/home/tamas/caps/screen"+toString(curScreenshotIndex)+".bmp").c_str());
    SDL_FreeSurface(img);
    free(data);

    curScreenshotIndex++;
}


void Render::updateFPS()
{
    int curtime = SDL_GetTicks();
    if (curtime - lastFPSCheck > 1000)
    {
	lastFPS = (1000.0f * curFPS) / (curtime-lastFPSCheck);
	printf("Last sec, %4.1f FPS\n", lastFPS);
	curFPS = 0;
	lastFPSCheck = curtime;
    }

    curFPS++;
}


// Does everything necessary for resizing
void Render::Resize(int width, int height)
{
    screen = SDL_SetVideoMode(width, height, 24,
			      SDL_OPENGL|SDL_FULLSCREEN);
    if (screen) 
    {
	printf("Screen resized to %d,%d!\n",width,height);
	// and create/recreate our buffers
	genBuffers();
    } 
    else 
    { 
	fprintf(stderr, "Couldn't set basic video mode: %s\n", SDL_GetError());
	SDL_Quit();
	exit(2);
    }
}

 

// Correctly sets OpenGL projection params
void Render::setPerspective()
{
    int width = screen->w;
    int height = screen->h;
    
    glViewport(0, 0, (GLint) width, (GLint) height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(fov, aspect, nearPlane, farPlane);
}

// Set up for orthogonal viewing
void Render::setOrtho()
{
    // set up correct view transform
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
}

// Sets the lookat matrix for perspective projection
void Render::setLookAt()
{
    Vector3 pos, targ, up;
    g_State->GetWorldVectors(pos, targ, up);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(pos.x, pos.y, pos.z, targ.x, targ.y, targ.z, up.x, up.y, up.z);
}


// returns world<->pixel scale factor at unit distance
double Render::getPixelScale()
{
    // fov degrees covers entire screen (y)
    // so use simple tan function
    return tan(M_PI*fov/360 )*screen->h*0.5;
}




// Creates the framebuffer object as necessary
void Render::genBuffers()
{
    if (renderbuffer != 0)
    {
//	glDeleteFramebuffers(NBUFS, buffers);
    }

    // print out this tidbit
    GLint maxbuffers;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxbuffers);
    printf("Found %d texture attachment points.\n",maxbuffers);

    // generate main render buffer
    glGenFramebuffersEXT(1, &renderbuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, renderbuffer);


    
    // make main texture
    // use ARB float format to avoid clamping, even with blending
    glGenTextures(1, &rendertex);
    glBindTexture(GL_TEXTURE_2D, rendertex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, screen->w, screen->h,
		 0, GL_RGB, GL_FLOAT, NULL);

    // set its parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    // and attach it
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			      GL_TEXTURE_2D, rendertex, 0);

    // see if it went ok
    checkFrameBuffer();



    // now make our minmax buffer
    glGenFramebuffersEXT(1, &minmaxbuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, minmaxbuffer);

    // and our minmax textures, two of 'em, each half the screen
    // note that we want to use RGBA format for these
    glGenTextures(1, minmaxtex+0);
    glBindTexture(GL_TEXTURE_2D, minmaxtex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, screen->w/MMFAC, screen->h/MMFAC,
		 0, GL_RGBA, GL_FLOAT, NULL);
    // set their parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, minmaxtex+1);
    glBindTexture(GL_TEXTURE_2D, minmaxtex[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, screen->w/MMFAC, screen->h/MMFAC,
		 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);

    // and attach them to the buffer
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			      GL_TEXTURE_2D, minmaxtex[0], 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT,
			      GL_TEXTURE_2D, minmaxtex[1], 0);
 
    // see if it went ok
    checkFrameBuffer();

    // and unbind
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);


    // now make the selected points texture
    // we need to use RGBA_FLOAT32_ATI for these
    glGenTextures(1, &selectedtex);
    glBindTexture(GL_TEXTURE_2D, selectedtex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA_FLOAT32_ATI, 
		 SELTEXDIM, SELTEXDIM, 0, GL_RGBA, GL_FLOAT, NULL);
    // set the parameters, must be this!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);


    // and unbind
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Checks status of bound FBO
void Render::checkFrameBuffer()
{
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status == GL_FRAMEBUFFER_COMPLETE_EXT)
    {
	printf("Framebuffer created successfully.\n");
    }
    else
    {
	switch (status)
	{
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
	    printf("ERROR: Incomplete attachment!\n");
	    break;
	case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
	    printf("ERROR: Incomplete dimensions!\n");
	    break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
	    printf("ERROR: Missing attachment!\n");
	    break;
	case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
	    printf("ERROR: Framebuffer unsupported!\n");
	    break;
	case GL_INVALID_ENUM:
	    printf("ERROR: Invalid enum!\n");
	    break;
	default:
	    printf("ERROR: %xd\n",status);
	    break;
	}
    }
}


// Loads fragment and vertex shaders
void Render::loadShaders()
{
    // make objects
    for (int i=0; i<NFRAGS; i++)
	fragshader[i] = glCreateShader(GL_FRAGMENT_SHADER);
    for (int i=0; i<NVERTS; i++)
	vertshader[i] = glCreateShader(GL_VERTEX_SHADER);
    for (int i=0; i<NPROGS; i++)
	shaderprog[i] = glCreateProgram();

    const char *src1 = FRAG_LOGSCALE.c_str();
    const char *src2 = FRAG_MINMAX.c_str();
    const char *src3 = FRAG_MINMAXSTART.c_str();
    const char *src4 = VERT_PSPRITE.c_str();
    const char *src5 = FRAG_PSPRITE.c_str();
    
    // now set sources
    glShaderSource(fragshader[0], 1, &src1, NULL);
    glShaderSource(fragshader[1], 1, &src2, NULL);
    glShaderSource(fragshader[2], 1, &src3, NULL);
    glShaderSource(fragshader[3], 1, &src5, NULL);

    glShaderSource(vertshader[0], 1, &src4, NULL);
 
    // and compile
    for (int i=0; i<NFRAGS; i++)
	glCompileShader(fragshader[i]);
    for (int i=0; i<NVERTS; i++)
	glCompileShader(vertshader[i]);
     
    // attach to programs
    glAttachShader(shaderprog[P_LOGSCALE], fragshader[0]);
    glAttachShader(shaderprog[P_MINMAX], fragshader[1]); 
    glAttachShader(shaderprog[P_MINMAXSTART], fragshader[2]);

    glAttachShader(shaderprog[P_PSPRITE], vertshader[0]);
    glAttachShader(shaderprog[P_PSPRITE], fragshader[3]);

    // and link
    for (int i=0; i<NPROGS; i++)
	glLinkProgram(shaderprog[i]);

    int param;
    for (int i=0; i<NFRAGS; i++)
    {
	glGetShaderiv(fragshader[i], GL_COMPILE_STATUS, &param);
	if (param == GL_TRUE)
	{
	    printf("Frag shader %d compiled.\n", i);
	}
	else
	{
	    printf("Error compiling frag shader %d!\n", i);
	    printInfoLog(fragshader[i]);
	    g_State->Quit();
	}
    }

    for (int i=0; i<NVERTS; i++)
    {
	glGetShaderiv(vertshader[i], GL_COMPILE_STATUS, &param);
	if (param == GL_TRUE)
	{
	    printf("Vert shader %d compiled.\n", i);
	}
	else
	{
	    printf("Error compiling vert shader %d!\n", i);
	    printInfoLog(vertshader[i]);
	    g_State->Quit();
	}
    }

    for (int i=0; i<NPROGS; i++)
    {
	glGetProgramiv(shaderprog[i], GL_LINK_STATUS, &param);
	if (param == GL_TRUE)
	{
	    printf("Program %d linked.\n", i);
	}
	else
	{
	    printf("Error linking program %d!\n", i);
	    printProgramLog(shaderprog[i]);
	    g_State->Quit();
	}
    }

    glUseProgram(shaderprog[P_LOGSCALE]);
    // and find the addresses of each uniform variable
    uniform[0] = glGetUniformLocation(shaderprog[P_LOGSCALE], "logminscl");
    uniform[1] = glGetUniformLocation(shaderprog[P_LOGSCALE], "selminscl");
    // oopsies
    uniform[8] = glGetUniformLocation(shaderprog[P_LOGSCALE], "vdispmax");
    uniform[9] = glGetUniformLocation(shaderprog[P_LOGSCALE], "src");
    uniform[10] = glGetUniformLocation(shaderprog[P_LOGSCALE], "ctable");

    glUseProgram(shaderprog[P_MINMAX]);
    uniform[2] = glGetUniformLocation(shaderprog[P_MINMAX], "xscale");
    uniform[3] = glGetUniformLocation(shaderprog[P_MINMAX], "yscale");
    uniform[4] = glGetUniformLocation(shaderprog[P_MINMAX], "width");
    uniform[5] = glGetUniformLocation(shaderprog[P_MINMAX], "height");

    glUseProgram(shaderprog[P_MINMAXSTART]);
    uniform[6] = glGetUniformLocation(shaderprog[P_MINMAXSTART], "width");
    uniform[7] = glGetUniformLocation(shaderprog[P_MINMAXSTART], "height");

    glUseProgram(shaderprog[P_PSPRITE]);
    ptuniform[0] = glGetUniformLocation(shaderprog[P_PSPRITE], "pmin");
    ptuniform[1] = glGetUniformLocation(shaderprog[P_PSPRITE], "vmin");
    ptuniform[2] = glGetUniformLocation(shaderprog[P_PSPRITE], "amin");
    ptuniform[3] = glGetUniformLocation(shaderprog[P_PSPRITE], "hmin");
    ptuniform[4] = glGetUniformLocation(shaderprog[P_PSPRITE], "cmin");
    ptuniform[5] = glGetUniformLocation(shaderprog[P_PSPRITE], "pscl");
    ptuniform[6] = glGetUniformLocation(shaderprog[P_PSPRITE], "vscl");
    ptuniform[7] = glGetUniformLocation(shaderprog[P_PSPRITE], "ascl");
    ptuniform[8] = glGetUniformLocation(shaderprog[P_PSPRITE], "hscl");
    ptuniform[9] = glGetUniformLocation(shaderprog[P_PSPRITE], "cscl");

    ptuniform[10] = glGetUniformLocation(shaderprog[P_PSPRITE], "dt");
    ptuniform[11] = glGetUniformLocation(shaderprog[P_PSPRITE], "hsmlscale");

    ptuniform[12] = glGetUniformLocation(shaderprog[P_PSPRITE], "nfac");
    ptuniform[13] = glGetUniformLocation(shaderprog[P_PSPRITE], "seltex");
    ptuniform[14] = glGetUniformLocation(shaderprog[P_PSPRITE], "numSel");

    // set default for this one, important
    glUniform1i(ptuniform[14],0);

    attribute[0] = glGetAttribLocation(shaderprog[P_PSPRITE], "pos");
    attribute[1] = glGetAttribLocation(shaderprog[P_PSPRITE], "vel");
    attribute[2] = glGetAttribLocation(shaderprog[P_PSPRITE], "acc");
    attribute[3] = glGetAttribLocation(shaderprog[P_PSPRITE], "hsml");
    attribute[4] = glGetAttribLocation(shaderprog[P_PSPRITE], "clr");
    attribute[5] = glGetAttribLocation(shaderprog[P_PSPRITE], "nclr");
    attribute[6] = glGetAttribLocation(shaderprog[P_PSPRITE], "pid");


    // and unbind
    glUseProgram(0);
}

// computes normalization scaling factors for point blobs
void Render::computeScaleFactors()
{
    float blobSum[MAX_POINT_SIZE];
    // loop through, and for each point size, compute what the sum
    // of all the weights will be
    for (int size=1; size<=MAX_POINT_SIZE; size++)
    {
	blobSum[size-1] = 0;
	for (int x=0; x<size; x++)
	{
	    for (int y=0;y<size;y++)
	    {
		// get coords in 0 ... 1, but never either extreme
		float px = (x+0.5f)/(float)size;
		float py = (y+0.5f)/(float)size;
		px = 2*(px-0.5f); // -1...1
		py = 2*(py-0.5f);
		// distance to center of sprite
		float u = sqrt(px*px+py*py);
		if (u > 1)
		    continue;
		// volker's code
		float fac = 0;
		if (u < 0.5)
		    fac = (2.546479089470 + 15.278874536822*(u-1)*u*u);
		else
		    fac = 5.092958178941*(1.0-u)*(1.0-u)*(1.0-u);
		blobSum[size-1] += fac;
	    }
	}
    }

    glUseProgram(shaderprog[P_PSPRITE]);
    // set array of sums
    glUniform1fv(ptuniform[12], MAX_POINT_SIZE, blobSum);
    // tell opengl to interpolate tex coords over point sprites
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
}

/* prints the debug output for a single shader. */
void Render::printInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten = 0;
    char *infoLog;

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    
    if (infologLength > 0)
    {
	infoLog = (char*)malloc(infologLength);
	glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
	printf("Shader %d infolog:\n%s\n", obj, infoLog);
	free(infoLog);
	fflush(stdout);
    }
}

/* prints the debug output for a single shader. */
void Render::printProgramLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten = 0;
    char *infoLog;

    glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &infologLength);
    
    if (infologLength > 0)
    {
	infoLog = (char*)malloc(infologLength);
	glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
	printf("Program %d infolog:\n%s\n", obj, infoLog);
	free(infoLog);
	fflush(stdout);
    }
}

/* Loads colormap data from file and puts it into texture. */
void Render::loadColorTable()
{			       	       
    // open and alloc array
    FILE *file = fopen("colortable.dat","rb");
    int size = 256*360*3;
    char *data = (char*)malloc(size);

    // read in table
    fread(data,size,1,file);
    fclose(file);

    // generate the texture
    glGenTextures(1, &colortex);
    // select it
    glBindTexture(GL_TEXTURE_2D, colortex);

    // and set it
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 360, 0, GL_RGB,
		 GL_UNSIGNED_BYTE, data);

    // set their parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);


    glBindTexture(GL_TEXTURE_2D, 0);

    free(data);
}



Render::Render()
{
}

Render::~Render()
{
}
