#ifndef _GADGET_H_
#define _GADGET_H_

// do this because units are in 100 Mpc/h
#define HUBBLE 100

// for endian-swapping
#include <byteswap.h>

/* CHANGE THIS FLAG TO READ BIG-ENDIAN FILES */
#define _READ_SWAP_BYTES

// these are done this way so doubles and floats can be converted as well
#ifdef _READ_SWAP_BYTES
#define bswap16(x) *(uint16_t*)(&x) = bswap_16(*(uint16_t*)&x)
#define bswap32(x) *(uint32_t*)(&x) = bswap_32(*(uint32_t*)&x)
#define bswap64(x) *(uint64_t*)(&x) = bswap_64(*(uint64_t*)&x)

#else
#define bswap16(x) (x)
#define bswap32(x) (x)
#define bswap64(x) (x)

#endif



/************************************************************/
/************************************************************/
/* VERY IMPORTANT!!! THIS IS TYPE FOR IDS IN SNAPSHOT FILES */
/************************************************************/
/************************************************************/
typedef uint64_t partid_t;




/* Contains the format and relevant structure for data in
   hsml files. The arrays should be of length numFile. */
   
struct HsmlData
{
    uint32_t numFile; // number of data points in this file
    uint32_t numPrevious; // number of data points before this file
    uint64_t numTotal; // total number of points
    uint32_t numFiles; // number of files

    float *hsml; // arrays of length numFile to contain actual values
    float *density;
    float *velDisp;
};


/* Contains format and header for vertex/high res position
   files. Arrays should be large enough to hold numParts for
   each type we read, *3 for the coordinates. */

struct VertexData
{
    uint32_t numParts[6]; // number of particles for each type
    double massParts[6]; // mass of each particle per type
    double time; // scale factor a
    double redshift; // current redshift
    uint32_t flag_sfr;
    uint32_t flag_feedback;
    uint32_t numTotals[6]; // total particles per type
    uint32_t flag_cooling; 
    uint32_t numSubfiles; // duh
    double boxSize; // size of containing box
    double omega0; // matter density at z = 0
    double omegaLambda; // in units of critical density
    double hubbleParam; // is 'h' in units of 100 km s^-1 Mpc^-1
    uint32_t flag_age; // unused
    uint32_t flag_metals; // unused
    uint32_t nLargeSims[6]; // hold MSB if > 2^32 particles
    uint32_t flag_entr_ics; // initial conditions contain other stuff?

    double *pos; // described above
    double *vel;
    partid_t *id;
};


/* Contains format and header for halo/subhalo group files. */

struct GroupData
{
    uint32_t numGroups; // groups in this file
    uint32_t totalGroups; // total number of groups
    uint32_t numIDs; // IDs in this index (but in other file)
    uint64_t totalIDs; // total IDs (contained in halos)
    uint32_t numFiles; // number of split files
    uint32_t numSubhalos; // number of subhalos in this file
    uint32_t totalSubhalos; // and total number of subhalos

    // following arrays all should be sized numGroups
    uint32_t *gLength; // number of IDs in group
    uint32_t *gOffset; // offset to group in ID file
    float *gMass; // total mass
    float *gPos;
    float *M_Mean200;
    float *R_Mean200;
    float *M_Crit200;
    float *R_Crit200;
    float *M_TopHat200;
    float *R_TopHat200;

/* ----- VELOCITY DISPERSIONS MAY NOT BE IN FILE, CHECK FIRST ----- */
    float *VelDisp_Mean200;
    float *VelDisp_Crit200;
    float *VelDisp_TopHat200;
/* ---------------------------------------------------------------- */

    uint32_t *contamCount;
    float *contamMass;
    uint32_t *numSubs; // number of subhalos
    uint32_t *firstSub; // first subhalo index

    // all following sized numSubgroups
    uint32_t *sLength; // number of particles in subhalo
    uint32_t *sOffset; // offset to particles in subhalo
    float *sMass;
    // following are three-coord
    float *sPos;
    float *sVel;
    float *CM;
    float *Spin;
    // back to one-coord
    float *velDisp;
    float *velMax;
    float *velMaxRad;
    float *halfMassRad;
    partid_t *IDMostBound;
    uint32_t *GrNr;    
};



/* Contains the format and relevant structure for data in
   particle ID files. These are pretty basic. */

struct IDData
{
    uint32_t numGroups; // number of groups in file
    uint32_t totalGroups; // and total
    uint32_t numIDs; // IDs in this file
    uint64_t totalIDs; // totals
    uint32_t numFiles; // number of files
    uint32_t offset; // index of first particle?
    
    partid_t *IDs; // duh
};


/* The format for a single halo in a tree file */

struct TreeHalo
{
    int32_t Descendant;
    int32_t FirstProgenitor;
    int32_t NextProgenitor;
    int32_t FirstHaloInFOFgroup;
    int32_t NextHaloInFOFgroup;
    int32_t Length;
    float M_Mean200;
    float M_Crit200;
    float M_TopHat;
    float Pos[3];
    float Vel[3];
    float VelDisp;
    float Vmax;
    float Spin[3];
    uint64_t MostBoundID;
    int32_t SnapNum;
    int32_t FileNum;
    int32_t SubhaloIndex;
    float SubHalfMass;
};

/* Contains the format for a halo tree file. */

struct TreeData
{
    int32_t numTrees; // # of distinct trees in this file
    int32_t totalHalos; // total # of halos in file

    int32_t *numHalos; // sized numTrees, number of halos in each tree
    TreeHalo *subhalos; // sized totalHalos
    // this is here for convenience, and points to the root halo of each tree
    // but it is not in the original file.
    TreeHalo ** roothalo;
};

#endif
