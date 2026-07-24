//Three RT entrypoints in one shared lib compile, each restricted to a different backend set via oxc::binary.
//The driver must filter per entrypoint at link time: raygen = both; miss = spv only; closesthit = dxil only.
//So SPIRV keeps {mainRaygen, mainMiss}, DXIL keeps {mainRaygen, mainCH}.

RWStructuredBuffer<float> o;
struct Payload { float v; };

[shader("raygeneration")]
void mainRaygen() { o[0] = 1; }

[[oxc::binary("spv")]]
[shader("miss")]
void mainMiss(inout Payload p) { p.v = 0; }

[[oxc::binary("dxil")]]
[shader("closesthit")]
void mainCH(inout Payload p, BuiltInTriangleIntersectionAttributes a) { p.v = 1; }
