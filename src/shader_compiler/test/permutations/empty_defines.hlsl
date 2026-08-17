//An explicitly empty defines annotation, which is a supported spelling meaning "one combination, with no
//defines" rather than "no combinations at all".
//It leaves definesPerCompilation holding a single 0 while defineNameValues stays completely empty, so the
// link time define matching walks one combination whose name/value range is empty.
//That range used to be built unconditionally, which both offset a null pointer and failed the compile
// outright, since a list ref refuses a zero length.
//Kept to a single entrypoint on purpose: this is about the empty range being walked at all, and a second
// entrypoint with the same empty body would compile to a byte identical binary.

[[oxc::defines()]]
[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main() { }
