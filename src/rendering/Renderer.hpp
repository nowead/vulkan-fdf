#pragma once

#include "src/core/VulkanDevice.hpp"
#include "src/rendering/VulkanSwapchain.hpp"
#include "src/rendering/VulkanPipeline.hpp"
#include "src/rendering/CommandManager.hpp"
#include "src/rendering/SyncManager.hpp"
#include "src/resources/VulkanImage.hpp"
#include "src/resources/VulkanBuffer.hpp"
#include "src/scene/Mesh.hpp"
#include "src/utils/VulkanCommon.hpp"
#include "src/utils/Vertex.hpp"

#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>

/**
 * @brief High-level renderer managing all Vulkan subsystems and rendering logic
 *
 * Responsibilities:
 * - Own and manage all Vulkan subsystems (device, swapchain, pipeline, etc.)
 * - Manage rendering resources (textures, meshes, buffers)
 * - Handle frame rendering and presentation
 * - Coordinate swapchain recreation
 * - Manage descriptor sets
 */
class Renderer {
public:
    /**
     * @brief Construct renderer with window
     * @param window GLFW window for surface creation
     * @param validationLayers Validation layers to enable
     * @param enableValidation Whether to enable validation
     */
    Renderer(GLFWwindow* window,
             const std::vector<const char*>& validationLayers,
             bool enableValidation);

    ~Renderer() = default;

    // Disable copy and move
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /**
     * @brief Load model from file
     * @param modelPath Path to model file
     */
    void loadModel(const std::string& modelPath);

    /**
     * @brief Load texture from file
     * @param texturePath Path to texture file
     */
    void loadTexture(const std::string& texturePath);

    /**
     * @brief Draw a single frame
     */
    void drawFrame();

    /**
     * @brief Wait for device to be idle (for cleanup)
     */
    void waitIdle();

    /**
     * @brief Handle framebuffer resize
     */
    void handleFramebufferResize();

private:
    // Window reference
    GLFWwindow* window;

    // Core subsystems
    std::unique_ptr<VulkanDevice> device;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<CommandManager> commandManager;
    std::unique_ptr<SyncManager> syncManager;

    // Resources
    std::unique_ptr<VulkanImage> depthImage;
    std::unique_ptr<VulkanImage> textureImage;
    std::unique_ptr<Mesh> mesh;

    // Uniform buffers (per frame in flight)
    std::vector<std::unique_ptr<VulkanBuffer>> uniformBuffers;

    // Descriptor management
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    // Frame synchronization
    uint32_t currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    // For uniform buffer animation
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    // Private initialization methods
    void createDepthResources();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateDescriptorSets();

    // Rendering methods
    void recordCommandBuffer(uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentImage);
    void transitionImageLayout(
        uint32_t imageIndex,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::AccessFlags2 srcAccessMask,
        vk::AccessFlags2 dstAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 dstStageMask);

    // Swapchain recreation
    void recreateSwapchain();

    // Utility
    vk::Format findDepthFormat();
};
