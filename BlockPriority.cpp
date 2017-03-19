#include "BlockPriority.h"
#include "Globals.h"

using namespace std;

// overload operator, for sorting fun!
bool operator<(const BlockInfo& a, const BlockInfo& b)
{
    // this is backwards so that we sort descending
    return a.score > b.score;
}

/* Recomputes entire split/merge queue across snapshots. */
void BlockPriority::Recompute()
{
    Lock();
    // reset and start over
    mergeBlocks.clear();
    splitBlocks.clear();

    // to temporarily pause priority comps
    if (freezePriority)
    {
	Unlock();
	return;
    }

    // now begin recursion on each root block
    int num = g_Blocks->GetSnapCount();

    for (int i=0; i<num; i++)
    {
	// tell globally what snap we are currently looking at
	curSnap = i;
        // penalize really distant snaps, but still load in correct order
	timeScale = getTimeScale(curSnap);

	Block* root = g_Blocks->GetRoot(i);
	// and build the queue, if it's loaded
	if (root != NULL)
	    buildQueue(root, NULL);
    }

    if (g_Opts->dbg.printPrior)
	printf("Split queue: %d, Merge queue: %d\n",
	       splitBlocks.size(), mergeBlocks.size());

    // and sort each vector
    sort(mergeBlocks.begin(), mergeBlocks.end());
    sort(splitBlocks.begin(), splitBlocks.end());

    Unlock();
}

/* This is the important recursive function that builds the queue, adding
   leaves to split queue and parents of all leaves to merge queue. 
   It returns whether block has any children loaded in memory. 
   It needs the parent so it can put it into the BlockInfos. */
bool BlockPriority::buildQueue(Block *block, Block *parent)
{
    // first: do i have any children? if not, can't split or merge
    // but can merge parent, so return false
    if (block->childFlags == 0)
	return false;

    // are we loading or deleting this block?
    // return true to avoid being merged
    if ((block->childFlags&BLOCK_MANAGER_FLAGS) != 0)
	return true;

    BlockInfo b;
    b.block = block;
    b.parent = parent;
    b.snap = curSnap;

    // if i *do* have children, are any of them loaded?
    // (if not, can split)
    // (if yes, and they are all leaves, can merge)
    bool hasGrandchildren = false;
    int nloaded = 0;
    for (int i=0; i<8; i++)
    {
	// ignore not loaded blocks
	if (block->childPtr[i] == NULL)
	    continue;

	nloaded++;

	// recurse, and update whether it has grandchildren
	bool ret = buildQueue(block->childPtr[i], block);
	hasGrandchildren = hasGrandchildren || ret;
    }

    // if i don't have any loaded, but children exist, i can split
    // (but can't merge, and want to return false)
    if (nloaded == 0)
    {
	b.score = scoreBlock(block);
	splitBlocks.push_back(b);	
	return false;
    }

    // otherwise, if i have children in memory, but have no
    // grandchildren in memory, i can safely merge
    if (!hasGrandchildren)
    {
	// use negative score, so we sort in other direction
	b.score = -scoreBlock(block);
	mergeBlocks.push_back(b);
	return true;
    }

    // otherwise, i have grandchildren loaded and am useless
    return true;
}

/* This is the most important function! It computes the score of a given block. */
double BlockPriority::scoreBlock(Block* block)
{
    double mins[3], scales[3];
    // get block space extents
    g_Blocks->GetBlockCoords(curSnap, block, mins, scales);
    // now compute their size on screen
    double radius = sqrt(scales[0]*scales[0]+scales[1]*scales[1]+scales[2]*scales[2]);    

    Vector3 pos, targ, up;
    // gets the world-coordinate vectors
    g_State->GetWorldVectors(pos,targ,up);

    // center the block
    for (int j=0; j<3; j++)
	mins[j] += 0.5*scales[j];

    // and offset the vector
    pos.x -= (float)mins[0];
    pos.y -= (float)mins[1];
    pos.z -= (float)mins[2];

    // return sin of on-screen box radius
    double score = radius/Vector3::Length(pos);

//    return score*timeScale;
    return timeScale*g_Render->scoreBlock(curSnap, block);
}

/* Returns a scaling factor for the specified snap. */
double BlockPriority::getTimeScale(int snap)
{
    double mu = g_State->GetTime();
    double sigma = g_State->GetTimeSigma();

    double time1 = g_Blocks->GetSnapTime(snap);

    // compute CDF for gaussian distribution
    if (snap+1 < g_Blocks->GetSnapCount())
    {
	double time2 = g_Blocks->GetSnapTime(snap+1);
	double cdf2 = 0.5*(1 + erf( (time2-mu)/(sigma*sqrt(2)) ));
	double cdf1 = 0.5*(1 + erf( (time1-mu)/(sigma*sqrt(2)) ));
	return cdf2-cdf1;
    }
    else
    {
	// get it from x to infinity
	return 1 - 0.5*(1 + erf( (time1-mu)/(sigma*sqrt(2)) ));
    }
    
}

