#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

VkRenderPass createVulkanRenderPass(const std::vector<Attachment>& attachments);