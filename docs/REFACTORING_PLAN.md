# Vulkan FDF - Refactoring Plan

## 목표
튜토리얼 방식의 단일 파일 구조(`main.cpp`)에서 벗어나 **엔진 프로그래밍 포트폴리오** 수준의 객체 지향적 아키텍처로 전환합니다. RAII 패턴을 활용하여 Vulkan 리소스 관리를 안전하고 명확하게 분리합니다.

---

## 현재 문제점

### 1. 책임 분리 부재
- 1150+ 라인의 단일 클래스 (`HelloTriangleApplication`)
- 모든 Vulkan 리소스와 로직이 한 곳에 집중
- 유지보수 및 확장이 어려움

### 2. 리소스 관리의 불명확함
- `vk::raii::*` 타입이 산재되어 있으나 개념적 그룹화 없음
- 스왑체인 재생성 시 수동으로 여러 리소스 정리 필요
- 버퍼 생성 로직이 중복 (vertex, index, uniform, staging)

### 3. 확장성 제약
- 새로운 모델 추가가 어려움 (현재는 단일 OBJ 로딩)
- 파이프라인 설정 변경 (와이어프레임, 다른 셰이더 등)이 복잡함
- FDF 파일 포맷으로 전환 시 대규모 수정 필요

---

## 리팩토링 아키텍처

### 계층 구조
```
┌─────────────────────────────┐
│       Application           │  ← Window, Main Loop
│    (또는 Engine)             │
└─────────────┬───────────────┘
              │ owns
┌─────────────▼───────────────┐
│         Renderer            │  ← Drawing, Command Recording
│                             │
├─────────────┬───────────────┤
│             │ owns          │
│  ┌──────────▼──────────┐    │
│  │  VulkanDevice       │    │  ← Device, Queue, Instance
│  │  (Core Context)     │    │
│  └─────────────────────┘    │
│             │               │
│  ┌──────────▼──────────┐    │
│  │  VulkanSwapchain    │    │  ← Swapchain, ImageViews, Framebuffers
│  └─────────────────────┘    │
│             │               │
│  ┌──────────▼──────────┐    │
│  │  VulkanPipeline     │    │  ← Graphics Pipeline, Layout
│  └─────────────────────┘    │
│                             │
│  ┌──────────────────────┐   │
│  │  CommandManager      │   │  ← Command Pool, Command Buffers
│  └──────────────────────┘   │
│                             │
│  ┌──────────────────────┐   │
│  │  SyncManager         │   │  ← Semaphores, Fences
│  └──────────────────────┘   │
└─────────────────────────────┘
              │ uses
┌─────────────▼───────────────┐
│      Resource Wrappers      │
│                             │
│  ┌──────────────────────┐   │
│  │  VulkanBuffer        │   │  ← Vertex, Index, Uniform, Staging
│  └──────────────────────┘   │
│                             │
│  ┌──────────────────────┐   │
│  │  VulkanImage         │   │  ← Textures, Depth Buffer
│  └──────────────────────┘   │
│                             │
│  ┌──────────────────────┐   │
│  │  Mesh                │   │  ← Vertex/Index Data + Buffers
│  └──────────────────────┘   │
│                             │
│  ┌──────────────────────┐   │
│  │  Material            │   │  ← Descriptor Sets, Textures
│  └──────────────────────┘   │
└─────────────────────────────┘
```

---

## 클래스 설계

### 1. Core Layer: 디바이스 컨텍스트

#### **VulkanDevice**
**역할**: Vulkan 인스턴스, 물리/논리 디바이스, 큐 관리
**파일**: `src/core/VulkanDevice.hpp`, `src/core/VulkanDevice.cpp`

```cpp
class VulkanDevice {
public:
    VulkanDevice(const std::vector<const char*>& validationLayers);
    ~VulkanDevice() = default;

    // Accessors
    vk::raii::Device& getDevice() { return device; }
    vk::raii::PhysicalDevice& getPhysicalDevice() { return physicalDevice; }
    vk::raii::Queue& getGraphicsQueue() { return graphicsQueue; }
    uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    vk::raii::Instance& getInstance() { return instance; }

    // Utility
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    vk::Format findSupportedFormat(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features);

private:
    vk::raii::Context context;
    vk::raii::Instance instance;
    vk::raii::DebugUtilsMessengerEXT debugMessenger;
    vk::raii::PhysicalDevice physicalDevice;
    vk::raii::Device device;
    vk::raii::Queue graphicsQueue;
    uint32_t graphicsQueueFamily;

    void createInstance(const std::vector<const char*>& validationLayers);
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();
};
```

