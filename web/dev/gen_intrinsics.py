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
#
# gen_intrinsics.py: generates js/intrinsics_data.js from the DXC fork's own sources.
#
# Nothing signature-shaped is written down here. The intrinsic file (utils/hct/gen_intrin_main.txt) is
# parsed by DXC's OWN loader, hctdb.py, the code that generates Sema's tables, so this can never grow a
# second grammar; the object vocabulary (which method table belongs to which objects, and what those
# objects are called) is read out of SemaHLSL.cpp: the ArBasicKind enum, its g_ArBasicTypeNames display
# strings and the GetIntrinsicMethods switch. Anything either source spells that this file does not
# recognize fails generation loudly, so a fork update cannot leave the page describing yesterday's
# compiler. The fork checkout is not part of this repository, so the output is committed, like
# js/mock_data.js, and build_web.py --run_frontend_tests regenerates it against the pinned fork commit
# (packages/dxc/conandata.yml) and fails when the committed copy differs:
#
#   python web/dev/gen_intrinsics.py <a DXC checkout, or the dxc conan package's res/dxc_intrinsics>
#                                    [-o out.js]
#
# The one thing that stays hand-written is prose: DXC carries no descriptions (every hctdb doc string
# is a "pending doc" placeholder), so DESC and METHOD_DESC below are editorial content keyed by name.
# A new intrinsic without one costs a warning and ships with its signature only, never a stale line.

import hashlib
import io
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ---- how a component family is respelled -----------------------------------------------------------

# Display spellings for DXC's component-type enum, the way HLSL documentation writes them: the meta
# families collapse (float_like reads float, any_int reads int) and the object-template types read T.
# Keyed by the LICOMPTYPE_* vocabulary hctdb itself declares, and checked COMPLETE against it in
# main(), so a base type added to the fork fails generation here instead of leaking through misspelled.
COMP_DISPLAY = {
	"LICOMPTYPE_BOOL": "bool", "LICOMPTYPE_INT": "int", "LICOMPTYPE_INT32_ONLY": "int",
	"LICOMPTYPE_INT64_ONLY": "int64_t", "LICOMPTYPE_INT16": "int16_t", "LICOMPTYPE_UINT": "uint",
	"LICOMPTYPE_UINT16": "uint16_t", "LICOMPTYPE_UINT64": "uint64_t", "LICOMPTYPE_ANY_INT": "int",
	"LICOMPTYPE_ANY_INT32": "int", "LICOMPTYPE_ANY_INT64": "int64_t", "LICOMPTYPE_UINT_ONLY": "uint",
	"LICOMPTYPE_ANY_INT16_OR_32": "int", "LICOMPTYPE_SINT16_OR_32_ONLY": "int",
	"LICOMPTYPE_INT8_4PACKED": "int8_t4_packed", "LICOMPTYPE_UINT8_4PACKED": "uint8_t4_packed",
	"LICOMPTYPE_FLOAT16": "float16_t", "LICOMPTYPE_FLOAT": "float", "LICOMPTYPE_FLOAT32_ONLY": "float",
	"LICOMPTYPE_FLOAT_DOUBLE": "float", "LICOMPTYPE_ANY_FLOAT": "float", "LICOMPTYPE_FLOAT_LIKE": "float",
	"LICOMPTYPE_DOUBLE": "double", "LICOMPTYPE_DOUBLE_ONLY": "double",
	"LICOMPTYPE_NUMERIC": "numeric", "LICOMPTYPE_NUMERIC16_ONLY": "numeric",
	"LICOMPTYPE_NUMERIC32": "numeric", "LICOMPTYPE_NUMERIC32_ONLY": "numeric", "LICOMPTYPE_ANY": "any",
	"LICOMPTYPE_SAMPLER": "SamplerState", "LICOMPTYPE_SAMPLERCMP": "SamplerComparisonState",
	"LICOMPTYPE_ANY_SAMPLER": "SamplerState",
	"LICOMPTYPE_RESOURCE": "resource", "LICOMPTYPE_RAYDESC": "RayDesc",
	"LICOMPTYPE_ACCELERATION_STRUCT": "RaytracingAccelerationStructure",
	"LICOMPTYPE_BUILTIN_TRIANGLE_POSITIONS": "BuiltInTrianglePositions",
	"LICOMPTYPE_USER_DEFINED_TYPE": "T", "LICOMPTYPE_VOID": "void", "LICOMPTYPE_STRING": "string",
	"LICOMPTYPE_TEXTURE2D": "Texture2D", "LICOMPTYPE_TEXTURE2DARRAY": "Texture2DArray",
	"LICOMPTYPE_WAVE": "wave", "LICOMPTYPE_BYTEADDRESSBUFFER": "ByteAddressBuffer",
	"LICOMPTYPE_RWBYTEADDRESSBUFFER": "RWByteAddressBuffer",
	"LICOMPTYPE_NODE_RECORD_OR_UAV": "NodeRecordOrUAV",
	"LICOMPTYPE_ANY_NODE_OUTPUT_RECORD": "AnyNodeOutputRecord",
	"LICOMPTYPE_GROUP_NODE_OUTPUT_RECORDS": "GroupNodeOutputRecords",
	"LICOMPTYPE_THREAD_NODE_OUTPUT_RECORDS": "ThreadNodeOutputRecords",
	"LICOMPTYPE_HIT_OBJECT": "dx::HitObject", "LICOMPTYPE_VK_BUFFER_POINTER": "vk::BufferPointer",
	"LICOMPTYPE_RAY_QUERY": "RayQuery", "LICOMPTYPE_LINALG_MATRIX": "LinAlgMatrix",
	"LICOMPTYPE_LINALG": "LinAlg",
}

# Legacy tex1D/tex2D/... take sampler1d-style combined samplers that shader model 6 cannot declare, so
# offering them would complete to code that cannot compile. Declarations using these are dropped whole;
# the completeness check counts them as covered.
LEGACY_SAMPLERS = { "LICOMPTYPE_SAMPLER1D", "LICOMPTYPE_SAMPLER2D", "LICOMPTYPE_SAMPLER3D", "LICOMPTYPE_SAMPLERCUBE" }

# ---- descriptions ----------------------------------------------------------------------------------

# One short sentence per name. Keys are the plain name for global intrinsics, ns::name for namespaced
# ones; methods have their own table because names repeat between the two worlds (TraceRay, RayFlags).

D_ATOMIC = "the overload with `original` also returns the value that was there before."

