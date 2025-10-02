#include "@types.hlsli"

struct A {
	F32x4 color1 : COLOR1;  
};

struct PSInput : A {
	F32x4 color : COLOR;
	F32x4 test();
};

struct PSInput2 : A {
	F32x4 color : COLOR;
	F32x4 test();
};

F32x4 extTest();

template<typename T>
F32x4 test() {
 T t;
 return t.test();
}

[[oxc::stage("pixel")]]
F32x4 PSMain(A input) : SV_TARGET {
	return input.color1 * test<PSInput>() * test<PSInput2>() * extTest();
}
