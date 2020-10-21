float rayleighHeightFactor(float height){
    return exp(-height * (1.f / 8));
}

float mieHeightFactor(float height){
    return exp(-height * (1.f / 1.2f));
}

float ozoneHeightFactor(float height){
    return max(0, 1 - abs(height - 25.f) / 15.f);
}

//see https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
vec3 rayFromInsideSphereIntersection(vec3 P, vec3 D, vec3 C, float r){
    vec3 L = C - P;
    float t_ca = dot(L, D);
    float d = sqrt(dot(L, L) - t_ca * t_ca);
    float t_hc = sqrt(r * r - d * d);
    float t = t_ca + t_hc;
    return P + t * D;
}

vec2 toSkyLut(vec3 V){
    float theta = acos(-V.y);
	float phi = -atan(V.z, V.x);
	return vec2(phi / (2.f * 3.1415) + 0.5f, theta / 3.1415f);
}

vec3 fromSkyLut(vec2 uv){
    float theta = ((1-uv.y) - 0.5) * 3.1415 + 3.1415 * 0.5;
    float phi = (-uv.x + 0.5) * 2 * 3.1415;
    return vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
}