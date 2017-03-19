#include "Globals.h"
#include "BlockManager.h"
#include "Blocks.h"
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "SDL_thread.h"

using namespace std;

// helper class for starting threads
struct threadInfo
{
    BlockManager *bm;
    int id;
};

/* Thread starter function, just calls member func. */
int startThread(void *ptr)
{
    threadInfo *thread = (threadInfo*)ptr;
    // start the thread
    thread->bm->LoadingLoop(thread->id);
    // and del object, though it doesn't really matter
    delete thread;

    return 0;
}

/* This is the loading loop, which is run in parallel for each dir. 
   Starts by loading root block. */
void BlockManager::LoadingLoop(const int id)
{
    printf("Loading thread %d for %s started.\n", id, g_Opts->file.dirs[id].c_str());

    // load all root blocks:
    for (int i=0; i<numSnaps; i++)
    {
	if (snaps[i].firstFile != id)
	    continue;
	// our file	
	// alloc correct block size
	Block* root = (Block*)malloc(snaps[i].firstLength);
	// and read in block
	read(i, snaps[i].firstFile, snaps[i].firstLocation, snaps[i].firstLength, root);
	// don't need to set internal ptrs to 0, they're 0 in the file
	// now that it's loaded, we can set the internal root ptr
	rootNodes[i] = root;

	// and add to global byte count
	lock();
	if (g_Opts->dbg.printBlocks)
	    printf("Loaded root block of size %llu at %llu.\n",
		   snaps[i].firstLength, snaps[i].firstLocation);
	totalBytes += snaps[i].firstLength;
	totalBlocks++;
	unlock();
    }

    // loop as long as program is running and load best block
    while (!g_State->IsQuit())
    {
	// first, just lock queue and mem mutex
	g_Priority->Lock();
	lock();

	// get "best" block on this thread's disk
	BlockInfo parent = g_Priority->GetSplit(id);

	// nothing to do, for whatever reason
	if (parent.block == NULL)
	{
	    unlock();
	    g_Priority->Unlock();

	    SDL_Delay(20);
	    continue;
	}

	// check if we have enough memory
	// and if we don't, should we load anyway?
	if (parent.block->childLength + totalBytes > g_Opts->sys.maxBytes)
	{
	    uint64_t bytesOver = parent.block->childLength + totalBytes - g_Opts->sys.maxBytes;
	    double freeScore = g_Priority->GetMergeCost(bytesOver, parent.parent);
	    // if we don't increase our score, don't load
	    if (parent.score < freeScore)
	    {
		unlock();
		g_Priority->Unlock();
		SDL_Delay(20);
		continue;
	    }

	    // otherwise, remove blocks that require removal
	    vector<Block*> rem = g_Priority->DoMerge(bytesOver, parent.parent);
	    int num = (int)rem.size();
	    for (int i=0; i<num; i++)
		removeBlocks(rem[i]);
	}

	// alright, we are ready to load: first, increment byte count and update queue
	totalBytes += parent.block->childLength;
	g_Priority->DoSplit(id);

	// and allocate
	Block* child = (Block*)malloc(parent.block->childLength);
	if (child == NULL)
	{
	    // out of memory, so cancel read
	    printf("Out of memory error!\n");
	    totalBytes -= parent.block->childLength;
	    unlock();
	    g_Priority->Unlock();
	    SDL_Delay(100);
	    continue;
	}

	// THIS IS SUPER IMPORTANT SO THAT WE DON'T TRY TO MERGE IT
        // flag that we are loading the block
	parent.block->childFlags |= BLOCK_LOAD_FLAG;

	// now we can safely unlock both
	unlock();
	g_Priority->Unlock();

	// now actually read the blocks (this is the long step)
	read(parent.snap, parent.block->childFile, parent.block->childLocation, parent.block->childLength, child);
	// and set pointers of parent block
	findBlocks(parent.block, child);

	// then unflag it
	parent.block->childFlags &= ~BLOCK_LOAD_FLAG;
    }

    printf("Loading thread %d finished.\n", id);

    return;
}

/* Removes all the child blocks of a single parent. Removal here means parent's
   pointers are nulled and the child is added to deadBlocks. */