DESC = {
	"D3DCOLORtoUBYTE4": "Scales a 0..1 float4 color by 255 and swizzles it the way D3D9's UBYTE4 vertex format expects.",
	"GetRenderTargetSampleCount": "MSAA sample count of the current render target.",
	"GetRenderTargetSamplePosition": "Position of MSAA sample s of the current render target, relative to the pixel center.",
	"abort": "Terminates execution of the current draw or dispatch.",
	"abs": "Absolute value, per component.",
	"acos": "Arccosine in radians, per component.",
	"all": "True when every component is non-zero.",
	"AllMemoryBarrier": "Waits until every outstanding device and groupshared access of this thread has finished.",
	"AllMemoryBarrierWithGroupSync": "AllMemoryBarrier plus an execution sync of the whole thread group.",
	"any": "True when any component is non-zero.",
	"asdouble": "Reassembles a double from its low and high uint words, per component.",
	"asfloat": "Reinterprets the bits as float, per component.",
	"asfloat16": "Reinterprets the bits of 16-bit values as float16_t, per component.",
	"asin": "Arcsine in radians, per component.",
	"asint": "Reinterprets the bits as int, per component.",
	"asint16": "Reinterprets the bits of 16-bit values as int16_t, per component.",
	"asuint": "Reinterprets the bits as uint; the double overload splits into low and high words.",
	"asuint16": "Reinterprets the bits of 16-bit values as uint16_t, per component.",
	"atan": "Arctangent in radians, per component.",
	"atan2": "Quadrant-correct arctangent: atan2(y, x) is the angle of the vector (x, y), in radians.",
	"ceil": "Rounds up to an integer, per component.",
	"clamp": "Clamps x to [min, max], per component.",
	"clip": "Discards the pixel when any component of x is negative.",
	"cos": "Cosine of an angle in radians, per component.",
	"cosh": "Hyperbolic cosine, per component.",
	"countbits": "Number of set bits, per component.",
	"cross": "Cross product of two 3-component vectors.",
	"ddx": "Screen-space derivative with respect to x, from the pixel quad.",
	"ddx_coarse": "ddx computed once per quad: cheaper, one value for all four pixels.",
	"ddx_fine": "ddx computed per pixel pair within the quad.",
	"ddy": "Screen-space derivative with respect to y, from the pixel quad.",
	"ddy_coarse": "ddy computed once per quad: cheaper, one value for all four pixels.",
	"ddy_fine": "ddy computed per pixel pair within the quad.",
	"DebugBreak": "Breaks into the attached shader debugger when one is present.",
	"degrees": "Converts radians to degrees, per component.",
	"determinant": "Determinant of a square matrix.",
	"DeviceMemoryBarrier": "Waits until every outstanding device (UAV) access of this thread has finished.",
	"DeviceMemoryBarrierWithGroupSync": "DeviceMemoryBarrier plus an execution sync of the whole thread group.",
	"distance": "Euclidean distance between two points.",
	"dot": "Dot product of two vectors.",
	"dst": "The D3D distance-vector helper: (1, a.y * b.y, a.z, b.w).",
	"EvaluateAttributeAtSample": "Evaluates a vertex attribute at a given MSAA sample position.",
	"EvaluateAttributeCentroid": "Evaluates a vertex attribute at the pixel's centroid location.",
	"EvaluateAttributeSnapped": "Evaluates a vertex attribute at a pixel offset, given in 1/16th steps.",
	"exp": "e raised to x, per component.",
	"exp2": "2 raised to x, per component.",
	"f16tof32": "Unpacks the half in a uint's low 16 bits to float, per component.",
	"f32tof16": "Packs a float to half into a uint's low 16 bits, per component.",
	"faceforward": "-N * sign(dot(I, Ng)): flips a normal so it faces against the incident direction.",
	"firstbithigh": "Bit index of the most significant set bit, per component; 0xffffffff when none.",
	"firstbitlow": "Bit index of the least significant set bit, per component; 0xffffffff when none.",
	"floor": "Rounds down to an integer, per component.",
	"fma": "Fused a * b + c for doubles: a single rounding.",
	"fmod": "Floating-point remainder of a / b, with a's sign, per component.",
	"frac": "Fractional part, per component.",
	"frexp": "Splits x into mantissa and exponent so that x = m * 2^exp, per component.",
	"fwidth": "abs(ddx(x)) + abs(ddy(x)), per component.",
	"GetAttributeAtVertex": "Reads a nointerpolation attribute at one of the primitive's vertices.",
	"GetGroupWaveCount": "Waves per thread group.",
	"GetGroupWaveIndex": "This wave's index within its thread group.",
	"GroupMemoryBarrier": "Waits until every outstanding groupshared access of this thread has finished.",
	"GroupMemoryBarrierWithGroupSync": "GroupMemoryBarrier plus an execution sync of the whole thread group.",
	"InterlockedAdd": "Atomic add on a groupshared or UAV destination; " + D_ATOMIC,
	"InterlockedAnd": "Atomic bitwise and on a groupshared or UAV destination; " + D_ATOMIC,
	"InterlockedCompareExchange": "Atomic compare-and-swap: stores value when the destination equals compare; returns the prior value.",
	"InterlockedCompareExchangeFloatBitwise": "Atomic float compare-and-swap comparing raw bits (no NaN or signed-zero equality).",
	"InterlockedCompareStore": "Atomically stores value when the destination equals compare.",
	"InterlockedCompareStoreFloatBitwise": "Atomically stores a float when the destination's raw bits equal compare's.",
	"InterlockedExchange": "Atomic swap; returns the value that was there before.",
	"InterlockedMax": "Atomic max on a groupshared or UAV destination; " + D_ATOMIC,
	"InterlockedMin": "Atomic min on a groupshared or UAV destination; " + D_ATOMIC,
	"InterlockedOr": "Atomic bitwise or on a groupshared or UAV destination; " + D_ATOMIC,
	"InterlockedXor": "Atomic bitwise xor on a groupshared or UAV destination; " + D_ATOMIC,
	"isfinite": "True per component where x is neither infinite nor NaN.",
	"isinf": "True per component where x is +/- infinity.",
	"isnan": "True per component where x is NaN.",
	"isnormal": "True per component where x is a normal float (not zero, denormal, infinite or NaN).",
	"IsHelperLane": "True on a pixel-quad helper lane that only runs to provide derivatives.",
	"ldexp": "x * 2^exp, per component.",
	"length": "Euclidean length of a vector.",
	"lerp": "a + s * (b - a), per component.",
	"lit": "Legacy lighting helper: (1, diffuse, specular, 1) from N.L, N.H and the specular power.",
	"log": "Natural logarithm, per component.",
	"log10": "Base-10 logarithm, per component.",
	"log2": "Base-2 logarithm, per component.",
	"mad": "a * b + c, per component; the compiler may fuse it.",
	"max": "The larger of a and b, per component.",
	"min": "The smaller of a and b, per component.",
	"modf": "Splits x into integer and fractional parts, both carrying x's sign.",
	"msad4": "Masked sum of absolute differences over byte quads, accumulated: the video motion-estimation primitive.",
	"mul": "Algebraic multiply for every scalar, vector and matrix pairing; vector * matrix treats the vector as a row.",
	"normalize": "x / length(x).",
	"pow": "x raised to y, per component.",
	"printf": "Formatted debug print, visible to tools that capture shader printf output.",
	"Process2DQuadTessFactorsAvg": "Rounds raw 2D-quad-patch tess factors, averaging to derive the inside factors.",
	"Process2DQuadTessFactorsMax": "Rounds raw 2D-quad-patch tess factors, taking the max for the inside factors.",
	"Process2DQuadTessFactorsMin": "Rounds raw 2D-quad-patch tess factors, taking the min for the inside factors.",
	"ProcessIsolineTessFactors": "Rounds raw isoline detail and density tess factors.",
	"ProcessQuadTessFactorsAvg": "Rounds raw quad-patch tess factors, averaging to derive the inside factors.",
	"ProcessQuadTessFactorsMax": "Rounds raw quad-patch tess factors, taking the max for the inside factors.",
	"ProcessQuadTessFactorsMin": "Rounds raw quad-patch tess factors, taking the min for the inside factors.",
	"ProcessTriTessFactorsAvg": "Rounds raw tri-patch tess factors, averaging to derive the inside factor.",
	"ProcessTriTessFactorsMax": "Rounds raw tri-patch tess factors, taking the max for the inside factor.",
	"ProcessTriTessFactorsMin": "Rounds raw tri-patch tess factors, taking the min for the inside factor.",
	"radians": "Converts degrees to radians, per component.",
	"rcp": "Fast approximate reciprocal, per component.",
	"reflect": "i - 2 * dot(i, n) * n: reflects a direction over a normal.",
	"refract": "Refracts direction i through a surface with normal n at index ratio ri; zero on total internal reflection.",
	"reversebits": "Reverses the bit order, per component.",
	"round": "Rounds to the nearest integer (half away from zero), per component.",
	"rsqrt": "1 / sqrt(x), per component.",
	"saturate": "Clamps to [0, 1], per component.",
	"sign": "-1, 0 or 1 per component, as int.",
	"sin": "Sine of an angle in radians, per component.",
	"sincos": "Sine and cosine of x in one call.",
	"sinh": "Hyperbolic sine, per component.",
	"smoothstep": "0 at a, 1 at b, smooth Hermite blend in between, per component.",
	"sqrt": "Square root, per component.",
	"step": "1 where x >= a, else 0, per component.",
	"tan": "Tangent of an angle in radians, per component.",
	"tanh": "Hyperbolic tangent, per component.",
	"transpose": "Transposed matrix.",
	"trunc": "Drops the fraction (rounds toward zero), per component.",
	"CheckAccessFullyMapped": "True when a Load/Sample status value says every byte came from mapped (resident) memory.",
	"AddUint64": "Treats a uint2/uint4 as one/two 64-bit values and adds with carry between the halves.",
	"NonUniformResourceIndex": "Marks a resource index as divergent, so the hardware must not assume it is wave-uniform.",
	"WaveIsFirstLane": "True on the first active lane of the wave.",
	"WaveGetLaneIndex": "This lane's index within its wave.",
	"WaveGetLaneCount": "Lanes per wave on this hardware.",
	"WaveActiveAnyTrue": "True when cond is true on any active lane.",
	"WaveActiveAllTrue": "True when cond is true on every active lane.",
	"WaveActiveAllEqual": "True, per component, where the value matches across every active lane.",
	"WaveActiveBallot": "Bitmask of the active lanes where cond is true, as uint4.",
	"WaveReadLaneAt": "Broadcasts value from the given lane.",
	"WaveReadLaneFirst": "Broadcasts value from the first active lane.",
	"WaveActiveCountBits": "How many active lanes pass true.",
	"WaveActiveSum": "Sum of value across all active lanes.",
	"WaveActiveProduct": "Product of value across all active lanes.",
	"WaveActiveBitAnd": "Bitwise and of value across all active lanes.",
	"WaveActiveBitOr": "Bitwise or of value across all active lanes.",
	"WaveActiveBitXor": "Bitwise xor of value across all active lanes.",
	"WaveActiveMin": "Minimum of value across all active lanes.",
	"WaveActiveMax": "Maximum of value across all active lanes.",
	"WavePrefixCountBits": "How many active lanes before this one pass true (exclusive).",
	"WavePrefixSum": "Exclusive prefix sum over the active lanes before this one.",
	"WavePrefixProduct": "Exclusive prefix product over the active lanes before this one.",
	"WaveMatch": "Bitmask of the active lanes holding the same value as this one, as uint4.",
	"WaveMultiPrefixBitAnd": "Prefix bitwise and within the lane cluster the mask describes.",
	"WaveMultiPrefixBitOr": "Prefix bitwise or within the lane cluster the mask describes.",
	"WaveMultiPrefixBitXor": "Prefix bitwise xor within the lane cluster the mask describes.",
	"WaveMultiPrefixCountBits": "How many earlier lanes of the masked cluster pass true.",
	"WaveMultiPrefixProduct": "Prefix product within the lane cluster the mask describes.",
	"WaveMultiPrefixSum": "Prefix sum within the lane cluster the mask describes.",
	"QuadReadLaneAt": "value from the given lane (0..3) of this 2x2 pixel quad.",
	"QuadReadAcrossX": "value from the horizontal neighbor in this 2x2 quad.",
	"QuadReadAcrossY": "value from the vertical neighbor in this 2x2 quad.",
	"QuadReadAcrossDiagonal": "value from the diagonal neighbor in this 2x2 quad.",
	"QuadAny": "True when cond is true on any lane of the 2x2 quad.",
	"QuadAll": "True when cond is true on every lane of the 2x2 quad.",
	"TraceRay": "Traces a ray through the acceleration structure, running hit and miss shaders with the payload.",
	"ReportHit": "Reports an intersection from an intersection shader; true when the hit was accepted.",
	"CallShader": "Invokes a callable shader by its shader table index, passing the parameter.",
	"IgnoreHit": "Rejects the candidate hit and resumes traversal (any-hit shaders).",
	"AcceptHitAndEndSearch": "Accepts the candidate hit and stops traversal (any-hit shaders).",
	"DispatchRaysIndex": "This thread's 3D index within the DispatchRays grid.",
	"DispatchRaysDimensions": "The 3D size of the DispatchRays grid.",
	"WorldRayOrigin": "The current ray's origin in world space.",
	"WorldRayDirection": "The current ray's direction in world space.",
	"ObjectRayOrigin": "The current ray's origin in object space of the hit instance.",
	"ObjectRayDirection": "The current ray's direction in object space of the hit instance.",
	"RayTMin": "The current ray's minimum t.",
	"RayTCurrent": "The current hit's t (or the search limit while traversing).",
	"PrimitiveIndex": "Index of the hit primitive within its geometry.",
	"InstanceID": "The user-supplied ID of the hit instance.",
	"InstanceIndex": "Index of the hit instance in the acceleration structure.",
	"GeometryIndex": "Index of the hit geometry within its instance.",
	"HitKind": "The hit kind: front/back facing for triangles, or what ReportHit passed.",
	"RayFlags": "The flags the current ray was traced with.",
	"ObjectToWorld": "Object-to-world transform of the hit instance.",
	"WorldToObject": "World-to-object transform of the hit instance.",
	"ObjectToWorld3x4": "Object-to-world transform of the hit instance, as 3x4.",
	"ObjectToWorld4x3": "Object-to-world transform of the hit instance, as 4x3.",
	"WorldToObject3x4": "World-to-object transform of the hit instance, as 3x4.",
	"WorldToObject4x3": "World-to-object transform of the hit instance, as 4x3.",
	"ClusterID": "ID of the hit cluster geometry.",
	"TriangleObjectPositions": "Object-space positions of the hit triangle's vertices.",
	"dot4add_u8packed": "Dot product of four packed u8 pairs, accumulated into c.",
	"dot4add_i8packed": "Dot product of four packed i8 pairs, accumulated into c.",
	"dot2add": "Dot product of two half pairs, accumulated into a float.",
	"unpack_s8s16": "Unpacks four packed signed bytes to int16_t4.",
	"unpack_u8u16": "Unpacks four packed unsigned bytes to uint16_t4.",
	"unpack_s8s32": "Unpacks four packed signed bytes to int4.",
	"unpack_u8u32": "Unpacks four packed unsigned bytes to uint4.",
	"pack_s8": "Packs four ints to signed bytes, truncating.",
	"pack_u8": "Packs four ints to unsigned bytes, truncating.",
	"pack_clamp_s8": "Packs four ints to signed bytes, saturating.",
	"pack_clamp_u8": "Packs four ints to unsigned bytes, saturating.",
	"SetMeshOutputCounts": "Declares how many vertices and primitives this mesh shader group emits.",
	"DispatchMesh": "Launches mesh shader groups from an amplification shader, passing the payload.",
	"and": "Component-wise logical and (HLSL 2021: && no longer works on vectors).",
	"or": "Component-wise logical or (HLSL 2021: || no longer works on vectors).",
	"select": "Component-wise cond ? t : f (HLSL 2021: ?: no longer works on vectors).",
	"Barrier": "Barrier over the given memory types and semantics: the SM 6.8 generalization of the *MemoryBarrier calls.",
	"GetRemainingRecursionLevels": "How many more recursion levels this work-graph node may launch.",
	"vk::ReadClock": "Reads the GPU clock at the given scope (shader clock extension).",
	"vk::RawBufferLoad": "Loads a T from a GPU virtual address (buffer device address).",
	"vk::RawBufferStore": "Stores a T at a GPU virtual address (buffer device address).",
	"vk::ext_execution_mode": "Emits a raw SPIR-V execution mode (inline SPIR-V).",
	"vk::ext_execution_mode_id": "Emits a raw SPIR-V execution mode taking id operands (inline SPIR-V).",
	"vk::static_pointer_cast": "Casts a vk::BufferPointer to a pointer to a layout-compatible type.",
	"vk::reinterpret_pointer_cast": "Casts a vk::BufferPointer to a pointer to any type, unchecked.",
	"dx::IsDebuggingEnabled": "True when the runtime asks shaders to take their debug paths.",
	"dx::MaybeReorderThread": "Hints the scheduler to regroup threads by hit object or coherence bits (Shader Execution Reordering).",
}

