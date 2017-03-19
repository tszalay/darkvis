//#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "CreateBlocks.h"
#include "PartFiles.h"
#include "Formats.h"


// ***************************************************************
// The most important single variable
// ***************************************************************

int MAX_COUNT;




// ***************************************************************
// these structs point to processed blocks that we need to use for
// merging in any sort of depth-first traversal
// ***************************************************************

// points to previously completed blocks
VertexB* PendingBlocks[MAX_DEPTH][8];

// and their counts
int PendingCount[MAX_DEPTH][8];

// and gives the file pointers to the first child (for depth->block)
uint64_t BlockChildLocation[MAX_DEPTH][8];

// and the length of children, in file
uint64_t BlockChildLength[MAX_DEPTH][8];

// which children exist
int16_t BlockChildFlags[MAX_DEPTH][8];

// and which file they're in
int16_t BlockChildFile[MAX_DEPTH][8];


// ***************************************************************
// Contains current read buffer, sized 
// ***************************************************************

#define BUFFER_FAC 3
// CurrentBlock is BUFFER_FAC*MAX_COUNT
int CurrentCount;
VertexB* CurrentBlock;

// reader and writer classes
BufferedReader<VertexB> *reader;

// one writer for each file
LargeWriter **writers;
// and the current writer
LargeWriter *writer;
// and the index of the current writer
int curWriterIndex;
// total number of writers
int numWriters;

int totalNodes;
int totalLeaves;


// also current block info, for merging
BlockFile *curBlockFile;


// Recursively creates all the blocks (...woah)
void CreateBlocks(uint64_t block, int shift, int depth)
{
    int nmatch = GetMatchingCount(block, shift);
    int blocknum = block&7;

    // if it all fits into this block, we can create it nooo problem
    if (nmatch <= MAX_COUNT)
    {
	CreateLeaf(depth, blocknum, nmatch);
	// set child size pointer to 0
	BlockChildLocation[depth][blocknum] = 0;
	BlockChildLength[depth][blocknum] = 0;
	// set flags to 0, to indicate leaf node
	BlockChildFlags[depth][blocknum] = 0;
	BlockChildFile[depth][blocknum] = -1;
	// and read in next block of points from file
	ReadNextBlock();
	return;
    }

    // otherwise, we need to create all child blocks, unfortunately
    for (int i=0; i<8; i++)
	CreateBlocks((block<<3) + i, shift - 3, depth + 1);

    // and then merge to create the current one
    MergeBlock(depth, blocknum, shift);

    uint64_t length = 0;
    
    // set first child's pointer
    BlockChildLocation[depth][blocknum] = writer->GetLocation();

    // and file index
    BlockChildFile[depth][blocknum] = curWriterIndex;

    // zero flags
    BlockChildFlags[depth][blocknum] = 0;

    // and then write child blocks (also frees pendingblock mem)
    for (int i=0; i<8; i++)
    {
	// skip blocks of size 0
	if (PendingCount[depth+1][i] == 0)
	    continue;
	// make note of the fact that child block exists in file
	BlockChildFlags[depth][blocknum] |= (1<<i);
	// and write it
	length += WritePendingBlock((block<<3) + i, shift - 3, depth + 1);
    }
    BlockChildLength[depth][blocknum] = length;

    // and now advance to next writer
    NextWriter();

    totalNodes++;
    if (totalNodes%100 == 0)
    {
	printf("%d nodes, %d leaves...", totalNodes, totalLeaves);
	fflush(stdout);
    }

    return;
}


// Copies current vertices into a pending block (blocknum is 0-7)
void CreateLeaf(int depth, int blocknum, int count)
{
    // alloc and copy data
    PendingBlocks[depth][blocknum] = (VertexB*)malloc(count*sizeof(VertexB));
    memcpy(PendingBlocks[depth][blocknum], CurrentBlock, count*sizeof(VertexB));
    PendingCount[depth][blocknum] = count;

//    printf("Alloc'd %xd, size %d\n",PendingBlocks[depth][blocknum],count);
    
    // delete count points from CurrentBlock
    CurrentCount -= count;
    // use memmove in case of overlap
    memmove(CurrentBlock, CurrentBlock+count, CurrentCount*sizeof(VertexB));

    if (count > 0)
	totalLeaves++;
}

// Returns the number of matching vertices in current
int GetMatchingCount(uint64_t block, int shift)
{
    for (int i=0; i<CurrentCount; i++)
    {
	if ( (CurrentBlock[i].coord >> shift) != block )
	    return i;
	if (i > MAX_COUNT)
	    return i;
    }
    return CurrentCount;
}

// Reads in a block's worth of vertices
void ReadNextBlock()
{
    int maxn = MAX_COUNT*BUFFER_FAC;
    // read in as many so as to fill the current block with data
    VertexB *data = CurrentBlock+CurrentCount;
    VertexB v;
    int i=0;
    for (i=0; i<(maxn-CurrentCount); i++)
    {
	if (reader->CanRead())
	    *data = reader->Read();
	else
	    break;

	data++;
	reader->Next();	
    }

    CurrentCount += i;
    // now as full of data as it can get
}

// Advances writer pointer
void NextWriter()
{
    curWriterIndex++;
    curWriterIndex %= numWriters;
    writer = writers[curWriterIndex];
}

// The main function
void ProcessBlocks(string infile, string outfile, int maxcnt, int numfiles, BlockFile& bf)
{
    MAX_COUNT = maxcnt;

    SetupBlocks();

    CurrentBlock = (VertexB*)malloc(MAX_COUNT * BUFFER_FAC * sizeof(VertexB));

    CurrentCount = 0;

    curBlockFile = &bf;

    totalNodes = 0;
    totalLeaves = 0;

    curWriterIndex = 0;
    numWriters = numfiles;

    // open up the input file
    BufferedReader<VertexB> readFile(infile);
    // set delete, since we don't need to keep them
    readFile.SetDelete(true);
    reader = &readFile;

    // now open up each output file, as outfile.i
    writers = new LargeWriter*[numfiles];
    for (int i=0; i<numfiles; i++)
	writers[i] = new LargeWriter(outfile + "." + toString<int>(i));

    // and set to first writer
    writer = writers[0];

    // load in first sample
    ReadNextBlock();

    printf("Creating blocks to %s...\n", outfile.c_str());
    printf("with vertex size of %d and block size of %d.\n",sizeof(OutVertex),sizeof(OutBlock));
    fflush(stdout);

    // and make the master block, with 0 blockid, max shift, and 0 depth
    CreateBlocks(0, MAX_DEPTH*3, 0);

    bf.firstLocation = writer->GetLocation();
    bf.firstFile = curWriterIndex;
    bf.firstLength = WritePendingBlock(0, MAX_DEPTH*3, 0);
    printf("\nDone, created %d nodes and %d leaves.\n", totalNodes, totalLeaves);

    // close all the writers
    for (int i=0; i<numfiles; i++)
    {
	writers[i]->Close();
	delete writers[i];
    }

    // since it's an array of pointers, destructors not called
    delete[] writers;

    free(CurrentBlock);

    CleanupBlocks();
    reader = NULL;
}
