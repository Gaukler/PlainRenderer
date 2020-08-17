//reference: https://en.wikipedia.org/wiki/SRGB

vec3 linearTosRGB(vec3 linear){
    vec3 sRGBLo = linear * 12.92;
    vec3 sRGBHi = (pow(abs(linear), vec3(1.0/2.4)) * 1.055)  - 0.055;
    vec3 sRGB;
    sRGB.x = linear.x  <= 0.0031308 ? sRGBLo.x : sRGBHi.x;
    sRGB.y = linear.y  <= 0.0031308 ? sRGBLo.y : sRGBHi.y;
    sRGB.z = linear.z  <= 0.0031308 ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}

vec3 sRGBToLinear(vec3 linear){
    vec3 sRGBLo = linear / 12.92;
    vec3 sRGBHi = (pow(abs(linear + 0.055) / 1.055, vec3(2.4)));
    vec3 sRGB;
    sRGB.x = linear.x  <= 0.004045 ? sRGBLo.x : sRGBHi.x;
    sRGB.y = linear.y  <= 0.004045 ? sRGBLo.y : sRGBHi.y;
    sRGB.z = linear.z  <= 0.004045 ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}