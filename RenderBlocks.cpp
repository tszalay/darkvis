#include <stdlib.h>
#include <iostream>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include "Globals.h"
#include "Render.h"

#include "SDL.h"


/* Sets up renderer and starts recursive draw. */
void Render::drawBlocks()
{
    // set up perspective transform
    setPerspective();

    // render points to offscreen buffer
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, renderbuffer);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // setup gl flags
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE,GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_POINT_SPRITE);

    glUseProgram(shaderprog[P_PSPRITE]);

    // bind the selecion texture and set the uniform
    glBindTexture(GL_TEXTURE_2D, selectedtex);
    glUniform1i(ptuniform[13], 0);

    // set the hsml scaling factor as a constant times the actual (correct) value
    glUniform1f(ptuniform[11], (float)g_State->GetQuality()*getPixelScale());

    // and get the current dt
    double curTime, dloga, dt;
    int curSnap;
    curTime = g_State->GetTime(dloga, dt, curSnap);
  
    glUniform1f(ptuniform[10], dt);

    if (g_Opts->dbg.printRender)
	printf("Rendering snap %d with dt = %f\n", curSnap, dt);

    // set view matrix to camera position
    setLookAt();

    // set this flag
    clippedBlocks = true;

    Block *root = g_Blocks->GetRoot(curSnap);
    // root loaded yet?
    if (root == NULL)
	return;

    // enable all the attribute arrays
    glEnableVertexAttribArray(attribute[0]);
    glEnableVertexAttribArray(attribute[1]);
    glEnableVertexAttribArray(attribute[2]);
    glEnableVertexAttribArray(attribute[3]);
    glEnableVertexAttribArray(attribute[4]);
    glEnableVertexAttribArray(attribute[5]);
    glEnableVertexAttribArray(attribute[6]);


    // and finally, draw

    int ndrawn = drawBoxesRec(curSnap, root);
    if (g_Opts->dbg.printRender)
	printf("%d points drawn recursively.\n",ndrawn);

    glDisableVertexAttribArray(attribute[0]);
    glDisableVertexAttribArray(attribute[1]);
    glDisableVertexAttribArray(attribute[2]);
    glDisableVertexAttribArray(attribute[3]);
    glDisableVertexAttribArray(attribute[4]);
    glDisableVertexAttribArray(attribute[5]);
    glDisableVertexAttribArray(attribute[6]);

    glDisable(GL_POINT_SPRITE);
}

