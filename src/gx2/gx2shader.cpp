#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#ifdef __WIIU__
#include "rwgx2.h"
#include "rwgx2shader.h"

namespace rw {
namespace gx2 {

std::vector<GX2RBuffer> blockBufs;

GX2SamplerVar *GX2GetPixelSamplerVar(const GX2PixelShader *shader, const char *name)
{
	for (uint32_t i = 0; i < shader->samplerVarCount; i++)
	{
	   if (strcmp(shader->samplerVars[i].name, name) == 0)
		   return &(shader->samplerVars[i]);
	}

	return NULL;
}

uint32 GX2GetPixelSamplerVarLocation(const GX2PixelShader *shader, const char *name)
{
	GX2SamplerVar *sampler = GX2GetPixelSamplerVar(shader, name);
	return sampler ? sampler->location : -1;
}

uint32 GX2GetPixelUniformVarOffset(const GX2PixelShader *shader, const char *name)
{
	GX2UniformVar *uniform = GX2GetPixelUniformVar(shader, name);
	return uniform ? uniform->offset : -1;
}

uint32 GX2GetVertexUniformVarOffset(const GX2VertexShader *shader, const char *name)
{
	GX2UniformVar *uniform = GX2GetVertexUniformVar(shader, name);
	return uniform ? uniform->offset : -1;
}

uint32 GX2GetPixelUniformBlockOffset(const GX2PixelShader *shader, const char *name)
{
	GX2UniformBlock *block = GX2GetPixelUniformBlock(shader, name);
	return block ? block->offset : -1;
}

uint32 GX2GetVertexUniformBlockOffset(const GX2VertexShader *shader, const char *name)
{
	GX2UniformBlock *block = GX2GetVertexUniformBlock(shader, name);
	return block ? block->offset : -1;
}

void GX2SetShaderMode(GX2ShaderMode mode)
{
	if (mode == GX2_SHADER_MODE_GEOMETRY_SHADER) {
		GX2SetShaderModeEx(mode, 44, 32, 64, 48, 76, 176);
	}
	else {
		GX2SetShaderModeEx(mode, 48, 64, 0, 0, 200, 192);
	}
}

UniformRegistry uniformRegistry;

int32
registerUniform(const char *name)
{
	int i = findUniform(name);
	if(i >= 0) return i;

	if(uniformRegistry.numUniforms + 1 >= MAX_UNIFORMS)
	{
		assert(0 && "no space for uniform");
		return -1;
	}
	uniformRegistry.uniformNames[uniformRegistry.numUniforms] = strdup(name);
	return uniformRegistry.numUniforms++;
}

int32
findUniform(const char *name)
{
	for(int i = 0; i < uniformRegistry.numUniforms; i++)
	{
		if(strcmp(name, uniformRegistry.uniformNames[i]) == 0)
			return i;
	}
	return -1;
}

void setUniform(int32 location, int32 count, const void* data)
{
	if (currentShader->uniforms[location].pixelLocation != -1)
		GX2SetPixelUniformReg(currentShader->uniforms[location].pixelLocation, count, data);
	if (currentShader->uniforms[location].vertexLocation != -1)
		GX2SetVertexUniformReg(currentShader->uniforms[location].vertexLocation, count, data);
}

int32 registerBlock(const char *name)
{
	int i = findBlock(name);
	if(i >= 0) return i;
	
	if(uniformRegistry.numBlocks+1 >= MAX_BLOCKS)
		return -1;
	uniformRegistry.blockNames[uniformRegistry.numBlocks] = strdup(name);
	return uniformRegistry.numBlocks++;
}

int32 findBlock(const char *name)
{
	for(int i = 0; i < uniformRegistry.numBlocks; i++)
	{
		if(strcmp(name, uniformRegistry.blockNames[i]) == 0)
			return i;
	}
	return -1;
}

void setBlock(int32 location, int32 size, void* data)
{
	GX2RBuffer buf;
	buf.elemCount = size / sizeof(float);
	buf.elemSize = sizeof(float);
	buf.flags = (GX2RResourceFlags) (GX2R_RESOURCE_BIND_UNIFORM_BLOCK|GX2R_RESOURCE_USAGE_CPU_WRITE|GX2R_RESOURCE_USAGE_GPU_READ);
	GX2RCreateBuffer(&buf);
	blockBufs.push_back(buf);

	float* lock = (float*) GX2RLockBufferEx(&buf, (GX2RResourceFlags) 0);

	memcpy(lock, data, size);
	for (int i = 0; i < size / sizeof(float); ++i)
		lock[i] = _floatswap32(lock[i]);

	GX2RUnlockBufferEx(&buf, (GX2RResourceFlags) 0);


	if (currentShader->blocks[location].pixelLocation != -1)
		GX2RSetPixelUniformBlock(&buf, currentShader->blocks[location].pixelLocation, 0);
	if (currentShader->blocks[location].vertexLocation != -1)
		GX2RSetVertexUniformBlock(&buf, currentShader->blocks[location].vertexLocation, 0);
}

void shaders_clean()
{
	for (GX2RBuffer& buf : blockBufs) {
		GX2RDestroyBufferEx(&buf, (GX2RResourceFlags) 0);
	}
	blockBufs.clear();
}

Shader *currentShader;

Shader*
Shader::create(const void *data, GX2ShaderMode m)
{
	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);
	sh->mode = m;
	sh->samplerLocation = -1;
	sh->sampler2Location = -1;

	if (!WHBGfxLoadGFDShaderGroup(&sh->group, 0, data))
	{
		rwFree(sh);
		return nil;
	}

	return sh;
}

bool Shader::initAttribute(const char* name, uint32 offset, GX2AttribFormat format)
{
	if (!WHBGfxInitShaderAttribute(&group, name, 0, offset, format))
		return false;

	return true;
}

bool Shader::init(void)
{
	if (!WHBGfxInitFetchShader(&group))
		return false;

	uniforms = rwNewT(Uniform, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(int i = 0; i < uniformRegistry.numUniforms; i++)
	{
		uniforms[i].pixelLocation = GX2GetPixelUniformVarOffset(group.pixelShader, uniformRegistry.uniformNames[i]);
		uniforms[i].vertexLocation = GX2GetVertexUniformVarOffset(group.vertexShader, uniformRegistry.uniformNames[i]);
	}

	blocks = rwNewT(Uniform, uniformRegistry.numBlocks, MEMDUR_EVENT | ID_DRIVER);
	for(int i = 0; i < uniformRegistry.numBlocks; i++)
	{
		blocks[i].pixelLocation = GX2GetPixelUniformBlockOffset(group.pixelShader, uniformRegistry.blockNames[i]);
		blocks[i].vertexLocation = GX2GetVertexUniformBlockOffset(group.vertexShader, uniformRegistry.blockNames[i]);
	}

	return true;
}

void
Shader::use(void)
{
	if(currentShader != this)
	{
		GX2SetShaderMode(mode);
		GX2SetFetchShader(&group.fetchShader);
		GX2SetVertexShader(group.vertexShader);
		GX2SetPixelShader(group.pixelShader);
		currentShader = this;
	}
}

void
Shader::destroy(void)
{
	WHBGfxFreeShaderGroup(&group);
	rwFree(this->uniforms);
	rwFree(this->blocks);
	rwFree(this);
}

}
}

#endif
