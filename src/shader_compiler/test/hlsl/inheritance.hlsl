#include "@types.hlsli"

struct A {
	F32x4 color1 : COLOR1;
};

//DXC's SPIR-V backend asserts (getNumBases() <= 1) on a struct with more than one base (concrete + interface).
//So SPIR-V exercises a single-inheritance variant, while the multi-base (concrete + interface) case is DXIL-only.
//Interfaces B and C carry no data, so PSInput's data layout (color1 from A, then color) is identical either way.

#ifdef __spirv__

	struct PSInput : A {
		F32x4 color : COLOR;
		F32x4 test() { return 2; }
	};

	struct PSInput2 : A {
		F32x4 color : COLOR;
		F32x4 test() { return 3; }
	};

#else

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

#endif

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
