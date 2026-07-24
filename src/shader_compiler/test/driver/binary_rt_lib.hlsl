//RT library with a both-backends raygen and a DXIL-only miss, for the driver's [[oxc::binary(...)]] test:
//SPIRV keeps only mainRaygen, DXIL keeps both. Exercises per-entrypoint backend filtering inside one shared
//lib compile while keeping >=1 entrypoint per backend (so neither side degenerates to an empty oiSH).

RWStructuredBuffer<float> o;
struct Payload { float v; };

[shader("raygeneration")]
void mainRaygen() { o[0] = 1; }

[[oxc::binary("dxil")]]
[shader("miss")]
void mainMiss(inout Payload p) { p.v = 0; }
