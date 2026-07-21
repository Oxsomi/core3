struct PSInput
{
	float4 color : COLOR;
};

struct Test;
struct Test;

struct A {
	float a;
	float t(Test t);  
};

struct Test {
	float a;  
};

float A::t(Test t) {
	return 2;
}

float4 test();
float4 test();

float4 test()
{
    return 0.xxxx;
}

[[oxc::stage("pixel")]]
float4 PSMain(PSInput input) : SV_TARGET
{
	A a;
	Test t;
	return input.color * a.t(t);
}