//Uniforms of every scalar width and sign, at values one byte or sixteen bits can't hold, each stored back as a word: the
//specialized binary has to hold every value as a constant, on SPIR-V with and without 16 bit types and on DXIL.
//Driven by test_shader_compiler_features.c.

RWByteAddressBuffer o : register(u0);

[[oxc::uniforms(
	U32 big = 300,
	I32 neg = -70000,
	F32 f = 0.123456789,
	I16 s16 = -2,
	I8 s8 = -1,
	F16 h = 1.5,
	U16 u16 = 65535,
	U8 u8 = 255,
	B1 b = true
)]]
[[oxc::extension()]]
[[oxc::extension("16BitTypes")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

	#ifdef $$big
		o.Store(0, $$big);
		o.Store(4, asuint((int) $$neg));
		o.Store(8, asuint($$f));
		o.Store(12, asuint((int) $$s16));
		o.Store(16, asuint((int) $$s8));
		o.Store(20, asuint((float) $$h));
		o.Store(24, (uint) $$u16);
		o.Store(28, (uint) $$u8);
		o.Store(32, $$b ? 7u : 9u);
	#endif
}