METHOD_DESC = {
	"CalculateLevelOfDetail": "The clamped mip level a Sample at x would pick.",
	"CalculateLevelOfDetailUnclamped": "The mip level a Sample at x would pick, before clamping to the resource's mip range.",
	"Gather": "The four texels bilinear filtering at x would blend, one per component: the red channel.",
	"GatherRed": "The four texels bilinear filtering at x would blend: the red channel.",
	"GatherGreen": "The four texels bilinear filtering at x would blend: the green channel.",
	"GatherBlue": "The four texels bilinear filtering at x would blend: the blue channel.",
	"GatherAlpha": "The four texels bilinear filtering at x would blend: the alpha channel.",
	"GatherCmp": "Gathers the four texels and compares each against compareValue, like SampleCmp.",
	"GatherCmpRed": "GatherCmp on the red channel.",
	"GatherCmpGreen": "GatherCmp on the green channel.",
	"GatherCmpBlue": "GatherCmp on the blue channel.",
	"GatherCmpAlpha": "GatherCmp on the alpha channel.",
	"GatherRaw": "Gathers the four texels as raw bits, without format conversion.",
	"GetDimensions": "The resource's dimensions, plus mip, sample, element or stride counts where it has them.",
	"GetSamplePosition": "Position of MSAA sample s within the pixel.",
	"Load": "Reads an element by integer address (texel coordinate, byte offset or structured index); the status overload reports residency.",
	"Load2": "Reads two consecutive uints at a byte offset.",
	"Load3": "Reads three consecutive uints at a byte offset.",
	"Load4": "Reads four consecutive uints at a byte offset.",
	"Store": "Writes a value at a byte offset.",
	"Store2": "Writes two consecutive uints at a byte offset.",
	"Store3": "Writes three consecutive uints at a byte offset.",
	"Store4": "Writes four consecutive uints at a byte offset.",
	"Sample": "Samples with filtering at coordinates x.",
	"SampleBias": "Samples with the implicit mip level biased.",
	"SampleCmp": "Samples through a comparison sampler: every fetched texel compares against compareValue, the results filter to 0..1.",
	"SampleCmpBias": "SampleCmp with the implicit mip level biased.",
	"SampleCmpGrad": "SampleCmp with explicit derivatives.",
	"SampleCmpLevel": "SampleCmp at an explicit mip level.",
	"SampleCmpLevelZero": "SampleCmp at mip 0, usable outside pixel shaders.",
	"SampleGrad": "Samples with explicit derivatives instead of the pixel quad's.",
	"SampleLevel": "Samples at an explicit mip level, usable outside pixel shaders.",
	"IncrementCounter": "Atomically bumps the buffer's hidden counter; returns the value before the increment.",
	"DecrementCounter": "Atomically drops the buffer's hidden counter; returns the value after the decrement.",
	"Append": "Appends value: onto an AppendStructuredBuffer, or as the next vertex of a geometry shader stream.",
	"Consume": "Pops and returns the element under the buffer's hidden counter.",
	"RestartStrip": "Ends the current strip, so the next Append starts a new one.",
	"WriteSamplerFeedback": "Records which mips a Sample at x would touch into the feedback map.",
	"WriteSamplerFeedbackBias": "WriteSamplerFeedback with the implicit mip level biased.",
	"WriteSamplerFeedbackGrad": "WriteSamplerFeedback with explicit derivatives.",
	"WriteSamplerFeedbackLevel": "WriteSamplerFeedback at an explicit mip level.",
	"InterlockedAdd": "Atomic add at a byte offset; " + D_ATOMIC,
	"InterlockedAnd": "Atomic bitwise and at a byte offset; " + D_ATOMIC,
	"InterlockedCompareExchange": "Atomic compare-and-swap at a byte offset; returns the prior value.",
	"InterlockedCompareStore": "Atomically stores value at a byte offset when the current value equals compare.",
	"InterlockedExchange": "Atomic swap at a byte offset; returns the prior value.",
	"InterlockedMax": "Atomic max at a byte offset; " + D_ATOMIC,
	"InterlockedMin": "Atomic min at a byte offset; " + D_ATOMIC,
	"InterlockedOr": "Atomic bitwise or at a byte offset; " + D_ATOMIC,
	"InterlockedXor": "Atomic bitwise xor at a byte offset; " + D_ATOMIC,
	"InterlockedAdd64": "64-bit atomic add at a byte offset; " + D_ATOMIC,
	"InterlockedAnd64": "64-bit atomic bitwise and at a byte offset; " + D_ATOMIC,
	"InterlockedCompareExchange64": "64-bit atomic compare-and-swap at a byte offset; returns the prior value.",
	"InterlockedCompareStore64": "64-bit atomic compare-and-store at a byte offset.",
	"InterlockedExchange64": "64-bit atomic swap at a byte offset; returns the prior value.",
	"InterlockedMax64": "64-bit atomic max at a byte offset; " + D_ATOMIC,
	"InterlockedMin64": "64-bit atomic min at a byte offset; " + D_ATOMIC,
	"InterlockedOr64": "64-bit atomic bitwise or at a byte offset; " + D_ATOMIC,
	"InterlockedXor64": "64-bit atomic bitwise xor at a byte offset; " + D_ATOMIC,
	"InterlockedExchangeFloat": "Atomic float swap at a byte offset; returns the prior value.",
	"InterlockedCompareStoreFloatBitwise": "Atomic float compare-and-store at a byte offset, comparing raw bits.",
	"InterlockedCompareExchangeFloatBitwise": "Atomic float compare-and-swap at a byte offset, comparing raw bits.",
	"TraceRayInline": "Initializes this ray query for a traversal; step it with Proceed().",
	"Proceed": "Advances traversal; true while a candidate hit needs this shader's evaluation.",
	"Abort": "Ends this ray query's traversal.",
	"CommitNonOpaqueTriangleHit": "Commits the candidate non-opaque triangle as the current closest hit.",
	"CommitProceduralPrimitiveHit": "Commits the candidate procedural primitive at t as the current closest hit.",
	"CommittedStatus": "What the committed hit is: nothing, a triangle, or a procedural primitive.",
	"CandidateType": "What the candidate is: a non-opaque triangle or a procedural primitive.",
	"CandidateObjectToWorld3x4": "Object-to-world transform of the candidate hit's instance, as 3x4.",
	"CandidateObjectToWorld4x3": "Object-to-world transform of the candidate hit's instance, as 4x3.",
	"CandidateWorldToObject3x4": "World-to-object transform of the candidate hit's instance, as 3x4.",
	"CandidateWorldToObject4x3": "World-to-object transform of the candidate hit's instance, as 4x3.",
	"CommittedObjectToWorld3x4": "Object-to-world transform of the committed hit's instance, as 3x4.",
	"CommittedObjectToWorld4x3": "Object-to-world transform of the committed hit's instance, as 4x3.",
	"CommittedWorldToObject3x4": "World-to-object transform of the committed hit's instance, as 3x4.",
	"CommittedWorldToObject4x3": "World-to-object transform of the committed hit's instance, as 4x3.",
	"CandidateProceduralPrimitiveNonOpaque": "True when the candidate procedural primitive is non-opaque.",
	"CandidateTriangleFrontFace": "True when the candidate triangle is front facing.",
	"CommittedTriangleFrontFace": "True when the committed triangle is front facing.",
	"CandidateTriangleBarycentrics": "Barycentrics of the candidate triangle hit.",
	"CommittedTriangleBarycentrics": "Barycentrics of the committed triangle hit.",
	"RayFlags": "The flags this query's ray was traced with.",
	"WorldRayOrigin": "This query's ray origin in world space.",
	"WorldRayDirection": "This query's ray direction in world space.",
	"RayTMin": "This query's ray minimum t.",
	"CandidateTriangleRayT": "t of the candidate triangle hit.",
	"CommittedRayT": "t of the committed hit.",
	"CandidateInstanceIndex": "Instance index of the candidate hit.",
	"CandidateInstanceID": "User-supplied instance ID of the candidate hit.",
	"CandidateGeometryIndex": "Geometry index of the candidate hit.",
	"CandidatePrimitiveIndex": "Primitive index of the candidate hit.",
	"CandidateObjectRayOrigin": "Ray origin in object space of the candidate hit's instance.",
	"CandidateObjectRayDirection": "Ray direction in object space of the candidate hit's instance.",
	"CommittedInstanceIndex": "Instance index of the committed hit.",
	"CommittedInstanceID": "User-supplied instance ID of the committed hit.",
	"CommittedGeometryIndex": "Geometry index of the committed hit.",
	"CommittedPrimitiveIndex": "Primitive index of the committed hit.",
	"CommittedObjectRayOrigin": "Ray origin in object space of the committed hit's instance.",
	"CommittedObjectRayDirection": "Ray direction in object space of the committed hit's instance.",
	"CandidateInstanceContributionToHitGroupIndex": "InstanceContributionToHitGroupIndex of the candidate hit's instance.",
	"CommittedInstanceContributionToHitGroupIndex": "InstanceContributionToHitGroupIndex of the committed hit's instance.",
	"CandidateClusterID": "Cluster ID of the candidate hit.",
	"CommittedClusterID": "Cluster ID of the committed hit.",
	"CandidateTriangleObjectPositions": "Object-space vertex positions of the candidate triangle.",
	"CommittedTriangleObjectPositions": "Object-space vertex positions of the committed triangle.",
	"MakeNop": "A hit object that is neither hit nor miss (no-op for MaybeReorderThread and Invoke).",
	"MakeMiss": "A hit object describing a miss of the given ray.",
	"FromRayQuery": "A hit object from a ray query's committed hit; the overload with Attributes overrides the hit kind.",
	"TraceRay": "Traces a ray and captures the result as a hit object without running hit or miss shaders.",
	"Invoke": "Runs the closest-hit or miss shader the hit object refers to, with the payload.",
	"IsMiss": "True when this hit object is a miss.",
	"IsHit": "True when this hit object is a hit.",
	"IsNop": "True when this hit object is neither hit nor miss.",
	"GetRayFlags": "The flags of the hit object's ray.",
	"GetRayTMin": "Minimum t of the hit object's ray.",
	"GetRayTCurrent": "t of the hit object's hit (or the ray extent for a miss).",
	"GetWorldRayOrigin": "The hit object's ray origin in world space.",
	"GetWorldRayDirection": "The hit object's ray direction in world space.",
	"GetObjectRayOrigin": "The hit object's ray origin in object space.",
	"GetObjectRayDirection": "The hit object's ray direction in object space.",
	"GetObjectToWorld3x4": "Object-to-world transform of the hit object's instance, as 3x4.",
	"GetObjectToWorld4x3": "Object-to-world transform of the hit object's instance, as 4x3.",
	"GetWorldToObject3x4": "World-to-object transform of the hit object's instance, as 3x4.",
	"GetWorldToObject4x3": "World-to-object transform of the hit object's instance, as 4x3.",
	"GetGeometryIndex": "Geometry index of the hit object's hit.",
	"GetInstanceIndex": "Instance index of the hit object's hit.",
	"GetInstanceID": "User-supplied instance ID of the hit object's hit.",
	"GetPrimitiveIndex": "Primitive index of the hit object's hit.",
	"GetHitKind": "Hit kind of the hit object's hit.",
	"GetShaderTableIndex": "The shader table record this hit object would invoke.",
	"GetAttributes": "Copies the hit object's intersection attributes out.",
	"SetShaderTableIndex": "Points the hit object at another shader table record.",
	"LoadLocalRootTableConstant": "Reads a local root table constant of the hit object's shader record.",
	"GetClusterID": "Cluster ID of the hit object's hit.",
	"TriangleObjectPositions": "Object-space vertex positions of the hit object's triangle.",
	"Count": "How many input records or items this node input holds.",
	"FinishedCrossGroupSharing": "True on the last group to finish with this shared input record.",
	"GetThreadNodeOutputRecords": "Allocates per-thread output records for this node output.",
	"GetGroupNodeOutputRecords": "Allocates group-shared output records for this node output.",
	"IsValid": "True when this node output is bound.",
	"GroupIncrementOutputCount": "Group-uniform bump of this empty output's record count.",
	"ThreadIncrementOutputCount": "Per-thread bump of this empty output's record count.",
	"OutputComplete": "Marks the allocated output records as done and launchable.",
	"SubpassLoad": "Reads the subpass input at this fragment's position.",
	"GetBufferContents": "Dereferences the vk::BufferPointer to its contents.",
}

