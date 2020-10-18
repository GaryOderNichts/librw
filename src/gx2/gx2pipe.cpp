#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

// TODO

void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_GX2)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	GX2RDestroyBufferEx(&header->indexBuffer, (GX2RResourceFlags) 0);
	GX2RDestroyBufferEx(&header->vertexBuffer, (GX2RResourceFlags) 0);
	rwFree(header->inst);
	rwFree(header);
}

void*
destroyNativeData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

static InstanceDataHeader*
instanceMesh(rw::ObjPipeline *rwpipe, Geometry *geo)
{
	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY);
	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_GX2;

	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ? GX2_PRIMITIVE_MODE_LINE_STRIP : GX2_PRIMITIVE_MODE_TRIANGLES;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);

	header->indexBuffer.flags = (GX2RResourceFlags) (GX2R_RESOURCE_BIND_INDEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
	header->indexBuffer.elemSize = 2;
	header->indexBuffer.elemCount = header->totalNumIndex;
	GX2RCreateBuffer(&header->indexBuffer);

	uint16* indices = (uint16*) GX2RLockBufferEx(&header->indexBuffer, (GX2RResourceFlags) 0);
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 startindex = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, (int32*) &inst->numVertices);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->baseIndex = inst->minVert;
		inst->startIndex = startindex;
		if(inst->minVert == 0)
			memcpy(&indices[inst->startIndex], mesh->indices, inst->numIndex*2);
		else
			for(uint32 j = 0; j < inst->numIndex; j++)
				indices[inst->startIndex+j] = mesh->indices[j] - inst->minVert;
		startindex += inst->numIndex;
		mesh++;
		inst++;
	}
	GX2RUnlockBufferEx(&header->indexBuffer, (GX2RResourceFlags) 0);

	return header;
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	// don't try to (re)instance native data
	if(geo->flags & Geometry::NATIVE)
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(geo->instData){
		// Already have instanced data, so check if we have to reinstance
		assert(header->platform == PLATFORM_GL3);
		if(header->serialNumber != geo->meshHeader->serialNum){
			// Mesh changed, so reinstance everything
			freeInstanceData(geo);
		}
	}

	// no instance or complete reinstance
	if(geo->instData == nil){
		geo->instData = instanceMesh(rwpipe, geo);
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 0);
	}else if(geo->lockedSinceInst)
		pipe->instanceCB(geo, (InstanceDataHeader*)geo->instData, 1);

	geo->lockedSinceInst = 0;
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if((geo->flags & Geometry::NATIVE) == 0)
		return;
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_GX2);
	geo->numTriangles = geo->meshHeader->guessNumTriangles();
	geo->allocateData();
	geo->allocateMeshes(geo->meshHeader->numMeshes, geo->meshHeader->totalIndices, 0);

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	uint16 *indices = (uint16*) GX2RLockBufferEx(&header->indexBuffer, (GX2RResourceFlags) 0);
	InstanceData *inst = header->inst;
	Mesh *mesh = geo->meshHeader->getMeshes();
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(inst->minVert == 0)
			memcpy(mesh->indices, &indices[inst->startIndex], inst->numIndex*2);
		else
			for(uint32 j = 0; j < inst->numIndex; j++)
				mesh->indices[j] = indices[inst->startIndex+j] + inst->minVert;
		mesh++;
		inst++;
	}
	GX2RUnlockBufferEx(&header->indexBuffer, (GX2RResourceFlags) 0);

	pipe->uninstanceCB(geo, header);
	geo->generateTriangles();
	geo->flags &= ~Geometry::NATIVE;
	destroyNativeData(geo, 0, 0);
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	pipe->instance(atomic);
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_GL3);
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_GX2);
	this->impl.instance = gx2::instance;
	this->impl.uninstance = gx2::uninstance;
	this->impl.render = gx2::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}

ObjPipeline*
ObjPipeline::create(void)
{
	ObjPipeline *pipe = rwNewT(ObjPipeline, 1, MEMDUR_GLOBAL);
	pipe->init();
	return pipe;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	// TODO: make all of this less hardcoded

	int stride = 3 + 3 + 4 + 2;

	if(!reinstance)
	{
		header->vertexBuffer.flags = (GX2RResourceFlags) (GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
		header->vertexBuffer.elemSize = stride * sizeof(float);
		header->vertexBuffer.elemCount = header->totalNumVertex;
		GX2RCreateBuffer(&header->vertexBuffer);
	}

	//
	// Fill vertex buffer
	//

	float* verts = (float*) GX2RLockBufferEx(&header->vertexBuffer, (GX2RResourceFlags) 0);
	// TODO we should rather not set an attribute instead of setting everything to 0
	memset(verts, 0, header->totalNumVertex * stride);

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES){
		int v = 0;
		for (int i = 0; i < header->totalNumVertex; i++)
		{
			V3d vert = geo->morphTargets[0].vertices[i];
			verts[v] = vert.x;
			verts[v+1] = vert.y;
			verts[v+2] = vert.z;

			v += stride;
		}
	}

	// Normals
	if(hasNormals && (!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS)){
		int v = 3;
		for (int i = 0; i < header->totalNumVertex; i++)
		{
			V3d vert = geo->morphTargets[0].normals[i];
			verts[v] = vert.x;
			verts[v+1] = vert.y;
			verts[v+2] = vert.z;

			v += stride;
		}
	}

	// Prelighting
	if(isPrelit && (!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT)){
		int n = header->numMeshes;
		InstanceData *inst = header->inst;
		while(n--){
			assert(inst->minVert != 0xFFFFFFFF);
			// TODO
		}
	}

	if(!reinstance || geo->lockedSinceInst&(Geometry::LOCKTEXCOORDS<<0)){
		int v = 10;
		for (int i = 0; i < header->totalNumVertex; i++)
		{
			TexCoords coo = geo->texCoords[0][i];
			verts[v] = coo.u;
			verts[v+1] = coo.v;

			v += stride;
		}
	}

	GX2RUnlockBufferEx(&header->vertexBuffer, (GX2RResourceFlags) 0);
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

}
}

#endif