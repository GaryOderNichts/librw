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
#include "../rwanim.h"
#include "../rwplugins.h"

#ifdef __WIIU__
#include "rwgx2.h"
#include "rwgx2shader.h"
#include "rwgx2plg.h"
#include "rwgx2impl.h"

#include "shaders/skin.h"

namespace rw {
namespace gx2 {

static Shader *skinShader;
static int32 u_boneMatrices;

void
skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	AttribDesc *attribs, *a;

	bool isPrelit = !!(geo->flags & Geometry::PRELIT);
	bool hasNormals = !!(geo->flags & Geometry::NORMALS);

	if(!reinstance)
	{
		AttribDesc tmpAttribs[12];
		uint32 stride;

		a = tmpAttribs;
		stride = 0;

		// pos
		a->index = ATTRIB_POS;
		a->size = 3;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// normals
		a->index = ATTRIB_NORMAL;
		a->size = 3;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// color
		a->index = ATTRIB_COLOR;
		a->size = 4;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// tex coords
		a->index = ATTRIB_TEXCOORDS0;
		a->size = 2;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// Weights
		a->index = ATTRIB_WEIGHTS;
		a->size = 4;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		// Indices
		a->index = ATTRIB_INDICES;
		a->size = 4;
		a->offset = stride;
		stride += a->size*sizeof(float);
		a++;

		header->numAttribs = a - tmpAttribs;
		for(a = tmpAttribs; a != &tmpAttribs[header->numAttribs]; a++)
			a->stride = stride;
		
		header->attribDesc = rwNewT(AttribDesc, header->numAttribs, MEMDUR_EVENT | ID_GEOMETRY);
		memcpy(header->attribDesc, tmpAttribs, header->numAttribs*sizeof(AttribDesc));

		header->vertexBuffer = (float*) memalign(GX2_VERTEX_BUFFER_ALIGNMENT, header->totalNumVertex * stride);

		header->skinPosData = rwNewT(V3d, header->totalNumVertex, MEMDUR_EVENT | ID_GEOMETRY);
	}

	Skin *skin = Skin::get(geo);
	attribs = header->attribDesc;

	//
	// Fill vertex buffer
	//

	uint8* verts = (uint8*) header->vertexBuffer;

	// Positions
	if(!reinstance || geo->lockedSinceInst&Geometry::LOCKVERTICES) {
		for(a = attribs; a->index != ATTRIB_POS; a++);

		// instV3d(VERT_FLOAT3, verts + a->offset, geo->morphTargets[0].vertices,
		// 	header->totalNumVertex, a->stride);

		memcpy(header->skinPosData, geo->morphTargets[0].vertices, sizeof(V3d)*header->totalNumVertex);
	}