/* Recursively draws blocks until threshold limit reached. */
int Render::drawBoxesRec(int snap, Block* block)
{
    // first draw the box
    // and recurse
    double mins[3];
    double scales[3];

    int ndrawn = 0;

    // only draw child blocks if score is favorable
    if (scoreBlock(snap, block)*curScoreScale > 1.0)
    {
	for (int i=0; i<8; i++)
	{
	    if (block->childPtr[i] != NULL)
		ndrawn += drawBoxesRec(snap,block->childPtr[i]);
	}
    }
    else
    {
	// we decided to clip, so set flag
	clippedBlocks = true;
    }

    // if we drew any child blocks we drew all, so return
    if (ndrawn > 0)
	return ndrawn;

    // otherwise, draw self

    g_Blocks->GetBlockCoords(snap, block, mins, scales);

    // floating point pos/scales
    float fmin[3], fscl[3];

    for (int i=0;i<3;i++)
    {
	fmin[i] = mins[i];
	fscl[i] = scales[i];
    }

    // set up position, color uniforms
    glUniform3fv(ptuniform[0], 1, fmin); // pmin
    glUniform2fv(ptuniform[4], 1, block->mins+7); // cmin

    glUniform3fv(ptuniform[5], 1, fscl); // pscl
    glUniform2fv(ptuniform[9], 1, block->scales+7); // cscl

    if (viewBoxes)
    {
	float zeros[3] = {0,0,0};
	// set velocity/accel to 0, so block stays put
	glUniform3fv(ptuniform[1], 1, zeros); // vmin
	glUniform3fv(ptuniform[2], 1, zeros); // amin

	glUniform1f( ptuniform[3], 0); // hmin
	glUniform1f( ptuniform[8], 0); // hscl

	// set actual vel/acc values to 0
	glVertexAttrib3f(attribute[1], 0, 0, 0);
	glVertexAttrib3f(attribute[2], 0, 0, 0);
	// also hsml
	glVertexAttrib2f(attribute[3], 0, 0);

	// draw using max density color/veldisp
	glVertexAttrib2f(attribute[4], 1, 1);
	// as well as for next frame
	glVertexAttrib2f(attribute[5], 1, 1);

	glBegin(GL_LINE_LOOP);
	glVertex3i(0,0,0);
	glVertex3i(1,0,0);
	glVertex3i(1,1,0);
	glVertex3i(0,1,0);
	glEnd();
	
	glBegin(GL_LINE_LOOP);
	glVertex3i(0,0,1);
	glVertex3i(1,0,1);
	glVertex3i(1,1,1);
	glVertex3i(0,1,1);
	glEnd();
	
	glBegin(GL_LINES);
	glVertex3i(0,0,0);
	glVertex3i(0,0,1);
	glVertex3i(1,0,0);
	glVertex3i(1,0,1);
	glVertex3i(1,1,0);
	glVertex3i(1,1,1);
	glVertex3i(0,1,0);
	glVertex3i(0,1,1);
	glEnd();
    }

    // set hsml unis
    glUniform1f( ptuniform[3], block->mins[6]); // hmin
    glUniform1f( ptuniform[8], block->scales[6]); // hscl

    // set velocity and accel uniforms
    glUniform3fv(ptuniform[1], 1, block->mins); // vmin
    glUniform3fv(ptuniform[2], 1, block->mins+3); // amin

    glUniform3fv(ptuniform[6], 1, block->scales); // vscl
    glUniform3fv(ptuniform[7], 1, block->scales+3); // ascl

    uint16_t* ptr = (uint16_t*)(block+1) + 2;

    if (viewPoints)
    {
	glEnableClientState(GL_VERTEX_ARRAY);

	glVertexAttribPointer(attribute[6], 4, GL_UNSIGNED_BYTE, GL_FALSE, g_Opts->file.vertexSize, ptr-2); // pid
	glVertexAttribPointer(attribute[0], 3, GL_UNSIGNED_SHORT, GL_TRUE, g_Opts->file.vertexSize, ptr); // pos
	glVertexAttribPointer(attribute[1], 3, GL_UNSIGNED_SHORT, GL_TRUE, g_Opts->file.vertexSize, ptr+3); // vel
	glVertexAttribPointer(attribute[2], 3, GL_UNSIGNED_SHORT, GL_TRUE, g_Opts->file.vertexSize, ptr+6); // acc
	glVertexAttribPointer(attribute[3], 2, GL_UNSIGNED_BYTE, GL_TRUE, g_Opts->file.vertexSize, ptr+9); // hsml bytes
	glVertexAttribPointer(attribute[4], 2, GL_UNSIGNED_SHORT, GL_TRUE, g_Opts->file.vertexSize, ptr+10); // cur color
	glVertexAttribPointer(attribute[5], 2, GL_UNSIGNED_SHORT, GL_TRUE, g_Opts->file.vertexSize, ptr+12); // next color

	glDrawArrays(GL_POINTS, 0, block->count);
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    
    return block->count;
}


/* Computes the on-screen score for a given block, used for thresholding. */
double Render::scoreBlock(int snap, Block* block)
{
    double score;

    double mins[3], scales[3];
    // get block space extents
    g_Blocks->GetBlockCoords(snap, block, mins, scales);
    // now compute their size on screen
    double radius = sqrt(scales[0]*scales[0]+scales[1]*scales[1]+scales[2]*scales[2]);

    for (int j=0; j<3; j++)
	scales[j] *= 0.5;

    Vector3 pos, targ, up;
    // gets the world-coordinate vectors
    g_State->GetWorldVectors(pos,targ,up);

    // center the block
    for (int j=0; j<3; j++)
	mins[j] += scales[j];

    // and offset the vector (negative of it, but who cares)
    pos.x -= (float)mins[0];
    pos.y -= (float)mins[1];
    pos.z -= (float)mins[2];

    // proportional to screen linear size of block
    score = radius/Vector3::Length(pos);
    // now screen area of block
      score = score*score;
    // divide by distance again
//      score /= sqrt(Vector3::Length(pos));

    return score;
}


/* Updates dynamic score scaling for FPS scaling. */
void Render::updateScoreScale(double lasttime)
{
    double targtime = g_State->GetDesiredFrameTime();

    if (fabs(lasttime-targtime)/targtime < 0.1)
	return;

    // if we didn't clip, we can't make it any slower
    if (!clippedBlocks && targtime > lasttime)
	return;

    if (curScoreScale < 0.0000001)
	curScoreScale = 0.00001;
    if (curScoreScale > 1e30)
	curScoreScale = 1e10;

    // scale by amount of time spent drawing
    double fac = (1+lasttime);

    if (lasttime > targtime)
	curScoreScale /= fac;
    else
	curScoreScale *= fac;
}

/* Manually increase/decreases render depth */
void Render::ChangeScoreScale(double fac)
{
    curScoreScale *= fac;
}
