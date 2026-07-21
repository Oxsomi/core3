
RWByteAddressBuffer output1;
RWByteAddressBuffer output2;
RWByteAddressBuffer output3 : register(u0);
RWByteAddressBuffer output4 : register(space1);
RWByteAddressBuffer output5 : SEMA;
RWByteAddressBuffer output6;
RWByteAddressBuffer output7 : register(u1);
RWByteAddressBuffer output8[12] : register(u3);
RWByteAddressBuffer output9[12];
RWByteAddressBuffer output10[33] : register(space1);
RWByteAddressBuffer output11[33] : register(space2);
RWByteAddressBuffer output12[33] : register(u0, space2);
StructuredBuffer<float> test;

[[oxc::stage("compute")]]
[numthreads(16, 16, 1)]
void main(uint id : SV_DispatchThreadID) {
    output2.Store<uint>(id * 4, 1);
}
