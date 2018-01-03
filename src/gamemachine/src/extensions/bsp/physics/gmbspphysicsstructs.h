﻿#ifndef __BSPPHYSICSSTRUCTS_H__
#define __BSPPHYSICSSTRUCTS_H__
#include <gmcommon.h>
#include <linearmath.h>
#include <gmbsp.h>
#include <gmphysics.h>
BEGIN_NS

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )
enum PlaneType
{
	PLANE_X = 0,
	PLANE_Y,
	PLANE_Z,
	PLANE_NON_AXIAL,
};

GM_ALIGNED_STRUCT(GMBSPLeafList)
{
	glm::vec3 bounds[2];
	AlignedVector<GMint> list;
	GMint lastLeaf;
};

struct BSPTracePlane;
GM_ALIGNED_STRUCT(GMBSP_Physics_BrushSide)
{
	GMBSPBrushSide* side;
	BSPTracePlane* plane;
	GMint surfaceFlags;
};

GM_ALIGNED_STRUCT(GMBSP_Physics_Brush)
{
	GMBSP_Physics_Brush()
		: checkcount(0)
	{
	}

	GMBSPBrush* brush;
	GMint contents;
	glm::vec3 bounds[2];
	GMBSP_Physics_BrushSide *sides;
	GMint checkcount;
};

// Begin patches definitions
GM_ALIGNED_STRUCT(GMBSPPatchPlane)
{
	glm::vec4 plane;
	GMint signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
};

GM_ALIGNED_STRUCT(GMBSPFacet)
{
	GMint surfacePlane;
	GMint numBorders;		// 3 or four + 6 axial bevels + 4 or 3 * 4 edge bevels
	GMint borderPlanes[4 + 6 + 16];
	GMint borderInward[4 + 6 + 16];
	bool borderNoAdjust[4 + 6 + 16];
};

GM_ALIGNED_STRUCT(GMBSPPatchCollide)
{
	glm::vec3 bounds[2];
	AlignedVector<GMBSPPatchPlane> planes;
	AlignedVector<GMBSPFacet> facets;
};
// End patches definitions

GM_ALIGNED_STRUCT(GMBSP_Physics_Patch)
{
	GMBSPSurface* surface = nullptr;
	GMBSPShader* shader = nullptr;
	GMBSPPatchCollide *pc = nullptr;
	GMint checkcount = 0;

	GMBSP_Physics_Patch()
		: checkcount(0)
		, pc(nullptr)
	{
	}

	~GMBSP_Physics_Patch()
	{
		if (pc)
			delete pc;
	}
};

GM_ALIGNED_STRUCT(GMBSPShapeProperties)
{
	glm::vec3 bounding[2]; //!< 最小边界和最大边界，用于碰撞检测
	GMfloat stepHeight;
};

GM_PRIVATE_OBJECT(GMBSPPhysicsObject)
{
	GMBSPShapeProperties shapeProps;
};

class GMBSPPhysicsObject : public GMPhysicsObject
{
	DECLARE_PRIVATE_AND_BASE(GMBSPPhysicsObject, GMPhysicsObject)
	friend class GMBSPPhysicsWorld;

protected:
	GMBSPPhysicsObject() = default;

public:
	//! 形状参数
	/*!
	  用户可以直接修改拿到的形状参数。
	  \return 当前物理对象的形状参数。
	*/
	inline GMBSPShapeProperties& shapeProperties() { D(d); return d->shapeProps; }
};

END_NS
#endif