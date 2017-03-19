#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>
#include <algorithm>

#include "CreateBlocks.h"
#include "PartFiles.h"
#include "Formats.h"
#include "TreeIndex.h"
#include "Process.h"

extern LargeWriter *writer;
extern VertexB* PendingBlocks[MAX_DEPTH][8];
extern int PendingCount[MAX_DEPTH][8];
extern uint64_t BlockChildLocation[MAX_DEPTH][8];
extern uint64_t BlockChildLength[MAX_DEPTH][8];
extern int16_t BlockChildFlags[MAX_DEPTH][8];
extern int16_t BlockChildFile[MAX_DEPTH][8];
extern int MAX_COUNT;
extern BlockFile *curBlockFile;


// input array into which we copy all pending blocks
VertexB* PtsIn;
// output array for writeblocks
OutVertex* OutPoints;


// a struct to hold rearranged PtsOut, for merging next
struct NextPt
{
    uint64_t coord; // octtree coord in *next* timestep
    double pos[3];
    VertexB* orig;

    bool operator< (const NextPt& b) const
    {
	return coord < b.coord;
    }
};

// for repeating procedure, just with the next position
NextPt* PtsNext;


void mergenext(VertexB *a, NextPt *b);



// Merges all of the children of specified block into one block
void MergeBlock(int depth, int blocknum, int shift)
{
    int count = 0;
    // first, copy input arrays to PtsIn
    for (int i=0; i<8; i++)
    {
	memcpy(PtsIn+count, PendingBlocks[depth+1][i], PendingCount[depth+1][i]*sizeof(VertexB));
	count += PendingCount[depth+1][i];
    }

    // now, allocate output array
    PendingCount[depth][blocknum] = MAX_COUNT;
    PendingBlocks[depth][blocknum] = (VertexB*)malloc(MAX_COUNT*sizeof(VertexB));
    VertexB* PtsOut = PendingBlocks[depth][blocknum];

    // alter shift, since we want to keep 12 bits in current block
    shift -= 12;

    /* do merging in current timestep */
    MergeCurrentTime(shift, count, PtsOut);
    
    double minpos[3], maxpos[3];
    for (int j=0;j<3;j++)
    {
	minpos[j] = 1e300;
	maxpos[j] = -1e300;
    }

    // now update positions in PtsIn, which don't matter
    // because we are never going to read from it again
    // anyway, update them to next timestep, and repeat
    for (int i=0; i<count; i++)
    {
	// note that inter-snap timescale is just as dt=1
	for (int j=0;j<3;j++)
	{
	    PtsIn[i].pos[j] += PtsIn[i].vel[j] + 0.5*PtsIn[i].acc[j];
	    minpos[j] = MIN(minpos[j], PtsIn[i].pos[j]);
	    maxpos[j] = MAX(maxpos[j], PtsIn[i].pos[j]);
	}
    }

    // and recompute bit indices, now that we have min/max of block
    for (int i=0; i<count; i++)
    {
	// if this was flagged as selected, it is already included in PtsOut
	if (PtsIn[i].coord == 0)
	    continue;

	PtsIn[i].coord = GetCoord( (PtsIn[i].pos[0]-minpos[0])/(maxpos[0]-minpos[0]),
				   (PtsIn[i].pos[1]-minpos[1])/(maxpos[1]-minpos[1]),
				   (PtsIn[i].pos[2]-minpos[2])/(maxpos[2]-minpos[2]));
    }

    // now do the same for PtsOut, but put reordered into PtsNext
    for (int i=0; i<MAX_COUNT; i++)
    {
	// set pointer to original point
	PtsNext[i].orig = PtsOut + i;
	// and update position by time
	for (int j=0; j<3; j++)
	    PtsNext[i].pos[j] = PtsOut[i].pos[j] + PtsOut[i].vel[j] + 0.5*PtsOut[i].acc[j];
	// and compute coord
	PtsNext[i].coord = GetCoord( (PtsNext[i].pos[0]-minpos[0])/(maxpos[0]-minpos[0]),
				     (PtsNext[i].pos[1]-minpos[1])/(maxpos[1]-minpos[1]),
				     (PtsNext[i].pos[2]-minpos[2])/(maxpos[2]-minpos[2]));
    }

    // alright, now we need to sort both arrays by coord
    std::sort(PtsIn, PtsIn+count);
    std::sort(PtsNext, PtsNext+MAX_COUNT);

    // now actually merge next timestep
    MergeNextTime(count, PtsOut);
}




