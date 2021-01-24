#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwplugins.h"

#ifdef __WIIU__
#include "rwgx2.h"
#include "rwgx2shader.h"
#include "rwgx2plg.h"
#include "rwgx2impl.h"

#include "shaders/matfx_env.h"

namespace rw {
namespace gx2 {


static Shader *envShader;
static int32 u_texMatrix;
static int32 u_fxparams;
static int32 u_colorClamp;

void
matfxDefaultRender(InstanceDataHeader *header, InstanceData *inst)
{
	Material *m;
	m = inst->material;

	defaultShader->use();

	setMaterial(m->color, m->surfaceProps);

	setTexture(0, m->texture);

	rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

	drawInst(header, inst);
}

static Frame *lastEnvFrame;

static RawMatrix normal2texcoord = {
	{ 0.5f,  0.0f, 0.0f }, 0.0f,
	{ 0.0f, -0.5f, 0.0f }, 0.0f,
	{ 0.0f,  0.0f, 1.0f }, 0.0f,
	{ 0.5f,  0.5f, 0.0f }, 1.0f
};

void
uploadEnvMatrix(Frame *frame)
{
	Matrix invMat;
	if(frame == nil)
		frame = engine->currentCamera->getFrame();

	// cache the matrix across multiple meshes
	static RawMatrix envMtx;
	// can't do it, frame matrix may change
	// if(frame != lastEnvFrame){
	// 	lastEnvFrame = frame;
	{

		RawMatrix invMtx;
		Matrix::invert(&invMat, frame->getLTM());
		convMatrix(&invMtx, &invMat);
		invMtx.pos.set(0.0f, 0.0f, 0.0f);
		RawMatrix::mult(&envMtx, &invMtx, &normal2texcoord);
	}
	setUniform(u_texMatrix, 16, &envMtx);
}

void
matfxEnvRender(InstanceDataHeader *header, InstanceData *inst, MatFX::Env *env)
{
	Material *m;
	m = inst->material;

	if(env->tex == nil || env->coefficient == 0.0f){
		matfxDefaultRender(header, inst);
		return;
	}

	envShader->use();

	setTexture(0, m->texture);
	setTexture(1, env->tex);
	uploadEnvMatrix(env->frame);

	setMaterial(m->color, m->surfaceProps);

	float fxparams[2];
	fxparams[0] = env->coefficient;
	fxparams[1] = env->fbAlpha ? 0.0f : 1.0f;

	setUniform(u_fxparams, 2, fxparams);
	static float zero[4] = { 0.0f };
	static float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	// This clamps the vertex color below. With it we can achieve both PC and PS2 style matfx
	if(MatFX::modulateEnvMap)
		setUniform(u_colorClamp, 4, zero);
	else
		setUniform(u_colorClamp, 4, one);

	rw::SetRenderState(VERTEXALPHA, 1);
	rw::SetRenderState(SRCBLEND, BLENDONE);

	drawInst(header, inst);

	rw::SetRenderState(SRCBLEND, BLENDSRCALPHA);
}

void
matfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	uint32 stride = header->attribDesc[0].stride;
	GX2SetAttribBuffer(0, header->totalNumVertex*stride, stride, header->vertexBuffer);

	lastEnvFrame = nil;

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		MatFX *matfx = MatFX::get(inst->material);

		if(matfx == nil)
			matfxDefaultRender(header, inst);
		else switch(matfx->type){
		case MatFX::ENVMAP:
			matfxEnvRender(header, inst, &matfx->fx[0].env);
			break;
		default:
			matfxDefaultRender(header, inst);
			break;
		}
		inst++;
	}
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = matfxRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

static void*
matfxOpen(void *o, int32, int32)
{
	matFXGlobals.pipelines[PLATFORM_GX2] = makeMatFXPipeline();

	envShader = Shader::create(matfx_env_gsh, GX2_SHADER_MODE_UNIFORM_REGISTER);
	envShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	envShader->initAttribute("in_normal", 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	envShader->initAttribute("in_color", 24, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	envShader->initAttribute("in_tex0", 40, GX2_ATTRIB_FORMAT_FLOAT_32_32);

	envShader->init();
	envShader->use();

	envShader->samplerLocation = GX2GetPixelSamplerVarLocation(envShader->group.pixelShader, "tex0");
	envShader->sampler2Location = GX2GetPixelSamplerVarLocation(envShader->group.pixelShader, "tex1");

	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_GX2])->destroy();
	matFXGlobals.pipelines[PLATFORM_GX2] = nil;

	envShader->destroy();
	envShader = nil;

    return o;
}

void
initMatFX(void)
{
	u_texMatrix = registerUniform("u_texMatrix");
	u_fxparams = registerUniform("u_fxparams");
	u_colorClamp = registerUniform("u_colorClamp");

	Driver::registerPlugin(PLATFORM_GX2, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}

}
}

#endif