#define TWO_PI 6.28318530718
#define PI     3.14159265359


float atan2(float y, float x) {
    bool s = (abs(x) > abs(y));
    return mix(PI/2.0 - atan(x,y), atan(y,x), s);
}

vec2 rotate(vec2 uv, float th){
    return mat2(cos(th), sin(th), -sin(th), cos(th)) * uv;
}

float clamp01(float x) {
    return clamp(x, 0.0, 1.0);
}