// merges all properties at current timestep
// reads from PtsIn, outputs to PtsOut
void MergeCurrentTime(int shift, int count, VertexB* PtsOut)
{
    // number originally in bin
    int binold[BIN_COUNT];
    // number to use from bins
    int binnum[BIN_COUNT];
    // first index of point in a bin, in PtsIn
    int binstart[BIN_COUNT];

    // setup
    for (int t=0; t<BIN_COUNT; t++)
    {
	binold[t] = 0;
	binnum[t] = 0;
	binstart[t] = -1;
    }

    // populate bins
    for (int i=0; i<count; i++)
    {
	int bin = (PtsIn[i].coord >> shift) % BIN_COUNT;
	binnum[bin]++;
	binold[bin]++;
	if (binstart[bin] == -1)
	    binstart[bin] = i;
    }

    // rescale each bin by (nearly) sqrt
    int total = 0;
    for (int i=0;i<BIN_COUNT;i++)
    {
//	binnum[i] = round(pow(binnum[i],0.75));
	total += binnum[i];
    }

    double red = MAX_COUNT/(double)total;
    total = MAX_COUNT;

    // count down from desired total
    for (int i=0;i<BIN_COUNT;i++)
    {
	binnum[i] = (int)(binnum[i]*red);

	// make sure we are reducing
	if (binnum[i] > binold[i])
	    binnum[i] = binold[i];
	if (binnum[i] > total)
	    binnum[i] = total;

	total -= binnum[i];
    }

    // set bins with 0 selected to have at least 1
    for (int i=0; i<BIN_COUNT;i++)
    {
	if (total <= 0)
	    break;

	if (binold[i] > 0 && binnum[i] == 0)
	{
	    binnum[i]++;
	    total--;
	}
    }

    // if we have any left, randomly distribute
    while (total > 0)
    {
	int ind = rand()%BIN_COUNT;
	// make sure we are reducing
	if (binnum[ind] < binold[ind])
	{
	    binnum[ind]++;
	    total--;
	}
    }

    // index of output point
    int outIndex=0;
    int cmp=0;
    for (int t=0;t<BIN_COUNT;t++)
    {
	if (binnum[t] == 0) // empty bin
	    continue;

	int cnt = binold[t];

	// copy points into output array
	for (int i=0;i<binnum[t];i++)
	{
	    int rnd = rand() % cnt;
	    // put point into output
	    PtsOut[outIndex+i] = PtsIn[binstart[t]+rnd];

	    // copy last point into just selected one, to make my job easier
	    // (only if it isn't itself, duh)
	    if (rnd != cnt-1)
		PtsIn[binstart[t]+rnd] = PtsIn[binstart[t]+cnt-1];

	    // flag this point as removed, for next stage
	    PtsIn[binstart[t]+cnt-1].coord = 0;

	    cnt--;
	}

	VertexB *a;
	VertexB *b;
	// and now take care of remaining points:
	// merge each one with nearest point in output array
	a = PtsIn + binstart[t];
	for (int i=0;i<cnt;i++)
	{
	    double minDist = 1000;
	    int minIndex = -1;
	    b = PtsOut + outIndex;
	    for (int j=0;j<binnum[t];j++)
	    {
		double d = (a->pos[0]-b->pos[0])*(a->pos[0]-b->pos[0])
		    + (a->pos[1]-b->pos[1])*(a->pos[1]-b->pos[1])
		    + (a->pos[2]-b->pos[2])*(a->pos[2]-b->pos[2]);
		if (d<minDist)
		{
		    minDist = d;
		    minIndex = j;
		}
		cmp++;
		b++;
	    }
	    mergecur(a, PtsOut+outIndex+minIndex);
	    a++;
	} 
	// and step forward the out index
	outIndex += binnum[t];
    }
}




