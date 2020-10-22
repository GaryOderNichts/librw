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
matfxDefaultRender(InstanceDataHeader *header, InstanceData *inst)
{

}

void
uploadEnvMatrix(Frame *frame)
{

}

void
matfxEnvRender(InstanceDataHeader *header, InstanceData *inst, MatFX::Env *env)
{

}

void
matfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{

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

	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_GX2])->destroy();
	matFXGlobals.pipelines[PLATFORM_GX2] = nil;

    return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_GX2, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}

}
}

#endif