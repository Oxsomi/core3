//Every combination disabled at compile: nothing compiles and nothing is written.
//Driven by test_shader_compiler_permutations.c.

#include "@variants.hlsli"

OXC_DISABLE_COMBINATION

RWByteAddressBuffer o : register(u0);

[[oxc::defines("MODE" = "0")]]
[[oxc::defines("MODE" = "1")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
	#ifdef $MODE
		o.Store(0, $MODE);
	#endif
}
