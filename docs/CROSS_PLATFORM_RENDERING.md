# Cross-Platform Rendering Support

## Overview

This project supports multiple rendering paths to ensure compatibility across different platforms and Vulkan implementations:

- **Linux (WSL/llvmpipe)**: Vulkan 1.1 with traditional render passes
- **macOS**: Vulkan 1.3 with dynamic rendering via MoltenVK
- **Windows**: Vulkan 1.3 with dynamic rendering

## Platform-Specific Requirements

### Linux
- **Vulkan Version**: 1.1.182+
- **Required Extensions**: `VK_KHR_swapchain`
- **Required Features**: None (all optional)
- **SPIR-V Version**: 1.3
- **Rendering Method**: Traditional render passes

### macOS
- **Vulkan Version**: 1.3+
- **Required Extensions**:
  - `VK_KHR_swapchain`
  - `VK_KHR_spirv_1_4`
  - `VK_KHR_synchronization2`
  - `VK_KHR_create_renderpass2`
  - `VK_KHR_portability_subset` (MoltenVK)
- **Required Features**:
  - `shaderDrawParameters`
  - `samplerAnisotropy`
  - `synchronization2`
  - `dynamicRendering`
  - `extendedDynamicState`
- **SPIR-V Version**: 1.4
- **Rendering Method**: Dynamic rendering

### Windows
- **Vulkan Version**: 1.3+
- **Required Extensions**:
  - `VK_KHR_swapchain`
  - `VK_KHR_spirv_1_4`
  - `VK_KHR_synchronization2`
  - `VK_KHR_create_renderpass2`
- **Required Features**:
  - `shaderDrawParameters`
  - `samplerAnisotropy`
  - `synchronization2`
  - `dynamicRendering`
  - `extendedDynamicState`
- **SPIR-V Version**: 1.4
- **Rendering Method**: Dynamic rendering

## Implementation Details

### Conditional Compilation

The project uses `#ifdef __linux__` preprocessor directives to enable platform-specific code paths:

```cpp
#ifdef __linux__
    // Linux-specific code (Vulkan 1.1)
#else
    // macOS/Windows code (Vulkan 1.3)
#endif
```

### Modified Components

#### 1. VulkanDevice (`src/core/VulkanDevice.cpp`)

**Device Extensions**:
```cpp
#ifdef __linux__
    // Minimal requirements for WSL/llvmpipe
    requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName
    };
#else
    // Full Vulkan 1.3 requirements
    requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };
#endif
```

**Device Features**:
```cpp
#ifdef __linux__
    // Accept all devices on Linux
    bool supportsRequiredFeatures = true;
#else
    // Require full Vulkan 1.3 features
    bool supportsRequiredFeatures =
        shaderDrawParameters &&
        samplerAnisotropy &&
        synchronization2 &&
        dynamicRendering &&
        extendedDynamicState;
#endif
```

**Logical Device Creation**:
```cpp
#ifdef __linux__
    // Use simple Vulkan 1.0 features
    vk::PhysicalDeviceFeatures2 featureChain = {
        .features = availableFeatures
    };
#else
    // Use Vulkan 1.3 feature chain
    vk::StructureChain<...> featureChain = {...};
#endif
```

#### 2. VulkanSwapchain (`src/rendering/VulkanSwapchain.cpp`)

**Render Pass Creation** (Linux only):
```cpp
#ifdef __linux__
void VulkanSwapchain::createRenderPass(vk::Format depthFormat) {
    // Create traditional render pass with:
    // - Color attachment (swapchain format)
    // - Depth attachment
    // - Single subpass
    // - Subpass dependencies
}

void VulkanSwapchain::createFramebuffers(const std::vector<vk::ImageView>& depthImageViews) {
    // Create framebuffers for each swapchain image
    // with color + depth attachments
}
#endif
```

#### 3. VulkanPipeline (`src/rendering/VulkanPipeline.cpp`)

**Pipeline Creation**:
```cpp
#ifdef __linux__
    // Traditional pipeline with render pass
    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .renderPass = renderPass,
        .subpass = 0,
        // ... other settings
    };
#else
    // Dynamic rendering pipeline
    vk::StructureChain<
        vk::GraphicsPipelineCreateInfo,
        vk::PipelineRenderingCreateInfo
    > pipelineCreateInfoChain = {
        {.renderPass = nullptr, ...},
        {.colorAttachmentCount = 1, ...}
    };
#endif
```

#### 4. Renderer (`src/rendering/Renderer.cpp`)

