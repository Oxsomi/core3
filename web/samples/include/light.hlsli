#pragma once
#include "@types.hlsli"

struct Light {
    
    F32x3 position;
    F32   radius;

    F32x3 color;
    F32   intensity;
};

F32x3 shadeLight(Light l, F32x3 cam) {
    F32x3 d = l.position - cam;
    return l.color * l.intensity / (1.0 + dot(d, d));
}
