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

}

void
initMatFX(void)
{

}

}
}

#endif