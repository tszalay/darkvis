#include "Keyframes.h"

bool operator<(const Frame& a, const Frame& b) {
    return a.time < b.time;
}

Frame Keyframes::GetValuesAtTime(double time)
{
    // base cases
    if (time < frames.front().time)
	return frames.front();
    else if (time > frames.back().time)
	return frames.back();

    Frame f;
    f.time = time;

    int n = (int)frames.size();

    if (n < 2)
	return frames.front();

    // or zomginterpolation
    for (int i=0; i<n; i++)
    {
	if (time < frames[i].time)
	{
	    // time sits between i-1, i
	    // do we need extra control point?
	    Frame f0;
	    Frame f1 = frames[i-1];
	    Frame f2 = frames[i];
	    Frame f3;

	    if (i > 1)
	    {
		f0 = frames[i-2];
	    }
	    else
	    {
		// create fake front control point
		f0.pos = f1.pos*2 - f2.pos;
		f0.targ = f1.targ*2 - f2.targ;
	    }

	    if (i < (n-1))
	    {
		f3 = frames[i+1];
	    }
	    else
	    {
		// create fake back control point
		f3.pos = f2.pos*2 - f1.pos;
		f3.targ = f2.targ*2 - f1.targ;
	    }

	    // now actually compute interpolations
	    // dt goes from 0...1
	    double dt = (time-f1.time)/(f2.time-f1.time);
	    // catmull-rom the vectors
	    f.pos = catmullRom(dt,f0.pos,f1.pos,f2.pos,f3.pos);
	    f.targ = catmullRom(dt,f0.targ,f1.targ,f2.targ,f3.targ);
	    // and log-interpolate the min/max
	    for (int j=0; j<4; j++)
		f.minmax[j] = exp(log(f1.minmax[j])*(1-dt)+log(f2.minmax[j])*dt);
	    f.detail = f1.detail*(1-dt) + f2.detail*dt;

	    return f;
	}
    }

    // how could this happen?
    return frames.back();
}

// returns interpolated vector values
Vector3 Keyframes:: catmullRom(double t, Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3)
{
    double t2 = (t*t); 
    double t3 = t*t2;
    return ((p1*2) + (p2 - p0) * t +
	    (p0*2 - p1*5 + p2*4 - p3) * t2 + 
	    (-p0 + p1*3 - p2*3 + p3) * t3)*0.5;
}
