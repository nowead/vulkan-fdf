#pragma once

#include "../utils/VulkanCommon.hpp"
#include "../core/VulkanDevice.hpp"

class VulkanBuffer {
public:
    VulkanBuffer(
        VulkanDevice& device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties);

    ~VulkanBuffer() = default; // RAII handles cleanup automatically

    // Disable copy, enable move construction only
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&&) = default;
    VulkanBuffer& operator=(VulkanBuffer&&) = delete;  // Move assignment not supported due to reference member

    // Data operations
    void map();
    void unmap();
    void copyData(const void* data, vk::DeviceSize size);
    void copyFrom(VulkanBuffer& srcBuffer, const vk::raii::CommandBuffer& cmdBuffer);

    // Accessors
    vk::Buffer getHandle() const { return *buffer; }
    vk::DeviceSize getSize() const { return size; }
    void* getMappedData() { return mappedData; }

private:
    VulkanDevice& device;
    vk::raii::Buffer buffer = nullptr;
    vk::raii::DeviceMemory memory = nullptr;
    vk::DeviceSize size;
    void* mappedData = nullptr;

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
};
