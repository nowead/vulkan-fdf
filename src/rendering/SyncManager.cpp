#include "SyncManager.hpp"

SyncManager::SyncManager(VulkanDevice& device, uint32_t maxFramesInFlight, uint32_t swapchainImageCount)
    : device(device), maxFramesInFlight(maxFramesInFlight) {
    imageAvailableSemaphores.reserve(maxFramesInFlight);
    renderFinishedSemaphores.reserve(swapchainImageCount);  // One per swapchain image
    inFlightFences.reserve(maxFramesInFlight);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled  // Start signaled so first frame doesn't wait
    };

    // Image available semaphores: one per frame in flight
    for (uint32_t i = 0; i < maxFramesInFlight; i++) {
        imageAvailableSemaphores.emplace_back(device.getDevice(), semaphoreInfo);
        inFlightFences.emplace_back(device.getDevice(), fenceInfo);
    }

    // Render finished semaphores: one per swapchain image
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        renderFinishedSemaphores.emplace_back(device.getDevice(), semaphoreInfo);
    }
}


vk::Semaphore SyncManager::getImageAvailableSemaphore(uint32_t frameIndex) const {
    return *imageAvailableSemaphores[frameIndex];
}

vk::Semaphore SyncManager::getRenderFinishedSemaphore(uint32_t frameIndex) const {
    return *renderFinishedSemaphores[frameIndex];
}

vk::Fence SyncManager::getInFlightFence(uint32_t frameIndex) const {
    return *inFlightFences[frameIndex];
}

void SyncManager::waitForFence(uint32_t frameIndex) {
    while (vk::Result::eTimeout == device.getDevice().waitForFences(
        *inFlightFences[frameIndex], vk::True, UINT64_MAX)) {
        // Wait until fence is signaled
    }
}

void SyncManager::resetFence(uint32_t frameIndex) {
    device.getDevice().resetFences(*inFlightFences[frameIndex]);
}
