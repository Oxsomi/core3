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

//formats/oiSR/sr_read.c

#include "formats/oiSR/sr_file.h"
#include "formats/oiDL/dl_file.h"
#include "types/container/buffer.h"
#include "types/container/ref_ptr.h"
#include "types/container/container_types.h"
#include "types/container/stream.h"
#include "types/base/allocator.h"
#include "types/base/error.h"

Bool SRFile_read(StreamRef *streamRef, U64 *offset, Bool isSubFile, const Allocator *alloc, SRFile *srFile, Error *e_rr) {

	Bool s_uccess = true;
	Bool didAllocate = false;
	StreamCursor cursor = (StreamCursor) { 0 };

	//Read into locals first so a validation failure can't leave a half-built srFile

	ListSRNode nodes = (ListSRNode) { 0 };
	ListSRSymbol symbols = (ListSRSymbol) { 0 };
	ListSRAnnotation annotations = (ListSRAnnotation) { 0 };
	ListSRRegister registers = (ListSRRegister) { 0 };
	ListSREnumValue enumValues = (ListSREnumValue) { 0 };
	ListSRType types = (ListSRType) { 0 };
	ListU32 arrayDims = (ListU32) { 0 };
	ListSRInterface interfaces = (ListSRInterface) { 0 };
	DLFile names = (DLFile) { 0 };

	if(!offset || !srFile)
		retError(clean, Error_nullPointer(!offset ? 1 : 4, "SRFile_read()::srFile and offset are required"));

	if(srFile->nodes.length || srFile->names.entryStrings.length)
		retError(clean, Error_invalidParameter(4, 0, "SRFile_read()::srFile is already present, possible memleak"));

	if(*offset & 15)
		retError(clean, Error_unsupportedOperation(0, "SRFile_read() at misaligned offset is unsupported (16-byte)"));

	gotoIfError3(clean, StreamCursor_create(streamRef, 0, false, alloc, &cursor, e_rr));

	OxStream *stream = RefPtr_data(streamRef, OxStream);

	if(!isSubFile) {

		U32 magic = 0;
		gotoIfError3(clean, StreamCursor_consumeU32(&cursor, offset, &magic, alloc, e_rr));

		if(magic != SRHeader_MAGIC)
			retError(clean, Error_invalidState(0, "SRFile_read()::file didn't start with oiSR"));
	}

	SRHeader header;
	gotoIfError3(clean, StreamCursor_consume(&cursor, offset, &header, sizeof(header), alloc, e_rr));

	if(header.version != ESRVersion_V1_1 || (header.flags & ESRFlag_Unsupported))
		retError(clean, Error_invalidState(0, "SRFile_read()::file didn't have the right version or flags"));

	if(header.features & ~(U32)ESRFeature_All & ~(U32)ESRFeature_SymbolInfo)
		retError(clean, Error_invalidState(0, "SRFile_read()::header.features contained unsupported bits"));

	Bool hasSymbols = header.flags & ESRFlag_HasSymbols;

	//The SymbolInfo feature and the HasSymbols flag must agree

	if((Bool)(header.features & ESRFeature_SymbolInfo) != hasSymbols)
		retError(clean, Error_invalidState(0, "SRFile_read()::header symbol feature and flag disagree"));

	//Consume the fixed-size POD arrays

	gotoIfError3(clean, ListSRNode_resize(&nodes, header.nodeCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRNode_buffer(nodes), alloc, e_rr));

	if(hasSymbols) {
		gotoIfError3(clean, ListSRSymbol_resize(&symbols, header.nodeCount, alloc, e_rr));
		gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRSymbol_buffer(symbols), alloc, e_rr));
	}

	gotoIfError3(clean, ListSRAnnotation_resize(&annotations, header.annotationCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRAnnotation_buffer(annotations), alloc, e_rr));

	gotoIfError3(clean, ListSRRegister_resize(&registers, header.registerCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRRegister_buffer(registers), alloc, e_rr));

	gotoIfError3(clean, ListSREnumValue_resize(&enumValues, header.enumValueCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSREnumValue_buffer(enumValues), alloc, e_rr));

	gotoIfError3(clean, ListSRType_resize(&types, header.typeCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRType_buffer(types), alloc, e_rr));

	gotoIfError3(clean, ListU32_resize(&arrayDims, header.arrayDimCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListU32_buffer(arrayDims), alloc, e_rr));

	gotoIfError3(clean, ListSRInterface_resize(&interfaces, header.interfaceCount, alloc, e_rr));
	gotoIfError3(clean, StreamCursor_consumeBuffer(&cursor, offset, ListSRInterface_buffer(interfaces), alloc, e_rr));

	//Align 16-byte then read the names oiDL.
	//Consume the pad bytes through the cursor rather than only advancing the offset, so a forward-only stream (file data
	// streaming straight off disk) moves past the padding too; otherwise DLFile_read would start reading inside the pad
	// and reject the names as a bad DLFile header.

	U64 aligned = (*offset + 15) & ~15;

	if(aligned != *offset) {
		U8 padDiscard[16];
		gotoIfError3(clean, StreamCursor_consume(&cursor, offset, padDiscard, aligned - *offset, alloc, e_rr));
	}

	gotoIfError3(clean, DLFile_read(streamRef, offset, NULL, I32x4_zero(), true, false, alloc, NULL, &names, e_rr));

	if(
		names.settings.dataType != EDLDataType_String ||
		names.settings.encryptionType ||
		names.settings.compressionType
	)
		retError(clean, Error_invalidParameter(0, 1, "SRFile_read() names didn't match expectations"));

	U64 nameCount = names.entryStrings.length;

	for(U64 i = 0; i < nameCount; ++i) {

		if(!DLFile_isFullyLoaded(&names, i))
			retError(clean, Error_invalidParameter(0, 1, "SRFile_read() one of the strings wasn't fully loaded"));

		//Cap string length, matching the producer's 32767 limit (and the oiSB/oiSH house style)

		if(CharString_length(names.entryStrings.ptr[i]) >= 32768)
			retError(clean, Error_invalidParameter(0, 1, "SRFile_read() one of the strings exceeded the length limit"));
	}

	//Validate node references

	for(U64 i = 0; i < nodes.length; ++i) {

		SRNode node = nodes.ptr[i];

		if(node.type >= ESRNodeType_Count)
			retError(clean, Error_invalidState(0, "SRFile_read() node had invalid type"));

		if(node.interpolation >= ESRInterpolation_Count)
			retError(clean, Error_invalidState(0, "SRFile_read() node had invalid interpolation"));

		if(node.flags & ESRNodeFlag_Invalid)
			retError(clean, Error_invalidState(0, "SRFile_read() node had invalid flags"));

		if(node.nameId != U32_MAX && node.nameId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() node.nameId out of bounds"));

		if(node.semanticId != U32_MAX && node.semanticId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() node.semanticId out of bounds"));

		//Parent must strictly precede the node (the producer emits parents before children).
		//This guarantees an acyclic, topologically ordered tree (node 0's only legal parent is the root),
		// which every parent-walk (SRFile_print, go-to-definition) relies on to terminate.

		if(node.parent != U32_MAX && node.parent >= i)
			retError(clean, Error_invalidState(0, "SRFile_read() node.parent must reference an earlier node (or be root)"));

		//Direct children are emitted contiguously, so the child block must fit within the node array.

		if((U64)i + node.childCount > nodes.length)
			retError(clean, Error_invalidState(0, "SRFile_read() node.childCount out of bounds"));

		if(node.fwdBckDeclareNode != U32_MAX && node.fwdBckDeclareNode >= nodes.length)
			retError(clean, Error_invalidState(0, "SRFile_read() node.fwdBckDeclareNode out of bounds"));

		//The forward-declaration flag is only meaningful on declarable symbols (matches the producer's allowFwdDeclare).

		if(
			(node.flags & ESRNodeFlag_IsFwdDeclare) &&
			node.type != ESRNodeType_Function && node.type != ESRNodeType_Enum &&
			node.type != ESRNodeType_Struct && node.type != ESRNodeType_Union && node.type != ESRNodeType_Interface
		)
			retError(clean, Error_invalidState(0, "SRFile_read() forward-declare flag on a non-declarable node type"));

		if((node.flags & ESRNodeFlag_HasReturn) && node.type != ESRNodeType_Function)
			retError(clean, Error_invalidState(0, "SRFile_read() HasReturn flag on a non-function node"));

		if((node.flags & ESRNodeFlag_ParamReturn) && node.type != ESRNodeType_Parameter)
			retError(clean, Error_invalidState(0, "SRFile_read() return-value flag on a non-parameter node"));

		if((node.flags & (ESRNodeFlag_ParamIn | ESRNodeFlag_ParamOut)) && node.type != ESRNodeType_Parameter)
			retError(clean, Error_invalidState(0, "SRFile_read() parameter-direction flag on a non-parameter node"));

		if(node.annotationCount) {

			if(node.annotationStart == U32_MAX || (U64)node.annotationStart + node.annotationCount > annotations.length)
				retError(clean, Error_invalidState(0, "SRFile_read() node annotation range out of bounds"));
		}
	}

	//Validate forward/backward declaration links (a forward declaration and its definition point at each other).
	//go-to-definition follows this link, so a self-loop or a wrong-direction / wrong-target link must be rejected.

	for(U64 i = 0; i < nodes.length; ++i) {

		SRNode node = nodes.ptr[i];
		U32 j = node.fwdBckDeclareNode;

		if(j == U32_MAX)
			continue;

		if(j == i)
			retError(clean, Error_invalidState(0, "SRFile_read() node links to itself as a forward/backward declaration"));

		SRNode other = nodes.ptr[j];

		if(other.type != node.type)
			retError(clean, Error_invalidState(0, "SRFile_read() forward/backward declaration type mismatch"));

		if(other.nameId != node.nameId)        //Both are U32_MAX when symbols are absent, so this still holds
			retError(clean, Error_invalidState(0, "SRFile_read() forward/backward declaration name mismatch"));

		Bool isFwd = (node.flags & ESRNodeFlag_IsFwdDeclare) != 0;
		Bool otherIsFwd = (other.flags & ESRNodeFlag_IsFwdDeclare) != 0;

		if(isFwd) {

			//A forward declaration points forward to its (non-forward) definition, carries no annotations, and
			// (unless it's a function with parameter children) has no children.

			if(j <= i || otherIsFwd)
				retError(clean, Error_invalidState(
					0, "SRFile_read() forward declaration must point forward to its definition"
				));

			if(node.annotationCount)
				retError(clean, Error_invalidState(0, "SRFile_read() forward declaration must not carry annotations"));

			if(node.type != ESRNodeType_Function && node.childCount)
				retError(clean, Error_invalidState(0, "SRFile_read() non-function forward declaration must have no children"));
		}

		//A definition's back-link points backward to its forward declaration

		else if(j >= i || !otherIsFwd)
			retError(clean, Error_invalidState(
				0, "SRFile_read() definition back-link must point to an earlier forward declaration"
			));
	}

	//Validate symbol references

	for(U64 i = 0; i < symbols.length; ++i)
		if(symbols.ptr[i].fileNameId != U32_MAX && symbols.ptr[i].fileNameId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() symbol.fileNameId out of bounds"));

	//Validate annotation references

	for(U64 i = 0; i < annotations.length; ++i)
		if(annotations.ptr[i].nameId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() annotation.nameId out of bounds"));

	//Validate register references (must point at a Register node, with in-range enum fields)

	for(U64 i = 0; i < registers.length; ++i) {

		SRRegister reg = registers.ptr[i];

		if(reg.nodeId >= nodes.length || nodes.ptr[reg.nodeId].type != ESRNodeType_Register)
			retError(clean, Error_invalidState(0, "SRFile_read() register.nodeId doesn't reference a Register node"));

		if(
			reg.type >= ESRResourceType_Count ||
			reg.dimension >= ESRResourceDimension_Count ||
			reg.returnType >= ESRResourceReturnType_Count
		)
			retError(clean, Error_invalidState(0, "SRFile_read() register had an invalid type/dimension/returnType"));

		if((reg.arrayDimStart != U32_MAX) != (reg.arrayDimCount >= 2))
			retError(clean, Error_invalidState(0, "SRFile_read() register arrayDim start/count disagree"));

		if(reg.arrayDimStart != U32_MAX && (U64) reg.arrayDimStart + reg.arrayDimCount > arrayDims.length)
			retError(clean, Error_invalidState(0, "SRFile_read() register.arrayDim range out of bounds"));
	}

	//Validate enum value references (must point at an EnumValue node, with an in-range underlying type)

	for(U64 i = 0; i < enumValues.length; ++i) {

		SREnumValue ev = enumValues.ptr[i];

		if(ev.nodeId >= nodes.length || nodes.ptr[ev.nodeId].type != ESRNodeType_EnumValue)
			retError(clean, Error_invalidState(0, "SRFile_read() enumValue.nodeId doesn't reference an EnumValue node"));

		if(ev.enumType >= ESREnumType_Count)
			retError(clean, Error_invalidState(0, "SRFile_read() enumValue had an invalid enumType"));
	}

	//Types must reference a value node (variable-like) and carry a valid class + name reference

	for(U64 i = 0; i < types.length; ++i) {

		SRType ty = types.ptr[i];

		if(ty.nodeId >= nodes.length)
			retError(clean, Error_invalidState(0, "SRFile_read() type.nodeId out of bounds"));

		U8 nt = nodes.ptr[ty.nodeId].type;

		if(nt != ESRNodeType_Variable && nt != ESRNodeType_Parameter && nt != ESRNodeType_StaticVariable &&
			nt != ESRNodeType_GroupsharedVariable && nt != ESRNodeType_Typedef && nt != ESRNodeType_Struct &&
			nt != ESRNodeType_Union)
			retError(clean, Error_invalidState(0, "SRFile_read() type.nodeId doesn't reference a value node"));

		if(ty.typeClass >= ESRTypeClass_Count)
			retError(clean, Error_invalidState(0, "SRFile_read() type had an invalid typeClass"));

		if(ty.typeNameId != U32_MAX && ty.typeNameId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() type.typeNameId out of bounds"));

		if(ty.displayNameId != U32_MAX && ty.displayNameId >= nameCount)
			retError(clean, Error_invalidState(0, "SRFile_read() type.displayNameId out of bounds"));

		//Go-to-definition + base class must reference a Struct/Union node (or be absent)

		if(ty.defNodeId != U32_MAX && (ty.defNodeId >= nodes.length ||
			(nodes.ptr[ty.defNodeId].type != ESRNodeType_Struct && nodes.ptr[ty.defNodeId].type != ESRNodeType_Union)
		))
			retError(clean, Error_invalidState(0, "SRFile_read() type.defNodeId doesn't reference a Struct/Union node"));

		if(ty.baseNodeId != U32_MAX && (ty.baseNodeId >= nodes.length ||
			(nodes.ptr[ty.baseNodeId].type != ESRNodeType_Struct && nodes.ptr[ty.baseNodeId].type != ESRNodeType_Union)
		))
			retError(clean, Error_invalidState(0, "SRFile_read() type.baseNodeId doesn't reference a Struct/Union node"));

		if((ty.arrayDimStart != U32_MAX) != (ty.arrayDimCount >= 2))
			retError(clean, Error_invalidState(0, "SRFile_read() type arrayDim start/count disagree"));

		if(ty.arrayDimStart != U32_MAX && (U64) ty.arrayDimStart + ty.arrayDimCount > arrayDims.length)
			retError(clean, Error_invalidState(0, "SRFile_read() type.arrayDim range out of bounds"));
	}

	//Interface edges must go from a Struct/Union node to an Interface node

	for(U64 i = 0; i < interfaces.length; ++i) {

		SRInterface itf = interfaces.ptr[i];

		if(itf.nodeId >= nodes.length ||
			(nodes.ptr[itf.nodeId].type != ESRNodeType_Struct && nodes.ptr[itf.nodeId].type != ESRNodeType_Union))
			retError(clean, Error_invalidState(0, "SRFile_read() interface.nodeId doesn't reference a Struct/Union node"));

		if(itf.interfaceNodeId >= nodes.length || nodes.ptr[itf.interfaceNodeId].type != ESRNodeType_Interface)
			retError(clean, Error_invalidState(
				0, "SRFile_read() interface.interfaceNodeId doesn't reference an Interface node"
			));
	}

	if(!isSubFile && *offset != stream->size)
		retError(clean, Error_invalidState(0, "SRFile_read() file had unrecognized data at the end"));

	//Move everything into srFile

	ESRSettingsFlags flags =
		(isSubFile ? ESRSettingsFlags_HideMagicNumber : ESRSettingsFlags_None) |
		(hasSymbols ? ESRSettingsFlags_HasSymbols : ESRSettingsFlags_None);

	didAllocate = true;

	srFile->names = names;
	srFile->nodes = nodes;
	srFile->symbols = symbols;
	srFile->annotations = annotations;
	srFile->registers = registers;
	srFile->enumValues = enumValues;
	srFile->types = types;
	srFile->arrayDims = arrayDims;
	srFile->interfaces = interfaces;
	srFile->flags = flags;
	srFile->features = header.features;

	names = (DLFile) { 0 };
	nodes = (ListSRNode) { 0 };
	symbols = (ListSRSymbol) { 0 };
	annotations = (ListSRAnnotation) { 0 };
	registers = (ListSRRegister) { 0 };
	enumValues = (ListSREnumValue) { 0 };
	types = (ListSRType) { 0 };
	arrayDims = (ListU32) { 0 };
	interfaces = (ListSRInterface) { 0 };

	gotoIfError3(clean, SRFile_finalize(srFile, alloc, e_rr));

clean:

	if(didAllocate && !s_uccess)
		SRFile_free(srFile, alloc);

	DLFile_free(&names, alloc);
	ListSRNode_free(&nodes, alloc);
	ListSRSymbol_free(&symbols, alloc);
	ListSRAnnotation_free(&annotations, alloc);
	ListSRRegister_free(&registers, alloc);
	ListSREnumValue_free(&enumValues, alloc);
	ListSRType_free(&types, alloc);
	ListU32_free(&arrayDims, alloc);
	ListSRInterface_free(&interfaces, alloc);
	StreamCursor_close(&cursor, alloc);
	return s_uccess;
}
