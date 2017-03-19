#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#include "Globals.h"
#include "Render.h"
#include <math.h>
#include <string.h>
#include "SDL.h"


void Render::Draw()
{
    int startTime = SDL_GetTicks();

    // draw all the blocks
    drawBlocks();
    // do this to synchronize, for timing
    glFinish();

    // now, update dynamic FPS scaling, if we want to
    if (autoScale)
	updateScoreScale(0.001*(SDL_GetTicks()-startTime));


    // compute min-max, if timing says to
    getMinMax();

    // and now render offscreen buffer to screen
    renderToScreen();

    // save screen, if requested
    if (takeScreenshot)
    {
	saveBuffer();
	takeScreenshot = false;
    }

    // and swap
    SDL_GL_SwapBuffers();

    // and update frame stats
    updateFPS();
}


// fast algorithm for computing min/max, uses GPU
void Render::getMinMax()
{
    // update with interpolated min/max values
    int timeSince = SDL_GetTicks() - lastMinmaxCheck;

    // get factor that takes us from 0...1
    float dt = timeSince / (float)minmaxInterpTime;
    if (dt > 1) dt = 1;

    float val[4];
    // now interpolate by logarithm
    for (int i=0; i<4; i++)
    {
	double cur = log((double)curMinmax[i]);
	double last = log((double)lastMinmax[i]);
	cur = dt * cur + (1-dt)*last;
	val[i] = (float)exp(cur);
    }

/*    if (isnan(val[0]))
    {
        val[0] = g_Opts->view.forceMin;
	val[2] = g_Opts->view.forceMin*1000;
    }
*/
    // adjust for forced min settings
    if (val[0] < g_Opts->view.forceMin)
	val[0] = g_Opts->view.forceMin;
    if (val[2] < val[0]) val[2] = 2*val[0];

    if (g_Opts->dbg.printRender)
	printf("min/max set to %g, %g\n",val[0],val[2]);

    glUseProgram(shaderprog[P_LOGSCALE]);
    // and update uniform variable for min/max, in shader
    glUniform2f(uniform[0], val[0], log(val[2]/val[0]));
    
    // and just return, if we don't want to recompute yet
    if (timeSince < (uint)minmaxTime)
	return;

    lastMinmaxCheck = SDL_GetTicks();

    // *** if we didn't draw any points, only blocks, don't recompute ***
    if (!viewPoints)
	return;

    if (g_Opts->dbg.printRender)
	printf("Recomputing min/max...\n");

    // set orthogonal transform
    setOrtho();

    // disable blending, obvs
    glDisable(GL_BLEND);

    int width = screen->w/MMFAC;
    int height = screen->h/MMFAC;

    int nsteps = 1;

    // resize viewport for smaller textures
    glPushAttrib(GL_VIEWPORT_BIT);
    glViewport(0,0,width,height);

    // set MINMAXSTART
    glUseProgram(shaderprog[P_MINMAXSTART]);

    // set width and height
    glUniform1f(uniform[6],(float)screen->w);
    glUniform1f(uniform[7],(float)screen->h);

    // draw to minmax buffer, from original texture
    // target 0 is okay for the moment
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, minmaxbuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

    // prepare texture for rendering
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, rendertex);

    glBegin(GL_QUADS);
    
    // draw quad on entire screen
    glVertex2f(0, 0);
    glVertex2f(1, 0);
    glVertex2f(1, 1);
    glVertex2f(0, 1);

    glEnd();

    int texsrc = 0;
    int texdest = 1;

    float texu = 2.0f;
    float texv = 2.0f;

    // now reduce the rest of the way
    glUseProgram(shaderprog[P_MINMAX]);
    // set the uniforms to source widths and heights
    glUniform1i(uniform[4],screen->w/MMFAC);
    glUniform1i(uniform[5],screen->h/MMFAC);

    while (width > 1 || height > 1)
    {
	int xfac, yfac;
	if (width%16 == 0)
	    xfac = 16;
	else if (width%8 == 0)
	    xfac = 8;
	else if (width%6 == 0)
	    xfac = 6;
	else if (width%5 == 0)
	    xfac = 5;
	else if (width%4 == 0)
	    xfac = 4;
	else if (width % 3 == 0)
	    xfac = 3;
	else if (width % 2 == 0)
	    xfac = 2;
	else
	    xfac = 1;

	if (height%16 == 0)
	    yfac = 16;
	else if (height%8 == 0)
	    yfac = 8;
	else if (height%6 == 0)
	    yfac = 6;
	else if (height%5 == 0)
	    yfac = 5;
	else if (height%4 == 0)
	    yfac = 4;
	else if (height % 3 == 0)
	    yfac = 3;
	else if (height % 2 == 0)
	    yfac = 2;
	else
	    yfac = 1;

	// set xscale, yscale uniforms
	glUniform1i(uniform[2],xfac);
	glUniform1i(uniform[3],yfac);	

	// reduce size
	width = width/xfac;
	height = height/yfac;

	// and source texture coordinates
	texu *= 0.5f;
	texv *= 0.5f;

	// bind source texture
	glBindTexture(GL_TEXTURE_2D, minmaxtex[texsrc]);
	// and destination target
	if (texdest == 0)
	    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	else
	    glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

	// set viewport
	glViewport(0,0,width,height);

	// now draw quad
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(0, 0);
	glTexCoord2f(texu, 0); glVertex2f(1, 0);
	glTexCoord2f(texu, texv); glVertex2f(1, 1);
	glTexCoord2f(0, texv); glVertex2f(0, 1);	
	glEnd();

	nsteps++;

	// swap source and dest
	texsrc = 1-texsrc;
	texdest = 1-texdest;
    }

    // set resulting texture to texsrc, the one containing output
    minmaxresult = texsrc;

    // swap last and cur minmax
    for (int i=0; i<4; i++)
	lastMinmax[i] = curMinmax[i];

    // read in new values
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, (GLvoid*)curMinmax);

    glPopAttrib();

    if (g_Opts->dbg.printRender)
	printf("Found select min/max of %g,%g\n",curMinmax[1],curMinmax[3]);

    glUseProgram(shaderprog[P_LOGSCALE]);
    // update selected range immediately:
    // make sure these values are valid, else nothing selected
    if (curMinmax[3] > 0)
	glUniform2f(uniform[1], curMinmax[1], log(curMinmax[3]/curMinmax[1]));
    else
	glUniform2f(uniform[1], -1, 1);


    // and if we've waited a really long time, update immediately
    if (timeSince > 1000)
    {
	for (int i=0; i<4; i++)
	    lastMinmax[i] = curMinmax[i];

	// and update uniform variables for min/max, in shader
	glUniform2f(uniform[0], curMinmax[0], log(curMinmax[2]/curMinmax[0]));
    }

    glUseProgram(0);
}

