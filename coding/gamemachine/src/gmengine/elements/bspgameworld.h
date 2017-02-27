﻿#ifndef __BSPGAMEWORLD_H__
#define __BSPGAMEWORLD_H__
#include "common.h"
#include "gameworld.h"
#include "bspgameworldprivate.h"
#include "gmengine/controllers/resource_container.h"
BEGIN_NS

struct Material;
class BSPGameWorld : public GameWorld
{
	DEFINE_PRIVATE(BSPGameWorld);
public:
	BSPGameWorld();

public:
	void loadBSP(const char* bspPath);

public:
	virtual void renderGameWorld() override;

	//renders:
private:
	void updateCamera();
	void calculateVisibleFaces();
	void drawFaces();
	void drawFace(GMint idx);
	void drawPolygonFace(int polygonFaceNumber);
	void drawMeshFace(int meshFaceNumber);
	void drawPatch(int patchNumber);
	void draw(BSP_Drawing_BiquadraticPatch& biqp, Material& material);
	bool setMaterialTexture(GMuint textureid, REF Material& m);
	void setMaterialLightmap(GMuint lightmapid, REF Material& m);
	int calculateCameraLeaf(const vmath::vec3& cameraPosition);
	int isClusterVisible(int cameraCluster, int testCluster);

	//imports:
private:
	void importBSP();
	void initShaders();
	void initTextures();
	bool findTexture(const char* textureFilename, OUT Image** img);
	void initLightmaps();
	void importEntities();

	// this is ususally used by BSPShaderLoader
public:
	const char* bspWorkingDirectory();
	void addShader(const char* name, const Shader& shader);
};

END_NS
#endif