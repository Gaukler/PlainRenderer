#include "pch.h"
#include "VulkanVertexInput.h"

const VkFormat vertexInputFormatsPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VK_FORMAT_R32G32B32_SFLOAT,         // position
    VK_FORMAT_R16G16_SFLOAT,            // uvs
    VK_FORMAT_A2R10G10B10_SNORM_PACK32, // normals
    VK_FORMAT_A2R10G10B10_SNORM_PACK32, // tangent
    VK_FORMAT_A2R10G10B10_SNORM_PACK32  // bitanget
};

std::vector<VkVertexInputAttributeDescription> createVertexInputDescriptions(const VertexInputFlags inputFlags) {

    std::vector<VkVertexInputAttributeDescription> attributes;
    uint32_t currentOffset = 0;

    for (uint32_t location = 0; location < VERTEX_INPUT_ATTRIBUTE_COUNT; location++) {
        const bool inputIsUsed = bool(vertexInputFlagPerLocation[location] & inputFlags);
        if (inputIsUsed) {
            VkVertexInputAttributeDescription attribute;
            attribute.location = location;
            attribute.binding = 0;
            attribute.format = vertexInputFormatsPerLocation[(size_t)location];
            attribute.offset = currentOffset;
            attributes.push_back(attribute);
        }
        // vertex buffer has attributes even if not used
        currentOffset += vertexInputBytePerLocation[(size_t)location];
    }

    return attributes;
}

VkVertexInputBindingDescription createVertexInputBindingDescription(const VertexFormat format) {
    VkVertexInputBindingDescription vertexBinding;
    vertexBinding.binding = 0;
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    switch (format) {
        case VertexFormat::Full: vertexBinding.stride = getFullVertexFormatByteSize(); break;
        case VertexFormat::PositionOnly: vertexBinding.stride = vertexInputPositionByteSize; break;
        default: vertexBinding.stride = 0; std::cout << "Warning: unknown vertex format\n"; break;
    }
    return vertexBinding;
}

VkPipelineVertexInputStateCreateInfo createPipelineVertexInputStateCreateInfo(
    const VkVertexInputBindingDescription& bindingDescription,
    const std::vector<VkVertexInputAttributeDescription>& attributes) {

    VkPipelineVertexInputStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.vertexBindingDescriptionCount = 1;
    info.pVertexBindingDescriptions = &bindingDescription;
    info.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
    info.pVertexAttributeDescriptions = attributes.data();
    return info;
}