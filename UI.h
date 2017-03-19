#ifndef _UI_H_
#define _UI_H_

#include <vector>

#ifndef WINDOWS
using namespace std;
#endif

class UIControl
{
private:
    bool isVisible;
    bool isEnabled;

public:
    virtual void Show() = 0;
    virtual void Hide() = 0;

    virtual void Enable() = 0;
    virtual void Disable() = 0;

    virtual void Draw() = 0;

    virtual bool MouseDown(int x, int y, int button) = 0;
    virtual bool MouseUp(int x, int y, int button) = 0;
    virtual bool MouseMove(int x, int y, int button) = 0;
};

class UIContainer
{
private:
    vector<UIControl*> children;

    float offsetX;
    float offsetY;

public:
    UIContainer();
    
    void Add(UIControl *obj)
    {
	children.push_back(obj);
    }

    void Draw()
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    (*it)->Draw();
	    it++;
	}
    }

    void Show()
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    (*it)->Show();
	    it++;
	}	
    }

    void Hide()
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    (*it)->Hide();
	    it++;
	}
    }

    bool MouseDown(int x, int y, int button)
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    if ((*it)->MouseDown(x,y,button))
		return true;
	    it++;
	}
    }

    bool MouseUp(int x, int y, int button)
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    if ((*it)->MouseUp(x,y,button))
		return true;
	    it++;
	}
    }

    bool MouseMove(int x, int y, int button)
    {
	vector<UIControl*>::iterator it = children.begin();
	while (it != children.end())
	{
	    if ((*it)->MouseMove(x,y,button))
		return true;
	    it++;
	}
    }    
};

class UIMain
{
private:
    vector<UIContainer*> active;

public:

    UIMain() : active() {}

    void ShowContainer(UIContainer* cont)
    {
	active.push_back(cont);
	cont->Show();
    }

    void HideContainer(UIContainer* cont)
    {
	if (active.back() == cont)
	{
	    active.pop_back();
	    cont->Hide();
	}
	else
	{
	    // an error has occured!
	    // we only want to close top-level containers
	}
    }

    void Draw()
    {
	// render from the bottom up
	vector<UIContainer*>::iterator it = active.begin();
	while (it != active.end())
	{
	    (*it)->Draw();
	    it++;
	}
    }

    bool MouseDown(int x, int y, int button)
    {
	if (active.size() == 0)
	    return false;
	return active.back()->MouseDown(x,y,button);
    }

    bool MouseUp(int x, int y, int button)
    {
	if (active.size() == 0)
	    return false;
	return active.back()->MouseUp(x,y,button);
    }

    bool MouseMove(int x, int y, int button)
    {	
	if (active.size() == 0)
	    return false;
	return active.back()->MouseMove(x,y,button);
    }
};

#endif
