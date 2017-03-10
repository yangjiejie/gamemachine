﻿#include "stdafx.h"
#include "bspphysicsworld.h"
#include "gmengine/elements/bspgameworld.h"

// keep 1/8 unit away to keep the position valid before network snapping
// and to avoid various numeric issues
#define	SURFACE_CLIP_EPSILON	(0.125)
#define VEC3(v4) vmath::vec3(v4[0], v4[1], v4[2])
#define VEC4(v3, v4) vmath::vec4(v3[0], v3[1], v3[2], v4[3])
#define	SUBDIVIDE_DISTANCE	16	//4	// never more than this units away from curve
#define	WRAP_POINT_EPSILON 0.1
#define	PLANE_TRI_EPSILON	0.1
#define	MAX_FACETS			1024
#define	MAX_PATCH_PLANES	2048
#define MAX_MAP_BOUNDS		65535
#define	MAX_POINTS_ON_WINDING	96
#define	NORMAL_EPSILON	0.0001
#define	DIST_EPSILON	0.02

typedef enum {
	EN_TOP,
	EN_RIGHT,
	EN_BOTTOM,
	EN_LEFT
} EdgeName;

enum
{
	SIDE_FRONT = 0,
	SIDE_ON = 2,
	SIDE_BACK = 1,
	SIDE_CROSS = -2,
};

struct PatchCollideContext
{
	int numPlanes;
	BSPPatchPlane planes[MAX_PATCH_PLANES];

	int numFacets;
	BSPFacet facets[MAX_PATCH_PLANES];
};

//a winding gives the bounding points of a convex polygon
struct BSPWinding
{
	GMint numpoints;
	vmath::vec3* p;
	GMint mysize;

	static BSPWinding* alloc(GMint pointNum)
	{
		BSPWinding* p = GM_new<BSPWinding>();
		p->p = new vmath::vec3[pointNum];
		p->mysize = sizeof(vmath::vec3) * pointNum;
		memset(p->p, 0, p->mysize);
		return p;
	}

	~BSPWinding()
	{
		delete p;
	}
};

//tools
static void clearBounds(vmath::vec3& mins, vmath::vec3& maxs) {
	mins = vmath::vec3(99999);
	maxs = vmath::vec3(-99999);
}

static void addPointToBounds(const vmath::vec3& v, vmath::vec3& mins, vmath::vec3& maxs) {
	if (v[0] < mins[0]) {
		mins[0] = v[0];
	}
	if (v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if (v[1] < mins[1]) {
		mins[1] = v[1];
	}
	if (v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if (v[2] < mins[2]) {
		mins[2] = v[2];
	}
	if (v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}

static void setGridWrapWidth(BSPGrid* grid) {
	GMint i, j;
	GMfloat d;

	for (i = 0; i < grid->height; i++) {
		for (j = 0; j < 3; j++) {
			d = grid->points[0][i][j] - grid->points[grid->width - 1][i][j];
			if (d < -WRAP_POINT_EPSILON || d > WRAP_POINT_EPSILON) {
				break;
			}
		}
		if (j != 3) {
			break;
		}
	}
	if (i == grid->height) {
		grid->wrapWidth = true;
	}
	else {
		grid->wrapWidth = false;
	}
}

static bool needsSubdivision(const vmath::vec3& a, const vmath::vec3& b, const vmath::vec3& c) {
	vmath::vec3 cmid;
	vmath::vec3 lmid;
	vmath::vec3 delta;
	GMfloat dist;

	// calculate the linear midpoint
	lmid = (a + c) * .5f;

	// calculate the exact curve midpoint
	cmid = ((a + b) * .5f + (b + c) * .5f) * .5f;

	// see if the curve is far enough away from the linear mid
	delta = cmid - lmid;
	dist = vmath::length(delta);

	return dist >= SUBDIVIDE_DISTANCE;
}

static void subdivide(vmath::vec3& a, vmath::vec3& b, vmath::vec3& c, vmath::vec3& out1, vmath::vec3& out2, vmath::vec3& out3) {
	out1 = (a + b) * .5f;
	out3 = (b + c) * .5f;
	out2 = (out1 + out3) * .5f;
}

static void subdivideGridColumns(BSPGrid* grid) {
	GMint i, j, k;

	for (i = 0; i < grid->width - 2; ) {
		// grid->points[i][x] is an interpolating control point
		// grid->points[i+1][x] is an aproximating control point
		// grid->points[i+2][x] is an interpolating control point

		//
		// first see if we can collapse the aproximating collumn away
		//
		for (j = 0; j < grid->height; j++) {
			if (needsSubdivision(grid->points[i][j], grid->points[i + 1][j], grid->points[i + 2][j])) {
				break;
			}
		}
		if (j == grid->height) {
			// all of the points were close enough to the linear midpoints
			// that we can collapse the entire column away
			for (j = 0; j < grid->height; j++) {
				// remove the column
				for (k = i + 2; k < grid->width; k++) {
					grid->points[k - 1][j] = grid->points[k][j];
				}
			}

			grid->width--;

			// go to the next curve segment
			i++;
			continue;
		}

		//
		// we need to subdivide the curve
		//
		for (j = 0; j < grid->height; j++) {
			vmath::vec3 prev, mid, next;

			// save the control points now
			prev = grid->points[i][j];
			mid = grid->points[i + 1][j];
			next = grid->points[i + 2][j];

			// make room for two additional columns in the grid
			// columns i+1 will be replaced, column i+2 will become i+4
			// i+1, i+2, and i+3 will be generated
			for (k = grid->width - 1; k > i + 1; k--)
			{
				grid->points[k + 2][j] = grid->points[k][j];
			}

			// generate the subdivided points
			subdivide(prev, mid, next, grid->points[i + 1][j], grid->points[i + 2][j], grid->points[i + 3][j]);
		}

		grid->width += 2;

		// the new aproximating point at i+1 may need to be removed
		// or subdivided farther, so don't advance i
	}
}

#define	POINT_EPSILON	0.1
static bool comparePoints(const vmath::vec3& a, const vmath::vec3& b) {
	GMfloat d;

	d = a[0] - b[0];
	if (d < -POINT_EPSILON || d > POINT_EPSILON) {
		return false;
	}
	d = a[1] - b[1];
	if (d < -POINT_EPSILON || d > POINT_EPSILON) {
		return false;
	}
	d = a[2] - b[2];
	if (d < -POINT_EPSILON || d > POINT_EPSILON) {
		return false;
	}
	return true;
}

static void removeDegenerateColumns(BSPGrid* grid) {
	GMint i, j, k;

	for (i = 0; i < grid->width - 1; i++) {
		for (j = 0; j < grid->height; j++) {
			if (!comparePoints(grid->points[i][j], grid->points[i + 1][j])) {
				break;
			}
		}

		if (j != grid->height) {
			continue;	// not degenerate
		}

		for (j = 0; j < grid->height; j++) {
			// remove the column
			for (k = i + 2; k < grid->width; k++) {
				grid->points[k - 1][j] = grid->points[k][j];
			}
		}
		grid->width--;

		// check against the next column
		i--;
	}
}

static void transposeGrid(BSPGrid* grid)
{
	GMint i, j, l;
	vmath::vec3	temp;
	bool tempWrap;

	if (grid->width > grid->height) {
		for (i = 0; i < grid->height; i++) {
			for (j = i + 1; j < grid->width; j++) {
				if (j < grid->height) {
					// swap the value
					temp = grid->points[i][j];
					grid->points[i][j] = grid->points[j][i];
					grid->points[j][i] = temp;
				}
				else {
					// just copy
					grid->points[i][j] = grid->points[j][i];
				}
			}
		}
	}
	else {
		for (i = 0; i < grid->width; i++) {
			for (j = i + 1; j < grid->height; j++) {
				if (j < grid->width) {
					// swap the value
					temp = grid->points[j][i];
					grid->points[j][i] = grid->points[i][j];
					grid->points[i][j] = temp;
				}
				else {
					// just copy
					grid->points[j][i] = grid->points[i][j];
				}
			}
		}
	}

	l = grid->width;
	grid->width = grid->height;
	grid->height = l;

	tempWrap = grid->wrapWidth;
	grid->wrapWidth = grid->wrapHeight;
	grid->wrapHeight = tempWrap;
}

static bool planeFromPoints(vmath::vec4& plane, const vmath::vec3& a, const vmath::vec3& b, const vmath::vec3& c) {
	vmath::vec3 d1, d2;
	d1 = b - a;
	d2 = c - a;
	vmath::vec3 t = vmath::cross(d2, d1);
	t = vmath::normalize(t);
	if (vmath::length(t) == 0)
		return false;

	plane = VEC4(t, plane);
	plane[3] = vmath::dot(a, plane);
	return true;
}

static int signbitsForNormal(const vmath::vec4& normal) {
	GMint bits, j;

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (normal[j] < 0) {
			bits |= 1 << j;
		}
	}
	return bits;
}

static GMint planeEqual(BSPPatchPlane* p, const vmath::vec4& plane, GMint *flipped)
{
	vmath::vec4 invplane;

	if (
		fabs(p->plane[0] - plane[0]) < NORMAL_EPSILON
		&& fabs(p->plane[1] - plane[1]) < NORMAL_EPSILON
		&& fabs(p->plane[2] - plane[2]) < NORMAL_EPSILON
		&& fabs(p->plane[3] - plane[3]) < DIST_EPSILON)
	{
		*flipped = false;
		return true;
	}

	invplane = -plane;
	invplane[3] = -plane[3];

	if (
		fabs(p->plane[0] - invplane[0]) < NORMAL_EPSILON
		&& fabs(p->plane[1] - invplane[1]) < NORMAL_EPSILON
		&& fabs(p->plane[2] - invplane[2]) < NORMAL_EPSILON
		&& fabs(p->plane[3] - invplane[3]) < DIST_EPSILON)
	{
		*flipped = true;
		return true;
	}

	return false;
}

static int findPlane(PatchCollideContext& context, const vmath::vec3& p1, const vmath::vec3& p2, const vmath::vec3& p3)
{
	vmath::vec4 plane;
	GMint i;
	GMfloat d;

	if (!planeFromPoints(plane, p1, p2, p3))
		return -1;

	// see if the points are close enough to an existing plane
	for (i = 0; i < context.numPlanes; i++) {
		if (vmath::dot(VEC3(plane), VEC3(context.planes[i].plane)) < 0) {
			continue;	// allow backwards planes?
		}

		d = vmath::dot(p1, VEC3(context.planes[i].plane)) - context.planes[i].plane[3];
		if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) {
			continue;
		}

		d = vmath::dot(p2, VEC3(context.planes[i].plane)) - context.planes[i].plane[3];
		if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) {
			continue;
		}

		d = vmath::dot(p3, VEC3(context.planes[i].plane)) - context.planes[i].plane[3];
		if (d < -PLANE_TRI_EPSILON || d > PLANE_TRI_EPSILON) {
			continue;
		}

		// found it
		return i;
	}

	// add a new plane
	if (context.numPlanes == MAX_PATCH_PLANES) {
		gm_error("MAX_PATCH_PLANES");
	}

	context.planes[context.numPlanes].plane = plane;
	context.planes[context.numPlanes].signbits = signbitsForNormal(plane);
	context.numPlanes++;
	return context.numPlanes - 1;
}

