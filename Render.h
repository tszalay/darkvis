#ifndef _RENDER_H_
#define _RENDER_H_

#include <GL/gl.h>
#include "SDL.h"
#include "Blocks.h"
#include <vector>

using namespace std;

// # of fragment/vertex shaders, programs
#define NFRAGS 4
#define NVERTS 1
#define NPROGS 4
// # of uniform variables
#define NUNI 11
#define NPUNI 15
// # of attr. vars
#define NATTR 7

#define MMTEX 2
#define MMFAC 4

enum PROGS
{
    P_LOGSCALE=0,
    P_MINMAX=1,
    P_MINMAXSTART=2,
    P_PSPRITE=3
};


/***** SHADER SETTINGS *****/
#define MAX_POINT_SIZE 100
#define PSSTR "100"
#define SELTEXDIM 512
#define SELTEXDIM_STR "512"
#define MAX_NUM_SELECTED SELTEXDIM*SELTEXDIM


/*
 * Does the rendering. Not much more to it than that, really.
 * Also computes min/max of screen for log scaling, and so on,
 * as well as framerate timing and speed/LOD adjustments to keep
 * program responsive.
 */
class Render
{
private:
    // duh
    SDL_Surface *screen;



    // this is the main framebuffer (offscreen object)
    GLuint renderbuffer;

    // this is the associated texture for the main fbo
    GLuint rendertex;

    // and this is the one for computing maximum intensity values
    GLuint minmaxbuffer;

    // and these are all the storage objects
    GLuint minmaxtex[MMTEX];

    // and this holds the result index into above array
    int minmaxresult;



    // this is the color table texture
    GLuint colortex;



    // this is the texture to hold a list of selected points
    GLuint selectedtex;



    // the vertex/fragment shaders and programs
    GLuint fragshader[NFRAGS];
    GLuint vertshader[NVERTS];
    GLuint shaderprog[NPROGS];

    // and locations of uniform variables
    GLint uniform[NUNI];
    GLint ptuniform[NPUNI];

    // and attributes
    GLint attribute[NATTR];



    // other timing
    int lastMinmaxCheck;
    static const int minmaxTime = 300;

    // interpolating minmax values, goes from last to cur in interptime
    float lastMinmax[4]; // interpolating from
    float curMinmax[4]; // interpolating to
    // can use lastMinmaxCheck with this for interpolation
    static const int minmaxInterpTime = 300;


    // FPS stuff
    int lastFPSCheck;
    float lastFPS;
    int curFPS;


    // rendering settings
    bool viewBoxes; // draw bounding boxes?
    bool viewPoints; // draw points?
    bool takeScreenshot; // take screencap @ next frame

    // current screenshot index, for filename
    int curScreenshotIndex;

    // scaling factor for block scores, to adjust how many we draw
    // higher means fewer blocks
    double curScoreScale;

    // are we currently automatically scaling detail?
    bool autoScale;
    // did we actually clip any blocks' score?
    bool clippedBlocks;



    // rendering constants
    static const double nearPlane = 0.001;
    static const double farPlane = 1000;
    static const double aspect = 4.0/3.0;
    static const double fov = 60;





    // sets up opengl view matrix etc
    void setPerspective();

    // sets up opengl view matrix etc
    void setOrtho();

    // sets the modelview matrix based on state info
    void setLookAt();



    // generate framebuffer objects
    void genBuffers();

    // checks status of current fbo
    void checkFrameBuffer();

    // loads and initializes shader objects
    void loadShaders();

    // prints the opengl shader infolog (for debugging)
    void printInfoLog(GLuint obj);

    // prints the opengl program infolog (for debugging)
    void printProgramLog(GLuint obj);

    // precomputes scaling factors for point sprites
    void computeScaleFactors();

    // loads in the color cube table and makes a texture for it
    void loadColorTable();



    // returns world<->pixel scale factor at unit distance
    double getPixelScale();
public:
    // returns score of a certain block, much like block priority's scorer
    double scoreBlock(int snap, Block *block);
private:
    // updates the internal scaling factor
    void updateScoreScale(double lasttime);



    // Statistics
    void updateFPS();




    // uses logarithmic reduction to quickly get min/max
    void getMinMax();




    // main function to start point-sprite drawing
    void drawBlocks();

    // recursive function to draw boxes
    int drawBoxesRec(int snap, Block* block);




    // draws buffered texture to backbuffer, and swaps
    void renderToScreen();

    // saves screen image to file
    void saveBuffer();


public:
    Render();
    ~Render();

    // gets called to set up screen and render params
    bool Init();

    // cleans up stuff
    void Cleanup();

    // redraws everything
    void Draw();

    // does all work necessary to resize display
    void Resize(int width, int height);

    // saves a screenshot
    void CaptureScreen() { takeScreenshot = true; }

    // sets selected IDs list to vector
    void SetSelect(std::vector<uint32_t>& pts);

    // clears selection
    void ClearSelect();

    // returns vectors pointing into world
    void Unproject(int x, int y, Vector3& start, Vector3& dir);

    // toggles stuff
    void ToggleBoxes() { viewBoxes = !viewBoxes; }
    void TogglePoints() { viewPoints = !viewPoints; }

    // starts autoscaling
    void ToggleAutoscale() { autoScale = !autoScale; }

    // changes render quality
    void ChangeScoreScale(double fac);

    // returns all internal values
    void GetCurrentState(float* cminmax, double& cscore)
    {
	for (int i=0;i<4;i++)
	    cminmax[i] = curMinmax[i];
	cscore = curScoreScale;
    }

    // or sets them (this is kinda evil, dur)
    void SetCurrentState(float* cminmax, double cscore)
    {
	for (int i=0;i<4;i++)
	{
	    // hijack minmax computations
	    curMinmax[i] = cminmax[i];
	    lastMinmax[i] = cminmax[i];
	    lastMinmaxCheck = SDL_GetTicks();
	}
	curScoreScale = cscore;
    }
};

#endif
