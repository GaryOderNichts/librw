#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#ifdef __WIIU__

#define PLUGIN_ID ID_DRIVER

#include "rwgx2.h"
#include "rwgx2shader.h"

#include "gfx_heap.h"
#include <whb/log.h>

namespace rw {
namespace gx2 {

int32 nativeRasterOffset;

static Raster*
rasterCreateTexture(Raster *raster)
{
	if(raster->format & (Raster::PAL4 | Raster::PAL8)) {
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	natras->texture = calloc(1, sizeof(GX2Texture));

	GX2Texture* tex = (GX2Texture*) natras->texture;
	memset(tex, 0, sizeof(GX2Texture));
	tex->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	tex->surface.use = GX2_SURFACE_USE_TEXTURE;
	tex->surface.width = raster->width;
	tex->surface.height = raster->height;
	tex->surface.depth = 1;
	tex->surface.aa = GX2_AA_MODE1X;
	tex->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	tex->viewNumSlices = 1;
	tex->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);

	switch(raster->format & 0xF00){
	case Raster::C888:
	case Raster::C8888:
	case Raster::C1555:
		tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
		natras->hasAlpha = true;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

	if(raster->format & Raster::MIPMAP){
		int w = raster->width;
		int h = raster->height;
		natras->numLevels = 0;
		while(w != 1 || h != 1){
			natras->numLevels++;
			if(w > 1) w /= 2;
			if(h > 1) h /= 2;
		}
	}
	natras->autogenMipmap = (raster->format & (Raster::MIPMAP|Raster::AUTOMIPMAP)) == (Raster::MIPMAP|Raster::AUTOMIPMAP);
	if(natras->autogenMipmap)
		natras->numLevels = 1;

	tex->surface.mipLevels = natras->numLevels;
	tex->viewNumMips = natras->numLevels;

	GX2RCreateSurface(&tex->surface, (GX2RResourceFlags)(GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ));
	GX2InitTextureRegs(tex);

	raster->stride = tex->surface.pitch * natras->bpp;

	natras->addressU = 0;
	natras->addressV = 0;
	natras->maxAnisotropy = 1;

	natras->sampler = (GX2Sampler*) calloc(1, sizeof(GX2Sampler));
	GX2InitSampler(natras->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

	return raster;
}

static Raster*
rasterCreateCameraTexture(Raster *raster)
{
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	natras->texture = calloc(1, sizeof(GX2Texture));

	GX2Texture* tex = (GX2Texture*) natras->texture;
	memset(tex, 0, sizeof(GX2Texture));
	tex->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	tex->surface.use = GX2_SURFACE_USE_TEXTURE;
	tex->surface.width = raster->width;
	tex->surface.height = raster->height;
	tex->surface.depth = 1;
	tex->surface.mipLevels = 1;
	tex->surface.aa = GX2_AA_MODE1X;
	tex->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	tex->viewNumMips = 1;
	tex->viewNumSlices = 1;

	switch(raster->format & 0xF00){
	case Raster::C8888:
	case Raster::C1555:
		tex->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
		tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
		natras->hasAlpha = true;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	case Raster::C888:
	default:
		// disable alpha in our comp map
		tex->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_1);
		tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
		natras->hasAlpha = true;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	}

	GX2RCreateSurface(&tex->surface, (GX2RResourceFlags)(GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ));
	GX2InitTextureRegs(tex);

	raster->stride = tex->surface.pitch * natras->bpp;

	natras->autogenMipmap = (raster->format & (Raster::MIPMAP|Raster::AUTOMIPMAP)) == (Raster::MIPMAP|Raster::AUTOMIPMAP);

	natras->addressU = 0;
	natras->addressV = 0;
	natras->maxAnisotropy = 1;

	natras->sampler = (GX2Sampler*) calloc(1, sizeof(GX2Sampler));
	GX2InitSampler(natras->sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

	return raster;
}

static void
GfxInitColorBuffer(GX2ColorBuffer *cb, uint32_t width, uint32_t height, GX2SurfaceFormat format, GX2AAMode aa)
{
	memset(cb, 0, sizeof(GX2ColorBuffer));
	cb->surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	cb->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	cb->surface.width = width;
	cb->surface.height = height;
	cb->surface.depth = 1;
	cb->surface.mipLevels = 1;
	cb->surface.format = format;
	cb->surface.aa = aa;
	cb->surface.tileMode = GX2_TILE_MODE_DEFAULT;
	cb->viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&cb->surface);
	GX2InitColorBufferRegs(cb);
}

static Raster*
rasterCreateCamera(Raster *raster)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	natras->texture = calloc(1, sizeof(GX2ColorBuffer));

	GX2ColorBuffer* colorBuffer = (GX2ColorBuffer*) natras->texture;

	GfxInitColorBuffer(colorBuffer, raster->width, raster->height, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_AA_MODE1X);

	raster->stride = colorBuffer->surface.pitch * natras->bpp;

	colorBuffer->surface.image = GfxHeapAllocMEM1(colorBuffer->surface.imageSize, colorBuffer->surface.alignment);
	addToMEM1Buffer(&colorBuffer->surface.image, colorBuffer->surface.imageSize, colorBuffer->surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, colorBuffer->surface.image, colorBuffer->surface.imageSize);

	natras->autogenMipmap = 0;

	return raster;
}

static void
GfxInitDepthBuffer(GX2DepthBuffer *db, uint32_t width, uint32_t height, GX2SurfaceFormat format, GX2AAMode aa)
{
	memset(db, 0, sizeof(GX2DepthBuffer));

	if (format == GX2_SURFACE_FORMAT_UNORM_R24_X8 || format == GX2_SURFACE_FORMAT_FLOAT_D24_S8) {
		db->surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
	} else {
		db->surface.use = (GX2SurfaceUse) (GX2_SURFACE_USE_DEPTH_BUFFER | GX2_SURFACE_USE_TEXTURE);
	}

	db->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	db->surface.width = width;
	db->surface.height = height;
	db->surface.depth = 1;
	db->surface.mipLevels = 1;
	db->surface.format = format;
	db->surface.aa = aa;
	db->surface.tileMode = GX2_TILE_MODE_DEFAULT;
	db->viewNumSlices = 1;
	db->depthClear = 1.0f;
	GX2CalcSurfaceSizeAndAlignment(&db->surface);
	GX2InitDepthBufferRegs(db);
}

static Raster*
rasterCreateZbuffer(Raster *raster)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;

