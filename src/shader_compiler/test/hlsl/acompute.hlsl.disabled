//Not in use until https://github.com/KhronosGroup/SPIRV-Reflect/issues/280 is fixed.
//Or until I separate the requirement that struct sizes and offsets must match between SPIRV & DXIL

struct Nicu {
	float a, b;
	uint64_t c;
	uint16_t d;
	float16_t e;
	int16_t f;
};

RWStructuredBuffer<Nicu> _sbuffer0;

[[oxc::extension("16BitTypes", "I64")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint id : SV_DispatchThreadID) {
	_sbuffer0[id].a = 123;
}