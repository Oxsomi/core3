//oxcDisableBinary() under a condition no specialization folds, a value read at run time: every binary keeps the call,
//so every one is dropped and nothing is written. Driven by test_shader_compiler_permutations.c.

#include "@variants.hlsli"

RWByteAddressBuffer o : register(u0);

[[oxc::uniforms(U32 MODE = 0)]]
[[oxc::uniforms(U32 MODE = 1)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

	if(o.Load(0) == 7)
		oxcDisableBinary();

	#ifdef $$MODE
		o.Store(4, $$MODE);
	#endif
}