void BlockManager::removeBlocks(Block* parent)
{
    Block *child = NULL;

    // THIS IS SUPER IMPORTANT: FLAG ALL CHILD BLOCKS AS SCHEDULED FOR
    // REMOVAL. THIS SHOULD STOP US FROM TRYING TO SPLIT THEM.

    // find pointer to head of block
    for (int i=0; i<8; i++)
    {
	if (parent->childPtr[i] != NULL)
	{
	    // flag child as removed
	    parent->childPtr[i]->childFlags |= BLOCK_DELETE_FLAG;
	    // set to first pointer
	    if (child == NULL)
		child = parent->childPtr[i];
	}
    }

    // set all parent pointers to 0
    for (int i=0; i<8; i++)
    {
	if (parent->childPtr[i] != NULL)
	    totalBlocks--;

	if (g_Opts->dbg.printBlocks)
	    printf("removing child at %x\n",(uint)parent->childPtr[i]);
	parent->childPtr[i] = NULL;
    }

    // and add child blocks to free queue
    deadBlocks.push_back(child);

    // finally, decrement global byte count
    // (this assumes we have a lock!)
    totalBytes -= parent->childLength;

    if (g_Opts->dbg.printBlocks)
	printf("Block %x added to delete queue.\n", (uint)child);
}

/* Finds all eight child blocks contained in chunk of memory pointed to by child. */
void BlockManager::findBlocks(Block* parent, Block* child)
{
    // pointer used for byte-by-byte iteration
    char *cur = (char*)child;

    if (g_Opts->dbg.printBlocks)
	printf("Scanning parent at %x.\n",(uint)parent);
    
    int nchildren=0;
    for (int i=0; i<8; i++)
    {
	// skip over this, if i'th child doesn't exist (and thus wasn't written)
	if ((parent->childFlags & (1<<i)) == 0)
	    continue;
	// otherwise, set child pointer
	parent->childPtr[i] = (Block*)cur;
	// and set children to NULL
	for (int j=0; j<8; j++)
	    parent->childPtr[i]->childPtr[j] = NULL;
	// and step cur by the size of the block (header + verts * vertsize)
	cur += sizeof(Block) + parent->childPtr[i]->count * g_Opts->file.vertexSize;
	nchildren++;
    }

    // add number of children loaded
    lock();
    totalBlocks += nchildren;
    unlock();

    if (g_Opts->dbg.printBlocks)
    {
	printf("Children of block %x loaded.\n", (uint)parent);
	printf("total bytes: %d, total blocks: %d\n", (int)totalBytes, totalBlocks);
    }
}

/* Returns the global coordinates of a given block/snapshot pair. */
void BlockManager::GetBlockCoords(int snap, Block *block, double* mins, double* scales)
{
    if (block == NULL)
    {
	for (int i=0; i<3; i++)
	{	    
	    mins[i] = snaps[snap].pos[i];
	    scales[i] = snaps[snap].scale[i];
	}
	return;
    }
    // each block pos goes from 0...65535
    for (int i=0; i<3; i++)
    {
	mins[i] = snaps[snap].pos[i] + (block->pos[i]*snaps[snap].scale[i]/65536.0);
	scales[i] = snaps[snap].scale[i] / (1<<block->depth);
    }
}


/* Frees memory used by all dead blocks. totalBytes was updated by removeBlocks. */
void BlockManager::FreeDeadBlocks()
{
    lock();

    int count = (int)deadBlocks.size();

    // free each one
    for (int i=0; i<count; i++)
    {
	free(deadBlocks[i]);
	if (g_Opts->dbg.printBlocks)
	    printf("Block %x freed!\n", (uint)deadBlocks[i]);
    }

    // and clear entire vector
    deadBlocks.clear();

    unlock();
    // yes, it really is that simple!
}

/* Waits on all loader threads to terminate, then closes files. */
void BlockManager::CloseFiles()
{
    // wait for threads
    for (int i=0; i<g_Opts->file.ndirs; i++)
	SDL_WaitThread(threads[i], NULL);

    // close all of our opened files
    for (int i=0;i<numSnaps;i++)
	close(i);

    printf("All files closed.\n");
}

/* Starts loading threads. Each thread starts by loading root block. */
void BlockManager::StartThreads()
{
    // start one for each dir
    for (int i=0; i<g_Opts->file.ndirs; i++)
    {
	threadInfo *thread = new threadInfo();
	thread->bm = this;
	thread->id = i;
	// start the thread, will free thread object
	threads[i] = SDL_CreateThread(&startThread, thread);
    }
}

