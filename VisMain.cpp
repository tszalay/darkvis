#include <stdio.h>
#include <string.h>
#include "SDL.h"
#include "SDL_thread.h"

#include "Globals.h"

void DoEvents();
void MainLoop();
void initGlobals();
int main(int argc, char ** argv);

// the main rendering/input handling loop
void MainLoop()
{
// while DGA mouse is buggy in XFree 4.0, do this before any other SDL init:
#ifdef linux
    putenv("SDL_VIDEO_X11_DGAMOUSE=0");
#endif 

    SDL_Init(SDL_INIT_VIDEO);    
    g_Render->Init();
    
    // and here is the main loop
    while (!g_State->IsQuit())
    {
	int start = SDL_GetTicks();
	// draw stuff
	g_Render->Draw();
	if (g_Opts->dbg.printRender)
	    printf("Render time: %d\n",SDL_GetTicks()-start);

	start = SDL_GetTicks();
	// free blocks we have recently deleted
	g_Blocks->FreeDeadBlocks();

	// and recompute all priorities
	g_Priority->Recompute();
	if (g_Opts->dbg.printPrior)
	    printf("Priority time: %d\n",SDL_GetTicks()-start);

	// durrr
	DoEvents();

	// and lastly, update state
	g_State->Update();
    }

    g_Render->Cleanup();
}

// process all SDL events we care about
void DoEvents()
{
    SDL_Event event;
    
    while ( SDL_PollEvent(&event) ) {
	switch(event.type) {
        case SDL_VIDEORESIZE:
	    g_Render->Resize(event.resize.w,event.resize.h);
	    break;
	case SDL_MOUSEBUTTONDOWN:
	    g_State->MouseDown(event.button.x,event.button.y,event.button.button);
	    break;
	case SDL_MOUSEBUTTONUP:
	    g_State->MouseUp(event.button.x,event.button.y,event.button.button);
	    break;
        case SDL_MOUSEMOTION:
	    g_State->MouseMove(event.motion.x,event.motion.y,
			       event.motion.xrel, event.motion.yrel,
			       event.motion.state);
			       break;
	case SDL_KEYDOWN:
	    g_State->KeyDown(event.key.keysym.sym);
	    break;
	case SDL_KEYUP:
	    g_State->KeyUp(event.key.keysym.sym);
	    break;
        case SDL_QUIT:
	    g_State->Quit();
	    break;
	}
    }
}

// initializes all g_*
void initGlobals(string sfile, string dfile)
{
    g_Opts = new GlobalOpts();
    g_Opts->OpenConfig(sfile);
    if (!g_Opts->OpenFile(dfile))
    {
	fflush(stderr);
	exit(1);
    }

    g_Render = new Render();
    g_State = new State();
    g_UI = new UIMain();
    g_Blocks = new BlockManager();
    g_Priority = new BlockPriority();
    g_Select = new SubSelect();

    // load snapshot headers
    g_Blocks->LoadSnapshots();

    // and set default values
    g_State->Initialize();
}

int main(int argc, char *argv[])
{
    // default file paths
    string cfgfile = CFGFILE;
    string datfile = DATFILE;

    bool pb=false;
    bool pp=false;
    bool pr=false;

    // check cmd line opts
    for (int i=1; i<argc; i++)
    {
	// load a different input data set
	if (strncmp(argv[i],"-dat",5) == 0 && i+1 < argc)
	    datfile = string(argv[i+1]);
	if (strncmp(argv[i],"-cfg",4) == 0 && i+1 < argc)
	    cfgfile = string(argv[i+1]);
	// debug flags
	if (strncmp(argv[i],"-v",2) == 0)
	{
	    if (strchr(argv[i],'b') != NULL)
		pb = true;
	    if (strchr(argv[i],'p') != NULL)
		pp = true;
	    if (strchr(argv[i],'r') != NULL)
		pr = true;
	}
    }

    // create all global classes
    initGlobals(cfgfile, datfile);

    g_Opts->dbg.printBlocks = pb;
    g_Opts->dbg.printPrior = pp;
    g_Opts->dbg.printRender = pr;

    // open files and start loading
    if (g_Blocks->OpenFiles())
    {
	printf("Block files opened successfully.\n");
    }
    else
    {
	printf("Error opening files!\n");
	fflush(stdout);
	exit(1);
    }
    g_Blocks->StartThreads();

    // start rendering as well
    MainLoop();

    // cleanups
    g_Blocks->CloseFiles();
    SDL_Quit();
    
    return 0;
}
