#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VulkanImageFormats.h"
#include "VulkanTransfer.h"

void transferDataIntoImageImmediate(Image& target, const Data& data, TransferResources& transferResources);