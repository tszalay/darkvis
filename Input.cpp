#include "Globals.h"
#include "SDL.h"
#include <stdio.h>

// minimum number of pixels we need to move by to drag
#define DRAG_MIN 5


// in case of left-click event
void State::click(int x, int y)
{
    // we are selecting for view
    if (altKey && !shiftKey)
    {
	viewSelect(x,y);
    }
    // adding to selection
    else if (shiftKey && !altKey)
    {
	addSelect(x,y);
    }
    // normal selection
    else
    {
	clearSelect();
	addSelect(x,y);
    }
}


// or drag
void State::drag(int x, int y, int dx, int dy)
{
}


void State::MouseDown(int x, int y, int button)
{
    // set buttons
    if (button == SDL_BUTTON_LEFT)
    {
	buttons[0] = SDL_GetTicks();
	startx = x;
	starty = y;
    }
    if (button == SDL_BUTTON_RIGHT)
    {
	buttons[1] = SDL_GetTicks();
    }
    if (button == SDL_BUTTON_MIDDLE)
    {
	buttons[2] = SDL_GetTicks();
    }

    switch (button)
    {
    case SDL_BUTTON_LEFT:
	// possibly zoom
	if (inputState == IS_LOOK)
	{
	    inputState = IS_ZOOM;
	}
	// or, if clicking, start drag
	else if (inputState == IS_IDLE)
	{
	    inputState = IS_DRAG;
	}
	break;
    case SDL_BUTTON_RIGHT:
	// transition to looking
	if (inputState == IS_IDLE)
	{
	    SDL_ShowCursor(0);
	    inputState = IS_LOOK;
	}
	// or abort drag, and go to zoom
	else if (inputState == IS_DRAG)
	{
	    SDL_ShowCursor(0);
	    inputState = IS_ZOOM;
	}
	break;
    }

    // or, mousewheel, to correspond to a few pixels
    if (button == 4)
	zoomView(-20); 
    if (button == 5)
	zoomView(20);
}

void State::MouseUp(int x, int y, int button)
{
    // set buttons
    if (button == SDL_BUTTON_LEFT) buttons[0] = 0;
    if (button == SDL_BUTTON_RIGHT) buttons[1] = 0;
    if (button == SDL_BUTTON_MIDDLE) buttons[2] = 0;

    switch (button)
    {
    case SDL_BUTTON_LEFT:
	if (inputState == IS_ZOOM)
	{
	    inputState = IS_LOOK;
	}
	else if (inputState == IS_DRAG)
	{	    
	    inputState = IS_IDLE;
	    // we dragged
	    if (abs(x-startx) > DRAG_MIN || abs(y-starty) > DRAG_MIN)
		drag(startx, starty, x-startx, y-starty);
	    else // click on position we first hit
		click(startx, starty);
	}
	break;
    case SDL_BUTTON_RIGHT:
	// release camera, but don't start dragging again
	if (inputState == IS_LOOK || inputState == IS_ZOOM)
	{
	    inputState = IS_IDLE;
	    SDL_ShowCursor(1);
	}
	break;
    }
}

void State::MouseMove(int x, int y, int dx, int dy, int button)
{
    // if we are looking, and check buttons just in case
    if (inputState == IS_LOOK && buttons[1] > 0)
	lookView(dx,dy);
    // same with zooming
    else if (inputState == IS_ZOOM && buttons[0] > 0 && buttons[1] > 0)
	zoomView(dy);	
}

void State::KeyDown(int key)
{
    switch(key)
    {
    case SDLK_ESCAPE:
	Quit();
	break;
    case SDLK_t:
	g_Render->CaptureScreen();
	break;
    case SDLK_p:
	g_Render->TogglePoints();
	break;
    case SDLK_b:
	g_Render->ToggleBoxes();
	break;
    case SDLK_f:
	g_Priority->ToggleFreeze();
	break;
    case SDLK_a:
	g_Render->ToggleAutoscale();
	break;
    case SDLK_r:
	Record(altKey);
	break;
    case SDLK_k:
	AddKeyframe();
	break;

    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
	g_Render->ChangeScoreScale(1.1);
	break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
	g_Render->ChangeScoreScale(0.9);
	break;

    case SDLK_UP:
	Play(5);
	break;
    case SDLK_DOWN:
	Play(-5);
	break;
    case SDLK_LEFT:
	Play(-1);
	break;
    case SDLK_RIGHT:
	Play(1);
	break;

    case SDLK_PAGEUP:
	ChangeSnap(2);
	break;
    case SDLK_PAGEDOWN:
	ChangeSnap(-2);
	break;
    case SDLK_HOME:
	ToFirstSnap();
	break;
    case SDLK_END:
	ToLastSnap();
	break;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
	shiftKey = true;
	break;

    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
	altKey = true;
	break;

    case SDLK_SPACE:
	TogglePlay(1);
	break;
    case SDLK_BACKSPACE:
	TogglePlay(-1);
	break;

    case SDLK_1:
	SetQuality(0);
	break;
    case SDLK_2:
	SetQuality(0.1);
	break;
    case SDLK_3:
	SetQuality(0.2);
	break;
    case SDLK_4:
	SetQuality(0.3);
	break;
    case SDLK_5:
	SetQuality(0.4);
	break;
    case SDLK_6:
	SetQuality(0.5);
	break;
    case SDLK_7:
	SetQuality(0.6);
	break;
    case SDLK_8:
	SetQuality(0.7);
	break;
    case SDLK_9:
	SetQuality(0.8);
	break;
    case SDLK_0:
	SetQuality(1.0);
	break;
    }
}

void State::KeyUp(int key)
{
    switch (key)
    {
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_LEFT:
    case SDLK_RIGHT:
	Stop();
	break;

    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
	shiftKey = false;
	break;

    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LCTRL:
    case SDLK_RCTRL:
	altKey = false;
	break;
    }
}
