﻿#ifndef __GMDX11RENDERERS_H__
#define __GMDX11RENDERERS_H__
#include <gmcommon.h>
#include <gamemachine.h>
#include "gmdx11graphic_engine.h"
BEGIN_NS

GM_PRIVATE_OBJECT(GMDx11Renderer)
{
	GMComPtr<ID3D11InputLayout> inputLayout;
	GMShader* shader = nullptr;
	GMComPtr<ID3DX11Effect> effect;
	ID3DX11EffectTechnique* technique = nullptr;
	ID3DX11EffectRasterizerVariable* rasterizer = nullptr;
};

class GMDx11Renderer : public IRenderer
{
	DECLARE_PRIVATE(GMDx11Renderer)

public:
	GMDx11Renderer();

public:
	virtual void beginModel(GMModel* model, const GMGameObject* parent) override;
	virtual void endModel() override;
	virtual void draw(IQueriable* painter, GMComponent* component, GMMesh* mesh) override;
	virtual const char* getTechniqueName() = 0;

private:
	inline GMDx11GraphicEngine* getEngine()
	{
		return gm_static_cast<GMDx11GraphicEngine*>(GM.getGraphicEngine());
	}

private:
	ID3DX11EffectTechnique* getTechnique();
	void prepareBuffer(IQueriable* painter);
	void prepareRasterizer(GMComponent* component);
	void passAllAndDraw(GMComponent* component);
	ITexture* GMDx11Renderer::getTexture(GMTextureFrames& frames);
	void drawTextures();
};

class GMDx11Renderer_3D : public GMDx11Renderer
{
	virtual const char* getTechniqueName() override
	{
		return "GMTech_3D";
	}
};

class GMDx11Renderer_2D : public GMDx11Renderer
{
	virtual const char* getTechniqueName() override
	{
		return "GMTech_2D";
	}
};

class GMDx11Renderer_Glyph : public GMDx11Renderer
{
	virtual const char* getTechniqueName() override
	{
		return "GMTech_Glyph";
	}
};

END_NS
#endif