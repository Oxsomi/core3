#include "@types.hlsli"

struct A {
	F32x4 color1 : COLOR1;  
};

interface B { };

interface C {
	F32x4 test();
};

struct PSInput : A, B, C {
	F32x4 color : COLOR;
	F32x4 test() { return 2; }
};

struct PSInput2 : A, B, C {
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
