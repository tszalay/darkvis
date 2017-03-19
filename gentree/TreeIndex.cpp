#include "TreeIndex.h"

/* Stupid C++ array definition... */

uint64_t lookup[NSIZE] = {0};

/* Builds lookup table recursively */

void BuildTreeLookup()
{
    int cur = 1;
    lookup[0] = 0;
    for (int n=0; n<NBITS; n++)
    {
	for (int i=0; i<cur; i++)
	    lookup[i+cur] = lookup[i] + (1 << (3*n));
	cur <<= 1;
    }
}


/* Returns the octtree coordinate, given x,y,z in (0,1)x(0,1)x(0,1) */

uint64_t GetCoord(double x, double y, double z)
{
    uint64_t coord = 0;
    // scale up first, to check range
    x *= NSIZE * NSIZE;
    y *= NSIZE * NSIZE;
    z *= NSIZE * NSIZE;
    // now check range
    if (x < 0) x = 0;
    if (x >= NSIZE*NSIZE) x = (NSIZE*NSIZE)-1;
    if (y < 0) y = 0;
    if (y >= NSIZE*NSIZE) y = (NSIZE*NSIZE)-1;
    if (z < 0) z = 0;
    if (z >= NSIZE*NSIZE) z = (NSIZE*NSIZE)-1;
    // now do lookups and combine
    coord += lookup[(int)(x/NSIZE)];
    coord += (lookup[(int)(y/NSIZE)]<<1);
    coord += (lookup[(int)(z/NSIZE)]<<2);
    // shift whole thing up
    coord <<= NBITS*3;
    // and do bottom half
    coord += lookup[((int)x) % NSIZE];
    coord += (lookup[((int)y) % NSIZE]<<1);
    coord += (lookup[((int)z) % NSIZE]<<2);

    return coord;
}