# ---- the fork's own sources ------------------------------------------------------------------------

def load_db(dxc):
	"""The intrinsic file, parsed by DXC's own loader (the code that generates Sema's tables)."""

	table = os.path.join(dxc, "utils", "hct", "gen_intrin_main.txt")

	if not os.path.isfile(table):
		raise SystemExit("gen_intrinsics.py: no intrinsic table at %s (pass the DXC checkout as the argument)" % table)

	sys.path.insert(0, os.path.join(dxc, "utils", "hct"))
	from hctdb import db_hlsl

	with io.open(table, encoding="utf-8") as f:
		return db_hlsl(f, {}), table

def parse_sema(dxc):
	"""The object vocabulary out of SemaHLSL.cpp: the ArBasicKind enum gives each kind its ordinal, the
	g_ArBasicTypeNames literal gives each ordinal its display string, and the GetIntrinsicMethods switch
	says which kinds each method table serves. Anchors pin the enum and the names to each other, so the
	two lists drifting out of alignment fails generation instead of mislabeling every object."""

	path = os.path.join(dxc, "tools", "clang", "lib", "Sema", "SemaHLSL.cpp")

	if not os.path.isfile(path):
		raise SystemExit("gen_intrinsics.py: no SemaHLSL.cpp at %s" % path)

	with io.open(path, encoding="utf-8") as f:
		text = f.read()

	# Both literals carry commented-out entries (// AR_OBJECT_TEXTURE, // "texture"), so comments go
	# before anything is extracted or the two lists shift against each other.
	decommented = lambda block: re.sub(r"//[^\n]*", "", block)

	m = re.search(r"enum ArBasicKind \{(.*?)\n\};", text, re.S)
	if not m:
		raise SystemExit("gen_intrinsics.py: SemaHLSL.cpp no longer spells enum ArBasicKind")
	kinds = re.findall(r"^\s*(AR_\w+)\s*,?\s*$", decommented(m.group(1)), re.M)

	m = re.search(r"g_ArBasicTypeNames\[\] = \{(.*?)\n\};", text, re.S)
	if not m:
		raise SystemExit("gen_intrinsics.py: SemaHLSL.cpp no longer spells g_ArBasicTypeNames")
	names = re.findall(r'"((?:[^"\\]|\\.)*)"', decommented(m.group(1)))

	kindName = dict(zip(kinds, names))

	for kind, want in (
		("AR_BASIC_BOOL", "bool"), ("AR_OBJECT_TEXTURE2D", "Texture2D"),
		("AR_OBJECT_TRIANGLESTREAM", "TriangleStream")
	):
		if kindName.get(kind) != want:
			raise SystemExit("gen_intrinsics.py: ArBasicKind and g_ArBasicTypeNames no longer line up at " + kind)

	m = re.search(r"static void GetIntrinsicMethods\(.*?\{(.*?)\n\}", text, re.S)
	if not m:
		raise SystemExit("gen_intrinsics.py: SemaHLSL.cpp no longer spells GetIntrinsicMethods")

	nsObjects = {}
	pending = []

	for line in m.group(1).split("\n"):

		c = re.match(r"\s*case (AR_\w+):", line)
		if c:
			pending.append(c.group(1))
			continue

		a = re.search(r"\*intrinsics = g_(\w+);", line)
		if a:
			if not pending:
				raise SystemExit("gen_intrinsics.py: GetIntrinsicMethods assigns g_%s outside any case" % a.group(1))
			unknown = [k for k in pending if k not in kindName]
			if unknown:
				raise SystemExit("gen_intrinsics.py: GetIntrinsicMethods names kinds g_ArBasicTypeNames doesn't: " + ", ".join(unknown))
			nsObjects[a.group(1)] = [kindName[k] for k in pending]
			pending = []

	return nsObjects, set(kindName.values()), text

