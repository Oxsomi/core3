//An entry's binaries keep the order its extension lines were written in, since GraphicsDeviceRef_getFirstShaderEntry
// takes the first one the device can run and that order is the author's preference.
//mainRichFirst lists the larger extension set first and mainBaseFirst the smaller one first,
// so neither order can come out of sorting by the extension mask.
//The uniforms multiply against the extension lines: extension lines vary fastest, uniform sets slowest.
//Driven by test_shader_compiler_permutations.c.

[[oxc::uniforms(U32 Mode = 0)]]
[[oxc::uniforms(U32 Mode = 1)]]
[[oxc::extension("16BitTypes", "I64")]]
[[oxc::extension("I64")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainRichFirst() {}

[[oxc::uniforms(U32 Mode = 0)]]
[[oxc::uniforms(U32 Mode = 1)]]
[[oxc::extension("I64")]]
[[oxc::extension("16BitTypes", "I64")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainBaseFirst() {}
