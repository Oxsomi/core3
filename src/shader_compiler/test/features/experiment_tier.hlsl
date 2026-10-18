//An experiment tier, ESHExtension2's: one line declares ExperimentTier1 and the other nothing, and each line's compile
// sees exactly its own define, so the two binaries differ by what they store. Both backends, as the tier is.

RWByteAddressBuffer output;

[[oxc::extension("ExperimentTier1")]]
[[oxc::extension()]]
[[oxc::stage("compute")]]
[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID) {

	#ifdef __OXC_EXT_EXPERIMENTTIER1
		output.Store(id * 4, 2);
	#else
		output.Store(id * 4, 1);
	#endif
}