def objects_for(ns, nsObjects, typeNames):
	"""The objects a method namespace serves: GetIntrinsicMethods' switch is the authority, and a table
	it doesn't route (vk::BufferPointer's methods are declared bespoke in ASTContextHLSL) falls back to
	the namespace stem matched against Sema's own type names, so a rename still fails loudly."""

	if ns in nsObjects:
		return nsObjects[ns]

	stem = ns[:-len("Methods")]
	for cand in (stem, stem[2:] if stem[:2] in ("Vk", "Dx") else stem):
		if cand in typeNames:
			return [cand]

	raise SystemExit(
		"gen_intrinsics.py: no object association for namespace %s (not in GetIntrinsicMethods, no matching type name)" % ns
	)

# ---- respelling ------------------------------------------------------------------------------------

# The layout variables (hctdb's IA_R/IA_C/IA_R2/IA_C2) read as dimension symbols per declaration, in
# order of first appearance across the arguments and then the return, so transpose reads
# anyMxN transpose(anyNxM x) with the argument owning N and M.
class Symbols:
	def __init__(self): self.map = {}
	def of(self, token):
		if token not in self.map:
			taken = set(self.map.values())
			self.map[token] = next(s for s in ("N", "M", "P", "Q") if s not in taken)
		return self.map[token]

