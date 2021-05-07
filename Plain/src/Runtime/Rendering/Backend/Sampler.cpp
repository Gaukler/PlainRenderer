#include "pch.h"
#include "Sampler.h"
#include "VulkanContext.h"

VkSampler createVulkanSampler(const SamplerDescription& desc) {
    //TODO proper min and mag filters
   //TODO allow unnormalized coordinates
    VkFilter filter;
    switch (desc.interpolation) {
    case(SamplerInterpolation::Linear): filter = VK_FILTER_LINEAR; break;
    case(SamplerInterpolation::Nearest): filter = VK_FILTER_NEAREST; break;
    default: throw std::runtime_error("unsupported sampler interpolation");
    }

    VkSamplerAddressMode wrapping;
    switch (desc.wrapping) {
    case(SamplerWrapping::Clamp): wrapping = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
    case(SamplerWrapping::Color): wrapping = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
    case(SamplerWrapping::Repeat): wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
    default: throw std::runtime_error("unsupported sampler wrapping mode");
    }

    VkBorderColor borderColor;
    switch (desc.borderColor) {
    case(SamplerBorderColor::Black): borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; break;
    case(SamplerBorderColor::White): borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; break;
    default: throw std::runtime_error("unsupported sampler border color");
    }

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
    samplerInfo.borderColor = borderColor;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    const auto result = vkCreateSampler(vkContext.device, &samplerInfo, nullptr, &sampler);
    //checkVulkanResult(result);

    return sampler;
}