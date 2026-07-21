#include "@types.hlsli"

struct A {
	Texture2D t;
};

F32x4 getVal(A a, Texture2D b) {
	return a.t[0.xx] * b[0.xx];
}

Texture2D _a, _b;

[[oxc::stage("pixel")]]
F32x4 PSMain() : SV_TARGET {
	A a;
	a.t = _a;
	return getVal(a, _b);
}