static GMint findPlane2(PatchCollideContext& context, const vmath::vec4& plane, GMint *flipped)
{
	GMint i;

	// see if the points are close enough to an existing plane
	for (i = 0; i < context.numPlanes; i++) {
		if (planeEqual(&context.planes[i], plane, flipped)) return i;
	}

	// add a new plane
	if (context.numPlanes == MAX_PATCH_PLANES) {
		gm_error("MAX_PATCH_PLANES");
	}

	context.planes[context.numPlanes].plane = plane;
	context.planes[context.numPlanes].signbits = signbitsForNormal(plane);

	context.numPlanes++;

	*flipped = false;

	return context.numPlanes - 1;
}

static GMint gridPlane(int gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], GMint i, GMint j, GMint tri)
{
	GMint p;

	p = gridPlanes[i][j][tri];
	if (p != -1) {
		return p;
	}
	p = gridPlanes[i][j][!tri];
	if (p != -1) {
		return p;
	}

	// should never happen
	gm_warning("WARNING: gridPlane unresolvable\n");
	return -1;
}

static int edgePlaneNum(PatchCollideContext& context, BSPGrid* grid, GMint gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], GMint i, GMint j, GMint k) {
	vmath::vec3 p1, p2;
	vmath::vec3 up;
	GMint p;
	vmath::vec4 t;

	switch (k) {
	case 0:	// top border
		p1 = grid->points[i][j];
		p2 = grid->points[i + 1][j];
		p = gridPlane(gridPlanes, i, j, 0);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);
		return findPlane(context, p1, p2, up);

	case 2:	// bottom border
		p1 = grid->points[i][j + 1];
		p2 = grid->points[i + 1][j + 1];
		p = gridPlane(gridPlanes, i, j, 1);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);

		return findPlane(context, p2, p1, up);

	case 3: // left border
		p1 = grid->points[i][j];
		p2 = grid->points[i][j + 1];
		p = gridPlane(gridPlanes, i, j, 1);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);
		return findPlane(context, p2, p1, up);

	case 1:	// right border
		p1 = grid->points[i + 1][j];
		p2 = grid->points[i + 1][j + 1];
		p = gridPlane(gridPlanes, i, j, 0);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);
		return findPlane(context, p1, p2, up);

	case 4:	// diagonal out of triangle 0
		p1 = grid->points[i + 1][j + 1];
		p2 = grid->points[i][j];
		p = gridPlane(gridPlanes, i, j, 0);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);
		return findPlane(context, p1, p2, up);

	case 5:	// diagonal out of triangle 1
		p1 = grid->points[i][j];
		p2 = grid->points[i + 1][j + 1];
		p = gridPlane(gridPlanes, i, j, 1);
		t = context.planes[p].plane * 4;
		up = p1 + VEC3(t);
		return findPlane(context, p1, p2, up);

	}

	gm_error("edgePlaneNum: bad k");
	return -1;
}

static int pointOnPlaneSide(PatchCollideContext& context, const vmath::vec3& p, GMint planeNum) {
	float	d;

	if (planeNum == -1) {
		return SIDE_ON;
	}

	const vmath::vec4& plane = context.planes[planeNum].plane;

	d = vmath::dot(p, plane) - plane[3];

	if (d > PLANE_TRI_EPSILON) {
		return SIDE_FRONT;
	}

	if (d < -PLANE_TRI_EPSILON) {
		return SIDE_BACK;
	}

	return SIDE_ON;
}


static void setBorderInward(PatchCollideContext& context, BSPFacet* facet, BSPGrid* grid, int gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2], GMint i, GMint j, GMint which)
{
	static bool debugBlock = false;
	GMint k, l;
	vmath::vec3 points[4];
	GMint numPoints;

	switch (which) {
	case -1:
		points[0] = grid->points[i][j];
		points[1] = grid->points[i + 1][j];
		points[2] = grid->points[i + 1][j + 1];
		points[3] = grid->points[i][j + 1];
		numPoints = 4;
		break;
	case 0:
		points[0] = grid->points[i][j];
		points[1] = grid->points[i + 1][j];
		points[2] = grid->points[i + 1][j + 1];
		numPoints = 3;
		break;
	case 1:
		points[0] = grid->points[i + 1][j + 1];
		points[1] = grid->points[i][j + 1];
		points[2] = grid->points[i][j];
		numPoints = 3;
		break;
	default:
		gm_error("setBorderInward: bad parameter");
		numPoints = 0;
		break;
	}

	for (k = 0; k < facet->numBorders; k++) {
		int		front, back;

		front = 0;
		back = 0;

		for (l = 0; l < numPoints; l++) {
			int		side;

			side = pointOnPlaneSide(context, points[l], facet->borderPlanes[k]);
			if (side == SIDE_FRONT) {
				front++;
			} if (side == SIDE_BACK) {
				back++;
			}
		}

		if (front && !back) {
			facet->borderInward[k] = true;
		}
		else if (back && !front) {
			facet->borderInward[k] = false;
		}
		else if (!front && !back) {
			// flat side border
			facet->borderPlanes[k] = -1;
		}
		else {
			// bisecting side border
			gm_error("WARNING: setBorderInward: mixed plane sides\n");
			facet->borderInward[k] = false;
			if (!debugBlock) {
				debugBlock = true;
				/*
				VectorCopy(grid->points[i][j], debugBlockPoints[0]);
				VectorCopy(grid->points[i + 1][j], debugBlockPoints[1]);
				VectorCopy(grid->points[i + 1][j + 1], debugBlockPoints[2]);
				VectorCopy(grid->points[i][j + 1], debugBlockPoints[3]);
				*/
				ASSERT(false);
			}
		}
	}
}

