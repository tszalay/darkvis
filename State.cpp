#include "Globals.h"
#include <iostream>
#include "quat.h"

using namespace std;


// updates time and other info
void State::Update()
{
    // time since, in seconds
    double dt = (SDL_GetTicks() - lastUpdate)*0.001;

    // animating? (do nothing if AS_IDLE)
    if (animState == AS_PLAY)
    {
	changeTime(dt * playSpeed * g_Opts->cam.tscale);

	// reached end, stop playing?
	if (playSpeed > 0 && curTime == maxTime)
	    Stop();
	else if (playSpeed < 0 && curTime == minTime)
	    Stop();
    }
    else if (animState == AS_REC)
    {
	// update by a fixed amount
	changeTime(g_Opts->view.recdt*g_Opts->cam.tscale);
	// check to stop?
	if (curTime == maxTime)
	    Stop();
	// and set rendering state
	Frame f = keyframes.GetValuesAtTime(curTime);
	g_Render->SetCurrentState(f.minmax,f.detail);
	// and tell it to save the damn image
	if (saveRecord)
	    g_Render->CaptureScreen();
    }

    lastUpdate = SDL_GetTicks();
}


// go all the way to beginning
void State::ToFirstSnap()
{
    // stop playing when time jumps
    Stop();
    setTime(minTime);
}


// or to end
void State::ToLastSnap()
{
    // stop playing when time jumps
    Stop();
    setTime(maxTime);
}



// advance snapshot number by a certain amount
void State::ChangeSnap(int dt)
{
    // stop playing when time jumps
    Stop();
    int snap = curSnap + dt;
    if (snap < 0) snap = 0;
    if (snap >= g_Blocks->GetSnapCount()) 
	snap = g_Blocks->GetSnapCount()-1;

    // and set the time
    setTime(g_Blocks->GetSnapTime(snap));
}


// play at a given speed (as a scale factor), can be negative
void State::Play(double speed)
{
    playSpeed = speed;
    animState = AS_PLAY;
}

// and stop
void State::Stop()
{
    animState = AS_IDLE;
}

// or toggle
void State::TogglePlay(double speed)
{
    if (animState == AS_IDLE)
    {
	Play(speed);
    }
    else if (animState == AS_PLAY)
    {
	Stop();
    }
}

// hhurngh record, but do we save images?
void State::Record(bool save)
{
    saveRecord = save;
    // stop
    if (animState == AS_REC)
    {
	animState = AS_IDLE;
	return;
    }
    // or go!
    if (keyframes.FrameCount() > 1)
	animState = AS_REC;
}


// we need to scale quality depending on what states are
double State::GetQuality()
{
    double fac = 1;

    // are we looking?
    if (inputState == IS_LOOK || inputState == IS_ZOOM)
	fac = g_Opts->view.camFactor;

    // but, are we also animating?
    if (animState == AS_PLAY && fac > g_Opts->view.animFactor)
	fac = g_Opts->view.animFactor;

    return renderQuality*fac; 
}

// this returns a per-frame render time (in secs) based on current quality
double State::GetDesiredFrameTime()
{
    double q = GetQuality();

    // now scale from min to max FPS based on quality
    // q is a value from 0...1

    double destfps = g_Opts->view.minFPS + (1-q)*(g_Opts->view.maxFPS-g_Opts->view.minFPS);

    // and return corresponding frame time
    return 1.0/destfps;
}

// this returns a 'time smear' sigma, as a dloga, by play speed
double State::GetTimeSigma()
{
    // the faster we're playing, the larger sigma we want to return
    if (animState == AS_PLAY)
	return 2*(g_Opts->cam.tscale*abs(playSpeed));
    else
	return 2*(g_Opts->cam.tscale);
}


// returns camera vectors in world coordinates
void State::GetWorldVectors(Vector3& pos, Vector3& targ, Vector3& up)
{
    // up is always the same
    up = this->up;

    // and bypass the entire thing if we're currently recording
    // since we just play from keyframes
    if (animState == AS_REC)
    {
	Frame f = keyframes.GetValuesAtTime(curTime);
	pos = f.pos;
	targ = f.targ;
	return;
    }

    if (targetState == TS_WORLD)
    {
	// no transform necessary
	pos = position;
	targ = target;
    }
    else if (targetState == TS_TREE)
    {
	// interpolate in moving tree coordinates
	double mins1[3], scales1[3], mins2[3], scales2[3];

	// get center of current and next tree
	g_Blocks->GetBlockCoords(curSnap, NULL, mins1, scales1);
	// last snap?
	if (curSnap+1 < g_Blocks->GetSnapCount())
	    g_Blocks->GetBlockCoords(curSnap+1, NULL, mins2, scales2);
	else
	    g_Blocks->GetBlockCoords(curSnap, NULL, mins2, scales2);

	// transform to centers
	for (int i=0;i<3;i++)
	{
	    mins1[i] += 0.5*scales1[i];
	    mins2[i] += 0.5*scales2[i];
	    mins1[i] = (mins1[i]*(1-curDt)+mins2[i]*curDt);
	}
	Vector3 origin(mins1[0],mins1[1],mins1[2]);

	pos = position + origin;
	targ = target + origin;
    }
    else if (targetState == TS_HALO)
    {
	// get time since halo set
	double dloga = curTime - g_Blocks->GetSnapTime(curTrackSnap);
	Vector3 haloPos = curTrackHalo.pos + curTrackHalo.vel * dloga 
	    + curTrackHalo.acc * (0.5 * dloga * dloga);

	pos = position + haloPos;
	targ = target + haloPos;
    }
}


