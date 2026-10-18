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

//RayMicromapOpacity (SM6.9 opacity micromaps).
//The OMM behavior is the ray flag ForceOpacityMicromap2StateKHR (0x400), which both backends emit from
// RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> and from TraceRay's flags.
//DXC's SPIR-V backend declares neither what that flag needs nor what Vulkan needs around it, so this declares them.
//On DXIL both forms below compile to nothing, since DXIL needs neither.
//
//OXC_ENABLE_OPACITY_MICROMAP() goes at the top of the BODY of every entry point that traces against micromap
// geometry through a ray query, whatever its stage: raygen and the other raytracing stages, compute and graphics.
//VK_KHR_opacity_micromap only lets a ray query reach micromaps from an entry point that carries the
// OpacityMicromapIdKHR execution mode with a true operand; without it a driver may silently ignore the micromaps.
//It is a macro because the mode lands on the entry point whose body names it:
// called from a helper function, DXC attaches it to whichever entry point of a library it picks, not the caller.
//
//oxc::EnableOpacityMicromap() only declares the capabilities and extension (RayTracingOpacityMicromapKHR,
// RayTracingOpacityMicromapExecutionModeKHR, SPV_KHR_opacity_micromap; only the KHR extension, since the device
// enables VK_KHR_opacity_micromap and never the EXT original), which spirv-val requires for the flag.
//That is all a shader needs whose micromap traces all go through a raytracing pipeline's TraceRay:
// the pipeline flag AllowOpacityMicromapExt covers those, so no entry point needs the mode.
//It can be called once from anywhere in the module, a helper included; the macro calls it too.

namespace oxc {

	#ifdef __spirv__

		[[vk::ext_capability(/* RayTracingOpacityMicromapKHR */ 5381)]]
		[[vk::ext_capability(/* RayTracingOpacityMicromapExecutionModeKHR */ 6032)]]
		[[vk::ext_extension("SPV_KHR_opacity_micromap")]]
		[[vk::ext_instruction(/* OpNop */ 0)]]
		void __declareOpacityMicromapCapability();

		void EnableOpacityMicromap() { __declareOpacityMicromapCapability(); }

	#else

		void EnableOpacityMicromap() {}

	#endif

}

#ifdef __spirv__
	#define OXC_ENABLE_OPACITY_MICROMAP()                                           \
		oxc::EnableOpacityMicromap();                                               \
		vk::ext_execution_mode_id(/* OpacityMicromapIdKHR */ 6031, true)
#else
	#define OXC_ENABLE_OPACITY_MICROMAP()
#endif
