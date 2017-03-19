#ifndef _FORMATS_H_
#define _FORMATS_H_

#include "xstdint.h"
#include <string>
#include <sstream>

using namespace std;

// function for string casting
template<typename T>
string toString(T t)
{
    ostringstream s;
    s << t; 
    return s.str();
}

/* Contains path info and various outputs */
struct PathInfo
{
    string base;
    string name;

    PathInfo(string base, string name)
    {
	this->base = base;
	this->name = name;
    }

    // returns correctly formatted ids, eg. 001-999, 1000+
    string FormatId(int id)
    {
	if (id < 10)
	    return "00" + toString<int>(id);
	else if (id < 100)
	    return "0" + toString<int>(id);
	else
	    return toString<int>(id);
    }

    string GetHsml(int id, int file)
    {
	return base + "hsmldir_" + FormatId(id) + 
	    "/hsml_" + FormatId(id) + "." + toString<int>(file);
    }

    string GetSubTab(int id, int file)
    {
	return base + "groups_" + FormatId(id) + 
	    "/subhalo_tab_" + FormatId(id) + "." + toString<int>(file);
    }

    string GetSubId(int id, int file)
    {
	return base + "groups_" + FormatId(id) + 
	    "/subhalo_ids_" + FormatId(id) + "." + toString<int>(file);
    }

    string GetSnap(int id, int file)
    {
	return base + "snapdir_" + FormatId(id) + 
	    "/snap_" + name + "_" + FormatId(id) + "." + toString<int>(file);
    }

    string GetTree(int id, int spacing)
    {
	return base + "treedata/trees_sf" + toString<int>(spacing) + 
	    "_" + FormatId(id) + ".0";
    }
};


/* Contains a file location and a temp path (aka dest path),
   for swapping read/write between disks. */
struct PathPair
{
    string location;
    string temp;

    void swap()
    {
	string tmp = location;
	location = temp;
	temp = tmp;
    }
};


/* This is just a snapshot info struct. Doesn't actually get saved to a file. */
struct SnapHeader
{
    uint64_t numTotal; // total number of IDs
    int32_t snap; // # of snap
    char filename[512]; // location of snap
    double mass; // mass of each particle
    double time; // all these are just taken from snaps
    double redshift; 
    double boxSize;
    double omega0;
    double omegaLambda;
    double hubbleParam;
    // these are min and max positions
    double minpos[3];
    double maxpos[3];

    SnapHeader()
    {
	snap = -1;
	filename[0] = 0;
    }

    SnapHeader(string fname)
    {
	FILE *file = fopen(fname.c_str(),"rb");
	if (file == NULL)
	{
	    filename[0] = 0;
	    snap = -1;
	    return;
	}
	fread(this, sizeof(SnapHeader), 1, file);
	fclose(file);
    }

    void SetFilename(string s)
    {
	strncpy(filename, s.c_str(), 512);
    }

    void Save()
    {
	char fname[512];
	sprintf(fname,"%s_info",filename);
	FILE *file = fopen(fname,"wb");
	fwrite(this, sizeof(SnapHeader), 1, file);
	fclose(file);
    }
};

/* This is the vertex format for the
   first stage of operations, sorting by pid. */
// this is 68 bytes total... 
struct VertexA
{
    uint64_t pid; // original particle ID
    double pos[3];
    double vel[3];
    float hsml;
    float densq; // squared local density
    float vdisp; // local vel. dispersion multiplied by densq

    // comparison operator, for sorts
    bool operator< (const VertexA& b) const
    {
	return pid < b.pid;
    }
};

/* This is the vertex format for the
   second stage of operations, sorting by coord. 
   It includes accelerations as well, once information
   from next snapshot is used as well. */
// this is 100 bytes total... 
struct VertexB
{
    uint64_t pid; // original particle ID
    uint64_t coord; // octtree index
    double pos[3];
    double vel[3];
    double acc[3];
    float hsml;
    float densq;
    float vdisp;
    float nhsml;
    float ndensq;
    float nvdisp;

