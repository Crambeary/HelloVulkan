#define GLFW_INCLUDE_VULKAN
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <GLFW/glfw3.h>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include <vulkan/vk_platform.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <sys/types.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector validationLayers = {"VK_LAYER_KHRONOS_validation"};

vk::raii::Queue presentQueue = nullptr;

const std::vector<const char *> deviceExtensions = {
    vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
    vk::KHRSynchronization2ExtensionName,
    vk::KHRCreateRenderpass2ExtensionName};

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
  GLFWwindow *window = nullptr;

  vk::raii::Context context;
  vk::raii::Instance instance = nullptr;
  vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;

  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  uint32_t graphicsIndex = ~0;
  vk::raii::Queue graphicsQueue = nullptr;

  vk::raii::SwapchainKHR swapChain = nullptr;
  std::vector<vk::Image> swapChainImages;
  vk::Format swapChainImageFormat = vk::Format::eUndefined;
  vk::Extent2D swapChainExtent;
  std::vector<vk::raii::ImageView> swapChainImageViews;

  vk::raii::Pipeline graphicsPipeline = nullptr;

  vk::raii::CommandPool commandPool = nullptr;
  vk::raii::CommandBuffer commandBuffer = nullptr;

  vk::raii::Semaphore presentCompleteSemaphore = nullptr;
  vk::raii::Semaphore renderFinishedSemaphore = nullptr;
  vk::raii::Fence drawFence = nullptr;

  void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
  }

  void createSwapChain() {
    auto surfaceCapabilities =
        physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto swapChainSurfaceFormat =
        chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    minImageCount = (surfaceCapabilities.maxImageCount > 0 &&
                     minImageCount > surfaceCapabilities.maxImageCount)
                        ? surfaceCapabilities.maxImageCount
                        : minImageCount;

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 &&
        imageCount > surfaceCapabilities.maxImageCount) {
      imageCount = surfaceCapabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .flags = vk::SwapchainCreateFlagsKHR(),
        .surface = surface,
        .minImageCount = minImageCount,
        .imageFormat = swapChainSurfaceFormat.format,
        .imageColorSpace = swapChainSurfaceFormat.colorSpace,
        .imageExtent = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = chooseSwapPresentMode(
            physicalDevice.getSurfacePresentModesKHR(surface)),
        .clipped = vk::True,
        .oldSwapchain = nullptr};

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();

    swapChainImageFormat = swapChainSurfaceFormat.format;
  }

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
      if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
          availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return availableFormat;
      }
    }
    return availableFormats[0];
  }

  vk::PresentModeKHR chooseSwapPresentMode(
      const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
        return availablePresentMode;
      }
    }
    return vk::PresentModeKHR::eFifo;
  }

  vk::Extent2D
  chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {std::clamp<uint32_t>(width, capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
  }

  void createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();

    // auto surfaceCapabilities =
    //     physicalDevice.getSurfaceCapabilitiesKHR(surface);
    std::vector<vk::SurfaceFormatKHR> availableFormats =
        physicalDevice.getSurfaceFormatsKHR(surface);
    std::vector<vk::PresentModeKHR> availablePresentModes =
        physicalDevice.getSurfacePresentModesKHR(surface);

    auto graphicsQueueFamilyProperty =
        std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
          return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                 static_cast<vk::QueueFlags>(0);
        });

    graphicsIndex = static_cast<uint32_t>(std::distance(
        queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

    auto presentIndex =
        physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface)
            ? graphicsIndex
            : static_cast<uint32_t>(queueFamilyProperties.size());

    if (presentIndex == queueFamilyProperties.size()) {
      for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
        if ((queueFamilyProperties[i].queueFlags &
             vk::QueueFlagBits::eGraphics) &&
            physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i),
                                                *surface)) {
          graphicsIndex = static_cast<uint32_t>(i);
          presentIndex = graphicsIndex;
          break;
        }
      }
      if (presentIndex == queueFamilyProperties.size()) {
        for (size_t i = 0; i < queueFamilyProperties.size(); i++) {
          if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i),
                                                  *surface)) {
            presentIndex = static_cast<uint32_t>(i);
            break;
          }
        }
      }
    }
    if ((graphicsIndex == queueFamilyProperties.size()) ||
        (presentIndex == queueFamilyProperties.size())) {
      throw std::runtime_error(
          "Could not find a queue for graphics or present -> terminating");
    }

    auto features = physicalDevice.getFeatures2();
    vk::PhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.dynamicRendering = vk::True;
    vulkan13Features.synchronization2 = vk::True;
    features.pNext = &vulkan13Features;

    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority};

    vk::DeviceCreateInfo deviceCreateInfo{.pNext = &features,
                                          .queueCreateInfoCount = 1,
                                          .pQueueCreateInfos =
                                              &deviceQueueCreateInfo};
    deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
    presentQueue = vk::raii::Queue(device, presentIndex, 0);
  }

  void pickPhysicalDevice() {
    std::vector<vk::raii::PhysicalDevice> devices =
        instance.enumeratePhysicalDevices();
    const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
      auto queueFamilies = device.getQueueFamilyProperties();
      bool isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
      const auto qfpIter = std::ranges::find_if(
          queueFamilies, [](vk::QueueFamilyProperties const &qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                   static_cast<vk::QueueFlags>(0);
          });
      isSuitable = isSuitable && (qfpIter != queueFamilies.end());
      auto extensions = device.enumerateDeviceExtensionProperties();
      bool found = true;
      for (auto const &extension : deviceExtensions) {
        auto extensionIter =
            std::ranges::find_if(extensions, [extension](auto const &ext) {
              return strcmp(ext.extensionName, extension) == 0;
            });
        found = found && extensionIter != extensions.end();
      }
      isSuitable = isSuitable && found;
      printf("\n");
      if (isSuitable) {
        physicalDevice = device;
      }
      return isSuitable;
    });
    if (devIter == devices.end()) {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
  }

  void setupDebugMessenger() {
    if (!enableValidationLayers)
      return;

    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severityFlags,
        .messageType = messageTypeFlags,
        .pfnUserCallback = &debugCallback};
    debugMessenger =
        instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
  }

  void createInstance() {

    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName = "Hello Vulkan",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14};

    // detect validationLayers
    std::vector<char const *> requiredLayers;
    if (enableValidationLayers) {
      requiredLayers.assign(validationLayers.begin(), validationLayers.end());
    }

    auto layerProperties = context.enumerateInstanceLayerProperties();

    if (std::ranges::any_of(
            requiredLayers, [&layerProperties](auto const &requiredLayer) {
              return std::ranges::none_of(
                  layerProperties, [requiredLayer](auto const &layerProperty) {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                  });
            })) {
      throw std::runtime_error(
          "One or more required layers are not supported!");
    }

    auto requiredExtensions = getRequiredExtensions();

    auto extensionProperties = context.enumerateInstanceExtensionProperties();
    for (auto const &requiredExtension : requiredExtensions) {
      if (std::ranges::none_of(
              extensionProperties,
              [requiredExtension](auto const &extensionProperty)

              {
                return strcmp(extensionProperty.extensionName,
                              requiredExtension) == 0;
              })) {
        throw std::runtime_error("Required extension not supported: " +
                                 std::string(requiredExtension));
      }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data()};

    instance = vk::raii::Instance(context, createInfo);
  }

  std::vector<const char *> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions =
        glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }

    return extensions;
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }

    device.waitIdle();
  }

  void cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  void createSurface() {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  void createImageViews() {
    swapChainImageViews.clear();

    vk::ImageViewCreateInfo imageViewCreateInfo{
        .viewType = vk::ImageViewType::e2D,
        .format = swapChainImageFormat,
        .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

    for (auto image : swapChainImages) {
      imageViewCreateInfo.image = image;
      swapChainImageViews.emplace_back(device, imageViewCreateInfo);
    }
  }

  void createGraphicsPipeline() {
    vk::raii::ShaderModule shaderModule =
        createShaderModule(readFile("shaders/slang_shaders.spv"));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shaderModule,
        .pName = "vertMain"};
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shaderModule,
        .pName = "fragMain"};

    vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList};

    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::vector dynamicStates = {vk::DynamicState::eViewport,
                                 vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 0, .pushConstantRangeCount = 0};
    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapChainImageFormat};

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .renderPass = nullptr};

    graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
  }

  void createCommandPool() {

      vk::CommandPoolCreateInfo poolInfo{
          .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
          .queueFamilyIndex = graphicsIndex
      };

      commandPool = vk::raii::CommandPool(device, poolInfo);
  }
  void createCommandBuffer() {
      vk::CommandBufferAllocateInfo allocInfo{
          .commandPool = commandPool,
          .level = vk::CommandBufferLevel::ePrimary,
          .commandBufferCount = 1
      };
        commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
  }

  void recordCommandBuffer(uint32_t imageIndex) {
      commandBuffer.begin({});

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


      // vk::ClearValue clearColor = vk::raii::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
      vk::ClearValue clearColor{ {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
      vk::RenderingAttachmentInfo attachmentInfo = {
          .imageView = swapChainImageViews[imageIndex],
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .clearValue = clearColor
      };
      vk::RenderingInfo renderingInfo = {
          .renderArea = { .offset = {0, 0}, .extent = swapChainExtent },
          .layerCount = 1,
          .colorAttachmentCount = 1,
          .pColorAttachments = &attachmentInfo
      };

      commandBuffer.beginRendering(renderingInfo);
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
      commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
      commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0,0), swapChainExtent));
      commandBuffer.draw(3, 1, 0, 0);
      commandBuffer.endRendering();
      transition_image_layout(
          imageIndex,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageLayout::ePresentSrcKHR,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          {},
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::PipelineStageFlagBits2::eBottomOfPipe);
      commandBuffer.end();
  }

  void transition_image_layout(
      uint32_t imageIndex,
      vk::ImageLayout oldLayout,
      vk::ImageLayout newLayout,
      vk::AccessFlags2 srcAccessMask,
      vk::AccessFlags2 dstAccessMask,
      vk::PipelineStageFlags2 srcStageMask,
      vk::PipelineStageFlags2 dstStageMask
  ) {
      vk::ImageMemoryBarrier2 barrier = {
          .srcStageMask = srcStageMask,
          .srcAccessMask = srcAccessMask,
          .dstStageMask = dstStageMask,
          .dstAccessMask = dstAccessMask,
          .oldLayout = oldLayout,
          .newLayout = newLayout,
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

      vk::DependencyInfo dependencyInfo = {
          .dependencyFlags = {},
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &barrier
      };
      commandBuffer.pipelineBarrier2(dependencyInfo);
  }

  void drawFrame() {
      // auto waitFence = vkWaitForFences(device, 1, drawFence, renderFinishedSemaphore, UINT64_MAX);
      graphicsQueue.waitIdle();

      auto [result, imageIndex] = swapChain.acquireNextImage( UINT64_MAX, *presentCompleteSemaphore, nullptr);
      recordCommandBuffer(imageIndex);
      device.resetFences( *drawFence );

      vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
      const vk::SubmitInfo submitInfo{
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &*presentCompleteSemaphore,
          .pWaitDstStageMask = &waitDestinationStageMask,
          .commandBufferCount = 1,
          .pCommandBuffers = &*commandBuffer,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &*renderFinishedSemaphore
      };
      graphicsQueue.submit(submitInfo, *drawFence);

      while (vk::Result::eTimeout == device.waitForFences(*drawFence, vk::True, UINT64_MAX)) ;

      // const vk::PresentInfoKHR presentInfoKHR( **renderFinishedSemaphore, **swapChain, imageIndex);
      const vk::PresentInfoKHR presentInfoKHR{
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &*renderFinishedSemaphore,
          .swapchainCount = 1,
          .pSwapchains = &*swapChain,
          .pImageIndices = &imageIndex
      };
      result = presentQueue.presentKHR(presentInfoKHR);
  }

  void createSyncObjects() {
      presentCompleteSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
      renderFinishedSemaphore = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
      drawFence = vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
  }

  [[nodiscard]] vk::raii::ShaderModule
  createShaderModule(const std::vector<char> &code) const {
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    vk::raii::ShaderModule shaderModule{device, createInfo};

    return shaderModule;
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
    std::cerr << "validation layer: severity: " << to_string(severity)
              << "type " << to_string(type)
              << " msg: " << pCallbackData->pMessage << std::endl;

    return vk::False;
  }

  static std::vector<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();
    return buffer;
  }

};

int main() {
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
