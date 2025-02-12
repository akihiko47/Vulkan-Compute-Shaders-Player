#define TWO_PI 6.28318530718
#define PI     3.14159265359

vec2 rotate(vec2 uv, float th){
    return mat2(cos(th), sin(th), -sin(th), cos(th)) * uv;
}

float clamp01(float x) {
    return clamp(x, 0.0, 1.0);
}
