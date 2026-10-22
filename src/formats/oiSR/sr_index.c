/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
*  Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
*  Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
*  To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
*  This is called dual licensing.
*/

//formats/oiSR/sr_index.c

#include "formats/oiSR/sr_file.h"
#include "types/container/list_basic_types.h"
#include <stddef.h>

//A node's satellites (its type, its register, its enum value, the interfaces it implements) are stored as
//flat lists keyed by node, and its children are found by their parent id. Answering "what belongs to this
//node" by scanning is quadratic in the node count, which is what makes emitting a whole tree the slow part
//of writing a document.
//This turns those scans into one pass, so a walk over every node costs the node count rather than its square.

//Grouped lists are built the same way: count per node, turn the counts into offsets, then fill.
//The counting pass writes into the offsets list itself, so nothing else has to be allocated.

static Bool SRNodeIndex_group(
	const U32 *keys,
	U64 keyStride,                //In bytes, a multiple of 4: keys are read as U32 (static asserted at the call sites)
	U64 keyCount,
	U64 nodeCount,
	ListU32 *offsets,
	ListU32 *values,
	const Allocator *alloc,
	Error *e_rr
) {

	Bool s_uccess = true;

	gotoIfError3(clean, ListU32_resize(offsets, nodeCount + 1, alloc, e_rr));

	for (U64 i = 0; i < keyCount; ++i) {

		const U32 node = *(const U32*)((const U8*)keys + i * keyStride);

		if(node < nodeCount)
			++offsets->ptrNonConst[node + 1];
	}

	for (U64 i = 0; i < nodeCount; ++i)
		offsets->ptrNonConst[i + 1] += offsets->ptr[i];

	//Only in range keys were counted, so this is the exact total: a root's out of range parent never
	//leaves a tail of unwritten slots behind the real children

	gotoIfError3(clean, ListU32_resize(values, offsets->ptr[nodeCount], alloc, e_rr));

	//Filled in key order, so a node's values come out in the order the file stores them

	for (U64 i = 0; i < keyCount; ++i) {

		const U32 node = *(const U32*)((const U8*)keys + i * keyStride);

		if(node >= nodeCount)
			continue;

		values->ptrNonConst[offsets->ptr[node]] = (U32) i;
		++offsets->ptrNonConst[node];
	}

	//The fill walked every offset forward by that node's count, so each one now holds the next node's start.
	//Shifting them back restores the starts, which is cheaper than keeping a second cursor list.

	for (U64 i = nodeCount; i > 0; --i)
		offsets->ptrNonConst[i] = offsets->ptr[i - 1];

	offsets->ptrNonConst[0] = 0;

clean:
	return s_uccess;
}

Bool SRFile_createNodeIndex(const SRFile *srFile, const Allocator *alloc, SRNodeIndex *index, Error *e_rr) {

	Bool s_uccess = true;

	//A refusal leaves the caller's index exactly as it was; only this call's own half built work is freed

	if(!srFile || !index)
		retError(cleanPreserve, Error_nullPointer(!srFile ? 0 : 2, "SRFile_createNodeIndex()::srFile and index are required"));

	if(index->typeAt.ptr)
		retError(cleanPreserve, Error_invalidState(0, "SRFile_createNodeIndex()::index is already allocated"));

	//The group walks read a U32 out of each record through a byte stride

	_Static_assert(
		!(sizeof(SRNode) % sizeof(U32)) && !(offsetof(SRNode, parent) % sizeof(U32)),
		"SRNode's parent must be U32 aligned for the node index"
	);

	_Static_assert(
		!(sizeof(SRInterface) % sizeof(U32)) && !(offsetof(SRInterface, nodeId) % sizeof(U32)),
		"SRInterface's nodeId must be U32 aligned for the node index"
	);

	const U64 nodes = srFile->nodes.length;

	gotoIfError3(clean, ListU32_resize(&index->typeAt, nodes, alloc, e_rr));
	gotoIfError3(clean, ListU32_resize(&index->registerAt, nodes, alloc, e_rr));
	gotoIfError3(clean, ListU32_resize(&index->enumValueAt, nodes, alloc, e_rr));

	for (U64 i = 0; i < nodes; ++i) {
		index->typeAt.ptrNonConst[i] = U32_MAX;
		index->registerAt.ptrNonConst[i] = U32_MAX;
		index->enumValueAt.ptrNonConst[i] = U32_MAX;
	}

	//The first record wins, which is what a scan that stops at its first match does

	for (U64 i = 0; i < srFile->types.length; ++i) {

		const U32 node = srFile->types.ptr[i].nodeId;

		if(node < nodes && index->typeAt.ptr[node] == U32_MAX)
			index->typeAt.ptrNonConst[node] = (U32) i;
	}

	for (U64 i = 0; i < srFile->registers.length; ++i) {

		const U32 node = srFile->registers.ptr[i].nodeId;

		if(node < nodes && index->registerAt.ptr[node] == U32_MAX)
			index->registerAt.ptrNonConst[node] = (U32) i;
	}

	for (U64 i = 0; i < srFile->enumValues.length; ++i) {

		const U32 node = srFile->enumValues.ptr[i].nodeId;

		if(node < nodes && index->enumValueAt.ptr[node] == U32_MAX)
			index->enumValueAt.ptrNonConst[node] = (U32) i;
	}

	//Children are grouped by the parent each node names, and a root's parent is out of range on purpose

	gotoIfError3(clean, SRNodeIndex_group(
		nodes ? &srFile->nodes.ptr[0].parent : NULL, sizeof(SRNode), nodes, nodes,
		&index->childOffset, &index->children, alloc, e_rr
	));

	gotoIfError3(clean, SRNodeIndex_group(
		srFile->interfaces.length ? &srFile->interfaces.ptr[0].nodeId : NULL, sizeof(SRInterface),
		srFile->interfaces.length, nodes,
		&index->interfaceOffset, &index->interfaces, alloc, e_rr
	));

clean:

	if(!s_uccess)
		SRNodeIndex_free(index, alloc);

cleanPreserve:
	return s_uccess;
}

void SRNodeIndex_free(SRNodeIndex *index, const Allocator *alloc) {

	if(!index)
		return;

	ListU32_free(&index->typeAt, alloc);
	ListU32_free(&index->registerAt, alloc);
	ListU32_free(&index->enumValueAt, alloc);
	ListU32_free(&index->childOffset, alloc);
	ListU32_free(&index->children, alloc);
	ListU32_free(&index->interfaceOffset, alloc);
	ListU32_free(&index->interfaces, alloc);
}
