vec3 cubemapCoordinateToDirection(ivec3 co, int cubeRes){
    vec3 forwardPerFace[6] = {
        vec3(1.f, 0.f, 0.f),    //right
        vec3(-1.f, 0.f, 0.f),   //left
        vec3(0.f, 1.f, 0.f),    //down
        vec3(0.f, -1.f, 0.f),   //up
        vec3(0.f, 0.f, 1.f),    //back
        vec3(0.f, 0.f, -1.f)    //forward
    };
    vec3 upPerFace[6] = {
        vec3(0.f, -1.f, 0.f),   //right
        vec3(0.f, -1.f, 0.f),   //left
        vec3(0.f, 0.f, 1.f),    //down
        vec3(0.f, 0.f, -1.f),   //up
        vec3(0.f, -1.f, 0.f),   //back
        vec3(0.f, -1.f, 0.f)    //forward
    };

    vec3 forward = forwardPerFace[co.z];
    vec3 up = upPerFace[co.z];
    vec3 right = cross(forward, up);

    vec2 uvNormalized = vec2(co.xy) / float(cubeRes);
    vec2 factors = 2.f * uvNormalized - 1.f;

    return normalize(forward + factors.y * up + factors.x * right);
}