def dim(token, sym, state):
	if token.isdigit():
		return token
	if not token.startswith("IA_"):
		raise SystemExit("gen_intrinsics.py: unhandled row/col spelling " + token)
	state["g"] = True
	return sym.of(token)

# The template references: from the object ($classT), from the call's explicit template ($funcT), or
# from the object's first element (Gather's per-channel returns); each reads as T.
TEMPLATE_REF = { "INTRIN_TEMPLATE_FROM_TYPE", "INTRIN_TEMPLATE_FROM_FUNCTION", "INTRIN_TEMPLATE_FROM_FUNCTION_2" }
COMP_REF = { "INTRIN_COMPTYPE_FROM_TYPE_ELT0", "INTRIN_COMPTYPE_FROM_NODEOUTPUT" }

def render_param(p, sym, state):

	# A component reference only erases the spelling when the declaration wrote none (void<4>); a named
	# base whose component comes from elsewhere (ThreadNodeOutputRecords) keeps its name.
	usesT = p.template_id in TEMPLATE_REF or p.component_list == "LICOMPTYPE_USER_DEFINED_TYPE" or \
		(p.component_id in COMP_REF and p.component_list == "LICOMPTYPE_VOID")

	if usesT:
		base = "T"
		state["t"] = True
	elif p.component_list == "LICOMPTYPE_VOID":
		base = "void"
	else:
		if p.component_list not in COMP_DISPLAY:
			raise SystemExit("gen_intrinsics.py: no display spelling for " + p.component_list)
		base = COMP_DISPLAY[p.component_list]

	if p.template_list in ("LITEMPLATE_SCALAR", "LITEMPLATE_OBJECT", "LITEMPLATE_VOID"):
		return base
	if p.template_list == "LITEMPLATE_VECTOR":
		c = dim(p.cols, sym, state)
		return base if c == "1" else base + c
	if p.template_list == "LITEMPLATE_MATRIX":
		return base + dim(p.rows, sym, state) + "x" + dim(p.cols, sym, state)
	if p.template_list == "LITEMPLATE_ANY":
		state["g"] = True
		return base
	if p.template_list == "LITEMPLATE_ARRAY":
		return base + "[]"
	raise SystemExit("gen_intrinsics.py: unhandled template kind " + p.template_list)

