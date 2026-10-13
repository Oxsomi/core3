//Combinations dropped before they compile (@variants.hlsli): three define lines crossed with two extension lines, the
//one under 16 bit types with MODE 2 disabled, so five of six have a binary. Driven by test_shader_compiler_permutations.c.

#include "@variants.hlsli"

#if defined(__OXC_EXT_16BITTYPES) && $MODE == 2
	OXC_DISABLE_COMBINATION
#endif

RWByteAddressBuffer o : register(u0);

[[oxc::defines("MODE" = "0")]]
[[oxc::defines("MODE" = "1")]]
[[oxc::defines("MODE" = "2")]]
[[oxc::extension("16BitTypes")]]
[[oxc::extension()]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
	#ifdef $MODE
		o.Store(0, $MODE);
	#endif
}
