#include <cmath>

#include "PartFiles.h"
#include "Formats.h"
#include "Loaders.h"
#include "Process.h"


int* LoadFileOffsets(PathInfo& ps, int snap);

/* This function loads in group catalogue files and particle subid files
   and outputs all of the points sorted by pid (pre-mergesort). */
void PrepareSubIds(PathInfo& ps, int snap, string filename)
{
    // load first group and id file
    GroupData gdata = LoadGroup(ps, snap, 0, false);
    IDData idata = LoadIDs(ps, snap, 0);

    int totalIDs = idata.totalIDs;
    int totalGroups = gdata.totalGroups;
    int totalSubhalos = gdata.totalSubhalos;

    int numIDFiles = idata.numFiles;
    int numGroupFiles = gdata.numFiles;

    // allocate overall arrays
    int32_t* groupStart = new int32_t[totalGroups];
    int32_t* groupLength = new int32_t[totalGroups];

    int32_t* subStart = new int32_t[totalSubhalos];
    int32_t* subLength = new int32_t[totalSubhalos];

    printf("Found %d subhalos, %d groups.\n", totalSubhalos, totalGroups);

    int curGroup = 0;
    int curSubhalo = 0;

    // copy first set of groups and subs
    memcpy(groupStart+curGroup, gdata.gOffset, gdata.numGroups*4);
    memcpy(groupLength+curGroup, gdata.gLength, gdata.numGroups*4);

    memcpy(subStart+curSubhalo, gdata.sOffset, gdata.numSubhalos*4);
    memcpy(subLength+curSubhalo, gdata.sLength, gdata.numSubhalos*4);

    curGroup += gdata.numGroups;
    curSubhalo += gdata.numSubhalos;

    FreeGroup(gdata);

    // repeat for every subsequent file
    for (int i=1; i<gdata.numFiles; i++)
    {
	gdata = LoadGroup(ps, snap, i, false);
	
	// copy first set of groups and subs
	memcpy(groupStart+curGroup, gdata.gOffset, gdata.numGroups*4);
	memcpy(groupLength+curGroup, gdata.gLength, gdata.numGroups*4);
	
	memcpy(subStart+curSubhalo, gdata.sOffset, gdata.numSubhalos*4);
	memcpy(subLength+curSubhalo, gdata.sLength, gdata.numSubhalos*4);
	
	curGroup += gdata.numGroups;
	curSubhalo += gdata.numSubhalos;
	
	FreeGroup(gdata);
    }

    // current group/subhalo id
    int curGroupIndex = 0;
    int curSubhaloIndex = 0;


    BufferedWriter<GroupVertex> writer(filename);
    writer.SetSort(true);

    GroupVertex tmp;

    printf("Reading %d points...\n",(int)totalIDs);


    // total count read
    int curCount = 0;

    for (int idFile = 0; idFile < numIDFiles; idFile++)
    {
	// load next id file
	if (idFile > 0)
	    idata = LoadIDs(ps, snap, idFile);

	// now write each particle
	for (int i=0; i<idata.numIDs; i++)
	{
	    // increment group?
	    if (curCount >= groupStart[curGroupIndex] + groupLength[curGroupIndex]
		&& curGroupIndex+1 < totalGroups)
	    {
		curGroupIndex++;
	    }

	    // increment subhalo if we are *in* it
	    if (curSubhaloIndex+1 < totalSubhalos 
		&& curCount >= subStart[curSubhaloIndex+1])
	    {
		curSubhaloIndex++;
	    }

	    // store pid, subid, fofid
	    tmp.pid = idata.IDs[i];
	    tmp.fofid = curGroupIndex;
	    
	    // are we past subhalo?
	    // (write -1 if in group, but not in subhalo)
	    if (curCount >= subStart[curSubhaloIndex] + subLength[curSubhaloIndex])
		tmp.subid = -1;
	    else
		tmp.subid = curSubhaloIndex;

	    writer.Write(tmp);

	    curCount++;
	}

	// free current id file
	FreeIDs(idata);
    }

    printf("Read %d points.\n",(int)curCount);

    int numFiles = writer.Close();

    // save # of files, for mergesort
    FILE *file = fopen(filename.c_str(),"wb");
    fwrite(&numFiles, sizeof(int), 1, file);
    fclose(file);

    // free up arrays
    delete[] groupStart;
    delete[] groupLength;

    delete[] subStart;
    delete[] subLength;
}

/* This builds the pid ordering, eg, makes a sorted list of all pids
   (from snapshot files). */
