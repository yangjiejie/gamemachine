﻿#ifndef __OBJECT_PAINTER_H__
#define __OBJECT_PAINTER_H__
#include "common.h"
#include "gmdatacore/object.h"
#include "gmglshaders.h"
BEGIN_NS

struct IGraphicEngine;
class GMGLGraphicEngine;
class GMGLShaders;
class GMGLShadowMapping;
class GameWorld;
class GMGLObjectPainter : public ObjectPainter
{
public:
	GMGLObjectPainter(IGraphicEngine* engine, GMGLShadowMapping& shadowMapping, Object* obj);

public:
	virtual void transfer() override;
	virtual void draw() override;
	virtual void dispose() override;
	void setWorld(GameWorld* world);

private:
	void setLights(Material& material, Object::ObjectType type);
	void beginTextures(TextureInfo* startTexture, Object::ObjectType type);
	void endTextures(TextureInfo* startTexture);
	void resetTextures(Object::ObjectType type);

private:
	GMGLGraphicEngine* m_engine;
	GMGLShadowMapping& m_shadowMapping;
	GameWorld* m_world;

private:
	bool m_inited;
};

END_NS
#endif