typedef float f32;

typedef RWByteAddressBuffer rwbab;
typedef ByteAddressBuffer bab;
typedef StructuredBuffer<float> sbuffloat;
typedef StructuredBuffer<f32> sbuffloat32;
typedef Texture2D texx;
typedef Texture2D<float> texxf;
typedef Texture2D<f32> texxf32;
typedef TextureCube texxc;
typedef TextureCube<float> texxfc;
typedef TextureCube<f32> texxf32c;
typedef SamplerState smp;

RWByteAddressBuffer output;
rwbab output1;

StructuredBuffer<float> test;
sbuffloat test1;
StructuredBuffer<f32> test2;
sbuffloat32 test3;

ByteAddressBuffer input;
bab input1;

Texture2D tex;
Texture2D<float> tex2;
Texture2D<f32> tex3;
texx tex4;
texxf tex5;
texxf32 tex6;

TextureCube texc;
TextureCube<float> tex2c;
TextureCube<f32> tex3c;
texxc tex4c;
texxfc tex5c;
texxf32c tex6c;

SamplerState sampler0;
smp sampler1;

[numthreads(16, 16, 1)]
void main() { }
