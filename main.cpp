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

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Third-party library implementations (only once)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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
	vk::raii::SwapchainKHR              swapChain       = nullptr;
	std::vector<vk::Image>              swapChainImages;
	vk::SurfaceFormatKHR                swapChainSurfaceFormat;
	vk::Extent2D                        swapChainExtent;
	std::vector<vk::raii::ImageView>    swapChainImageViews;

	vk::raii::DescriptorSetLayout		descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout            pipelineLayout      = nullptr;
	vk::raii::Pipeline                  graphicsPipeline    = nullptr;

	vk::raii::Image 					depthImage 			= nullptr;
	vk::raii::DeviceMemory 				depthImageMemory 	= nullptr;
	vk::raii::ImageView 				depthImageView 		= nullptr;

	vk::raii::Image 					textureImage 		= nullptr;
	vk::raii::DeviceMemory 				textureImageMemory 	= nullptr;
	vk::raii::ImageView					textureImageView 	= nullptr;
	vk::raii::Sampler					textureSampler 		= nullptr;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	vk::raii::Buffer vertexBuffer 				= nullptr;
	vk::raii::DeviceMemory vertexBufferMemory 	= nullptr;
	vk::raii::Buffer indexBuffer 				= nullptr;
	vk::raii::DeviceMemory indexBufferMemory 	= nullptr;

	std::vector<vk::raii::Buffer> uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	vk::raii::CommandPool commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphore;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphore;
	std::vector<vk::raii::Fence> inFlightFences;
	uint32_t semaphoreIndex = 0;
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
		createSwapChain();
		createImageViews();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createDepthResources();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		loadModel();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}
		vulkanDevice->getDevice().waitIdle();
	}

	void cleanupSwapChain() {
		swapChainImageViews.clear();
		swapChain = nullptr;
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

		vulkanDevice->getDevice().waitIdle();

		cleanupSwapChain();
		createSwapChain();
		createImageViews();
		createDepthResources();
	}

	void createSwapChain() {
		auto surfaceCapabilities = vulkanDevice->getPhysicalDevice().getSurfaceCapabilitiesKHR( *vulkanDevice->getSurface() );
		swapChainExtent          = chooseSwapExtent( surfaceCapabilities );
		swapChainSurfaceFormat   = chooseSwapSurfaceFormat( vulkanDevice->getPhysicalDevice().getSurfaceFormatsKHR( *vulkanDevice->getSurface() ) );
		vk::SwapchainCreateInfoKHR swapChainCreateInfo{ .surface          = *vulkanDevice->getSurface(),
														.minImageCount    = chooseSwapMinImageCount( surfaceCapabilities ),
														.imageFormat      = swapChainSurfaceFormat.format,
														.imageColorSpace  = swapChainSurfaceFormat.colorSpace,
														.imageExtent      = swapChainExtent,
														.imageArrayLayers = 1,
														.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
														.imageSharingMode = vk::SharingMode::eExclusive,
														.preTransform     = surfaceCapabilities.currentTransform,
														.compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
														.presentMode      = chooseSwapPresentMode( vulkanDevice->getPhysicalDevice().getSurfacePresentModesKHR( *vulkanDevice->getSurface() ) ),
														.clipped          = true };

		swapChain = vk::raii::SwapchainKHR( vulkanDevice->getDevice(), swapChainCreateInfo );
		swapChainImages = swapChain.getImages();
	}

	void createImageViews() {
		assert(swapChainImageViews.empty());

		vk::ImageViewCreateInfo imageViewCreateInfo{
			.viewType = vk::ImageViewType::e2D,
			.format = swapChainSurfaceFormat.format,
			.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };
		for ( auto image : swapChainImages )
		{
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back( vulkanDevice->getDevice(), imageViewCreateInfo );
		}
	}

	void createDescriptorSetLayout() {
		std::array bindings = {
			vk::DescriptorSetLayoutBinding( 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
			vk::DescriptorSetLayoutBinding( 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{ .bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data() };
		descriptorSetLayout = vk::raii::DescriptorSetLayout( vulkanDevice->getDevice(), layoutInfo );
	}

	void createGraphicsPipeline() {
		vk::raii::ShaderModule shaderModule = createShaderModule(FileUtils::readFile("shaders/slang.spv"));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule,  .pName = "vertMain" };
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{ .stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain" };
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
			.pVertexAttributeDescriptions = attributeDescriptions.data()
		};
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
			.topology = vk::PrimitiveTopology::eTriangleList,
			.primitiveRestartEnable = vk::False
		};
		vk::PipelineViewportStateCreateInfo viewportState{
			.viewportCount = 1,
			.scissorCount = 1
		};
		vk::PipelineRasterizationStateCreateInfo rasterizer{
			.depthClampEnable = vk::False,
			.rasterizerDiscardEnable = vk::False,
			.polygonMode = vk::PolygonMode::eFill,
			.cullMode = vk::CullModeFlagBits::eBack,
			.frontFace = vk::FrontFace::eCounterClockwise,
			.depthBiasEnable = vk::False
		};
		rasterizer.lineWidth = 1.0f;
		vk::PipelineMultisampleStateCreateInfo multisampling{
			.rasterizationSamples = vk::SampleCountFlagBits::e1,
			.sampleShadingEnable = vk::False
		};
		vk::PipelineDepthStencilStateCreateInfo depthStencil{
			.depthTestEnable = vk::True,
			.depthWriteEnable = vk::True,
			.depthCompareOp = vk::CompareOp::eLess,
			.depthBoundsTestEnable = vk::False,
			.stencilTestEnable = vk::False
		};
		vk::PipelineColorBlendAttachmentState colorBlendAttachment;
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = vk::False;

		vk::PipelineColorBlendStateCreateInfo colorBlending{
			.logicOpEnable = vk::False,
			.logicOp = vk::LogicOp::eCopy,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment
		};

		std::vector dynamicStates = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
		vk::PipelineDynamicStateCreateInfo dynamicState{ .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data() };

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{  .setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0 };

		pipelineLayout = vk::raii::PipelineLayout( vulkanDevice->getDevice(), pipelineLayoutInfo );

		vk::Format depthFormat = findDepthFormat();

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		  {.stageCount = 2,
			.pStages = shaderStages,
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = nullptr },
		  {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format, .depthAttachmentFormat = depthFormat }
		};

		graphicsPipeline = vk::raii::Pipeline(vulkanDevice->getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool() {
		vk::CommandPoolCreateInfo poolInfo{  .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
											 .queueFamilyIndex = vulkanDevice->getGraphicsQueueFamily() };
		commandPool = vk::raii::CommandPool(vulkanDevice->getDevice(), poolInfo);
	}

	void createDepthResources() {
		vk::Format depthFormat = findDepthFormat();

		createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
		depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
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

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});
		createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void* data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

		transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	}

	void createTextureImageView() {
		textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
	}

	void createTextureSampler() {
		vk::PhysicalDeviceProperties properties = vulkanDevice->getPhysicalDevice().getProperties();
		vk::SamplerCreateInfo samplerInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
			.compareEnable = vk::False,
			.compareOp = vk::CompareOp::eAlways };
		textureSampler = vk::raii::Sampler(vulkanDevice->getDevice(), samplerInfo);
	}

	vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
		vk::ImageViewCreateInfo viewInfo{
			.image = image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = { aspectFlags, 0, 1, 0, 1 }
		};
		return vk::raii::ImageView(vulkanDevice->getDevice(), viewInfo);
	}

	void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory) {
		vk::ImageCreateInfo imageInfo{ .imageType = vk::ImageType::e2D, .format = format,
									  .extent = {width, height, 1}, .mipLevels = 1, .arrayLayers = 1,
									  .samples = vk::SampleCountFlagBits::e1, .tiling = tiling,
									  .usage = usage, .sharingMode = vk::SharingMode::eExclusive };

		image = vk::raii::Image( vulkanDevice->getDevice(), imageInfo );

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
										 .memoryTypeIndex = vulkanDevice->findMemoryType(memRequirements.memoryTypeBits, properties) };
		imageMemory = vk::raii::DeviceMemory( vulkanDevice->getDevice(), allocInfo );
		image.bindMemory(imageMemory, 0);
	}

	void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
		auto commandBuffer = beginSingleTimeCommands();

		vk::ImageMemoryBarrier barrier{ .oldLayout = oldLayout, .newLayout = newLayout,
									   .image = image,
									   .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } };

		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		} else {
			throw std::invalid_argument("unsupported layout transition!");
		}
		commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
		endSingleTimeCommands(*commandBuffer);
	}

	void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = beginSingleTimeCommands();
		vk::BufferImageCopy region{ .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0,
								   .imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
								   .imageOffset = {0, 0, 0}, .imageExtent = {width, height, 1} };
		commandBuffer->copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
		endSingleTimeCommands(*commandBuffer);
	}

	void loadModel() {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = {1.0f, 1.0f, 1.0f};

				if (!uniqueVertices.contains(vertex)) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
			}
		}
	}

	void createVertexBuffer() {
		vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, vertices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	}

	void createIndexBuffer() {
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});
		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

		void* data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);
   }

   void createUniformBuffers() {
		uniformBuffers.clear();
		uniformBuffersMemory.clear();
		uniformBuffersMapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			vk::raii::Buffer buffer({});
			vk::raii::DeviceMemory bufferMem({});
			createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(bufferMem));
			uniformBuffersMapped.emplace_back( uniformBuffersMemory[i].mapMemory(0, bufferSize));
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
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo{
			.descriptorPool = descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data()
		};

		descriptorSets.clear();
		descriptorSets = vulkanDevice->getDevice().allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject)
			};
			vk::DescriptorImageInfo imageInfo{
				.sampler = textureSampler,
				.imageView = textureImageView,
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

	void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) {
		vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };
		buffer = vk::raii::Buffer(vulkanDevice->getDevice(), bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{ .allocationSize =memRequirements.size, .memoryTypeIndex = vulkanDevice->findMemoryType(memRequirements.memoryTypeBits, properties) };
		bufferMemory = vk::raii::DeviceMemory(vulkanDevice->getDevice(), allocInfo);
		buffer.bindMemory(bufferMemory, 0);
	}

	std::unique_ptr<vk::raii::CommandBuffer> beginSingleTimeCommands() {
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
		std::unique_ptr<vk::raii::CommandBuffer> commandBuffer = std::make_unique<vk::raii::CommandBuffer>(std::move(vk::raii::CommandBuffers(vulkanDevice->getDevice(), allocInfo).front()));

		vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		commandBuffer->begin(beginInfo);

		return commandBuffer;
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer) {
		commandBuffer.end();

		vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandBuffer };
		vulkanDevice->getGraphicsQueue().submit(submitInfo, nullptr);
		vulkanDevice->getGraphicsQueue().waitIdle();
	}

	void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 };
		vk::raii::CommandBuffer commandCopyBuffer = std::move(vulkanDevice->getDevice().allocateCommandBuffers(allocInfo).front());
		commandCopyBuffer.begin(vk::CommandBufferBeginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
		commandCopyBuffer.end();
		vulkanDevice->getGraphicsQueue().submit(vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer }, nullptr);
		vulkanDevice->getGraphicsQueue().waitIdle();
	}

	void createCommandBuffers() {
		vk::CommandBufferAllocateInfo allocInfo{ .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary,
												 .commandBufferCount = MAX_FRAMES_IN_FLIGHT };
		commandBuffers = vk::raii::CommandBuffers( vulkanDevice->getDevice(), allocInfo );
	}

	void recordCommandBuffer(uint32_t imageIndex) {
		commandBuffers[currentFrame].begin({});
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
			.image = depthImage,
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
		commandBuffers[currentFrame].pipelineBarrier2(depthDependencyInfo);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo = {
			.imageView = swapChainImageViews[imageIndex],
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor
		};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = depthImageView,
			.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth
		};

		vk::RenderingInfo renderingInfo = {
			.renderArea = { .offset = { 0, 0 }, .extent = swapChainExtent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};
		commandBuffers[currentFrame].beginRendering(renderingInfo);
		commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
		commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffers[currentFrame].bindIndexBuffer( *indexBuffer, 0, vk::IndexType::eUint32 );
		commandBuffers[currentFrame].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[currentFrame], nullptr);
		commandBuffers[currentFrame].drawIndexed(indices.size(), 1, 0, 0, 0);
		commandBuffers[currentFrame].endRendering();
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
		commandBuffers[currentFrame].end();
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
			.image = swapChainImages[imageIndex],
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
		commandBuffers[currentFrame].pipelineBarrier2(dependency_info);
	}

	void createSyncObjects() {
		presentCompleteSemaphore.clear();
		renderFinishedSemaphore.clear();
		inFlightFences.clear();

		for (size_t i = 0; i < swapChainImages.size(); i++) {
			presentCompleteSemaphore.emplace_back(vulkanDevice->getDevice(), vk::SemaphoreCreateInfo());
			 renderFinishedSemaphore.emplace_back(vulkanDevice->getDevice(), vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			inFlightFences.emplace_back(vulkanDevice->getDevice(), vk::FenceCreateInfo { .flags = vk::FenceCreateFlagBits::eSignaled });
		}
	}

	void updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void drawFrame() {
		while ( vk::Result::eTimeout == vulkanDevice->getDevice().waitForFences( *inFlightFences[currentFrame], vk::True, UINT64_MAX ) )
			;
		auto [result, imageIndex] = swapChain.acquireNextImage( UINT64_MAX, *presentCompleteSemaphore[semaphoreIndex], nullptr );

		if (result == vk::Result::eErrorOutOfDateKHR) {
			recreateSwapChain();
			return;
		}
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		updateUniformBuffer(currentFrame);

		vulkanDevice->getDevice().resetFences(  *inFlightFences[currentFrame] );
		commandBuffers[currentFrame].reset();
		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask( vk::PipelineStageFlagBits::eColorAttachmentOutput );
		const vk::SubmitInfo submitInfo{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*presentCompleteSemaphore[semaphoreIndex],
							.pWaitDstStageMask = &waitDestinationStageMask, .commandBufferCount = 1, .pCommandBuffers = &*commandBuffers[currentFrame],
							.signalSemaphoreCount = 1, .pSignalSemaphores = &*renderFinishedSemaphore[imageIndex] };
		vulkanDevice->getGraphicsQueue().submit(submitInfo, *inFlightFences[currentFrame]);

		const vk::PresentInfoKHR presentInfoKHR{ .waitSemaphoreCount = 1, .pWaitSemaphores = &*renderFinishedSemaphore[imageIndex],
												.swapchainCount = 1, .pSwapchains = &*swapChain, .pImageIndices = &imageIndex };
		result = vulkanDevice->getGraphicsQueue().presentKHR( presentInfoKHR );
		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		} else if (result != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present swap chain image!");
		}
		semaphoreIndex = (semaphoreIndex + 1) % presentCompleteSemaphore.size();
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
		vk::ShaderModuleCreateInfo createInfo{ .codeSize = code.size() * sizeof(char), .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
		vk::raii::ShaderModule shaderModule{ vulkanDevice->getDevice(), createInfo };

		return shaderModule;
	}

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const & surfaceCapabilities) {
		auto minImageCount = std::max( 3u, surfaceCapabilities.minImageCount );
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const & availableFormats) {
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
			availableFormats,
			[]( const auto & format ) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; } );
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode){ return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
			[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; } ) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
	}

	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
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