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

vec3 rayEarthIntersection(vec3 P, vec3 D, vec3 C, float earthRadius, float atmosphere){
    vec3 L = C - P;
    float t_ca = dot(L, D);
    float d = sqrt(dot(L, L) - t_ca * t_ca);
    
    //earth intersection
    float t_hc_earth = sqrt(earthRadius * earthRadius - d * d);
    float t_earth = t_ca + t_hc_earth; //different sign than earth because its from outside
    
    //atmosphere intersection
    float r = earthRadius + atmosphere;
    float t_hc_atmosphere = sqrt(r * r - d * d);
    float t_atmosphere = t_ca - t_hc_atmosphere;
    
    //if ray intersect with earth in front then view is blocked by earth
    float t = t_earth < 0 ? t_earth : t_atmosphere; 

    return P + t * D;
}

vec2 toSkyLut(vec3 V){
    float theta = acos(-(V.y));     //[0, pi]
    float y = theta / 3.1415f;      //[0, 1]
    float y_lowRange = y * 2 - 1;   //[-1, 1]
    float y_lowRangeScaled = sign(y_lowRange) * sqrt(abs(y_lowRange)); //[-1, 1]
    y = y_lowRangeScaled * 0.5 + 0.5;
    
	float phi = -atan(V.z, V.x);
	return vec2(phi / (2.f * 3.1415) + 0.5f, y);
}

vec3 fromSkyLut(vec2 uv){
    float theta = (1 - uv.y) - 0.5; //[-0.5, 0.5]
    theta = sign(theta) * theta * theta * 2.f;
    theta *= 3.1415;                //[-pi/2, pi/2]
    theta += 3.1415 * 0.5;          //[0, pi]
    float phi = (-uv.x + 0.5) * 2 * 3.1415;
    return vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));
}