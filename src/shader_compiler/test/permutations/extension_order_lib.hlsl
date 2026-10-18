//extension_order.hlsl in one lib compile: both entrypoints land in the same binaries, one for each extension line,
// and each entrypoint keeps the order it wrote its own lines in.
//Driven by test_shader_compiler_permutations.c.

RWStructuredBuffer<float> o;
struct Payload { float v; };

[[oxc::extension("16BitTypes", "I64")]]
[[oxc::extension("I64")]]
[shader("raygeneration")]
void rayRichFirst() { o[0] = 1; }

[[oxc::extension("I64")]]
[[oxc::extension("16BitTypes", "I64")]]
[shader("miss")]
void missBaseFirst(inout Payload p) { p.v = 0; }