    // comparison operator, for sorts
    bool operator< (const VertexB& b) const
    {
	return coord < b.coord;
    }
};

// compressed output vertex format
// 32 bytes
struct __attribute__ ((__packed__)) OutVertex
{
    uint32_t pid;
// these are quite essential
    uint16_t pos[3];
    uint16_t vel[3];
    uint16_t acc[3];
// for following, we store current and next vals
// and interpolate linearly
// use only 8 bits, since range is 1...~30 pixels
    uint8_t hsml;
    uint8_t nhsml;
// and these two
    uint16_t densq;
    uint16_t vdisp;
    uint16_t ndensq;
    uint16_t nvdisp;
};


// output block header format
struct __attribute__ ((__packed__)) OutBlock
{
    uint16_t pos[3];
    uint16_t depth; // size is TreeSize/(1<<depth)
    uint32_t count;
    uint64_t childLocation; // location in file of first child
    uint64_t childLength; // length in file of all children (bytes)
    int16_t childFile; // which file contains child?
    int16_t childFlags; // bitwise flags telling us which children exist
    float mins[9]; // vx, vy, vz, ax, ay, az, hsml, density, veldisp
    float scales[9]; // ditto
    // note: these are only for use once loaded into memory, but they're
    // stored here so we can read completely sequentially
    // and they're stored as 4 bytes, just in case we're processing on opa cluster
    uint32_t childPtr[8]; // pointers to children, not set in this program
};

// describes an outputted blockfile
struct __attribute__ ((__packed__)) BlockFile
{
    int32_t snapnum; // number of snapshot
    int32_t firstFile; // subfile containing first block
    uint64_t firstLocation; // file location of first block
    uint64_t firstLength; // file length of first block
    double time; // time of snapshot, as log(a)
    double pos[3]; // min. of block in each dim
    double scale[3]; // size of block in each dim

    void Save(string filename)
    {
	FILE *file = fopen(filename.c_str(),"wb");
	fwrite(this, sizeof(BlockFile), 1, file);
	fclose(file);
    }
};


/* Self-descriptive. Used for sorting points by pid. */
struct GroupVertex
{
    uint64_t pid; // original particle id
    int32_t subid; // subhalo id, duh (set to -1 for non-subhalo fof particles)
    int32_t fofid; // duh

    // comparison operator, for sorts
    bool operator< (const GroupVertex& b) const
    {
	return pid < b.pid;
    }
};

struct GroupVertexB
{
    uint64_t pid; // original particle id
    int32_t subid; // subhalo id, duh (set to -1 for non-subhalo fof particles)
    int32_t fofid; // duh

    bool operator< (const GroupVertexB& b) const
    {
	// we want to put points with subid of -1 at the end
	if (subid == -1)
	{
	    // if both -1, compare by fofid
	    if (b.subid == -1)
		return fofid < b.fofid;
	    // otherwise, i am larger
	    return false;
	}
	if (b.subid == -1)
	    // we know my subid is not -1
	    return true;
	return subid < b.subid;
    }
};


/* Each snapshot's halo list is just a file with a list of these arrays.
   (and a header with the total number of subhalos) */
struct __attribute__ ((__packed__)) OutHalo
{
    int32_t fofGroup; // the fof group index of this halo, in current snapshot
    int32_t mainProgenitor; // the main progenitor index in prev. snapshot
    int32_t mainDescendant; // main descendant in next snapshot
    // starting point index (not in bytes, just index) and count
    int32_t pointIndex;
    int32_t pointCount;

    // now here come the actual halo properties
    float pos[3];
    float vel[3];
    float acc[3];
    // acceleration etc. is interpolated using info from main progenitor
    // for camera tracking. for selection, it is not used.

    float radius;
    // and mass is just number of points, duh
};

#endif
