#include "expected.hpp"
#include "gsl/gsl-lite.hpp"
#include "vk.hpp"
#include <array>
#include <cstdio>
#include <system_error>
#include <vector>

constexpr uint32_t const sWindowWidth = 800;
constexpr uint32_t const sWindowHeight = 600;

VkPhysicalDeviceFeatures2 sDeviceFeatures = {};

std::array<gsl::czstring, 4> sDeviceExtensions = {
  VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
  VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
  VK_KHR_MAINTENANCE2_EXTENSION_NAME,
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

GLFWwindow* sWindow = nullptr;
bool sFramebufferResized = false;

VkInstance sInstance = VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT sDebugUtilsMessenger = VK_NULL_HANDLE;
VkPhysicalDevice sPhysicalDevice = VK_NULL_HANDLE;

uint32_t sQueueFamilyIndex = UINT32_MAX;
VkDevice sDevice = VK_NULL_HANDLE;
VkSurfaceKHR sSurface = VK_NULL_HANDLE;

static void Draw() noexcept {} // Draw

static void FramebufferResized(GLFWwindow*, int, int) noexcept {
  sFramebufferResized = true;
}

static int sErrorCode;
static std::string sErrorMessage;

static void ErrorCallback(int error, gsl::czstring message) {
  sErrorCode = error;
  sErrorMessage = message;
}

static tl::expected<void, std::system_error> InitWindow() noexcept {
  glfwInit();
  glfwSetErrorCallback(ErrorCallback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  sWindow = glfwCreateWindow(sWindowWidth, sWindowHeight, "01_sphere", nullptr,
                             nullptr);

  if (!sWindow) {
    return tl::unexpected(
      std::system_error(std::error_code(sErrorCode, std::system_category()),
                        "glfwCreateWindow: " + sErrorMessage));
  }

  glfwSetFramebufferSizeCallback(sWindow, FramebufferResized);

  Ensures(sWindow != nullptr);
  return {};
} // InitWindow

static tl::expected<void, std::system_error> InitVulkan() noexcept {
  flextVkInit();

  uint32_t count;
  gsl::czstring* exts = glfwGetRequiredInstanceExtensions(&count);
  std::vector<gsl::czstring> extensions(exts, exts + count);

  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  std::array<gsl::czstring, 1> layers = {"VK_LAYER_LUNARG_standard_validation"};

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "01_sphere";

  VkInstanceCreateInfo instanceCI = {};
  instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCI.pApplicationInfo = &appInfo;
  instanceCI.enabledLayerCount = gsl::narrow_cast<uint32_t>(std::size(layers));
  instanceCI.ppEnabledLayerNames = layers.data();
  instanceCI.enabledExtensionCount =
    gsl::narrow_cast<uint32_t>(std::size(extensions));
  instanceCI.ppEnabledExtensionNames = extensions.data();

  if (auto result = vkCreateInstance(&instanceCI, nullptr, &sInstance);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateInstance"));
  }

  flextVkInitInstance(sInstance);
  Ensures(sInstance != VK_NULL_HANDLE);
  return {};
} // InitVulkan

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT,
  VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void*) noexcept {

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    fprintf(stderr, "ERROR  : %s", pCallbackData->pMessage);
  } else if (messageSeverity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    fprintf(stderr, "WARNING: %s", pCallbackData->pMessage);
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    fprintf(stderr, "   INFO: %s", pCallbackData->pMessage);
  } else if (messageSeverity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    fprintf(stderr, "VERBOSE: %s", pCallbackData->pMessage);
  }

  if (pCallbackData->objectCount > 0) {
    fprintf(stderr, " Objects:");
    for (uint32_t i = 0; i < pCallbackData->objectCount - 1; ++i) {
      auto&& pObject = pCallbackData->pObjects[i];
      fprintf(stderr, " %s (%llx),",
              pObject.pObjectName ? pObject.pObjectName : "<unnamed>",
              pObject.objectHandle);
    }

    auto&& pObject = pCallbackData->pObjects[pCallbackData->objectCount - 1];
    fprintf(stderr, " %s (%llx)",
            pObject.pObjectName ? pObject.pObjectName : "<unnamed>",
            pObject.objectHandle);
  }

  fprintf(stderr, "\n");
  return VK_FALSE;
} // DebugUtilsMessengerCallback

