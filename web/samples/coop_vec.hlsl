#include "@types.hlsli"
#include "@extensions.hlsli"

// Cooperative vectors: the matrix times vector at the heart of neural inference as one hardware op.
// Two layers of a tiny MLP, F16 weights with a bias, a ReLU, then FP8 weights for the second layer,
// the shape a neural texture decoder or a learned BRDF runs per pixel. DXIL lowers each step to
// __builtin_MatVecMul through dx/linalg.h, SPIR-V to SPV_NV_cooperative_vector, from one macro.
// The vector lives in a buffer between steps, which is what the macros work on.

OXC_COOPVEC_MATRIX_BUFFER(_layer1);     // 4x4 F16
OXC_COOPVEC_VECTOR_BUFFER(_bias1);
OXC_COOPVEC_MATRIX_BUFFER(_layer2);     // 4x4 FP8 e4m3, a quarter of the bytes of layer 1
OXC_COOPVEC_VECTOR_BUFFER(_activations);

[[oxc::model("6.10")]]
[[oxc::extension("CoopVec", "CoopFP8", "16BitTypes")]]
[shader("compute")]
[numthreads(1, 1, 1)]
void main() {

    OXC_COOPVEC_MATVEC_BIAS_4X4_F16(_layer1, 0, 8, _bias1, _activations);

    [unroll]
    for (U32 i = 0; i < 4; ++i)
        _activations[0].data[i] = max(_activations[0].data[i], (float16_t) 0);

    OXC_COOPVEC_MATVEC_FP8W_4X4_F16(_layer2, 0, _activations);
}
