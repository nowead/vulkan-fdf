#pragma once

#include "../utils/VulkanCommon.hpp"
#include "../core/VulkanDevice.hpp"
#include "VulkanBuffer.hpp"
#include <optional>

class VulkanImage {
public:
    VulkanImage(
        VulkanDevice& device,
        uint32_t width,
        uint32_t height,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::ImageAspectFlags aspectFlags);

    ~VulkanImage() = default; // RAII handles cleanup

    // Disable copy, enable move construction only
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;
    VulkanImage(VulkanImage&&) = default;
    VulkanImage& operator=(VulkanImage&&) = delete;  // Move assignment not supported due to reference member

    // Layout transitions
    void transitionLayout(
        const vk::raii::CommandBuffer& cmdBuffer,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout);

    void copyFromBuffer(const vk::raii::CommandBuffer& cmdBuffer, VulkanBuffer& buffer);

    // Sampler operations
    void createSampler(
        vk::Filter magFilter = vk::Filter::eLinear,
        vk::Filter minFilter = vk::Filter::eLinear,
        vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eRepeat);

    // Accessors
    vk::Image getImage() const { return *image; }
    vk::ImageView getImageView() const { return *imageView; }
    vk::Sampler getSampler() const { return sampler ? **sampler : nullptr; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

private:
    VulkanDevice& device;
    vk::raii::Image image = nullptr;
    vk::raii::DeviceMemory memory = nullptr;
    vk::raii::ImageView imageView = nullptr;
    std::optional<vk::raii::Sampler> sampler;

    uint32_t width;
    uint32_t height;
    vk::Format format;
    vk::ImageAspectFlags aspectFlags;

    void createImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
    void createImageView();
};