static tl::expected<void, std::system_error>
CreateDebugUtilsMessenger() noexcept {
  Expects(sInstance != VK_NULL_HANDLE);

  VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI = {};
  debugUtilsMessengerCI.sType =
    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugUtilsMessengerCI.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debugUtilsMessengerCI.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugUtilsMessengerCI.pfnUserCallback = DebugUtilsMessengerCallback;

  if (auto result = vkCreateDebugUtilsMessengerEXT(
        sInstance, &debugUtilsMessengerCI, nullptr, &sDebugUtilsMessenger);
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateDebugUtilsMessengerEXT"));
  }

  return {};
} // CreateDebugUtilsMessenger

static [[nodiscard]] uint32_t
GetQueueFamilyIndex(VkPhysicalDevice device, VkQueueFlags queueFlags) noexcept {
  Expects(device != VK_NULL_HANDLE);

  // Get the number of physical device queue family properties
  uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, nullptr);

  // Get the physical device queue family properties
  std::vector<VkQueueFamilyProperties2> properties(count);
  for (auto& property : properties) {
    property.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    property.pNext = nullptr;
  }

  vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, properties.data());

  for (uint32_t i = 0; i < count; ++i) {
    VkQueueFamilyProperties props = properties[i].queueFamilyProperties;
    if (props.queueCount == 0) continue;
    if (props.queueFlags & queueFlags) return i;
  }

  return UINT32_MAX;
} // GetQueueFamilyIndex

static [[nodiscard]] bool
ComparePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2 a,
                              VkPhysicalDeviceFeatures2 b) noexcept {
  bool result = false;
  result |= (a.features.robustBufferAccess == b.features.robustBufferAccess);
  result |= (a.features.fullDrawIndexUint32 == b.features.fullDrawIndexUint32);
  result |= (a.features.imageCubeArray == b.features.imageCubeArray);
  result |= (a.features.independentBlend == b.features.independentBlend);
  result |= (a.features.geometryShader == b.features.geometryShader);
  result |= (a.features.tessellationShader == b.features.tessellationShader);
  result |= (a.features.sampleRateShading == b.features.sampleRateShading);
  result |= (a.features.dualSrcBlend == b.features.dualSrcBlend);
  result |= (a.features.logicOp == b.features.logicOp);
  result |= (a.features.multiDrawIndirect == b.features.multiDrawIndirect);
  result |= (a.features.drawIndirectFirstInstance ==
             b.features.drawIndirectFirstInstance);
  result |= (a.features.depthClamp == b.features.depthClamp);
  result |= (a.features.depthBiasClamp == b.features.depthBiasClamp);
  result |= (a.features.fillModeNonSolid == b.features.fillModeNonSolid);
  result |= (a.features.depthBounds == b.features.depthBounds);
  result |= (a.features.wideLines == b.features.wideLines);
  result |= (a.features.largePoints == b.features.largePoints);
  result |= (a.features.alphaToOne == b.features.alphaToOne);
  result |= (a.features.multiViewport == b.features.multiViewport);
  result |= (a.features.samplerAnisotropy == b.features.samplerAnisotropy);
  result |=
    (a.features.textureCompressionETC2 == b.features.textureCompressionETC2);
  result |= (a.features.textureCompressionASTC_LDR ==
             b.features.textureCompressionASTC_LDR);
  result |=
    (a.features.textureCompressionBC == b.features.textureCompressionBC);
  result |=
    (a.features.occlusionQueryPrecise == b.features.occlusionQueryPrecise);
  result |=
    (a.features.pipelineStatisticsQuery == b.features.pipelineStatisticsQuery);
  result |= (a.features.vertexPipelineStoresAndAtomics ==
             b.features.vertexPipelineStoresAndAtomics);
  result |= (a.features.fragmentStoresAndAtomics ==
             b.features.fragmentStoresAndAtomics);
  result |= (a.features.shaderTessellationAndGeometryPointSize ==
             b.features.shaderTessellationAndGeometryPointSize);
  result |= (a.features.shaderImageGatherExtended ==
             b.features.shaderImageGatherExtended);
  result |= (a.features.shaderStorageImageExtendedFormats ==
             b.features.shaderStorageImageExtendedFormats);
  result |= (a.features.shaderStorageImageMultisample ==
             b.features.shaderStorageImageMultisample);
  result |= (a.features.shaderStorageImageReadWithoutFormat ==
             b.features.shaderStorageImageReadWithoutFormat);
  result |= (a.features.shaderStorageImageWriteWithoutFormat ==
             b.features.shaderStorageImageWriteWithoutFormat);
  result |= (a.features.shaderUniformBufferArrayDynamicIndexing ==
             b.features.shaderUniformBufferArrayDynamicIndexing);
  result |= (a.features.shaderSampledImageArrayDynamicIndexing ==
             b.features.shaderSampledImageArrayDynamicIndexing);
  result |= (a.features.shaderStorageBufferArrayDynamicIndexing ==
             b.features.shaderStorageBufferArrayDynamicIndexing);
  result |= (a.features.shaderStorageImageArrayDynamicIndexing ==
             b.features.shaderStorageImageArrayDynamicIndexing);
  result |= (a.features.shaderClipDistance == b.features.shaderClipDistance);
  result |= (a.features.shaderCullDistance == b.features.shaderCullDistance);
  result |= (a.features.shaderFloat64 == b.features.shaderFloat64);
  result |= (a.features.shaderInt64 == b.features.shaderInt64);
  result |= (a.features.shaderInt16 == b.features.shaderInt16);
  result |=
    (a.features.shaderResourceResidency == b.features.shaderResourceResidency);
  result |=
    (a.features.shaderResourceMinLod == b.features.shaderResourceMinLod);
  result |= (a.features.sparseBinding == b.features.sparseBinding);
  result |=
    (a.features.sparseResidencyBuffer == b.features.sparseResidencyBuffer);
  result |=
    (a.features.sparseResidencyImage2D == b.features.sparseResidencyImage2D);
  result |=
    (a.features.sparseResidencyImage3D == b.features.sparseResidencyImage3D);
  result |=
    (a.features.sparseResidency2Samples == b.features.sparseResidency2Samples);
  result |=
    (a.features.sparseResidency4Samples == b.features.sparseResidency4Samples);
  result |=
    (a.features.sparseResidency8Samples == b.features.sparseResidency8Samples);
  result |= (a.features.sparseResidency16Samples ==
             b.features.sparseResidency16Samples);
  result |=
    (a.features.sparseResidencyAliased == b.features.sparseResidencyAliased);
  result |=
    (a.features.variableMultisampleRate == b.features.variableMultisampleRate);
  result |= (a.features.inheritedQueries == b.features.inheritedQueries);
  return result;
} // ComparePhysicalDeviceFeatures

