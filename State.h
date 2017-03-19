#ifndef _STATE_H_
#define _STATE_H_

#include "Globals.h"
#include "Keyframes.h"
#include "Vec.h"

/* First, define individual sub-state enums. */

enum AnimState
{
    AS_IDLE, // time is static
    AS_PLAY, // playing, as a function of real-time
    AS_REC   // recording, updated frame-by-frame
};

enum TargetState
{
    TS_WORLD, // camera locked to world coords
    TS_TREE, // camera locked to tree coords (e.g. center of bounding box)
    TS_HALO   // camera locked to a single halo
};

enum InputState
{
    IS_IDLE, // mouse is doing nothing
    IS_LOOK, // right mouse, looking
    IS_ZOOM, // both mice, zooming
    IS_DRAG  // left mouse, dragging
};


class State
{
    AnimState animState;
    TargetState targetState;
    InputState inputState;


    // quit flag
    bool doQuit;

    // a factor that sets how pretty we want everything to look
    double renderQuality;

    // camera position, **relative to target**
    // needs to be transformed when we update target
    Vector3 position;
    // and the target
    Vector3 target;
    // our "up" vector, should be part of config
    Vector3 up;


    // the current time
    double curTime;


    // variables used by rest of the program
    // ugh these variable names are ugly
    double curDt;
    double curDloga;
    int curSnap;


    // for private use, gives the limits of log(a)
    double minTime;
    double maxTime;


    // these are for animation
    double playSpeed;

    // and flag for recording
    bool saveRecord;


    // internal timing, in ms
    int lastUpdate;




    // for input, mouse down x,y for left button
    int startx, starty;
    int buttons[3];
    // for special keys
    bool altKey;
    bool shiftKey;


    // internal selected pid list
    std::vector<uint32_t> selectedPoints;


    // vars for subhalo tracking
    // snap that tracked halo came from
    int curTrackSnap;
    Halo curTrackHalo;


    // private click or drag handling functions (for convenience)
    void click(int x, int y);
    void drag(int x, int y, int dx, int dy);



    // keyframing subclass
    Keyframes keyframes;
   



    // what to do if time changes
    void changeTime(double dt);

    // what to do if time changes
    void setTime(double time);

    // position update functions (passed as delta pixels)
    void lookView(int dx, int dy);
    void zoomView(int dz);


    // selection functions
    void clearSelect();
    void addSelect(int x, int y);
    void viewSelect(int x, int y);

    // view transform function
    void setTargetState(TargetState newTarget);


public:

    State();

    // sets reasonable initial values, based on block info
    void Initialize();

    // sets quit flag
    void Quit();

    // updates internal variables each timestep
    void Update();



    // sets dynamic playback/recording
    void Play(double speed);
    void Stop();
    void Record(bool save); // bool to see if we save while recording
    // plays at given speed only if not currently playing
    // otherwise, stops
    void TogglePlay(double speed);
    // adds keyframe to internal storage
    void AddKeyframe();


    // moves time this way or that by snapshot increments
    void ChangeSnap(int dt);
    void ToFirstSnap();
    void ToLastSnap();


    // sets the internal quality value
    void SetQuality(double qual)
    { renderQuality = qual; }



    // public input functions
    void MouseDown(int x, int y, int buttons);
    void MouseUp(int x, int y, int buttons);
    void MouseMove(int x, int y, int dx, int dy, int buttons);

    void KeyDown(int key);
    void KeyUp(int key);





    // accessors

    // returns camera vectors in world coords
    void GetWorldVectors(Vector3& pos, Vector3& targ, Vector3& up);

    // returns quality, scaled by optional factors
    double GetQuality();

    // similar to above, returns an 'ideal' frame render time
    double GetDesiredFrameTime();

    // this one returns "time smear", the sigma, based on play speed
    double GetTimeSigma();

    bool IsQuit()
    { return doQuit; }

    double GetTime()
    { return curTime; }

    double GetTime(double& dloga)
    {
	dloga = this->curDloga;
	return curTime;
    }

    double GetTime(double& dloga, double& dt, int& curSnap)
    {
	dloga = this->curDloga;
	dt = this->curDt;
	curSnap = this->curSnap;
	return curTime;
    }
};

#endif