/* Returns info of highest-score split candidate. Assumes locked.
   I know this is not efficient, but it only searches the first few anyway
   since recompute gets called often enough. */
BlockInfo BlockPriority::GetSplit(int subfile)
{
    int count = (int)splitBlocks.size();

    for (int i=0; i<count; i++)
    {
	// have we removed it from the queue?
	if (splitBlocks[i].block == NULL)
	    continue;
	// only criteria
	if (splitBlocks[i].block->childFile == subfile)
	    return splitBlocks[i];
    }

    // or we didn't find one?
    BlockInfo b;
    b.block = NULL;

    return b;
}

/* Removes split candidate from list. Assumes locked. */
void BlockPriority::DoSplit(int subfile)
{
    int count = (int)splitBlocks.size();

    for (int i=0; i<count; i++)
    {
	// already removed?
	if (splitBlocks[i].block == NULL)
	    continue;
	if (splitBlocks[i].block->childFile == subfile)
	{
	    // NOW IMPORTANT PART:
	    // remove parent from merge queue
	    for (int j=0; j<(int)mergeBlocks.size(); j++)
	    {
		if (mergeBlocks[j].block == NULL)
		    continue;
		// my parent is merge block?
		if (mergeBlocks[j].block == splitBlocks[i].parent)
		    mergeBlocks[j].block = NULL;
	    }

	    // remove block
	    splitBlocks[i].block = NULL;
	    break;
	}
    }
}

/* Returns lowest-score merge candidate and removes it from list. */
vector<Block*> BlockPriority::DoMerge(uint64_t numBytes, Block *parent)
{
    int count = (int)mergeBlocks.size();
    uint64_t bytesMerged = 0;

    vector<Block*> blocks;

    // sorted correctly, since merge scores are negative
    for (int i=0; i<count; i++)
    {
	// make sure isn't removed
	if (mergeBlocks[i].block == NULL)
	    continue;
	// skip over this one
	if (mergeBlocks[i].block == parent)
	    continue;
	// we can merge blocks of score 0, or anything
	// add it to vec
	bytesMerged += mergeBlocks[i].block->childLength;
	blocks.push_back(mergeBlocks[i].block);
	// and remove it from queue
	mergeBlocks[i].block = NULL;
	if (bytesMerged > numBytes)
	    break;
    }

    // NOW IMPORTANT PART:
    // remove all children from split queue
    for (int i=0; i<(int)blocks.size(); i++)
    {
	for (int j=0; j<(int)splitBlocks.size(); j++)
	{
	    // already removed
	    if (splitBlocks[j].block == NULL)
		continue;
	    // however, if split's parent equals current, remove split
	    if (splitBlocks[j].parent == blocks[i])
	    {
		if (g_Opts->dbg.printPrior)
		    printf("%x removed from split queue because of merging %x.\n",
			   (uint)splitBlocks[j].block, (uint)blocks[i]);
		splitBlocks[j].block = NULL;
	    }
	}
    }

    return blocks;
}

/* Returns the sum cost of merging numBytes' worth child blocks. 
   parent is the parent of the split candidate, so we don't merge those. */
double BlockPriority::GetMergeCost(uint64_t numBytes, Block *parent)
{
    int count = (int)mergeBlocks.size();
    uint64_t bytesMerged = 0;
    double score = 0;

    // count forwards, but use neg. score
    for (int i=0; i<count; i++)
    {
	// removed?
	if (mergeBlocks[i].block == NULL)
	    continue;
	// skip over this one
	if (mergeBlocks[i].block == parent)
	    continue;
	// add stuff
	bytesMerged += mergeBlocks[i].block->childLength;
	// add negative score, since it is negative of actual score
	score += (-mergeBlocks[i].score);
	if (bytesMerged > numBytes)
	    return score;
    }

    return 0.0;
}

void BlockPriority::Lock()
{
    SDL_mutexP(queueMutex);
}

void BlockPriority::Unlock()
{
    SDL_mutexV(queueMutex);
}

BlockPriority::BlockPriority()
{
    // just initialize mutex
    queueMutex = SDL_CreateMutex();
    freezePriority = false;
}

BlockPriority::~BlockPriority()
{
    // destroy mutex
    SDL_DestroyMutex(queueMutex);
}
