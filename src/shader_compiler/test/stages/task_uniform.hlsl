//An amplification entry written with [shader("amplification")], DXC's name for it, and a uniform: linked and reflected
//as a task stage on both backends. Driven by test_shader_compiler_stages.c.
//The payload is two words: DXC's library link refuses one of a single word as larger than declared (8 against 4).

struct Payload { uint meshletIndex; uint count; };

groupshared Payload payload;

[[oxc::uniforms(U32 count = 1)]]
[[oxc::uniforms(U32 count = 2)]]
[shader("amplification")]
[numthreads(1, 1, 1)]
void main(uint gtid : SV_GroupIndex) {

	uint count = 1;

	#ifdef $$count
		count = $$count;
	#endif

	payload.meshletIndex = gtid;
	payload.count = count;
	DispatchMesh(count, 1, 1, payload);
}
