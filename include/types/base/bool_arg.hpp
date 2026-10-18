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

//types/base/bool_arg.hpp

#pragma once

namespace oxc {

	//The type every Bool parameter of the C++ wrappers takes.
	//Only a bool converts to it; a pointer, integer, enum, float or class argument fails to compile instead of
	// silently becoming a bool.
	//What it guards is a left out Bool directly before e_rr:
	// f(..., e_rr) would otherwise bind the Error* to the Bool, flip the flag to true and drop the error channel.
	//Call sites that pass true, false or a Bool are unchanged; an integer or flag test needs != 0 or !! first.

	class BoolArg {

		bool value;

	public:

		constexpr BoolArg(bool v) noexcept : value(v) {}

		//Deduces an exact match for every other argument type, so it wins over the conversion to bool above,
		// and being deleted that makes the call ill-formed.

		template<typename T>
		BoolArg(T) = delete;

		constexpr operator bool() const noexcept { return value; }
	};
}
