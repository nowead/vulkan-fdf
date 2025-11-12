#pragma once

#include "../utils/VulkanCommon.hpp"
#include "../core/VulkanDevice.hpp"
#include <vector>

/**
 * @brief Manages Vulkan swapchain and associated image views
 *
 * Handles swapchain creation, recreation, image view management,
 * and provides utilities for swapchain configuration.
 */
class VulkanSwapchain {
public:
    /**
     * @brief Construct swapchain with optimal settings
     * @param device Vulkan device reference
     * @param window GLFW window for framebuffer size queries
     */
    VulkanSwapchain(VulkanDevice& device, GLFWwindow* window);

    ~VulkanSwapchain() = default;

    // Disable copy, enable move construction only
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&) = default;
    VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

    // Swapchain operations
    void recreate();
    void cleanup();
    
    // Image acquisition
    std::pair<vk::Result, uint32_t> acquireNextImage(
        uint64_t timeout,
        vk::Semaphore semaphore,
        vk::Fence fence = nullptr);

    // Accessors
    vk::SwapchainKHR getSwapchain() const { return *swapchain; }
    const std::vector<vk::Image>& getImages() const { return images; }
    vk::ImageView getImageView(uint32_t index) const { return *imageViews[index]; }
    const std::vector<vk::raii::ImageView>& getImageViews() const { return imageViews; }
    vk::Format getFormat() const { return surfaceFormat.format; }
    vk::Extent2D getExtent() const { return extent; }
    uint32_t getImageCount() const { return static_cast<uint32_t>(images.size()); }

private:
    VulkanDevice& device;
    GLFWwindow* window;

    vk::raii::SwapchainKHR swapchain = nullptr;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> imageViews;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::Extent2D extent;

    void createSwapchain();
    void createImageViews();

    // Helper functions for swapchain configuration
    static uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities);
    static vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats);
    static vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& modes);
    vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
};
