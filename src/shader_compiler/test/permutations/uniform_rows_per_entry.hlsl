//Two entrypoints of one compilation unit with the same uniform but rows of their own: one library compile, linked once
//per entry and row, each binary carrying its own entry's value. Driven by test_shader_compiler_permutations.c.

RWByteAddressBuffer o : register(u0);

[[oxc::uniforms(U32 fmt = 0)]]
[[oxc::uniforms(U32 fmt = 1)]]
[[oxc::uniforms(U32 fmt = 2)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainWide() {
	#ifdef $$fmt
		o.Store(0, $$fmt);
	#endif
}

[[oxc::uniforms(U32 fmt = 3)]]
[[oxc::uniforms(U32 fmt = 5)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainDeep() {
	#ifdef $$fmt
		o.Store(4, $$fmt);
	#endif
}
