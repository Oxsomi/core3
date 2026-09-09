/* GENERATED FILE, do not edit: python web/dev/gen_intrinsics.py <DXC checkout or package tables>
 *
 * The HLSL builtin intrinsics, off the DXC fork's own sources: utils/hct/gen_intrin_main.txt parsed
 * by the fork's hctdb.py (the loader that generates Sema's tables), object names and method-table
 * associations out of SemaHLSL.cpp, respelled the way documentation writes signatures.
 * `fns` are the free functions (ns marks vk:: and dx::), `methods` the object methods grouped by name
 * with the objects each signature exists on. g = layout-generic (scalar, vector and matrix takes),
 * t = T stands for the element or template type, sm = minimum shader model, more = overloads past the cap.
 *
 * Source: github.com/Oxsomi/DirectXShaderCompiler @ db9a488703d4 (table sha256 c57366fbc5dc346e, SemaHLSL.cpp sha256 f03e135724ccecff), 359 names / 1074 signatures.
 */
window.OxIntrinsicsData = {
fns: {
  "AcceptHitAndEndSearch": { sigs: ["void AcceptHitAndEndSearch()"], doc: "Accepts the candidate hit and stops traversal (any-hit shaders)." },
  "AddUint64": { sigs: ["uintN AddUint64(uintN a, uintN b)"], g: 1, doc: "Treats a uint2/uint4 as one/two 64-bit values and adds with carry between the halves." },
  "AllMemoryBarrier": { sigs: ["void AllMemoryBarrier()"], doc: "Waits until every outstanding device and groupshared access of this thread has finished." },
  "AllMemoryBarrierWithGroupSync": { sigs: ["void AllMemoryBarrierWithGroupSync()"], doc: "AllMemoryBarrier plus an execution sync of the whole thread group." },
  "Barrier": { sigs: ["void Barrier(uint MemoryTypeFlags, uint SemanticFlags)", "void Barrier(NodeRecordOrUAV o, uint SemanticFlags)"], doc: "Barrier over the given memory types and semantics: the SM 6.8 generalization of the *MemoryBarrier calls." },
  "CallShader": { sigs: ["void CallShader(uint ShaderIndex, inout T Parameter)"], t: 1, doc: "Invokes a callable shader by its shader table index, passing the parameter." },
  "CheckAccessFullyMapped": { sigs: ["bool CheckAccessFullyMapped(uint status)"], doc: "True when a Load/Sample status value says every byte came from mapped (resident) memory." },
  "ClusterID": { sigs: ["uint ClusterID()"], sm: "6.10", doc: "ID of the hit cluster geometry." },
  "D3DCOLORtoUBYTE4": { sigs: ["int4 D3DCOLORtoUBYTE4(float4 x)"], doc: "Scales a 0..1 float4 color by 255 and swizzles it the way D3D9's UBYTE4 vertex format expects." },
  "DebugBreak": { sigs: ["void DebugBreak()"], sm: "6.10", doc: "Breaks into the attached shader debugger when one is present." },
  "DeviceMemoryBarrier": { sigs: ["void DeviceMemoryBarrier()"], doc: "Waits until every outstanding device (UAV) access of this thread has finished." },
  "DeviceMemoryBarrierWithGroupSync": { sigs: ["void DeviceMemoryBarrierWithGroupSync()"], doc: "DeviceMemoryBarrier plus an execution sync of the whole thread group." },
  "DispatchMesh": { sigs: ["void DispatchMesh(uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ, T meshPayload)"], t: 1, doc: "Launches mesh shader groups from an amplification shader, passing the payload." },
  "DispatchRaysDimensions": { sigs: ["uint3 DispatchRaysDimensions()"], doc: "The 3D size of the DispatchRays grid." },
  "DispatchRaysIndex": { sigs: ["uint3 DispatchRaysIndex()"], doc: "This thread's 3D index within the DispatchRays grid." },
  "EvaluateAttributeAtSample": { sigs: ["numeric EvaluateAttributeAtSample(numeric value, uint index)"], g: 1, doc: "Evaluates a vertex attribute at a given MSAA sample position." },
  "EvaluateAttributeCentroid": { sigs: ["numeric EvaluateAttributeCentroid(numeric value)"], g: 1, doc: "Evaluates a vertex attribute at the pixel's centroid location." },
  "EvaluateAttributeSnapped": { sigs: ["numeric EvaluateAttributeSnapped(numeric value, int2 offset)"], g: 1, doc: "Evaluates a vertex attribute at a pixel offset, given in 1/16th steps." },
  "GeometryIndex": { sigs: ["uint GeometryIndex()"], doc: "Index of the hit geometry within its instance." },
  "GetAttributeAtVertex": { sigs: ["numeric GetAttributeAtVertex(numeric value, uint VertexID)"], g: 1, doc: "Reads a nointerpolation attribute at one of the primitive's vertices." },
  "GetGroupWaveCount": { sigs: ["uint GetGroupWaveCount()"], sm: "6.10", doc: "Waves per thread group." },
  "GetGroupWaveIndex": { sigs: ["uint GetGroupWaveIndex()"], sm: "6.10", doc: "This wave's index within its thread group." },
  "GetRemainingRecursionLevels": { sigs: ["uint GetRemainingRecursionLevels()"], sm: "6.8", doc: "How many more recursion levels this work-graph node may launch." },
  "GetRenderTargetSampleCount": { sigs: ["uint GetRenderTargetSampleCount()"], doc: "MSAA sample count of the current render target." },
  "GetRenderTargetSamplePosition": { sigs: ["float2 GetRenderTargetSamplePosition(int s)"], doc: "Position of MSAA sample s of the current render target, relative to the pixel center." },
  "GroupMemoryBarrier": { sigs: ["void GroupMemoryBarrier()"], doc: "Waits until every outstanding groupshared access of this thread has finished." },
  "GroupMemoryBarrierWithGroupSync": { sigs: ["void GroupMemoryBarrierWithGroupSync()"], doc: "GroupMemoryBarrier plus an execution sync of the whole thread group." },
  "HitKind": { sigs: ["uint HitKind()"], doc: "The hit kind: front/back facing for triangles, or what ReportHit passed." },
  "IgnoreHit": { sigs: ["void IgnoreHit()"], doc: "Rejects the candidate hit and resumes traversal (any-hit shaders)." },
  "InstanceID": { sigs: ["uint InstanceID()"], doc: "The user-supplied ID of the hit instance." },
  "InstanceIndex": { sigs: ["uint InstanceIndex()"], doc: "Index of the hit instance in the acceleration structure." },
  "InterlockedAdd": { sigs: ["void InterlockedAdd(inout int64_t result, uint64_t value)", "void InterlockedAdd(inout int64_t result, uint64_t value, out int64_t original)", "void InterlockedAdd(inout int result, uint value)", "void InterlockedAdd(inout int result, uint value, out int original)"], doc: "Atomic add on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "InterlockedAnd": { sigs: ["void InterlockedAnd(inout int64_t result, uint64_t value)", "void InterlockedAnd(inout int64_t result, uint64_t value, out int64_t original)", "void InterlockedAnd(inout int result, uint value)", "void InterlockedAnd(inout int result, uint value, out int original)"], doc: "Atomic bitwise and on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "InterlockedCompareExchange": { sigs: ["void InterlockedCompareExchange(inout int64_t result, uint64_t compare, uint64_t value, out int64_t original)", "void InterlockedCompareExchange(inout int result, uint compare, uint value, out int original)"], doc: "Atomic compare-and-swap: stores value when the destination equals compare; returns the prior value." },
  "InterlockedCompareExchangeFloatBitwise": { sigs: ["void InterlockedCompareExchangeFloatBitwise(inout float result, float compare, float value, out float original)"], doc: "Atomic float compare-and-swap comparing raw bits (no NaN or signed-zero equality)." },
  "InterlockedCompareStore": { sigs: ["void InterlockedCompareStore(inout int64_t result, uint64_t compare, uint64_t value)", "void InterlockedCompareStore(inout int result, uint compare, uint value)"], doc: "Atomically stores value when the destination equals compare." },
  "InterlockedCompareStoreFloatBitwise": { sigs: ["void InterlockedCompareStoreFloatBitwise(inout float result, float compare, float value)"], doc: "Atomically stores a float when the destination's raw bits equal compare's." },
  "InterlockedExchange": { sigs: ["void InterlockedExchange(inout int64_t result, int64_t value, out int64_t original)", "void InterlockedExchange(inout float result, float value, out float original)", "void InterlockedExchange(inout int result, uint value, out int original)"], doc: "Atomic swap; returns the value that was there before." },
  "InterlockedMax": { sigs: ["void InterlockedMax(inout int64_t result, int64_t value)", "void InterlockedMax(inout int64_t result, int64_t value, out int64_t original)", "void InterlockedMax(inout int result, int value)", "void InterlockedMax(inout int result, int value, out int original)"], doc: "Atomic max on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "InterlockedMin": { sigs: ["void InterlockedMin(inout int64_t result, int64_t value)", "void InterlockedMin(inout int64_t result, int64_t value, out int64_t original)", "void InterlockedMin(inout int result, int value)", "void InterlockedMin(inout int result, int value, out int original)"], doc: "Atomic min on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "InterlockedOr": { sigs: ["void InterlockedOr(inout int64_t result, uint64_t value)", "void InterlockedOr(inout int64_t result, uint64_t value, out int64_t original)", "void InterlockedOr(inout int result, uint value)", "void InterlockedOr(inout int result, uint value, out int original)"], doc: "Atomic bitwise or on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "InterlockedXor": { sigs: ["void InterlockedXor(inout int64_t result, uint64_t value)", "void InterlockedXor(inout int64_t result, uint64_t value, out int64_t original)", "void InterlockedXor(inout int result, uint value)", "void InterlockedXor(inout int result, uint value, out int original)"], doc: "Atomic bitwise xor on a groupshared or UAV destination; the overload with `original` also returns the value that was there before." },
  "IsDebuggingEnabled": { sigs: ["bool IsDebuggingEnabled()"], ns: "dx", sm: "6.10", doc: "True when the runtime asks shaders to take their debug paths." },
  "IsHelperLane": { sigs: ["bool IsHelperLane()"], doc: "True on a pixel-quad helper lane that only runs to provide derivatives." },
  "MaybeReorderThread": { sigs: ["void MaybeReorderThread(dx::HitObject HitObject)", "void MaybeReorderThread(uint CoherenceHint, uint NumCoherenceHintBitsFromLSB)", "void MaybeReorderThread(dx::HitObject HitObject, uint CoherenceHint, uint NumCoherenceHintBitsFromLSB)"], ns: "dx", sm: "6.9", doc: "Hints the scheduler to regroup threads by hit object or coherence bits (Shader Execution Reordering)." },
  "NonUniformResourceIndex": { sigs: ["any NonUniformResourceIndex(any index)"], g: 1, doc: "Marks a resource index as divergent, so the hardware must not assume it is wave-uniform." },
  "ObjectRayDirection": { sigs: ["float3 ObjectRayDirection()"], doc: "The current ray's direction in object space of the hit instance." },
  "ObjectRayOrigin": { sigs: ["float3 ObjectRayOrigin()"], doc: "The current ray's origin in object space of the hit instance." },
  "ObjectToWorld": { sigs: ["float3x4 ObjectToWorld()"], doc: "Object-to-world transform of the hit instance." },
  "ObjectToWorld3x4": { sigs: ["float3x4 ObjectToWorld3x4()"], doc: "Object-to-world transform of the hit instance, as 3x4." },
  "ObjectToWorld4x3": { sigs: ["float4x3 ObjectToWorld4x3()"], doc: "Object-to-world transform of the hit instance, as 4x3." },
  "PrimitiveIndex": { sigs: ["uint PrimitiveIndex()"], doc: "Index of the hit primitive within its geometry." },
  "Process2DQuadTessFactorsAvg": { sigs: ["void Process2DQuadTessFactorsAvg(float4 RawEdgeFactors, float2 InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw 2D-quad-patch tess factors, averaging to derive the inside factors." },
  "Process2DQuadTessFactorsMax": { sigs: ["void Process2DQuadTessFactorsMax(float4 RawEdgeFactors, float2 InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw 2D-quad-patch tess factors, taking the max for the inside factors." },
  "Process2DQuadTessFactorsMin": { sigs: ["void Process2DQuadTessFactorsMin(float4 RawEdgeFactors, float2 InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw 2D-quad-patch tess factors, taking the min for the inside factors." },
  "ProcessIsolineTessFactors": { sigs: ["void ProcessIsolineTessFactors(float RawDetailFactor, float RawDensityFactor, out float RoundedDetailFactorr, out float RoundedDensityFactor)"], doc: "Rounds raw isoline detail and density tess factors." },
  "ProcessQuadTessFactorsAvg": { sigs: ["void ProcessQuadTessFactorsAvg(float4 RawEdgeFactors, float InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw quad-patch tess factors, averaging to derive the inside factors." },
  "ProcessQuadTessFactorsMax": { sigs: ["void ProcessQuadTessFactorsMax(float4 RawEdgeFactors, float InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw quad-patch tess factors, taking the max for the inside factors." },
  "ProcessQuadTessFactorsMin": { sigs: ["void ProcessQuadTessFactorsMin(float4 RawEdgeFactors, float InsideScale, out float4 RoundedEdgeFactors, out float2 RoundedInsideFactors, out float2 UnroundedInsideFactors)"], doc: "Rounds raw quad-patch tess factors, taking the min for the inside factors." },
  "ProcessTriTessFactorsAvg": { sigs: ["void ProcessTriTessFactorsAvg(float3 RawEdgeFactors, float InsideScale, out float3 RoundedEdgeFactors, out float RoundedInsideFactor, out float UnroundedInsideFactor)"], doc: "Rounds raw tri-patch tess factors, averaging to derive the inside factor." },
  "ProcessTriTessFactorsMax": { sigs: ["void ProcessTriTessFactorsMax(float3 RawEdgeFactors, float InsideScale, out float3 RoundedEdgeFactors, out float RoundedInsideFactor, out float UnroundedInsideFactor)"], doc: "Rounds raw tri-patch tess factors, taking the max for the inside factor." },
  "ProcessTriTessFactorsMin": { sigs: ["void ProcessTriTessFactorsMin(float3 RawEdgeFactors, float InsideScale, out float3 RoundedEdgeFactors, out float RoundedInsideFactor, out float UnroundedInsideFactor)"], doc: "Rounds raw tri-patch tess factors, taking the min for the inside factor." },
  "QuadAll": { sigs: ["bool QuadAll(bool cond)"], doc: "True when cond is true on every lane of the 2x2 quad." },
  "QuadAny": { sigs: ["bool QuadAny(bool cond)"], doc: "True when cond is true on any lane of the 2x2 quad." },
  "QuadReadAcrossDiagonal": { sigs: ["numeric QuadReadAcrossDiagonal(numeric value)"], g: 1, doc: "value from the diagonal neighbor in this 2x2 quad." },
  "QuadReadAcrossX": { sigs: ["numeric QuadReadAcrossX(numeric value)"], g: 1, doc: "value from the horizontal neighbor in this 2x2 quad." },
  "QuadReadAcrossY": { sigs: ["numeric QuadReadAcrossY(numeric value)"], g: 1, doc: "value from the vertical neighbor in this 2x2 quad." },
  "QuadReadLaneAt": { sigs: ["numeric QuadReadLaneAt(numeric value, uint quadLane)"], g: 1, doc: "value from the given lane (0..3) of this 2x2 pixel quad." },
  "RawBufferLoad": { sigs: ["T RawBufferLoad(uint64_t addr)", "T RawBufferLoad(uint64_t addr, uint alignment)"], ns: "vk", t: 1, doc: "Loads a T from a GPU virtual address (buffer device address)." },
  "RawBufferStore": { sigs: ["void RawBufferStore(uint64_t addr, T value)", "void RawBufferStore(uint64_t addr, T value, uint alignment)"], ns: "vk", t: 1, doc: "Stores a T at a GPU virtual address (buffer device address)." },
  "RayFlags": { sigs: ["uint RayFlags()"], doc: "The flags the current ray was traced with." },
  "RayTCurrent": { sigs: ["float RayTCurrent()"], doc: "The current hit's t (or the search limit while traversing)." },
  "RayTMin": { sigs: ["float RayTMin()"], doc: "The current ray's minimum t." },
  "ReadClock": { sigs: ["uint64_t ReadClock(uint scope)"], ns: "vk", doc: "Reads the GPU clock at the given scope (shader clock extension)." },
  "ReportHit": { sigs: ["bool ReportHit(float THit, uint HitKind, T Attributes)"], t: 1, doc: "Reports an intersection from an intersection shader; true when the hit was accepted." },
  "SetMeshOutputCounts": { sigs: ["void SetMeshOutputCounts(uint numVertices, uint numPrimitives)"], doc: "Declares how many vertices and primitives this mesh shader group emits." },
  "TraceRay": { sigs: ["void TraceRay(RaytracingAccelerationStructure AccelerationStructure, uint RayFlags, uint InstanceInclusionMask, uint RayContributionToHitGroupIndex, uint MultiplierForGeometryContributionToHitGroupIndex, uint MissShaderIndex, RayDesc Ray, inout T Payload)"], t: 1, doc: "Traces a ray through the acceleration structure, running hit and miss shaders with the payload." },
  "TriangleObjectPositions": { sigs: ["BuiltInTrianglePositions TriangleObjectPositions()"], sm: "6.10", doc: "Object-space positions of the hit triangle's vertices." },
  "WaveActiveAllEqual": { sigs: ["bool WaveActiveAllEqual(any value)"], g: 1, doc: "True, per component, where the value matches across every active lane." },
  "WaveActiveAllTrue": { sigs: ["bool WaveActiveAllTrue(bool cond)"], doc: "True when cond is true on every active lane." },
  "WaveActiveAnyTrue": { sigs: ["bool WaveActiveAnyTrue(bool cond)"], doc: "True when cond is true on any active lane." },
  "WaveActiveBallot": { sigs: ["uint4 WaveActiveBallot(bool cond)"], doc: "Bitmask of the active lanes where cond is true, as uint4." },
  "WaveActiveBitAnd": { sigs: ["uint WaveActiveBitAnd(uint value)"], g: 1, doc: "Bitwise and of value across all active lanes." },
  "WaveActiveBitOr": { sigs: ["uint WaveActiveBitOr(uint value)"], g: 1, doc: "Bitwise or of value across all active lanes." },
  "WaveActiveBitXor": { sigs: ["uint WaveActiveBitXor(uint value)"], g: 1, doc: "Bitwise xor of value across all active lanes." },
  "WaveActiveCountBits": { sigs: ["uint WaveActiveCountBits(bool value)"], doc: "How many active lanes pass true." },
  "WaveActiveMax": { sigs: ["numeric WaveActiveMax(numeric value)"], g: 1, doc: "Maximum of value across all active lanes." },
  "WaveActiveMin": { sigs: ["numeric WaveActiveMin(numeric value)"], g: 1, doc: "Minimum of value across all active lanes." },
  "WaveActiveProduct": { sigs: ["numeric WaveActiveProduct(numeric value)"], g: 1, doc: "Product of value across all active lanes." },
  "WaveActiveSum": { sigs: ["numeric WaveActiveSum(numeric value)"], g: 1, doc: "Sum of value across all active lanes." },
  "WaveGetLaneCount": { sigs: ["uint WaveGetLaneCount()"], doc: "Lanes per wave on this hardware." },
  "WaveGetLaneIndex": { sigs: ["uint WaveGetLaneIndex()"], doc: "This lane's index within its wave." },
  "WaveIsFirstLane": { sigs: ["bool WaveIsFirstLane()"], doc: "True on the first active lane of the wave." },
  "WaveMatch": { sigs: ["uint4 WaveMatch(numeric value)"], g: 1, doc: "Bitmask of the active lanes holding the same value as this one, as uint4." },
  "WaveMultiPrefixBitAnd": { sigs: ["int WaveMultiPrefixBitAnd(int value, uint4 mask)"], g: 1, doc: "Prefix bitwise and within the lane cluster the mask describes." },
  "WaveMultiPrefixBitOr": { sigs: ["int WaveMultiPrefixBitOr(int value, uint4 mask)"], g: 1, doc: "Prefix bitwise or within the lane cluster the mask describes." },
  "WaveMultiPrefixBitXor": { sigs: ["int WaveMultiPrefixBitXor(int value, uint4 mask)"], g: 1, doc: "Prefix bitwise xor within the lane cluster the mask describes." },
  "WaveMultiPrefixCountBits": { sigs: ["uint WaveMultiPrefixCountBits(bool value, uint4 mask)"], doc: "How many earlier lanes of the masked cluster pass true." },
  "WaveMultiPrefixProduct": { sigs: ["numeric WaveMultiPrefixProduct(numeric value, uint4 mask)"], g: 1, doc: "Prefix product within the lane cluster the mask describes." },
  "WaveMultiPrefixSum": { sigs: ["numeric WaveMultiPrefixSum(numeric value, uint4 mask)"], g: 1, doc: "Prefix sum within the lane cluster the mask describes." },
  "WavePrefixCountBits": { sigs: ["uint WavePrefixCountBits(bool value)"], doc: "How many active lanes before this one pass true (exclusive)." },
  "WavePrefixProduct": { sigs: ["numeric WavePrefixProduct(numeric value)"], g: 1, doc: "Exclusive prefix product over the active lanes before this one." },
  "WavePrefixSum": { sigs: ["numeric WavePrefixSum(numeric value)"], g: 1, doc: "Exclusive prefix sum over the active lanes before this one." },
  "WaveReadLaneAt": { sigs: ["any WaveReadLaneAt(any value, uint lane)"], g: 1, doc: "Broadcasts value from the given lane." },
  "WaveReadLaneFirst": { sigs: ["any WaveReadLaneFirst(any value)"], g: 1, doc: "Broadcasts value from the first active lane." },
  "WorldRayDirection": { sigs: ["float3 WorldRayDirection()"], doc: "The current ray's direction in world space." },
  "WorldRayOrigin": { sigs: ["float3 WorldRayOrigin()"], doc: "The current ray's origin in world space." },
  "WorldToObject": { sigs: ["float3x4 WorldToObject()"], doc: "World-to-object transform of the hit instance." },
  "WorldToObject3x4": { sigs: ["float3x4 WorldToObject3x4()"], doc: "World-to-object transform of the hit instance, as 3x4." },
  "WorldToObject4x3": { sigs: ["float4x3 WorldToObject4x3()"], doc: "World-to-object transform of the hit instance, as 4x3." },
  "abort": { sigs: ["void abort()"], doc: "Terminates execution of the current draw or dispatch." },
  "abs": { sigs: ["numeric abs(numeric x)"], g: 1, doc: "Absolute value, per component." },
  "acos": { sigs: ["float acos(float x)"], g: 1, doc: "Arccosine in radians, per component." },
  "all": { sigs: ["bool all(any x)"], g: 1, doc: "True when every component is non-zero." },
  "and": { sigs: ["bool and(bool x, bool y)"], g: 1, doc: "Component-wise logical and (HLSL 2021: && no longer works on vectors)." },
  "any": { sigs: ["bool any(any x)"], g: 1, doc: "True when any component is non-zero." },
  "asdouble": { sigs: ["double asdouble(uint x, uint y)"], g: 1, doc: "Reassembles a double from its low and high uint words, per component." },
  "asfloat": { sigs: ["float asfloat(numeric x)"], g: 1, doc: "Reinterprets the bits as float, per component." },
  "asfloat16": { sigs: ["float16_t asfloat16(numeric x)"], g: 1, doc: "Reinterprets the bits of 16-bit values as float16_t, per component." },
  "asin": { sigs: ["float asin(float x)"], g: 1, doc: "Arcsine in radians, per component." },
  "asint": { sigs: ["int asint(numeric x)"], g: 1, doc: "Reinterprets the bits as int, per component." },
  "asint16": { sigs: ["int16_t asint16(numeric x)"], g: 1, doc: "Reinterprets the bits of 16-bit values as int16_t, per component." },
  "asuint": { sigs: ["void asuint(double d, out uint x, out uint y)", "uint asuint(numeric x)"], g: 1, doc: "Reinterprets the bits as uint; the double overload splits into low and high words." },
  "asuint16": { sigs: ["uint16_t asuint16(numeric x)"], g: 1, doc: "Reinterprets the bits of 16-bit values as uint16_t, per component." },
  "atan": { sigs: ["float atan(float x)"], g: 1, doc: "Arctangent in radians, per component." },
  "atan2": { sigs: ["float atan2(float x, float y)"], g: 1, doc: "Quadrant-correct arctangent: atan2(y, x) is the angle of the vector (x, y), in radians." },
  "ceil": { sigs: ["float ceil(float x)"], g: 1, doc: "Rounds up to an integer, per component." },
  "clamp": { sigs: ["numeric clamp(numeric x, numeric min, numeric max)"], g: 1, doc: "Clamps x to [min, max], per component." },
  "clip": { sigs: ["void clip(float x)"], g: 1, doc: "Discards the pixel when any component of x is negative." },
  "cos": { sigs: ["float cos(float x)"], g: 1, doc: "Cosine of an angle in radians, per component." },
  "cosh": { sigs: ["float cosh(float x)"], g: 1, doc: "Hyperbolic cosine, per component." },
  "countbits": { sigs: ["uint countbits(int x)"], g: 1, doc: "Number of set bits, per component." },
  "cross": { sigs: ["float3 cross(float3 a, float3 b)"], doc: "Cross product of two 3-component vectors." },
  "ddx": { sigs: ["float ddx(float x)"], g: 1, doc: "Screen-space derivative with respect to x, from the pixel quad." },
  "ddx_coarse": { sigs: ["float ddx_coarse(float x)"], g: 1, doc: "ddx computed once per quad: cheaper, one value for all four pixels." },
  "ddx_fine": { sigs: ["float ddx_fine(float x)"], g: 1, doc: "ddx computed per pixel pair within the quad." },
  "ddy": { sigs: ["float ddy(float x)"], g: 1, doc: "Screen-space derivative with respect to y, from the pixel quad." },
  "ddy_coarse": { sigs: ["float ddy_coarse(float x)"], g: 1, doc: "ddy computed once per quad: cheaper, one value for all four pixels." },
  "ddy_fine": { sigs: ["float ddy_fine(float x)"], g: 1, doc: "ddy computed per pixel pair within the quad." },
  "degrees": { sigs: ["float degrees(float x)"], g: 1, doc: "Converts radians to degrees, per component." },
  "determinant": { sigs: ["float determinant(floatNxN x)"], g: 1, doc: "Determinant of a square matrix." },
  "distance": { sigs: ["float distance(floatN a, floatN b)"], g: 1, doc: "Euclidean distance between two points." },
  "dot": { sigs: ["numeric dot(numericN a, numericN b)"], g: 1, doc: "Dot product of two vectors." },
  "dot2add": { sigs: ["float dot2add(float16_t2 a, float16_t2 b, float c)"], doc: "Dot product of two half pairs, accumulated into a float." },
  "dot4add_i8packed": { sigs: ["int dot4add_i8packed(uint a, uint b, int c)"], doc: "Dot product of four packed i8 pairs, accumulated into c." },
  "dot4add_u8packed": { sigs: ["uint dot4add_u8packed(uint a, uint b, uint c)"], doc: "Dot product of four packed u8 pairs, accumulated into c." },
  "dst": { sigs: ["numeric4 dst(numeric4 a, numeric4 b)"], doc: "The D3D distance-vector helper: (1, a.y * b.y, a.z, b.w)." },
  "exp": { sigs: ["float exp(float x)"], g: 1, doc: "e raised to x, per component." },
  "exp2": { sigs: ["float exp2(float x)"], g: 1, doc: "2 raised to x, per component." },
  "ext_execution_mode": { sigs: ["void ext_execution_mode(uint mode, ...)"], ns: "vk", doc: "Emits a raw SPIR-V execution mode (inline SPIR-V)." },
  "ext_execution_mode_id": { sigs: ["void ext_execution_mode_id(uint mode, ...)"], ns: "vk", doc: "Emits a raw SPIR-V execution mode taking id operands (inline SPIR-V)." },
  "f16tof32": { sigs: ["float f16tof32(uint x)"], g: 1, doc: "Unpacks the half in a uint's low 16 bits to float, per component." },
  "f32tof16": { sigs: ["uint f32tof16(float x)"], g: 1, doc: "Packs a float to half into a uint's low 16 bits, per component." },
  "faceforward": { sigs: ["floatN faceforward(floatN N, floatN I, floatN Ng)"], g: 1, doc: "-N * sign(dot(I, Ng)): flips a normal so it faces against the incident direction." },
  "firstbithigh": { sigs: ["uint firstbithigh(int x)"], g: 1, doc: "Bit index of the most significant set bit, per component; 0xffffffff when none." },
  "firstbitlow": { sigs: ["uint firstbitlow(int x)"], g: 1, doc: "Bit index of the least significant set bit, per component; 0xffffffff when none." },
  "floor": { sigs: ["float floor(float x)"], g: 1, doc: "Rounds down to an integer, per component." },
  "fma": { sigs: ["double fma(double a, double b, double c)"], g: 1, doc: "Fused a * b + c for doubles: a single rounding." },
  "fmod": { sigs: ["float fmod(float a, float b)"], g: 1, doc: "Floating-point remainder of a / b, with a's sign, per component." },
  "frac": { sigs: ["float frac(float x)"], g: 1, doc: "Fractional part, per component." },
  "frexp": { sigs: ["float frexp(float x, out float exp)"], g: 1, doc: "Splits x into mantissa and exponent so that x = m * 2^exp, per component." },
  "fwidth": { sigs: ["float fwidth(float x)"], g: 1, doc: "abs(ddx(x)) + abs(ddy(x)), per component." },
  "isfinite": { sigs: ["bool isfinite(float x)"], g: 1, doc: "True per component where x is neither infinite nor NaN." },
  "isinf": { sigs: ["bool isinf(float x)"], g: 1, doc: "True per component where x is +/- infinity." },
  "isnan": { sigs: ["bool isnan(float x)"], g: 1, doc: "True per component where x is NaN." },
  "isnormal": { sigs: ["bool isnormal(float x)"], g: 1, doc: "True per component where x is a normal float (not zero, denormal, infinite or NaN)." },
  "ldexp": { sigs: ["float ldexp(float x, float exp)"], g: 1, doc: "x * 2^exp, per component." },
  "length": { sigs: ["float length(floatN x)"], g: 1, doc: "Euclidean length of a vector." },
  "lerp": { sigs: ["float lerp(float a, float b, float s)"], g: 1, doc: "a + s * (b - a), per component." },
  "lit": { sigs: ["float4 lit(float l, float h, float m)"], doc: "Legacy lighting helper: (1, diffuse, specular, 1) from N.L, N.H and the specular power." },
  "log": { sigs: ["float log(float x)"], g: 1, doc: "Natural logarithm, per component." },
  "log10": { sigs: ["float log10(float x)"], g: 1, doc: "Base-10 logarithm, per component." },
  "log2": { sigs: ["float log2(float x)"], g: 1, doc: "Base-2 logarithm, per component." },
  "mad": { sigs: ["numeric mad(numeric a, numeric b, numeric c)"], g: 1, doc: "a * b + c, per component; the compiler may fuse it." },
  "max": { sigs: ["numeric max(numeric a, numeric b)"], g: 1, doc: "The larger of a and b, per component." },
  "min": { sigs: ["numeric min(numeric a, numeric b)"], g: 1, doc: "The smaller of a and b, per component." },
  "modf": { sigs: ["float modf(float x, out float ip)"], g: 1, doc: "Splits x into integer and fractional parts, both carrying x's sign." },
  "msad4": { sigs: ["uint4 msad4(uint reference, uint2 source, uint4 accum)"], doc: "Masked sum of absolute differences over byte quads, accumulated: the video motion-estimation primitive." },
  "mul": { sigs: ["numeric mul(numeric a, numeric b)", "numericN mul(numeric a, numericN b)", "numericNxM mul(numeric a, numericNxM b)", "numericN mul(numericN a, numeric b)", "numeric mul(numericN a, numericN b)", "numericM mul(numericN a, numericNxM b)", "numericNxM mul(numericNxM a, numeric b)", "numericN mul(numericNxM a, numericM b)", "numericNxP mul(numericNxM a, numericMxP b)"], g: 1, doc: "Algebraic multiply for every scalar, vector and matrix pairing; vector * matrix treats the vector as a row." },
  "normalize": { sigs: ["floatN normalize(floatN x)"], g: 1, doc: "x / length(x)." },
  "or": { sigs: ["bool or(bool x, bool y)"], g: 1, doc: "Component-wise logical or (HLSL 2021: || no longer works on vectors)." },
  "pack_clamp_s8": { sigs: ["int8_t4_packed pack_clamp_s8(int4 v)"], doc: "Packs four ints to signed bytes, saturating." },
  "pack_clamp_u8": { sigs: ["uint8_t4_packed pack_clamp_u8(int4 v)"], doc: "Packs four ints to unsigned bytes, saturating." },
  "pack_s8": { sigs: ["int8_t4_packed pack_s8(int4 v)"], doc: "Packs four ints to signed bytes, truncating." },
  "pack_u8": { sigs: ["uint8_t4_packed pack_u8(int4 v)"], doc: "Packs four ints to unsigned bytes, truncating." },
  "pow": { sigs: ["float pow(float x, float y)"], g: 1, doc: "x raised to y, per component." },
  "printf": { sigs: ["void printf(string Format, ...)"], doc: "Formatted debug print, visible to tools that capture shader printf output." },
  "radians": { sigs: ["float radians(float x)"], g: 1, doc: "Converts degrees to radians, per component." },
  "rcp": { sigs: ["float rcp(float x)"], g: 1, doc: "Fast approximate reciprocal, per component." },
  "reflect": { sigs: ["floatN reflect(floatN i, floatN n)"], g: 1, doc: "i - 2 * dot(i, n) * n: reflects a direction over a normal." },
  "refract": { sigs: ["floatN refract(floatN i, floatN n, float ri)"], g: 1, doc: "Refracts direction i through a surface with normal n at index ratio ri; zero on total internal reflection." },
  "reinterpret_pointer_cast": { sigs: ["T reinterpret_pointer_cast(vk::BufferPointer ptr)"], ns: "vk", t: 1, doc: "Casts a vk::BufferPointer to a pointer to any type, unchecked." },
  "reversebits": { sigs: ["int reversebits(int x)"], g: 1, doc: "Reverses the bit order, per component." },
  "round": { sigs: ["float round(float x)"], g: 1, doc: "Rounds to the nearest integer (half away from zero), per component." },
  "rsqrt": { sigs: ["float rsqrt(float x)"], g: 1, doc: "1 / sqrt(x), per component." },
  "saturate": { sigs: ["float saturate(float x)"], g: 1, doc: "Clamps to [0, 1], per component." },
  "select": { sigs: ["any select(bool cond, any t, any f)", "SamplerState select(bool cond, SamplerState t, SamplerState f)"], g: 1, doc: "Component-wise cond ? t : f (HLSL 2021: ?: no longer works on vectors)." },
  "sign": { sigs: ["int sign(numeric x)"], g: 1, doc: "-1, 0 or 1 per component, as int." },
  "sin": { sigs: ["float sin(float x)"], g: 1, doc: "Sine of an angle in radians, per component." },
  "sincos": { sigs: ["void sincos(float x, out float s, out float c)"], g: 1, doc: "Sine and cosine of x in one call." },
  "sinh": { sigs: ["float sinh(float x)"], g: 1, doc: "Hyperbolic sine, per component." },
  "smoothstep": { sigs: ["float smoothstep(float a, float b, float x)"], g: 1, doc: "0 at a, 1 at b, smooth Hermite blend in between, per component." },
  "sqrt": { sigs: ["float sqrt(float x)"], g: 1, doc: "Square root, per component." },
  "static_pointer_cast": { sigs: ["T static_pointer_cast(vk::BufferPointer ptr)"], ns: "vk", t: 1, doc: "Casts a vk::BufferPointer to a pointer to a layout-compatible type." },
  "step": { sigs: ["float step(float a, float x)"], g: 1, doc: "1 where x >= a, else 0, per component." },
  "tan": { sigs: ["float tan(float x)"], g: 1, doc: "Tangent of an angle in radians, per component." },
  "tanh": { sigs: ["float tanh(float x)"], g: 1, doc: "Hyperbolic tangent, per component." },
  "transpose": { sigs: ["anyMxN transpose(anyNxM x)"], g: 1, doc: "Transposed matrix." },
  "trunc": { sigs: ["float trunc(float x)"], g: 1, doc: "Drops the fraction (rounds toward zero), per component." },
  "unpack_s8s16": { sigs: ["int16_t4 unpack_s8s16(int8_t4_packed pk)"], doc: "Unpacks four packed signed bytes to int16_t4." },
  "unpack_s8s32": { sigs: ["int4 unpack_s8s32(int8_t4_packed pk)"], doc: "Unpacks four packed signed bytes to int4." },
  "unpack_u8u16": { sigs: ["uint16_t4 unpack_u8u16(uint8_t4_packed pk)"], doc: "Unpacks four packed unsigned bytes to uint16_t4." },
  "unpack_u8u32": { sigs: ["uint4 unpack_u8u32(uint8_t4_packed pk)"], doc: "Unpacks four packed unsigned bytes to uint4." },
},
methods: {
  "Abort": {
    sigs: [
      { s: "void Abort()", on: ["RayQuery"] },
    ],
    doc: "Ends this ray query's traversal.",
  },
  "Append": {
    sigs: [
      { s: "void Append(T x)", on: ["TriangleStream", "PointStream", "LineStream"] },
      { s: "void Append(T value)", on: ["AppendStructuredBuffer"] },
    ],
    t: 1,
    doc: "Appends value: onto an AppendStructuredBuffer, or as the next vertex of a geometry shader stream.",
  },
  "CalculateLevelOfDetail": {
    sigs: [
      { s: "float CalculateLevelOfDetail(float x)", on: ["SampledTexture1D", "SampledTexture1DArray"] },
      { s: "float CalculateLevelOfDetail(float2 x)", on: ["SampledTexture2D", "SampledTexture2DArray"] },
      { s: "float CalculateLevelOfDetail(float3 x)", on: ["SampledTexture3D", "SampledTextureCUBE", "SampledTextureCUBEArray"] },
      { s: "float CalculateLevelOfDetail(SamplerState s, float x)", on: ["Texture1D", "Texture1DArray"] },
      { s: "float CalculateLevelOfDetail(SamplerState s, float2 x)", on: ["Texture2D", "Texture2DArray"] },
      { s: "float CalculateLevelOfDetail(SamplerState s, float3 x)", on: ["Texture3D", "TextureCube", "TextureCubeArray"] },
    ],
    doc: "The clamped mip level a Sample at x would pick.",
  },
  "CalculateLevelOfDetailUnclamped": {
    sigs: [
      { s: "float CalculateLevelOfDetailUnclamped(float x)", on: ["SampledTexture1D", "SampledTexture1DArray"] },
      { s: "float CalculateLevelOfDetailUnclamped(float2 x)", on: ["SampledTexture2D", "SampledTexture2DArray"] },
      { s: "float CalculateLevelOfDetailUnclamped(float3 x)", on: ["SampledTexture3D", "SampledTextureCUBE", "SampledTextureCUBEArray"] },
      { s: "float CalculateLevelOfDetailUnclamped(SamplerState s, float x)", on: ["Texture1D", "Texture1DArray"] },
      { s: "float CalculateLevelOfDetailUnclamped(SamplerState s, float2 x)", on: ["Texture2D", "Texture2DArray"] },
      { s: "float CalculateLevelOfDetailUnclamped(SamplerState s, float3 x)", on: ["Texture3D", "TextureCube", "TextureCubeArray"] },
    ],
    doc: "The mip level a Sample at x would pick, before clamping to the resource's mip range.",
  },
  "CandidateClusterID": {
    sigs: [
      { s: "uint CandidateClusterID()", on: ["RayQuery"] },
    ],
    sm: "6.10",
    doc: "Cluster ID of the candidate hit.",
  },
  "CandidateGeometryIndex": {
    sigs: [
      { s: "uint CandidateGeometryIndex()", on: ["RayQuery"] },
    ],
    doc: "Geometry index of the candidate hit.",
  },
  "CandidateInstanceContributionToHitGroupIndex": {
    sigs: [
      { s: "uint CandidateInstanceContributionToHitGroupIndex()", on: ["RayQuery"] },
    ],
    doc: "InstanceContributionToHitGroupIndex of the candidate hit's instance.",
  },
  "CandidateInstanceID": {
    sigs: [
      { s: "uint CandidateInstanceID()", on: ["RayQuery"] },
    ],
    doc: "User-supplied instance ID of the candidate hit.",
  },
  "CandidateInstanceIndex": {
    sigs: [
      { s: "uint CandidateInstanceIndex()", on: ["RayQuery"] },
    ],
    doc: "Instance index of the candidate hit.",
  },
  "CandidateObjectRayDirection": {
    sigs: [
      { s: "float3 CandidateObjectRayDirection()", on: ["RayQuery"] },
    ],
    doc: "Ray direction in object space of the candidate hit's instance.",
  },
  "CandidateObjectRayOrigin": {
    sigs: [
      { s: "float3 CandidateObjectRayOrigin()", on: ["RayQuery"] },
    ],
    doc: "Ray origin in object space of the candidate hit's instance.",
  },
  "CandidateObjectToWorld3x4": {
    sigs: [
      { s: "float3x4 CandidateObjectToWorld3x4()", on: ["RayQuery"] },
    ],
    doc: "Object-to-world transform of the candidate hit's instance, as 3x4.",
  },
  "CandidateObjectToWorld4x3": {
    sigs: [
      { s: "float4x3 CandidateObjectToWorld4x3()", on: ["RayQuery"] },
    ],
    doc: "Object-to-world transform of the candidate hit's instance, as 4x3.",
  },
  "CandidatePrimitiveIndex": {
    sigs: [
      { s: "uint CandidatePrimitiveIndex()", on: ["RayQuery"] },
    ],
    doc: "Primitive index of the candidate hit.",
  },
  "CandidateProceduralPrimitiveNonOpaque": {
    sigs: [
      { s: "bool CandidateProceduralPrimitiveNonOpaque()", on: ["RayQuery"] },
    ],
    doc: "True when the candidate procedural primitive is non-opaque.",
  },
  "CandidateTriangleBarycentrics": {
    sigs: [
      { s: "float2 CandidateTriangleBarycentrics()", on: ["RayQuery"] },
    ],
    doc: "Barycentrics of the candidate triangle hit.",
  },
  "CandidateTriangleFrontFace": {
    sigs: [
      { s: "bool CandidateTriangleFrontFace()", on: ["RayQuery"] },
    ],
    doc: "True when the candidate triangle is front facing.",
  },
  "CandidateTriangleObjectPositions": {
    sigs: [
      { s: "BuiltInTrianglePositions CandidateTriangleObjectPositions()", on: ["RayQuery"] },
    ],
    sm: "6.10",
    doc: "Object-space vertex positions of the candidate triangle.",
  },
  "CandidateTriangleRayT": {
    sigs: [
      { s: "float CandidateTriangleRayT()", on: ["RayQuery"] },
    ],
    doc: "t of the candidate triangle hit.",
  },
  "CandidateType": {
    sigs: [
      { s: "uint CandidateType()", on: ["RayQuery"] },
    ],
    doc: "What the candidate is: a non-opaque triangle or a procedural primitive.",
  },
  "CandidateWorldToObject3x4": {
    sigs: [
      { s: "float3x4 CandidateWorldToObject3x4()", on: ["RayQuery"] },
    ],
    doc: "World-to-object transform of the candidate hit's instance, as 3x4.",
  },
  "CandidateWorldToObject4x3": {
    sigs: [
      { s: "float4x3 CandidateWorldToObject4x3()", on: ["RayQuery"] },
    ],
    doc: "World-to-object transform of the candidate hit's instance, as 4x3.",
  },
  "CommitNonOpaqueTriangleHit": {
    sigs: [
      { s: "void CommitNonOpaqueTriangleHit()", on: ["RayQuery"] },
    ],
    doc: "Commits the candidate non-opaque triangle as the current closest hit.",
  },
  "CommitProceduralPrimitiveHit": {
    sigs: [
      { s: "void CommitProceduralPrimitiveHit(float t)", on: ["RayQuery"] },
    ],
    doc: "Commits the candidate procedural primitive at t as the current closest hit.",
  },
  "CommittedClusterID": {
    sigs: [
      { s: "uint CommittedClusterID()", on: ["RayQuery"] },
    ],
    sm: "6.10",
    doc: "Cluster ID of the committed hit.",
  },
  "CommittedGeometryIndex": {
    sigs: [
      { s: "uint CommittedGeometryIndex()", on: ["RayQuery"] },
    ],
    doc: "Geometry index of the committed hit.",
  },
  "CommittedInstanceContributionToHitGroupIndex": {
    sigs: [
      { s: "uint CommittedInstanceContributionToHitGroupIndex()", on: ["RayQuery"] },
    ],
    doc: "InstanceContributionToHitGroupIndex of the committed hit's instance.",
  },
  "CommittedInstanceID": {
    sigs: [
      { s: "uint CommittedInstanceID()", on: ["RayQuery"] },
    ],
    doc: "User-supplied instance ID of the committed hit.",
  },
  "CommittedInstanceIndex": {
    sigs: [
      { s: "uint CommittedInstanceIndex()", on: ["RayQuery"] },
    ],
    doc: "Instance index of the committed hit.",
  },
  "CommittedObjectRayDirection": {
    sigs: [
      { s: "float3 CommittedObjectRayDirection()", on: ["RayQuery"] },
    ],
    doc: "Ray direction in object space of the committed hit's instance.",
  },
  "CommittedObjectRayOrigin": {
    sigs: [
      { s: "float3 CommittedObjectRayOrigin()", on: ["RayQuery"] },
    ],
    doc: "Ray origin in object space of the committed hit's instance.",
  },
  "CommittedObjectToWorld3x4": {
    sigs: [
      { s: "float3x4 CommittedObjectToWorld3x4()", on: ["RayQuery"] },
    ],
    doc: "Object-to-world transform of the committed hit's instance, as 3x4.",
  },
  "CommittedObjectToWorld4x3": {
    sigs: [
      { s: "float4x3 CommittedObjectToWorld4x3()", on: ["RayQuery"] },
    ],
    doc: "Object-to-world transform of the committed hit's instance, as 4x3.",
  },
  "CommittedPrimitiveIndex": {
    sigs: [
      { s: "uint CommittedPrimitiveIndex()", on: ["RayQuery"] },
    ],
    doc: "Primitive index of the committed hit.",
  },
  "CommittedRayT": {
    sigs: [
      { s: "float CommittedRayT()", on: ["RayQuery"] },
    ],
    doc: "t of the committed hit.",
  },
  "CommittedStatus": {
    sigs: [
      { s: "uint CommittedStatus()", on: ["RayQuery"] },
    ],
    doc: "What the committed hit is: nothing, a triangle, or a procedural primitive.",
  },
  "CommittedTriangleBarycentrics": {
    sigs: [
      { s: "float2 CommittedTriangleBarycentrics()", on: ["RayQuery"] },
    ],
    doc: "Barycentrics of the committed triangle hit.",
  },
  "CommittedTriangleFrontFace": {
    sigs: [
      { s: "bool CommittedTriangleFrontFace()", on: ["RayQuery"] },
    ],
    doc: "True when the committed triangle is front facing.",
  },
  "CommittedTriangleObjectPositions": {
    sigs: [
      { s: "BuiltInTrianglePositions CommittedTriangleObjectPositions()", on: ["RayQuery"] },
    ],
    sm: "6.10",
    doc: "Object-space vertex positions of the committed triangle.",
  },
  "CommittedWorldToObject3x4": {
    sigs: [
      { s: "float3x4 CommittedWorldToObject3x4()", on: ["RayQuery"] },
    ],
    doc: "World-to-object transform of the committed hit's instance, as 3x4.",
  },
  "CommittedWorldToObject4x3": {
    sigs: [
      { s: "float4x3 CommittedWorldToObject4x3()", on: ["RayQuery"] },
    ],
    doc: "World-to-object transform of the committed hit's instance, as 4x3.",
  },
  "Consume": {
    sigs: [
      { s: "T Consume()", on: ["ConsumeStructuredBuffer"] },
    ],
    t: 1,
    doc: "Pops and returns the element under the buffer's hidden counter.",
  },
  "Count": {
    sigs: [
      { s: "uint Count()", on: ["EmptyNodeInput", "GroupNodeInputRecords", "RWGroupNodeInputRecords"] },
    ],
    doc: "How many input records or items this node input holds.",
  },
  "DecrementCounter": {
    sigs: [
      { s: "uint DecrementCounter()", on: ["RWStructuredBuffer", "RasterizerOrderedStructuredBuffer"] },
    ],
    doc: "Atomically drops the buffer's hidden counter; returns the value after the decrement.",
  },
  "FinishedCrossGroupSharing": {
    sigs: [
      { s: "bool FinishedCrossGroupSharing()", on: ["RWDispatchNodeInputRecord"] },
    ],
    doc: "True on the last group to finish with this shared input record.",
  },
  "FromRayQuery": {
    sigs: [
      { s: "static dx::HitObject FromRayQuery(RayQuery rq)", on: ["HitObject"] },
      { s: "static dx::HitObject FromRayQuery(RayQuery rq, uint HitKind, T Attributes)", on: ["HitObject"] },
    ],
    sm: "6.9",
    t: 1,
    doc: "A hit object from a ray query's committed hit; the overload with Attributes overrides the hit kind.",
  },
  "Gather": {
    sigs: [
      { s: "T4 Gather(float2 x)", on: ["SampledTexture2D"] },
      { s: "T4 Gather(float3 x)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 Gather(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 Gather(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 Gather(SamplerState s, float3 x)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 Gather(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 12,
    doc: "The four texels bilinear filtering at x would blend, one per component: the red channel.",
  },
  "GatherAlpha": {
    sigs: [
      { s: "T4 GatherAlpha(float2 x)", on: ["SampledTexture2D"] },
      { s: "T4 GatherAlpha(float3 x)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherAlpha(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherAlpha(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 GatherAlpha(SamplerState s, float3 x)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherAlpha(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "The four texels bilinear filtering at x would blend: the alpha channel.",
  },
  "GatherBlue": {
    sigs: [
      { s: "T4 GatherBlue(float2 x)", on: ["SampledTexture2D"] },
      { s: "T4 GatherBlue(float3 x)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherBlue(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherBlue(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 GatherBlue(SamplerState s, float3 x)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherBlue(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "The four texels bilinear filtering at x would blend: the blue channel.",
  },
  "GatherCmp": {
    sigs: [
      { s: "T4 GatherCmp(float2 x, float compareValue)", on: ["SampledTexture2D"] },
      { s: "T4 GatherCmp(float3 x, float compareValue)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherCmp(float4 x, float compareValue)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherCmp(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture2D"] },
      { s: "T4 GatherCmp(SamplerComparisonState s, float3 x, float compareValue)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherCmp(SamplerComparisonState s, float4 x, float compareValue)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 12,
    doc: "Gathers the four texels and compares each against compareValue, like SampleCmp.",
  },
  "GatherCmpAlpha": {
    sigs: [
      { s: "T4 GatherCmpAlpha(float2 x, float compareValue)", on: ["SampledTexture2D"] },
      { s: "T4 GatherCmpAlpha(float3 x, float compareValue)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherCmpAlpha(float4 x, float compareValue)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherCmpAlpha(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture2D"] },
      { s: "T4 GatherCmpAlpha(SamplerComparisonState s, float3 x, float compareValue)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherCmpAlpha(SamplerComparisonState s, float4 x, float compareValue)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "GatherCmp on the alpha channel.",
  },
  "GatherCmpBlue": {
    sigs: [
      { s: "T4 GatherCmpBlue(float2 x, float compareValue)", on: ["SampledTexture2D"] },
      { s: "T4 GatherCmpBlue(float3 x, float compareValue)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherCmpBlue(float4 x, float compareValue)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherCmpBlue(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture2D"] },
      { s: "T4 GatherCmpBlue(SamplerComparisonState s, float3 x, float compareValue)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherCmpBlue(SamplerComparisonState s, float4 x, float compareValue)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "GatherCmp on the blue channel.",
  },
  "GatherCmpGreen": {
    sigs: [
      { s: "T4 GatherCmpGreen(float2 x, float compareValue)", on: ["SampledTexture2D"] },
      { s: "T4 GatherCmpGreen(float3 x, float compareValue)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherCmpGreen(float4 x, float compareValue)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherCmpGreen(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture2D"] },
      { s: "T4 GatherCmpGreen(SamplerComparisonState s, float3 x, float compareValue)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherCmpGreen(SamplerComparisonState s, float4 x, float compareValue)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "GatherCmp on the green channel.",
  },
  "GatherCmpRed": {
    sigs: [
      { s: "T4 GatherCmpRed(float2 x, float compareValue)", on: ["SampledTexture2D"] },
      { s: "T4 GatherCmpRed(float3 x, float compareValue)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherCmpRed(float4 x, float compareValue)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherCmpRed(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture2D"] },
      { s: "T4 GatherCmpRed(SamplerComparisonState s, float3 x, float compareValue)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherCmpRed(SamplerComparisonState s, float4 x, float compareValue)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "GatherCmp on the red channel.",
  },
  "GatherGreen": {
    sigs: [
      { s: "T4 GatherGreen(float2 x)", on: ["SampledTexture2D"] },
      { s: "T4 GatherGreen(float3 x)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherGreen(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherGreen(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 GatherGreen(SamplerState s, float3 x)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherGreen(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "The four texels bilinear filtering at x would blend: the green channel.",
  },
  "GatherRaw": {
    sigs: [
      { s: "T4 GatherRaw(float3 x)", on: ["SampledTexture2DArray"] },
      { s: "T4 GatherRaw(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 GatherRaw(SamplerState s, float3 x)", on: ["Texture2DArray"] },
    ],
    t: 1,
    more: 6,
    doc: "Gathers the four texels as raw bits, without format conversion.",
  },
  "GatherRed": {
    sigs: [
      { s: "T4 GatherRed(float2 x)", on: ["SampledTexture2D"] },
      { s: "T4 GatherRed(float3 x)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "T4 GatherRed(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T4 GatherRed(SamplerState s, float2 x)", on: ["Texture2D"] },
      { s: "T4 GatherRed(SamplerState s, float3 x)", on: ["Texture2DArray", "TextureCube"] },
      { s: "T4 GatherRed(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 20,
    doc: "The four texels bilinear filtering at x would blend: the red channel.",
  },
  "GetAttributes": {
    sigs: [
      { s: "void GetAttributes(out T Attributes)", on: ["HitObject"] },
    ],
    sm: "6.9",
    t: 1,
    doc: "Copies the hit object's intersection attributes out.",
  },
  "GetBufferContents": {
    sigs: [
      { s: "T GetBufferContents()", on: ["BufferPointer"] },
    ],
    t: 1,
    doc: "Dereferences the vk::BufferPointer to its contents.",
  },
  "GetClusterID": {
    sigs: [
      { s: "uint GetClusterID()", on: ["HitObject"] },
    ],
    sm: "6.10",
    doc: "Cluster ID of the hit object's hit.",
  },
  "GetDimensions": {
    sigs: [
      { s: "void GetDimensions(out uint width)", on: ["Texture1D", "Buffer", "RWTexture1D", "RasterizerOrderedTexture1D", "RWBuffer", "RasterizerOrderedBuffer", "ByteAddressBuffer", "RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer", "SampledTexture1D"] },
      { s: "void GetDimensions(out float width)", on: ["Texture1D", "RWTexture1D", "RasterizerOrderedTexture1D", "SampledTexture1D"] },
      { s: "void GetDimensions(out uint width, out uint height)", on: ["Texture2D", "TextureCube", "RWTexture2D", "RasterizerOrderedTexture2D", "SampledTexture2D", "SampledTextureCUBE"] },
      { s: "void GetDimensions(out uint count, out uint stride)", on: ["StructuredBuffer", "RWStructuredBuffer", "RasterizerOrderedStructuredBuffer", "AppendStructuredBuffer", "ConsumeStructuredBuffer"] },
      { s: "void GetDimensions(out uint width, out uint elements)", on: ["Texture1DArray", "RWTexture1DArray", "RasterizerOrderedTexture1DArray", "SampledTexture1DArray"] },
      { s: "void GetDimensions(out float width, out float height)", on: ["Texture2D", "TextureCube", "RWTexture2D", "RasterizerOrderedTexture2D", "SampledTexture2D", "SampledTextureCUBE"] },
      { s: "void GetDimensions(out float width, out float elements)", on: ["Texture1DArray", "RWTexture1DArray", "RasterizerOrderedTexture1DArray", "SampledTexture1DArray"] },
      { s: "void GetDimensions(uint x, out uint width, out uint levels)", on: ["Texture1D", "SampledTexture1D"] },
      { s: "void GetDimensions(uint x, out float width, out float levels)", on: ["Texture1D", "SampledTexture1D"] },
      { s: "void GetDimensions(out uint width, out uint height, out uint depth)", on: ["Texture3D", "RWTexture3D", "RasterizerOrderedTexture3D", "SampledTexture3D"] },
    ],
    more: 15,
    doc: "The resource's dimensions, plus mip, sample, element or stride counts where it has them.",
  },
  "GetGeometryIndex": {
    sigs: [
      { s: "uint GetGeometryIndex()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Geometry index of the hit object's hit.",
  },
  "GetGroupNodeOutputRecords": {
    sigs: [
      { s: "GroupNodeOutputRecords GetGroupNodeOutputRecords(uint numRecords)", on: ["NodeOutput"] },
    ],
    doc: "Allocates group-shared output records for this node output.",
  },
  "GetHitKind": {
    sigs: [
      { s: "uint GetHitKind()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Hit kind of the hit object's hit.",
  },
  "GetInstanceID": {
    sigs: [
      { s: "uint GetInstanceID()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "User-supplied instance ID of the hit object's hit.",
  },
  "GetInstanceIndex": {
    sigs: [
      { s: "uint GetInstanceIndex()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Instance index of the hit object's hit.",
  },
  "GetObjectRayDirection": {
    sigs: [
      { s: "float3 GetObjectRayDirection()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The hit object's ray direction in object space.",
  },
  "GetObjectRayOrigin": {
    sigs: [
      { s: "float3 GetObjectRayOrigin()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The hit object's ray origin in object space.",
  },
  "GetObjectToWorld3x4": {
    sigs: [
      { s: "float3x4 GetObjectToWorld3x4()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Object-to-world transform of the hit object's instance, as 3x4.",
  },
  "GetObjectToWorld4x3": {
    sigs: [
      { s: "float4x3 GetObjectToWorld4x3()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Object-to-world transform of the hit object's instance, as 4x3.",
  },
  "GetPrimitiveIndex": {
    sigs: [
      { s: "uint GetPrimitiveIndex()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Primitive index of the hit object's hit.",
  },
  "GetRayFlags": {
    sigs: [
      { s: "uint GetRayFlags()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The flags of the hit object's ray.",
  },
  "GetRayTCurrent": {
    sigs: [
      { s: "float GetRayTCurrent()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "t of the hit object's hit (or the ray extent for a miss).",
  },
  "GetRayTMin": {
    sigs: [
      { s: "float GetRayTMin()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Minimum t of the hit object's ray.",
  },
  "GetSamplePosition": {
    sigs: [
      { s: "float2 GetSamplePosition(int s)", on: ["Texture2DMS", "Texture2DMSArray", "RWTexture2DMS", "RWTexture2DMSArray", "SampledTexture2DMS", "SampledTexture2DMSArray"] },
    ],
    doc: "Position of MSAA sample s within the pixel.",
  },
  "GetShaderTableIndex": {
    sigs: [
      { s: "uint GetShaderTableIndex()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The shader table record this hit object would invoke.",
  },
  "GetThreadNodeOutputRecords": {
    sigs: [
      { s: "ThreadNodeOutputRecords GetThreadNodeOutputRecords(uint numRecords)", on: ["NodeOutput"] },
    ],
    doc: "Allocates per-thread output records for this node output.",
  },
  "GetWorldRayDirection": {
    sigs: [
      { s: "float3 GetWorldRayDirection()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The hit object's ray direction in world space.",
  },
  "GetWorldRayOrigin": {
    sigs: [
      { s: "float3 GetWorldRayOrigin()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "The hit object's ray origin in world space.",
  },
  "GetWorldToObject3x4": {
    sigs: [
      { s: "float3x4 GetWorldToObject3x4()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "World-to-object transform of the hit object's instance, as 3x4.",
  },
  "GetWorldToObject4x3": {
    sigs: [
      { s: "float4x3 GetWorldToObject4x3()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "World-to-object transform of the hit object's instance, as 4x3.",
  },
  "GroupIncrementOutputCount": {
    sigs: [
      { s: "void GroupIncrementOutputCount(uint count)", on: ["EmptyNodeOutput"] },
    ],
    doc: "Group-uniform bump of this empty output's record count.",
  },
  "IncrementCounter": {
    sigs: [
      { s: "uint IncrementCounter()", on: ["RWStructuredBuffer", "RasterizerOrderedStructuredBuffer"] },
    ],
    doc: "Atomically bumps the buffer's hidden counter; returns the value before the increment.",
  },
  "InterlockedAdd": {
    sigs: [
      { s: "void InterlockedAdd(uint byteOffset, uint value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedAdd(uint byteOffset, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic add at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedAdd64": {
    sigs: [
      { s: "void InterlockedAdd64(uint byteOffset, uint64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedAdd64(uint byteOffset, uint64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic add at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedAnd": {
    sigs: [
      { s: "void InterlockedAnd(uint byteOffset, uint value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedAnd(uint byteOffset, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic bitwise and at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedAnd64": {
    sigs: [
      { s: "void InterlockedAnd64(uint byteOffset, uint64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedAnd64(uint byteOffset, uint64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic bitwise and at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedCompareExchange": {
    sigs: [
      { s: "void InterlockedCompareExchange(uint byteOffset, uint compare, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic compare-and-swap at a byte offset; returns the prior value.",
  },
  "InterlockedCompareExchange64": {
    sigs: [
      { s: "void InterlockedCompareExchange64(uint byteOffset, uint64_t compare, uint64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic compare-and-swap at a byte offset; returns the prior value.",
  },
  "InterlockedCompareExchangeFloatBitwise": {
    sigs: [
      { s: "void InterlockedCompareExchangeFloatBitwise(uint byteOffest, float compare, float value, out float original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic float compare-and-swap at a byte offset, comparing raw bits.",
  },
  "InterlockedCompareStore": {
    sigs: [
      { s: "void InterlockedCompareStore(uint byteOffset, uint compare, uint value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomically stores value at a byte offset when the current value equals compare.",
  },
  "InterlockedCompareStore64": {
    sigs: [
      { s: "void InterlockedCompareStore64(uint byteOffset, uint64_t compare, uint64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic compare-and-store at a byte offset.",
  },
  "InterlockedCompareStoreFloatBitwise": {
    sigs: [
      { s: "void InterlockedCompareStoreFloatBitwise(uint byteOffest, float compare, float value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic float compare-and-store at a byte offset, comparing raw bits.",
  },
  "InterlockedExchange": {
    sigs: [
      { s: "void InterlockedExchange(uint byteOffset, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic swap at a byte offset; returns the prior value.",
  },
  "InterlockedExchange64": {
    sigs: [
      { s: "void InterlockedExchange64(uint byteOffset, int64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic swap at a byte offset; returns the prior value.",
  },
  "InterlockedExchangeFloat": {
    sigs: [
      { s: "void InterlockedExchangeFloat(uint byteOffest, float value, out float original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic float swap at a byte offset; returns the prior value.",
  },
  "InterlockedMax": {
    sigs: [
      { s: "void InterlockedMax(uint byteOffset, int value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedMax(uint byteOffset, int value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic max at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedMax64": {
    sigs: [
      { s: "void InterlockedMax64(uint byteOffset, int64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedMax64(uint byteOffset, int64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic max at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedMin": {
    sigs: [
      { s: "void InterlockedMin(uint byteOffset, int value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedMin(uint byteOffset, int value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic min at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedMin64": {
    sigs: [
      { s: "void InterlockedMin64(uint byteOffset, int64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedMin64(uint byteOffset, int64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic min at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedOr": {
    sigs: [
      { s: "void InterlockedOr(uint byteOffset, uint value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedOr(uint byteOffset, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic bitwise or at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedOr64": {
    sigs: [
      { s: "void InterlockedOr64(uint byteOffset, uint64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedOr64(uint byteOffset, uint64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic bitwise or at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedXor": {
    sigs: [
      { s: "void InterlockedXor(uint byteOffset, uint value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedXor(uint byteOffset, uint value, out uint original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Atomic bitwise xor at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "InterlockedXor64": {
    sigs: [
      { s: "void InterlockedXor64(uint byteOffset, uint64_t value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "void InterlockedXor64(uint byteOffset, uint64_t value, out int64_t original)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "64-bit atomic bitwise xor at a byte offset; the overload with `original` also returns the value that was there before.",
  },
  "Invoke": {
    sigs: [
      { s: "static void Invoke(dx::HitObject ho, inout T Payload)", on: ["HitObject"] },
    ],
    sm: "6.9",
    t: 1,
    doc: "Runs the closest-hit or miss shader the hit object refers to, with the payload.",
  },
  "IsHit": {
    sigs: [
      { s: "bool IsHit()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "True when this hit object is a hit.",
  },
  "IsMiss": {
    sigs: [
      { s: "bool IsMiss()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "True when this hit object is a miss.",
  },
  "IsNop": {
    sigs: [
      { s: "bool IsNop()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "True when this hit object is neither hit nor miss.",
  },
  "IsValid": {
    sigs: [
      { s: "bool IsValid()", on: ["NodeOutput", "EmptyNodeOutput"] },
    ],
    doc: "True when this node output is bound.",
  },
  "Load": {
    sigs: [
      { s: "T Load(int x)", on: ["Buffer", "RWTexture1D", "RasterizerOrderedTexture1D", "RWBuffer", "RasterizerOrderedBuffer", "StructuredBuffer", "RWStructuredBuffer", "RasterizerOrderedStructuredBuffer"] },
      { s: "T Load(int2 x)", on: ["Texture1D", "RWTexture1DArray", "RasterizerOrderedTexture1DArray", "RWTexture2D", "RasterizerOrderedTexture2D", "SampledTexture1D"] },
      { s: "T Load(int3 x)", on: ["Texture1DArray", "Texture2D", "RWTexture2DArray", "RasterizerOrderedTexture2DArray", "RWTexture3D", "RasterizerOrderedTexture3D", "SampledTexture1DArray", "SampledTexture2D"] },
      { s: "T Load(int4 x)", on: ["Texture2DArray", "Texture3D", "SampledTexture2DArray", "SampledTexture3D"] },
      { s: "T Load(uint byteOffset)", on: ["ByteAddressBuffer", "RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
      { s: "T Load(int2 x, int s)", on: ["Texture2DMS", "RWTexture2DMS", "SampledTexture2DMS"] },
      { s: "T Load(int3 x, int s)", on: ["Texture2DMSArray", "RWTexture2DMSArray", "SampledTexture2DMSArray"] },
    ],
    t: 1,
    more: 20,
    doc: "Reads an element by integer address (texel coordinate, byte offset or structured index); the status overload reports residency.",
  },
  "Load2": {
    sigs: [
      { s: "uint2 Load2(uint byteOffset)", on: ["ByteAddressBuffer", "RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    more: 1,
    doc: "Reads two consecutive uints at a byte offset.",
  },
  "Load3": {
    sigs: [
      { s: "uint3 Load3(uint byteOffset)", on: ["ByteAddressBuffer", "RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    more: 1,
    doc: "Reads three consecutive uints at a byte offset.",
  },
  "Load4": {
    sigs: [
      { s: "uint4 Load4(uint byteOffset)", on: ["ByteAddressBuffer", "RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    more: 1,
    doc: "Reads four consecutive uints at a byte offset.",
  },
  "LoadLocalRootTableConstant": {
    sigs: [
      { s: "uint LoadLocalRootTableConstant(uint RootConstantOffsetInBytes)", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Reads a local root table constant of the hit object's shader record.",
  },
  "MakeMiss": {
    sigs: [
      { s: "static dx::HitObject MakeMiss(uint RayFlags, uint MissShaderIndex, RayDesc Ray)", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "A hit object describing a miss of the given ray.",
  },
  "MakeNop": {
    sigs: [
      { s: "static dx::HitObject MakeNop()", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "A hit object that is neither hit nor miss (no-op for MaybeReorderThread and Invoke).",
  },
  "OutputComplete": {
    sigs: [
      { s: "void OutputComplete()", on: ["ThreadNodeOutputRecords", "GroupNodeOutputRecords"] },
    ],
    doc: "Marks the allocated output records as done and launchable.",
  },
  "Proceed": {
    sigs: [
      { s: "bool Proceed()", on: ["RayQuery"] },
    ],
    doc: "Advances traversal; true while a candidate hit needs this shader's evaluation.",
  },
  "RayFlags": {
    sigs: [
      { s: "uint RayFlags()", on: ["RayQuery"] },
    ],
    doc: "The flags this query's ray was traced with.",
  },
  "RayTMin": {
    sigs: [
      { s: "float RayTMin()", on: ["RayQuery"] },
    ],
    doc: "This query's ray minimum t.",
  },
  "RestartStrip": {
    sigs: [
      { s: "void RestartStrip()", on: ["TriangleStream", "PointStream", "LineStream"] },
    ],
    doc: "Ends the current strip, so the next Append starts a new one.",
  },
  "Sample": {
    sigs: [
      { s: "T Sample(float x)", on: ["SampledTexture1D"] },
      { s: "T Sample(float2 x)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "T Sample(float3 x)", on: ["SampledTexture2DArray", "SampledTexture3D", "SampledTextureCUBE"] },
      { s: "T Sample(float4 x)", on: ["SampledTextureCUBEArray"] },
      { s: "T Sample(SamplerState s, float x)", on: ["Texture1D"] },
      { s: "T Sample(SamplerState s, float2 x)", on: ["Texture1DArray", "Texture2D"] },
      { s: "T Sample(SamplerState s, float3 x)", on: ["Texture2DArray", "Texture3D", "TextureCube"] },
      { s: "T Sample(SamplerState s, float4 x)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 38,
    doc: "Samples with filtering at coordinates x.",
  },
  "SampleBias": {
    sigs: [
      { s: "T SampleBias(float x, float bias)", on: ["SampledTexture1D"] },
      { s: "T SampleBias(float2 x, float bias)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "T SampleBias(float3 x, float bias)", on: ["SampledTexture2DArray", "SampledTexture3D", "SampledTextureCUBE"] },
      { s: "T SampleBias(float4 x, float bias)", on: ["SampledTextureCUBEArray"] },
      { s: "T SampleBias(SamplerState s, float x, float bias)", on: ["Texture1D"] },
      { s: "T SampleBias(SamplerState s, float2 x, float bias)", on: ["Texture1DArray", "Texture2D"] },
      { s: "T SampleBias(SamplerState s, float3 x, float bias)", on: ["Texture2DArray", "Texture3D", "TextureCube"] },
      { s: "T SampleBias(SamplerState s, float4 x, float bias)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 38,
    doc: "Samples with the implicit mip level biased.",
  },
  "SampleCmp": {
    sigs: [
      { s: "float SampleCmp(float3 x, float c)", on: ["SampledTextureCUBE"] },
      { s: "float SampleCmp(float4 x, float c)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmp(float x, float compareValue)", on: ["SampledTexture1D"] },
      { s: "float SampleCmp(float2 x, float compareValue)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "float SampleCmp(float3 x, float compareValue)", on: ["SampledTexture2DArray"] },
      { s: "float SampleCmp(float4 x, float compareValue, float clamp)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmp(SamplerComparisonState s, float3 x, float c)", on: ["TextureCube"] },
      { s: "float SampleCmp(SamplerComparisonState s, float4 x, float c)", on: ["TextureCubeArray"] },
      { s: "float SampleCmp(SamplerComparisonState s, float x, float compareValue)", on: ["Texture1D"] },
      { s: "float SampleCmp(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture1DArray", "Texture2D"] },
    ],
    more: 32,
    doc: "Samples through a comparison sampler: every fetched texel compares against compareValue, the results filter to 0..1.",
  },
  "SampleCmpBias": {
    sigs: [
      { s: "float SampleCmpBias(float x, float compareValue, float bias)", on: ["SampledTexture1D"] },
      { s: "float SampleCmpBias(float2 x, float compareValue, float bias)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "float SampleCmpBias(float3 x, float compareValue, float bias)", on: ["SampledTexture2DArray", "SampledTextureCUBE"] },
      { s: "float SampleCmpBias(float4 x, float compareValue, float bias)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmpBias(SamplerComparisonState s, float x, float compareValue, float bias)", on: ["Texture1D"] },
      { s: "float SampleCmpBias(SamplerComparisonState s, float2 x, float compareValue, float bias)", on: ["Texture1DArray", "Texture2D"] },
      { s: "float SampleCmpBias(SamplerComparisonState s, float3 x, float compareValue, float bias)", on: ["Texture2DArray", "TextureCube"] },
      { s: "float SampleCmpBias(SamplerComparisonState s, float4 x, float compareValue, float bias)", on: ["TextureCubeArray"] },
    ],
    more: 32,
    doc: "SampleCmp with the implicit mip level biased.",
  },
  "SampleCmpGrad": {
    sigs: [
      { s: "float SampleCmpGrad(float x, float compareValue, float ddx, float ddy)", on: ["SampledTexture1D"] },
      { s: "float SampleCmpGrad(float2 x, float compareValue, float ddx, float ddy)", on: ["SampledTexture1DArray"] },
      { s: "float SampleCmpGrad(float2 x, float compareValue, float2 ddx, float2 ddy)", on: ["SampledTexture2D"] },
      { s: "float SampleCmpGrad(float3 x, float compareValue, float2 ddx, float2 ddy)", on: ["SampledTexture2DArray"] },
      { s: "float SampleCmpGrad(float3 x, float compareValue, float3 ddx, float3 ddy)", on: ["SampledTextureCUBE"] },
      { s: "float SampleCmpGrad(SamplerComparisonState s, float x, float compareValue, float ddx, float ddy)", on: ["Texture1D"] },
      { s: "float SampleCmpGrad(SamplerComparisonState s, float2 x, float compareValue, float ddx, float ddy)", on: ["Texture1DArray"] },
      { s: "float SampleCmpGrad(SamplerComparisonState s, float2 x, float compareValue, float2 ddx, float2 ddy)", on: ["Texture2D"] },
      { s: "float SampleCmpGrad(SamplerComparisonState s, float3 x, float compareValue, float2 ddx, float2 ddy)", on: ["Texture2DArray"] },
      { s: "float SampleCmpGrad(SamplerComparisonState s, float3 x, float compareValue, float3 ddx, float3 ddy)", on: ["TextureCube"] },
    ],
    more: 31,
    doc: "SampleCmp with explicit derivatives.",
  },
  "SampleCmpLevel": {
    sigs: [
      { s: "float SampleCmpLevel(float3 x, float c, float lod)", on: ["SampledTextureCUBE"] },
      { s: "float SampleCmpLevel(float4 x, float c, float lod)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmpLevel(float x, float compareValue, float lod)", on: ["SampledTexture1D"] },
      { s: "float SampleCmpLevel(float2 x, float compareValue, float lod)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "float SampleCmpLevel(float3 x, float compareValue, float lod)", on: ["SampledTexture2DArray"] },
      { s: "float SampleCmpLevel(SamplerComparisonState s, float3 x, float c, float lod)", on: ["TextureCube"] },
      { s: "float SampleCmpLevel(SamplerComparisonState s, float4 x, float c, float lod)", on: ["TextureCubeArray"] },
      { s: "float SampleCmpLevel(float4 x, float compareValue, float lod, out uint status)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmpLevel(SamplerComparisonState s, float x, float compareValue, float lod)", on: ["Texture1D"] },
      { s: "float SampleCmpLevel(SamplerComparisonState s, float2 x, float compareValue, float lod)", on: ["Texture1DArray", "Texture2D"] },
    ],
    more: 20,
    doc: "SampleCmp at an explicit mip level.",
  },
  "SampleCmpLevelZero": {
    sigs: [
      { s: "float SampleCmpLevelZero(float3 x, float c)", on: ["SampledTextureCUBE"] },
      { s: "float SampleCmpLevelZero(float4 x, float c)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmpLevelZero(float x, float compareValue)", on: ["SampledTexture1D"] },
      { s: "float SampleCmpLevelZero(float2 x, float compareValue)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "float SampleCmpLevelZero(float3 x, float compareValue)", on: ["SampledTexture2DArray"] },
      { s: "float SampleCmpLevelZero(SamplerComparisonState s, float3 x, float c)", on: ["TextureCube"] },
      { s: "float SampleCmpLevelZero(SamplerComparisonState s, float4 x, float c)", on: ["TextureCubeArray"] },
      { s: "float SampleCmpLevelZero(float4 x, float compareValue, out uint status)", on: ["SampledTextureCUBEArray"] },
      { s: "float SampleCmpLevelZero(SamplerComparisonState s, float x, float compareValue)", on: ["Texture1D"] },
      { s: "float SampleCmpLevelZero(SamplerComparisonState s, float2 x, float compareValue)", on: ["Texture1DArray", "Texture2D"] },
    ],
    more: 20,
    doc: "SampleCmp at mip 0, usable outside pixel shaders.",
  },
  "SampleGrad": {
    sigs: [
      { s: "T SampleGrad(float x, float ddx, float ddy)", on: ["SampledTexture1D"] },
      { s: "T SampleGrad(float2 x, float ddx, float ddy)", on: ["SampledTexture1DArray"] },
      { s: "T SampleGrad(float2 x, float2 ddx, float2 ddy)", on: ["SampledTexture2D"] },
      { s: "T SampleGrad(float3 x, float2 ddx, float2 ddy)", on: ["SampledTexture2DArray"] },
      { s: "T SampleGrad(float3 x, float3 ddx, float3 ddy)", on: ["SampledTexture3D", "SampledTextureCUBE"] },
      { s: "T SampleGrad(float4 x, float3 ddx, float3 ddy)", on: ["SampledTextureCUBEArray"] },
      { s: "T SampleGrad(SamplerState s, float x, float ddx, float ddy)", on: ["Texture1D"] },
      { s: "T SampleGrad(SamplerState s, float2 x, float ddx, float ddy)", on: ["Texture1DArray"] },
      { s: "T SampleGrad(SamplerState s, float2 x, float2 ddx, float2 ddy)", on: ["Texture2D"] },
      { s: "T SampleGrad(SamplerState s, float3 x, float2 ddx, float2 ddy)", on: ["Texture2DArray"] },
    ],
    t: 1,
    more: 40,
    doc: "Samples with explicit derivatives instead of the pixel quad's.",
  },
  "SampleLevel": {
    sigs: [
      { s: "T SampleLevel(float x, float lod)", on: ["SampledTexture1D"] },
      { s: "T SampleLevel(float2 x, float lod)", on: ["SampledTexture1DArray", "SampledTexture2D"] },
      { s: "T SampleLevel(float3 x, float lod)", on: ["SampledTexture2DArray", "SampledTexture3D", "SampledTextureCUBE"] },
      { s: "T SampleLevel(float4 x, float lod)", on: ["SampledTextureCUBEArray"] },
      { s: "T SampleLevel(SamplerState s, float x, float lod)", on: ["Texture1D"] },
      { s: "T SampleLevel(SamplerState s, float2 x, float lod)", on: ["Texture1DArray", "Texture2D"] },
      { s: "T SampleLevel(SamplerState s, float3 x, float lod)", on: ["Texture2DArray", "Texture3D", "TextureCube"] },
      { s: "T SampleLevel(SamplerState s, float4 x, float lod)", on: ["TextureCubeArray"] },
    ],
    t: 1,
    more: 24,
    doc: "Samples at an explicit mip level, usable outside pixel shaders.",
  },
  "SetShaderTableIndex": {
    sigs: [
      { s: "void SetShaderTableIndex(uint RecordIndex)", on: ["HitObject"] },
    ],
    sm: "6.9",
    doc: "Points the hit object at another shader table record.",
  },
  "Store": {
    sigs: [
      { s: "void Store(uint byteOffset, T value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    t: 1,
    doc: "Writes a value at a byte offset.",
  },
  "Store2": {
    sigs: [
      { s: "void Store2(uint byteOffset, uint2 value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Writes two consecutive uints at a byte offset.",
  },
  "Store3": {
    sigs: [
      { s: "void Store3(uint byteOffset, uint3 value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Writes three consecutive uints at a byte offset.",
  },
  "Store4": {
    sigs: [
      { s: "void Store4(uint byteOffset, uint4 value)", on: ["RWByteAddressBuffer", "RasterizerOrderedByteAddressBuffer"] },
    ],
    doc: "Writes four consecutive uints at a byte offset.",
  },
  "SubpassLoad": {
    sigs: [
      { s: "T SubpassLoad()", on: ["SubpassInput"] },
      { s: "T SubpassLoad(int sample)", on: ["SubpassInputMS"] },
    ],
    t: 1,
    doc: "Reads the subpass input at this fragment's position.",
  },
  "ThreadIncrementOutputCount": {
    sigs: [
      { s: "void ThreadIncrementOutputCount(uint count)", on: ["EmptyNodeOutput"] },
    ],
    doc: "Per-thread bump of this empty output's record count.",
  },
  "TraceRay": {
    sigs: [
      { s: "static dx::HitObject TraceRay(RaytracingAccelerationStructure AccelerationStructure, uint RayFlags, uint InstanceInclusionMask, uint RayContributionToHitGroupIndex, uint MultiplierForGeometryContributionToHitGroupIndex, uint MissShaderIndex, RayDesc Ray, inout T Payload)", on: ["HitObject"] },
    ],
    sm: "6.9",
    t: 1,
    doc: "Traces a ray and captures the result as a hit object without running hit or miss shaders.",
  },
  "TraceRayInline": {
    sigs: [
      { s: "void TraceRayInline(RaytracingAccelerationStructure AccelerationStructure, uint RayFlags, uint InstanceInclusionMask, RayDesc Ray)", on: ["RayQuery"] },
    ],
    doc: "Initializes this ray query for a traversal; step it with Proceed().",
  },
  "TriangleObjectPositions": {
    sigs: [
      { s: "BuiltInTrianglePositions TriangleObjectPositions()", on: ["HitObject"] },
    ],
    sm: "6.10",
    doc: "Object-space vertex positions of the hit object's triangle.",
  },
  "WorldRayDirection": {
    sigs: [
      { s: "float3 WorldRayDirection()", on: ["RayQuery"] },
    ],
    doc: "This query's ray direction in world space.",
  },
  "WorldRayOrigin": {
    sigs: [
      { s: "float3 WorldRayOrigin()", on: ["RayQuery"] },
    ],
    doc: "This query's ray origin in world space.",
  },
  "WriteSamplerFeedback": {
    sigs: [
      { s: "void WriteSamplerFeedback(Texture2D t, SamplerState s, float2 x)", on: ["FeedbackTexture2D"] },
      { s: "void WriteSamplerFeedback(Texture2DArray t, SamplerState s, float3 x)", on: ["FeedbackTexture2DArray"] },
    ],
    more: 2,
    doc: "Records which mips a Sample at x would touch into the feedback map.",
  },
  "WriteSamplerFeedbackBias": {
    sigs: [
      { s: "void WriteSamplerFeedbackBias(Texture2D t, SamplerState s, float2 x, float bias)", on: ["FeedbackTexture2D"] },
      { s: "void WriteSamplerFeedbackBias(Texture2DArray t, SamplerState s, float3 x, float bias)", on: ["FeedbackTexture2DArray"] },
    ],
    more: 2,
    doc: "WriteSamplerFeedback with the implicit mip level biased.",
  },
  "WriteSamplerFeedbackGrad": {
    sigs: [
      { s: "void WriteSamplerFeedbackGrad(Texture2D t, SamplerState s, float2 x, float2 ddx, float2 ddy)", on: ["FeedbackTexture2D"] },
      { s: "void WriteSamplerFeedbackGrad(Texture2DArray t, SamplerState s, float3 x, float2 ddx, float2 ddy)", on: ["FeedbackTexture2DArray"] },
    ],
    more: 2,
    doc: "WriteSamplerFeedback with explicit derivatives.",
  },
  "WriteSamplerFeedbackLevel": {
    sigs: [
      { s: "void WriteSamplerFeedbackLevel(Texture2D t, SamplerState s, float2 x, float lod)", on: ["FeedbackTexture2D"] },
      { s: "void WriteSamplerFeedbackLevel(Texture2DArray t, SamplerState s, float3 x, float lod)", on: ["FeedbackTexture2DArray"] },
    ],
    doc: "WriteSamplerFeedback at an explicit mip level.",
  },
}
};
