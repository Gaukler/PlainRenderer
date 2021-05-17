#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkSemaphore createSemaphore();
VkFence     createFence();
void        waitForFence(const VkFence fence);