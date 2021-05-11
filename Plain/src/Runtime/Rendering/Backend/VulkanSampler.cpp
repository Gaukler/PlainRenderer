#include "VulkanSampler.h"
#include "VulkanContext.h"

VkSampler createVulkanSampler(const SamplerDescription& desc) {
    // TODO proper min and mag filters
    // TODO allow unnormalized coordinates

    const VkFilter filter = samplerInterpolationToVkFilter(desc.interpolation);
    const VkSamplerAddressMode wrapping = wrappingModeToVkSamplerAddressMode(desc.wrapping);

    VkSamplerCreateInfo samplerInfo;
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = wrapping;
    samplerInfo.addressModeV = wrapping;
    samplerInfo.addressModeW = wrapping;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.anisotropyEnable = desc.useAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = 8.f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = (float)desc.maxMip;
    samplerInfo.borderColor = samplerBorderColorToVkBorderColor(desc.borderColor);
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    const auto result = vkCreateSampler(vkContext.device, &samplerInfo, nullptr, &sampler);
    checkVulkanResult(result);

    return sampler;
}

VkFilter samplerInterpolationToVkFilter(const SamplerInterpolation mode) {
    switch (mode) {
        case(SamplerInterpolation::Linear):     return VK_FILTER_LINEAR;
        case(SamplerInterpolation::Nearest):    return VK_FILTER_NEAREST;
        default: std::cout << "unsupported sampler interpolation\n"; return VK_FILTER_MAX_ENUM;
    }
}

VkSamplerAddressMode wrappingModeToVkSamplerAddressMode(const SamplerWrapping wrapping) {
    switch (wrapping) {
        case(SamplerWrapping::Clamp):   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case(SamplerWrapping::Color):   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case(SamplerWrapping::Repeat):  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        default: std::cout << "unsupported sampler wrapping mode\n"; return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
    }
}

VkBorderColor samplerBorderColorToVkBorderColor(const SamplerBorderColor color) {
    switch (color) {
        case(SamplerBorderColor::Black): return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case(SamplerBorderColor::White): return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        default: std::cout << "unsupported sampler border color\n"; return VK_BORDER_COLOR_MAX_ENUM;
    }
}