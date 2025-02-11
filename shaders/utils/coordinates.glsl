#include "utils/functions.glsl"


#define TWO_PI 6.28318530718
#define PI     3.14159265359


struct squareData {
    vec2 uv;    // uv of each sqr
    vec2 id;    // id of each sqr
    float d;    // distance from center
    float r;    // polar angle
};

squareData squareCoords(vec2 uv) {
    squareData res;

    res.uv = fract(uv);
    res.uv = res.uv * 2.0 - 1.0;

    res.id = floor(uv);
    res.d = length(res.uv);
    res.r = (atan2(-res.uv.x, -res.uv.y) / TWO_PI) + 0.5;

    return res;
}