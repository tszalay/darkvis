/*
 * This file contains the actual usage of the global stuff
 * that is referenced from everywhere else
 */

#include "Globals.h"
#include <stdio.h>
#include <string.h>
#include <sstream>

Render *g_Render=0;
State *g_State=0;
UIMain *g_UI=0;
BlockManager *g_Blocks=0;
BlockPriority *g_Priority=0;
SubSelect *g_Select=0;

GlobalOpts *g_Opts=0;

void GlobalOpts::defaults()
{
    cam.zscale = 0.002f;
    cam.scale = 0.08f;
    cam.tscale = 0.01f;

    // set this to ~2 GB
    sys.maxBytes = 2000000000;
    // try only using 500 megs
//    sys.maxBytes = 500000000;

    dbg.printBlocks = false;
    dbg.printPrior = false;
    dbg.printRender = false;

    // set to invalid values, to make sure we load from file
    file.vertexSize = -1;
    file.ndirs = -1;
    file.dirs = NULL;
    file.firstSnap = -1;
    file.lastSnap = -1;
    file.interval = -1;

    // and some default stuff for view as well
    view.minBlockPixels = 100;
    view.camFactor = 0.3f;
    view.animFactor = 0.5f;
    view.forceMin = 0.0f;

    view.minFPS = 0.8f;
    view.maxFPS = 20;

    view.recdt = 0.05;
}

GlobalOpts::GlobalOpts()
{
    defaults();
}

// load and parse system config file
bool GlobalOpts::OpenConfig(string filename)
{
    FILE *file = fopen(filename.c_str(), "r");

    // can use defaults for system config
    if (file == NULL)
    {
	fprintf(stderr,"Error opening system config file %s!\n",filename.c_str());
	fprintf(stderr,"Using default values.\n");
	return false;
    }

    char line[1024];

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
	if (strncmp(line+s, "zoomScale", 6) == 0)
	    cam.zscale = atof(line+v);
	else if (strncmp(line+s, "lookScale", 5) == 0)
	    cam.scale = atof(line+v);
	else if (strncmp(line+s, "timeScale", 6) == 0)
	    cam.tscale = atof(line+v);
	else if (strncmp(line+s, "maxBytes", 8) == 0)
	    sys.maxBytes = atoi(line+v);
	else if (strncmp(line+s, "minBlockPixels", 14) == 0)
	    view.minBlockPixels = atoi(line+v);
	else if (strncmp(line+s, "camFactor", 9) == 0)
	    view.camFactor = atof(line+v);
	else if (strncmp(line+s, "animFactor", 10) == 0)
	    view.animFactor = atof(line+v);
	else if (strncmp(line+s, "forceMin", 8) == 0)
	    view.forceMin = atof(line+v);
	else if (strncmp(line+s, "minFPS", 6) == 0)
	    view.minFPS = atof(line+v);
	else if (strncmp(line+s, "maxFPS", 6) == 0)
	    view.maxFPS = atof(line+v);
	else if (strncmp(line+s, "recdt", 5) == 0)
	    view.recdt = atof(line+v);
    }

    fclose(file);

    // store for saving at close
    cfgFile = filename;

    return true;
}

// load and parse sim options file
bool GlobalOpts::OpenFile(string filename)
{
    FILE *fin = fopen(filename.c_str(), "r");

    // can use defaults for system config
    if (fin == NULL)
    {
	fprintf(stderr,"Error opening simulation file %s!\n",filename.c_str());
	fprintf(stderr,"Nothing to load. Exiting.\n");
	return false;
    }

    char line[1024];
    string dirs[32];
    int curdir = 0;

    while (fgets(line, 1024, fin)!= NULL)
    {
	// find pos of first actual character
	int s = strspn(line, " \t\n\v");
	// skip comment lines
	if (line[s] == '#')
	    continue;
	// and find position of the value
	int v = strcspn(line, "=")+1;

	// otherwise (also know line is null-term.)
	// check for each different param
	if (strncmp(line+s, "vertexSize", 10) == 0)
	    file.vertexSize = atoi(line+v);
	else if (strncmp(line+s, "ndirs", 5) == 0)
	    file.ndirs = atoi(line+v);
	else if (strncmp(line+s, "dir", 3) == 0)
	    dirs[curdir++] = string(line+v, strcspn(line+v,"\n\r"));
	else if (strncmp(line+s, "firstSnap", 9) == 0)
	    file.firstSnap = atoi(line+v);
	else if (strncmp(line+s, "lastSnap", 8) == 0)
	    file.lastSnap = atoi(line+v);
	else if (strncmp(line+s, "interval", 8) == 0)
	    file.interval = atoi(line+v);
    }

    fclose(fin);

    // check to make sure values are valid
    if (file.vertexSize <= 0 || file.ndirs <= 0 
	|| file.firstSnap < 0 || file.lastSnap < 0 
	|| file.lastSnap < file.firstSnap || file.interval <= 0)
    {
	fprintf(stderr,"Error parsing config file. Exiting.\n");
	return false;
    }

    // otherwise, set dirs
    file.dirs = new string[file.ndirs];
    for (int i=0; i<file.ndirs;i++)
	file.dirs[i] = dirs[i];

    printf("Loading every %dth snapshot from %d to %d,\n",
	   file.interval, file.firstSnap, file.lastSnap);
    printf("with a vertex size of %d,\n", file.vertexSize);
    printf("from %d directories:\n", file.ndirs);
    for (int i=0;i<file.ndirs;i++)
	printf("%s\n",file.dirs[i].c_str());

    return true;
}



// helper func
string toString(int t)
{
    ostringstream s;
    s << t; 
    return s.str();
}

