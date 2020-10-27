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
	tex->compMap = 0x00010203;

	switch(raster->format & 0xF00){
	case Raster::C888:
	case Raster::C8888:
		tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
		natras->hasAlpha = true;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	// case Raster::C888:
	// 	tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8;
	// 	natras->hasAlpha = true;
	// 	natras->bpp = 3;
	// 	raster->depth = 24;
	// 	break;
	case Raster::C1555:
		tex->surface.format = GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
		natras->hasAlpha = true;
		natras->bpp = 2;
		raster->depth = 16;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

	GX2RCreateSurface(&tex->surface, (GX2RResourceFlags)(GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ));
	GX2InitTextureRegs(tex);

	raster->stride = tex->surface.width * natras->bpp;

	natras->addressU = 0;
	natras->addressV = 0;

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

	WHBLogPrintf("rasterCreateCameraTexture not implemented");
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

	raster->stride = raster->width * natras->bpp;

	colorBuffer->surface.image = GfxHeapAllocMEM1(colorBuffer->surface.imageSize, colorBuffer->surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, colorBuffer->surface.image, colorBuffer->surface.imageSize);

	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->pixels = nil;

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
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, depthBuffer->surface.image, depthBuffer->surface.imageSize);

	uint32 zSize, zAlignment;
	GX2CalcDepthBufferHiZInfo(depthBuffer, &zSize, &zAlignment);
	depthBuffer->hiZPtr = GfxHeapAllocMEM1(zSize, zAlignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, depthBuffer->hiZPtr, zSize);
	GX2InitDepthBufferHiZEnable(depthBuffer, TRUE);

	return raster;
}

Raster*
rasterCreate(Raster *raster)
{
	if(raster->width == 0 || raster->height == 0)
	{
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		return raster;
	}
	if(raster->flags & Raster::DONTALLOCATE)
		return raster;

	switch(raster->type)
	{
	case Raster::NORMAL:
	case Raster::TEXTURE:
		return rasterCreateTexture(raster);
	case Raster::CAMERATEXTURE:
		return rasterCreateCameraTexture(raster);
	case Raster::ZBUFFER:
		return rasterCreateZbuffer(raster);
	case Raster::CAMERA:
		return rasterCreateCamera(raster);

	default:
		break;
	}

	RWERROR((ERR_INVRASTER));
	return NULL;
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
	// case Raster::CAMERATEXTURE:
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
rasterNumLevels(Raster*)
{
	return 1;
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
	switch(image->depth){
	case 32:
		if(raster->format == Raster::C8888)
		{
			conv = conv_RGBA8888_from_RGBA8888;
		}
		else if(raster->format == Raster::C888)
		{
			// conv = conv_RGB888_from_RGB888;
			conv = conv_RGBA8888_from_RGB888;
		}
		else
			goto err;
		break;
	case 24:
		if(raster->format == Raster::C8888)
		{
			conv = conv_RGBA8888_from_RGB888;
		}
		else if(raster->format == Raster::C888)
		{
			// conv = conv_RGB888_from_RGB888;
			conv = conv_RGBA8888_from_RGB888;
		}
		else
			goto err;
		break;
	case 16:
		if(raster->format == Raster::C1555)
			conv = conv_RGBA5551_from_ARGB1555;
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

	uint8 *pixels = raster->lock(0, Raster::LOCKWRITE | Raster::LOCKNOFETCH);
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
		if(i < raster->getNumLevels()){
			data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
			stream->read8(data, size);
			raster->unlock(i);
		}else
			stream->seek(size);
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
	int32 numLevels = raster->getNumLevels();
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
	uint32 size = 12 + 72 + 20;
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
		}
		free(natras->sampler);
		break;
	case Raster::CAMERATEXTURE:
		break;
	case Raster::ZBUFFER:
		if (natras->texture)
		{
			GfxHeapFreeMEM1(((GX2DepthBuffer*)natras->texture)->surface.image);
			GfxHeapFreeMEM1(((GX2DepthBuffer*)natras->texture)->hiZPtr);
			free(natras->texture);
		}
		break;
	case Raster::CAMERA:
		if (natras->texture)
		{
			GfxHeapFreeMEM1(((GX2ColorBuffer*)natras->texture)->surface.image);
			free(natras->texture);
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