	// Normals
	if (!reinstance || geo->lockedSinceInst&Geometry::LOCKNORMALS) {
		if(hasNormals) {
			for(a = attribs; a->index != ATTRIB_NORMAL; a++);

			instV3d(VERT_FLOAT3, verts + a->offset, geo->morphTargets[0].normals,
				header->totalNumVertex, a->stride);
		}
		else {
			for(a = attribs; a->index != ATTRIB_NORMAL; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 0.0f;
				f_verts[2] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	// Prelighting
	if (!reinstance || geo->lockedSinceInst&Geometry::LOCKPRELIGHT) {
		if(isPrelit) {
			for(a = attribs; a->index != ATTRIB_COLOR; a++);

			int n = header->numMeshes;
			InstanceData *inst = header->inst;
			while(n--){
				assert(inst->minVert != 0xFFFFFFFF);
				inst->vertexAlpha = instColorFloat(VERT_RGBA,
					verts + a->offset + a->stride*inst->minVert,
					geo->colors + inst->minVert,
					inst->numVertices, a->stride);
				inst++;
			}
		}
		else {
			for(a = attribs; a->index != ATTRIB_COLOR; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 0.0f;
				f_verts[2] = 0.0f;
				f_verts[3] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	// Texture coordinates
	if(!reinstance || geo->lockedSinceInst&(Geometry::LOCKTEXCOORDS)) {
		if (geo->numTexCoordSets > 0) {
			for(a = attribs; a->index != ATTRIB_TEXCOORDS0; a++);

			instTexCoords(VERT_FLOAT2, verts + a->offset, geo->texCoords[0],
				header->totalNumVertex, a->stride);
		}
		else {
			for(a = attribs; a->index != ATTRIB_TEXCOORDS0; a++);

			float* f_verts = (float*)(verts + a->offset);
			for (uint32 i = 0; i < header->totalNumVertex; i++) {
				f_verts[0] = 0.0f;
				f_verts[1] = 1.0f;
				f_verts += a->stride/sizeof(float);
			}
		}
	}

	// Weights
	if(!reinstance){
		for(a = attribs; a->index != ATTRIB_WEIGHTS; a++);
		float *w = skin->weights;
		instV4d(VERT_FLOAT4, verts + a->offset,
			(V4d*)w,
			header->totalNumVertex, a->stride);
	}

	// Indices
	if(!reinstance){
		for(a = attribs; a->index != ATTRIB_INDICES; a++);
		// not really colors of course but what the heck
		instColorFloat_nonNormalized(VERT_RGBA, verts + a->offset,
			  (RGBA*)skin->indices,
			  header->totalNumVertex, a->stride);
	}

	// GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, verts, header->totalNumVertex * attribs->stride);
}

void
skinUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

static float skinMatrices[64*16];

void
uploadSkinMatrices(Atomic *a)
{
	int i;
	Skin *skin = Skin::get(a->geometry);
	Matrix *m = (Matrix*)skinMatrices;
	HAnimHierarchy *hier = Skin::getHierarchy(a);

	if(hier){
		Matrix *invMats = (Matrix*)skin->inverseMatrices;
		Matrix tmp;

		assert(skin->numBones == hier->numNodes);
		if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(m, &invMats[i], &hier->matrices[i]);
				m++;
			}
		}else{
			Matrix invAtmMat;
			Matrix::invert(&invAtmMat, a->getFrame()->getLTM());
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
				Matrix::mult(m, &invMats[i], &tmp);
				m++;
			}
		}
	}else{
		for(i = 0; i < skin->numBones; i++){
			m->setIdentity();
			m++;
		}
	}

	// setUniform(u_boneMatrices, 64*16, skinMatrices);
}

static void doSkin(V3d* res, V3d* src1, Matrix* src2)
{
	res->x = src1->x*src2->right.x + src1->y*src2->up.x + src1->z*src2->at.x + src2->pos.x;
	res->y = src1->x*src2->right.y + src1->y*src2->up.y + src1->z*src2->at.y + src2->pos.y;
	res->z = src1->x*src2->right.z + src1->y*src2->up.z + src1->z*src2->at.z + src2->pos.z;
}

void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Material *m;

	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	uploadSkinMatrices(atomic);

	/*
	This should go into the skin shader but there isn't enough uniform space for the skinMatrices in there.
	So we're just calculating the skin vertex on the CPU. */
	AttribDesc *posDesc, *weightDesc, *indexDesc;
	for(posDesc = header->attribDesc; posDesc->index != ATTRIB_POS; posDesc++);
	for(weightDesc = header->attribDesc; weightDesc->index != ATTRIB_WEIGHTS; weightDesc++);
	for(indexDesc = header->attribDesc; indexDesc->index != ATTRIB_INDICES; indexDesc++);

	uint8* verts = (uint8*) header->vertexBuffer;

	for (uint32 i = 0; i < header->totalNumVertex; i++) {
		V3d* dstPos = (V3d*) (verts + posDesc->offset + (i * posDesc->stride));

		V3d* pos = &header->skinPosData[i];
		float32* weights = (float32*) (verts + weightDesc->offset + (i * weightDesc->stride));
		float32* indices = (float32*) (verts + indexDesc->offset + (i * indexDesc->stride));

		V3d SkinVertex = {0.0f, 0.0f, 0.0f};
		for (int j = 0; j < 4; j++) {
		 	V3d temp;
		 	doSkin(&temp, pos, &((Matrix*)skinMatrices)[(int)indices[j]]);
		 	SkinVertex.x += temp.x * weights[j];
		 	SkinVertex.y += temp.y * weights[j];
		 	SkinVertex.z += temp.z * weights[j];
		}

		dstPos->x = SkinVertex.x;
		dstPos->y = SkinVertex.y;
		dstPos->z = SkinVertex.z;
	}

	uint32 stride = header->attribDesc[0].stride;
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, verts, header->totalNumVertex * stride);
	GX2SetAttribBuffer(0, header->totalNumVertex*stride, stride, verts);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	skinShader->use();

	while(n--){
		m = inst->material;

		setMaterial(m->color, m->surfaceProps);

		setTexture(0, m->texture);

		rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

		drawInst(header, inst);
		inst++;
	}
}

static void*
skinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_GX2] = makeSkinPipeline();

	skinShader = Shader::create(skin_gsh, GX2_SHADER_MODE_UNIFORM_REGISTER);
	skinShader->initAttribute("in_pos", 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	skinShader->initAttribute("in_normal", 12, GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
	skinShader->initAttribute("in_color", 24, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	skinShader->initAttribute("in_tex0", 40, GX2_ATTRIB_FORMAT_FLOAT_32_32);
	skinShader->initAttribute("in_weights", 48, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
	skinShader->initAttribute("in_indices", 64, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);

	skinShader->init();
	skinShader->use();

	skinShader->samplerLocation = GX2GetPixelSamplerVarLocation(skinShader->group.pixelShader, "tex0");

	return o;
}

static void*
skinClose(void *o, int32, int32)
{
	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_GX2])->destroy();
	skinGlobals.pipelines[PLATFORM_GX2] = nil;

	skinShader->destroy();
	skinShader = nil;

	return o;
}

void
initSkin(void)
{
	u_boneMatrices = registerUniform("u_boneMatrices");

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