void BuildSubOrder(PathInfo& ps, int snap, string filename)
{
    BufferedWriter<uint64_t> writer(filename);
    writer.SetSort(true);

    printf("Loading ids from snapshot %d...\n",snap);

    VertexData vdata = LoadSnap(ps, snap, 0);

    uint64_t numTotal = vdata.numTotals[1];

    printf("Processing %lu particles...", (long unsigned int)numTotal);

    uint32_t totalIndex = 0; // overall index
    uint32_t vIndex = 0; // index into snap file
    int vFileId = 0; // index of snap file

    while (totalIndex <  numTotal)
    {
	if (vIndex == vdata.numParts[1]) // load next input file
	{
	    vFileId++;
	    FreeSnap(vdata);
	    vdata = LoadSnap(ps, snap, vFileId);
	    vIndex = 0;
	    printf("Loaded snap part %d; ",vFileId);
	    fflush(stdout);
	}

	// and write vertex pid to file (| is for > 32-bit particle count)
	writer.Write(vdata.id[vIndex] | (((uint64_t)vdata.nLargeSims[1])<<32));

	totalIndex++;
	vIndex++;
    }

    int numFiles = writer.Close();

    // save # of files, for mergesort
    FILE *file = fopen(filename.c_str(),"wb");
    fwrite(&numFiles, sizeof(int), 1, file);
    fclose(file);

    printf("Wrote subid order for snap %d.\n",snap);
    fflush(stdout);

    FreeSnap(vdata);
}

/* Modifies pids to go from 0...n-1 */
void SequenceSubIds(PathPair paths, string suborder, string filename)
{
    // read from one path, write to the other
    BufferedReader<GroupVertex> reader(paths.location+filename);
    BufferedWriter<GroupVertexB> writer(paths.temp+filename);
    // also read pid order file
    BufferedReader<uint64_t> pidreader(suborder);

    writer.SetSort(true);
    reader.SetDelete(true);

    GroupVertex gin;
    GroupVertexB gout;

    // index of point
    uint64_t curIndex = 0;

    while (reader.CanRead())
    {
	gin = reader.Read();
	// set these two
	gout.subid = gin.subid;
	gout.fofid = gin.fofid;

	// now read in as many pids as we need to
	while (gin.pid != pidreader.Read())
	{
	    pidreader.Next();
	    curIndex++;
	}

	gout.pid = curIndex;

	writer.Write(gout);
	reader.Next();
    }

    int numFiles = writer.Close();

    // save # of files, for mergesort
    FILE *file = fopen((paths.temp+filename).c_str(),"wb");
    fwrite(&numFiles, sizeof(int), 1, file);
    fclose(file);

    printf("Wrote adjusted pids.\n");
    fflush(stdout);
}


