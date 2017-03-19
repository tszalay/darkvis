#include "PartFiles.h"
#include "Process.h"
#include "Loaders.h"

#include <stdlib.h>
#include <cmath>
#include <cassert>


/* Loads each file from a snapshot, creates an
   interleaved format, does some basic processing on
   the file (and sorts by pid), and then saves it. 
   Returns a header containing info about the snapshot. */

SnapHeader Interleave(PathInfo& ps, string filename, int snap)
{
    printf("Loading snapshot %d...\n",snap);

    HsmlData hdata = LoadHsml(ps, snap, 0);
    VertexData vdata = LoadSnap(ps, snap, 0);

    if (hdata.numTotal != vdata.numTotals[1])
    {
	fprintf(stderr,"Particle counts do not match!\n");
	FreeHsml(hdata);
	FreeSnap(vdata);
	return SnapHeader();
    }

    uint64_t numTotal = hdata.numTotal;

    printf("Processing %lu particles...", (long unsigned int)numTotal);

    // make a header for each interleaved file
    SnapHeader head;
    head.numTotal = numTotal;
    head.mass = vdata.massParts[1];
    head.time = vdata.time;
    head.redshift = vdata.redshift;
    head.boxSize = vdata.boxSize;
    head.omega0 = vdata.omega0;
    head.omegaLambda = vdata.omegaLambda;
    head.hubbleParam = vdata.hubbleParam;
    head.snap = snap;

    head.SetFilename(filename);

    for (int i=0; i<3; i++)
    {
	head.minpos[i] = 1e300;
	head.maxpos[i] = -1e300;
    }

    // ******* Velocity scaling info *******
    // factor to convert velocity to dx/dloga:
    double vfac = 1.0 / (HUBBLE * sqrt(head.omega0 / pow(head.time,3)
				+ head.omegaLambda) * sqrt(head.time));

    BufferedWriter<VertexA> writer(filename);
    writer.SetSort(true);

    uint32_t totalIndex = 0; // overall index

    uint32_t vIndex = 0; // index into snap file
    uint32_t hIndex = 0; // index into hsml file

    int hFileId = 0; // index of hsml file
    int vFileId = 0; // index of snap file

    VertexA tmp;

    while (totalIndex <  numTotal)
    {
	if (vIndex == vdata.numParts[1]) // load next input file
	{
	    vFileId++;
	    FreeSnap(vdata);
	    vdata = LoadSnap(ps, snap, vFileId);
	    vIndex = 0;
	    printf("Loaded snap part %d; ",vFileId);
	    fflush(stdout);
	}
	if (hIndex == hdata.numFile) // load next input file
	{
	    hFileId++;
	    FreeHsml(hdata);
	    hdata = LoadHsml(ps, snap, hFileId);
	    hIndex = 0;
	    printf("Loaded hsml part %d; ",hFileId);
	    fflush(stdout);
	}

	// and write vertex data (| is for > 32-bit particle count)
	tmp.pid = vdata.id[vIndex] | (((uint64_t)vdata.nLargeSims[1])<<32);
	assert(tmp.pid == vdata.id[vIndex]);
	for (int i=0;i<3;i++)
	{
	    tmp.pos[i] = vdata.pos[vIndex*3 + i];
	    // check  bounds
	    if (tmp.pos[i] > head.maxpos[i])
		head.maxpos[i] = tmp.pos[i];
	    if (tmp.pos[i] < head.minpos[i])
		head.minpos[i] = tmp.pos[i];
	    // convert velocity
	    tmp.vel[i] = vdata.vel[vIndex*3 + i] * vfac;
	}
	tmp.hsml = hdata.hsml[hIndex];
	tmp.densq = hdata.density[hIndex]*hdata.density[hIndex];
	// weight velocity dispersion by squared density, since
	// that is what we compute the sum of
	tmp.vdisp = hdata.velDisp[hIndex]*tmp.densq;

	writer.Write(tmp);

	totalIndex++;
	vIndex++;
	hIndex++;
    }

    int numFiles = writer.Close();

    // save # of files, for mergesort
    FILE *file = fopen(filename.c_str(),"wb");
    fwrite(&numFiles, sizeof(int), 1, file);
    fclose(file);

    printf("\nInterleaved snap %d\n",snap);
    fflush(stdout);

    FreeHsml(hdata);
    FreeSnap(vdata);

    return head;
}
