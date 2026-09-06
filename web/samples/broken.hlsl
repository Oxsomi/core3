#include "@types.hlsli"

// A file mid-edit, with one mistake of every shape the compiler can outline through. It does not
// compile, but the Symbols tab, hover and completion keep working on every declaration below.

struct Light {
    F32x3 pos;
    Radiance intensity;                     // undefined type on a member
    F32 range;
};

struct Node {
    Node next;                              // a type that contains itself
    F32 weight;
};

struct Params { F32 exposure; };
struct Params { F32 gamma; };               // duplicate definition

StructuredBuffer<Light> _lights;
RWTexture2D<F32x4> _output;

F32 attenuate(Distance d, F32 range) {      // undefined type on a parameter
    return saturate(1 - d / range);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(U32x3 id : SV_DispatchThreadID) {

    Light light = _lights[0];
    F32 a = attenuate(length(light.pos), light.range);
    F32x3 c = shade(light, a);              // call to an undeclared function
    _output[id.xy] = F32x4(c, 1) + position;    // undeclared identifier
    F32x3 unfinished = light.               // half-typed member access
}
