# OxC3(Oxsomi core 3), a general framework and toolset for cross-platform applications.
# Copyright (C) 2023 - 2026 Oxsomi / Nielsbishere (Niels Brunekreef)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see https://github.com/Oxsomi/core3/blob/main/LICENSE.
# Be aware that GPL3 requires closed source products to be GPL3 too if released to the public.
# To prevent this a separate license will have to be requested at contact@osomi.net for a premium;
# This is called dual licensing.

# OxC3_wasm: the module the web frontend loads (web/, see its README).
#
# It is not a program: there is no main() and --no-entry says so, because every call arrives from the page
# after the module has instantiated. That is also why apply_web_link_options takes MODE browser, which drops
# NODERAWFS (a browser has no host filesystem to bind it to) and keeps the runtime alive past its entry point.
#
# MODULARIZE hands the page a factory rather than a global, so a page can hold more than one module and so
# nothing lands on window.
#
# The three runtime methods are all the page needs. FS is how it writes the project tree that the compiler's
# #include resolution reads back. wasmMemory is what every heap view is built from: memory growth replaces the
# buffer, so a view taken once goes stale, and taking it from the memory object per call cannot. HEAPU8 rides
# along because the same export list is what keeps the views refreshed at all.
#
# Nothing calls ccall: under memory64 a pointer crosses as a BigInt and ccall does not convert one, so the
# page marshals pointers itself (see web/js/wasm.js).

file(GLOB wasmHeaders CONFIGURE_DEPENDS "../../../include/tools/oxc3_wasm/*.h")
file(GLOB wasmSources CONFIGURE_DEPENDS "*.c")

add_executable(
	OxC3_wasm
	${wasmHeaders}
	${wasmSources}
	CMakeLists.txt
)

set_target_properties(OxC3_wasm PROPERTIES FOLDER Oxsomi/CLI)

target_link_libraries(
	OxC3_wasm PUBLIC
	OxC3_types_base OxC3_types_math OxC3_types_container
	OxC3_platforms
	OxC3_formats_oiSB OxC3_formats_oiSR OxC3_formats_oiSH OxC3_formats_oiSP
	OxC3_shader_compiler
)

include(../../../cmake/oxc3.cmake)

apply_web_link_options(OxC3_wasm MODE browser)

target_link_options(OxC3_wasm PRIVATE
	"--no-entry"
	"-sMODULARIZE=1"
	"-sEXPORT_NAME=createOxC3Module"
	"-sEXPORTED_RUNTIME_METHODS=FS,HEAPU8,wasmMemory"
)

# SINGLE_FILE embeds the wasm in the js, so instantiating it needs no second request.
# That is what lets web/index.html run from file://, where a fetch of a sibling .wasm is refused; see
# WebFrontendSingleFile in the root CMakeLists for which flavor is which.
# emcc emits no .wasm of its own in this mode, so nothing else may expect one.

if(WebFrontendSingleFile)
	target_link_options(OxC3_wasm PRIVATE "-sSINGLE_FILE=1")
endif()

apply_dependencies(OxC3_wasm)
