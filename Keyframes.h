#ifndef _KEYFRAMES_H_
#define _KEYFRAMES_H_

#include "Vec.h"
#include <vector>
#include <algorithm>

// struct for a single keyframe of animation
// all values get interpolated
struct Frame
{
    // camera values
    Vector3 pos;
    Vector3 targ;
    // this one's kinda important too
    double time;
    
    // saved min/max values
    float minmax[4];

    // and render depth
    double detail;
};

bool operator<(const Frame& a, const Frame& b);

class Keyframes
{
    // vector of frames
    std::vector<Frame> frames;

    // interpolation functions
    Vector3 catmullRom(double t, Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3);

public:

    Keyframes()
    {
    }

    // returns interpolated values at specified time
    Frame GetValuesAtTime(double time);
    
    // deletes all keyframes
    void ClearFrames()
    {
	frames.clear();
    }

    // adds a frame, this is easy
    void AddFrame(Frame frame)
    {
	frames.push_back(frame);
	std::sort(frames.begin(),frames.end());
    }

    // gets number of frames
    int FrameCount()
    {
	return frames.size();
    }
};

#endif
