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

void
initSkin(void)
{

}

ObjPipeline*
makeSkinPipeline(void)
{

}

}
}

#endif