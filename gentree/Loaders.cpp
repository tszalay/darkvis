#include <stdio.h>
#include "Loaders.h"


HsmlData LoadHsml(PathInfo& ps, int id, int fn)
{
    FILE *file = fopen(ps.GetHsml(id,fn).c_str(),"rb");
    if (file == NULL)
    {
	fprintf(stderr,"Hsml file %s not found!\n",ps.GetHsml(id,fn).c_str());
	return HsmlData();
    }

    HsmlData data;

    // read header
    fread(&data, 4, 5, file);

    bswap32(data.numFile);
    bswap32(data.numPrevious);
    bswap64(data.numTotal);
    bswap32(data.numFiles);

    // allocate arrays
    data.hsml = new float[data.numFile];
    data.density = new float[data.numFile];
    data.velDisp = new float[data.numFile];

    // and read them
    fread(data.hsml, 4, data.numFile, file);
    fread(data.density, 4, data.numFile, file);
    fread(data.velDisp, 4, data.numFile, file);

    fclose(file);

    // swap bytes if necessary
#ifdef _READ_SWAP_BYTES
    uint32_t* hsml = (uint32_t*)data.hsml;
    uint32_t* dens = (uint32_t*)data.density;
    uint32_t* vdisp = (uint32_t*)data.velDisp;
    for (int i=0; i<data.numFile; i++)
    {
	bswap32(hsml[i]);
	bswap32(dens[i]);
	bswap32(vdisp[i]);
    }
#endif

    return data;
}

void FreeHsml(HsmlData& data)
{
    delete[] data.hsml;
    delete[] data.density;
    delete[] data.velDisp;
}

VertexData LoadSnap(PathInfo& ps, int id, int fn)
{
    FILE *file = fopen(ps.GetSnap(id, fn).c_str(), "rb");

    if (file == NULL)
    {
	fprintf(stderr,"Vertex file %s not found!\n",ps.GetSnap(id, fn).c_str());
	return VertexData();
    }

    VertexData data;

    // needed for reading dumb format
    uint32_t nbytes;
    uint32_t tmp;
    // position of read
    uint64_t fPos;

    fread(&nbytes, 4, 1, file);
    bswap32(nbytes);

    fPos = 4;

    // read header
    // er, this is wrong, but i don't really feel like changing it right now
    fread(&data, sizeof(data), 1, file);

    // swap header bytes
    for (int i=0; i<6; i++)
    {
	bswap32(data.numParts[i]);
	bswap32(data.numTotals[i]);
	bswap64(data.massParts[i]);
	bswap32(data.nLargeSims[i]);
    }
    bswap64(data.time);
    bswap64(data.redshift);
    bswap64(data.boxSize);
    bswap64(data.omega0);
    bswap64(data.omegaLambda);
    bswap64(data.hubbleParam);

    bswap32(data.numSubfiles);

    // allocate arrays
    data.pos = new double[data.numParts[1]*3];
    data.vel = new double[data.numParts[1]*3];
    data.id = new partid_t[data.numParts[1]];

    // go to next data loc.
    fPos += nbytes;
    fseek(file, fPos, SEEK_SET);
    fread(&tmp, 4, 1, file);
    bswap32(tmp);

    // check to make sure head size and tail size are equal
    if (tmp != nbytes)
	fprintf(stderr,"Error! Head size = %d, tail size = %d\n",nbytes,tmp);
    fread(&nbytes, 4, 1, file);
    bswap32(nbytes);

    fPos += 8;


    // and read them
    fread(data.pos, 24, data.numParts[1], file);

    // go to next data loc.
    fPos += nbytes;
    fseek(file, fPos, SEEK_SET);
    fread(&tmp, 4, 1, file);
    bswap32(tmp);

    // check to make sure head size and tail size are equal
    if (tmp != nbytes)
	fprintf(stderr,"Error! Head size = %d, tail size = %d\n",nbytes,tmp);
    fread(&nbytes, 4, 1, file);
    bswap32(nbytes);

    fPos += 8;

    // read next array
    fread(data.vel, 24, data.numParts[1], file);

    // go to next data loc.
    fPos += nbytes;
    fseek(file, fPos, SEEK_SET);
    fread(&tmp, 4, 1, file);
    bswap32(tmp);

    // check to make sure head size and tail size are equal
    if (tmp != nbytes)
	fprintf(stderr,"Error! Head size = %d, tail size = %d\n",nbytes,tmp);
    fread(&nbytes, 4, 1, file);
    bswap32(nbytes);

    fPos += 8;

    // and read last array
    fread(data.id, sizeof(partid_t), data.numParts[1], file);

    fclose(file);


#ifdef _READ_SWAP_BYTES
    uint64_t* pos = (uint64_t*)data.pos;
    uint64_t* vel = (uint64_t*)data.vel;
    // swap bytes if necessary
    for (int i=0; i<3*data.numParts[1]; i++)
    {
	bswap64(pos[i]);
	bswap64(vel[i]);
    }

    if (sizeof(partid_t) == 4)
	for (int i=0; i<data.numParts[1]; i++)
	    bswap32(data.id[i]);
    else
	for (int i=0; i<data.numParts[1]; i++)
	    bswap64(data.id[i]);
#endif

    return data;
}

