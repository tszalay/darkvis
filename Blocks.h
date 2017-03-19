#ifndef _BLOCKS_H_
#define _BLOCKS_H_

#include <stdint.h>
#include "Vec.h"

/* This is the format for a block header. Vertex data comes immediately after the header. */
struct __attribute__ ((__packed__)) Block
{
    uint16_t pos[3]; // position, from 0..65535
    uint16_t depth; // size is TreeSize/(1<<depth)
    uint32_t count; // number of elts in block
    uint64_t childLocation; // location in file of first child
    uint64_t childLength; // length in file of all children (bytes)
    int16_t childFile; // which file contains child?
    int16_t childFlags; // bitwise flags telling us which children exist
    float mins[9]; // vx, vy, vz, ax, ay, az, hsml, density, veldisp
    float scales[9]; // ditto
    Block *childPtr[8]; // pointers to children in memory
};

/* This is a block descriptor used by the priority class. */
struct BlockInfo
{
    Block* block; // pointer dur
    Block* parent; // parent of block, if exists
    int snap; // snapshot index (not actual snapshot number!)
    double score; // evaluated score of this block
};

/* Format of a halo, as stored in custom halo table files. */
struct __attribute__ ((__packed__)) Halo
{
    int32_t fofGroup; // the fof group index of this halo, in current snapshot
    int32_t mainProgenitor; // the main progenitor index in prev. snapshot
    int32_t mainDescendant; // main descendant in next snapshot
    // starting point index (not in bytes, just index) and count
    int32_t pointIndex;
    int32_t pointCount;

    // now here come the actual halo properties
    Vector3 pos;
    Vector3 vel;
    Vector3 acc;

    float radius;
};


#endif
