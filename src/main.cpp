// Standard library headers
#include <assert.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>

// Project utility headers
#include "src/utils/VulkanCommon.hpp"
#include "src/utils/Vertex.hpp"
#include "src/utils/FileUtils.hpp"

// Project core headers
#include "src/core/VulkanDevice.hpp"
#include "src/resources/VulkanBuffer.hpp"
#include "src/resources/VulkanImage.hpp"
#include "src/rendering/SyncManager.hpp"
#include "src/rendering/CommandManager.hpp"
#include "src/rendering/VulkanSwapchain.hpp"
#include "src/rendering/VulkanPipeline.hpp"
#include "src/scene/Mesh.hpp"

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Third-party library implementations (only once)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow *                        window          = nullptr;
	std::unique_ptr<VulkanDevice>       vulkanDevice;
	std::unique_ptr<VulkanSwapchain>    swapchain;
	std::unique_ptr<VulkanPipeline>     pipeline;

	std::unique_ptr<VulkanImage> depthImage;
	std::unique_ptr<VulkanImage> textureImage;

	std::unique_ptr<Mesh> mesh;

	std::vector<std::unique_ptr<VulkanBuffer>> uniformBuffers;

	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	std::unique_ptr<CommandManager> commandManager;
	std::unique_ptr<SyncManager> syncManager;
	uint32_t currentFrame = 0;

	bool framebufferResized = false;

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void initVulkan() {
		// Create VulkanDevice (handles instance and physical device)
		vulkanDevice = std::make_unique<VulkanDevice>(validationLayers, enableValidationLayers);
		vulkanDevice->createSurface(window);
		vulkanDevice->createLogicalDevice(); // Must be called after surface creation
		swapchain = std::make_unique<VulkanSwapchain>(*vulkanDevice, window);
		pipeline = std::make_unique<VulkanPipeline>(*vulkanDevice, *swapchain, "shaders/slang.spv", findDepthFormat());
		commandManager = std::make_unique<CommandManager>(*vulkanDevice, vulkanDevice->getGraphicsQueueFamily(), MAX_FRAMES_IN_FLIGHT);
		createDepthResources();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createMesh();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		syncManager = std::make_unique<SyncManager>(*vulkanDevice, MAX_FRAMES_IN_FLIGHT, swapchain->getImageCount());
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}
		vulkanDevice->getDevice().waitIdle();
	}

	void cleanup() const {
		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void recreateSwapChain() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		swapchain->recreate();
		createDepthResources();
	}

	void createDepthResources() {
		vk::Format depthFormat = findDepthFormat();

		depthImage = std::make_unique<VulkanImage>(*vulkanDevice,
			swapchain->getExtent().width, swapchain->getExtent().height,
			depthFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::ImageAspectFlagBits::eDepth);
	}

	vk::Format findDepthFormat() {
		return vulkanDevice->findSupportedFormat(
			{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
			vk::ImageTiling::eOptimal,
			vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool hasStencilComponent(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		// Create staging buffer
		VulkanBuffer stagingBuffer(*vulkanDevice, imageSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		stagingBuffer.map();
		stagingBuffer.copyData(pixels, imageSize);
		stagingBuffer.unmap();

		stbi_image_free(pixels);

		// Create texture image
		textureImage = std::make_unique<VulkanImage>(*vulkanDevice,
			texWidth, texHeight,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vk::ImageAspectFlagBits::eColor);

		// Transition and copy
		auto commandBuffer = commandManager->beginSingleTimeCommands();
		textureImage->transitionLayout(*commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		textureImage->copyFromBuffer(*commandBuffer, stagingBuffer);
		textureImage->transitionLayout(*commandBuffer, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		commandManager->endSingleTimeCommands(*commandBuffer);
	}

	void createTextureImageView() {
		// Image view is created automatically in VulkanImage constructor
		// This function is now a no-op but kept for compatibility
	}

	void createTextureSampler() {
		// Create sampler for the texture image
		textureImage->createSampler();
	}









	void createMesh() {
		mesh = std::make_unique<Mesh>(*vulkanDevice, *commandManager);
		mesh->loadFromOBJ(MODEL_PATH);
	}

   void createUniformBuffers() {
		uniformBuffers.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			auto uniformBuffer = std::make_unique<VulkanBuffer>(*vulkanDevice, bufferSize,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			// Map the buffer for persistent mapping
			uniformBuffer->map();
			uniformBuffers.emplace_back(std::move(uniformBuffer));
		}
	}

	void createDescriptorPool() {
		std::array poolSize {
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
		};
		vk::DescriptorPoolCreateInfo poolInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes = poolSize.data()
		};
		descriptorPool = vk::raii::DescriptorPool(vulkanDevice->getDevice(), poolInfo);
	}

	void createDescriptorSets() {
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, pipeline->getDescriptorSetLayout());
		vk::DescriptorSetAllocateInfo allocInfo{
			.descriptorPool = descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data()
		};

		descriptorSets.clear();
		descriptorSets = vulkanDevice->getDevice().allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffers[i]->getHandle(),
				.offset = 0,
				.range = sizeof(UniformBufferObject)
			};
			vk::DescriptorImageInfo imageInfo{
				.sampler = textureImage->getSampler(),
				.imageView = textureImage->getImageView(),
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			};
			std::array descriptorWrites{
				vk::WriteDescriptorSet{
					.dstSet = descriptorSets[i],
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &bufferInfo
				},
				vk::WriteDescriptorSet{
					.dstSet = descriptorSets[i],
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &imageInfo
				}
			};
			vulkanDevice->getDevice().updateDescriptorSets(descriptorWrites, {});
		}
	}







	void recordCommandBuffer(uint32_t imageIndex) {
		commandManager->getCommandBuffer(currentFrame).begin({});
		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			imageIndex,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},                                                     // srcAccessMask (no need to wait for previous operations)
			vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
			vk::PipelineStageFlagBits2::eTopOfPipe,                   // srcStage
			vk::PipelineStageFlagBits2::eColorAttachmentOutput        // dstStage
		);
		// Transition depth image to depth attachment optimal layout
		vk::ImageMemoryBarrier2 depthBarrier = {
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = depthImage->getImage(),
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eDepth,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		vk::DependencyInfo depthDependencyInfo = {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &depthBarrier
		};
		commandManager->getCommandBuffer(currentFrame).pipelineBarrier2(depthDependencyInfo);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = swapchain->getImageView(imageIndex),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor
		};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = depthImage->getImageView(),
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea = { .offset = { 0, 0 }, .extent = swapchain->getExtent() },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};
		commandManager->getCommandBuffer(currentFrame).beginRendering(renderingInfo);
		pipeline->bind(commandManager->getCommandBuffer(currentFrame));
		commandManager->getCommandBuffer(currentFrame).setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchain->getExtent().width), static_cast<float>(swapchain->getExtent().height), 0.0f, 1.0f));
		commandManager->getCommandBuffer(currentFrame).setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain->getExtent()));
		mesh->bind(commandManager->getCommandBuffer(currentFrame));
		commandManager->getCommandBuffer(currentFrame).bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout(), 0, *descriptorSets[currentFrame], nullptr);
		mesh->draw(commandManager->getCommandBuffer(currentFrame));
		commandManager->getCommandBuffer(currentFrame).endRendering();
		// After rendering, transition the swapchain image to PRESENT_SRC
		transition_image_layout(
			imageIndex,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,                 // srcAccessMask
			{},                                                      // dstAccessMask
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
			vk::PipelineStageFlagBits2::eBottomOfPipe                  // dstStage
		);
		commandManager->getCommandBuffer(currentFrame).end();
	}

	void transition_image_layout(
		uint32_t imageIndex,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		vk::AccessFlags2 src_access_mask,
		vk::AccessFlags2 dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask
		) {
		vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = swapchain->getImages()[imageIndex],
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		vk::DependencyInfo dependency_info = {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};
		commandManager->getCommandBuffer(currentFrame).pipelineBarrier2(dependency_info);
	}


	void updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchain->getExtent().width) / static_cast<float>(swapchain->getExtent().height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffers[currentImage]->getMappedData(), &ubo, sizeof(ubo));
	}

	void drawFrame() {
		// Wait for the current frame's fence
		syncManager->waitForFence(currentFrame);

		// Acquire next swapchain image
		auto [result, imageIndex] = swapchain->acquireNextImage(
			UINT64_MAX,
			syncManager->getImageAvailableSemaphore(currentFrame),
			nullptr);

		if (result == vk::Result::eErrorOutOfDateKHR) {
			recreateSwapChain();
			return;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		updateUniformBuffer(currentFrame);

		// Reset fence and record command buffer
		syncManager->resetFence(currentFrame);
		commandManager->getCommandBuffer(currentFrame).reset();
		recordCommandBuffer(imageIndex);

		// Submit command buffer
		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		vk::Semaphore waitSemaphores[] = { syncManager->getImageAvailableSemaphore(currentFrame) };
		vk::Semaphore signalSemaphores[] = { syncManager->getRenderFinishedSemaphore(imageIndex) };

		const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = waitSemaphores,
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandManager->getCommandBuffer(currentFrame),
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = signalSemaphores
		};
		vulkanDevice->getGraphicsQueue().submit(submitInfo, syncManager->getInFlightFence(currentFrame));

		// Present
		vk::SwapchainKHR swapchainHandle = swapchain->getSwapchain();
		const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = signalSemaphores,
			.swapchainCount = 1,
			.pSwapchains = &swapchainHandle,
			.pImageIndices = &imageIndex
		};
		result = vulkanDevice->getGraphicsQueue().presentKHR(presentInfoKHR);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		} else if (result != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
};

int main() {
	try {
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}