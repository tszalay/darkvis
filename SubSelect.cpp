#include "Globals.h"
#include "SubSelect.h"
#include <stdio.h>


/* Updates the position of a halo in a different snap using merger tree info. */
void SubSelect::UpdateHalo(int& snap, Halo& halo)
{
    // get the current time info
    double curTime, dloga, dt;
    int cursnap;
    curTime = g_State->GetTime(dloga, dt, cursnap);

    if (snap == cursnap)
	return;

    // go backwards?
    if (cursnap < snap)
    {
	// while we can load
	while (cursnap < snap && halo.mainProgenitor != -1)
	{
	    // load next halo
	    halo = loadSingleHalo(snap-1, halo.mainProgenitor);
	    snap--;
	}
    }
    else
    {
	// while we can load
	while (snap < cursnap && halo.mainDescendant != -1)
	{
	    // load next halo
	    halo = loadSingleHalo(snap+1, halo.mainDescendant);
	    snap++;
	}	
    }
}


/* Selects a halo intersecting a given ray, maximizing the halo's screen area */
Halo SubSelect::SelectHalo(Vector3 start, Vector3 direction)
{
    // get the current dt
    double curTime, dloga, dt;
    int snap;
    curTime = g_State->GetTime(dloga, dt, snap);

    // alright. make sure current snapshot is loaded
    loadHaloTable(snap);

    // now, normalize direction vector
    direction = Vector3::Normalize(direction);

    float minVal = 1e30;
    int minIndex = -1;

    // now loop and pick
    for (int i=0; i<numHalos; i++)
    {
	Vector3 pos = curHalos[i].pos + curHalos[i].vel * dloga
	    + curHalos[i].acc * (dloga*dloga*0.5);
	Vector3 d = pos - start;
	// make sure it is in the direction of the ray
	if (Vector3::Dot(d,direction) < 0)
	    continue;

	float dist = Vector3::Length(Vector3::Cross(d, direction));
	// skip if we didn't click inside the halo
	if (dist > 2*curHalos[i].radius)
	    continue;
	// otherwise...
	float farea = curHalos[i].radius*curHalos[i].radius/dist;
	// divide area by distance to get relative area
	farea = farea / Vector3::Length(d);
	// and...
	if (farea < minVal)
	{
	    minVal = farea;
	    minIndex = i;
	}
    }

    // none selected
    if (minIndex == -1)
    {
	Halo halo;
	halo.pointIndex = -1;
	return halo;
    }

    printf("Selected halo %d.\n", minIndex);

    return curHalos[minIndex];
}

/* Loads a buncha pids */
std::vector<uint32_t> SubSelect::GetHaloPoints(int snap, Halo& halo)
{
    int start = halo.pointIndex;
    int count = halo.pointCount;

    string path = g_Opts->file.dirs[0];
    int snapnum = g_Opts->file.firstSnap + snap*g_Opts->file.interval;
    path += "/subid_" + toString(snapnum);
    FILE* file = fopen(path.c_str(),"rb");

    if (file == NULL)
	return vector<uint32_t>();

    // seek to correct location
    fseek(file, 4*start, SEEK_SET);

    // create empty vector
    vector<uint32_t> subids(count);

    // and read subids
    fread(&subids[0], 4, count, file);
    fclose(file);

    return subids;
}

/* Loads an entire halo table. */
void SubSelect::loadHaloTable(int snap)
{
    // is current snap loaded already?
    if (snap == loadedSnap)
	return;

    // first, free old halos
    freeHalos();

    string path = g_Opts->file.dirs[0];
    int snapnum = g_Opts->file.firstSnap + snap*g_Opts->file.interval;
    path += "/halos_" + toString(snapnum);
    FILE* file = fopen(path.c_str(),"rb");

    if (file == NULL)
    {
	numHalos = 0;
	loadedSnap = -1;
	return;
    }

    // read halo count
    int32_t num;
    fread(&num, 4, 1, file);

    curHalos = new Halo[num];
    numHalos = num;

    // and load new ones
    fread(curHalos, sizeof(Halo), num, file);
    fclose(file);
    loadedSnap = snap;
    printf("Loaded halo table %d with %d halos.\n",loadedSnap,num);
}

/* Loads a single halo, from a file or from memory. */
Halo SubSelect::loadSingleHalo(int snap, int index)
{
    // already loaded?
    if (snap == loadedSnap)
	return curHalos[index];

    string path = g_Opts->file.dirs[0];
    int snapnum = g_Opts->file.firstSnap + snap*g_Opts->file.interval;
    path += "/halos_" + toString(snapnum);
    FILE* file = fopen(path.c_str(),"rb");

    if (file == NULL)
    {
	return Halo();
    }

    // now seek to halo location
    fseek(file, 4 + index*sizeof(Halo), SEEK_SET);

    Halo halo;
    // and load new ones
    fread(&halo, sizeof(Halo), 1, file);
    fclose(file);

    return halo;
}

/* Frees halos, if loaded. */
void SubSelect::freeHalos()
{
    if (curHalos != NULL)
	delete[] curHalos;
    curHalos = NULL;
    loadedSnap = -1;
}

SubSelect::SubSelect()
{
    curHalos = NULL;
    numHalos = 0;
    loadedSnap = -1;
}

SubSelect::~SubSelect()
{
    freeHalos();
}