	natras->texture = calloc(1, sizeof(GX2DepthBuffer));

	GX2DepthBuffer* depthBuffer = (GX2DepthBuffer*) natras->texture;

	GfxInitDepthBuffer(depthBuffer, raster->width, raster->height, GX2_SURFACE_FORMAT_FLOAT_R32, GX2_AA_MODE1X);

	depthBuffer->surface.image = GfxHeapAllocMEM1(depthBuffer->surface.imageSize, depthBuffer->surface.alignment);
	addToMEM1Buffer(&depthBuffer->surface.image, depthBuffer->surface.imageSize, depthBuffer->surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, depthBuffer->surface.image, depthBuffer->surface.imageSize);

	uint32 zSize, zAlignment;
	GX2CalcDepthBufferHiZInfo(depthBuffer, &zSize, &zAlignment);
	depthBuffer->hiZPtr = GfxHeapAllocMEM1(zSize, zAlignment);
	addToMEM1Buffer(&depthBuffer->hiZPtr, zSize, zAlignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, depthBuffer->hiZPtr, zSize);
	GX2InitDepthBufferHiZEnable(depthBuffer, TRUE);

	return raster;
}

Raster*
rasterCreate(Raster *raster)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	natras->isCompressed = 0;
	natras->hasAlpha = 0;
	natras->numLevels = 1;

	Raster *ret = raster;

	if(raster->width == 0 || raster->height == 0)
	{
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		goto ret;
	}
	if(raster->flags & Raster::DONTALLOCATE)
		goto ret;