// updates camera x and y by angle steps
void State::lookView(int dx, int dy)
{
    double xang = dx * g_Opts->cam.scale;
    double yang = dy * g_Opts->cam.scale;

    Vector3 pos = position - target;

    Vector3 axis = Vector3::Cross(pos,up);
    axis = Vector3::Normalize(axis);
    CQuat qrot(yang,axis);
    CQuat qrel(pos.x,pos.y,pos.z,0);
    qrel = qrot*qrel*qrot.Invert();
    qrot = CQuat(xang,up);
    qrel = qrot*qrel*qrot.Invert();
    Vector3 newpos(qrel.x,qrel.y,qrel.z);
    // make sure we didn't cross over vertical
    Vector3 naxis = Vector3::Cross(newpos,up);
    // these should point in same direction
    float v = Vector3::Dot(axis,naxis);
    // so we're ok
    if (v>0)
	position = newpos + target;
}

// zooms in/out
void State::zoomView(int dz)
{
    double zfac = dz*g_Opts->cam.zscale;
    position = (position-target)*(1.0f + zfac) + target;
}


// stores current info as a keyframe
void State::AddKeyframe()
{
    Frame f;
    Vector3 up;
    GetWorldVectors(f.pos,f.targ,up);
    g_Render->GetCurrentState(f.minmax,f.detail);
    f.time = curTime;
    keyframes.AddFrame(f);
}



// clears current selection
void State::clearSelect()
{
    g_Render->ClearSelect();
    selectedPoints.clear();
}


// selects points based on click location
void State::addSelect(int x, int y)
{
    Vector3 start, dir;

    // get world coords for click location
    g_Render->Unproject(x,y,start,dir);

    // find which halo we selected
    Halo selHalo = g_Select->SelectHalo(start, dir);

    // if we didn't select one?
    if (selHalo.pointIndex == -1)
	return;

    // otherwise, get points and add to vector
    std::vector<uint32_t> newSel = g_Select->GetHaloPoints(curSnap, selHalo);
    selectedPoints.insert(selectedPoints.end(), newSel.begin(), newSel.end());

    // and update
    g_Render->SetSelect(selectedPoints);
}


// sets view to focus on specified subhalo
void State::viewSelect(int x, int y)
{
    Vector3 start, dir;

    // get world coords for click location
    g_Render->Unproject(x,y,start,dir);

    // find which halo we selected
    Halo selHalo = g_Select->SelectHalo(start, dir);

    // first, unlock view
    setTargetState(TS_TREE);

    // if none selected, do nuthin
    if (selHalo.pointIndex == -1)
	return;

    // otherwise, set tracking vars
    curTrackHalo = selHalo;
    curTrackSnap = curSnap;

    setTargetState(TS_HALO);

    // and set origin to center of halo
    target = Vector3(0,0,0);
}


// changes camera frame
void State::setTargetState(TargetState newTarget)
{
    // need to change?
    if (newTarget == targetState)
	return;

    // get current vectors in world frame
    Vector3 pos, targ, up;
    GetWorldVectors(pos,targ,up);

    // change state
    targetState = newTarget;

    // set global position and target to 0
    position = Vector3(0,0,0);
    target = Vector3(0,0,0);
    // retrieve origin of new frame, in world coords
    Vector3 npos, ntarg, nup;
    GetWorldVectors(npos,ntarg,nup);
    
    // and now transform old ones back to new frame
    position = pos - npos;
    target = targ - ntarg;
}

// updates time and related variables
void State::changeTime(double dt)
{
    setTime(curTime + dt);
}

// updates time and related variables
void State::setTime(double time)
{
    curTime = time;

    int lastSnap = curSnap;

    if (curTime < minTime) curTime = minTime;
    if (curTime > maxTime) curTime = maxTime;

    curSnap = g_Blocks->GetSnapAt(curTime);
    double snaptime = g_Blocks->GetSnapTime(curSnap);
    this->curDloga = (curTime - snaptime);

    // next snap exists?
    if (curSnap+1 < g_Blocks->GetSnapCount())
	this->curDt = this->curDloga/(g_Blocks->GetSnapTime(curSnap+1)-snaptime);
    else
	this->curDt = 0;

    // update tracked halo as best we can, if snap changed
    if (targetState == TS_HALO && curSnap != lastSnap)
    {
	g_Select->UpdateHalo(curTrackSnap, curTrackHalo);
	// did we lose the track?
	if (curTrackSnap != curSnap)
	    setTargetState(TS_TREE);
    }
}

// or quit
void State::Quit()
{
    doQuit = true;
}

// actually initialize
void State::Initialize()
{
    lastUpdate = SDL_GetTicks();

    animState = AS_IDLE;
    targetState = TS_TREE;
    inputState = IS_IDLE;

    minTime = g_Blocks->GetSnapTime(0);
    maxTime = g_Blocks->GetSnapTime(g_Blocks->GetSnapCount()-1);
    printf("Time range is %g to %g.\n", minTime, maxTime);

    curTime = maxTime;
    curDloga = 0;
    curDt = 0;

    curSnap = g_Blocks->GetSnapCount()-1;

    renderQuality = 0.5;

    // set target to center of tree, position to corner
    double mins[3], scales[3];
    g_Blocks->GetBlockCoords(curSnap, NULL, mins, scales);

    position = Vector3(scales[0]/2,scales[1]/2,scales[2]/2);
    target = Vector3(0,0,0);
    up = Vector3(0,0,1);

    // key defaults
    buttons[0] = buttons[1] = buttons[2] = 0;
    altKey = false;
    shiftKey = false;
}


// initialize some default stuff
State::State()
{
    doQuit = false;
}
