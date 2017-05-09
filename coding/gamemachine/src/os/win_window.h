﻿#ifndef __WIN_WINDOW_H__
#define __WIN_WINDOW_H__
#include "common.h"
#include "gmengine/controllers/gamemachine.h"
BEGIN_NS

#ifdef _WINDOWS

struct WinWindowPrivate
{
	char windowTitle[128];
	LONG left, top, width, height;
	GMint depthBits, stencilBits;

	HWND hWnd;
	HDC hDC;
	HGLRC hRC;
	MSG msg;
	HINSTANCE hInstance;
	// bool fullscreen;
};

// Windows下的Window类
class WinWindow : public IWindow
{
	DEFINE_PRIVATE(WinWindow)

public:
	WinWindow();
	~WinWindow();

public:
	virtual bool createWindow() override;
	virtual GMRect getWindowRect() override;
	virtual bool handleMessages() override;
	virtual void swapBuffers() override;

public:
	HWND hwnd();

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	void dispose();
};

#endif

END_NS
#endif