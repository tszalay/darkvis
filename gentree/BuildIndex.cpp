#include <cmath>
#include <cassert>
#include "PartFiles.h"
#include "Process.h"
#include "Loaders.h"
#include "TreeIndex.h"


BlockFile BuildIndex(SnapHeader *curSnap, SnapHeader *nextSnap, string filename)
{
    printf("Building index for snap %d...",curSnap->snap);
    fflush(stdout);

    // load input streams
    BufferedReader<VertexA> readCur(string(curSnap->filename));
    BufferedReader<VertexA> *readNext = NULL;
    bool hasNext = false;
    if (nextSnap != NULL)
    {
	readNext = new BufferedReader<VertexA>(string(nextSnap->filename));
	// set next snaps' reader to delete after read,
	// since we won't be needing that data anymore
	readNext->SetDelete(true);
	hasNext = true;
    }

    // create writer on output filename
    BufferedWriter<VertexB> writer(filename);
    writer.SetSort(true);

    VertexA vcur;
    VertexA vnext;
    VertexB vout;

    double scale[3];
    for (int i=0;i<3;i++)
	scale[i] = curSnap->maxpos[i]-curSnap->minpos[i];

    double dloga=1;
    if (hasNext)
	dloga = log(nextSnap->time) - log(curSnap->time);

    uint64_t npts = 0;
    int64_t lastpid = 0;

    // time halfway between two snapshots, for interpolatioon
    double tmid = dloga/2;

    while (readCur.CanRead())
    {
	vcur = readCur.Read();

	// set positions, duh
	for (int i=0;i<3;i++)
	    vout.pos[i] = vcur.pos[i];

	// combine with next vertex, if exists
	if (hasNext)
	{
	    vnext = readNext->Read();
	    assert(vcur.pid == vnext.pid);
	    // set accelerations so that point ends up at vnext.pos
	    // after dloga time, but passes through correct intermediate point
	    for (int i=0;i<3;i++)
	    {
		double x0 = vcur.pos[i];
		double v0 = vcur.vel[i];
		// this is volker's code, i have no idea what it does (besides interpolates)
		double dx = vnext.pos[i] - x0;
		double dv = vnext.vel[i] - v0;

		double dt = dloga;
		double t = tmid;

		double t2 = t*t;
		double t3 = t*t*t;

		double dt2inv = 1.0/((dt*dt)/2);
		double dt2invb = 1.0 / (3*dt*dt);

		double c = (3 * dx - (3 * v0 + dv) * dt) * dt2inv;
		double f = (dv - dt * c) * dt2invb;

		// now compute interpolated position halfway between snaps
		double dxmid = v0 * t + 0.5 * t2 * c + t3 * f;

		// now acceleration is easy to compute
		// ****** note that we are computing this with unit timestep *****
		// so that t=0 is cursnap, t=1 is nextsnap
		vout.acc[i] = 4*(dx - 2*dxmid);
		// and thus so is velocity
		vout.vel[i] = dx - 0.5*vout.acc[i];
	    }
	    // set particle's next dens, vdisp, and hsml
	    vout.nhsml = vnext.hsml;
	    vout.ndensq = vnext.densq;
	    vout.nvdisp = vnext.vdisp;	    
	    
	    readNext->Next();
	}
	else
	{
	    vout.nhsml = vcur.hsml;
	    vout.ndensq = vcur.densq;
	    vout.nvdisp = vcur.vdisp;

	    // zero accel, zero vel if last snapshot
	    for (int i=0;i<3;i++)
	    {
		vout.vel[i] = 0;
		vout.acc[i] = 0;
	    }
	}

//	vout.pid = vcur.pid;
	// reset pid's to go from 0...n-1
	vout.pid = npts;
	vout.hsml = vcur.hsml;
	vout.densq = vcur.densq;
	vout.vdisp = vcur.vdisp;

	// set octtree coord via scaled coordinates
	vout.coord = GetCoord(
	    (vcur.pos[0]-curSnap->minpos[0])/scale[0],
	    (vcur.pos[1]-curSnap->minpos[1])/scale[1],
	    (vcur.pos[2]-curSnap->minpos[2])/scale[2]
	    );

	// and write
	writer.Write(vout);

	readCur.Next();

	npts++;

	if (npts % 1000000 == 0)
	{
	    printf("%d million...",npts/1000000);
	    fflush(stdout);
	}
    }

    delete(readNext);

    // and clean up
    int numFiles = writer.Close();

    printf("\nIndexed %d points.\n",npts);
    fflush(stdout);

    // save # of files, for mergesort
    FILE *file = fopen(filename.c_str(),"wb");
    fwrite(&numFiles, sizeof(int), 1, file);
    fclose(file);

    // and return a blockfile object
    BlockFile bf;
    for (int i=0;i<3;i++)
    {
	bf.pos[i] = curSnap->minpos[i];
	bf.scale[i] = scale[i];
    }

    // use log-time for all of these
    bf.time = log(curSnap->time);
    bf.snapnum = curSnap->snap;
    
    return bf;
}
