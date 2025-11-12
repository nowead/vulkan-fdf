#pragma once

#include "../utils/VulkanCommon.hpp"
#include "../core/VulkanDevice.hpp"
#include <vector>

/**
 * @brief Manages synchronization primitives (semaphores and fences) for frame rendering
 *
 * Handles the creation and management of semaphores for image acquisition and presentation,
 * and fences for CPU-GPU synchronization across multiple frames in flight.
 */
class SyncManager {
public:
    /**
     * @brief Construct synchronization objects for rendering
     * @param device Vulkan device reference
     * @param maxFramesInFlight Maximum number of frames being processed simultaneously
     * @param swapchainImageCount Number of swapchain images (for per-image semaphores)
     */
    SyncManager(VulkanDevice& device, uint32_t maxFramesInFlight, uint32_t swapchainImageCount);

    ~SyncManager() = default;

    // Disable copy, enable move construction only
    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;
    SyncManager(SyncManager&&) = default;
    SyncManager& operator=(SyncManager&&) = delete;

    // Accessors for synchronization objects
    vk::Semaphore getImageAvailableSemaphore(uint32_t frameIndex) const;
    vk::Semaphore getRenderFinishedSemaphore(uint32_t imageIndex) const;  // Use image index for per-image semaphores
    vk::Fence getInFlightFence(uint32_t frameIndex) const;

    // Fence operations
    void waitForFence(uint32_t frameIndex);
    void resetFence(uint32_t frameIndex);

    uint32_t getMaxFramesInFlight() const { return maxFramesInFlight; }

private:
    VulkanDevice& device;
    uint32_t maxFramesInFlight;

    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;
};