// draws buffered data to the screen
void Render::renderToScreen()
{
    // set up for orthogonal drawing
    setOrtho();

    // set log shader
    glUseProgram(shaderprog[P_LOGSCALE]);

    // set maximum velocity dispersion
    glUniform1f(uniform[8], 500);

    glDisable(GL_BLEND);

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

    // prepare texture for rendering
    // and bind color table texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rendertex);
    glEnable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, colortex);
    glEnable(GL_TEXTURE_2D);

    // also set uniforms, to tell it where we bound which texture
    glUniform1i(uniform[9], 0);
    glUniform1i(uniform[10], 1);

    glBegin(GL_QUADS);
    
    // draw quad on entire screen
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(1, 0);
    glTexCoord2f(1, 1); glVertex2f(1, 1);
    glTexCoord2f(0, 1); glVertex2f(0, 1);

    glEnd();

    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
}

// sets selection to given vector of pids
void Render::SetSelect(std::vector<uint32_t>& pts)
{
    int count = pts.size();

    if (count > MAX_NUM_SELECTED)
	count = MAX_NUM_SELECTED;

    printf("Selecting %d points.\n", count);

    // sort the subid array
    std::sort(pts.begin(), pts.end());

    void* data = malloc(SELTEXDIM*SELTEXDIM*4);
    memcpy(data,&pts[0],4*count);

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, selectedtex);
    // store each pid as a 4-vector of bytes
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA_FLOAT32_ATI, 
		 SELTEXDIM, SELTEXDIM, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid*)data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // and now we need to set the uniform for point count
    glUseProgram(shaderprog[P_PSPRITE]);
    // set count first
    glUniform1i(ptuniform[14], count);

    glUseProgram(0);

    // if we didn't select anything, clear selection values
    if (count == 0)
	ClearSelect();

    free(data);
}

// returns world vectors of given screen coords
void Render::Unproject(int x, int y, Vector3& start, Vector3& dir)
{
    // make sure we have correct look-at matrix
    setLookAt();

    // and perspective transform
    setPerspective();

    GLdouble modelMatrix[16];
    GLdouble projMatrix[16];
    int viewport[4];

    GLdouble pos[3];

    glGetDoublev(GL_MODELVIEW_MATRIX,modelMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX,projMatrix);
    glGetIntegerv(GL_VIEWPORT,viewport);

    // adjust for opengl silliness
    y = screen->h - y;

    gluUnProject(x,y,1.0,modelMatrix,projMatrix,viewport,pos,pos+1,pos+2);
    
    Vector3 targ, up;
    g_State->GetWorldVectors(start, targ, up);

    Vector3 end(pos[0],pos[1],pos[2]);

    // and set proper direction vector
    dir = end-start;
}

// clears current selection
void Render::ClearSelect()
{
    curMinmax[1] = lastMinmax[1] = 1e30;
    curMinmax[3] = lastMinmax[3] = 0;

    glUseProgram(shaderprog[P_PSPRITE]);
    // set selected count to 0
    glUniform1i(ptuniform[14], 0);

    glUseProgram(0);
}