**개선점**:
- 디바이스 선택 로직을 설정 가능하게 (현재는 하드코딩)
- 큐 패밀리 인덱스를 명확히 캡슐화
- 컴퓨트 큐 등 확장 가능한 구조

---

### 2. Resource Layer: RAII 리소스 래퍼

#### **VulkanBuffer** (가장 중요!)
**역할**: 모든 종류의 버퍼를 일반화 (Vertex, Index, Uniform, Staging)
**파일**: `src/resources/VulkanBuffer.hpp`, `src/resources/VulkanBuffer.cpp`

```cpp
class VulkanBuffer {
public:
    VulkanBuffer(
        VulkanDevice& device,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties);

    ~VulkanBuffer(); // RAII: 자동 vkDestroyBuffer, vkFreeMemory

    // Data operations
    void map();
    void unmap();
    void copyData(const void* data, vk::DeviceSize size);
    void copyFrom(VulkanBuffer& srcBuffer, vk::CommandBuffer cmdBuffer);

    // Accessors
    vk::Buffer getHandle() const { return *buffer; }
    vk::DeviceSize getSize() const { return size; }
    void* getMappedData() { return mappedData; }

private:
    VulkanDevice& device;
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
    vk::DeviceSize size;
    void* mappedData = nullptr;
};
```

**사용 예시**:
```cpp
// Vertex Buffer
VulkanBuffer vertexBuffer(device, vertexDataSize,
    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
    vk::MemoryPropertyFlagBits::eDeviceLocal);

// Staging Buffer
VulkanBuffer stagingBuffer(device, vertexDataSize,
    vk::BufferUsageFlagBits::eTransferSrc,
    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

stagingBuffer.map();
stagingBuffer.copyData(vertices.data(), vertexDataSize);
stagingBuffer.unmap();
vertexBuffer.copyFrom(stagingBuffer, commandBuffer);
```

#### **VulkanImage**
**역할**: 이미지, 이미지 뷰, 샘플러를 하나의 단위로 관리
**파일**: `src/resources/VulkanImage.hpp`, `src/resources/VulkanImage.cpp`

```cpp
class VulkanImage {
public:
    VulkanImage(
        VulkanDevice& device,
        uint32_t width, uint32_t height,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::ImageAspectFlags aspectFlags);

    ~VulkanImage();

    // Layout transitions
    void transitionLayout(
        vk::CommandBuffer cmdBuffer,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout);

    void copyFromBuffer(vk::CommandBuffer cmdBuffer, VulkanBuffer& buffer);

    // Accessors
    vk::Image getImage() const { return *image; }
    vk::ImageView getImageView() const { return *imageView; }
    vk::Sampler getSampler() const { return sampler ? *sampler : nullptr; }

    void createSampler(); // Optional: for textures

private:
    VulkanDevice& device;
    vk::raii::Image image;
    vk::raii::DeviceMemory memory;
    vk::raii::ImageView imageView;
    std::optional<vk::raii::Sampler> sampler;
};
```

---

### 3. Functional Layer: 기능 단위 캡슐화

#### **VulkanSwapchain**
**역할**: 스왑체인, 이미지뷰, 재생성 로직 통합
**파일**: `src/rendering/VulkanSwapchain.hpp`, `src/rendering/VulkanSwapchain.cpp`

```cpp
class VulkanSwapchain {
public:
    VulkanSwapchain(
        VulkanDevice& device,
        vk::raii::SurfaceKHR& surface,
        GLFWwindow* window);

    ~VulkanSwapchain();

    // Swapchain operations
    void recreate();
    uint32_t acquireNextImage(vk::Semaphore semaphore, vk::Fence fence);
    void present(vk::Queue queue, uint32_t imageIndex, vk::Semaphore waitSemaphore);

    // Accessors
    vk::Extent2D getExtent() const { return extent; }
    vk::Format getImageFormat() const { return imageFormat; }
    const std::vector<vk::ImageView>& getImageViews() const { return imageViews; }
    uint32_t getImageCount() const { return images.size(); }

private:
    VulkanDevice& device;
    vk::raii::SurfaceKHR& surface;
    GLFWwindow* window;

    vk::raii::SwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::raii::ImageView> imageViews;
    vk::Format imageFormat;
    vk::Extent2D extent;

    void createSwapchain();
    void createImageViews();
    void cleanup();
};
```

