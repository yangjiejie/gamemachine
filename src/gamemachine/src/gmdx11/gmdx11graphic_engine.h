﻿#ifndef __GMDX11GRAPHIC_ENGINE_H__
#define __GMDX11GRAPHIC_ENGINE_H__
#include <gmcommon.h>
#include <gmcom.h>
#include <gmdxincludes.h>
BEGIN_NS

GM_PRIVATE_OBJECT(GMDx11GraphicEngine)
{
	GMComPtr<ID3D11Device> device;
	GMComPtr<ID3D11DeviceContext> context;
};

class GMDx11GraphicEngine : public GMObject, public IGraphicEngine
{
	DECLARE_PRIVATE(GMDx11GraphicEngine)

public:
	virtual void init() override;
	virtual void newFrame() override;
	virtual void drawObjects(GMGameObject *objects[], GMuint count, GMBufferMode bufferMode) override;
	virtual void update(GMUpdateDataType type) override;
	virtual void addLight(const GMLight& light) override;
	virtual void removeLights() override;
	virtual void clearStencil() override;
	virtual void beginCreateStencil() override;
	virtual void endCreateStencil() override;
	virtual void beginUseStencil(bool inverse) override;
	virtual void endUseStencil() override;
	virtual void beginBlend(GMS_BlendFunc sfactor, GMS_BlendFunc dfactor) override;
	virtual void endBlend() override;
	virtual IShaderProgram* getShaderProgram(GMShaderProgramType type) override;
	virtual bool event(const GameMachineMessage& e) override;

public:
	GMComPtr<ID3D11Device> getDevice()
	{
		D(d);
		return d->device;
	}

	GMComPtr<ID3D11DeviceContext> getDeviceContext()
	{
		D(d);
		return d->context;
	}

private:
	void updateProjection();
	void updateView();
};

END_NS
#endif