#pragma once

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace Platform {

// Platform-specific constants
#ifdef __linux__
	constexpr bool USE_DYNAMIC_RENDERING = false;
	constexpr bool USE_VULKAN_1_3_FEATURES = false;
	constexpr uint32_t REQUIRED_VULKAN_VERSION = VK_API_VERSION_1_1;
#elif defined(__APPLE__)
	constexpr bool USE_DYNAMIC_RENDERING = true;
	constexpr bool USE_VULKAN_1_3_FEATURES = true;
	constexpr uint32_t REQUIRED_VULKAN_VERSION = VK_API_VERSION_1_3;
#else
	// Windows: Full Vulkan 1.3 support
	constexpr bool USE_DYNAMIC_RENDERING = true;
	constexpr bool USE_VULKAN_1_3_FEATURES = true;
	constexpr uint32_t REQUIRED_VULKAN_VERSION = VK_API_VERSION_1_3;
#endif

// Get platform-specific required device extensions
inline std::vector<const char*> getRequiredDeviceExtensions() {
#ifdef __linux__
	// Linux: Minimal requirements for WSL/llvmpipe compatibility
	return {
		vk::KHRSwapchainExtensionName
	};
#elif defined(__APPLE__)
	// macOS: Full Vulkan 1.3 requirements + MoltenVK portability
	return {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRDynamicRenderingExtensionName,
		"VK_KHR_portability_subset"
	};
#else
	// Windows: Full Vulkan 1.3 requirements
	return {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRDynamicRenderingExtensionName
	};
#endif
}

// Check if device supports required features
inline bool checkDeviceFeatureSupport(const vk::raii::PhysicalDevice& device) {
#ifdef __linux__
	// Linux: Relaxed requirements for WSL/llvmpipe compatibility
	// No required features - accept any device
	return true;
#else
	// macOS/Windows: Full Vulkan 1.3 feature requirements
	auto features13 = device.getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceDynamicRenderingFeatures
	>();

	auto& vulkan13Features = features13.get<vk::PhysicalDeviceVulkan13Features>();
	auto& dynamicRenderingFeatures = features13.get<vk::PhysicalDeviceDynamicRenderingFeatures>();

	return vulkan13Features.synchronization2 && dynamicRenderingFeatures.dynamicRendering;
#endif
}

// Helper function to create device create info
// Note: Feature chain setup is still done in VulkanDevice.cpp due to complexity of StructureChain
inline vk::DeviceQueueCreateInfo createDeviceQueueCreateInfo(uint32_t queueFamilyIndex, float* queuePriority) {
	return vk::DeviceQueueCreateInfo{
		.queueFamilyIndex = queueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = queuePriority
	};
}

} // namespace Platform
