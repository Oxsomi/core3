#include "@types.hlsli"

typedef F32x4 F32x4a4[4];		//Should turn into F32x4[4]
typedef F32x4a4 F32x4a4a4[4];		//Should turn into name + array: F32x4a4[4], semantic: float4[4][4]
typedef F32x4a4a4 F32x4a4a4_2;		//Should turn into F32x4a4a4, semantic: float4[4][4]
typedef F32x4a4a4 F32x4a4a4a4[4];	//Should turn into name + array: F32x4a4a4[4], semantic: float4[4][4][4]

typedef F32x4 F32a4;			//Should turn into F32x4
typedef F32x4 F32a4a4[4];		//Should turn into F32x4[4]
typedef F32x4 F32a4a4a4[4][4];		//Should turn into F32x4[4][4]
typedef F32x4 F32a4a4a4a4[4][4][4];	//Should turn into F32x4[4][4][4]

[[oxc::stage("pixel")]]
F32x4 PSMain(F32x4 a : A) : SV_TARGET {
	return a;
}
