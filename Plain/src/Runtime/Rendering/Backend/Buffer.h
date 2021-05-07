#pragma once
#include "pch.h"
#include "Resources.h"

Buffer createVulkanBuffer(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, 
    const VkBufferUsageFlags usage, const uint32_t memoryFlags);

void destroyVulkanBuffer(const Buffer& buffer);