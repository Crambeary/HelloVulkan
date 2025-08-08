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
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/types.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector validationLayers = {"VK_LAYER_KHRONOS_validation"};

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

  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;

  vk::raii::Queue graphicsQueue = nullptr;

  void initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
  }

  void createLogicalDevice() {
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
        physicalDevice.getQueueFamilyProperties();

    const auto qfpIter = std::ranges::find_if(
        queueFamilyProperties, [](vk::QueueFamilyProperties const &qfp) {
          return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) !=
                 static_cast<vk::QueueFlags>(0);
        });

    if (qfpIter == queueFamilyProperties.end()) {
      throw std::runtime_error(
          "No graphics queue found that supports graphics!");
    }

    uint32_t graphicsIndex = static_cast<uint32_t>(
        std::distance(queueFamilyProperties.begin(), qfpIter));

    // vk::PhysicalDeviceFeatures deviceFeatures;

    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex = graphicsIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority};

    vk::StructureChain<vk::PhysicalDeviceFeatures2,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain = {
            {}, {.dynamicRendering = true}, {.extendedDynamicState = true}};

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledExtensionCount =
            static_cast<u_int32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data()};

    device = vk::raii::Device(physicalDevice, deviceCreateInfo);

    graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
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
    }
  }

  void cleanup() {
    glfwDestroyWindow(window);
    glfwTerminate();
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
