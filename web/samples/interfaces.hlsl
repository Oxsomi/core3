#include "@types.hlsli"

// Interfaces and inheritance reflect into the oiSR type graph: the Symbols tab records which interface a
// struct implements and which base it extends, so go-to-definition works across the hierarchy.

interface IArea {
    F32 area();
};

struct Shape {
    F32x2 size;
};

struct Rect : Shape, IArea {
    F32 area() { return size.x * size.y; }
};

struct Circle : Shape, IArea {
    F32 area() { return 3.14159265 * size.x * size.x; }
};

RWStructuredBuffer<F32> _areas;

struct Shapes {
    U32 count;
};

PUSH_CONSTANT Shapes _shapes;

template<typename T>
F32 areaOf(F32x2 size) {
    T t;
    t.size = size;
    return t.area();
}

[[oxc::stage("compute")]]
[numthreads(1, 1, 1)]
void main(U32 id : SV_DispatchThreadID) {

    if (id >= _shapes.count)
        return;

    _areas[id * 2 + 0] = areaOf<Rect>(F32x2(3, 4));
    _areas[id * 2 + 1] = areaOf<Circle>(F32x2(2, 2));
}
