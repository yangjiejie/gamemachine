﻿#include "stdafx.h"
#include "foundation/gamemachine.h"
#include "gmparticlemodel.h"
#include "foundation/gmasync.h"
#include <gmengine/gmcomputeshadermanager.h>

namespace
{
	bool normalFuzzyEquals(const GMVec3& normal1, const GMVec3& normal2, GMfloat epsilon = .01f)
	{
		return (
			normal1.getX() - normal2.getX() < epsilon &&
			normal1.getY() - normal2.getY() < epsilon &&
			normal1.getZ() - normal2.getZ() < epsilon);
	}

	static GMString s_code;
}

GMParticleModel::GMParticleModel(GMParticleSystem* system)
{
	D(d);
	d->system = system;
}

GMParticleModel::~GMParticleModel()
{
	disposeGPUHandles();
}

GMGameObject* GMParticleModel::createGameObject(
	const IRenderContext* context
)
{
	D(d);
	GMGameObject* object = new GMGameObject();
	d->particleModel = new GMModel();
	d->particleModel->getShader().setCull(GMS_Cull::None);
	d->particleModel->getShader().setBlend(true);
	d->particleModel->getShader().setNoDepthTest(true);
	d->particleModel->getShader().setBlendFactorSource(GMS_BlendFunc::SourceAlpha);
	d->particleModel->getShader().setBlendFactorDest(GMS_BlendFunc::One);
	d->particleModel->setUsageHint(GMUsageHint::DynamicDraw);
	d->particleModel->setType(GMModelType::Particle);

	d->particleModel->setPrimitiveTopologyMode(GMTopologyMode::Triangles);
	GMPart* part = new GMPart(d->particleModel);

	// 使用triangles拓扑，一次性填充所有的矩形
	GMsize_t total = d->system->getEmitter()->getParticleCount();
	for (GMsize_t i = 0; i < total; ++i)
	{
		// 一个particle由6个定点组成
		part->vertex(GMVertex());
		part->vertex(GMVertex());
		part->vertex(GMVertex());
		part->vertex(GMVertex());
		part->vertex(GMVertex());
		part->vertex(GMVertex());
	}

	context->getEngine()->createModelDataProxy(context, d->particleModel);
	object->setContext(context);
	object->setAsset(gm::GMScene::createSceneFromSingleModel(GMAsset(GMAssetType::Model, d->particleModel)));
	return object;
}

