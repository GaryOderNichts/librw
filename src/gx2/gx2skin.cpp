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

namespace rw {
namespace gx2 {

// TODO

void
skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{

}

void
skinUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

void
uploadSkinMatrices(Atomic *a)
{

}

void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{

}

static void*
skinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_GX2] = makeSkinPipeline();

	return o;
}

static void*
skinClose(void *o, int32, int32)
{
	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_GX2])->destroy();
	skinGlobals.pipelines[PLATFORM_GX2] = nil;

	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_GX2, 0, ID_SKIN,
	                       skinOpen, skinClose);
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = skinUninstanceCB;
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

}
}

#endif