static void baseWindingForPlane(const vmath::vec3& normal, GMfloat dist, OUT BSPWinding** out)
{
	int		i, x;
	GMfloat max, v;
	vmath::vec3 org, vright, vup;
	BSPWinding* w;

	// find the major axis

	max = -MAX_MAP_BOUNDS;
	x = -1;
	for (i = 0; i < 3; i++)
	{
		v = fabs(normal[i]);
		if (v > max)
		{
			x = i;
			max = v;
		}
	}
	if (x == -1)
		gm_error("baseWindingForPlane: no axis found");

	vup = vmath::vec3(0, 0, 0);
	switch (x)
	{
	case 0:
	case 1:
		vup[2] = 1;
		break;
	case 2:
		vup[0] = 1;
		break;
	}

	v = vmath::dot(vup, normal);
	vup = vup - normal * v;
	vup = vmath::normalize(vup);
	org = normal * dist;
	vright = vmath::cross(vup, normal);
	vup = vup * MAX_MAP_BOUNDS;
	vright = vright * MAX_MAP_BOUNDS;

	// project a really big	axis aligned box onto the plane
	w = BSPWinding::alloc(4);

	w->p[0] = org - vright;
	w->p[0] = w->p[0] + vup;

	w->p[1] = org + vright;
	w->p[1] = w->p[1] + vup;

	w->p[2] = org + vright;
	w->p[2] = w->p[2] - vup;

	w->p[3] = org - vright;
	w->p[3] = w->p[3] - vup;

	w->numpoints = 4;
	
	*out = w;
}