**Initialization**:
```cpp
#ifdef __linux__
    // Create render pass and framebuffers
    swapchain->createRenderPass(findDepthFormat());
    swapchain->createFramebuffers(depthViews);

    // Create pipeline with render pass
    pipeline = std::make_unique<VulkanPipeline>(
        *device, *swapchain, "shaders/slang.spv",
        findDepthFormat(), swapchain->getRenderPass());
#else
    // Create pipeline with dynamic rendering
    pipeline = std::make_unique<VulkanPipeline>(
        *device, *swapchain, "shaders/slang.spv", findDepthFormat());
#endif
```

**Command Recording**:
```cpp
#ifdef __linux__
    // Traditional render pass
    vk::RenderPassBeginInfo renderPassInfo{...};
    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    // Draw commands...

    commandBuffer.endRenderPass();
#else
    // Dynamic rendering
    vk::RenderingInfo renderingInfo{...};
    commandBuffer.beginRendering(renderingInfo);

    // Draw commands...

    commandBuffer.endRendering();
#endif
```

#### 5. VulkanImage (`src/resources/VulkanImage.cpp`)

**Sampler Anisotropy**:
```cpp
void VulkanImage::createSampler(...) {
    vk::PhysicalDeviceFeatures features = device.getPhysicalDevice().getFeatures();

    // Only enable anisotropy if device supports it
    bool enableAnisotropy = features.samplerAnisotropy;

    vk::SamplerCreateInfo samplerInfo{
        .anisotropyEnable = enableAnisotropy ? vk::True : vk::False,
        .maxAnisotropy = enableAnisotropy ? maxAnisotropy : 1.0f,
        // ...
    };
}
```

#### 6. CMakeLists.txt

**Shader Compilation**:
```cmake
if(UNIX AND NOT APPLE)
    # Linux: SPIR-V 1.3 for Vulkan 1.1 compatibility
    add_custom_command(
        OUTPUT ${SHADERS_DIR}/slang.spv
        COMMAND ... slangc ... -profile spirv_1_3 ...
    )
elseif(APPLE)
    # macOS: SPIR-V 1.4
    add_custom_command(
        OUTPUT ${SHADERS_DIR}/slang.spv
        COMMAND ... slangc ... -profile spirv_1_4 ...
    )
else()
    # Windows: SPIR-V 1.4
    add_custom_command(
        OUTPUT ${SHADERS_DIR}/slang.spv
        COMMAND ... slangc ... -profile spirv_1_4 ...
    )
endif()
```

## Testing

### Linux (WSL)
```bash
make run
```

Expected output:
```
WARNING: lavapipe is not a conformant vulkan implementation, testing use only.
```

This warning is normal for llvmpipe/lavapipe.

### macOS
```bash
make run
```

Should run with full Vulkan 1.3 features via MoltenVK.

### Windows
```bash
make run
```

Should run with native Vulkan 1.3 support.

## Debugging

### Enable Verbose Device Selection

To see which device is being selected, temporarily add debug output in `VulkanDevice::pickPhysicalDevice()`:

```cpp
auto props = device.getProperties();
std::cout << "Checking device: " << props.deviceName << std::endl;
```

### Check Supported Extensions

```cpp
auto extensions = device.enumerateDeviceExtensionProperties();
for (const auto& ext : extensions) {
    std::cout << ext.extensionName << std::endl;
}
```

### Check Supported Features

```cpp
auto features = device.getFeatures();
std::cout << "Anisotropy: " << features.samplerAnisotropy << std::endl;
```

## Known Limitations

### Linux (llvmpipe/lavapipe)
- No anisotropic filtering support
- Software rendering (slower performance)
- Limited to Vulkan 1.1 feature set
- Uses traditional render passes (older API)

### macOS (MoltenVK)
- Requires `VK_KHR_portability_subset` extension
- Some Vulkan features translated to Metal

## Future Improvements

1. **Runtime Detection**: Detect rendering capabilities at runtime instead of compile-time
2. **Fallback Pipeline**: Automatically switch to traditional render passes if dynamic rendering is unavailable
3. **Feature Query System**: Centralized system for querying and adapting to available features
4. **Performance Profiling**: Compare performance between traditional and dynamic rendering

## References

- [Vulkan 1.1 Specification](https://registry.khronos.org/vulkan/specs/1.1/)
- [Vulkan 1.3 Specification](https://registry.khronos.org/vulkan/specs/1.3/)
- [Dynamic Rendering](https://docs.vulkan.org/samples/latest/samples/extensions/dynamic_rendering/README.html)
- [MoltenVK Documentation](https://github.com/KhronosGroup/MoltenVK)