def qual_text(p):
	q = p.param_qual
	out = ""
	if "AR_QUAL_GROUPSHARED" in q: out += "groupshared "
	if "AR_QUAL_REF" in q or ("AR_QUAL_IN" in q and "AR_QUAL_OUT" in q): out += "inout "
	elif "AR_QUAL_OUT" in q: out += "out "
	return out

def render_decl(i, displayName):

	sym = Symbols()
	state = { "g": False, "t": False }
	args = []

	for p in i.params[1:]:

		if p.template_id == "INTRIN_TEMPLATE_VARARGS":
			args.append("...")
			continue

		args.append((qual_text(p) + render_param(p, sym, state) + " " + p.name).strip())

	ret = render_param(i.params[0], sym, state)
	static = "static " if i.static_member else ""
	sig = "%s%s %s(%s)" % (static, ret, displayName, ", ".join(args))
	return sig, state["g"], state["t"]

def min_sm(i):
	if not i.min_shader_model:
		return None
	return "%d.%d" % (i.min_shader_model >> 4, i.min_shader_model & 15)

# ---- building the tables ---------------------------------------------------------------------------

def fn_prefix_and_name(i):
	"""Namespace prefix and unmangled name for a free function: hctdb prepends the namespace tag to the
	stored name (VkReadClock, DxMaybeReorderThread), so the display strips exactly that tag back off."""

	tag = i.ns[:-len("Intrinsics")]
	if not tag:
		return "", i.name
	if not i.name.startswith(tag):
		raise SystemExit("gen_intrinsics.py: %s::%s doesn't carry its namespace tag" % (i.ns, i.name))
	return tag.lower(), i.name[len(tag):]

def skip(i, name):
	if i.hidden or name.startswith("__builtin") or name == "source_mark":
		return True
	return any(p.component_list in LEGACY_SAMPLERS for p in i.params)

MAX_METHOD_SIGS = 10

# The trailing parameters an overload may add to another without being its own story in a hover.
SUFFIX_PARAM = re.compile(r"^(int\d? o[1-4]?|float (bias|clamp|lod)|out uint status)$")

def arg_list(sig):
	inner = sig[sig.index("(") + 1:-1]
	return [a for a in inner.split(", ") if a]

def extends(args, base):
	return len(args) > len(base) and args[:len(base)] == base and \
		all(SUFFIX_PARAM.match(a) for a in args[len(base):])

