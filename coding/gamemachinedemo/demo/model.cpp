﻿#include "stdafx.h"
#include "model.h"
#include <linearmath.h>
#include <gmmodelreader.h>
#include <gmimagebuffer.h>

#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

namespace
{
	const gm::GMCameraLookAt s_lookAt = {
		glm::vec3(0, -.3f, -1.f),
		glm::vec3(0, .4f, .5f),
	};
}

Demo_Model::~Demo_Model()
{
	D(d);
	gm::GM_delete(d->demoWorld);
}

void Demo_Model::setLookAt()
{
	D(d);
	gm::GMCamera& camera = GM.getCamera();
	camera.setPerspective(glm::radians(75.f), 1.333f, .1f, 3200);
	camera.lookAt(s_lookAt);
	d->lookAtRotation = glm::identity<glm::quat>();
}

void Demo_Model::init()
{
	D(d);
	Base::init();

	// 创建对象
	d->demoWorld = new gm::GMDemoGameWorld();
	
	gm::GMGamePackage& pk = *GM.getGamePackageManager();

	gm::GMModelLoadSettings loadSettings = {
		pk,
		pk.pathOf(gm::GMPackageIndex::Models, "baymax/baymax.obj"),
		"baymax"
	};

	gm::GMModel* model = new gm::GMModel();
	gm::GMModelReader::load(loadSettings, &model);

	// 交给GameWorld管理资源
	gm::GMAsset asset = d->demoWorld->getAssets().insertAsset(gm::GMAssetType::Model, model);

	d->gameObject = new gm::GMGameObject(asset);
	d->gameObject->setScaling(glm::scale(.005f, .005f, .005f));
	d->gameObject->setRotation(glm::rotate(glm::identity<glm::quat>(), PI, glm::vec3(0, 1, 0)));
	d->skyObject = createCubeMap();
	d->skyObject->setScaling(glm::scale(100, 100, 100));

	d->demoWorld->addObject("sky", d->skyObject);
	d->demoWorld->addObject("baymax", d->gameObject);
}

void Demo_Model::handleMouseEvent()
{
	D(d);
	gm::IMouseState& ms = GM.getMainWindow()->getInputMananger()->getMouseState();
	gm::GMMouseState state = ms.mouseState();
	if (state.wheel.wheeled)
	{
		gm::GMfloat delta = .001f * state.wheel.delta / WHEEL_DELTA;
		gm::GMfloat scaling[4];
		{
			glm::getScalingFromMatrix(d->gameObject->getScaling(), scaling);
			scaling[0] += delta;
			scaling[1] += delta;
			scaling[2] += delta;

			if (scaling[0] > 0 && scaling[1] > 0 && scaling[2] > 0)
				d->gameObject->setScaling(glm::scale(scaling[0], scaling[1], scaling[2]));
		}
	}

	if (state.downButton & GMMouseButton_Left)
	{
		d->mouseDownX = state.posX;
		d->mouseDownY = state.posY;
		d->draggingL = true;
		GM.getMainWindow()->setLockWindow(true);
	}
	else if (state.upButton & GMMouseButton_Left)
	{
		d->draggingL = false;
		GM.getMainWindow()->setLockWindow(false);
	}
	if (state.downButton & GMMouseButton_Right)
	{
		d->mouseDownX = state.posX;
		d->mouseDownY = state.posY;
		d->draggingR = true;
		GM.getMainWindow()->setLockWindow(true);
	}
	else if (state.upButton & GMMouseButton_Right)
	{
		d->draggingR = false;
		GM.getMainWindow()->setLockWindow(false);
	}
}

