﻿#include "stdafx.h"
#include "texture.h"
#include <linearmath.h>

void Demo_Texture::init()
{
	D(d);
	Base::init();

	// 创建对象
	getDemoWorldReference() = new gm::GMDemoGameWorld();

	// 创建一个纹理
	struct _ShaderCb : public gm::IPrimitiveCreatorShaderCallback
	{
		gm::GMDemoGameWorld* world = nullptr;

		_ShaderCb(gm::GMDemoGameWorld* d) : world(d)
		{
		}

		virtual void onCreateShader(gm::GMShader& shader) override
		{
			shader.getMaterial().kd = GMVec3(1, 1, 1);
			shader.getMaterial().ks = GMVec3(0);

			gm::ITexture* tex = nullptr;
			gm::GMToolUtil::createTexture("gamemachine.png", &tex);
			gm::GMToolUtil::addTextureToShader(shader, tex, gm::GMTextureType::Diffuse);
			world->getAssets().insertAsset(gm::GMAssetType::Texture, tex);
		}
	} cb(asDemoGameWorld(getDemoWorldReference()));

	// 创建一个带纹理的对象
	gm::GMfloat extents[] = { 1.f, .5f, .5f };
	gm::GMfloat pos[] = { 0, 0, 0 };
	gm::GMModel* model;
	gm::GMPrimitiveCreator::createQuad(extents, pos, &model, &cb);
	gm::GMAsset quadAsset = getDemoWorldReference()->getAssets().insertAsset(gm::GMAssetType::Model, model);
	gm::GMGameObject* obj = new gm::GMGameObject(quadAsset);
	asDemoGameWorld(getDemoWorldReference())->addObject("texture", obj);
}

void Demo_Texture::event(gm::GameMachineEvent evt)
{
	D_BASE(db, Base);
	D(d);
	Base::event(evt);
	switch (evt)
	{
	case gm::GameMachineEvent::FrameStart:
		break;
	case gm::GameMachineEvent::FrameEnd:
		break;
	case gm::GameMachineEvent::Simulate:
		getDemoWorldReference()->simulateGameWorld();
		break;
	case gm::GameMachineEvent::Render:
		getDemoWorldReference()->renderScene();
		break;
	case gm::GameMachineEvent::Activate:
	{
		gm::IInput* inputManager = GM.getMainWindow()->getInputMananger();
		gm::IKeyboardState& kbState = inputManager->getKeyboardState();
		if (kbState.keyTriggered('N'))
			switchNormal();

		getDemoWorldReference()->notifyControls();
		break;
	}
	case gm::GameMachineEvent::Deactivate:
		break;
	case gm::GameMachineEvent::Terminate:
		break;
	default:
		break;
	}
}

void Demo_Texture_Index::init()
{
	D(d);
	Base::init();

	// 创建对象
	getDemoWorldReference() = new gm::GMDemoGameWorld();

	gm::GMModel* quad = new gm::GMModel();
	quad->setPrimitiveTopologyMode(gm::GMTopologyMode::Triangles);
	quad->setDrawMode(gm::GMModelDrawMode::Index);
	gm::GMMesh* mesh = new gm::GMMesh(quad);

	gm::GMVertex V1 = { { -1.f, -.5f, 0 }, { 0, 0, -1 }, { 0, 1 } };
	gm::GMVertex V2 = { { -1.f, .5f, 0 }, { 0, 0, -1 }, { 0, 0 } };
	gm::GMVertex V3 = { { 1.f, -.5f, 0 }, { 0, 0, -1 }, { 1, 1 } };
	gm::GMVertex V4 = { { 1.f, .5f, 0 }, { 0, 0, -1 }, { 1, 0 } };
	mesh->vertex(V1);
	mesh->vertex(V2);
	mesh->vertex(V3);
	mesh->vertex(V4);
	mesh->index(0);
	mesh->index(1);
	mesh->index(2);

	gm::GMMesh* mesh2 = new gm::GMMesh(quad);
	mesh2->vertex(V3);
	mesh2->vertex(V2);
	mesh2->vertex(V4);
	mesh2->index(0);
	mesh2->index(1);
	mesh2->index(2);

	gm::GMShader& shader = quad->getShader();
	shader.getMaterial().kd = GMVec3(1, 1, 1);
	shader.getMaterial().ks = GMVec3(0);

	gm::ITexture* tex = nullptr;
	gm::GMToolUtil::createTexture("gamemachine.png", &tex);
	gm::GMToolUtil::addTextureToShader(shader, tex, gm::GMTextureType::Diffuse);
	getDemoWorldReference()->getAssets().insertAsset(gm::GMAssetType::Texture, tex);

	gm::GMAsset quadAsset = getDemoWorldReference()->getAssets().insertAsset(gm::GMAssetType::Model, quad);
	asDemoGameWorld(getDemoWorldReference())->addObject("texture", new gm::GMGameObject(quadAsset));
}