/* Opens all snapshot files. */
bool BlockManager::OpenFiles()
{
    for (int i=0; i<numSnaps; i++)
    {
	bool ret = open(i, "/blocks_" + toString(snaps[i].snapnum));
	if (!ret)
	    return false;
    }
    // all opened successfully
    return true;
}

/* Loads all snapshot headers. */
bool BlockManager::LoadSnapshots()
{
    // now we can load each snapshot header
    numSnaps = (g_Opts->file.lastSnap-g_Opts->file.firstSnap)/g_Opts->file.interval + 1;
    snaps = new SnapInfo[numSnaps];

    // load snapshot desc. from dir 0
    int snap = g_Opts->file.firstSnap;
    for (int i=0;i<numSnaps;i++)
    {
	string snapName = "/blocks_" + toString(snap);
	if (loadSnapHeader(g_Opts->file.dirs[0] + snapName + "_info", i))
	{
	    printf("Snapshot info %s loaded.\n", (snapName+"_info").c_str());
	}
	else
	{
	    fprintf(stderr,"Snapshot info %s not found!\n", (snapName+"_info").c_str());
	    return false;
	}
	snaps[i].snapnum = snap;
	snap += g_Opts->file.interval;
    }

    // init rootnodes array, have none loaded yet
    rootNodes = new Block*[numSnaps];
    for (int i=0; i<numSnaps; i++)
	rootNodes[i] = NULL;

    // also initialize file array
    snapFile = new LFILE*[numSnaps];
    for (int i=0; i<numSnaps; i++)
    {
	snapFile[i] = new LFILE[g_Opts->file.ndirs];
	for (int j=0; j<g_Opts->file.ndirs; j++)
	    snapFile[i][j] = NULL;
    }

    // and thread array
    threads = new SDL_Thread*[g_Opts->file.ndirs];

    return true;
}

/* Returns root block info for certain snapshot */
Block* BlockManager::GetRoot(int snap)
{
    return rootNodes[snap];
}

/* Returns numSnaps */
int BlockManager::GetSnapCount()
{
    return numSnaps;
}

/* Returns time of a given snapshot. */
double BlockManager::GetSnapTime(int snap)
{
    return snaps[snap].time;
}

/* Returns the snapshot that falls within a given time. */
int BlockManager::GetSnapAt(double time)
{
    if (time < snaps[0].time)
	return 0;
    for (int i=1; i<numSnaps; i++)
	if (time < snaps[i].time)
	    return i-1;
    return numSnaps-1;
}

/* Attempts to load a single snapshot header, returns false if not found. */
bool BlockManager::loadSnapHeader(string filename, int index)
{
    FILE *file = fopen(filename.c_str(),"rb");
    if (file == NULL)
	return false;
    fread(snaps + index, sizeof(SnapInfo), 1, file);
    fclose(file);

    return true;
}

/* Opens a given fileset for reading. OS-dependent, due to large file sizes. */
bool BlockManager::open(int index, string filename)
{
    for (int i=0; i<g_Opts->file.ndirs; i++)
    {
	snapFile[index][i] = fopen((g_Opts->file.dirs[i]+filename+"."+toString(i)).c_str(),"rb");
	if (snapFile[index][i] == NULL)
	{
	    printf("File %s not found!\n", (g_Opts->file.dirs[i]+filename+"."+toString(i)).c_str());
	    return false;
	}
	printf("File %s opened.\n", (g_Opts->file.dirs[i]+filename+"."+toString(i)).c_str());
    }
    return true;
}

/* Reads a certain amount from a file, at a given location (also OS-dependent). */
void BlockManager::read(int index, int part, uint64_t loc, int length, void* data)
{
    // need to use fseeko so that it uses off_t offset, which
    // is now off64_t
    fseeko(snapFile[index][part], loc, SEEK_SET);
    fread(data, length, 1, snapFile[index][part]);
}

/* Closes a file. OS-dependent. */
void BlockManager::close(int index)
{
    for (int i=0; i<g_Opts->file.ndirs; i++)
	fclose(snapFile[index][i]);
}

BlockManager::BlockManager()
{
    memMutex = SDL_CreateMutex();
    totalBytes = 0;
    totalBlocks = 0;
}

BlockManager::~BlockManager()
{
    SDL_DestroyMutex(memMutex);
}

void BlockManager::lock()
{
    SDL_mutexP(memMutex);
}

void BlockManager::unlock()
{
    SDL_mutexV(memMutex);
}
