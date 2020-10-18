#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"

#ifdef __WIIU__

#include "rwgx2.h"
#include "rwgx2shader.h"
#include "rwgx2impl.h"
#include "gfx_heap.h"

#include "shaders/default.h"

#include <proc_ui/procui.h>
#include <whb/log.h>

namespace rw {
namespace gx2 {

struct GX2Globals
{
	void* commandBufferPool = NULL;
	GX2DrcRenderMode drcRenderMode;
	void* drcScanBuffer = NULL;
	uint32_t drcScanBufferSize = 0;
	GX2SurfaceFormat drcSurfaceFormat;
	GX2TVRenderMode tvRenderMode;
	void* tvScanBuffer = NULL;
	uint32_t tvScanBufferSize = 0;
	GX2SurfaceFormat tvSurfaceFormat;
	GX2ContextState* contextState = NULL;
} globals;

int32   alphaFunc;
float32 alphaRef;

struct UniformState
{
	float32 alphaRefLow;
	float32 alphaRefHigh;
	int32   pad[2];

	float32 fogStart;
	float32 fogEnd;
	float32 fogRange;
	float32 fogDisable;

	RGBAf   fogColor;
};

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	RawMatrix    world;
	RGBAf        ambLight;
	struct {
		float type;
		float radius;
		float minusCosAngle;
		float hardSpot;
	} lightParams[MAX_LIGHTS];
	V4d lightPosition[MAX_LIGHTS];
	V4d lightDirection[MAX_LIGHTS];
	RGBAf lightColor[MAX_LIGHTS];
};

struct GX2ShaderState
{
	RGBA matColor;
	SurfaceProperties surfProps;
};

static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;
static GX2ShaderState shaderState;

GX2Texture whitetex;
GX2Sampler whitesamp;

// State
int32 u_alphaRef;
int32 u_fogData;
int32 u_fogColor;

// Scene
int32 u_proj;
int32 u_view;

// Object
int32 u_world;
int32 u_ambLight;
int32 u_lightParams;
int32 u_lightPosition;
int32 u_lightDirection;
int32 u_lightColor;

int32 u_matColor;
int32 u_surfProps;

Shader *defaultShader;

static bool32 stateDirty = 1;
static bool32 sceneDirty = 1;
static bool32 objectDirty = 1;

struct RwRasterStateCache
{
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

#define MAXNUMSTAGES 8

// cached RW render states
struct RwStateCache {
	bool32 vertexAlpha;
	uint32 alphaTestEnable;
	uint32 alphaFunc;
	bool32 textureAlpha;
	bool32 blendEnable;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;

	// emulation of PS2 GS
	bool32 gsalpha;
	uint32 gsalpharef;

	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

enum
{
	RWGX2_BLEND,
	RWGX2_SRCBLEND,
	RWGX2_DESTBLEND,
	RWGX2_DEPTHTEST,
	RWGX2_DEPTHFUNC,
	RWGX2_DEPTHMASK,
	RWGX2_CULLFRONT,
	RWGX2_CULLBACK,
	RWGX2_ALPHAFUNC,
	RWGX2_ALPHAREF,
	RWGX2_FOG,
	RWGX2_FOGSTART,
	RWGX2_FOGEND,
	RWGX2_FOGCOLOR,

	RWGX2_NUM_STATES
};
static bool uniformStateDirty[RWGX2_NUM_STATES];

struct GX2State {
	bool32 blendEnable;
	GX2BlendMode srcblend, destblend;

	bool32 depthTest;
	GX2CompareFunction depthFunc;

	uint32 depthMask;

