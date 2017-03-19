/* This class contains a lookup table of uint64_t's, which are just
   the bits spread out by threes, that is, 1101 -> 1001000001 */

#ifndef _TREEINDEX_H_
#define _TREEINDEX_H_

#include "Formats.h"

#define NBITS 10
#define NSIZE (1<<NBITS)

void BuildTreeLookup();
uint64_t GetCoord(double x, double y, double z);

#endif
