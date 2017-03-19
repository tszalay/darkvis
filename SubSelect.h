#ifndef _SUBSELECT_H_
#define _SUBSELECT_H_

#include "Globals.h"
#include "Vec.h"
#include <vector>

class SubSelect
{
    // halo table for current snapshot, if loaded
    Halo* curHalos;
    // and how many
    int numHalos;
    // and snapshot for which we loaded ^^ array
    int loadedSnap;


    // loads a specified halo file into memory
    void loadHaloTable(int snap);

    // or just a specific halo
    Halo loadSingleHalo(int snap, int index);

    // frees halo table
    void freeHalos();

public:

    SubSelect();
    ~SubSelect();

    // selects a halo based on a start location and direction
    Halo SelectHalo(Vector3 start, Vector3 direction);
    
    // same as above, but returns the pids contained in halo
    std::vector<uint32_t> GetHaloPoints(int snap, Halo& halo);

    // attempts to update the halo by reading from next file, if snap chagnes
    void UpdateHalo(int& snap, Halo& halo);
};

#endif
