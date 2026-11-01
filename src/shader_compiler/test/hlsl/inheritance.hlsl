#include "@types.hlsli"

struct A {
	F32x4 color1 : COLOR1;
};

//HLSL 202x has no interface keyword, and DXC accepts one base per struct, so this tests single inheritance
// with a method on both backends: PSInput's data layout is color1 from A, then color.

struct PSInput : A {
	F32x4 color : COLOR;
	F32x4 test() { return 2; }
};

struct PSInput2 : A {
	F32x4 color : COLOR;
	F32x4 test() { return 3; }
};

StructuredBuffer<PSInput> _forceCheckPSInput;

template<typename T>
F32x4 test() {
 T t;
 return t.test();
}

[[oxc::stage("pixel")]]
F32x4 PSMain(A input) : SV_TARGET {
	return input.color1 * test<PSInput>() * test<PSInput2>();
}