	switch(raster->type)
	{
	case Raster::NORMAL:
	case Raster::TEXTURE:
		ret = rasterCreateTexture(raster);
		break;
	case Raster::CAMERATEXTURE:
		ret = rasterCreateCameraTexture(raster);
		break;
	case Raster::ZBUFFER:
		ret = rasterCreateZbuffer(raster);
		break;
	case Raster::CAMERA:
		ret = rasterCreateCamera(raster);
		break;

	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

	ret:
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->originalStride = raster->stride;
	raster->originalPixels = raster->pixels;
	return ret;
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	GX2Texture* tex = (GX2Texture*) natras->texture;

	switch(raster->type & 0xF00)
	{
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		assert(raster->pixels == nil);
		raster->pixels = (uint8*) GX2RLockSurfaceEx(&tex->surface, level, (GX2RResourceFlags) 0);
		raster->privateFlags = lockMode;
		break;

	default:
		WHBLogPrintf("cannot lock this type of raster yet");
		assert(0 && "cannot lock this type of raster yet");
		return nil;
	}

	return raster->pixels;
}

void
rasterUnlock(Raster *raster, int32 level)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	GX2Texture* tex = (GX2Texture*) natras->texture;

	GX2RUnlockSurfaceEx(&tex->surface, level, (GX2RResourceFlags) 0);

	raster->pixels = nil;
	raster->privateFlags = 0;
}

int32
rasterNumLevels(Raster* raster)
{
	return PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset)->numLevels;
}

bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 width, height, depth, format;

	assert((type&0xF) == Raster::TEXTURE);

	width = img->width;
	height = img->height;
	depth = img->depth;

	if(depth <= 8)
		depth = 32;

	switch(depth)
	{
	case 32:
		if(img->hasAlpha())
			format = Raster::C8888;
		else{
			format = Raster::C888;
			depth = 24;
		}
		break;
	case 24:
		format = Raster::C888;
		break;
	case 16:
		format = Raster::C1555;
		break;

	case 8:
	case 4:
	default:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	format |= type;

	*pWidth = width;
	*pHeight = height;
	*pDepth = depth;
	*pFormat = format;

	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type&0xF) != Raster::TEXTURE)
		return 0;

	void (*conv)(uint8 *out, uint8 *in) = nil;

	// Unpalettize image if necessary but don't change original
	Image *truecolimg = nil;
	if(image->depth <= 8){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unpalettize();
		image = truecolimg;
	}

	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	int32 format = raster->format&0xF00;
	switch(image->depth){
	case 32:
		if(format == Raster::C8888)
		{
			conv = conv_RGBA8888_from_RGBA8888;
		}
		else if(format == Raster::C888)
		{
			conv = conv_RGBA8888_from_RGB888;
		}
		else
			goto err;
		break;
	case 24:
		if(format == Raster::C8888)
		{
			conv = conv_RGBA8888_from_RGB888;
		}
		else if(format == Raster::C888)
		{
			conv = conv_RGBA8888_from_RGB888;
		}
		else
			goto err;
		break;
	case 16:
		if(format == Raster::C1555)
			conv = conv_RGBA8888_from_ARGB1555;
		else
			goto err;
		break;

	case 8:
	case 4:
	default:
	err:
		WHBLogPrintf("inv raster");
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	natras->hasAlpha = image->hasAlpha();

	bool unlock = false;
	if(raster->pixels == nil){
		raster->lock(0, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
		unlock = true;
	}

	uint8 *pixels = raster->pixels;
	assert(pixels);

	uint8 *imgpixels = image->pixels + (image->height-1)*image->stride;

	int x, y;
	assert(image->width == raster->width);
	assert(image->height == raster->height);
	for(y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(x = 0; x < image->width; x++){
			conv(rasrow, imgrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels -= image->stride;
		pixels += raster->stride;
	}
	if(unlock)
		raster->unlock(0);

	if(truecolimg)
		truecolimg->destroy();

	return 1;
}

static uint32
getLevelSize(Raster *raster, int32 level)
{
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	uint32 size = raster->stride*raster->height;
	while(level--)
		size /= 4;
	return size;
}

Texture*
readNativeTexture(Stream *stream)
{
	uint32 platform;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	platform = stream->readU32();
	if(platform != PLATFORM_GX2){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}
	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	// Texture
	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	// Raster
	uint32 format = stream->readU32();
	int32 width = stream->readI32();
	int32 height = stream->readI32();
	int32 depth = stream->readI32();
	int32 numLevels = stream->readI32();

	Raster *raster;
	GX2Raster *natras;
	raster = Raster::create(width, height, depth, format | Raster::TEXTURE, PLATFORM_GX2);
	assert(raster);
	natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);
	tex->raster = raster;

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = stream->readU32();
		data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
		stream->read8(data, size);
		raster->unlock(i);
	}
	return tex;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, raster, nativeRasterOffset);

	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_STRUCT, chunksize-12);
	stream->writeU32(PLATFORM_GX2);

	// Texture
	stream->writeU32(tex->filterAddressing);
	stream->write8(tex->name, 32);
	stream->write8(tex->mask, 32);

	// Raster
	int32 numLevels = natras->numLevels;
	stream->writeI32(raster->format);
	stream->writeI32(raster->width);
	stream->writeI32(raster->height);
	stream->writeI32(raster->depth);
	stream->writeI32(numLevels);

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = getLevelSize(raster, i);
		stream->writeU32(size);
		data = raster->lock(i, Raster::LOCKREAD);
		stream->write8(data, size);
		raster->unlock(i);
	}
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 72 + 28;
	int32 levels = tex->raster->getNumLevels();
	for(int32 i = 0; i < levels; i++)
		size += 4 + getLevelSize(tex->raster, i);
	return size;
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	GX2Raster *ras = PLUGINOFFSET(GX2Raster, object, offset);
	ras->bpp = 0;
	ras->texture = NULL;

	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	Raster *raster = (Raster*)object;
	GX2Raster *natras = PLUGINOFFSET(GX2Raster, object, offset);

	if (raster->platform != PLATFORM_GX2)
		return object;

	switch (raster->type)
	{
	case Raster::NORMAL:
	case Raster::TEXTURE:
		if (natras->texture)
		{
			GX2RDestroySurfaceEx(&((GX2Texture*)natras->texture)->surface, (GX2RResourceFlags) 0);
			free(natras->texture);
			natras->texture = nil;
		}
		free(natras->sampler);
		natras->sampler = nil;
		break;
	case Raster::CAMERATEXTURE:
		if (natras->texture)
		{
			GX2RDestroySurfaceEx(&((GX2Texture*)natras->texture)->surface, (GX2RResourceFlags) 0);
			free(natras->texture);
			natras->texture = nil;
		}
		free(natras->sampler);
		natras->sampler = nil;
		break;
	case Raster::ZBUFFER:
		if (natras->texture)
		{
			removeFromMEM1Buffer(((GX2DepthBuffer*)natras->texture)->surface.image);
			removeFromMEM1Buffer(((GX2DepthBuffer*)natras->texture)->hiZPtr);
			if (gfxInForeground) {
				GfxHeapFreeMEM1(((GX2DepthBuffer*)natras->texture)->surface.image);
				GfxHeapFreeMEM1(((GX2DepthBuffer*)natras->texture)->hiZPtr);
			}
			free(natras->texture);
			natras->texture = nil;
		}
		break;
	case Raster::CAMERA:
		if (natras->texture)
		{
			removeFromMEM1Buffer(((GX2ColorBuffer*)natras->texture)->surface.image);
			if (gfxInForeground) {
				GfxHeapFreeMEM1(((GX2ColorBuffer*)natras->texture)->surface.image);
			}
			free(natras->texture);
			natras->texture = nil;
		}
		break;

	default:
		break;
	}

	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	GX2Raster *ras = PLUGINOFFSET(GX2Raster, dst, offset);
	ras->bpp = 0;
	ras->texture = NULL;

	return dst;
}

void registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(GX2Raster),
	                                            ID_RASTERGX2,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

}
}

#endif