**핵심 개선**:
- 윈도우 리사이즈 시 `recreate()` 한 번만 호출하면 됨
- 스왑체인 관련 모든 리소스가 한 곳에서 관리됨

#### **VulkanPipeline**
**역할**: 그래픽 파이프라인과 레이아웃 관리
**파일**: `src/rendering/VulkanPipeline.hpp`, `src/rendering/VulkanPipeline.cpp`

```cpp
struct PipelineConfig {
    std::string vertexShaderPath;
    std::string fragmentShaderPath;
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
    vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
    bool depthTestEnable = true;
    // ... other states
};

class VulkanPipeline {
public:
    VulkanPipeline(
        VulkanDevice& device,
        const PipelineConfig& config,
        vk::DescriptorSetLayout descriptorSetLayout,
        vk::Format colorFormat,
        vk::Format depthFormat);

    ~VulkanPipeline();

    vk::Pipeline getPipeline() const { return *pipeline; }
    vk::PipelineLayout getLayout() const { return *layout; }

private:
    VulkanDevice& device;
    vk::raii::ShaderModule vertexShader;
    vk::raii::ShaderModule fragmentShader;
    vk::raii::PipelineLayout layout;
    vk::raii::Pipeline pipeline;

    vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);
};
```

**장점**:
- 와이어프레임 파이프라인, 솔리드 파이프라인을 각각 `PipelineConfig`로 쉽게 생성
- 셰이더 교체가 간편함

#### **CommandManager**
**역할**: 커맨드 풀 및 커맨드 버퍼 관리
**파일**: `src/rendering/CommandManager.hpp`, `src/rendering/CommandManager.cpp`

```cpp
class CommandManager {
public:
    CommandManager(VulkanDevice& device, uint32_t queueFamilyIndex);
    ~CommandManager();

    // Command buffer allocation
    vk::raii::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::raii::CommandBuffer& cmdBuffer);

    const std::vector<vk::raii::CommandBuffer>& getCommandBuffers() const { return commandBuffers; }
    void resetCommandBuffer(uint32_t index);

private:
    VulkanDevice& device;
    vk::raii::CommandPool commandPool;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    void createCommandPool(uint32_t queueFamilyIndex);
    void allocateCommandBuffers(uint32_t count);
};
```

#### **SyncManager**
**역할**: 세마포어와 펜스 관리
**파일**: `src/rendering/SyncManager.hpp`, `src/rendering/SyncManager.cpp`

```cpp
class SyncManager {
public:
    SyncManager(VulkanDevice& device, uint32_t maxFramesInFlight, uint32_t swapchainImageCount);
    ~SyncManager() = default;

    vk::Semaphore getPresentCompleteSemaphore(uint32_t index) const;
    vk::Semaphore getRenderFinishedSemaphore(uint32_t index) const;
    vk::Fence getInFlightFence(uint32_t index) const;

    void waitForFence(uint32_t index);
    void resetFence(uint32_t index);

private:
    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;
};
```

---

### 4. High-Level Layer: 조립 및 렌더링

#### **Mesh**
**역할**: 모델 데이터(정점, 인덱스) + GPU 버퍼 소유
**파일**: `src/scene/Mesh.hpp`, `src/scene/Mesh.cpp`

```cpp
class Mesh {
public:
    Mesh(VulkanDevice& device, CommandManager& commandManager);
    ~Mesh() = default;

    // Load methods
    void loadFromOBJ(const std::string& filepath);
    void loadFromFDF(const std::string& filepath); // 향후 FDF 지원

    // Rendering
    void bind(vk::CommandBuffer cmdBuffer);
    void draw(vk::CommandBuffer cmdBuffer);

    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }

private:
    VulkanDevice& device;
    CommandManager& commandManager;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::unique_ptr<VulkanBuffer> vertexBuffer;
    std::unique_ptr<VulkanBuffer> indexBuffer;

    void createBuffers();
};
```

**사용 예시**:
```cpp
Mesh mesh(device, commandManager);
mesh.loadFromOBJ("models/viking_room.obj");

// In command buffer recording:
mesh.bind(commandBuffer);
mesh.draw(commandBuffer);
```

#### **Material** (선택적, 향후 확장)
**역할**: Descriptor Sets, Uniform Buffers, Textures 관리
**파일**: `src/scene/Material.hpp`, `src/scene/Material.cpp`

