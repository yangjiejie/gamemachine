﻿#ifndef __OBJREADER_PRIVATE_H__
#define __OBJREADER_PRIVATE_H__
#include "common.h"
#include <string.h>
#include <vector>
#include "gl/GL.h"
#include "mtlreader.h"
#include "gmdatacore/object.h"
BEGIN_NS

class Image;

class ObjReaderPrivate
{
	friend class ObjReader;
	friend class ObjReaderCallback;

private:
	ObjReaderPrivate();
	~ObjReaderPrivate();
	void setObject(Object* obj) { m_object = obj; }
	void setWorkingDir(const std::string& workingDir) { m_workingDir = workingDir; }
	void parseLine(const char* line);
	void pushData();
	void endParse();

private:
	Object* m_object;
	std::string m_workingDir;
	std::vector<GMfloat> m_vertices;
	std::vector<GMuint> m_indices;
	MtlReader* m_pMtlReader;
	Component* m_currentComponent;
	const MaterialProperties* m_currentMaterial;
};

END_NS
#endif