static [[nodiscard]] tl::expected<bool, std::system_error> IsPhysicalDeviceGood(
  VkPhysicalDevice device, VkPhysicalDeviceFeatures2 features,
  gsl::span<gsl::czstring> extensions, VkQueueFlags queueFlags) noexcept {
  Expects(device != VK_NULL_HANDLE);

  //
  // Get the properties.
  //

  VkPhysicalDeviceMaintenance3Properties maint3Props = {};
  maint3Props.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;

  VkPhysicalDeviceProperties2 physicalDeviceProperties = {};
  physicalDeviceProperties.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  physicalDeviceProperties.pNext = &maint3Props;

  vkGetPhysicalDeviceProperties2(device, &physicalDeviceProperties);

  //
  // Get the features.
  //

  VkPhysicalDeviceFeatures2 physicalDeviceFeatures = {};
  physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

  vkGetPhysicalDeviceFeatures2(device, &physicalDeviceFeatures);

  //
  // Get the extension properties.
  //

  // Get the number of physical device extension properties.
  uint32_t count;
  if (auto result =
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkEnumerateDeviceExtensionProperties"));
  }

  // Get the physical device extension properties.
  std::vector<VkExtensionProperties> properties(count);
  if (auto result = vkEnumerateDeviceExtensionProperties(
        device, nullptr, &count, properties.data());
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkEnumerateDeviceExtensionProperties"));
  }

  uint32_t queueFamilyIndex = GetQueueFamilyIndex(device, queueFlags);

  //
  // Check all queried data to see if this device is good.
  //

  // Check for any required features
  if (!ComparePhysicalDeviceFeatures(physicalDeviceFeatures, features)) {
    return false;
  }

  // Check for each required extension
  for (auto&& required : extensions) {
    bool found = false;

    for (auto&& property : properties) {
      if (std::strcmp(required, property.extensionName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) return false;
  }

  // Check for the queue
  if (queueFamilyIndex == UINT32_MAX) return false;

  return true;
} // IsPhysicalDeviceGood

static tl::expected<void, std::system_error> ChoosePhysicalDevice() noexcept {
  Expects(sInstance != VK_NULL_HANDLE);

  sDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  sDeviceFeatures.features.fullDrawIndexUint32 = VK_TRUE;
  sDeviceFeatures.features.geometryShader = VK_TRUE;
  sDeviceFeatures.features.tessellationShader = VK_TRUE;
  sDeviceFeatures.features.depthClamp = VK_TRUE;
  sDeviceFeatures.features.fillModeNonSolid = VK_TRUE;
  sDeviceFeatures.features.wideLines = VK_TRUE;
  sDeviceFeatures.features.largePoints = VK_TRUE;
  sDeviceFeatures.features.multiViewport = VK_TRUE;
  sDeviceFeatures.features.pipelineStatisticsQuery = VK_TRUE;
  sDeviceFeatures.features.shaderTessellationAndGeometryPointSize = VK_TRUE;
  sDeviceFeatures.features.shaderUniformBufferArrayDynamicIndexing = VK_TRUE;
  sDeviceFeatures.features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
  sDeviceFeatures.features.shaderStorageBufferArrayDynamicIndexing = VK_TRUE;
  sDeviceFeatures.features.shaderStorageImageArrayDynamicIndexing = VK_TRUE;
  sDeviceFeatures.features.shaderClipDistance = VK_TRUE;
  sDeviceFeatures.features.shaderCullDistance = VK_TRUE;
  sDeviceFeatures.features.shaderFloat64 = VK_TRUE;
  sDeviceFeatures.features.shaderInt64 = VK_TRUE;

  uint32_t count;
  if (auto result = vkEnumeratePhysicalDevices(sInstance, &count, nullptr);
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkEnumeratePhysicalDevices"));
  }

  std::vector<VkPhysicalDevice> devices(count);
  if (auto result =
        vkEnumeratePhysicalDevices(sInstance, &count, devices.data());
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkEnumeratePhysicalDevices"));
  }

  for (auto&& device : devices) {
    if (auto good =
          IsPhysicalDeviceGood(device, sDeviceFeatures, sDeviceExtensions,
                               VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
      sPhysicalDevice = device;
      break;
    }
  }

  sQueueFamilyIndex = GetQueueFamilyIndex(
    sPhysicalDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  Ensures(sPhysicalDevice != VK_NULL_HANDLE);
  Ensures(sQueueFamilyIndex != UINT32_MAX);
  return {};
} // ChoosePhysicalDevice

static tl::expected<void, std::system_error> CreateDevice() noexcept {
  Expects(sInstance != VK_NULL_HANDLE);
  Expects(sPhysicalDevice != VK_NULL_HANDLE);
  Expects(sQueueFamilyIndex != UINT32_MAX);

  float priority = 1.f;

  VkDeviceQueueCreateInfo deviceQueueCI = {};
  deviceQueueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCI.queueFamilyIndex = sQueueFamilyIndex;
  deviceQueueCI.queueCount = 1;
  deviceQueueCI.pQueuePriorities = &priority;

  VkDeviceCreateInfo deviceCI = {};
  deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCI.pNext = &sDeviceFeatures;
  deviceCI.queueCreateInfoCount = 1;
  deviceCI.pQueueCreateInfos = &deviceQueueCI;
  deviceCI.enabledExtensionCount =
    gsl::narrow_cast<uint32_t>(std::size(sDeviceExtensions));
  deviceCI.ppEnabledExtensionNames = sDeviceExtensions.data();

  if (auto result =
        vkCreateDevice(sPhysicalDevice, &deviceCI, nullptr, &sDevice);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateDevice"));
  }

  vk::NameObject(sDevice, VK_OBJECT_TYPE_INSTANCE, sInstance, "sInstance");
  if (sDebugUtilsMessenger != VK_NULL_HANDLE) {
    vk::NameObject(sDevice, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,
                   sDebugUtilsMessenger, "sDebugUtilsMessenger");
  }
  vk::NameObject(sDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE, sPhysicalDevice,
                 "sPhysicalDevice");
  vk::NameObject(sDevice, VK_OBJECT_TYPE_DEVICE, sDevice, "sDevice");

  Ensures(sDevice != VK_NULL_HANDLE);
  return {};
} // CreateDevice

static tl::expected<void, std::system_error> CreateSurface() noexcept {
  Expects(sWindow != nullptr);
  Expects(sInstance != VK_NULL_HANDLE);
  Expects(sPhysicalDevice != VK_NULL_HANDLE);

  if (auto result =
        glfwCreateWindowSurface(sInstance, sWindow, nullptr, &sSurface);
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "glfwCreateWindowSurface"));
  }

  Ensures(sSurface != VK_NULL_HANDLE);
  return {};
}

int main() {
  // clang-format off
  auto res = InitWindow()
    .and_then(InitVulkan)
    .and_then(CreateDebugUtilsMessenger)
    .and_then(ChoosePhysicalDevice)
    .and_then(CreateDevice)
    .and_then(CreateSurface)
    ;
  // clang-format on

  if (!res) {
    std::fprintf(stderr, "Init: %s\n", res.error().what());
    std::exit(EXIT_FAILURE);
  }

  while (!glfwWindowShouldClose(sWindow)) {
    glfwPollEvents();
    Draw();
  }
}
