#include "pch.h"
#include "MathUtils.h"

/*
=========
directionToVector
=========
*/
glm::vec3 directionToVector(const glm::vec2 direction) {

    const float theta   = direction.y / 180.f * 3.1415f;
    const float phi     = direction.x / 180.f * 3.1415f;

    glm::vec3 vec;
    vec.x = sin(theta) * cos(phi);
    vec.y = sin(theta) * sin(phi);
    vec.z = cos(theta);
    return vec;
}

/*
=========
mipCountFromResolution
=========
*/
uint32_t mipCountFromResolution(const uint32_t width, const uint32_t height, const uint32_t depth) {
    return 1 + (uint32_t)std::floor(std::log2(std::max(std::max(width, height), depth)));
}

/*
=========
mipCountFromResolution
=========
*/
glm::mat4 getOpenGLToVulkanCorrectionMatrix() {
    return glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);
}

/*
=========
hammersley2D
=========
*/
glm::vec2 hammersley2D(const uint32_t index) {
    //reference: http://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler.html
    return glm::vec2(radicalInverseBase2(index), radicalInverseBase3(index));
}

/*
=========
reverse32Bit
=========
*/
uint32_t reverse32Bit(const uint32_t in) {
    //reference: http://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler.html
    uint32_t out = (in << 16) | (in >> 16);                         //swap adjacent 16 bits
    out  = ((out & 0x00ff00ff) << 8) | ((out & 0xff00ff00) >> 8);   //swap adjacent 8 bits
    out  = ((out & 0x0f0f0f0f) << 4) | ((out & 0xf0f0f0f0) >> 4);   //swap adjacent 4 bits
    out  = ((out & 0x33333333) << 2) | ((out & 0xcccccccc) >> 2);   //swap adjacent 2 bits
    out  = ((out & 0x55555555) << 1) | ((out & 0xaaaaaaaa) >> 1);   //swap adjacent 1 bits
    return out;
}

/*
=========
radicalInverseBase2
=========
*/
float radicalInverseBase2(const uint32_t in) {
    //reference: http://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler.html
    const uint32_t rev = reverse32Bit(in);
    return float(rev) * (float)2.3283064365386963e-10;
}

/*
=========
radicalInverseBase3
=========
*/
float radicalInverseBase3(const uint32_t in) {
    //reference: http://www.pbr-book.org/3ed-2018/Sampling_and_Reconstruction/The_Halton_Sampler.html
    const uint32_t base = 3;
    const float inverseBase = 1.f / (float)base;
    uint32_t reversedDigits = 0;
    uint32_t current = in;                              //example: starting at 10
                                                        //we go from largest to smallest digit, until current reaches 0
    float inverseBasePowerN = 1;
    while (current) {
        const uint32_t next = current / base;           //example: 10 / base = 3
                                                        //digits go from 0 to base-1
                                                        //for example binary hase base 2 and has digits 0 and 1
        const uint32_t digit = current - next * base;   //example: 10 - 3 * 3 = 10 - 9 = 1
        reversedDigits *= base;                         //'lifting' current digits to the next base
                                                        //example: take 101 base 10 and go left to right
                                                        //the first 1 has to be 'lifted' twice
                                                        //101 = 1 * 100 + 1 * 10 + 1 * 1
        reversedDigits += digit;                        //adding current digit 
        inverseBasePowerN *= inverseBase;               //incrementally building 1 / base^n
                                                        //this is simply done by multiplying 1 / base together n times 
        current = next;
    }
    return reversedDigits * inverseBasePowerN;          //we multiply only once at the end with the inverse base
                                                        //by the previous 'lifting' this provides the correct weight per digit
}