```cpp
class Material {
public:
    Material(VulkanDevice& device, vk::DescriptorSetLayout layout);

    void setTexture(VulkanImage* texture);
    void setUniformBuffer(VulkanBuffer* uniformBuffer);
    void updateDescriptorSet();

    void bind(vk::CommandBuffer cmdBuffer, vk::PipelineLayout pipelineLayout);

private:
    VulkanDevice& device;
    vk::raii::DescriptorPool descriptorPool;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    VulkanImage* texture = nullptr;
    VulkanBuffer* uniformBuffer = nullptr;
};
```

#### **Renderer**
**역할**: 최상위 렌더링 오케스트레이션
**파일**: `src/rendering/Renderer.hpp`, `src/rendering/Renderer.cpp`

```cpp
class Renderer {
public:
    Renderer(GLFWwindow* window, const std::vector<const char*>& validationLayers);
    ~Renderer();

    void addMesh(std::shared_ptr<Mesh> mesh);
    void drawFrame();

private:
    std::unique_ptr<VulkanDevice> device;
    vk::raii::SurfaceKHR surface;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanPipeline> pipeline;
    std::unique_ptr<CommandManager> commandManager;
    std::unique_ptr<SyncManager> syncManager;

    std::unique_ptr<VulkanImage> depthImage;
    std::unique_ptr<VulkanImage> textureImage;

    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<VulkanBuffer>> uniformBuffers;

    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    void recordCommandBuffer(vk::CommandBuffer cmdBuffer, uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentFrame);
    void recreateSwapchain();
};
```

**핵심 로직**:
```cpp
void Renderer::drawFrame() {
    syncManager->waitForFence(currentFrame);

    auto [result, imageIndex] = swapchain->acquireNextImage(
        syncManager->getPresentCompleteSemaphore(currentFrame),
        nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    }

    updateUniformBuffer(currentFrame);
    syncManager->resetFence(currentFrame);

    commandManager->resetCommandBuffer(currentFrame);
    recordCommandBuffer(commandManager->getCommandBuffers()[currentFrame], imageIndex);

    // Submit and present...
}
```

#### **Application**
**역할**: 최상위 메인 루프, 윈도우 관리
**파일**: `src/Application.hpp`, `src/Application.cpp`

```cpp
class Application {
public:
    Application();
    ~Application();

    void run();

private:
    GLFWwindow* window;
    std::unique_ptr<Renderer> renderer;

    void initWindow();
    void mainLoop();
    void cleanup();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};
```

---

## 디렉토리 구조

```
vulkan-fdf/
├── src/
│   ├── core/                    # 핵심 Vulkan 컨텍스트
│   │   ├── VulkanDevice.hpp
│   │   └── VulkanDevice.cpp
│   │
│   ├── resources/               # RAII 리소스 래퍼
│   │   ├── VulkanBuffer.hpp
│   │   ├── VulkanBuffer.cpp
│   │   ├── VulkanImage.hpp
│   │   └── VulkanImage.cpp
│   │
│   ├── rendering/               # 렌더링 서브시스템
│   │   ├── VulkanSwapchain.hpp
│   │   ├── VulkanSwapchain.cpp
│   │   ├── VulkanPipeline.hpp
│   │   ├── VulkanPipeline.cpp
│   │   ├── CommandManager.hpp
│   │   ├── CommandManager.cpp
│   │   ├── SyncManager.hpp
│   │   ├── SyncManager.cpp
│   │   ├── Renderer.hpp
│   │   └── Renderer.cpp
│   │
│   ├── scene/                   # 씬 객체
│   │   ├── Mesh.hpp
│   │   ├── Mesh.cpp
│   │   ├── Material.hpp         (향후)
│   │   └── Material.cpp         (향후)
│   │
│   ├── loaders/                 # 모델/텍스처 로더
│   │   ├── OBJLoader.hpp
│   │   ├── OBJLoader.cpp
│   │   ├── FDFLoader.hpp        (향후)
│   │   ├── FDFLoader.cpp        (향후)
│   │   └── TextureLoader.hpp
│   │   └── TextureLoader.cpp
│   │
│   ├── utils/                   # 유틸리티
│   │   ├── FileUtils.hpp        (readFile 등)
│   │   └── Vertex.hpp           (Vertex 구조체)
│   │
│   ├── Application.hpp
│   ├── Application.cpp
│   └── main.cpp                 (단순화: Application 생성 및 run)
│
├── shaders/
├── models/
├── textures/
├── docs/
│   └── REFACTORING_PLAN.md     (이 문서)
└── CMakeLists.txt
```

---

## 리팩토링 실행 계획

### Phase 1: 유틸리티 및 구조체 분리 (난이도: 하)
**목표**: 재사용 가능한 공통 코드 추출

