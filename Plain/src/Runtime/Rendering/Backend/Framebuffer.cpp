#include "pch.h"
#include "Framebuffer.h"
#include "VulkanContext.h"

void destroyFramebuffers(const std::vector<VkFramebuffer>& framebuffers) {
    for (const auto& f : framebuffers) {
        vkDestroyFramebuffer(vkContext.device, f, nullptr);
    }
}