﻿#ifndef __GAMEMACHINE_H__
#define __GAMEMACHINE_H__
#include "common.h"
#include "graphic_engine.h"
#include "utilities/autoptr.h"
#include <queue>
#include "utilities/fpscounter.h"
BEGIN_NS

enum GameMachineEvent
{
	GM_EVENT_RENDER,
	GM_EVENT_ACTIVATE_MESSAGE,
};

enum GameMachineMessage
{
	GM_MESSAGE_EXIT,
};

struct IWindow
{
	virtual ~IWindow() {}
	virtual bool createWindow() = 0;
	virtual GMRect getWindowRect() = 0;
	virtual bool handleMessages() = 0;
	virtual void swapBuffers() = 0;
};

class GameMachine;
struct IGameHandler
{
	virtual void setGameMachine(GameMachine* gm) = 0;
	virtual GameMachine* getGameMachine() = 0;
	virtual void init() = 0;
	virtual void event(GameMachineEvent evt) = 0;
	virtual void logicalFrame(GMfloat elapsed) = 0;
	virtual void onExit() = 0;
	virtual bool isWindowActivate() = 0;
};

struct IFactory;
struct IGameHandler;
class GlyphManager;

struct GameMachinePrivate
{
	FPSCounter fpsCounter;
	GraphicSettings settings;
	AutoPtr<IWindow> window;
	AutoPtr<IFactory> factory;
	AutoPtr<IGraphicEngine> engine;
	AutoPtr<IGameHandler> gameHandler;
	AutoPtr<GlyphManager> glyphManager;
	std::queue<GameMachineMessage> messageQueue;
};

class GameMachine
{
	DEFINE_PRIVATE(GameMachine)

	enum
	{
		MAX_KEY_STATE_BITS = 512,
	};

public:
	GameMachine(
		GraphicSettings settings,
		AUTORELEASE IWindow* window,
		AUTORELEASE IFactory* factory,
		AUTORELEASE IGameHandler* gameHandler
	);

public:
	IGraphicEngine* getGraphicEngine();
	IWindow* getWindow();
	IFactory* getFactory();
	GraphicSettings& getSettings();
	GlyphManager* getGlyphManager();
	void startGameMachine();
	void postMessage(GameMachineMessage msg);
	

private:
	void init();
	void initDebugger();
	bool handleMessages();
};

END_NS
#endif