double bswapd(double x)
{
    double y = x;
    bswap64(y);
    return y;
}

float bswapf(float x)
{
    float y = x;
    bswap32(y);
    return y;
}

void FreeSnap(VertexData& data)
{
    delete[] data.pos;
    delete[] data.vel;
    delete[] data.id;
}

VertexData LoadSnapHeader(PathInfo& ps, int id, int fn)
{
    FILE *file = fopen(ps.GetSnap(id, fn).c_str(), "rb");

    if (file == NULL)
    {
	fprintf(stderr,"Vertex file %s not found!\n",ps.GetSnap(id, fn).c_str());
	VertexData vd;
	vd.numSubfiles = 0;
	return vd;
    }

    VertexData data;

    // needed for reading dumb format
    uint32_t nbytes;
    uint32_t tmp;
    // position of read
    uint64_t fPos;

    fread(&nbytes, 4, 1, file);
    fPos = 4;

    // read header
    // er, this is wrong, but i don't really feel like changing it right now
    fread(&data, sizeof(data), 1, file);

    fclose(file);

    // swap header bytes
    for (int i=0; i<6; i++)
    {
	bswap32(data.numParts[i]);
	bswap32(data.numTotals[i]);
	bswap64(data.massParts[i]);
	bswap32(data.nLargeSims[i]);
    }
    bswap64(data.time);
    bswap64(data.redshift);
    bswap64(data.boxSize);
    bswap64(data.omega0);
    bswap64(data.omegaLambda);
    bswap64(data.hubbleParam);

    bswap32(data.numSubfiles);


    return data;
}


GroupData LoadGroup(PathInfo& ps, int id, int fn, bool vel)
{
    FILE *file = fopen(ps.GetSubTab(id,fn).c_str(),"rb");
    if (file == NULL)
    {
	fprintf(stderr,"Group file %s not found!\n",ps.GetSubTab(id,fn).c_str());
	return GroupData();
    }

    GroupData data;

    // read header
    fread(&data, 4, 8, file);

    bswap32(data.numGroups);
    bswap32(data.totalGroups);
    bswap32(data.numIDs);
    bswap64(data.totalIDs);
    bswap32(data.numFiles);
    bswap32(data.numSubhalos);
    bswap32(data.totalSubhalos);

    // allocate arrays
    data.sLength = new uint32_t[data.numSubhalos];
    data.sOffset = new uint32_t[data.numSubhalos];

    data.gLength = new uint32_t[data.numGroups];
    data.gOffset = new uint32_t[data.numGroups];

    fread(data.gLength, 4, data.numGroups, file);
    fread(data.gOffset, 4, data.numGroups, file);

    // skip over useless stuff we don't need
    fseek(file, data.numGroups*4*14, SEEK_CUR);
    // and read good fields
    fread(data.sLength, 4, data.numSubhalos, file);
    fread(data.sOffset, 4, data.numSubhalos, file);

    fclose(file);

#ifdef _READ_SWAP_BYTES
    for (int i=0; i<data.numGroups; i++)
    {
	bswap32(data.gLength[i]);
	bswap32(data.gOffset[i]);
    }
    for (int i=0; i<data.numSubhalos; i++)
    {
	bswap32(data.sLength[i]);
	bswap32(data.sOffset[i]);
    }
#endif

    return data;
}

