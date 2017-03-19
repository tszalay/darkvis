#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "Render.h"
#include "State.h"
#include "UI.h"
#include "BlockManager.h"
#include "BlockPriority.h"
#include "SubSelect.h"

#define CFGFILE "config.ini"
#define DATFILE "data.ini"

/*
 * struct for camera options
 */
struct CameraOpts
{
    float zscale; // zoom scaling factor
    float scale; // horizontal pan scaling factor
    float tscale; // time scaling factor, dloga/sec
};

/*
 * struct for format options
 */
struct FileOpts
{
    // format specs
    int vertexSize; // size of each vertex, in bytes
    // file location info
    int ndirs;
    string *dirs;
    // snapshot info
    int firstSnap;
    int lastSnap;
    int interval;
};

/*
 * system options, relating to current comp config
 * this is kinda sparse, dur
 */
struct SystemOpts
{
    uint32_t maxBytes; // memory usage limit of program
};

/*
 * view options, for viewing settings
 * does not store current state
 */
struct ViewOpts
{
    // don't draw blocks smaller than this many pixels on screen
    int minBlockPixels;
    // 0..1 quality factor when camera is panning
    float camFactor;
    // ditto for time changing
    float animFactor;

    // the smallest min. we can use
    float forceMin;

    // maximum framerate at lowest quality
    float maxFPS;
    // min ditto
    float minFPS;

    // recording time increment, in sec
    float recdt;
};

/*
 * debug options, for what to print
 */
struct DebugOpts
{
    bool printBlocks; // print block loading info?
    bool printPrior; // print priority queue info?
    bool printRender; // print rendering info
};



struct GlobalOpts
{
    CameraOpts cam;
    FileOpts file;
    SystemOpts sys;
    DebugOpts dbg;
    ViewOpts view;

    GlobalOpts();
    ~GlobalOpts();

// open system/file settings
    bool OpenConfig(string filename);
    bool OpenFile(string filename);

    void SaveSystem();

private:
    // path to config file
    string cfgFile;
    
    void defaults();
};

/*
 * Contains static pointers to all of the master objects/managers
 */

extern Render *g_Render;
extern State *g_State;
extern UIMain *g_UI;
extern BlockManager *g_Blocks;
extern BlockPriority *g_Priority;
extern SubSelect *g_Select;

extern GlobalOpts *g_Opts;

// a helper function
string toString(int t);

#endif
