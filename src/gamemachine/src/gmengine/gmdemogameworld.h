﻿#ifndef __GMDEMOGAMEWORLD_H__
#define __GMDEMOGAMEWORLD_H__
#include <gmcommon.h>
#include "gmgameworld.h"
#include "gameobjects/gmgameobject.h"
BEGIN_NS

// 提供一些示例的世界

GM_PRIVATE_OBJECT(GMDemoGameWorld)
{
	HashMap<GMString, GMGameObject*, GMStringHashFunctor> renderList;
	Map<const GMGameObject*, GMString> renderListInv;
	bool sorted = false;
};

class GM_EXPORT GMDemoGameWorld : public GMGameWorld
{
	GM_DECLARE_PRIVATE_AND_BASE(GMDemoGameWorld, GMGameWorld)

public:
	GMDemoGameWorld(const IRenderContext* context);
	~GMDemoGameWorld();

public:
	virtual bool removeObject(GMGameObject* obj) override;

public:
	bool addObject(const GMString& name, AUTORELEASE GMGameObject* obj);
	bool removeObject(const GMString& name);
	GMGameObject* findGameObject(const GMString& name);
	bool findGameObject(const GMGameObject* obj, REF GMString& name);

private:
	// 这个应该由addObject来转调，addObject会传入一个对象的名称
	using GMGameWorld::addObjectAndInit;
};

END_NS
#endif