gm::GMCubeMapGameObject* Demo_Model::createCubeMap()
{
	gm::GMGamePackage* pk = GM.getGamePackageManager();
	gm::GMImage* slices[6] = { nullptr };
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_posx.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[0]);
		GM_ASSERT(slices[0]);
	}
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_negx.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[1]);
		GM_ASSERT(slices[1]);
	}
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_posy.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[2]);
		GM_ASSERT(slices[2]);
	}
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_negy.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[3]);
		GM_ASSERT(slices[3]);
	}
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_posz.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[4]);
		GM_ASSERT(slices[4]);
	}
	{
		gm::GMBuffer buf;
		pk->readFile(gm::GMPackageIndex::Textures, "cubemap/cubemap_negz.jpg", &buf);
		gm::GMImageReader::load(buf.buffer, buf.size, &slices[5]);
		GM_ASSERT(slices[5]);
	}

	gm::GMCubeMapBuffer cubeMap(*slices[0], *slices[1], *slices[2], *slices[3], *slices[4], *slices[5]);
	gm::ITexture* cubeMapTex = nullptr;
	GM.getFactory()->createTexture(&cubeMap, &cubeMapTex);

	for (auto slice : slices)
	{
		delete slice;
	}

	return new gm::GMCubeMapGameObject(cubeMapTex);
}

void Demo_Model::handleDragging()
{
	D(d);
	gm::IMouseState& ms = GM.getMainWindow()->getInputMananger()->getMouseState();
	gm::GMMouseState state = ms.mouseState();

	if (d->draggingL)
	{
		gm::GMfloat rotateX = state.posX - d->mouseDownX;
		gm::GMfloat rotateY = state.posY - d->mouseDownY;

		glm::quat q = glm::rotate(d->gameObject->getRotation(),
			PI * rotateX / GM.getGameMachineRunningStates().windowRect.width,
			glm::vec3(0, 1, 0));
		d->gameObject->setRotation(q);

		d->mouseDownX = state.posX;
		d->mouseDownY = state.posY;
	}
	else if (d->draggingR)
	{
		gm::GMfloat rotateX = state.posX - d->mouseDownX;
		gm::GMfloat rotateY = state.posY - d->mouseDownY;
		glm::vec3 lookAt3 = glm::normalize(s_lookAt.lookAt);
		glm::vec4 lookAt = glm::toHomogeneous(lookAt3);
		glm::quat q = glm::rotate(d->lookAtRotation,
			PI * rotateX / GM.getGameMachineRunningStates().windowRect.width,
			glm::vec3(0, 1, 0));
		d->lookAtRotation = q;
		q = glm::rotate(d->lookAtRotation,
			PI * rotateY / GM.getGameMachineRunningStates().windowRect.width,
			glm::vec3(1, 0, 0));
		d->lookAtRotation = q;
		gm::GMCameraLookAt cameraLookAt = {
			glm::mat4_cast(q) * glm::toHomogeneous(s_lookAt.lookAt),
			s_lookAt.position
		};
		GM.getCamera().lookAt(cameraLookAt);
		d->mouseDownX = state.posX;
		d->mouseDownY = state.posY;
	}
}

void Demo_Model::setDefaultLights()
{
	D(d);
	if (isInited())
	{
		{
			gm::GMLight light(gm::GMLightType::SPECULAR);
			gm::GMfloat lightPos[] = { 0, 1.f, -1.f };
			light.setLightPosition(lightPos);
			gm::GMfloat color[] = { .7f, .7f, .7f };
			light.setLightColor(color);
			GM.getGraphicEngine()->addLight(light);
		}

		{
			gm::GMLight light(gm::GMLightType::AMBIENT);
			gm::GMfloat color[] = { .3f, .3f, .3f };
			light.setLightColor(color);
			GM.getGraphicEngine()->addLight(light);
		}
	}
}

void Demo_Model::event(gm::GameMachineEvent evt)
{
	D(d);
	Base::event(evt);
	switch (evt)
	{
	case gm::GameMachineEvent::FrameStart:
		break;
	case gm::GameMachineEvent::FrameEnd:
		break;
	case gm::GameMachineEvent::Simulate:
		d->demoWorld->simulateGameWorld();
		break;
	case gm::GameMachineEvent::Render:
		d->demoWorld->renderScene();
		break;
	case gm::GameMachineEvent::Activate:
		handleMouseEvent();
		handleDragging();
		d->demoWorld->notifyControls();
		break;
	case gm::GameMachineEvent::Deactivate:
		break;
	case gm::GameMachineEvent::Terminate:
		break;
	default:
		break;
	}
}