/* Now this is the main function, it outputs the halo file using merger tree and everything. */
void BuildHaloTable(PathInfo& ps, string treefile, PathPair paths, int snap, int step, string filename)
{
    // load file offsets for next, current, and previous snaps
    int *curFileOffset = LoadFileOffsets(ps, snap);
    int *prevFileOffset = LoadFileOffsets(ps, snap-step);
    int *nextFileOffset = LoadFileOffsets(ps, snap+step);

    // load only the header for info
    GroupData gdata = LoadGroupHeader(ps, snap, 0, false);

    int numGroupFiles = gdata.numFiles;
    int numSubhalos = gdata.totalSubhalos;


    // alright, so far so good. now we need to make our array of output subs
    OutHalo* outHalos = new OutHalo[numSubhalos];

    // set the default values for stuff, just in case
    for (int i=0; i<numSubhalos; i++)
    {
	outHalos[i].pointIndex = -1;
	outHalos[i].pointCount = -1;
    }

    // and we want to populate the subid pointers, all the while writing the output points
    BufferedReader<GroupVertexB> pidreader(paths.location + filename);
    pidreader.SetDelete(true);

    FILE *pidfile = fopen((paths.temp + filename).c_str(),"wb");

    GroupVertexB tmp;

    uint32_t curIndex = 0;

    while (pidreader.CanRead())
    {
	tmp = pidreader.Read();
	pidreader.Next();
	
	// not part of a subhalo, ignore for now
	if (tmp.subid == -1)
	    continue;

	// write pid as an int32
	uint32_t pid = (uint32_t)tmp.pid;
	fwrite(&pid, 4, 1, pidfile);

	// now set substart?
	if (outHalos[tmp.subid].pointIndex == -1)
	    outHalos[tmp.subid].pointIndex = curIndex;

	// and always set count
	outHalos[tmp.subid].pointCount = curIndex - outHalos[tmp.subid].pointIndex + 1;
	// and fofgroup
	outHalos[tmp.subid].fofGroup = tmp.fofid;

	curIndex++;
    }

    fclose(pidfile);
    printf("%d subids written to file.\n", curIndex);

    // get velocity scaling factor for snapshot
    // to convert to dx/dloga
    VertexData vdata1 = LoadSnapHeader(ps, snap, 0);
    VertexData vdata2 = LoadSnapHeader(ps, snap+step, 0);

    double vfac = 1.0 / (HUBBLE * sqrt(vdata1.omega0 / pow(vdata1.time,3)
				+ vdata1.omegaLambda) * sqrt(vdata1.time));

    
    double dloga = 1;
    // does next snap exist?
    if(vdata2.numSubfiles > 0)
	dloga = log(vdata2.time) - log(vdata1.time);
    printf("using dloga of %g.\n",dloga);

    // alright, now that part is set correctly, we can move on to loading
    // *** the merger tree ***
    TreeData tdata = LoadTree(treefile);
    printf("Merger tree contains %d trees, %d subs.\n",tdata.numTrees,tdata.totalHalos);

    // i can just loop through every halo in each tree
    for (int t=0; t<tdata.numTrees;t++)
    {
	// loop through the halos in each tree
	for (int i=0; i<tdata.numHalos[t]; i++)
	{
	    TreeHalo* halo = tdata.roothalo[t] + i;

	    // check if it's the correct snapshot
	    if (halo->SnapNum != snap)
		continue;

	    int outIndex = curFileOffset[halo->FileNum] + halo->SubhaloIndex;
	    // set current snapshot properties
	    for (int j=0; j<3; j++)
	    {
		outHalos[outIndex].pos[j] = halo->Pos[j];
		outHalos[outIndex].vel[j] = halo->Vel[j]*vfac;
		outHalos[outIndex].acc[j] = 0;
	    }
	    outHalos[outIndex].radius = halo->SubHalfMass;

	    outHalos[outIndex].mainDescendant = -1;
	    outHalos[outIndex].mainProgenitor = -1;

	    // check next properties
	    if (halo->Descendant != -1)
	    {
		TreeHalo* nhalo = tdata.roothalo[t] + halo->Descendant;
		if (nhalo->SnapNum == snap+step)
		{
		    // first, set our actual file index
		    outHalos[outIndex].mainDescendant = nhalo->SubhaloIndex
			+ nextFileOffset[nhalo->FileNum];
		    // then, our position interpolation info
		    for (int j=0; j<3; j++)
		    {
			outHalos[outIndex].acc[j] = (nhalo->Pos[j]-halo->Pos[j])/(dloga*dloga)
			    - halo->Vel[j]*vfac/dloga;
			outHalos[outIndex].acc[j] *= 2;
		    }
		}
	    }

	    // or prev properties
	    if (halo->FirstProgenitor != -1)
	    {
		TreeHalo* phalo = tdata.roothalo[t] + halo->FirstProgenitor;
		if (phalo->SnapNum == snap-step)
		{
		    // just set file index
		    outHalos[outIndex].mainProgenitor = phalo->SubhaloIndex
			+ prevFileOffset[phalo->FileNum];
		}
	    }
	}
    }

    printf("Done.\n");

    // now, save the actual halo data
    FILE *halofile = fopen((paths.temp + "/halos_" + toString<int>(snap)).c_str(),"wb");
    // write header with num halos
    fwrite(&numSubhalos, 4, 1, halofile);
    fwrite(outHalos, sizeof(OutHalo), numSubhalos, halofile);
    fclose(halofile);

    FreeTree(tdata);
    delete[] outHalos;

    delete[] curFileOffset;
    if (nextFileOffset != NULL)
	delete[] nextFileOffset;
    if (prevFileOffset != NULL)
	delete[] prevFileOffset;
}


/* This helper just loads an array of offsets into a file for a given snap. */
int* LoadFileOffsets(PathInfo& ps, int snap)
{
    // load only the header for info
    GroupData gdata = LoadGroupHeader(ps, snap, 0, false);

    // didn't find file
    if (gdata.numFiles == -1)
	return NULL;

    int numGroupFiles = gdata.numFiles;
    int numSubhalos = gdata.totalSubhalos;

    // this is the offset in subhalo index for the ith file,
    // to convert from (file, index) to just an overall index
    int *fileOffset = new int[numGroupFiles];
    fileOffset[0] = 0;
    fileOffset[1] = gdata.numSubhalos;

    for (int i=1; i<numGroupFiles-1; i++)
    {
	gdata = LoadGroupHeader(ps, snap, i, false);
	fileOffset[i+1] = fileOffset[i] + gdata.numSubhalos;
    }

    return fileOffset;
}
