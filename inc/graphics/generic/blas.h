/* OxC3(Oxsomi core 3), a general framework and toolset for cross platform applications.
*  Copyright (C) 2023 Oxsomi / Nielsbishere (Niels Brunekreef)
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

#pragma once
#include "acceleration_structure.h"

typedef enum EBLASInstanceFlag {
	EBLASInstanceFlag_AvoidDuplicateAnyHit	= 1 << 0,		//Don't run the same anyHit twice on the same triangle/AABB
	EBLASInstanceFlag_DisableAnyHit			= 1 << 1,		//Force anyHit off for the geometry's triangles/AABBs
	EBLASInstanceFlag_Count					= 2
} EBLASInstanceFlag;

typedef enum EBLASConstructionType {
	EBLASConstructionType_Geometry,			//Triangles
	EBLASConstructionType_Procedural,		//AABBs
	EBLASConstructionType_Serialized,
	EBLASConstructionType_Count
} EBLASConstructionType;

typedef struct BLAS {

	RTAS base;

	union {
		DeviceData deviceData;				//Only if EBLASBuildFlags_UseDeviceMemory
		Buffer cpuData;						//Contains CPU-sided data required to make BLAS
	};

} BLAS;

typedef RefPtr BLASRef;

#define BLAS_ext(ptr, T) (!ptr ? NULL : (T##BLAS*)(ptr + 1))		//impl
#define BLASRef_ptr(ptr) RefPtr_data(ptr, BLAS)

Error BLASRef_dec(BLASRef **blas);
Error BLASRef_inc(BLASRef *blas);
