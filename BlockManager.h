#ifndef _BLOCKMANAGER_H_
#define _BLOCKMANAGER_H_

#include "Globals.h"
#include "Blocks.h"
#include "SDL_thread.h"

#include <stdint.h>
#include <vector>

// the OS-dependent file representation
typedef FILE* LFILE;

// snapinfo struct, as stored in snapshot files
struct __attribute__ ((__packed__)) SnapInfo
{
    int snapnum; // number of snapshot
    int firstFile; // subfile containing first block
    uint64_t firstLocation; // file location of first block
    uint64_t firstLength; // file length of first block
    double time; // time of snapshot, as log(a)
    double pos[3]; // min. of snapshot in each dim
    double scale[3]; // size of snapshot in each dim
};

// a flag to let priority know we are loading this block's children
#define BLOCK_LOAD_FLAG (1<<9)
// a flag to let a block's children know they are going to be deleted
#define BLOCK_DELETE_FLAG (1<<10)
// both
#define BLOCK_MANAGER_FLAGS (BLOCK_LOAD_FLAG | BLOCK_DELETE_FLAG)

class BlockManager
{
    int numSnaps; // total snapshot count
    SnapInfo* snaps; // infos

    // file pointers for each snapshot
    // (note that this array is a 2D array of arrays
    // and accessed using snapFile[snap][part])
    LFILE ** snapFile;


    Block ** rootNodes; // pointers to the root blocks of each snap



    SDL_Thread **threads; // the loader threads


    // controls access to r/w globals (such as bytesloaded, etc)
    SDL_mutex *memMutex;

    // total number of bytes allocated (not including pending frees!)
    uint64_t totalBytes;

    // and total blocks loaded
    int totalBlocks;

    // a vector containing the blocks we need to free next update
    std::vector<Block*> deadBlocks;



    // attempts to load single snapshot header from a file
    bool loadSnapHeader(string filename, int index);

    // following two are OS-dependent
    bool open(int index, string filename); // opens single snap, all parts
    void read(int index, int part, uint64_t loc, int length, void* data); // reads from a given subfile
    void close(int index);

    // finds child blocks contained in chunk of mem.
    void findBlocks(Block* parent, Block* child);

    // "removes" child blocks (adds them to deadBlocks also)
    void removeBlocks(Block* parent);

    // locks/unlocks mem mutex
    void lock();
    void unlock();


public:

    // these mostly just handle mutex stuff
    BlockManager();
    ~BlockManager();

    // loads snapshot info files
    bool LoadSnapshots();

    // opens all snapshot files
    bool OpenFiles();

    // starts all loading threads
    void StartThreads();

    // the main loop for a single loading thread
    void LoadingLoop(const int id);

    // waits for loader threads to terminate
    void CloseFiles();

    // returns root block for a given snapshot
    Block* GetRoot(int snap);

    // returns number of snapshots
    int GetSnapCount();

    // or the time of a snapshot
    double GetSnapTime(int snap);

    // or the snapshot visible at a given time
    int GetSnapAt(double time);

    // sets arrays to min and scale of a block, given snapshot
    void GetBlockCoords(int snap, Block *block, double* mins, double* scales);

    // frees blocks that have been removed by loader since last update
    void FreeDeadBlocks();
};

#endif
