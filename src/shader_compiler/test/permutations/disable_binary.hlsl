//Binaries dropped after their link (@variants.hlsli): of three uniform rows, the one with MODE 2 still calls
//oxcDisableBinary() once specialized, so two binaries are kept, neither listing the marker's register.
//Driven by test_shader_compiler_permutations.c.

#include "@variants.hlsli"

RWByteAddressBuffer o : register(u0);

[[oxc::uniforms(U32 MODE = 0)]]
[[oxc::uniforms(U32 MODE = 1)]]
[[oxc::uniforms(U32 MODE = 2)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

	#ifdef $$MODE

		if($$MODE == 2)
			oxcDisableBinary();

		o.Store(0, $$MODE);

	#endif
}
