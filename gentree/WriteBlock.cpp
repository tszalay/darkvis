#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>
#include "CreateBlocks.h"
#include "PartFiles.h"
#include "Formats.h"

extern LargeWriter *writer;
extern VertexB* PendingBlocks[MAX_DEPTH][8];
extern int PendingCount[MAX_DEPTH][8];
extern uint64_t BlockChildLocation[MAX_DEPTH][8];
extern uint64_t BlockChildLength[MAX_DEPTH][8];
extern int16_t BlockChildFlags[MAX_DEPTH][8];
extern int16_t BlockChildFile[MAX_DEPTH][8];
extern int MAX_COUNT;
extern BlockFile *curBlockFile;

extern OutVertex* OutPoints;

// Outputs specified pending block to a file, and frees up associated memory
// (returns # of bytes written)
uint64_t WritePendingBlock(uint64_t block, int shift, int depth)
{
    OutBlock head;
    int blocknum = block&7;

    // first, give it a count and child location info
    head.count = PendingCount[depth][blocknum];
    int count = head.count;

    head.childLocation = BlockChildLocation[depth][blocknum];
    head.childLength = BlockChildLength[depth][blocknum];
    head.childFlags = BlockChildFlags[depth][blocknum];
    head.childFile = BlockChildFile[depth][blocknum];

    // and set the pointers to 0, for when we load it into mem
    for (int i=0; i<8; i++)
	head.childPtr[i] = 0;

    // now compute x, y, z
    // our shift factor is depth*3
    deinterleave(block << shift, head.pos);
    // and also size, which we just set as is
    head.depth = depth;

    VertexB *data = PendingBlocks[depth][blocknum];


    // ****** important ******
    // now we want to do log compression of densities and (weighted) dispersions
    for (int i=0; i < count; i++)
    {
	data[i].densq = log(data[i].densq);
	data[i].vdisp = log(data[i].vdisp);
	data[i].ndensq = log(data[i].ndensq);
	data[i].nvdisp = log(data[i].nvdisp);
    }

    // initialize maxes and mins to outlandish values
    for (int i=0; i<9; i++)
    {
	head.mins[i] = 1e30;
	head.scales[i] = -1e30;
    }

    // now we need to set the mins / scales
    for (int i=0; i < count; i++)
    {
	for (int j=0; j<3; j++)
	{
	    head.mins[j] = MIN(head.mins[j], (float)data[i].vel[j]);
	    head.mins[j+3] = MIN(head.mins[j+3], (float)data[i].acc[j]);
	    head.scales[j] = MAX(head.scales[j], (float)data[i].vel[j]);
	    head.scales[j+3] = MAX(head.scales[j+3], (float)data[i].acc[j]);
	}
	head.mins[6] = MIN(head.mins[6], data[i].hsml);
	head.mins[7] = MIN(head.mins[7], data[i].densq);
	head.mins[8] = MIN(head.mins[8], data[i].vdisp);
	// use same scale for current and next densq,vdisp,hsml
	head.mins[6] = MIN(head.mins[6], data[i].nhsml);
	head.mins[7] = MIN(head.mins[7], data[i].ndensq);
	head.mins[8] = MIN(head.mins[8], data[i].nvdisp);	

	head.scales[6] = MAX(head.scales[6], data[i].hsml);
	head.scales[7] = MAX(head.scales[7], data[i].densq);
	head.scales[8] = MAX(head.scales[8], data[i].vdisp);
	// ditto
	head.scales[6] = MAX(head.scales[6], data[i].nhsml);
	head.scales[7] = MAX(head.scales[7], data[i].ndensq);
	head.scales[8] = MAX(head.scales[8], data[i].nvdisp);
    }

    // change from max to scale
    for (int i=0; i<9; i++)
	head.scales[i] -= head.mins[i];

    double minpos[3];
    double scale[3];
    // set minpos and scale using bounds on current snapshot
    for (int j=0; j<3; j++)
    {
	scale[j] = curBlockFile->scale[j] / (1<<depth);
	minpos[j] = curBlockFile->pos[j] 
	    + curBlockFile->scale[j]*head.pos[j]/65536.0;
    }

    for (int i=0; i < count; i++)
    {
	OutPoints[i].pid = (uint32_t)data[i].pid;

	for (int j=0; j<3; j++)
	{
	    OutPoints[i].pos[j] = (uint16_t)( 65535.99*(data[i].pos[j]-minpos[j])/scale[j]);
	    OutPoints[i].vel[j] = (uint16_t)( 65535.99*(data[i].vel[j]-head.mins[j]) / head.scales[j]);
	    OutPoints[i].acc[j] = (uint16_t)( 65535.99*(data[i].acc[j]-head.mins[j+3]) / head.scales[j+3]);
	}	
	// these two are only 8-bit
	OutPoints[i].hsml = (uint8_t)( 255.999*(data[i].hsml-head.mins[6]) / head.scales[6]);
	OutPoints[i].nhsml = (uint8_t)( 255.999*(data[i].nhsml-head.mins[6]) / head.scales[6]);
	// back to 16-bit
	OutPoints[i].densq = (uint16_t)( 65535.99*(data[i].densq-head.mins[7]) / head.scales[7]);
	OutPoints[i].vdisp = (uint16_t)( 65535.99*(data[i].vdisp-head.mins[8]) / head.scales[8]);
	OutPoints[i].ndensq = (uint16_t)( 65535.99*(data[i].ndensq-head.mins[7]) / head.scales[7]);
	OutPoints[i].nvdisp = (uint16_t)( 65535.99*(data[i].nvdisp-head.mins[8]) / head.scales[8]);
    }

    // write header
    writer->Write(&head, sizeof(OutBlock));
    // and write points
    writer->Write(OutPoints, sizeof(OutVertex)*count);

    // free the pending blocks
    free(data);

    // return bytes written
    return sizeof(OutBlock) + sizeof(OutVertex)*count;
}

inline void deinterleave(uint64_t block, uint16_t pos[3])
{
    pos[0] = pos[1] = pos[2] = 0;
    // we need to get rid of a few bits (4*3, actually)
    block >>= 12;
    for (int i=0; i<16; i++)
    {
	pos[0] |= ((block&1) << i);
	block >>= 1;
	pos[1] |= ((block&1) << i);
	block >>= 1;
	pos[2] |= ((block&1) << i);
	block >>= 1;	
    }    
}