void GMParticleModel::update6Vertices(
	GMVertex* vertex,
	const GMVec3& centerPt,
	const GMVec2& halfExtents,
	const GMVec4& color,
	const GMQuat& quat,
	const GMVec3& lookDirection,
	GMfloat z
)
{
	const static GMVec3 s_normal(0, 0, -1.f);
	constexpr GMfloat texcoord[4][2] =
	{
		{ 0, 1 },
		{ 0, 0 },
		{ 1, 1 },
		{ 1, 0 },
	};

	const GMfloat x = halfExtents.getX(), y = halfExtents.getY();
	GMVec4 raw[4] = {
		GMVec4(centerPt.getX() - x, centerPt.getY() - y, z, 1),
		GMVec4(centerPt.getX() - x, centerPt.getY() + y, z, 1),
		GMVec4(centerPt.getX() + x, centerPt.getY() - y, z, 1),
		GMVec4(centerPt.getX() + x, centerPt.getY() + y, z, 1),
	};

	GMVec4 transformed[4] = {
		raw[0] * quat,
		raw[1] * quat,
		raw[2] * quat,
		raw[3] * quat,
	};

	// 当玩家没有直视粒子时，使用billboard效果
	if (!normalFuzzyEquals(-lookDirection, s_normal))
	{
		GMQuat rot = RotationTo(s_normal, -lookDirection, Zero<GMVec3>());

		GMMat4 transToOrigin = Translate(-centerPt);
		GMMat4 transToCenterPt = Translate(centerPt);
		// 先移回原点，旋转之后再移会原位置
		transformed[0] = transformed[0] * transToOrigin * rot * transToCenterPt;
		transformed[1] = transformed[1] * transToOrigin * rot * transToCenterPt;
		transformed[2] = transformed[2] * transToOrigin * rot * transToCenterPt;
		transformed[3] = transformed[3] * transToOrigin * rot * transToCenterPt;
	}

	// 排列方式：
	// 1   | 1 3
	// 0 2 |   2
	// (0, 1, 2), (2, 1, 3)
	const GMfloat vertices[4][3] = {
		{ transformed[0].getX(), transformed[0].getY(), transformed[0].getZ() },
		{ transformed[1].getX(), transformed[1].getY(), transformed[1].getZ() },
		{ transformed[2].getX(), transformed[2].getY(), transformed[2].getZ() },
		{ transformed[3].getX(), transformed[3].getY(), transformed[3].getZ() },
	};

	vertex[0] = {
		{ vertices[0][0], vertices[0][1], vertices[0][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[0][0], texcoord[0][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
	vertex[1] = {
		{ vertices[1][0], vertices[1][1], vertices[1][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[1][0], texcoord[1][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
	vertex[2] = {
		{ vertices[2][0], vertices[2][1], vertices[2][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[2][0], texcoord[2][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
	vertex[3] = {
		{ vertices[2][0], vertices[2][1], vertices[2][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[2][0], texcoord[2][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
	vertex[4] = {
		{ vertices[1][0], vertices[1][1], vertices[1][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[1][0], texcoord[1][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
	vertex[5] = {
		{ vertices[3][0], vertices[3][1], vertices[3][2] }, //position
		{ 0, 0, -1.f }, //normal
		{ texcoord[3][0], texcoord[3][1] }, //texcoord
		{ 0 },
		{ 0 },
		{ 0 },
		{ color.getX(), color.getY(), color.getZ(), color.getW() }
	};
}

void GMParticleModel::updateData(const IRenderContext* context, void* dataPtr)
{
	D(d);
	if (d->GPUValid)
	{
		IComputeShaderProgram* prog = GMComputeShaderManager::instance().getComputeShaderProgram(context, GMCS_PARTICLE_DATA_TRANSFER, L".", getCode(), L"main");
		if (!prog)
		{
			d->GPUValid = false;
			CPUUpdate(context, dataPtr);
			return;
		}

		GPUUpdate(prog, context, dataPtr);
	}
	else
	{
		CPUUpdate(context, dataPtr);
	}
}

void GMParticleModel::GPUUpdate(IComputeShaderProgram* shaderProgram, const IRenderContext* context, void* dataPtr)
{
	D(d);
	const GMuint32 sz = gm_sizet_to_uint(d->system->getEmitter()->getParticles().size());
	if (sz > d->lastMaxSize)
	{
		d->lastMaxSize = sz;
		d->particleSizeChanged = true;
	}
	else
	{
		d->particleSizeChanged = false;
	}

	GMComputeBufferHandle futureResult = prepareBuffers(shaderProgram, context, dataPtr, GMParticleModel::None);
	shaderProgram->dispatch(sz, 1, 1);

	// 从futureResult获取结果，并拷贝到dataPtr
	bool canReadFromGPU = shaderProgram->canRead(futureResult);
	if (!canReadFromGPU)
	{
		if (d->resultBuffer_CPU)
			shaderProgram->release(d->resultBuffer_CPU);
		shaderProgram->createReadOnlyBufferFrom(futureResult, &d->resultBuffer_CPU);
	}

	GMComputeBufferHandle resultHandle = canReadFromGPU ? futureResult : d->resultBuffer_CPU;
	if (!canReadFromGPU)
		shaderProgram->copyBuffer(resultHandle, futureResult);

	void* resultPtr = shaderProgram->mapBuffer(resultHandle);
	const GMsize_t verticesSize = sizeof(GMVertex) * 6 * sz; // 一个粒子6个顶点
	memcpy_s(dataPtr, verticesSize, resultPtr, verticesSize);
	shaderProgram->unmapBuffer(resultHandle);
}

GMComputeBufferHandle GMParticleModel::prepareBuffers(IComputeShaderProgram* shaderProgram, const IRenderContext* context, void* dataPtr, BufferFlags flags)
{
	struct Constant
	{
		GMMat4 billboardRotation;
		int ignoreZ;
	};

	D(d);
	auto& particles = d->system->getEmitter()->getParticles();
	if (!d->constantBuffer || d->particleSizeChanged)
	{
		disposeGPUHandles();
		shaderProgram->createBuffer(sizeof(Constant), 1, nullptr, GMComputeBufferType::Constant, &d->constantBuffer);
		shaderProgram->createBuffer(sizeof(particles[0]), gm_sizet_to_uint(particles.size()), nullptr, GMComputeBufferType::Structured, &d->particleBuffer);
		shaderProgram->createBufferShaderResourceView(d->particleBuffer, &d->particleView);
		shaderProgram->createBuffer(sizeof(GMVertex), gm_sizet_to_uint(particles.size()) * 6, nullptr, GMComputeBufferType::UnorderedStructured, &d->resultBuffer);
		shaderProgram->createBufferUnorderedAccessView(d->resultBuffer, &d->resultView);
	}

	const static GMVec3 s_normal(0, 0, -1.f);
	GMVec3 lookDir = context->getEngine()->getCamera().getLookAt().lookDirection;
	GMQuat rot = RotationTo(s_normal, -lookDir, Zero<GMVec3>());
	Constant c = { QuatToMatrix(rot), flags & IgnorePosZ };
	shaderProgram->setBuffer(d->constantBuffer, GMComputeBufferType::Constant, &c, sizeof(c));
	shaderProgram->bindConstantBuffer(d->constantBuffer);

	shaderProgram->setBuffer(d->particleBuffer, GMComputeBufferType::Structured, particles.data(), sizeof(particles[0]) * gm_sizet_to_uint(particles.size()));
	shaderProgram->bindShaderResourceView(1, &d->particleView);

	// 创建结果
	shaderProgram->setBuffer(d->resultBuffer, GMComputeBufferType::Structured, nullptr, sizeof(GMVertex) * 6 * gm_sizet_to_uint(particles.size()));
	shaderProgram->bindUnorderedAccessView(1, &d->resultView);
	return d->resultBuffer;
}


void GMParticleModel::disposeGPUHandles()
{
	D(d);
	GMComputeBufferHandle* handles[] = {
		&d->constantBuffer,
		&d->particleBuffer,
		&d->particleView,
		&d->resultBuffer,
		&d->resultView,
		&d->resultBuffer_CPU,
	};

	for (auto handle : handles)
	{
		if (*handle)
		{
			GMComputeShaderManager::instance().releaseHandle(*handle);
			*handle = 0;
		}
	}
}


void GMParticleModel::setDefaultCode(const GMString& code)
{
	s_code = code;
}

void GMParticleModel::render(const IRenderContext* context)
{
	D(d);
	if (!d->particleObject)
	{
		d->particleObject.reset(createGameObject(context));

		if (d->system->getTexture().isEmpty())
		{
			// 获取并设置纹理
			GMImage* image = nullptr;
			auto& buffer = d->system->getTextureBuffer();
			if (buffer.getData())
			{
				GMImageReader::load(buffer.getData(), buffer.getSize(), &image);
				if (image)
				{
					GMTextureAsset texture;
					GM.getFactory()->createTexture(context, image, texture);
					GM_delete(image);
					GM_ASSERT(d->particleObject->getModel());
					d->particleObject->getModel()->getShader().getTextureList().getTextureSampler(GMTextureType::Ambient).addFrame(texture);
					d->system->setTexture(texture);
				}
			}
		}
	}

	if (d->particleObject)
	{
		// 开始更新粒子数据
		auto dataProxy = d->particleModel->getModelDataProxy();
		dataProxy->beginUpdateBuffer();
		void* dataPtr = dataProxy->getBuffer();
		updateData(context, dataPtr);
		dataProxy->endUpdateBuffer();
	}

	GM_ASSERT(d->particleObject);
	d->particleObject->draw();
}

void GMParticleModel_2D::CPUUpdate(const IRenderContext* context, void* dataPtr)
{
	D(d);
	auto& particles = d->system->getEmitter()->getParticles();
	const auto& lookDirection = context->getEngine()->getCamera().getLookAt().lookDirection;

	// 一个粒子有6个顶点，2个三角形，放入并行计算
	enum { VerticesPerParticle = 6 };
	GMAsync::blockedAsync(
		GMAsync::Async,
		GM.getRunningStates().systemInfo.numberOfProcessors,
		particles.begin(),
		particles.end(),
		[&particles, dataPtr, this, &lookDirection](auto begin, auto end) {
		// 计算一下数据偏移
		GMVertex* dataOffset = reinterpret_cast<GMVertex*>(dataPtr) + (begin - particles.begin()) * VerticesPerParticle;
		for (auto iter = begin; iter != end; ++iter)
		{
			GMParticle& particle = *iter;
			GMfloat he = particle.getSize() / 2.f;
			update6Vertices(
				dataOffset,
				particle.getPosition(),
				he,
				particle.getColor(),
				Rotate(particle.getRotation(), GMVec3(0, 0, 1)),
				lookDirection
			);
			dataOffset += VerticesPerParticle;
		}
	}
	);
}

GMString GMParticleModel_2D::getCode()
{
	return s_code;
}

void GMParticleModel_3D::CPUUpdate(const IRenderContext* context, void* dataPtr)
{
	D(d);
	auto& particles = d->system->getEmitter()->getParticles();
	const auto& lookDirection = context->getEngine()->getCamera().getLookAt().lookDirection;

	// 粒子本身若带有旋转，则会在正对用户视觉后再来应用此旋转
	// 一个粒子有6个顶点，2个三角形，放入并行计算
	enum { VerticesPerParticle = 6 };
	GMAsync::blockedAsync(
		GMAsync::Async,
		GM.getRunningStates().systemInfo.numberOfProcessors,
		particles.begin(),
		particles.end(),
		[&particles, dataPtr, this, &lookDirection](auto begin, auto end) {
		// 计算一下数据偏移
		GMVertex* dataOffset = reinterpret_cast<GMVertex*>(dataPtr) + (begin - particles.begin()) * VerticesPerParticle;
		for (auto iter = begin; iter != end; ++iter)
		{
			GMParticle& particle = *iter;
			GMfloat he = particle.getSize() / 2.f;

			update6Vertices(
				dataOffset,
				particle.getPosition(),
				he,
				particle.getColor(),
				Rotate(particle.getRotation(), GMVec3(0, 0, 1)),
				lookDirection,
				particle.getPosition().getZ()
			);
			dataOffset += VerticesPerParticle;
		}
	}
	);
}

GMString GMParticleModel_3D::getCode()
{
	return s_code;
}

GMComputeBufferHandle GMParticleModel_3D::prepareBuffers(IComputeShaderProgram* shaderProgram, const IRenderContext* context, void* dataPtr, BufferFlags)
{
	return GMParticleModel::prepareBuffers(shaderProgram, context, dataPtr, GMParticleModel::None);
}
