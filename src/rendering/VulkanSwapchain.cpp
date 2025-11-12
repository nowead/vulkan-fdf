#include "VulkanSwapchain.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cassert>

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, GLFWwindow* window)
    : device(device), window(window) {
    createSwapchain();
    createImageViews();
}

void VulkanSwapchain::createSwapchain() {
    auto surfaceCapabilities = device.getPhysicalDevice().getSurfaceCapabilitiesKHR(*device.getSurface());
    extent = chooseExtent(surfaceCapabilities);
    surfaceFormat = chooseSurfaceFormat(device.getPhysicalDevice().getSurfaceFormatsKHR(*device.getSurface()));

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = *device.getSurface(),
        .minImageCount = chooseImageCount(surfaceCapabilities),
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = choosePresentMode(device.getPhysicalDevice().getSurfacePresentModesKHR(*device.getSurface())),
        .clipped = true
    };

    swapchain = vk::raii::SwapchainKHR(device.getDevice(), createInfo);
    images = swapchain.getImages();
}

void VulkanSwapchain::createImageViews() {
    assert(imageViews.empty());

    vk::ImageViewCreateInfo viewInfo{
        .viewType = vk::ImageViewType::e2D,
        .format = surfaceFormat.format,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
    };

    for (auto image : images) {
        viewInfo.image = image;
        imageViews.emplace_back(device.getDevice(), viewInfo);
    }
}

void VulkanSwapchain::cleanup() {
    imageViews.clear();
    swapchain = nullptr;
}

void VulkanSwapchain::recreate() {
    // Wait for device to be idle before recreating swapchain
    device.getDevice().waitIdle();
    
    cleanup();
    createSwapchain();
    createImageViews();
}

std::pair<vk::Result, uint32_t> VulkanSwapchain::acquireNextImage(
    uint64_t timeout,
    vk::Semaphore semaphore,
    vk::Fence fence) {
    return swapchain.acquireNextImage(timeout, semaphore, fence);
}

uint32_t VulkanSwapchain::chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
    auto imageCount = std::max(3u, capabilities.minImageCount);
    if ((0 < capabilities.maxImageCount) && (capabilities.maxImageCount < imageCount)) {
        imageCount = capabilities.maxImageCount;
    }
    return imageCount;
}

vk::SurfaceFormatKHR VulkanSwapchain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    assert(!formats.empty());
    
    for (const auto& format : formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb && 
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    
    return formats[0];
}

vk::PresentModeKHR VulkanSwapchain::choosePresentMode(const std::vector<vk::PresentModeKHR>& modes) {
    // FIFO is always supported
    for (const auto& mode : modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return vk::PresentModeKHR::eMailbox;
        }
    }
    
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanSwapchain::chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != 0xFFFFFFFF) {
        return capabilities.currentExtent;
    }
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };
}
