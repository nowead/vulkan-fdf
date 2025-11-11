#include "VulkanBuffer.hpp"

VulkanBuffer::VulkanBuffer(
    VulkanDevice& device,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
    : device(device), size(size) {
    createBuffer(size, usage, properties);
}

void VulkanBuffer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    buffer = vk::raii::Buffer(device.getDevice(), bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties)
    };

    memory = vk::raii::DeviceMemory(device.getDevice(), allocInfo);
    buffer.bindMemory(*memory, 0);
}

void VulkanBuffer::map() {
    if (mappedData == nullptr) {
        mappedData = memory.mapMemory(0, size);
    }
}

void VulkanBuffer::unmap() {
    if (mappedData != nullptr) {
        memory.unmapMemory();
        mappedData = nullptr;
    }
}

void VulkanBuffer::copyData(const void* data, vk::DeviceSize copySize) {
    if (mappedData == nullptr) {
        throw std::runtime_error("Buffer is not mapped. Call map() before copyData()");
    }
    memcpy(mappedData, data, static_cast<size_t>(copySize));
}

void VulkanBuffer::copyFrom(VulkanBuffer& srcBuffer, const vk::raii::CommandBuffer& cmdBuffer) {
    vk::BufferCopy copyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = srcBuffer.getSize()
    };
    cmdBuffer.copyBuffer(srcBuffer.getHandle(), getHandle(), copyRegion);
}
