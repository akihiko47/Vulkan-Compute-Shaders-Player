#include "utils/functions.glsl"
#include "utils/hashes.glsl"


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
    res.r = (atan(-res.uv.x, -res.uv.y) / TWO_PI) + 0.5;

    return res;
}


struct cellData{
    vec2 uv; // uv of each cell
    vec2 id; // id of each cell
    float d;   // distance from center
    float bd;  // distance from edges
};

cellData cellCoords(vec2 uv, float time) {
    cellData res;

    vec2 sqrId = vec2(floor(uv));
    vec2 sqrUv = fract(uv);

    vec2 minOffset;
    vec2 minP;

    float d = 100000;
    vec2 id;
    for(int y = -1; y <= 1; y++){
        for(int x = -1; x <= 1; x++){
            vec2 idOffset = vec2(float(x), float(y));
            vec2 randPoint = hash22(sqrId + idOffset);

            // ANIMATION HERE
            randPoint = sin(time + randPoint * 6.2831) * 0.5 + 0.5;

            vec2 p = randPoint + idOffset - sqrUv;
            float sqrDist = dot(p, p);
            if(sqrDist < d){
                d = sqrDist;
                id = sqrId + idOffset;
                minOffset = idOffset;
                minP = p;
            }
        }
    }

    float bd = 3.402823466e+38F;
    for(int j = -2; j <= 2; j++){
        for(int i = -2; i <= 2; i++){
            vec2 idOffset = minOffset + vec2(float(i), float(j));
            vec2 randPoint = hash22(sqrId + idOffset);

            // ANIMATION HERE
            randPoint = sin(time + randPoint * 6.2831) * 0.5 + 0.5;

            vec2 p = randPoint + idOffset - sqrUv;

            if(dot(minP - p,minP - p) > 0.00001)
                bd = min(bd, dot(0.5 * (minP + p), normalize(p - minP)));
        }
    }
    
    res.d = sqrt(d);
    res.id = id;
    res.uv = vec2(-minP.x, -minP.y);
    res.bd = bd;

    return res;
}