	bool32 cullFront;
	bool32 cullBack;
};
static GX2State curGX2State, oldGX2State;

static GX2BlendMode blendMap[] = {
	GX2_BLEND_MODE_ZERO,
	GX2_BLEND_MODE_ZERO,
	GX2_BLEND_MODE_ONE,
	GX2_BLEND_MODE_SRC_COLOR,
	GX2_BLEND_MODE_INV_SRC_COLOR,
	GX2_BLEND_MODE_SRC_ALPHA,
	GX2_BLEND_MODE_INV_SRC_ALPHA,
	GX2_BLEND_MODE_DST_ALPHA,
	GX2_BLEND_MODE_INV_DST_ALPHA,
	GX2_BLEND_MODE_DST_COLOR,
	GX2_BLEND_MODE_INV_DST_COLOR,
	GX2_BLEND_MODE_SRC_ALPHA_SAT,
};

/*
 * GX2 state cache
 */

void
setGX2RenderState(uint32 state, uint32 value)
{
	switch(state){
	case RWGX2_BLEND:  curGX2State.blendEnable = value; break;
	case RWGX2_SRCBLEND: curGX2State.srcblend = (GX2BlendMode) value; break;
	case RWGX2_DESTBLEND: curGX2State.destblend = (GX2BlendMode) value; break;
	case RWGX2_DEPTHTEST: curGX2State.depthTest = value; break;
	case RWGX2_DEPTHFUNC:  curGX2State.depthFunc = (GX2CompareFunction) value;  break;
	case RWGX2_DEPTHMASK: curGX2State.depthMask = value; break;
	case RWGX2_CULLFRONT: curGX2State.cullFront = value; break;
	case RWGX2_CULLBACK: curGX2State.cullBack = value; break;
	}
}

void
flushGX2RenderState(void)
{
	bool blendNeedsRefresh = false;
	bool depthNeedsRefresh = false;

	if(oldGX2State.blendEnable != curGX2State.blendEnable){
		oldGX2State.blendEnable = curGX2State.blendEnable;
		blendNeedsRefresh = true;
	}

	if(oldGX2State.srcblend != curGX2State.srcblend ||
	   oldGX2State.destblend != curGX2State.destblend){
		oldGX2State.srcblend = curGX2State.srcblend;
		oldGX2State.destblend = curGX2State.destblend;
		blendNeedsRefresh = true;
	}

	if(oldGX2State.depthTest != curGX2State.depthTest){
		oldGX2State.depthTest = curGX2State.depthTest;
		depthNeedsRefresh = true;
	}
	if(oldGX2State.depthFunc != curGX2State.depthFunc){
		oldGX2State.depthFunc = curGX2State.depthFunc;
		depthNeedsRefresh = true;
	}
	if(oldGX2State.depthMask != curGX2State.depthMask){
		oldGX2State.depthMask = curGX2State.depthMask;
		depthNeedsRefresh = true;
	}

	if(oldGX2State.cullFront != curGX2State.cullFront
		|| oldGX2State.cullBack != curGX2State.cullBack){
		oldGX2State.cullFront = curGX2State.cullFront;
		oldGX2State.cullBack = curGX2State.cullBack;
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, oldGX2State.cullFront, oldGX2State.cullBack);
	}

	if (blendNeedsRefresh)
	{
		GX2SetBlendControl(GX2_RENDER_TARGET_0,
							oldGX2State.blendEnable ? GX2_BLEND_MODE_SRC_ALPHA : GX2_BLEND_MODE_ONE,
							oldGX2State.blendEnable ? GX2_BLEND_MODE_INV_SRC_ALPHA : GX2_BLEND_MODE_ZERO,
                            GX2_BLEND_COMBINE_MODE_ADD,
						    TRUE,
                            oldGX2State.srcblend,
						    oldGX2State.destblend,
                            GX2_BLEND_COMBINE_MODE_ADD);
        GX2SetColorControl(GX2_LOGIC_OP_COPY, oldGX2State.blendEnable ? 0xFF : 0, FALSE, TRUE);
	}

	if (depthNeedsRefresh)
	{
		GX2SetDepthOnlyControl(oldGX2State.depthTest, oldGX2State.depthMask, oldGX2State.depthFunc);
	}
}

void
setAlphaBlend(bool32 enable)
{
	if(rwStateCache.blendEnable != enable){
		rwStateCache.blendEnable = enable;
		setGX2RenderState(RWGX2_BLEND, enable);
	}
}

bool32
getAlphaBlend(void)
{
	return rwStateCache.blendEnable;
}

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		if(rwStateCache.zwrite && !enable){
			// If we still want to write, enable but set mode to always
			setGX2RenderState(RWGX2_DEPTHTEST, true);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_ALWAYS);
		}else{
			setGX2RenderState(RWGX2_DEPTHTEST, rwStateCache.ztest);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_LEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	enable = enable ? true : false;
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			// Have to switch on ztest so writing can work
			setGX2RenderState(RWGX2_DEPTHTEST, true);
			setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_ALWAYS);
		}
		setGX2RenderState(RWGX2_DEPTHMASK, rwStateCache.zwrite);
	}
}

// TODO: GX2SetAlphaTest
static void
setAlphaTest(bool32 enable)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaTestEnable != enable){
		rwStateCache.alphaTestEnable = enable;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
			uniformStateDirty[RWGX2_ALPHAFUNC] = true;
			stateDirty = 1;
		}
	}
}

static void
setAlphaTestFunction(uint32 function)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaFunc != function){
		rwStateCache.alphaFunc = function;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
			uniformStateDirty[RWGX2_ALPHAFUNC] = true;
			stateDirty = 1;
		}
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			setAlphaBlend(enable);
			setAlphaTest(enable);
		}
		rwStateCache.vertexAlpha = enable;
	}
}

static GX2TexXYFilterMode filterConvMap_NoMIP[] = {
	GX2_TEX_XY_FILTER_MODE_POINT, 
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR,
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR,
	GX2_TEX_XY_FILTER_MODE_POINT, GX2_TEX_XY_FILTER_MODE_LINEAR
};

static GX2TexClampMode addressConvMap[] = {
	GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_CLAMP_MODE_MIRROR,
	GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_CLAMP_MODE_CLAMP_BORDER
};