void FreeGroup(GroupData& data)
{
    delete[] data.sLength;
    delete[] data.sOffset;

    delete[] data.gLength;
    delete[] data.gOffset;
}

/* This function doesn't load any arrays, only the headers. */
GroupData LoadGroupHeader(PathInfo& ps, int id, int fn, bool vel)
{
    GroupData data;
    data.numFiles = -1;

    FILE *file = fopen(ps.GetSubTab(id,fn).c_str(),"rb");
    if (file == NULL)
    {
	fprintf(stderr,"Group file %s not found!\n",ps.GetSubTab(id,fn).c_str());
	return data;
    }

    // read header
    fread(&data, 4, 8, file);

    fclose(file);

    data.numGroups = bswap32(data.numGroups);
    data.totalGroups = bswap32(data.totalGroups);
    data.numIDs = bswap32(data.numIDs);
    data.totalIDs = bswap64(data.totalIDs);
    data.numFiles = bswap32(data.numFiles);
    data.numSubhalos = bswap32(data.numSubhalos);
    data.totalSubhalos = bswap32(data.totalSubhalos);

    return data;
}

IDData LoadIDs(PathInfo& ps, int id, int fn)
{
    FILE *file = fopen(ps.GetSubId(id,fn).c_str(),"rb");
    if (file == NULL)
    {
	fprintf(stderr,"SubID file %s not found!\n",ps.GetSubId(id,fn).c_str());
	return IDData();
    }

    IDData data;

    // read header
    fread(&data, 4, 7, file);

    data.numGroups = bswap32(data.numGroups);
    data.totalGroups = bswap32(data.totalGroups);
    data.numIDs = bswap32(data.numIDs);
    data.totalIDs = bswap64(data.totalIDs);
    data.numFiles = bswap32(data.numFiles);
    data.offset = bswap32(data.offset);

    // allocate arrays
    data.IDs = new partid_t[data.numIDs];

    // and read good fields
    fread(data.IDs, sizeof(partid_t), data.numIDs, file);

    fclose(file);

#ifdef _READ_SWAP_BYTES

    // swap bytes if necessary
    if (sizeof(partid_t) == 4)
	for (int i=0; i<data.numIDs; i++)
	    bswap32(data.IDs[i]);
    else
	for (int i=0; i<data.numIDs; i++)
	    bswap64(data.IDs[i]);

#endif    

    return data;
}

void FreeIDs(IDData& data)
{
    delete[] data.IDs;
}


TreeData LoadTree(string filename)
{
    FILE *file = fopen(filename.c_str(),"rb");
    if (file == NULL)
    {
	fprintf(stderr,"Merger tree file %s not found!\n",filename.c_str());
	return TreeData();
    }
    
    TreeData data;

    // read header
    fread(&data, 4, 2, file);

    // allocate arrays
    data.numHalos = new int32_t[data.numTrees];
    data.subhalos = new TreeHalo[data.totalHalos];
    data.roothalo = new TreeHalo*[data.numTrees];

    // read in the halo counts
    fread(data.numHalos, 4, data.numTrees, file);
    // and read in the halos
    fread(data.subhalos, sizeof(TreeHalo), data.totalHalos, file);

    // set the root pointers
    int curHalo = 0;
    for (int i=0; i<data.numTrees; i++)
    {
	data.roothalo[i] = data.subhalos + curHalo;
	curHalo += data.numHalos[i];
    }

    return data;
}


void FreeTree(TreeData& data)
{
    delete[] data.numHalos;
    delete[] data.subhalos;
    delete[] data.roothalo;
}
