#include "@types.hlsli"

Texture2D a;
Texture2D b;
RWTexture2D<F32x4> c;

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main()
{
    c[0.xx] = b[0.xx];
}

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main2()
{
    c[0.xx] = a[0.xx];
}