#ifndef _BLOCKPRIORITY_H_
#define _BLOCKPRIORITY_H_

#include "SDL.h"
#include "SDL_thread.h"
#include "Blocks.h"

#include <stdint.h>
#include <vector>

class BlockPriority
{
    SDL_mutex *queueMutex;

    // containers for each set of blocks
    std::vector<BlockInfo> mergeBlocks;
    std::vector<BlockInfo> splitBlocks;

    // a few cached vars for recursive functions
    double timeScale; // scaling factor based on abs(curtime - snaptime)
    int curSnap; // snapshot we're checking

    // prevents priority recalculation
    bool freezePriority;



    // recursive function to traverse tree
    // (returns whether any grandchildren are loaded)
    bool buildQueue(Block *block, Block *parent);

    // returns the score of a single block
    double scoreBlock(Block* block);

    // returns the scaling factor for a given snapshot
    double getTimeScale(int snap);

public:

    BlockPriority();
    ~BlockPriority();

    // Locks class
    void Lock();

    // Unlocks class
    void Unlock();

    // computes scores and refreshes queues
    void Recompute();

    // gets highest-score split in a certain subfile
    BlockInfo GetSplit(int subfile);

    // removes best split candidate from list
    void DoSplit(int subfile);

    // returns blocks to merge, while removing from internal list
    // (does not include parents of block)
    std::vector<Block*> DoMerge(uint64_t numBytes, Block* parent);

    // returns the score penalty of freeing a given number of bytes
    // via merging, not using given block's parents
    double GetMergeCost(uint64_t numBytes, Block* parent);

    // toggles priority freezing
    void ToggleFreeze() { freezePriority = !freezePriority; }
};

#endif