def build(db, nsObjects, typeNames):

	fns, methods, missing = {}, {}, []

	for i in db.intrinsics:

		isFn = i.ns.endswith("Intrinsics")

		if not isFn and not i.ns.endswith("Methods"):
			raise SystemExit("gen_intrinsics.py: namespace %s is neither functions nor methods" % i.ns)

		prefix, name = fn_prefix_and_name(i) if isFn else ("", i.name)

		if skip(i, name):
			continue

		sig, generic, usesT = render_decl(i, name)
		sm = min_sm(i)

		if isFn:

			key = (prefix + "::" if prefix else "") + name
			e = fns.setdefault(name, { "sigs": [] })
			if prefix: e["ns"] = prefix
			if sig not in e["sigs"]: e["sigs"].append(sig)
			if generic: e["g"] = 1
			if usesT: e["t"] = 1
			if sm and not e.get("sm"): e["sm"] = sm
			if key in DESC: e["doc"] = DESC[key]
			elif "doc" not in e: missing.append(key)

		else:

			e = methods.setdefault(name, { "sigs": [] })
			row = next((r for r in e["sigs"] if r["s"] == sig), None)
			if not row:
				row = { "s": sig, "on": [] }
				e["sigs"].append(row)
			for o in objects_for(i.ns, nsObjects, typeNames):
				if o not in row["on"]: row["on"].append(o)
			if generic: e["g"] = 1
			if usesT: e["t"] = 1
			if sm and not e.get("sm"): e["sm"] = sm
			if name in METHOD_DESC: e["doc"] = METHOD_DESC[name]
			elif "doc" not in e: missing.append(i.ns + "." + name)

	# The Gather and Sample families explode into offset, clamp and status overloads per object: each is
	# another signature with the same core plus trailing convenience parameters. A hover reads the core
	# forms, so a signature that only extends a kept one with those trailing parameters folds into the
	# overload count, and the count also absorbs whatever the cap drops after that.
	for e in methods.values():

		e["sigs"].sort(key=lambda r: (r["s"].count(","), len(r["s"])))

		kept = []
		dropped = 0
		for r in e["sigs"]:
			if any(extends(arg_list(r["s"]), arg_list(k["s"])) for k in kept):
				dropped += 1
			else:
				kept.append(r)

		dropped += max(0, len(kept) - MAX_METHOD_SIGS)
		e["sigs"] = kept[:MAX_METHOD_SIGS]
		if dropped: e["more"] = dropped

	return fns, methods, sorted(set(missing))

# ---- output ----------------------------------------------------------------------------------------

def js_str(s):
	return json.dumps(s)

def emit_fns(w, fns):
	for name in sorted(fns):
		e = fns[name]
		fields = ["sigs: [%s]" % ", ".join(js_str(s) for s in e["sigs"])]
		for k in ("ns", "sm"):
			if k in e: fields.append("%s: %s" % (k, js_str(e[k])))
		for k in ("g", "t"):
			if k in e: fields.append("%s: 1" % k)
		if "doc" in e: fields.append("doc: %s" % js_str(e["doc"]))
		w.write("  %s: { %s },\n" % (js_str(name), ", ".join(fields)))

def emit_methods(w, methods):
	for name in sorted(methods):
		e = methods[name]
		w.write("  %s: {\n    sigs: [\n" % js_str(name))
		for r in e["sigs"]:
			w.write("      { s: %s, on: [%s] },\n" % (js_str(r["s"]), ", ".join(js_str(o) for o in r["on"])))
		w.write("    ],\n")
		fields = []
		if "sm" in e: fields.append("sm: %s" % js_str(e["sm"]))
		for k in ("g", "t"):
			if k in e: fields.append("%s: 1" % k)
		if "more" in e: fields.append("more: %d" % e["more"])
		if "doc" in e: fields.append("doc: %s" % js_str(e["doc"]))
		if fields: w.write("    %s,\n" % ",\n    ".join(fields))
		w.write("  },\n")

def forkCommit(dxc):

	"""What the sources are, for the provenance line: a checkout answers with git, and the conan package,
	which ships no .git, answers with the DXC_COMMIT its recipe wrote while a tree still existed."""

	try:
		return subprocess.check_output(
			["git", "-C", dxc, "rev-parse", "HEAD"], stderr=subprocess.DEVNULL).decode().strip()
	except Exception:
		pass

	stamp = os.path.join(dxc, "DXC_COMMIT")

	if os.path.isfile(stamp):
		with io.open(stamp, encoding="utf-8") as f:
			return f.read().strip()

	return "unknown"

def main():

	argv = list(sys.argv[1:])
	out = os.path.join(ROOT, "js", "intrinsics_data.js")

	if "-o" in argv:
		at = argv.index("-o")
		out = argv[at + 1]
		del argv[at:at + 2]

	# No default: the only tree that could be one is a checkout beside the repository, which exists on
	# whoever happens to have cloned the fork and nowhere else. build_web.py passes the conan package.

	if not argv:
		raise SystemExit(
			"gen_intrinsics.py: pass a DXC checkout, or the dxc conan package's res/dxc_intrinsics "
			"(build_web.py --run_frontend_tests finds it for you)"
		)

	dxc = argv[0]

	db, tablePath = load_db(dxc)
	nsObjects, typeNames, semaText = parse_sema(dxc)

	# Every component type the fork's loader declares has to have a display spelling here, so a new one
	# fails generation instead of leaking through misspelled or unlisted.
	unknown = sorted(set(db.base_types.values()) - set(COMP_DISPLAY) - LEGACY_SAMPLERS)
	if unknown:
		raise SystemExit("gen_intrinsics.py: the fork added component types with no display spelling: " + ", ".join(unknown))

	commit = forkCommit(dxc)

	fns, methods, missing = build(db, nsObjects, typeNames)

	for name in missing:
		print("-- no description for %s (it ships with its signature only)" % name)

	sigCount = sum(len(e["sigs"]) for e in fns.values()) + \
		sum(len(e["sigs"]) + e.get("more", 0) for e in methods.values())

	with io.open(tablePath, "rb") as f:
		tableSha = hashlib.sha256(f.read()).hexdigest()[:16]
	semaSha = hashlib.sha256(semaText.encode("utf-8")).hexdigest()[:16]

	with io.open(out, "w", encoding="utf-8", newline="\n") as w:
		w.write(
"""/* GENERATED FILE, do not edit: python web/dev/gen_intrinsics.py <DXC checkout or package tables>
 *
 * The HLSL builtin intrinsics, off the DXC fork's own sources: utils/hct/gen_intrin_main.txt parsed
 * by the fork's hctdb.py (the loader that generates Sema's tables), object names and method-table
 * associations out of SemaHLSL.cpp, respelled the way documentation writes signatures.
 * `fns` are the free functions (ns marks vk:: and dx::), `methods` the object methods grouped by name
 * with the objects each signature exists on. g = layout-generic (scalar, vector and matrix takes),
 * t = T stands for the element or template type, sm = minimum shader model, more = overloads past the cap.
 *
 * Source: %s @ %s (table sha256 %s, SemaHLSL.cpp sha256 %s), %d names / %d signatures.
 */
window.OxIntrinsicsData = {
""" % ("github.com/Oxsomi/DirectXShaderCompiler", commit[:12], tableSha, semaSha,
			len(fns) + len(methods), sigCount))
		w.write("fns: {\n")
		emit_fns(w, fns)
		w.write("},\nmethods: {\n")
		emit_methods(w, methods)
		w.write("}\n};\n")

	print("-- %s: %d functions, %d methods, %d signatures" % (os.path.basename(out), len(fns), len(methods), sigCount))

if __name__ == "__main__":
	main()

