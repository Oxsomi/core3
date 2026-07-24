//NEGATIVE: uniforms must be identical across every entrypoint of a compilation unit (they specialize one
//shared linking step). These two entrypoints declare DIFFERENT uniforms, which is illegal - the compiler
//must reject it cleanly. Driven by test_shader_compiler_permutations.c (expected to fail compilation).

[[oxc::uniforms(B1 X = true)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainA() {}

[[oxc::uniforms(B1 Y = true)]]
[shader("compute")]
[numthreads(1, 1, 1)]
void mainB() {}
