#ifndef COLOR_CONVERSION_INC
#define COLOR_CONVERSION_INC

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

//reference: https://en.wikipedia.org/wiki/YCoCg
vec3 linearToYCoCg(vec3 linear){
    return vec3(
     linear.x * 0.25 + 0.5 * linear.y + 0.25 * linear.z,
     linear.x * 0.5                   - 0.5  * linear.z,
    -linear.x * 0.25 + 0.5 * linear.y - 0.25 * linear.z);
}

vec3 YCoCgToLinear(vec3 YCoCg){
    return vec3(
    YCoCg.x + YCoCg.y - YCoCg.z,
    YCoCg.x           + YCoCg.z,
    YCoCg.x - YCoCg.y - YCoCg.z);
}

#endif // #ifndef COLOR_CONVERSION_INC