static void
setFilterMode(uint32 stage, int32 filter)
{
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter)
	{
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster)
		{
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if(natras->filterMode != filter && natras->sampler)
			{
				GX2InitSamplerXYFilter(natras->sampler, filterConvMap_NoMIP[filter], filterConvMap_NoMIP[filter], GX2_TEX_ANISO_RATIO_NONE);
				natras->filterMode = filter;
			}
		}
	}
}

static void
setAddressU(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing)
	{
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster)
		{
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			if(natras->addressU != addressing && natras->sampler)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[addressing], addressConvMap[natras->addressV], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressU = addressing;
			}
		}
	}
}

static void
setAddressV(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing)
	{
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			if(natras->addressV != addressing && natras->sampler)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[natras->addressV], addressConvMap[addressing], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressV = addressing;
			}
		}
	}
}

static void
setRasterStageOnly(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster)
	{
		rwStateCache.texstage[stage].raster = raster;
		if(raster)
		{
			assert(raster->platform == PLATFORM_GX2);
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			GX2SetPixelTexture((GX2Texture*) natras->texture, currentShader->samplerLocation);
    		GX2SetPixelSampler(natras->sampler, currentShader->samplerLocation);

			rwStateCache.texstage[stage].filter = (rw::Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (rw::Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (rw::Texture::Addressing)natras->addressV;

			alpha = natras->hasAlpha;
		}
		else
		{
			GX2SetPixelTexture(&whitetex, currentShader->samplerLocation);
			GX2SetPixelSampler(&whitesamp, currentShader->samplerLocation);
			alpha = 0;
		}

		if(stage == 0)
		{
			if(alpha != rwStateCache.textureAlpha)
			{
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha)
				{
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

static void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster)
	{
		rwStateCache.texstage[stage].raster = raster;
		if(raster)
		{
			assert(raster->platform == PLATFORM_GX2);
			GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
			GX2SetPixelTexture((GX2Texture*) natras->texture, currentShader->samplerLocation);
    		GX2SetPixelSampler(natras->sampler, currentShader->samplerLocation);
			uint32 filter = rwStateCache.texstage[stage].filter;
			uint32 addrU = rwStateCache.texstage[stage].addressingU;
			uint32 addrV = rwStateCache.texstage[stage].addressingV;
			if(natras->filterMode != filter)
			{
				GX2InitSamplerXYFilter(natras->sampler, filterConvMap_NoMIP[filter], filterConvMap_NoMIP[filter], GX2_TEX_ANISO_RATIO_NONE);
				natras->filterMode = filter;
			}
			if(natras->addressU != addrU || natras->addressV != addrV)
			{
				GX2InitSamplerClamping(natras->sampler, addressConvMap[addrU], addressConvMap[addrV], GX2_TEX_CLAMP_MODE_WRAP);
				natras->addressU = addrU;
				natras->addressV = addrV;
			}
			alpha = natras->hasAlpha;
		}else
		{
			GX2SetPixelTexture(&whitetex, currentShader->samplerLocation);
			GX2SetPixelSampler(&whitesamp, currentShader->samplerLocation);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					setAlphaBlend(alpha);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

void
setTexture(int32 stage, Texture *tex)
{
	if(tex == nil || tex->raster == nil){
		setRasterStage(stage, nil);
		return;
	}
	setRasterStageOnly(stage, tex->raster);
	setFilterMode(stage, tex->getFilter());
	setAddressU(stage, tex->getAddressU());
	setAddressV(stage, tex->getAddressV());
}


static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:
		setRasterStage(0, (Raster*)pvalue);
		break;
	case TEXTUREADDRESS:
		setAddressU(0, value);
		setAddressV(0, value);
		break;
	case TEXTUREADDRESSU:
		setAddressU(0, value);
		break;
	case TEXTUREADDRESSV:
		setAddressV(0, value);
		break;
	case TEXTUREFILTER:
		setFilterMode(0, value);
		break;
	case VERTEXALPHA:
		setVertexAlpha(value);
		break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			setGX2RenderState(RWGX2_SRCBLEND, blendMap[rwStateCache.srcblend]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			setGX2RenderState(RWGX2_DESTBLEND, blendMap[rwStateCache.destblend]);
		}
		break;
	case ZTESTENABLE:
		setDepthTest(value);
		break;
	case ZWRITEENABLE:
		setDepthWrite(value);
		break;
	case FOGENABLE:
		if(rwStateCache.fogEnable != value){
			rwStateCache.fogEnable = value;
			uniformStateDirty[RWGX2_FOG] = true;
			stateDirty = 1;
		}
		break;
	case FOGCOLOR:
		// no cache check here...too lazy
		RGBA c;
		c.red = value;
		c.green = value>>8;
		c.blue = value>>16;
		c.alpha = value>>24;
		convColor(&uniformState.fogColor, &c);
		uniformStateDirty[RWGX2_FOGCOLOR] = true;
		stateDirty = 1;
		break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			if(rwStateCache.cullmode == CULLNONE) {
				setGX2RenderState(RWGX2_CULLFRONT, false);
				setGX2RenderState(RWGX2_CULLBACK, false);
			} else{
				setGX2RenderState(RWGX2_CULLFRONT, rwStateCache.cullmode == CULLBACK ? false : true);
				setGX2RenderState(RWGX2_CULLBACK, rwStateCache.cullmode == CULLBACK ? true : false);
			}
		}
		break;

	case ALPHATESTFUNC:
		setAlphaTestFunction(value);
		break;
	case ALPHATESTREF:
		if(alphaRef != value/255.0f){
			alphaRef = value/255.0f;
			uniformStateDirty[RWGX2_ALPHAREF] = true;
			stateDirty = 1;
		}
		break;
	case GSALPHATEST:
		rwStateCache.gsalpha = value;
		break;
	case GSALPHATESTREF:
		rwStateCache.gsalpharef = value;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 val;
	RGBA rgba;
	switch(state)
	{
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSV:
		val = rwStateCache.texstage[0].addressingV;
		break;
	case TEXTUREFILTER:
		val = rwStateCache.texstage[0].filter;
		break;

	case VERTEXALPHA:
		val = rwStateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = rwStateCache.srcblend;
		break;
	case DESTBLEND:
		val = rwStateCache.destblend;
		break;
	case ZTESTENABLE:
		val = rwStateCache.ztest;
		break;
	case ZWRITEENABLE:
		val = rwStateCache.zwrite;
		break;
	case FOGENABLE:
		val = rwStateCache.fogEnable;
		break;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		val = RWRGBAINT(rgba.red, rgba.green, rgba.blue, rgba.alpha);
		break;
	case CULLMODE:
		val = rwStateCache.cullmode;
		break;

	case ALPHATESTFUNC:
		val = rwStateCache.alphaFunc;
		break;
	case ALPHATESTREF:
		val = (uint32)(alphaRef*255.0f);
		break;
	case GSALPHATEST:
		val = rwStateCache.gsalpha;
		break;
	case GSALPHATESTREF:
		val = rwStateCache.gsalpharef;
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}


static void
resetRenderState(void)
{	
	rwStateCache.alphaFunc = ALPHAGREATEREQUAL;
	alphaFunc = 0;
	alphaRef = 10.0f/255.0f;
	uniformState.fogDisable = 1.0f;
	uniformState.fogStart = 0.0f;
	uniformState.fogEnd = 0.0f;
	uniformState.fogRange = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	rwStateCache.gsalpha = 0;
	rwStateCache.gsalpharef = 128;
	stateDirty = 1;

	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;
	rwStateCache.alphaTestEnable = 0;

	memset(&oldGX2State, 0xFF, sizeof(oldGX2State));

	rwStateCache.blendEnable = 0;
	setGX2RenderState(RWGX2_BLEND, false);
	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	setGX2RenderState(RWGX2_SRCBLEND, blendMap[rwStateCache.srcblend]);
	setGX2RenderState(RWGX2_DESTBLEND, blendMap[rwStateCache.destblend]);

	rwStateCache.zwrite = true;
	setGX2RenderState(RWGX2_DEPTHMASK, rwStateCache.zwrite);

	rwStateCache.ztest = 1;
	setGX2RenderState(RWGX2_DEPTHTEST, true);
	setGX2RenderState(RWGX2_DEPTHFUNC, GX2_COMPARE_FUNC_LEQUAL);

	rwStateCache.cullmode = CULLNONE;
	setGX2RenderState(RWGX2_CULLFRONT, false);
	setGX2RenderState(RWGX2_CULLBACK, false);
}

void
setWorldMatrix(Matrix *mat)
{
	convMatrix(&uniformObject.world, mat);
	objectDirty = 1;
}

int32
setLights(WorldLights *lightData)
{
	int i, n;
	Light *l;
	int32 bits;

	uniformObject.ambLight = lightData->ambient;

	bits = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	n = 0;
	for(i = 0; i < lightData->numDirectionals && i < 8; i++){
		l = lightData->directionals[i];
		uniformObject.lightParams[n].type = 1.0f;
		uniformObject.lightColor[n] = l->color;
		memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
		bits |= VSLIGHT_POINT;
		n++;
		if(n >= MAX_LIGHTS)
			goto out;
	}

	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			uniformObject.lightParams[n].type = 2.0f;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			bits |= VSLIGHT_POINT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			uniformObject.lightParams[n].type = 3.0f;
			uniformObject.lightParams[n].minusCosAngle = l->minusCosAngle;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				uniformObject.lightParams[n].hardSpot = 0.0f;
			else
				uniformObject.lightParams[n].hardSpot = 1.0f;
			bits |= VSLIGHT_SPOT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		}
	}

	uniformObject.lightParams[n].type = 0.0f;
out:
	objectDirty = 1;
	return bits;
}

void
setProjectionMatrix(float32 *mat)
{
	memcpy(&uniformScene.proj, mat, 64);
	sceneDirty = 1;
}

void
setViewMatrix(float32 *mat)
{
	memcpy(&uniformScene.view, mat, 64);
	sceneDirty = 1;
}

Shader *lastShaderUploaded;

void
setMaterial(const RGBA &color, const SurfaceProperties &surfaceprops)
{
	bool force = lastShaderUploaded != currentShader;
	if(force || !equal(shaderState.matColor, color)){
		rw::RGBAf col;
		convColor(&col, &color);
		setUniform(u_matColor, 4, &col);
		shaderState.matColor = color;
	}

	if(force ||
	   shaderState.surfProps.ambient != surfaceprops.ambient ||
	   shaderState.surfProps.specular != surfaceprops.specular ||
	   shaderState.surfProps.diffuse != surfaceprops.diffuse){
		float surfProps[4];
		surfProps[0] = surfaceprops.ambient;
		surfProps[1] = surfaceprops.specular;
		surfProps[2] = surfaceprops.diffuse;
		surfProps[3] = 0.0f;
		setUniform(u_surfProps, 4, surfProps);
		shaderState.surfProps = surfaceprops;
	}
}

void
flushCache(void)
{
	flushGX2RenderState();

	if(lastShaderUploaded != currentShader){
		lastShaderUploaded = currentShader;
		objectDirty = 1;
		sceneDirty = 1;
		stateDirty = 1;

		int i;
		for(i = 0; i < RWGX2_NUM_STATES; i++)
			uniformStateDirty[i] = true;
	}

	if(sceneDirty){
		setUniform(u_proj, 16, uniformScene.proj);
		setUniform(u_view, 16, uniformScene.view);
		sceneDirty = 0;
	}

	if(objectDirty){
		setUniform(u_world, 16, &uniformObject.world);
		setUniform(u_ambLight, 4, &uniformObject.ambLight);
		setUniform(u_lightParams, MAX_LIGHTS * 4, &uniformObject.lightParams);
		setUniform(u_lightPosition, MAX_LIGHTS * 4, uniformObject.lightPosition);
		setUniform(u_lightDirection, MAX_LIGHTS * 4, uniformObject.lightDirection);
		setUniform(u_lightColor, MAX_LIGHTS * 4, uniformObject.lightColor);
		objectDirty = 0;
	}

	uniformState.fogDisable = rwStateCache.fogEnable ? 0.0f : 1.0f;
	uniformState.fogStart = rwStateCache.fogStart;
	uniformState.fogEnd = rwStateCache.fogEnd;
	uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd);

	if(uniformStateDirty[RWGX2_ALPHAFUNC] || uniformStateDirty[RWGX2_ALPHAREF]){
		switch(alphaFunc){
		case ALPHAALWAYS:
		default:
		{
			float lalphaRef[2] = {-1000.0f, 1000.0f};
			setUniform(u_alphaRef, 2, lalphaRef);
		}
			break;
		case ALPHAGREATEREQUAL:
		{
			float lalphaRef[2] = {alphaRef, 1000.0f};
			setUniform(u_alphaRef, 2, lalphaRef);
		}
			break;
		case ALPHALESS:
		{
			float lalphaRef[2] = {-1000.0f, alphaRef};
			setUniform(u_alphaRef, 2, lalphaRef);
		}
			break;
		}
		uniformStateDirty[RWGX2_ALPHAFUNC] = false;
		uniformStateDirty[RWGX2_ALPHAREF] = false;
	}

	if(uniformStateDirty[RWGX2_FOG] ||
		uniformStateDirty[RWGX2_FOGSTART] ||
		uniformStateDirty[RWGX2_FOGEND]){
		float fogData[4] = {
			uniformState.fogStart,
			uniformState.fogEnd,
			uniformState.fogRange,
			uniformState.fogDisable
		};
		setUniform(u_fogData, 4, fogData);
		uniformStateDirty[RWGX2_FOG] = false;
		uniformStateDirty[RWGX2_FOGSTART] = false;
		uniformStateDirty[RWGX2_FOGEND] = false;
	}

	if(uniformStateDirty[RWGX2_FOGCOLOR]){
		setUniform(u_fogColor, 4, &uniformState.fogColor);
		uniformStateDirty[RWGX2_FOGCOLOR] = false;
	}
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, cam->frameBuffer, nativeRasterOffset);
	GX2Raster *natzb = PLUGINOFFSET(GX2Raster, cam->zBuffer, nativeRasterOffset);

	GX2ColorBuffer* frameBuf = (GX2ColorBuffer*) natfb->texture;
	GX2DepthBuffer* depthBuf = (GX2DepthBuffer*) natzb->texture;

	if(mode & Camera::CLEARIMAGE)
		GX2ClearColor(frameBuf, col->red / 255.0f, col->green / 255.0f, col->blue / 255.0f, col->alpha / 255.0f);
	if(mode & Camera::CLEARZ)
		GX2ClearDepthStencilEx(depthBuf, depthBuf->depthClear, depthBuf->stencilClear, (GX2ClearFlags) (GX2_CLEAR_FLAGS_DEPTH | GX2_CLEAR_FLAGS_STENCIL));
	
	GX2SetContextState(globals.contextState);
}

static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  =  -inv.at.x;
	view[9]  =   inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, &view, sizeof(RawMatrix));
	setViewMatrix(view);

	// Projection Matrix
	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		proj[10] = -(cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 0.0f;

		proj[14] = 2.0f*invz;
		proj[15] = 1.0f;
	}
	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));
	setProjectionMatrix(proj);

	if(rwStateCache.fogStart != cam->fogPlane){
		rwStateCache.fogStart = cam->fogPlane;
		uniformStateDirty[RWGX2_FOGSTART] = true;
		stateDirty = 1;
	}
	if(rwStateCache.fogEnd != cam->farPlane){
		rwStateCache.fogEnd = cam->farPlane;
		uniformStateDirty[RWGX2_FOGEND] = true;
		stateDirty = 1;
	}

	GX2WaitForVsync();

	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, cam->frameBuffer, nativeRasterOffset);
	GX2Raster *natzb = PLUGINOFFSET(GX2Raster, cam->zBuffer, nativeRasterOffset);

	GX2ColorBuffer* frameBuf = (GX2ColorBuffer*) natfb->texture;
	GX2DepthBuffer* depthBuf = (GX2DepthBuffer*) natzb->texture;

	GX2SetContextState(globals.contextState);

	GX2SetColorBuffer(frameBuf, GX2_RENDER_TARGET_0);
	GX2SetDepthBuffer(depthBuf);
	GX2SetViewport(0.0f, 0.0f, frameBuf->surface.width, frameBuf->surface.height, renderdevice.zNear, renderdevice.zFar);
	GX2SetScissor(0, 0, frameBuf->surface.width, frameBuf->surface.height);
	GX2SetDRCScale(frameBuf->surface.width, frameBuf->surface.height);
}

