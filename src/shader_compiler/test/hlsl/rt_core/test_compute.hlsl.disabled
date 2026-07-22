struct Nested {
	float16_t nest;
};

struct Pannenkoek {
	float nest0;
	uint64_t c;
	Nested nest;
	Nested nest1[3];
	float a[3][3];
	float b[3][3][3];
};

struct Nicu {
	float a, b;
	uint64_t c;
	uint16_t d;
	float16_t e;
	int16_t f;
};

AppendStructuredBuffer<Pannenkoek> _append;
ConsumeStructuredBuffer<Pannenkoek> _consume;

Nicu _nicu;
Pannenkoek _kak1[2];
int16_t _e;
int64_t _i;
float4x3 _a;
float16_t _f;
Pannenkoek _kak;
uint64_t _h;
double _g;
float3x4 _b;
uint _c;
uint16_t _d;
uint16_t _j[3][3];

RWTexture2D<uint4> _output;
StructuredBuffer<Nicu> _sbuffer0;
RWStructuredBuffer<Nicu> _sbuffer20;
StructuredBuffer<float4x3> _sbuffer;
RWStructuredBuffer<float4x3> _sbuffer2;
Texture2DMS<float> _multiSample;

//StructuredBuffer<float4x3[3]> _sbuffer;

Texture1D<float> _test0;
Texture3D<float> _test1;
Texture1DArray<float> _test2;
Texture2DArray<float> _test3;
TextureCube<float> _test4;
TextureCubeArray<float> _test5;

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
Texture2D<float> _bla;

[[vk::combinedImageSampler]]
[[vk::binding(0, 2)]]
SamplerState _blabla;

Texture2D<unorm float> _test;
Texture2D<unorm float> _test69[2][2];

#ifdef __spirv__
	[[vk::input_attachment_index(1)]] SubpassInput input;
#endif

[[oxc::extension("16BitTypes", "F64", "I64")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint2 id : SV_DispatchThreadID) {
	_output[id] = _c.xxxx + _multiSample[0.xx].xxxx;
	_append.Append(_consume.Consume());
	_sbuffer2[0] = _sbuffer[0];
	_sbuffer20[0] = _sbuffer0[0];
}