/* OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
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

//This file has to be included in the main entrypoint to support all required graphics features.
//#include "graphics/generic/application.h" has to be present
//Otherwise it will produce a linker error

#include "instance.h"

#if _GRAPHICS_API == GRAPHICS_API_D3D12

	#define UNICODE
	#define WIN32_LEAN_AND_MEAN
	#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
	#include "d3d12.h"
	__declspec(dllexport) extern const uint32_t D3D12SDKVersion = D3D12_SDK_VERSION;
	__declspec(dllexport) extern const char *D3D12SDKPath = "./D3D12/";

	const U32 GraphicsInstance_verificationVersion = 1;

#else
	const U32 GraphicsInstance_verificationVersion = 1;
#endif