static void
endUpdate(Camera *cam)
{
	GX2Raster *natfb = PLUGINOFFSET(GX2Raster, cam->frameBuffer, nativeRasterOffset);
	GX2Raster *natzb = PLUGINOFFSET(GX2Raster, cam->zBuffer, nativeRasterOffset);

	GX2ColorBuffer* frameBuf = (GX2ColorBuffer*) natfb->texture;
	GX2DepthBuffer* depthBuf = (GX2DepthBuffer*) natzb->texture;

	GX2CopyColorBufferToScanBuffer(frameBuf, GX2_SCAN_TARGET_TV);
	GX2CopyColorBufferToScanBuffer(frameBuf, GX2_SCAN_TARGET_DRC);

	GX2SwapScanBuffers();
	GX2Flush();
	GX2DrawDone();
	GX2SetTVEnable(TRUE);
	GX2SetDRCEnable(TRUE);

	freeIm2DBuffers();
	freeIm3DBuffers();
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	GX2Raster *natdst = PLUGINOFFSET(GX2Raster, dst, nativeRasterOffset);
	GX2Raster *natsrc = PLUGINOFFSET(GX2Raster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	// case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			// TODO render raster to camera
			return 1;
		}
		break;
	}
	return 0;
}

static void*
GfxGX2RAlloc(GX2RResourceFlags flags, uint32_t size, uint32_t alignment)
{
	// Color, depth, scan buffers all belong in MEM1
	if ((flags & (GX2R_RESOURCE_BIND_COLOR_BUFFER
				| GX2R_RESOURCE_BIND_DEPTH_BUFFER
				| GX2R_RESOURCE_BIND_SCAN_BUFFER
				| GX2R_RESOURCE_USAGE_FORCE_MEM1))
		&& !(flags & GX2R_RESOURCE_USAGE_FORCE_MEM2)) {
		return GfxHeapAllocMEM1(size, alignment);
	} else {
		return GfxHeapAllocMEM2(size, alignment);
	}
}

