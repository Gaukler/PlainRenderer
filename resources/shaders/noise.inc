//from: "Next Generation Post Processing in Call Of Duty Advanced Warfare" slide page 123
float hash(vec2 uv){
    vec3 magic = vec3(0.06711056, 0.00583715, 62.9829189);
    return fract(magic.z * fract(dot(uv, magic.xy)));
}