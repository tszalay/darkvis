#ifndef _CREATEBLOCKS_H_
#define _CREATEBLOCKS_H_

#include "Formats.h"

// Global defines
#define MAX_DEPTH 20

#define BIN_COUNT 4096

// evil macros!
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))


// ***************************************************************
// Function prototypes
// ***************************************************************
int GetMatchingCount(uint64_t block, int shift);
void CreateBlocks(uint64_t block, int shift, int depth);
void CreateLeaf(int depth, int blocknum, int count);
void ReadNextBlock();
void NextWriter();


void MergeBlock(int depth, int blocknum, int shift);
void MergeCurrentTime(int shift, int count, VertexB* PtsOut);
void MergeNextTime(int count, VertexB* PtsOut);

uint64_t WritePendingBlock(uint64_t block, int shift, int depth);
void deinterleave(uint64_t block, uint16_t pos[3]);
void mergecur(VertexB *a, VertexB *b);

void SetupBlocks();
void CleanupBlocks();

#endif