// merges all properties at next timestep
// reads from updated PtsIn and uses PtsNext to update PtsOut
void MergeNextTime(int count, VertexB* PtsOut)
{
    // we are going to merge each point in PtsIn with the point in
    // PtsOut that has the closest octtree coordinate. that's all.
    // our goal is to give away all of PtsIn's net next properties
    // to PtsNext->orig (in PtsOut).

    // index of output point
    int outIndex=0;
    for (int i=0; i<count; i++)
    {
	// skip over all the points that we have already put in PtsOut
	if (PtsIn[i].coord == 0)
	    continue;

	// advance outIndex to next point we want to merge with
	while (outIndex < MAX_COUNT-1 && PtsIn[i].coord > PtsNext[outIndex].coord)
	    outIndex++;

	// find spatially nearest point within a few
	int start = outIndex - 3;
	int end = outIndex + 3;
	if (start < 0) start = 0;
	if (end > MAX_COUNT) end = MAX_COUNT;

	int minIndex = -1;
	double minDist = 1e300;

	for (int j=start; j< end; j++)
	{
	    double d = (PtsIn[i].pos[0]-PtsNext[j].pos[0])*(PtsIn[i].pos[0]-PtsNext[j].pos[0])
		+ (PtsIn[i].pos[1]-PtsNext[j].pos[1])*(PtsIn[i].pos[1]-PtsNext[j].pos[1])
		+ (PtsIn[i].pos[2]-PtsNext[j].pos[2])*(PtsIn[i].pos[2]-PtsNext[j].pos[2]);
	    if (d<minDist)
	    {
		minDist = d;
		minIndex = j;
	    }
	}
	if (minIndex >= 0)
	    mergenext(PtsIn+i, PtsNext+minIndex);
    }
}






// merges the two points, leaving result in b
// really only adds relevant properties, the rest stay fixed
inline void mergecur(VertexB *a, VertexB *b)
{
    // don't merge positions, to avoid striping artifacts
    // and don't merge vels or acc's either

    // set largest hsml we will use
    double maxhsml = sqrt((a->pos[0]-b->pos[0])*(a->pos[0]-b->pos[0])
		       + (a->pos[1]-b->pos[1])*(a->pos[1]-b->pos[1])
		       + (a->pos[2]-b->pos[2])*(a->pos[2]-b->pos[2]));

    // if it's already larger, do nothing
//    maxhsml = MAX(b->hsml, maxhsml);

//    double fac = a->densq / (b->densq + a->densq);

    // new hsml is point-point distance, plus stuff
    b->hsml = MAX(b->hsml, maxhsml+a->hsml);
//    b->hsml = fac*(maxhsml - b->hsml) + b->hsml;
//    b->hsml = b->densq*b->hsml + a->densq*maxhsml;

    // and add densities and (weighted) velocity dispersions
    b->densq = (a->densq + b->densq);
    b->vdisp = (a->vdisp + b->vdisp);

//    b->hsml /= b->densq;

    // the coord and pid are kept from original vertex
}

// merges the two points, leaving result in b
// really only adds relevant properties, the rest stay fixed
inline void mergenext(VertexB *a, NextPt *b)
{
    // don't merge positions, to avoid striping artifacts
    // and don't merge vels or acc's either

    // set hsml to largest distance we merge
    // set largest hsml we will use
    double maxhsml = sqrt((a->pos[0]-b->pos[0])*(a->pos[0]-b->pos[0])
		       + (a->pos[1]-b->pos[1])*(a->pos[1]-b->pos[1])
		       + (a->pos[2]-b->pos[2])*(a->pos[2]-b->pos[2]));

    // if it's already larger, do nothing
    //maxhsml = MAX(b->orig->nhsml, maxhsml);

//    double fac = a->ndensq / (b->orig->ndensq + a->ndensq);

    // new hsml is point-point distance, plus stuff
    b->orig->nhsml = MAX(b->orig->nhsml, maxhsml+a->nhsml);
//    b->orig->nhsml = fac*(maxhsml - b->orig->nhsml) + b->orig->nhsml;
//    b->orig->nhsml = b->orig->nhsml*b->orig->ndensq + maxhsml*a->ndensq;

    // don't forget to add ones for next timestep too
    b->orig->ndensq = (a->ndensq + b->orig->ndensq);
    b->orig->nvdisp = (a->nvdisp + b->orig->nvdisp);

//    b->orig->nhsml /= b->orig->ndensq;
}








void SetupBlocks()
{
    PtsIn = (VertexB*)malloc(8*MAX_COUNT*sizeof(VertexB));
    PtsNext = (NextPt*)malloc(MAX_COUNT*sizeof(NextPt));
    OutPoints = (OutVertex*)malloc(MAX_COUNT*sizeof(OutVertex));
}

void CleanupBlocks()
{
    free(PtsIn);
    free(PtsNext);
    free(OutPoints);
}