static void
GfxGX2RFree(GX2RResourceFlags flags, void *block)
{
	if ((flags & (GX2R_RESOURCE_BIND_COLOR_BUFFER
				| GX2R_RESOURCE_BIND_DEPTH_BUFFER
				| GX2R_RESOURCE_BIND_SCAN_BUFFER
				| GX2R_RESOURCE_USAGE_FORCE_MEM1))
		&& !(flags & GX2R_RESOURCE_USAGE_FORCE_MEM2)) {
		return GfxHeapFreeMEM1(block);
	} else {
		return GfxHeapFreeMEM2(block);
	}
}

static int
openGX2(void)
{
	// TODO make use of tvwidth and height
	uint32_t tvWidth, tvHeight;
	uint32_t unk;

	globals.commandBufferPool = GfxHeapAllocMEM2(0x400000, GX2_COMMAND_BUFFER_ALIGNMENT);

	if (!globals.commandBufferPool)
	{
		WHBLogPrintf("%s: failed to allocate command buffer pool", __FUNCTION__);
		return FALSE;
	}

	uint32_t initAttribs[] = {
		GX2_INIT_CMD_BUF_BASE, (uintptr_t)globals.commandBufferPool,
		GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
		GX2_INIT_ARGC, 0,
		GX2_INIT_ARGV, 0,
		GX2_INIT_END
	};
	GX2Init(initAttribs);

	globals.drcRenderMode = GX2GetSystemDRCScanMode();
	globals.tvSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	globals.drcSurfaceFormat = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;

	switch(GX2GetSystemTVScanMode())
	{
	case GX2_TV_SCAN_MODE_480I:
	case GX2_TV_SCAN_MODE_480P:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_480P;
		tvWidth = 854;
		tvHeight = 480;
		break;
	case GX2_TV_SCAN_MODE_1080I:
	case GX2_TV_SCAN_MODE_1080P:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_1080P;
		tvWidth = 1920;
		tvHeight = 1080;
		break;
	case GX2_TV_SCAN_MODE_720P:
	default:
		globals.tvRenderMode = GX2_TV_RENDER_MODE_WIDE_720P;
		tvWidth = 1280;
		tvHeight = 720;
		break;
	}

	// Setup TV and DRC buffers - they will be allocated in GfxProcCallbackAcquired.
	GX2CalcTVSize(globals.tvRenderMode, globals.tvSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &globals.tvScanBufferSize, &unk);
	GX2CalcDRCSize(globals.drcRenderMode, globals.drcSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE, &globals.drcScanBufferSize, &unk);

	GX2RSetAllocator(&GfxGX2RAlloc, &GfxGX2RFree);

	if (!GfxHeapInitMEM1())
	{
		WHBLogPrintf("%s: GfxHeapInitMEM1 failed", __FUNCTION__);
		goto error;
	}

	if (!GfxHeapInitForeground())
	{
		WHBLogPrintf("%s: GfxHeapInitForeground failed", __FUNCTION__);
		goto error;
	}

	// Allocate TV scan buffer.
	globals.tvScanBuffer = GfxHeapAllocForeground(globals.tvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
	if (!globals.tvScanBuffer)
	{
		WHBLogPrintf("%s: sTvScanBuffer = GfxHeapAllocForeground(0x%X, 0x%X) failed", __FUNCTION__, globals.tvScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
		goto error;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, globals.tvScanBuffer, globals.tvScanBufferSize);
	GX2SetTVBuffer(globals.tvScanBuffer, globals.tvScanBufferSize, globals.tvRenderMode, globals.tvSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

	// Allocate DRC scan buffer.
	globals.drcScanBuffer = GfxHeapAllocForeground(globals.drcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
	if (!globals.drcScanBuffer)
	{
		WHBLogPrintf("%s: sDrcScanBuffer = GfxHeapAllocForeground(0x%X, 0x%X) failed", __FUNCTION__, globals.drcScanBufferSize, GX2_SCAN_BUFFER_ALIGNMENT);
		goto error;
	}
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, globals.drcScanBuffer, globals.drcScanBufferSize);
	GX2SetDRCBuffer(globals.drcScanBuffer, globals.drcScanBufferSize, globals.drcRenderMode, globals.drcSurfaceFormat, GX2_BUFFERING_MODE_DOUBLE);

	// Initialise TV context state.
	globals.contextState = (GX2ContextState*) GfxHeapAllocMEM2(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	if (!globals.contextState)
	{
		WHBLogPrintf("%s: failed to allocate sTvContextState", __FUNCTION__);
		goto error;
	}

	GX2SetupContextStateEx(globals.contextState, TRUE);
	GX2SetContextState(globals.contextState);

	// TODO maybe set vsync in gx2::ShowRaster?
	GX2SetSwapInterval(1);

	return TRUE;

error:
	if (globals.commandBufferPool)
	{
		GfxHeapFreeMEM2(globals.commandBufferPool);
		globals.commandBufferPool = NULL;
	}

	if (globals.tvScanBuffer)
	{
		GfxHeapFreeForeground(globals.tvScanBuffer);
		globals.tvScanBuffer = NULL;
	}

	if (globals.contextState)
	{
		GfxHeapFreeMEM2(globals.contextState);
		globals.contextState = NULL;
	}

	if (globals.drcScanBuffer)
	{
		GfxHeapFreeForeground(globals.drcScanBuffer);
		globals.drcScanBuffer = NULL;
	}
	return FALSE;
}

static int
closeGX2(void)
{
	GX2DrawDone();

	if (globals.tvScanBuffer)
	{
		GfxHeapFreeForeground(globals.tvScanBuffer);
		globals.tvScanBuffer = NULL;
	}

	if (globals.drcScanBuffer)
	{
		GfxHeapFreeForeground(globals.drcScanBuffer);
		globals.drcScanBuffer = NULL;
	}

	GfxHeapDestroyMEM1();
	GfxHeapDestroyForeground();

	GX2RSetAllocator(NULL, NULL);
	GX2Shutdown();

	if (globals.contextState)
	{
		GfxHeapFreeMEM2(globals.contextState);
		globals.contextState = NULL;
	}

	if (globals.commandBufferPool)
	{
		GfxHeapFreeMEM2(globals.commandBufferPool);
		globals.commandBufferPool = NULL;
	}
	return 1;
}

void createWhiteTexture(void)
{
	int width = 128;
	int height = 128;
	whitetex.surface.width = width;
	whitetex.surface.height = height;
	whitetex.surface.depth = 1;
	whitetex.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	whitetex.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	whitetex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	whitetex.viewNumSlices = 1;
	whitetex.compMap = 0x00010203;
	GX2CalcSurfaceSizeAndAlignment(&whitetex.surface);
	GX2InitTextureRegs(&whitetex);

	whitetex.surface.image = memalign(whitetex.surface.alignment, whitetex.surface.imageSize);
	memset(whitetex.surface.image, 0xFF, whitetex.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, whitetex.surface.image, whitetex.surface.imageSize);

	GX2InitSampler(&whitesamp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
}

static int
initGX2()
{
	u_alphaRef = registerUniform("u_alphaRef");
	u_fogData = registerUniform("u_fogData");
	u_fogColor = registerUniform("u_fogColor");
	u_proj = registerUniform("u_proj");
	u_view = registerUniform("u_view");
	u_world = registerUniform("u_world");
	u_ambLight = registerUniform("u_ambLight");
	u_lightParams = registerUniform("u_lightParams");
	u_lightPosition = registerUniform("u_lightPosition");
	u_lightDirection = registerUniform("u_lightDirection");
	u_lightColor = registerUniform("u_lightColor");
	u_matColor = registerUniform("u_matColor");
	u_surfProps = registerUniform("u_surfProps");
	lastShaderUploaded = nil;

	createWhiteTexture();

	resetRenderState();

	defaultShader = Shader::create(default_gsh);

	defaultShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	defaultShader->initAttribute("in_normal", 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	defaultShader->initAttribute("in_color", 24, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	defaultShader->initAttribute("in_tex0", 40, GX2_ATTRIB_FORMAT_FLOAT_32_32);

	defaultShader->init();
	defaultShader->use();

	defaultShader->samplerLocation = GX2GetPixelSamplerVarLocation(defaultShader->group.pixelShader, "tex0");

	openIm2D();
	openIm3D();

	// this should be fine to do here since all sampler locations are 0 in our shaders
	// not the best thing to do though
	GX2SetPixelTexture(&whitetex, currentShader->samplerLocation);
	GX2SetPixelSampler(&whitesamp, currentShader->samplerLocation);

	return 1;
}

static int
termGX2()
{
	defaultShader->destroy();

	closeIm3D();
	closeIm2D();

	return 1;
}

static int
deviceSystemGX2(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openGX2();
	case DEVICECLOSE:
		return closeGX2();

	case DEVICEINIT:
		return initGX2();
	case DEVICETERM:
		return termGX2();

	case DEVICEFINALIZE:
		return 1;

	case DEVICEGETNUMSUBSYSTEMS:
		return 1;

	case DEVICEGETCURRENTSUBSYSTEM:
		return 1;

	case DEVICESETSUBSYSTEM:
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		strncpy(((SubSystemInfo*)arg)->name, "Wii U", sizeof(SubSystemInfo::name));
		return 1;

	case DEVICEGETNUMVIDEOMODES:
		return 1;

	case DEVICEGETCURRENTVIDEOMODE:
		return 1;

	case DEVICESETVIDEOMODE:
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		// TODO: maybe use the actual tv size here?
		rwmode = (VideoMode*)arg;
		rwmode->width = 1280;
		rwmode->height = 720;
		rwmode->depth = 32;
		rwmode->flags = VIDEOMODEEXCLUSIVE;
		return 1;

	default:
		WHBLogPrintf("not implemented");
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

Device renderdevice = {
	0.0f, 1.0f,
	gx2::beginUpdate,
	gx2::endUpdate,
	gx2::clearCamera,
	null::showRaster,
	gx2::rasterRenderFast,
	gx2::setRenderState,
	gx2::getRenderState,

	gx2::im2DRenderLine,
	gx2::im2DRenderTriangle,
	gx2::im2DRenderPrimitive,
	gx2::im2DRenderIndexedPrimitive,

	gx2::im3DTransform,
	gx2::im3DRenderPrimitive,
	gx2::im3DRenderIndexedPrimitive,
	gx2::im3DEnd,

	gx2::deviceSystemGX2
};

}
}

#endif

