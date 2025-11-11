#include "VulkanImage.hpp"

VulkanImage::VulkanImage(
    VulkanDevice& device,
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::ImageAspectFlags aspectFlags)
    : device(device), width(width), height(height), format(format), aspectFlags(aspectFlags) {
    createImage(tiling, usage, properties);
    createImageView();
}

void VulkanImage::createImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    image = vk::raii::Image(device.getDevice(), imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties)
    };

    memory = vk::raii::DeviceMemory(device.getDevice(), allocInfo);
    image.bindMemory(*memory, 0);
}

void VulkanImage::createImageView() {
    vk::ImageViewCreateInfo viewInfo{
        .image = *image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {aspectFlags, 0, 1, 0, 1}
    };
    imageView = vk::raii::ImageView(device.getDevice(), viewInfo);
}

void VulkanImage::transitionLayout(
    const vk::raii::CommandBuffer& cmdBuffer,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout) {

    vk::ImageMemoryBarrier barrier{
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *image,
        .subresourceRange = {aspectFlags, 0, 1, 0, 1}
    };

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    cmdBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
}

void VulkanImage::copyFromBuffer(const vk::raii::CommandBuffer& cmdBuffer, VulkanBuffer& buffer) {
    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    cmdBuffer.copyBufferToImage(buffer.getHandle(), *image, vk::ImageLayout::eTransferDstOptimal, region);
}

void VulkanImage::createSampler(
    vk::Filter magFilter,
    vk::Filter minFilter,
    vk::SamplerAddressMode addressMode) {

    vk::PhysicalDeviceProperties properties = device.getPhysicalDevice().getProperties();
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = magFilter,
        .minFilter = minFilter,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::True,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False
    };

    sampler = vk::raii::Sampler(device.getDevice(), samplerInfo);
}
