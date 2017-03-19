#include "Formats.h"
#include "PartFiles.h"
#include "Process.h"
#include "MergeFiles.h"
#include "TreeIndex.h"
#include <stdio.h>

#include <sys/stat.h>
#include <sys/types.h>

/* creates last and then all previous snapshot block files. */
void DoProcessing(PathInfo& ps, PathPair paths, int firstSnap, int lastSnap, int step, int maxcnt, int numsubs)
{
    SnapHeader curSnap, nextSnap;

    // now count backwards from lastSnap and do all processing required
    for (int snap = lastSnap; snap>=firstSnap; snap -= step)
    {
	string snapName = "/snap_" + toString<int>(snap);
	string indName = "/ind_" + toString<int>(snap);
	string blocksName = "/blocks_" + toString<int>(snap);

	// have we loaded anything yet?
	if (nextSnap.snap < 0)
	{
	    // try loading first (last) snap
	    nextSnap = SnapHeader(paths.location + snapName+"_info");
	    // if we succeeded, we don't need to process it
	    if (nextSnap.snap >= 0)
	    {
		printf("Found snap %d at %s, resuming...",snap,nextSnap.filename);
		continue;
	    }
	    // otherwise, try other path
	    nextSnap = SnapHeader(paths.temp + snapName+"_info");
	    if (nextSnap.snap >= 0)
	    {
		printf("Found snap %d at %s, resuming...",snap,nextSnap.filename);
		continue;	    
	    }
	    // otherwise, no dice
	    printf("Failed to locate previously generated block, starting from scratch.\n");
	}

	// interleave and sort by pid
	curSnap = Interleave(ps, paths.location + snapName, snap);
	paths = MergeSorted<VertexA>(snapName, paths);
	curSnap.SetFilename(paths.location + snapName);

	// build index and sort by index
	BlockFile bf;
	// do we have a next snap?
	if (nextSnap.snap >= 0)
	    bf = BuildIndex(&curSnap, &nextSnap, paths.temp + indName);
	else
	    bf = BuildIndex(&curSnap, NULL, paths.temp + indName);

	paths.swap();
	paths = MergeSorted<VertexB>(indName, paths);

	// and create all the block files
	ProcessBlocks(paths.location + indName, paths.temp + blocksName, maxcnt, numsubs, bf);
	bf.Save(paths.temp+blocksName+"_info");

	// also save snap info
	curSnap.Save();

	nextSnap = curSnap;
    }
}

/* creates all specified subhalo files. */
void DoSubhalos(PathInfo& ps, PathPair paths, int firstSnap, int lastSnap, int step)
{
    printf("Building subhalo table for %d...%d, every %d.\n",firstSnap,lastSnap,step);

    string orderfile = "/suborder";
    // first, we need to make the subid lookup table
    BuildSubOrder(ps, firstSnap, paths.location + orderfile);
    // and merge them
    paths = MergeSorted<uint64_t>(orderfile, paths);
    // keep track of its location
    string suborder = paths.location + orderfile;

    // and get a treefile
    string treefile = ps.GetTree(lastSnap,step);

    for (int snap=firstSnap; snap<= lastSnap; snap+=step)
    {
	printf("Doing snap %d...\n",snap);
	// first, rearrange subids
	string subName = "/subid_" + toString<int>(snap);
	PrepareSubIds(ps, snap, paths.location + subName);
	// and mergesort (probably not neccessary)
	paths = MergeSorted<GroupVertex>(subName, paths);
	// now fix the pids to go from 0....n-1
	SequenceSubIds(paths, suborder, subName);
	paths.swap();
	// and resort
	paths = MergeSorted<GroupVertexB>(subName, paths);
	// now build the table omgzzz
	BuildHaloTable(ps, treefile, paths, snap, step, subName);
    }
}



PathInfo ReadParams(string filename, PathPair& paths, int& first, int& last, int& step, int& maxcount, int& nstripes)
{
    FILE *file = fopen(filename.c_str(), "r");

    // can use defaults for system config
    if (file == NULL)
    {
	fprintf(stderr,"Error opening param file %s!\n",filename.c_str());
	return PathInfo("","");
    }

    char line[1024];

    string srcpath;
    string srcname;

    while (fgets(line, 1024, file)!= NULL)
    {
	// find pos of first actual character
	int s = strspn(line, " \t\n\v");
	// skip comment lines
	if (strlen(line) == 0)
	    continue;
	if (line[s] == '#')
	    continue;
	// and find position of the value
	int v = strcspn(line, "=")+1;

	// otherwise (also know line is null-term.)
	// check for each different param
	if (strncmp(line+s, "FirstSnap", 9) == 0)
	    first = atoi(line+v);
	else if (strncmp(line+s, "LastSnap", 8) == 0)
	    last = atoi(line+v);
	else if (strncmp(line+s, "SnapInterval", 12) == 0)
	    step = atoi(line+v);
	else if (strncmp(line+s, "MaxCount", 8) == 0)
	    maxcount = atoi(line+v);
	else if (strncmp(line+s, "NumStripes", 10) == 0)
	    nstripes = atoi(line+v);
	else if (strncmp(line+s, "Out1", 4) == 0)
	    paths.location = string(line+v, strcspn(line+v,"\n\r"));
	else if (strncmp(line+s, "Out2", 4) == 0)
	    paths.temp = string(line+v, strcspn(line+v,"\n\r"));
	else if (strncmp(line+s, "SrcPath", 7) == 0)
	    srcpath = string(line+v, strcspn(line+v,"\n\r"));
	else if (strncmp(line+s, "SrcName", 7) == 0)
	    srcname = string(line+v, strcspn(line+v,"\n\r"));
    }

    fclose(file);

    return PathInfo(srcpath,srcname);
}




int main(int argc, char * argv[])
{
    // initialize octtree lookup table
    BuildTreeLookup();

    if (argc < 2)
    {
	printf("\nusage: createblocks <paramfile>\n\n");
	exit(1);
    }

    int findex = 0;

    bool dogroups = true;
    bool dopoints = true;
    for (int i=1; i<argc; i++)
    {
	if (strncmp(argv[i], "-g", 2) == 0)
	    dopoints = false;
	else if (strncmp(argv[i], "-p", 2) == 0)
	    dogroups = false;
	else
	    findex = i;
    }

    int first, last, step;
    int maxcount = 16000;
    int nstripes = 2;

    PathPair paths;
    PathInfo ps = ReadParams(string(argv[findex]), paths, first, last, step, maxcount, nstripes);

    printf("Processing snaps %d to %d, every %d, with max count of %d and %d stripes.\n", first, last, step, maxcount, nstripes);

    if (dopoints)
	DoProcessing(ps, paths, first, last, step, maxcount, nstripes);

    if (dogroups)
	DoSubhalos(ps, paths, first, last, step);

    return 0;
}
