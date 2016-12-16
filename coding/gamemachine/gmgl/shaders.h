﻿#ifndef __SHADERS_H__
#define __SHADERS_H__
#include "common.h"
#include <vector>
BEGIN_NS

struct GMGLShaderInfo
{
	GLenum type;
	const char* filename;
	GLuint shader;
};

typedef std::vector<GMGLShaderInfo> GMGLShadersInfo;

class GMGLShaders
{
public:
	~GMGLShaders();

	void useProgram();
	GLuint getProgram() { return m_shaderProgram; }

	void appendShader(const GMGLShaderInfo& shader);
	GMGLShadersInfo& getShaders() { return m_shaders; }
	void setProgram(GLuint program) { m_shaderProgram = program; }

private:
	GMGLShadersInfo m_shaders;
	GLuint m_shaderProgram;
};

class GMGLShadersLoader
{
public:
	static void loadShaders(GMGLShaders& shaders);
};

END_NS
#endif