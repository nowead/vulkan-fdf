#include "Application.hpp"

#include <iostream>
#include <stdexcept>

Application::Application() {
    initWindow();
    initVulkan();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();
}

void Application::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void Application::initVulkan() {
    renderer = std::make_unique<Renderer>(window, validationLayers, enableValidationLayers);
    renderer->loadModel(MODEL_PATH);
    renderer->loadTexture(TEXTURE_PATH);
}

void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        renderer->drawFrame();
    }
    renderer->waitIdle();
}

void Application::cleanup() {
    // Renderer is automatically destroyed (unique_ptr)
    // Clean up renderer before destroying window
    renderer.reset();

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->renderer->handleFramebufferResize();
}
