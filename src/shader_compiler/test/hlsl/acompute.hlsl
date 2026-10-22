//Nicu's members end at 22 bytes while its element stride is 24: the snapshot pins that the SPIRV side
// reflects the decorated ArrayStride rather than spirv-reflect's summed size (SPIRV-Reflect#280), so
// both backends combine.

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