- [ ] `src/utils/Vertex.hpp` 생성 (Vertex 구조체, UniformBufferObject)
- [ ] `src/utils/FileUtils.hpp` 생성 (`readFile()` 함수)
- [ ] `main.cpp`에서 이들을 사용하도록 수정

**예상 시간**: 1시간

---

### Phase 2: Core Layer 구현 (난이도: 중)
**목표**: VulkanDevice 분리

- [ ] `VulkanDevice` 클래스 작성
  - `createInstance()`, `pickPhysicalDevice()`, `createLogicalDevice()`
  - 디버그 메신저 포함
- [ ] `main.cpp`의 해당 로직을 `VulkanDevice`로 마이그레이션
- [ ] 테스트: 프로그램이 여전히 실행되는지 확인

**예상 시간**: 2-3시간

---

### Phase 3: Resource Layer 구현 (난이도: 중상)
**목표**: VulkanBuffer, VulkanImage 분리

- [ ] `VulkanBuffer` 클래스 작성
  - `map()`, `unmap()`, `copyData()`, `copyFrom()` 구현
- [ ] `VulkanImage` 클래스 작성
  - `transitionLayout()`, `copyFromBuffer()`, `createSampler()` 구현
- [ ] `main.cpp`의 버퍼/이미지 생성 로직을 새 클래스로 대체
  - `createVertexBuffer()` → `VulkanBuffer vertexBuffer(...)`
  - `createTextureImage()` → `VulkanImage textureImage(...)`

**예상 시간**: 4-5시간

---

### Phase 4: Functional Layer 구현 (난이도: 중상)
**목표**: Swapchain, Pipeline, Command/Sync 관리 분리

- [ ] `VulkanSwapchain` 클래스 작성
  - `recreate()` 로직 완전 캡슐화
- [ ] `VulkanPipeline` 클래스 작성
  - `PipelineConfig` 구조체로 설정 주입
- [ ] `CommandManager` 클래스 작성
- [ ] `SyncManager` 클래스 작성
- [ ] `main.cpp`의 해당 로직들을 새 클래스로 대체

**예상 시간**: 5-6시간

---

### Phase 5: Scene Layer 구현 (난이도: 중)
**목표**: Mesh 클래스 분리

- [ ] `Mesh` 클래스 작성
  - `loadFromOBJ()` 구현 (기존 `loadModel()` 로직)
  - `bind()`, `draw()` 메서드
- [ ] `OBJLoader` 유틸리티 작성 (선택적)
- [ ] `main.cpp`에서 Mesh 사용

**예상 시간**: 2-3시간

---

### Phase 6: Renderer 통합 (난이도: 상)
**목표**: 최상위 Renderer 클래스 구현

- [ ] `Renderer` 클래스 작성
  - 모든 서브시스템 소유 및 조립
  - `drawFrame()` 로직 마이그레이션
- [ ] `recordCommandBuffer()` 구조화
- [ ] Descriptor Set 관리 로직 정리

**예상 시간**: 4-5시간

---

### Phase 7: Application Layer 정리 (난이도: 하)
**목표**: main.cpp 단순화

- [ ] `Application` 클래스 작성
- [ ] `main.cpp`를 다음과 같이 단순화:
  ```cpp
  int main() {
      try {
          Application app;
          app.run();
      } catch (const std::exception& e) {
          std::cerr << e.what() << std::endl;
          return EXIT_FAILURE;
      }
      return EXIT_SUCCESS;
  }
  ```

**예상 시간**: 1시간

---

### Phase 8: FDF 파일 지원 (난이도: 중, 선택적)
**목표**: FDF 파일 포맷 로더 구현

- [ ] FDF 파일 포맷 명세 정의
- [ ] `FDFLoader` 클래스 작성
- [ ] `Mesh::loadFromFDF()` 구현
- [ ] FDF → 정점/인덱스 버퍼 변환 로직

**예상 시간**: 3-4시간

---

## 핵심 설계 원칙

### 1. RAII (Resource Acquisition Is Initialization)
- 모든 Vulkan 리소스는 C++ 클래스 생성자에서 생성
- 소멸자에서 자동으로 파괴 (`vk::raii::*` 활용)
- 예외 안전성 보장

### 2. 단일 책임 원칙 (Single Responsibility Principle)
- 각 클래스는 하나의 명확한 책임만 가짐
- `VulkanBuffer`는 버퍼만, `VulkanPipeline`은 파이프라인만

