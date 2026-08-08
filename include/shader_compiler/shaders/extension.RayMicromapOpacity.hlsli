R"(
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

//RayMicromapOpacity (SM6.9 opacity micromaps). The OMM behaviour is the ray flag ForceOpacityMicromap2StateEXT
//(0x400), which both backends emit from the RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, ...> template. That flag's enabling
//capability is RayTracingOpacityMicromapEXT, which DXC's SPIR-V backend does NOT declare, so spirv-val rejects the
//module. oxc::EnableOpacityMicromap() only injects that missing capability + extension via a no-op carrier - it does
//not touch the ray flag itself. Call it once in an OMM shader; on DXIL it compiles to nothing.

namespace oxc {

	#ifdef __spirv__

		[[vk::ext_capability(/* RayTracingOpacityMicromapEXT */ 5381)]]
		[[vk::ext_extension("SPV_EXT_opacity_micromap")]]
		[[vk::ext_instruction(/* OpNop */ 0)]]
		void __declareOpacityMicromapCapability();

		void EnableOpacityMicromap() { __declareOpacityMicromapCapability(); }

	#else

		void EnableOpacityMicromap() {}

	#endif

}
)"