static void chopWindingInPlace(BSPWinding** inout, const vmath::vec3& normal, GMfloat dist, GMfloat epsilon)
{
	BSPWinding* in;
	GMfloat dists[MAX_POINTS_ON_WINDING + 4];
	GMint sides[MAX_POINTS_ON_WINDING + 4];
	GMint counts[3];
	GMfloat dot;		// VC 4.2 optimizer bug if not static
	GMint i, j;
	vmath::vec3 *p1, *p2;
	vmath::vec3 mid;
	BSPWinding *f;
	GMint maxpts;

	in = *inout;
	counts[0] = counts[1] = counts[2] = 0;

	// determine sides for each point
	for (i = 0; i < in->numpoints; i++)
	{
		dot = vmath::dot(in->p[i], normal);
		dot -= dist;
		dists[i] = dot;
		if (dot > epsilon)
			sides[i] = SIDE_FRONT;
		else if (dot < -epsilon)
			sides[i] = SIDE_BACK;
		else
		{
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	if (!counts[0])
	{
		delete in;
		*inout = NULL;
		return;
	}
	if (!counts[1])
		return;		// inout stays the same

	maxpts = in->numpoints + 4;	// cant use counts[0]+2 because
								// of fp grouping errors

	f = BSPWinding::alloc(maxpts);

	for (i = 0; i < in->numpoints; i++)
	{
		p1 = &in->p[i];

		if (sides[i] == SIDE_ON)
		{
			f->p[f->numpoints] = *p1;
			f->numpoints++;
			continue;
		}

		if (sides[i] == SIDE_FRONT)
		{
			f->p[f->numpoints] = *p1;
			f->numpoints++;
		}

		if (sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		// generate a split point
		p2 = &in->p[(i + 1) % in->numpoints];

		dot = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++)
		{	// avoid round off error when possible
			if (normal[j] == 1)
				mid[j] = dist;
			else if (normal[j] == -1)
				mid[j] = -dist;
			else
				mid[j] = *p1[j] + dot*(*p2[j] - *p1[j]);
		}

		f->p[f->numpoints] = mid;
		f->numpoints++;
	}

	if (f->numpoints > maxpts)
		gm_error("clipWinding: points exceeded estimate");
	if (f->numpoints > MAX_POINTS_ON_WINDING)
		gm_error("clipWinding: MAX_POINTS_ON_WINDING");

	delete in;
	*inout = f;
}

static void windingBounds(BSPWinding* w, vmath::vec3& mins, vmath::vec3& maxs)
{
	GMfloat v;
	GMint i, j;

	mins = vmath::vec3(MAX_MAP_BOUNDS);
	maxs = vmath::vec3(-MAX_MAP_BOUNDS);

	for (i = 0; i < w->numpoints; i++)
	{
		for (j = 0; j < 3; j++)
		{
			v = w->p[i][j];
			if (v < mins[j])
				mins[j] = v;
			if (v > maxs[j])
				maxs[j] = v;
		}
	}
}

static void copyWinding(BSPWinding* w, REF BSPWinding** out)
{
	BSPWinding *c;
	c = BSPWinding::alloc(w->numpoints);
	memcpy(c, w, w->mysize);
}

void snapVector(vmath::vec3& normal) {
	int		i;

	for (i = 0; i < 3; i++)
	{
		if (fabs(normal[i] - 1) < NORMAL_EPSILON)
		{
			normal = vmath::vec3(0);
			normal[i] = 1;
			break;
		}
		if (fabs(normal[i] - -1) < NORMAL_EPSILON)
		{
			normal = vmath::vec3(0);
			normal[i] = -1;
			break;
		}
	}
}

static void addFacetBevels(PatchCollideContext& context, BSPFacet *facet)
{
	GMint i, j, k, l;
	GMint axis, dir, order, flipped;
	vmath::vec4 plane;
	GMfloat d;
	vmath::vec4 newplane;
	BSPWinding *w, *w2;
	vmath::vec3 mins, maxs, vec, vec2;

	plane = context.planes[facet->surfacePlane].plane;

	baseWindingForPlane(VEC3(plane), plane[3], &w);
	for (j = 0; j < facet->numBorders && w; j++) {
		if (facet->borderPlanes[j] == facet->surfacePlane) continue;
		plane = context.planes[facet->borderPlanes[j]].plane;

		if (!facet->borderInward[j]) {
			vmath::vec3 t = vmath::vec3(0) - plane;
			plane = VEC4(t, plane);
			plane[3] = -plane[3];
		}

		chopWindingInPlace(&w, VEC3(plane), plane[3], 0.1f);
	}
	if (!w) {
		return;
	}

	windingBounds(w, mins, maxs);

	// add the axial planes
	order = 0;
	for (axis = 0; axis < 3; axis++)
	{
		for (dir = -1; dir <= 1; dir += 2, order++)
		{
			plane = vmath::vec4(0);
			plane[axis] = dir;
			if (dir == 1) {
				plane[3] = maxs[axis];
			}
			else {
				plane[3] = -mins[axis];
			}
			//if it's the surface plane
			if (planeEqual(&context.planes[facet->surfacePlane], plane, &flipped)) {
				continue;
			}
			// see if the plane is allready present
			for (i = 0; i < facet->numBorders; i++) {
				if (planeEqual(&context.planes[facet->borderPlanes[i]], plane, &flipped))
					break;
			}

			if (i == facet->numBorders) {
				if (facet->numBorders > 4 + 6 + 16) gm_error("ERROR: too many bevels\n");
				facet->borderPlanes[facet->numBorders] = findPlane2(context, plane, &flipped);
				facet->borderNoAdjust[facet->numBorders] = 0;
				facet->borderInward[facet->numBorders] = flipped;
				facet->numBorders++;
			}
		}
	}
	//
	// add the edge bevels
	//
	// test the non-axial plane edges
	for (j = 0; j < w->numpoints; j++)
	{
		k = (j + 1) % w->numpoints;
		vec = w->p[j] - w->p[k];
		//if it's a degenerate edge
		vec = vmath::normalize(vec);
		if (vmath::length(vec) < 0.5)
			continue;
		snapVector(vec);
		for (k = 0; k < 3; k++)
			if (vec[k] == -1 || vec[k] == 1)
				break;	// axial
		if (k < 3)
			continue;	// only test non-axial edges

						// try the six possible slanted axials from this edge
		for (axis = 0; axis < 3; axis++)
		{
			for (dir = -1; dir <= 1; dir += 2)
			{
				// construct a plane
				vec2 = vmath::vec3(0);
				vec2[axis] = dir;
				vmath::vec3 t = vmath::cross(vec, vec2);
				t = vmath::normalize(t);
				if (vmath::length(t) < 0.5)
					continue;
				plane = VEC4(t, plane);
				plane[3] = vmath::dot(w->p[j], VEC3(plane));

				// if all the points of the facet winding are
				// behind this plane, it is a proper edge bevel
				for (l = 0; l < w->numpoints; l++)
				{
					d = vmath::dot(w->p[l], VEC3(plane)) - plane[3];
					if (d > 0.1)
						break;	// point in front
				}
				if (l < w->numpoints)
					continue;

				//if it's the surface plane
				if (planeEqual(&context.planes[facet->surfacePlane], plane, &flipped)) {
					continue;
				}
				// see if the plane is allready present
				for (i = 0; i < facet->numBorders; i++) {
					if (planeEqual(&context.planes[facet->borderPlanes[i]], plane, &flipped)) {
						break;
					}
				}

				if (i == facet->numBorders) {
					if (facet->numBorders > 4 + 6 + 16) gm_error("ERROR: too many bevels\n");
					facet->borderPlanes[facet->numBorders] = findPlane2(context, plane, &flipped);

					for (k = 0; k < facet->numBorders; k++) {
						if (facet->borderPlanes[facet->numBorders] ==
							facet->borderPlanes[k]) gm_warning("WARNING: bevel plane already used\n");
					}

					facet->borderNoAdjust[facet->numBorders] = 0;
					facet->borderInward[facet->numBorders] = flipped;
					//
					copyWinding(w, &w2);
					newplane = context.planes[facet->borderPlanes[facet->numBorders]].plane;
					if (!facet->borderInward[facet->numBorders])
					{
						newplane = -newplane;
						newplane[3] = -newplane[3];
					} //end if
					chopWindingInPlace(&w2, VEC3(newplane), newplane[3], 0.1f);
					if (!w2) {
						gm_warning("WARNING: addFacetBevels... invalid bevel\n");
						continue;
					}
					else {
						delete w2;
					}
					//
					facet->numBorders++;
					//already got a bevel
					//					break;
				}
			}
		}
	}
	delete w;

	//add opposite plane
	facet->borderPlanes[facet->numBorders] = facet->surfacePlane;
	facet->borderNoAdjust[facet->numBorders] = 0;
	facet->borderInward[facet->numBorders] = true;
	facet->numBorders++;
}

static bool validateFacet(PatchCollideContext& context, BSPFacet* facet) {
	vmath::vec4 plane;
	GMint j;
	BSPWinding* w;
	vmath::vec3 bounds[2];
	vmath::vec3 origin(0, 0, 0);

	if (facet->surfacePlane == -1) {
		return false;
	}

	plane = context.planes[facet->surfacePlane].plane;
	baseWindingForPlane(VEC3(plane), plane[3], &w);
	for (j = 0; j < facet->numBorders && w; j++) {
		if (facet->borderPlanes[j] == -1) {
			return false;
		}
		plane = context.planes[facet->borderPlanes[j]].plane;
		if (!facet->borderInward[j]) {
			vmath::vec3 t = origin - VEC3(plane);
			plane = VEC4(t, plane);
			plane[3] = -plane[3];
		}
		chopWindingInPlace(&w, VEC3(plane), plane[3], 0.1f);
	}

	if (!w) {
		return false;		// winding was completely chopped away
	}

	// see if the facet is unreasonably large
	windingBounds(w, bounds[0], bounds[1]);
	delete w;

	for (j = 0; j < 3; j++) {
		if (bounds[1][j] - bounds[0][j] > MAX_MAP_BOUNDS) {
			return false;		// we must be missing a plane
		}
		if (bounds[0][j] >= MAX_MAP_BOUNDS) {
			return false;
		}
		if (bounds[1][j] <= -MAX_MAP_BOUNDS) {
			return false;
		}
	}
	return true;		// winding is fine
}

static void patchCollideFromGrid(BSPGrid *grid, BSPPatchCollide *pf)
{
	GMint i, j;
	vmath::vec3 p1, p2, p3;
	GMint gridPlanes[MAX_GRID_SIZE][MAX_GRID_SIZE][2];
	BSPFacet *facet;
	GMint borders[4];
	bool noAdjust[4];
	PatchCollideContext context;

	context.numPlanes = 0;
	context.numFacets = 0;

	// find the planes for each triangle of the grid
	for (i = 0; i < grid->width - 1; i++) {
		for (j = 0; j < grid->height - 1; j++) {
			p1 = grid->points[i][j];
			p2 = grid->points[i + 1][j];
			p3 = grid->points[i + 1][j + 1];
			gridPlanes[i][j][0] = findPlane(context, p1, p2, p3);

			p1 = grid->points[i + 1][j + 1];
			p2 = grid->points[i][j + 1];
			p3 = grid->points[i][j];
			gridPlanes[i][j][1] = findPlane(context, p1, p2, p3);
		}
	}

	// create the borders for each facet
	for (i = 0; i < grid->width - 1; i++) {
		for (j = 0; j < grid->height - 1; j++) {

			borders[EN_TOP] = -1;
			if (j > 0) {
				borders[EN_TOP] = gridPlanes[i][j - 1][1];
			}
			else if (grid->wrapHeight) {
				borders[EN_TOP] = gridPlanes[i][grid->height - 2][1];
			}
			noAdjust[EN_TOP] = (borders[EN_TOP] == gridPlanes[i][j][0]);
			if (borders[EN_TOP] == -1 || noAdjust[EN_TOP]) {
				borders[EN_TOP] = edgePlaneNum(context, grid, gridPlanes, i, j, 0);
			}

			borders[EN_BOTTOM] = -1;
			if (j < grid->height - 2) {
				borders[EN_BOTTOM] = gridPlanes[i][j + 1][0];
			}
			else if (grid->wrapHeight) {
				borders[EN_BOTTOM] = gridPlanes[i][0][0];
			}
			noAdjust[EN_BOTTOM] = (borders[EN_BOTTOM] == gridPlanes[i][j][1]);
			if (borders[EN_BOTTOM] == -1 || noAdjust[EN_BOTTOM]) {
				borders[EN_BOTTOM] = edgePlaneNum(context, grid, gridPlanes, i, j, 2);
			}

			borders[EN_LEFT] = -1;
			if (i > 0) {
				borders[EN_LEFT] = gridPlanes[i - 1][j][0];
			}
			else if (grid->wrapWidth) {
				borders[EN_LEFT] = gridPlanes[grid->width - 2][j][0];
			}
			noAdjust[EN_LEFT] = (borders[EN_LEFT] == gridPlanes[i][j][1]);
			if (borders[EN_LEFT] == -1 || noAdjust[EN_LEFT]) {
				borders[EN_LEFT] = edgePlaneNum(context, grid, gridPlanes, i, j, 3);
			}

			borders[EN_RIGHT] = -1;
			if (i < grid->width - 2) {
				borders[EN_RIGHT] = gridPlanes[i + 1][j][1];
			}
			else if (grid->wrapWidth) {
				borders[EN_RIGHT] = gridPlanes[0][j][1];
			}
			noAdjust[EN_RIGHT] = (borders[EN_RIGHT] == gridPlanes[i][j][0]);
			if (borders[EN_RIGHT] == -1 || noAdjust[EN_RIGHT]) {
				borders[EN_RIGHT] = edgePlaneNum(context, grid, gridPlanes, i, j, 1);
			}

			if (context.numFacets == MAX_FACETS) {
				gm_error("MAX_FACETS");
			}
			facet = &context.facets[context.numFacets];
			memset(facet, 0, sizeof(*facet));

			if (gridPlanes[i][j][0] == gridPlanes[i][j][1]) {
				if (gridPlanes[i][j][0] == -1) {
					continue;		// degenrate
				}
				facet->surfacePlane = gridPlanes[i][j][0];
				facet->numBorders = 4;
				facet->borderPlanes[0] = borders[EN_TOP];
				facet->borderNoAdjust[0] = noAdjust[EN_TOP];
				facet->borderPlanes[1] = borders[EN_RIGHT];
				facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
				facet->borderPlanes[2] = borders[EN_BOTTOM];
				facet->borderNoAdjust[2] = noAdjust[EN_BOTTOM];
				facet->borderPlanes[3] = borders[EN_LEFT];
				facet->borderNoAdjust[3] = noAdjust[EN_LEFT];
				setBorderInward(context, facet, grid, gridPlanes, i, j, -1);
				if (validateFacet(context, facet)) {
					addFacetBevels(context, facet);
					context.numFacets++;
				}
			}
			else {
				// two seperate triangles
				facet->surfacePlane = gridPlanes[i][j][0];
				facet->numBorders = 3;
				facet->borderPlanes[0] = borders[EN_TOP];
				facet->borderNoAdjust[0] = noAdjust[EN_TOP];
				facet->borderPlanes[1] = borders[EN_RIGHT];
				facet->borderNoAdjust[1] = noAdjust[EN_RIGHT];
				facet->borderPlanes[2] = gridPlanes[i][j][1];
				if (facet->borderPlanes[2] == -1) {
					facet->borderPlanes[2] = borders[EN_BOTTOM];
					if (facet->borderPlanes[2] == -1) {
						facet->borderPlanes[2] = edgePlaneNum(context, grid, gridPlanes, i, j, 4);
					}
				}
				setBorderInward(context, facet, grid, gridPlanes, i, j, 0);
				if (validateFacet(context, facet)) {
					addFacetBevels(context, facet);
					context.numFacets++;
				}

				if (context.numFacets == MAX_FACETS) {
					gm_error("MAX_FACETS");
				}
				facet = &context.facets[context.numFacets];
				memset(facet, 0, sizeof(*facet));

				facet->surfacePlane = gridPlanes[i][j][1];
				facet->numBorders = 3;
				facet->borderPlanes[0] = borders[EN_BOTTOM];
				facet->borderNoAdjust[0] = noAdjust[EN_BOTTOM];
				facet->borderPlanes[1] = borders[EN_LEFT];
				facet->borderNoAdjust[1] = noAdjust[EN_LEFT];
				facet->borderPlanes[2] = gridPlanes[i][j][0];
				if (facet->borderPlanes[2] == -1) {
					facet->borderPlanes[2] = borders[EN_TOP];
					if (facet->borderPlanes[2] == -1) {
						facet->borderPlanes[2] = edgePlaneNum(context, grid, gridPlanes, i, j, 5);
					}
				}
				setBorderInward(context, facet, grid, gridPlanes, i, j, 1);
				if (validateFacet(context, facet)) {
					addFacetBevels(context, facet);
					context.numFacets++;
				}
			}
		}
	}

	// copy the results out
	pf->numPlanes = context.numPlanes;
	pf->numFacets = context.numFacets;

	pf->facets = GM_new<BSPFacet>();
	memcpy(pf->facets, context.facets, context.numFacets * sizeof(*pf->facets));
	pf->planes = GM_new<BSPPatchPlane>();
	memcpy(pf->planes, context.planes, context.numPlanes * sizeof(*pf->planes));
}

//class
BSPPhysicsWorld::BSPPhysicsWorld(GameWorld* world)
	: PhysicsWorld(world)
{
	D(d);
	d.world = static_cast<BSPGameWorld*>(world);
	memset(&d.camera, 0, sizeof(d.camera));
}

void BSPPhysicsWorld::simulate()
{
	D(d);
	BSPData& bsp = d.world->bspData();

	d.camera.motions.translation += d.camera.motions.velocity;


	vmath::vec3 n(d.camera.motions.translation);
	n[2] -= 1.f;

	BSPTrace t;
	trace(d.camera.motions.translation,
		n,
		vmath::vec3(0, 0, 0),
		vmath::vec3(0),
		vmath::vec3(0),
		t
	);
}

CollisionObject* BSPPhysicsWorld::find(GameObject* obj)
{
	D(d);
	// 优先查找视角位置
	if (d.camera.object == obj)
		return &d.camera;

	return nullptr;
}

void BSPPhysicsWorld::initBSPPhysicsWorld()
{
	generatePhysicsPlaneData();
	generatePhysicsBrushSideData();
	generatePhysicsBrushData();
	generatePhysicsPatches();
}

void BSPPhysicsWorld::setCamera(GameObject* obj)
{
	D(d);
	CollisionObject c;
	// Setup physical properties
	c.object = obj;
	d.camera = c;
}

void BSPPhysicsWorld::generatePhysicsPlaneData()
{
	D(d);
	BSPData& bsp = d.world->bspData();
	d.planes.resize(bsp.numplanes);
	for (GMint i = 0; i < bsp.numplanes; i++)
	{
		d.planes[i].plane = &bsp.planes[i];
		d.planes[i].planeType = PlaneTypeForNormal(bsp.planes[i].normal);
		d.planes[i].signbits = 0;
		for (GMint j = 0; j < 3; j++)
		{
			if (bsp.planes[i].normal[j] < 0)
				d.planes[i].signbits |= 1 << j;
		}
	}
}

void BSPPhysicsWorld::generatePhysicsBrushSideData()
{
	D(d);
	BSPData& bsp = d.world->bspData();
	d.brushsides.resize(bsp.numbrushsides);
	for (GMint i = 0; i < bsp.numbrushsides; i++)
	{
		BSP_Physics_BrushSide* bs = &d.brushsides[i];
		bs->side = &bsp.brushsides[i];
		bs->plane = &d.planes[bs->side->planeNum];
		bs->surfaceFlags = bsp.shaders[bs->side->shaderNum].surfaceFlags;
	}
}

void BSPPhysicsWorld::generatePhysicsBrushData()
{
	D(d);
	BSPData& bsp = d.world->bspData();
	d.brushes.resize(bsp.numbrushes);
	for (GMint i = 0; i < bsp.numbrushes; i++)
	{
		BSP_Physics_Brush* b = &d.brushes[i];
		b->checkcount = 0;
		b->brush = &bsp.brushes[i];
		b->sides = &d.brushsides[b->brush->firstSide];
		b->contents = bsp.shaders[b->brush->shaderNum].contentFlags;
		b->bounds[0][0] = --b->sides[0].plane->plane->intercept;
		b->bounds[1][0] = -b->sides[1].plane->plane->intercept;
		b->bounds[0][1] = --b->sides[2].plane->plane->intercept;
		b->bounds[1][1] = -b->sides[3].plane->plane->intercept;
		b->bounds[0][2] = --b->sides[4].plane->plane->intercept;
		b->bounds[1][2] = -b->sides[5].plane->plane->intercept;
	}
}

void BSPPhysicsWorld::generatePhysicsPatches()
{
	D(d);
	BSPData& bsp = d.world->bspData();
	d.patches.resize(bsp.numDrawSurfaces);
	// scan through all the surfaces, but only load patches,
	// not planar faces
	for (GMint i = 0; i < bsp.numDrawSurfaces; i++)
	{
		if (bsp.drawSurfaces[i].surfaceType != MST_PATCH)
			continue;

		GMint width = bsp.drawSurfaces[i].patchWidth, height = bsp.drawSurfaces[i].patchHeight;
		GMint c = width * height;
		std::vector<vmath::vec3> points;
		points.resize(c);
		BSPDrawVertices* v = &bsp.vertices[bsp.drawSurfaces[i].firstVert];
		for (GMint j = 0; j < c; j++)
		{
			points[j] = v->xyz;
		}
		GMint shaderNum = bsp.drawSurfaces[i].shaderNum;

		BSP_Physics_Patch* patch = GM_new<BSP_Physics_Patch>();
		patch->surface = &bsp.drawSurfaces[i];
		patch->contents = bsp.shaders[shaderNum].contentFlags;
		patch->surfaceFlags = bsp.shaders[shaderNum].surfaceFlags;
		generatePatchCollide(width, height, points.data(), &patch->pc);

		d.patches[i] = patch;
	}
}

void BSPPhysicsWorld::trace(const vmath::vec3& start, const vmath::vec3& end, const vmath::vec3& origin, const vmath::vec3& mins, const vmath::vec3& maxs, REF BSPTrace& trace)
{
	D(d);
	BSPData& bsp = d.world->bspData();
	d.checkcount++;

	BSPTraceWork tw = { 0 };
	tw.trace.fraction = 1;
	tw.modelOrigin = origin;

	if (!bsp.numnodes) {
		trace = tw.trace;
		return;	// map not loaded, shouldn't happen
	}

	tw.contents = 1; //TODO brushmask

	vmath::vec3 offset = (mins + maxs) * 0.5;
	tw.size[0] = mins - offset;
	tw.size[1] = maxs - offset;
	tw.start = start + offset;
	tw.end = end + offset;

	tw.maxOffset = tw.size[1][0] + tw.size[1][1] + tw.size[1][2];

	// tw.offsets[signbits] = vector to appropriate corner from origin
	// 以原点为中心，offsets[8]表示立方体的8个顶点
	tw.offsets[0][0] = tw.size[0][0];
	tw.offsets[0][1] = tw.size[0][1];
	tw.offsets[0][2] = tw.size[0][2];

	tw.offsets[1][0] = tw.size[1][0];
	tw.offsets[1][1] = tw.size[0][1];
	tw.offsets[1][2] = tw.size[0][2];

	tw.offsets[2][0] = tw.size[0][0];
	tw.offsets[2][1] = tw.size[1][1];
	tw.offsets[2][2] = tw.size[0][2];

	tw.offsets[3][0] = tw.size[1][0];
	tw.offsets[3][1] = tw.size[1][1];
	tw.offsets[3][2] = tw.size[0][2];

	tw.offsets[4][0] = tw.size[0][0];
	tw.offsets[4][1] = tw.size[0][1];
	tw.offsets[4][2] = tw.size[1][2];

	tw.offsets[5][0] = tw.size[1][0];
	tw.offsets[5][1] = tw.size[0][1];
	tw.offsets[5][2] = tw.size[1][2];

	tw.offsets[6][0] = tw.size[0][0];
	tw.offsets[6][1] = tw.size[1][1];
	tw.offsets[6][2] = tw.size[1][2];

	tw.offsets[7][0] = tw.size[1][0];
	tw.offsets[7][1] = tw.size[1][1];
	tw.offsets[7][2] = tw.size[1][2];

	if (tw.sphere.use) {
		for (GMint i = 0; i < 3; i++) {
			if (tw.start[i] < tw.end[i]) {
				tw.bounds[0][i] = tw.start[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
				tw.bounds[1][i] = tw.end[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
			}
			else {
				tw.bounds[0][i] = tw.end[i] - fabs(tw.sphere.offset[i]) - tw.sphere.radius;
				tw.bounds[1][i] = tw.start[i] + fabs(tw.sphere.offset[i]) + tw.sphere.radius;
			}
		}
	}
	else
	{
		for (GMint i = 0; i < 3; i++) {
			if (tw.start[i] < tw.end[i]) {
				tw.bounds[0][i] = tw.start[i] + tw.size[0][i];
				tw.bounds[1][i] = tw.end[i] + tw.size[1][i];
			}
			else {
				tw.bounds[0][i] = tw.end[i] + tw.size[0][i];
				tw.bounds[1][i] = tw.start[i] + tw.size[1][i];
			}
		}
	}

#if 0
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		if (model) {
			if (model == CAPSULE_MODEL_HANDLE) {
				if (tw.sphere.use) {
					CM_TestCapsuleInCapsule(&tw, model);
				}
				else {
					CM_TestBoundingBoxInCapsule(&tw, model);
				}
			}
			else {
				CM_TestInLeaf(&tw, &cmod->leaf);
			}
		}
		else
			positionTest(tw);
	}
#endif
	if (tw.size[0][0] == 0 && tw.size[0][1] == 0 && tw.size[0][2] == 0)
	{
		tw.isPoint = true;
		tw.extents = vmath::vec3(0);
	}
	else {
		tw.isPoint = false;
		tw.extents = tw.size[1];
	}

	traceThroughTree(tw, 0, 0, 1, tw.start, tw.end);

	// generate endpos from the original, unmodified start/end
	if (tw.trace.fraction == 1)
	{
		tw.trace.endpos = end;
	}
	else
	{
		tw.trace.endpos = start + tw.trace.fraction * (end - start);
	}

	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	ASSERT(tw.trace.allsolid ||
		tw.trace.fraction == 1.0 ||
		vmath::lengthSquare(tw.trace.plane.plane->normal) > 0.9999f);
	trace = tw.trace;
}

void BSPPhysicsWorld::positionTest(BSPTraceWork& tw)
{
	BSPLeafList ll;
	ll.bounds[0] = tw.start + tw.size[0] - vmath::vec3(1);
	ll.bounds[1] = tw.start + tw.size[1] + vmath::vec3(1);
	ll.lastLeaf = 0;

	getTouchedLeafs(ll, 0);

	// TODO testInLeaf
}

void BSPPhysicsWorld::traceThroughTree(BSPTraceWork& tw, GMint num, GMfloat p1f, GMfloat p2f, vmath::vec3 p1, vmath::vec3 p2)
{
	D(d);
	BSPData& bsp = d.world->bspData();
	BSPNode* node;
	BSP_Physics_Plane* plane;

	if (tw.trace.fraction <= p1f)
	{
		return; // already hit something nearer
	}

	// if < 0, we are in a leaf node
	if (num < 0)
	{
		traceThroughLeaf(tw, &bsp.leafs[~num]);
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = &bsp.nodes[num];
	plane = &d.planes[node->planeNum];
	
	// t1, t2表示p1和p2与plane的垂直距离
	// 如果平面是与坐标系垂直，可以直接用p[plane->planeType]来拿距离
	GMfloat t1, t2, offset;

	// 由于把BSP坐标系变为了OpenGL坐标系，导致intercept变成了原来的负数 (bsp.cpp)
	GMfloat dist = -plane->plane->intercept;

	if (plane->planeType < PLANE_NON_AXIAL) {
		t1 = p1[plane->planeType] - dist;
		t2 = p2[plane->planeType] - dist;
		offset = tw.extents[plane->planeType];
	}
	else {
		t1 = vmath::dot(plane->plane->normal, p1) - dist;
		t2 = vmath::dot(plane->plane->normal, p2) - dist;
		if (tw.isPoint) {
			offset = 0;
		}
		else {
			// this is silly
			offset = 2048;
		}
	}

	// see which sides we need to consider
	if (t1 >= offset + 1 && t2 >= offset + 1) {
		traceThroughTree(tw, node->children[0], p1f, p2f, p1, p2); // 在平面前
		return;
	}
	if (t1 < -offset - 1 && t2 < -offset - 1) {
		traceThroughTree(tw, node->children[1], p1f, p2f, p1, p2); // 在平面后
		return;
	}

	// put the crosspoint SURFACE_CLIP_EPSILON pixels on the near side
	GMfloat idist;
	GMint side;
	GMfloat frac, frac2;
	if (t1 < t2) {
		idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 - offset + SURFACE_CLIP_EPSILON)*idist;
	}
	else if (t1 > t2) {
		idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
	}
	else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if (frac < 0) {
		frac = 0;
	}
	if (frac > 1) {
		frac = 1;
	}

	GMfloat midf;
	vmath::vec3 mid;
	midf = p1f + (p2f - p1f)*frac;

	mid = p1 + frac*(p2 - p1);
	mid = p1 + frac*(p2 - p1);
	mid = p1 + frac*(p2 - p1);

	traceThroughTree(tw, node->children[side], p1f, midf, p1, mid);


	// go past the node
	if (frac2 < 0) {
		frac2 = 0;
	}
	if (frac2 > 1) {
		frac2 = 1;
	}

	midf = p1f + (p2f - p1f)*frac2;

	mid[0] = p1[0] + frac2*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac2*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac2*(p2[2] - p1[2]);

	traceThroughTree(tw, node->children[side ^ 1], midf, p2f, mid, p2);
}

void BSPPhysicsWorld::traceThroughLeaf(BSPTraceWork& tw, BSPLeaf* leaf)
{
	D(d);
	BSPData& bsp = d.world->bspData();
	// trace line against all brushes in the leaf
	for (GMint k = 0; k < leaf->numLeafBrushes; k++)
	{
		GMint brushnum = bsp.leafbrushes[leaf->firstLeafBrush + k];

		BSP_Physics_Brush* b = &d.brushes[brushnum];

		if (b->checkcount == d.checkcount) {
			continue;	// already checked this brush in another leaf
		}
		b->checkcount = d.checkcount;

		if (!(b->contents & tw.contents)) {
			continue;
		}

		traceThroughBrush(tw, b);
		if (!tw.trace.fraction) {
			return;
		}
	}

	for (GMint k = 0; k < leaf->numLeafSurfaces; k++) {
		BSP_Physics_Patch* patch = d.patches[bsp.leafsurfaces[leaf->firstLeafSurface + k]];
		if (!patch) {
			continue;
		}
		if (patch->checkcount == d.checkcount) {
			continue;	// already checked this patch in another leaf
		}
		patch->checkcount = d.checkcount;

		if (!(patch->contents & tw.contents)) {
			continue;
		}

		traceThroughPatch(tw, patch);
		if (!tw.trace.fraction) {
			return;
		}
	}
}

void BSPPhysicsWorld::traceThroughPatch(BSPTraceWork& tw, BSP_Physics_Patch* patch)
{
	GMfloat oldFrac;

	oldFrac = tw.trace.fraction;

	traceThroughPatchCollide(tw, patch->pc);

	if (tw.trace.fraction < oldFrac) {
		tw.trace.surfaceFlags = patch->surfaceFlags;
		tw.trace.contents = patch->contents;
	}
}

void BSPPhysicsWorld::traceThroughPatchCollide(BSPTraceWork& tw, BSPPatchCollide* pc)
{
	int i, j, hit, hitnum;
	float offset, enterFrac, leaveFrac, t;
	BSPPatchPlane* planes;
	BSPFacet* facet;
	vmath::vec4 plane, bestplane;
	vmath::vec3 startp, endp;

	if (tw.isPoint) {
		tracePointThroughPatchCollide(tw, pc);
		return;
	}

	facet = pc->facets;
	for (i = 0; i < pc->numFacets; i++, facet++) {
		enterFrac = -1.0;
		leaveFrac = 1.0;
		hitnum = -1;
		//
		planes = &pc->planes[facet->surfacePlane];
		plane = planes->plane;
		if (tw.sphere.use) {
			// adjust the plane distance apropriately for radius
			plane[3] += tw.sphere.radius;

			// find the closest point on the capsule to the plane
			t = vmath::dot(VEC3(plane), tw.sphere.offset);
			if (t > 0.0f) {
				startp = tw.start - tw.sphere.offset;
				endp = tw.end - tw.sphere.offset;
			}
			else {
				startp = tw.start + tw.sphere.offset;
				endp = tw.end + tw.sphere.offset;
			}
		}
		else {
			offset = vmath::dot(tw.offsets[planes->signbits], VEC3(plane));
			plane[3] -= offset;
			startp = tw.start;
			endp = tw.end;
		}

		if (!checkFacetPlane(plane, startp, endp, &enterFrac, &leaveFrac, &hit)) {
			continue;
		}
		if (hit) {
			bestplane = plane;
		}

		for (j = 0; j < facet->numBorders; j++) {
			planes = &pc->planes[facet->borderPlanes[j]];
			if (facet->borderInward[j]) {
				plane = planes->plane;
				plane[3] = -planes->plane[3];
			}
			else {
				plane = planes->plane;
				plane[3] = planes->plane[3];
			}
			if (tw.sphere.use) {
				// adjust the plane distance apropriately for radius
				plane[3] += tw.sphere.radius;

				// find the closest point on the capsule to the plane
				t = vmath::dot(VEC3(plane), tw.sphere.offset);
				if (t > 0.0f) {
					startp = tw.start - tw.sphere.offset;
					endp = tw.end - tw.sphere.offset;
				}
				else {
					startp = tw.start + tw.sphere.offset;
					endp = tw.end + tw.sphere.offset;
				}
			}
			else {
				// NOTE: this works even though the plane might be flipped because the bbox is centered
				offset = vmath::dot(tw.offsets[planes->signbits], VEC3(plane));
				plane[3] += fabs(offset);
				startp = tw.start;
				endp = tw.end;
			}

			if (!checkFacetPlane(plane, startp, endp, &enterFrac, &leaveFrac, &hit)) {
				break;
			}
			if (hit) {
				hitnum = j;
				bestplane = plane;
			}
		}

		if (j < facet->numBorders)
			continue;

		//never clip against the back side
		if (hitnum == facet->numBorders - 1)
			continue;

		if (enterFrac < leaveFrac && enterFrac >= 0) {
			if (enterFrac < tw.trace.fraction) {
				if (enterFrac < 0) {
					enterFrac = 0;
				}

				tw.trace.fraction = enterFrac;
				tw.trace.plane.plane->normal = VEC3(bestplane);
				tw.trace.plane.plane->intercept = bestplane[3];
			}
		}
	}
}

void BSPPhysicsWorld::tracePointThroughPatchCollide(BSPTraceWork& tw, const BSPPatchCollide *pc)
{
	GMfloat intersect;
	GMint i, j, k;
	GMfloat offset;
	GMfloat d1, d2;
#if 0
	static cvar_t *cv;
	if (!cm_playerCurveClip->integer || !tw->isPoint) {
		return;
	}
#endif

	BSPPatchPlane* planes;
	BSPFacet* facet;
	std::vector<GMint> frontFacing;
	frontFacing.resize(pc->numPlanes);
	std::vector<GMfloat> intersection;
	intersection.resize(pc->numPlanes);

	// determine the trace's relationship to all planes
	planes = pc->planes;
	for (i = 0; i < pc->numPlanes; i++, planes++) {
		offset = vmath::dot(tw.offsets[planes->signbits], VEC3(planes->plane));
		d1 = vmath::dot(tw.start, VEC3(planes->plane)) - planes->plane[3] + offset;
		d2 = vmath::dot(tw.end, VEC3(planes->plane)) - planes->plane[3] + offset;
		if (d1 <= 0) {
			frontFacing[i] = 0;
		}
		else {
			frontFacing[i] = 1;
		}
		if (d1 == d2) {
			intersection[i] = 99999;
		}
		else {
			intersection[i] = d1 / (d1 - d2);
			if (intersection[i] <= 0) {
				intersection[i] = 99999;
			}
		}
	}


	// see if any of the surface planes are intersected
	facet = pc->facets;
	for (i = 0; i < pc->numFacets; i++, facet++) {
		if (!frontFacing[facet->surfacePlane]) {
			continue;
		}
		intersect = intersection[facet->surfacePlane];
		if (intersect < 0) {
			continue;		// surface is behind the starting point
		}
		if (intersect > tw.trace.fraction) {
			continue;		// already hit something closer
		}
		for (j = 0; j < facet->numBorders; j++) {
			k = facet->borderPlanes[j];
			if (frontFacing[k] ^ facet->borderInward[j]) {
				if (intersection[k] > intersect) {
					break;
				}
			}
			else {
				if (intersection[k] < intersect) {
					break;
				}
			}
		}
		if (j == facet->numBorders) {
			// we hit this facet
#if 0
			if (!cv) {
				cv = Cvar_Get("r_debugSurfaceUpdate", "1", 0);
			}
			if (cv->integer) {
				debugPatchCollide = pc;
				debugFacet = facet;
			}
#endif //BSPC
			planes = &pc->planes[facet->surfacePlane];

			// calculate intersection with a slight pushoff
			offset = vmath::dot(tw.offsets[planes->signbits], VEC3(planes->plane));
			d1 = vmath::dot(tw.start, VEC3(planes->plane)) - planes->plane[3] + offset;
			d2 = vmath::dot(tw.end, VEC3(planes->plane)) - planes->plane[3] + offset;
			tw.trace.fraction = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);

			if (tw.trace.fraction < 0) {
				tw.trace.fraction = 0;
			}

			tw.trace.plane.plane->normal = VEC3(planes->plane);
			tw.trace.plane.plane->intercept = planes->plane[3];
		}
	}
}

GMint BSPPhysicsWorld::checkFacetPlane(const vmath::vec4& plane, const vmath::vec3& start, const vmath::vec3& end, GMfloat *enterFrac, GMfloat *leaveFrac, GMint *hit)
{
	float d1, d2, f;

	*hit = false;

	d1 = vmath::dot(start, vmath::vec3(plane[0], plane[1], plane[2])) - plane[3];
	d2 = vmath::dot(end, vmath::vec3(plane[0], plane[1], plane[2])) - plane[3];

	// if completely in front of face, no intersection with the entire facet
	if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) {
		return false;
	}

	// if it doesn't cross the plane, the plane isn't relevent
	if (d1 <= 0 && d2 <= 0) {
		return true;
	}

	// crosses face
	if (d1 > d2) {	// enter
		f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
		if (f < 0) {
			f = 0;
		}
		//always favor previous plane hits and thus also the surface plane hit
		if (f > *enterFrac) {
			*enterFrac = f;
			*hit = true;
		}
	}
	else {	// leave
		f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
		if (f > 1) {
			f = 1;
		}
		if (f < *leaveFrac) {
			*leaveFrac = f;
		}
	}
	return true;
}

BSPPatchCollide* BSPPhysicsWorld::generatePatchCollide(GMint width, GMint height, const vmath::vec3* points, OUT BSPPatchCollide** pc)
{
	BSPGrid grid;
	GMint i, j;

	if (width <= 2 || height <= 2 || !points) {
		gm_error("generatePatchFacets: bad parameters: (%i, %i, %p)",
			width, height, points);
	}

	if (!(width & 1) || !(height & 1)) {
		gm_error("generatePatchFacets: even sizes are invalid for quadratic meshes");
	}

	if (width > MAX_GRID_SIZE || height > MAX_GRID_SIZE) {
		gm_error("generatePatchFacets: source is > MAX_GRID_SIZE");
	}

	// build a grid
	grid.width = width;
	grid.height = height;
	grid.wrapWidth = false;
	grid.wrapHeight = false;
	for (i = 0; i < width; i++) {
		for (j = 0; j < height; j++) {
			grid.points[i][j] = points[j*width + i];
		}
	}

	// subdivide the grid
	setGridWrapWidth(&grid);
	subdivideGridColumns(&grid);
	removeDegenerateColumns(&grid);

	transposeGrid(&grid);

	setGridWrapWidth(&grid);
	subdivideGridColumns(&grid);
	removeDegenerateColumns(&grid);

	// we now have a grid of points exactly on the curve
	// the aproximate surface defined by these points will be
	// collided against
	BSPPatchCollide* pf = GM_new<BSPPatchCollide>();
	*pc = pf;
	clearBounds(pf->bounds[0], pf->bounds[1]);
	for (i = 0; i < grid.width; i++) {
		for (j = 0; j < grid.height; j++) {
			addPointToBounds(grid.points[i][j], pf->bounds[0], pf->bounds[1]);
		}
	}

	//c_totalPatchBlocks += (grid.width - 1) * (grid.height - 1);

	// generate a bsp tree for the surface
	patchCollideFromGrid(&grid, pf);

	// expand by one unit for epsilon purposes
	pf->bounds[0][0] -= 1;
	pf->bounds[0][1] -= 1;
	pf->bounds[0][2] -= 1;

	pf->bounds[1][0] += 1;
	pf->bounds[1][1] += 1;
	pf->bounds[1][2] += 1;

	return pf;
}

void BSPPhysicsWorld::traceThroughBrush(BSPTraceWork& tw, BSP_Physics_Brush *brush)
{
	D(d);
	BSPData& bsp = d.world->bspData();

	if (!brush->brush->numSides) {
		return;
	}

	bool getout = false, startout = false;
	BSP_Physics_BrushSide* side = nullptr, *leadside = nullptr;
	BSP_Physics_Plane* plane = nullptr, *clipplane = nullptr;
	GMfloat f = 0;
	GMfloat enterFrac = -1.0;
	GMfloat leaveFrac = 1.0;
	vmath::vec3 startp, endp;

	if (tw.sphere.use)
	{
		for (GMint i = 0; i < brush->brush->numSides; i++) {
			side = brush->sides + i;
			plane = &d.planes[side->side->planeNum];

			// adjust the plane distance apropriately for radius
			GMfloat _intercept = -plane->plane->intercept; // bsp.cpp
			GMfloat dist = _intercept + tw.sphere.radius;

			// find the closest point on the capsule to the plane
			GMfloat t = vmath::dot(plane->plane->normal, tw.sphere.offset);
			if (t > 0)
			{
				startp = tw.start - tw.sphere.offset;
				endp = tw.end - tw.sphere.offset;
			}
			else
			{
				startp = tw.start + tw.sphere.offset;
				endp = tw.end + tw.sphere.offset;
			}

			GMfloat d1 = vmath::dot(startp, plane->plane->normal) - dist;
			GMfloat d2 = vmath::dot(endp, plane->plane->normal) - dist;

			if (d2 > 0) {
				getout = true;	// endpoint is not in solid
			}
			if (d1 > 0) {
				startout = false;
			}

			// if completely in front of face, no intersection with the entire brush
			if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) {
				return;
			}

			// if it doesn't cross the plane, the plane isn't relevent
			if (d1 <= 0 && d2 <= 0) {
				continue;
			}

			// crosses face
			if (d1 > d2) {	// enter
				f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
				if (f < 0) {
					f = 0;
				}
				if (f > enterFrac) {
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else {	// leave
				f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
				if (f > 1) {
					f = 1;
				}
				if (f < leaveFrac) {
					leaveFrac = f;
				}
			}
		}
	}
	else
	{
		//
		// compare the trace against all planes of the brush
		// find the latest time the trace crosses a plane towards the interior
		// and the earliest time the trace crosses a plane towards the exterior
		//
		for (GMint i = 0; i < brush->brush->numSides; i++) {
			side = brush->sides + i;
			plane = &d.planes[side->side->planeNum];

			// adjust the plane distance apropriately for mins/maxs
			GMfloat _intercept = -plane->plane->intercept; // bsp.cpp
			GMfloat dist = _intercept - vmath::dot(tw.offsets[plane->signbits], plane->plane->normal);

			GMfloat d1 = vmath::dot(tw.start, plane->plane->normal) - dist;
			GMfloat d2 = vmath::dot(tw.end, plane->plane->normal) - dist;

			if (d2 > 0) {
				getout = true;	// endpoint is not in solid
			}
			if (d1 > 0) {
				startout = true;
			}

			// if completely in front of face, no intersection with the entire brush
			if (d1 > 0 && (d2 >= SURFACE_CLIP_EPSILON || d2 >= d1)) {
				return;
			}

			// if it doesn't cross the plane, the plane isn't relevent
			if (d1 <= 0 && d2 <= 0) {
				continue;
			}

			// crosses face
			if (d1 > d2) {	// enter
				f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
				if (f < 0) {
					f = 0;
				}
				if (f > enterFrac) {
					enterFrac = f;
					clipplane = plane;
					leadside = side;
				}
			}
			else {	// leave
				f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
				if (f > 1) {
					f = 1;
				}
				if (f < leaveFrac) {
					leaveFrac = f;
				}
			}
		}
	}

	//
	// all planes have been checked, and the trace was not
	// completely outside the brush
	//
	if (!startout) {	// original point was inside brush
		tw.trace.startsolid = true;
		if (!getout) {
			tw.trace.allsolid = true;
			tw.trace.fraction = 0;
			tw.trace.contents = brush->contents;
		}
		return;
	}

	if (enterFrac < leaveFrac) {
		if (enterFrac > -1 && enterFrac < tw.trace.fraction) {
			if (enterFrac < 0) {
				enterFrac = 0;
			}
			tw.trace.fraction = enterFrac;
			tw.trace.plane = *clipplane;
			tw.trace.surfaceFlags = leadside->surfaceFlags;
			tw.trace.contents = brush->contents;
		}
	}
}

void BSPPhysicsWorld::getTouchedLeafs(REF BSPLeafList& leafList, int nodeNum)
{
	D(d);
	BSPData& bsp = d.world->bspData();
	while (true)
	{
		if (nodeNum < 0) {
			storeLeafs(leafList, nodeNum);
			return;
		}

		BSPNode* node = &bsp.nodes[nodeNum];
		BSPPlane* plane = &bsp.planes[node->planeNum];
		BSP_Physics_Plane* p_plane = &d.planes[node->planeNum];
		GMint p = p_plane->classifyBox(leafList.bounds[0], leafList.bounds[1]);
		if (p == 1)
		{
			nodeNum = node->children[0]; //front
		}
		else if (p == 2)
		{
			nodeNum = node->children[1]; //back
		}
		else
		{
			// go down both
			getTouchedLeafs(leafList, node->children[0]);
			nodeNum = node->children[1];
		}
	}
}

void BSPPhysicsWorld::storeLeafs(REF BSPLeafList& lst, GMint nodeNum)
{
	D(d);
	BSPData& bsp = d.world->bspData();
	GMint leafNum = -1 - nodeNum;
	if (bsp.leafs[leafNum].cluster != -1)
		lst.lastLeaf = leafNum;

	lst.list.push_back(leafNum);
}