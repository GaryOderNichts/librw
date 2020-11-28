#ifdef __WIIU__

#include <whb/gfx.h>

namespace rw {
namespace gx2 {

GX2SamplerVar *GX2GetPixelSamplerVar(const GX2PixelShader *shader, const char *name);
uint32 GX2GetPixelSamplerVarLocation(const GX2PixelShader *shader, const char *name);
uint32 GX2GetPixelUniformVarOffset(const GX2PixelShader *shader, const char *name);
uint32 GX2GetVertexUniformVarOffset(const GX2VertexShader *shader, const char *name);
uint32 GX2GetPixelUniformBlockOffset(const GX2PixelShader *shader, const char *name);
uint32 GX2GetVertexUniformBlockOffset(const GX2VertexShader *shader, const char *name);
void GX2SetShaderMode(GX2ShaderMode mode);

struct Uniform
{
	int32 pixelLocation;
	int32 vertexLocation;
};

#define MAX_UNIFORMS 40
#define MAX_BLOCKS 20

struct UniformRegistry
{
	int32 numUniforms;
	char *uniformNames[MAX_UNIFORMS];

	int32 numBlocks;
	char *blockNames[MAX_BLOCKS];
};

int32 registerUniform(const char *name);
int32 findUniform(const char *name);
void setUniform(int32 location, int32 count, const void* data);

int32 registerBlock(const char *name);
int32 findBlock(const char *name);
void setBlock(int32 location, int32 size, void* data);
void shaders_clean();

struct Shader
{
	WHBGfxShaderGroup group;
	GX2ShaderMode mode;

	uint32 samplerLocation;
	// if a shader has a second sampler this can be used
	uint32 sampler2Location;

	Uniform* uniforms;
	Uniform* blocks;

	static Shader *create(const void* data, GX2ShaderMode m);
	bool initAttribute(const char* name, uint32 offset, GX2AttribFormat format);
	bool init(void);
	void use(void);
	void destroy(void);
};

extern Shader *currentShader;

}
}

#endif