### 3. 의존성 주입 (Dependency Injection)
- 클래스는 필요한 의존성을 생성자에서 받음
- 예: `VulkanBuffer(VulkanDevice& device, ...)`
- 테스트 가능성 향상

### 4. 소유권 명확화
- `std::unique_ptr`: 독점 소유 (Renderer → VulkanDevice)
- `std::shared_ptr`: 공유 소유 (여러 Mesh가 같은 Texture 참조)
- 참조(`&`): 소유하지 않음 (VulkanBuffer → VulkanDevice&)

### 5. 재사용성
- `VulkanBuffer`는 vertex, index, uniform, staging 모두 처리
- `PipelineConfig`로 다양한 파이프라인 생성
- `Mesh`는 OBJ, FDF 등 다양한 포맷 지원

---

## 리팩토링 시 주의사항

### 1. 점진적 진행
- 한 번에 모든 것을 바꾸지 말 것
- Phase별로 진행하며 매 단계마다 **빌드 및 실행 테스트**

### 2. Git 커밋 전략
- Phase별로 커밋 (`feat: Add VulkanDevice class`)
- 동작하는 상태에서만 커밋
- 브랜치 사용 권장 (`refactor/phase-1`, `refactor/phase-2`)

### 3. 기존 코드 보존
- 리팩토링 중에도 프로그램은 계속 실행 가능해야 함
- 필요시 임시로 old/new 코드 공존

### 4. 성능 고려
- RAII 패턴은 성능 오버헤드가 거의 없음 (C++ 컴파일러 최적화)
- 복사 금지: `= delete` 또는 move semantics 사용
- 불필요한 std::shared_ptr 남용 금지

### 5. 에러 처리
- Vulkan 함수 호출 결과를 항상 확인
- 리소스 생성 실패 시 명확한 예외 메시지
- `vk::raii::*`는 자동으로 예외 처리

---

## 예상 효과

### 포트폴리오 측면
✅ "튜토리얼 따라한 수준"에서 "아키텍처를 설계할 수 있는 엔지니어"로 평가 상승
✅ 코드 리뷰 시 객체 지향 설계 역량 증명
✅ 확장 가능한 구조로 추가 기능 구현이 용이함을 보여줌

### 개발 효율성
✅ 버그 발생 시 문제 위치 파악 용이 (책임이 분리됨)
✅ 새 기능 추가 시 기존 코드 수정 최소화
✅ 유닛 테스트 작성 가능 (각 클래스 독립적 테스트)

### 유지보수성
✅ 코드 가독성 대폭 향상 (main.cpp가 1150줄 → 50줄)
✅ 리소스 관리 실수 감소 (RAII 자동 정리)
✅ 멀티플 뷰포트, 포스트 프로세싱 등 추가 시 구조적 확장 가능

---

## 참고 자료

### Vulkan 오픈소스 프로젝트
- **Sascha Willems Vulkan Examples**: https://github.com/SaschaWillems/Vulkan
  - 산업 표준 수준의 Vulkan 코드 예제
  - 클래스 분리 구조 참고

- **Hazel Engine**: https://github.com/TheCherno/Hazel
  - 게임 엔진 아키텍처 참고
  - Renderer 추상화 레이어 설계

### Vulkan 공식 문서
- **Vulkan Guide**: https://vkguide.dev/
  - 모던 Vulkan 아키텍처 패턴
  - Dynamic Rendering 사용법

### C++ 설계 패턴
- **RAII 패턴**: https://en.cppreference.com/w/cpp/language/raii
- **Dependency Injection in C++**: https://www.modernescpp.com/

---

## 마무리

이 리팩토링 계획은 **점진적이고 안전한 구조 개선**을 목표로 합니다. 모든 Phase가 완료되면:

1. **main.cpp**: 50줄 이하로 단순화
2. **VulkanDevice**: Vulkan 컨텍스트 완전 캡슐화
3. **VulkanBuffer/Image**: 재사용 가능한 리소스 래퍼
4. **Renderer**: 확장 가능한 렌더링 엔진 구조
5. **Mesh**: 다양한 포맷 지원 가능한 모델 시스템

이 구조는 **실무 Vulkan 프로젝트**에서 사용하는 패턴과 거의 동일하며, 포트폴리오에서 매우 긍정적으로 평가될 것입니다.

---

**작성일**: 2025-11-04
**대상 프로젝트**: vulkan-fdf
**현재 코드 베이스**: main.cpp (1150 lines)
**목표**: 엔진 프로그래밍 포트폴리오 수준의 아키텍처
