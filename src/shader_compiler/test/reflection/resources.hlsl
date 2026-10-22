//A single compute entrypoint with one of (almost) every EGfxRegisterType, each actually used so DXC keeps it.
//RayQuery gives us an AccelerationStructure register and compiles inline RT on both backends (SM6.5).
//Driven by test_shader_compiler_reflection.c, which asserts each reflected register's type/write/array/stride.

struct Particle { uint x, y, z, w; };        //16 bytes

//Members end at 22 bytes, but the U64 aligns the element stride to 24; both backends must reflect 24.

struct PaddedTail {
	float a, b;
	uint64_t c;
	uint16_t d;
	float16_t e;
	int16_t f;
};

//A vector crossing a 16-byte boundary: legal in a structured buffer (test lands at 4, stride 20), since only
// cbuffers keep 16-byte rows.

struct Straddle { uint pad; uint4 test; };

StructuredBuffer<Particle> inBuf;
RWStructuredBuffer<Particle> outBuf;
RWStructuredBuffer<PaddedTail> padBuf;
RWStructuredBuffer<Straddle> straddleBuf;
ByteAddressBuffer rawIn;
RWByteAddressBuffer rawOut;

Texture1D<float4> tex1d;
Texture2D<float4> tex;
Texture3D<float4> tex3d;
TextureCube<float4> texCube;
Texture2DMS<float4> texMS;
Texture2D<float> shadowMap;
RWTexture2D<float4> img;
Texture2D<float4> texArr[4];

SamplerState samp;
SamplerComparisonState sampCmp;

RaytracingAccelerationStructure tlas;

[[oxc::extension("RayQuery", "16BitTypes", "I64")]]        //inline RT; 16-bit + 64-bit for PaddedTail
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	float4 c = (float)inBuf[0].x;
	c += tex1d.SampleLevel(samp, 0.5, 0);
	c += tex.SampleLevel(samp, float2(0, 0), 0);
	c += texArr[id & 3].SampleLevel(samp, float2(0, 0), 0);
	c += tex3d.SampleLevel(samp, float3(0, 0, 0), 0);
	c += texCube.SampleLevel(samp, float3(0, 0, 1), 0);
	c += texMS.Load(int2(0, 0), 0);
	c += shadowMap.SampleCmpLevelZero(sampCmp, float2(0, 0), 0.5);
	RayQuery<RAY_FLAG_NONE> q;
	RayDesc ray; ray.Origin = float3(0, 0, 0); ray.TMin = 0; ray.Direction = float3(0, 0, 1); ray.TMax = 1e30;
	q.TraceRayInline(tlas, RAY_FLAG_NONE, 0xFF, ray);
	q.Proceed();
	if (q.CommittedStatus() != COMMITTED_NOTHING) c += 1;
	rawOut.Store<uint>(0, rawIn.Load<uint>(0) + (uint)c.x);
	outBuf[0] = inBuf[0];
	padBuf[id].a = c.x;
	straddleBuf[id].test = uint4(1, 2, 3, 4);
	img[uint2(0, 0)] = c;
}
