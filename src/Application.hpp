#pragma once

#include "src/rendering/Renderer.hpp"

#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <string>

/**
 * @brief Top-level application class managing window and main loop
 *
 * Responsibilities:
 * - GLFW window creation and management
 * - Main event loop
 * - Renderer lifecycle management
 * - Window resize callbacks
 */
class Application {
public:
    /**
     * @brief Construct application with default window size and validation settings
     */
    Application();

    /**
     * @brief Destructor - ensures proper cleanup order
     */
    ~Application();

    // Disable copy and move
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * @brief Run the application (initialize, loop, cleanup)
     */
    void run();

private:
    // Window configuration
    static constexpr uint32_t WINDOW_WIDTH = 800;
    static constexpr uint32_t WINDOW_HEIGHT = 600;
    static constexpr const char* WINDOW_TITLE = "Vulkan";

    // Asset paths
    static constexpr const char* MODEL_PATH = "models/viking_room.obj";
    static constexpr const char* TEXTURE_PATH = "textures/viking_room.png";

    // Validation layers
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    static constexpr bool enableValidationLayers = false;
#else
    static constexpr bool enableValidationLayers = true;
#endif

    // Members
    GLFWwindow* window = nullptr;
    std::unique_ptr<Renderer> renderer;

    // Initialization
    void initWindow();
    void initVulkan();

    // Main loop
    void mainLoop();

    // Cleanup
    void cleanup();

    // Callbacks
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
