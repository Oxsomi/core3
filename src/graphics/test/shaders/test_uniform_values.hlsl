//Uniform values at run time: every scalar type written back as a word, in two rows the interface test tells apart by
//value. It finds each row's binary with its values spelled otherwise than written here, dispatches it and checks the
//words (test_graphics_caps_exec.cpp). The 16 bit line is taken where the device has 16 bit types.

#include "@resources.hlsli"
#include "@buffer.hlsli"

struct UniformPush {
	U32 output;        //Bindless write handle of the output buffer
	U32 padding0, padding1, padding2;
};

PUSH_CONSTANT UniformPush _push;

[[oxc::uniforms(
	U32 big = 300, I32 neg = -70000, F32 f = 0.123456789, I16 s16 = -2, I8 s8 = -1,
	F16 h = 1.5, U16 u16 = 65535, U8 u8 = 255, B1 b = true
)]]
[[oxc::uniforms(
	U32 big = 4000000000, I32 neg = 70000, F32 f = -2.5, I16 s16 = 32767, I8 s8 = -128,
	F16 h = -0.25, U16 u16 = 0, U8 u8 = 1, B1 b = false
)]]
[[oxc::extension("16BitTypes")]]
[[oxc::extension()]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

	#ifdef $$big
		setAtUniform<U32x4>(_push.output, 0, U32x4($$big, asuint((I32) $$neg), asuint($$f), asuint((I32) $$s16)));
		setAtUniform<U32x4>(_push.output, 16, U32x4(asuint((I32) $$s8), asuint((F32) $$h), (U32) $$u16, (U32) $$u8));
		setAtUniform<U32>(_push.output, 32, $$b ? 7u : 9u);
	#endif
}
