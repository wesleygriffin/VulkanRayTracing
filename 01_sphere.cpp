// clang-format off
#include "flextVk.h"
#define VK_VERSION_1_0
#include "GLFW/glfw3.h"

// vma needs these, flextGL should probably generate them
using PFN_vkGetPhysicalDeviceProperties =
  decltype(vkGetPhysicalDeviceProperties);
using PFN_vkGetPhysicalDeviceMemoryProperties =
  decltype(vkGetPhysicalDeviceMemoryProperties);
using PFN_vkAllocateMemory = decltype(vkAllocateMemory);
using PFN_vkFreeMemory = decltype(vkFreeMemory);
using PFN_vkMapMemory = decltype(vkMapMemory);
using PFN_vkUnmapMemory = decltype(vkUnmapMemory);
using PFN_vkFlushMappedMemoryRanges = decltype(vkFlushMappedMemoryRanges);
using PFN_vkInvalidateMappedMemoryRanges =
  decltype(vkInvalidateMappedMemoryRanges);
using PFN_vkBindBufferMemory = decltype(vkBindBufferMemory);
using PFN_vkBindImageMemory = decltype(vkBindImageMemory);
using PFN_vkGetBufferMemoryRequirements =
  decltype(vkGetBufferMemoryRequirements);
using PFN_vkGetImageMemoryRequirements = decltype(vkGetImageMemoryRequirements);
using PFN_vkCreateBuffer = decltype(vkCreateBuffer);
using PFN_vkDestroyBuffer = decltype(vkDestroyBuffer);
using PFN_vkCreateImage = decltype(vkCreateImage);
using PFN_vkDestroyImage = decltype(vkDestroyImage);
using PFN_vkCmdCopyBuffer = decltype(vkCmdCopyBuffer);

#include "vk_mem_alloc.h"
// clang-format on

#include "arcball.hpp"
#include "camera.hpp"
#include "expected.hpp"
#include "glm/common.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "gsl/gsl-lite.hpp"
#include "shader_binding_table_generator.hpp"
#include "vk_result.hpp"
#include <array>
#include <cstdio>
#include <system_error>
#include <vector>

#ifdef NDEBUG

#define LOG_ENTER() do {} while(false)
#define LOG_LEAVE() do {} while(false)

#else

#define LOG_ENTER()                                                            \
  do {                                                                         \
    std::fprintf(stderr, "ENTER: %s (%s:%d)\n", __func__, __FILE__, __LINE__); \
  } while (false)

#define LOG_LEAVE()                                                            \
  do {                                                                         \
    std::fprintf(stderr, "LEAVE: %s (%s:%d)\n", __func__, __FILE__, __LINE__); \
  } while (false)

#endif

static constexpr std::uint32_t const kWindowWidth = 1600;
static constexpr std::uint32_t const kWindowHeight = 1200;

static Camera sCamera(90.f,
                      static_cast<float>(kWindowWidth) /
                        static_cast<float>(kWindowHeight),
                      glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, -1.f),
                      glm::vec3(0.f, 1.f, 0.f));
static Arcball sArcball;

static bool sLeftMouseButtonDown = false;
static bool sRightMouseButtonDown = false;
static glm::vec2 sCurrMousePos = {0.f, 0.f};
static glm::vec2 sPrevMousePos = {0.f, 0.f};

static VkPhysicalDeviceFeatures2 sDeviceFeatures = {};

static std::array<gsl::czstring, 5> sDeviceExtensions = {
  VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
  VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
  VK_KHR_MAINTENANCE2_EXTENSION_NAME,
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  VK_NV_RAY_TRACING_EXTENSION_NAME,
};

static GLFWwindow* sWindow = nullptr;
static bool sFramebufferResized = false;

static VkInstance sInstance = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT sDebugUtilsMessenger = VK_NULL_HANDLE;
static VkPhysicalDevice sPhysicalDevice = VK_NULL_HANDLE;

static std::uint32_t sQueueFamilyIndex = UINT32_MAX;
static VkDevice sDevice = VK_NULL_HANDLE;
static VkQueue sQueue = VK_NULL_HANDLE;
static VkCommandPool sCommandPool = VK_NULL_HANDLE;
static VmaAllocator sAllocator = VK_NULL_HANDLE;

static VkSurfaceFormatKHR sSurfaceColorFormat = {
  VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

static VkRenderPass sRenderPass = VK_NULL_HANDLE;
static VkSurfaceKHR sSurface = VK_NULL_HANDLE;

static VkSwapchainKHR sSwapchain = VK_NULL_HANDLE;
static VkExtent2D sSwapchainExtent;
static std::vector<VkImage> sSwapchainImages;
static std::vector<VkImageView> sSwapchainImageViews;

struct Frame {
  VkSemaphore imageAvailable{VK_NULL_HANDLE};
  VkCommandPool commandPool{VK_NULL_HANDLE};
  VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
  VkFramebuffer framebuffer{VK_NULL_HANDLE};
}; // struct Frame

static std::uint32_t sCurrentFrame = 0;
static std::vector<Frame> sFrames;
static std::vector<VkFence> sFramesComplete;
static VkSemaphore sRenderFinished = VK_NULL_HANDLE;

static VkDescriptorPool sDescriptorPool = VK_NULL_HANDLE;
static VkQueryPool sQueryPool = VK_NULL_HANDLE;

static VkDescriptorSetLayout sDescriptorSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout sPipelineLayout = VK_NULL_HANDLE;
static VkPipeline sPipeline = VK_NULL_HANDLE;

struct UniformBuffer {
  glm::vec4 Eye;
  glm::vec4 U;
  glm::vec4 V;
  glm::vec4 W;
}; // struct UniformBuffer

static VkBuffer sUniformBuffer = VK_NULL_HANDLE;
static VmaAllocation sUniformBufferAllocation = VK_NULL_HANDLE;

static VkImage sOutputImage = VK_NULL_HANDLE;
static VmaAllocation sOutputImageAllocation = VK_NULL_HANDLE;
static VkImageView sOutputImageView = VK_NULL_HANDLE;

struct Sphere {
  glm::vec3 aabbMin;
  glm::vec3 aabbMax;

  Sphere(glm::vec3 center, float radius) noexcept
    : aabbMin(center - glm::vec3(radius))
    , aabbMax(center + glm::vec3(radius)) {}

  glm::vec3 center() const noexcept {
    return (aabbMax + aabbMin) / glm::vec3(2.f);
  }

  float radius() const noexcept { return (aabbMax.x - aabbMin.x) / 2.f; }
}; // struct Spheres

static std::array<Sphere, 2> sSpheres = {
  Sphere(glm::vec3(0.f, 0.f, -1.f), .5f),
  Sphere(glm::vec3(0.f, -100.5f, -1.f), 100.f),
};

static VkBuffer sSpheresBuffer = VK_NULL_HANDLE;
static VmaAllocation sSpheresBufferAllocation = VK_NULL_HANDLE;

static VkAccelerationStructureNV sBottomLevelAccelerationStructure =
  VK_NULL_HANDLE;
static VmaAllocation sBottomLevelAccelerationStructureAllocation =
  VK_NULL_HANDLE;

static VkAccelerationStructureNV sTopLevelAccelerationStructure =
  VK_NULL_HANDLE;
static VmaAllocation sTopLevelAccelerationStructureAllocation = VK_NULL_HANDLE;

static std::uint32_t sShaderGroupHandleSize = 0;
static ShaderBindingTableGenerator sShaderBindingTableGenerator;
static VkBuffer sShaderBindingTable = VK_NULL_HANDLE;
static VmaAllocation sShaderBindingTableAllocation = VK_NULL_HANDLE;

static std::vector<VkDescriptorSet> sDescriptorSets;

static void MouseButtonChanged(GLFWwindow*, int button, int action, int) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    sLeftMouseButtonDown = (action == GLFW_PRESS);
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    sRightMouseButtonDown = (action == GLFW_PRESS);
  }
} // MouseButtonChanged

static void CursorMoved(GLFWwindow*, double x, double y) {
  glm::vec2 const swapchainSize(sSwapchainExtent.width,
                                sSwapchainExtent.height);
  glm::vec2 const currMousePos(x, y);

  if (sRightMouseButtonDown) {
    glm::vec2 const delta = sPrevMousePos - currMousePos;
    glm::vec2 const nDelta = delta / swapchainSize;
    float const max =
      std::abs(nDelta.x) > std::abs(nDelta.y) ? nDelta.x : nDelta.y;
    sCamera.scale(std::min(max, 0.9f));
  } else if (sLeftMouseButtonDown) {
    glm::vec2 const a = sPrevMousePos / swapchainSize;
    glm::vec2 const b = currMousePos / swapchainSize;
    sCamera.rotate(sArcball.rotate(b, a));
  }

  sPrevMousePos = glm::vec2(x, y);
} // CursorPosChanged

template <class T>
void NameObject(VkDevice device, VkObjectType objectType, T objectHandle,
                gsl::czstring objectName) noexcept {
#if 0 // broken in 1.1.101.0 SDK with validation layers enabled
  VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
    VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, objectType,
    reinterpret_cast<std::uint64_t>(objectHandle), objectName};
  vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
#endif
} // NameObject

template <class T>
tl::expected<T, std::system_error>
MapMemory(VmaAllocator allocator, VmaAllocation allocation) noexcept {
  void* ptr;
  if (auto result = vmaMapMemory(allocator, allocation, &ptr);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaMapMemory"));
  }
  return reinterpret_cast<T>(ptr);
} // MapMemory

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
  LOG_ENTER();

  glfwInit();
  glfwSetErrorCallback(ErrorCallback);

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  sWindow = glfwCreateWindow(kWindowWidth, kWindowHeight, "01_sphere", nullptr,
                             nullptr);

  if (!sWindow) {
    return tl::unexpected(
      std::system_error(std::error_code(sErrorCode, std::system_category()),
                        "glfwCreateWindow: " + sErrorMessage));
  }

  glfwSetMouseButtonCallback(sWindow, MouseButtonChanged);
  glfwSetCursorPosCallback(sWindow, CursorMoved);
  glfwSetFramebufferSizeCallback(sWindow, FramebufferResized);

  Ensures(sWindow != nullptr);

  LOG_LEAVE();
  return {};
} // InitWindow

static tl::expected<void, std::system_error> InitVulkan() noexcept {
  LOG_ENTER();
  flextVkInit();

  std::uint32_t count;
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
  instanceCI.enabledLayerCount =
    gsl::narrow_cast<std::uint32_t>(std::size(layers));
  instanceCI.ppEnabledLayerNames = layers.data();
  instanceCI.enabledExtensionCount =
    gsl::narrow_cast<std::uint32_t>(std::size(extensions));
  instanceCI.ppEnabledExtensionNames = extensions.data();

  if (auto result = vkCreateInstance(&instanceCI, nullptr, &sInstance);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateInstance"));
  }

  flextVkInitInstance(sInstance);
  Ensures(sInstance != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // InitVulkan

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessengerCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT,
  VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, void*) noexcept {

  auto printObjects = [&]() {
    if (pCallbackData->objectCount > 0) {
      std::fprintf(stderr, " Objects:");
      for (std::uint32_t i = 0; i < pCallbackData->objectCount - 1; ++i) {
        auto&& pObject = pCallbackData->pObjects[i];
        std::fprintf(stderr, " %s (%llx),",
                     pObject.pObjectName ? pObject.pObjectName : "<unnamed>",
                     pObject.objectHandle);
      }

      auto&& pObject = pCallbackData->pObjects[pCallbackData->objectCount - 1];
      std::fprintf(stderr, " %s (%llx)",
                   pObject.pObjectName ? pObject.pObjectName : "<unnamed>",
                   pObject.objectHandle);
    }
  };

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    std::fprintf(stderr, "ERROR  : %s", pCallbackData->pMessage);
    printObjects();
    std::fprintf(stderr, "\n");
  } else if (messageSeverity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::fprintf(stderr, "WARNING: %s", pCallbackData->pMessage);
    printObjects();
    std::fprintf(stderr, "\n");
  } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    //std::fprintf(stderr, "   INFO: %s", pCallbackData->pMessage);
    //printObjects();
    //std::fprintf(stderr, "\n");
  } else if (messageSeverity >=
             VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    //std::fprintf(stderr, "VERBOSE: %s", pCallbackData->pMessage);
    //printObjects();
    //std::fprintf(stderr, "\n");
  }

  return VK_FALSE;
} // DebugUtilsMessengerCallback

static tl::expected<void, std::system_error>
CreateDebugUtilsMessenger() noexcept {
  LOG_ENTER();
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
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateDebugUtilsMessengerEXT"));
  }

  LOG_LEAVE();
  return {};
} // CreateDebugUtilsMessenger

[[nodiscard]] static std::uint32_t
GetQueueFamilyIndex(VkPhysicalDevice device, VkQueueFlags queueFlags) noexcept {
  LOG_ENTER();
  Expects(device != VK_NULL_HANDLE);

  // Get the number of physical device queue family properties
  std::uint32_t count;
  vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, nullptr);

  // Get the physical device queue family properties
  std::vector<VkQueueFamilyProperties2> properties(count);
  for (auto& property : properties) {
    property.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    property.pNext = nullptr;
  }

  vkGetPhysicalDeviceQueueFamilyProperties2(device, &count, properties.data());

  for (std::uint32_t i = 0; i < count; ++i) {
    VkQueueFamilyProperties props = properties[i].queueFamilyProperties;
    if (props.queueCount == 0) continue;

    if (props.queueFlags & queueFlags) {
      LOG_LEAVE();
      return i;
    }
  }

  LOG_LEAVE();
  return UINT32_MAX;
} // GetQueueFamilyIndex

[[nodiscard]] static bool
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

[[nodiscard]] static tl::expected<bool, std::system_error> IsPhysicalDeviceGood(
  VkPhysicalDevice device, VkPhysicalDeviceFeatures2 features,
  gsl::span<gsl::czstring> extensions, VkQueueFlags queueFlags) noexcept {
  LOG_ENTER();
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
  std::uint32_t count;
  if (auto result =
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkEnumerateDeviceExtensionProperties"));
  }

  // Get the physical device extension properties.
  std::vector<VkExtensionProperties> properties(count);
  if (auto result = vkEnumerateDeviceExtensionProperties(
        device, nullptr, &count, properties.data());
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkEnumerateDeviceExtensionProperties"));
  }

  std::uint32_t queueFamilyIndex = GetQueueFamilyIndex(device, queueFlags);

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

    if (!found) {
      LOG_LEAVE();
      return false;
    }
  }

  // Check for the queue
  if (queueFamilyIndex == UINT32_MAX) {
    LOG_LEAVE();
    return false;
  }

  LOG_LEAVE();
  return true;
} // IsPhysicalDeviceGood

static tl::expected<void, std::system_error> ChoosePhysicalDevice() noexcept {
  LOG_ENTER();
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

  std::uint32_t count;
  if (auto result = vkEnumeratePhysicalDevices(sInstance, &count, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkEnumeratePhysicalDevices"));
  }

  std::vector<VkPhysicalDevice> devices(count);
  if (auto result =
        vkEnumeratePhysicalDevices(sInstance, &count, devices.data());
      result != VK_SUCCESS) {
    LOG_LEAVE();
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

  LOG_LEAVE();
  return {};
} // ChoosePhysicalDevice

static tl::expected<void, std::system_error> CreateDevice() noexcept {
  LOG_ENTER();
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
    gsl::narrow_cast<std::uint32_t>(std::size(sDeviceExtensions));
  deviceCI.ppEnabledExtensionNames = sDeviceExtensions.data();

  if (auto result =
        vkCreateDevice(sPhysicalDevice, &deviceCI, nullptr, &sDevice);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateDevice"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_INSTANCE, sInstance, "sInstance");
  if (sDebugUtilsMessenger != VK_NULL_HANDLE) {
    NameObject(sDevice, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT,
               sDebugUtilsMessenger, "sDebugUtilsMessenger");
  }
  NameObject(sDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE, sPhysicalDevice,
             "sPhysicalDevice");
  NameObject(sDevice, VK_OBJECT_TYPE_DEVICE, sDevice, "sDevice");

  VkDeviceQueueInfo2 deviceQueueInfo = {};
  deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
  deviceQueueInfo.queueFamilyIndex = sQueueFamilyIndex;
  deviceQueueInfo.queueIndex = 0;

  vkGetDeviceQueue2(sDevice, &deviceQueueInfo, &sQueue);

  NameObject(sDevice, VK_OBJECT_TYPE_QUEUE, sQueue, "sQueue");

  Ensures(sDevice != VK_NULL_HANDLE);
  Ensures(sQueue != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateDevice

static tl::expected<void, std::system_error> CreateCommandPool() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  VkCommandPoolCreateInfo commandPoolCI = {};
  commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCI.queueFamilyIndex = sQueueFamilyIndex;

  if (auto result =
        vkCreateCommandPool(sDevice, &commandPoolCI, nullptr, &sCommandPool);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateCommandPool"));
  }

  Ensures(sCommandPool != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateCommandPool

[[nodiscard]] static tl::expected<VkCommandBuffer, std::system_error>
BeginOneTimeSubmit() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sCommandPool != VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo commandBufferAI = {};
  commandBufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAI.commandPool = sCommandPool;
  commandBufferAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAI.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  if (auto result =
        vkAllocateCommandBuffers(sDevice, &commandBufferAI, &commandBuffer);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkAllocateCommandBuffers"));
  }

  VkCommandBufferBeginInfo commandBufferBI = {};
  commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  if (auto result = vkBeginCommandBuffer(commandBuffer, &commandBufferBI);
      result != VK_SUCCESS) {
    vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkBeginCommandBuffer"));
  }

  LOG_LEAVE();
  return commandBuffer;
} // BeginOneTimeSubmit

static tl::expected<void, std::system_error>
EndOneTimeSubmit(VkCommandBuffer commandBuffer) noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sCommandPool != VK_NULL_HANDLE);
  Expects(sQueue != VK_NULL_HANDLE);
  Expects(commandBuffer != VK_NULL_HANDLE);

  if (auto result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
    vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkEndCommandBuffer"));
  }

  VkFenceCreateInfo fenceCI = {};
  fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

  VkFence fence;
  if (auto result = vkCreateFence(sDevice, &fenceCI, nullptr, &fence);
      result != VK_SUCCESS) {
    vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateFence"));
  }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  if (auto result = vkQueueSubmit(sQueue, 1, &submitInfo, fence);
      result != VK_SUCCESS) {
    vkDestroyFence(sDevice, fence, nullptr);
    vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkQueueSubmit"));
  }

  if (auto result = vkWaitForFences(sDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    result != VK_SUCCESS) {
    vkDestroyFence(sDevice, fence, nullptr);
    vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkWaitForFences"));
  }

  vkDestroyFence(sDevice, fence, nullptr);
  vkFreeCommandBuffers(sDevice, sCommandPool, 1, &commandBuffer);

  LOG_LEAVE();
  return {};
} // EndOneTimeSubmit

static tl::expected<void, std::system_error> CreateAllocator() noexcept {
  LOG_ENTER();
  Expects(sPhysicalDevice != VK_NULL_HANDLE);
  Expects(sDevice != VK_NULL_HANDLE);

  VmaAllocatorCreateInfo allocatorCI = {};
  allocatorCI.flags = VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
  allocatorCI.physicalDevice = sPhysicalDevice;
  allocatorCI.device = sDevice;

  if (auto result = vmaCreateAllocator(&allocatorCI, &sAllocator);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateAllocator"));
  }

  Ensures(sAllocator != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateAllocator

static tl::expected<void, std::system_error> CreateRenderPass() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  VkAttachmentDescription colorAttachmentDescription = {
    0,                                       // flags
    sSurfaceColorFormat.format,              // format
    VK_SAMPLE_COUNT_1_BIT,                   // samples
    VK_ATTACHMENT_LOAD_OP_CLEAR,             // loadOp (color and depth)
    VK_ATTACHMENT_STORE_OP_DONT_CARE,        // storeOp (color and depth)
    VK_ATTACHMENT_LOAD_OP_DONT_CARE,         // stencilLoadOp
    VK_ATTACHMENT_STORE_OP_DONT_CARE,        // stencilStoreOp
    VK_IMAGE_LAYOUT_UNDEFINED,               // initialLayout
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // finalLayout
  };

  VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpassDescription = {
    0,                               // flags
    VK_PIPELINE_BIND_POINT_GRAPHICS, // pipelineBindPoint
    0,                               // inputAttachmentCount
    nullptr,                         // pInputAttachments
    1,                               // colorAttachmentCount
    &color,                          // pColorAttachments (array)
    nullptr,                         // pResolveAttachments (array)
    nullptr,                         // pDepthStencilAttachment (single)
    0,                               // preserveAttachmentCount
    nullptr                          // pPreserveAttachments
  };

  std::array<VkSubpassDependency, 2> subpassDependencies = {
    VkSubpassDependency{
      VK_SUBPASS_EXTERNAL,                           // srcSubpass
      0,                                             // dstSubpass
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // srcStageMask
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
      VK_ACCESS_MEMORY_READ_BIT,                     // srcAccessMask
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dstAccessMask
      VK_DEPENDENCY_BY_REGION_BIT             // dependencyFlags
    },
    VkSubpassDependency{
      0,                                             // srcSubpass
      VK_SUBPASS_EXTERNAL,                           // dstSubpass
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // dstStageMask
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
      VK_ACCESS_MEMORY_READ_BIT,              // dstAccessMask
      VK_DEPENDENCY_BY_REGION_BIT             // dependencyFlags
    }};

  VkRenderPassCreateInfo renderPassCI = {};
  renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCI.attachmentCount = 1;
  renderPassCI.pAttachments = &colorAttachmentDescription;
  renderPassCI.subpassCount = 1;
  renderPassCI.pSubpasses = &subpassDescription;
  renderPassCI.dependencyCount =
    gsl::narrow_cast<std::uint32_t>(subpassDependencies.size());
  renderPassCI.pDependencies = subpassDependencies.data();

  if (auto result =
        vkCreateRenderPass(sDevice, &renderPassCI, nullptr, &sRenderPass);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateRenderPass"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_RENDER_PASS, sRenderPass, "sRenderPass");

  Ensures(sRenderPass != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateRenderPass

static tl::expected<void, std::system_error> CreateSurface() noexcept {
  LOG_ENTER();
  Expects(sWindow != nullptr);
  Expects(sInstance != VK_NULL_HANDLE);
  Expects(sPhysicalDevice != VK_NULL_HANDLE);
  Expects(sQueueFamilyIndex != UINT32_MAX);

  if (auto result =
        glfwCreateWindowSurface(sInstance, sWindow, nullptr, &sSurface);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "glfwCreateWindowSurface"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_SURFACE_KHR, sSurface, "sSurface");

  VkBool32 surfaceSupported = VK_FALSE;
  if (auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
        sPhysicalDevice, sQueueFamilyIndex, sSurface, &surfaceSupported);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkGetPhysicalDeviceSurfaceSupportKHR"));
  }

  Ensures(sSurface != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateSurface

static tl::expected<void, std::system_error> VerifySurfaceFormat() noexcept {
  LOG_ENTER();
  Expects(sPhysicalDevice != VK_NULL_HANDLE);
  Expects(sSurface != VK_NULL_HANDLE);
  Expects(sSurfaceColorFormat.format != VK_FORMAT_UNDEFINED);

  VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
  surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  surfaceInfo.surface = sSurface;

  std::uint32_t count;

  if (auto result = vkGetPhysicalDeviceSurfaceFormats2KHR(
        sPhysicalDevice, &surfaceInfo, &count, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkGetPhysicalDeviceSurfaceFormats2KHR"));
  }

  std::vector<VkSurfaceFormat2KHR> formats(count);
  for (auto& format : formats) {
    format.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
    format.pNext = nullptr;
  }

  if (auto result = vkGetPhysicalDeviceSurfaceFormats2KHR(
        sPhysicalDevice, &surfaceInfo, &count, formats.data());
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkGetPhysicalDeviceSurfaceFormats2KHR"));
  }

  if (formats.size() == 1 &&
      formats[0].surfaceFormat.format == VK_FORMAT_UNDEFINED) {
    LOG_LEAVE();
    return {};
  }

  for (auto&& format : formats) {
    if (format.surfaceFormat.format == sSurfaceColorFormat.format &&
        format.surfaceFormat.colorSpace == sSurfaceColorFormat.colorSpace) {
      LOG_LEAVE();
      return {};
    }
  }

  LOG_LEAVE();
  return tl::unexpected(std::system_error(
    vk::make_error_code(vk::VulkanResult::kErrorFormatNotSupported)));
} // VerifySurfaceFormat

static tl::expected<void, std::system_error> CreateFrames() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sSurface != VK_NULL_HANDLE);

  VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
  surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  surfaceInfo.surface = sSurface;

  VkSurfaceCapabilities2KHR surfaceCapabilities = {};
  surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

  if (auto result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(
        sPhysicalDevice, &surfaceInfo, &surfaceCapabilities);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result),
                        "vkGetPhysicalDeviceSurfaceCapabilities2KHR"));
  }

  std::uint32_t imageCount =
    glm::clamp(surfaceCapabilities.surfaceCapabilities.minImageCount + 1,
               surfaceCapabilities.surfaceCapabilities.minImageCount,
               surfaceCapabilities.surfaceCapabilities.maxImageCount);

  sFrames.resize(imageCount);

  VkSemaphoreCreateInfo semaphoreCI = {};
  semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkCommandPoolCreateInfo commandPoolCI = {};
  commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  commandPoolCI.queueFamilyIndex = sQueueFamilyIndex;

  VkCommandBufferAllocateInfo commandBufferAI = {};
  commandBufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAI.commandBufferCount = 1;

  for (auto& frame : sFrames) {
    if (auto result = vkCreateSemaphore(sDevice, &semaphoreCI, nullptr,
                                        &frame.imageAvailable);
        result != VK_SUCCESS) {
      LOG_LEAVE();
      return tl::unexpected(
        std::system_error(vk::make_error_code(result), "vkCreateSemaphore"));
    }

    if (auto result = vkCreateCommandPool(sDevice, &commandPoolCI, nullptr,
                                          &frame.commandPool);
        result != VK_SUCCESS) {
      LOG_LEAVE();
      return tl::unexpected(
        std::system_error(vk::make_error_code(result), "vkCreateCommandPool"));
    }

    commandBufferAI.commandPool = frame.commandPool;

    if (auto result = vkAllocateCommandBuffers(sDevice, &commandBufferAI,
                                               &frame.commandBuffer);
        result != VK_SUCCESS) {
      LOG_LEAVE();
      return tl::unexpected(std::system_error(vk::make_error_code(result),
                                              "vkAllocateCommandBuffers"));
    }

    Ensures(frame.imageAvailable != VK_NULL_HANDLE);
    Ensures(frame.commandPool != VK_NULL_HANDLE);
    Ensures(frame.commandBuffer != VK_NULL_HANDLE);

    NameObject(sDevice, VK_OBJECT_TYPE_SEMAPHORE, frame.imageAvailable,
               "sFrames.imageAvailable");
    NameObject(sDevice, VK_OBJECT_TYPE_COMMAND_POOL, frame.commandPool,
               "sFrames.commandPool");
    NameObject(sDevice, VK_OBJECT_TYPE_COMMAND_BUFFER, frame.commandBuffer,
               "sFrames.commandBuffer");
  }

  LOG_LEAVE();
  return {};
} // CreateFrames

static tl::expected<void, std::system_error> CreateSwapchain() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sSurface != VK_NULL_HANDLE);

  VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
  surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
  surfaceInfo.surface = sSurface;

  VkSurfaceCapabilities2KHR surfaceCapabilities = {};
  surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

  if (auto result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(
        sPhysicalDevice, &surfaceInfo, &surfaceCapabilities);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result),
                        "vkGetPhysicalDeviceSurfaceCapabilities2KHR"));
  }

  int width, height;
  glfwGetFramebufferSize(sWindow, &width, &height);

  sSwapchainExtent.width =
    surfaceCapabilities.surfaceCapabilities.currentExtent.width == UINT32_MAX
      ? glm::clamp(gsl::narrow_cast<std::uint32_t>(width),
                   surfaceCapabilities.surfaceCapabilities.minImageExtent.width,
                   surfaceCapabilities.surfaceCapabilities.maxImageExtent.width)
      : surfaceCapabilities.surfaceCapabilities.currentExtent.width;

  sSwapchainExtent.height =
    surfaceCapabilities.surfaceCapabilities.currentExtent.height == UINT32_MAX
      ? glm::clamp(
          gsl::narrow_cast<std::uint32_t>(height),
          surfaceCapabilities.surfaceCapabilities.minImageExtent.height,
          surfaceCapabilities.surfaceCapabilities.maxImageExtent.height)
      : surfaceCapabilities.surfaceCapabilities.currentExtent.height;

  VkSwapchainCreateInfoKHR swapchainCI = {};
  swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCI.surface = sSurface;
  swapchainCI.minImageCount = gsl::narrow_cast<std::uint32_t>(sFrames.size());
  swapchainCI.imageFormat = sSurfaceColorFormat.format;
  swapchainCI.imageColorSpace = sSurfaceColorFormat.colorSpace;
  swapchainCI.imageExtent = sSwapchainExtent;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.imageUsage =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainCI.preTransform =
    surfaceCapabilities.surfaceCapabilities.currentTransform;
  swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCI.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swapchainCI.clipped = VK_TRUE;

  if (auto result =
        vkCreateSwapchainKHR(sDevice, &swapchainCI, nullptr, &sSwapchain);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateSwapchainKHR"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, sSwapchain, "sSwapchain");

  Ensures(sSwapchain != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateSwapchain

static tl::expected<void, std::system_error>
CreateSwapchainImagesAndViews() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sSwapchain != VK_NULL_HANDLE);

  std::uint32_t count;

  if (auto result =
        vkGetSwapchainImagesKHR(sDevice, sSwapchain, &count, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkGetSwapchainImagesKHR"));
  }

  sSwapchainImages.resize(count);

  if (auto result = vkGetSwapchainImagesKHR(sDevice, sSwapchain, &count,
                                            sSwapchainImages.data());
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkGetSwapchainImagesKHR"));
  }

  VkImageViewCreateInfo imageViewCI = {};
  imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCI.format = sSurfaceColorFormat.format;
  imageViewCI.components = {
    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  imageViewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  sSwapchainImageViews.resize(count);
  for (std::size_t i = 0; i < sSwapchainImageViews.size(); ++i) {
    imageViewCI.image = sSwapchainImages[i];

    if (auto result = vkCreateImageView(sDevice, &imageViewCI, nullptr,
                                        &sSwapchainImageViews[i]);
        result != VK_SUCCESS) {
      return tl::unexpected(
        std::system_error(vk::make_error_code(result), "vkCreateImageView"));
    }

    NameObject(sDevice, VK_OBJECT_TYPE_IMAGE_VIEW, sSwapchainImageViews[i],
               "sSwapchainImageViews");
  }

  for (std::size_t i = 0; i < sSwapchainImageViews.size(); ++i) {
    Ensures(sSwapchainImageViews[i] != VK_NULL_HANDLE);
  }

  Ensures(!sSwapchainImages.empty());

  LOG_LEAVE();
  return {};
} // CreateSwapchainImagesAndViews

static tl::expected<void, std::system_error> CreateFramebuffers() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sRenderPass != VK_NULL_HANDLE);
  Expects(!sSwapchainImageViews.empty());
  Expects(sSwapchainImageViews.size() == sFrames.size());

  VkFramebufferCreateInfo framebufferCI = {};
  framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCI.renderPass = sRenderPass;
  framebufferCI.attachmentCount = 1;
  framebufferCI.width = sSwapchainExtent.width;
  framebufferCI.height = sSwapchainExtent.height;
  framebufferCI.layers = 1;

  for (std::size_t i = 0; i < sSwapchainImageViews.size(); ++i) {
    framebufferCI.pAttachments = &sSwapchainImageViews[i];

    if (auto result = vkCreateFramebuffer(sDevice, &framebufferCI, nullptr,
                                          &sFrames[i].framebuffer);
        result != VK_SUCCESS) {
      LOG_LEAVE();
      return tl::unexpected(
        std::system_error(vk::make_error_code(result), "vkCreateFramebuffer"));
    }
  }

  for (auto&& frame : sFrames) {
    Ensures(frame.framebuffer != VK_NULL_HANDLE);
    NameObject(sDevice, VK_OBJECT_TYPE_FRAMEBUFFER, frame.framebuffer,
               "sFrames.framebuffer");
  }

  LOG_LEAVE();
  return {};
} // CreateFramebuffers

static tl::expected<void, std::system_error> CreateDescriptorPool() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  std::array<VkDescriptorPoolSize, 4> poolSizes = {
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  VkDescriptorPoolCreateInfo descriptorPoolCI = {};
  descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCI.poolSizeCount =
    gsl::narrow_cast<std::uint32_t>(poolSizes.size());
  descriptorPoolCI.pPoolSizes = poolSizes.data();
  descriptorPoolCI.maxSets = 1;

  if (auto result = vkCreateDescriptorPool(sDevice, &descriptorPoolCI, nullptr,
                                           &sDescriptorPool);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateDescriptorPool"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_DESCRIPTOR_POOL, sDescriptorPool,
             "sDescriptorPool");

  Ensures(sDescriptorPool != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateDescriptorPool

static tl::expected<void, std::system_error> CreateQueryPool() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  VkQueryPoolCreateInfo queryPoolCI = {};
  queryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
  queryPoolCI.queryCount = 32;

  if (auto result =
        vkCreateQueryPool(sDevice, &queryPoolCI, nullptr, &sQueryPool);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateQueryPool"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_QUERY_POOL, sQueryPool, "sQueryPool");

  Ensures(sQueryPool != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateQueryPool

static tl::expected<void, std::system_error>
CreateDescriptorSetLayout() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  VkDescriptorSetLayoutBinding accelerationStructureLB = {};
  accelerationStructureLB.binding = 0;
  accelerationStructureLB.descriptorCount = 1;
  accelerationStructureLB.descriptorType =
    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
  accelerationStructureLB.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

  VkDescriptorSetLayoutBinding outputImageLB = {};
  outputImageLB.binding = 1;
  outputImageLB.descriptorCount = 1;
  outputImageLB.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  outputImageLB.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

  VkDescriptorSetLayoutBinding uniformBufferLB = {};
  uniformBufferLB.binding = 2;
  uniformBufferLB.descriptorCount = 1;
  uniformBufferLB.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uniformBufferLB.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NV;

  VkDescriptorSetLayoutBinding spheresBufferLB = {};
  spheresBufferLB.binding = 3;
  spheresBufferLB.descriptorCount = 1;
  spheresBufferLB.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  spheresBufferLB.stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_NV;

  std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
    accelerationStructureLB, outputImageLB, uniformBufferLB, spheresBufferLB};

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = {};
  descriptorSetLayoutCI.sType =
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCI.bindingCount =
    gsl::narrow_cast<std::uint32_t>(bindings.size());
  descriptorSetLayoutCI.pBindings = bindings.data();

  if (auto result = vkCreateDescriptorSetLayout(sDevice, &descriptorSetLayoutCI,
                                                nullptr, &sDescriptorSetLayout);
      result != VK_SUCCESS) {
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateDescriptorSetLayout"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
             sDescriptorSetLayout, "sDescriptorSetLayout");

  Ensures(sDescriptorSetLayout != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateDescriptorSetLayout

[[nodiscard]] static tl::expected<VkShaderModule, std::system_error>
CreateShaderModule(gsl::czstring filename) noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);

  std::unique_ptr<std::FILE, decltype(&std::fclose)> fh(
    std::fopen(filename, "rb"), std::fclose);

  if (!fh) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(std::make_error_code(std::errc::io_error), filename));
  }

  std::fseek(fh.get(), 0L, SEEK_END);
  std::vector<std::byte> code(std::ftell(fh.get()));
  std::fseek(fh.get(), 0L, SEEK_SET);

  std::size_t nread =
    std::fread(code.data(), sizeof(std::byte), std::size(code), fh.get());

  if (std::ferror(fh.get()) && !std::feof(fh.get()) ||
      nread != std::size(code)) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(std::make_error_code(std::errc::io_error), filename));
  }

  VkShaderModuleCreateInfo shaderModuleCI = {};
  shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderModuleCI.codeSize = code.size();
  shaderModuleCI.pCode = reinterpret_cast<std::uint32_t*>(code.data());

  VkShaderModule shaderModule;
  if (auto result =
        vkCreateShaderModule(sDevice, &shaderModuleCI, nullptr, &shaderModule);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateShaderModule"));
  }

  LOG_LEAVE();
  return shaderModule;
} // CreateShaderModule

static tl::expected<void, std::system_error> CreatePipeline() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sDescriptorSetLayout != VK_NULL_HANDLE);

  VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
  pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCI.setLayoutCount = 1;
  pipelineLayoutCI.pSetLayouts = &sDescriptorSetLayout;

  if (auto result = vkCreatePipelineLayout(sDevice, &pipelineLayoutCI, nullptr,
                                           &sPipelineLayout);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vKCreatePipelineLayout"));
  }

  auto rgenSM = CreateShaderModule("01_sphere_rgen.spv");
  if (!rgenSM) {
    LOG_LEAVE();
    return tl::unexpected(rgenSM.error());
  }

  auto rmissSM = CreateShaderModule("01_sphere_rmiss.spv");
  if (!rmissSM) {
    LOG_LEAVE();
    return tl::unexpected(rmissSM.error());
  }

  auto rchitSM = CreateShaderModule("01_sphere_rchit.spv");
  if (!rchitSM) {
    LOG_LEAVE();
    return tl::unexpected(rchitSM.error());
  }

  auto rintSM = CreateShaderModule("01_sphere_rint.spv");
  if (!rintSM) {
    LOG_LEAVE();
    return tl::unexpected(rintSM.error());
  }

  std::array<VkPipelineShaderStageCreateInfo, 4> stages = {
    VkPipelineShaderStageCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_RAYGEN_BIT_NV, *rgenSM, "main", nullptr},
    VkPipelineShaderStageCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_MISS_BIT_NV, *rmissSM, "main", nullptr},
    VkPipelineShaderStageCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, *rchitSM, "main", nullptr},
    VkPipelineShaderStageCreateInfo{
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
      VK_SHADER_STAGE_INTERSECTION_BIT_NV, *rintSM, "main", nullptr}};

  std::array<VkRayTracingShaderGroupCreateInfoNV, 3> groups = {
    VkRayTracingShaderGroupCreateInfoNV{
      VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr,
      VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 0, VK_SHADER_UNUSED_NV,
      VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV},
    VkRayTracingShaderGroupCreateInfoNV{
      VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr,
      VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV, 1, VK_SHADER_UNUSED_NV,
      VK_SHADER_UNUSED_NV, VK_SHADER_UNUSED_NV},
    VkRayTracingShaderGroupCreateInfoNV{
      VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV, nullptr,
      VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV,
      VK_SHADER_UNUSED_NV, 2, VK_SHADER_UNUSED_NV, 3}};

  VkRayTracingPipelineCreateInfoNV pipelineCI = {};
  pipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
  pipelineCI.stageCount = gsl::narrow_cast<std::uint32_t>(stages.size());
  pipelineCI.pStages = stages.data();
  pipelineCI.groupCount = gsl::narrow_cast<std::uint32_t>(groups.size());
  pipelineCI.pGroups = groups.data();
  pipelineCI.maxRecursionDepth = 1;
  pipelineCI.layout = sPipelineLayout;

  if (auto result = vkCreateRayTracingPipelinesNV(
        sDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &sPipeline);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateRayTracingPipelinesNV"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, sPipelineLayout,
             "sPipelineLayout");
  NameObject(sDevice, VK_OBJECT_TYPE_PIPELINE, sPipeline, "sPipeline");

  Ensures(sPipelineLayout != VK_NULL_HANDLE);
  Ensures(sPipeline != VK_NULL_HANDLE);

  vkDestroyShaderModule(sDevice, *rgenSM, nullptr);
  vkDestroyShaderModule(sDevice, *rmissSM, nullptr);
  vkDestroyShaderModule(sDevice, *rchitSM, nullptr);
  vkDestroyShaderModule(sDevice, *rintSM, nullptr);

  LOG_LEAVE();
  return {};
} // CreatePipeline

static tl::expected<void, std::system_error> CreateUniformBuffer() noexcept {
  LOG_ENTER();
  Expects(sAllocator != VK_NULL_HANDLE);

  char objectName[] = "sUniformBuffer";

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size = sizeof(UniformBuffer);
  bufferCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  allocationCI.pUserData = objectName;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &sUniformBuffer,
                        &sUniformBufferAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  Ensures(sUniformBuffer != VK_NULL_HANDLE);
  Ensures(sUniformBufferAllocation != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateUniformBuffer

static tl::expected<void, std::system_error> CreateOutputImage() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sAllocator != VK_NULL_HANDLE);
  Expects(sSwapchain != VK_NULL_HANDLE); // Ensures sSwapchainExtent is valid

  char objectName[] = "sOutputImage";

  VkImageCreateInfo imageCI = {};
  imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCI.imageType = VK_IMAGE_TYPE_2D;
  imageCI.format = sSurfaceColorFormat.format;
  imageCI.extent = {sSwapchainExtent.width, sSwapchainExtent.height, 1};
  imageCI.mipLevels = 1;
  imageCI.arrayLayers = 1;
  imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocationCI.pUserData = objectName;

  if (auto result =
        vmaCreateImage(sAllocator, &imageCI, &allocationCI, &sOutputImage,
                       &sOutputImageAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateImage"));
  }

  VkImageViewCreateInfo imageViewCI = {};
  imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCI.image = sOutputImage;
  imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCI.format = sSurfaceColorFormat.format;
  imageViewCI.components = {
    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
    VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
  imageViewCI.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  if (auto result =
        vkCreateImageView(sDevice, &imageViewCI, nullptr, &sOutputImageView);
      result != VK_NULL_HANDLE) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateImageView"));
  }

  Ensures(sOutputImage != VK_NULL_HANDLE);
  Ensures(sOutputImageAllocation != VK_NULL_HANDLE);
  Ensures(sOutputImageView != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateOutputImage

static tl::expected<void, std::system_error>
CreateSpheresBuffer() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sAllocator != VK_NULL_HANDLE);

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size = sSpheres.size() * sizeof(Sphere);
  bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  VkBuffer stagingBuffer;
  VmaAllocation stagingAllocation;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &stagingBuffer,
                        &stagingAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  Sphere* pStaging;
  if (auto ptr = MapMemory<Sphere*>(sAllocator, stagingAllocation)) {
    pStaging = *ptr;
  } else {
    LOG_LEAVE();
    return tl::unexpected(ptr.error());
  }

  std::memcpy(pStaging, sSpheres.data(), sSpheres.size() * sizeof(Sphere));
  vmaUnmapMemory(sAllocator, stagingAllocation);

  char objectName[] = "sSpheresBuffer";

  bufferCI.usage =
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocationCI.pUserData = objectName;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &sSpheresBuffer,
                        &sSpheresBufferAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  auto commandBuffer = BeginOneTimeSubmit();
  if (!commandBuffer) {
    LOG_LEAVE();
    return tl::unexpected(commandBuffer.error());
  }

  VkBufferCopy region = {};
  region.srcOffset = 0;
  region.dstOffset = 0;
  region.size = bufferCI.size;

  vkCmdCopyBuffer(*commandBuffer, stagingBuffer, sSpheresBuffer, 1, &region);

  if (auto result = EndOneTimeSubmit(*commandBuffer); !result) {
    LOG_LEAVE();
    return tl::unexpected(result.error());
  }

  LOG_LEAVE();
  return {};
} // CreateSpheresBuffer

static tl::expected<void, std::system_error>
CreateBottomLevelAccelerationStructure() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sAllocator != VK_NULL_HANDLE);

  VkGeometryTrianglesNV triangles = {};
  triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;

  VkGeometryAABBNV spheres = {};
  spheres.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
  spheres.aabbData = sSpheresBuffer;
  spheres.numAABBs = gsl::narrow_cast<std::uint32_t>(sSpheres.size());
  spheres.stride = sizeof(Sphere);
  spheres.offset = offsetof(Sphere, aabbMin);

  VkGeometryNV geometry = {};
  geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
  geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
  geometry.geometry.triangles = triangles;
  geometry.geometry.aabbs = spheres;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

  VkAccelerationStructureCreateInfoNV accelerationStructureCI = {};
  accelerationStructureCI.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
  accelerationStructureCI.compactedSize = 0;
  accelerationStructureCI.info.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
  accelerationStructureCI.info.type =
    VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
  accelerationStructureCI.info.flags = 0;
  accelerationStructureCI.info.instanceCount = 0;
  accelerationStructureCI.info.geometryCount = 1;
  accelerationStructureCI.info.pGeometries = &geometry;

  if (auto result = vkCreateAccelerationStructureNV(
        sDevice, &accelerationStructureCI, nullptr,
        &sBottomLevelAccelerationStructure);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateAccelerationStructureNV"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV,
             sBottomLevelAccelerationStructure,
             "sBottomLevelAccelerationStructure");

  VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = {};
  memReqInfo.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
  memReqInfo.accelerationStructure = sBottomLevelAccelerationStructure;
  memReqInfo.type =
    VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

  VkMemoryRequirements2 memReq = {};
  memReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

  vkGetAccelerationStructureMemoryRequirementsNV(sDevice, &memReqInfo, &memReq);

  char objectName[] = "sBottomLevelAccelerationStructureAllocation";

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_UNKNOWN;
  allocationCI.pUserData = objectName;

  if (auto result = vmaAllocateMemory(
        sAllocator, &memReq.memoryRequirements, &allocationCI,
        &sBottomLevelAccelerationStructureAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaAllocateMemory"));
  }

  VmaAllocationInfo info;
  vmaGetAllocationInfo(sAllocator, sBottomLevelAccelerationStructureAllocation,
                       &info);

  VkBindAccelerationStructureMemoryInfoNV bindInfo = {};
  bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
  bindInfo.accelerationStructure = sBottomLevelAccelerationStructure;
  bindInfo.memory = info.deviceMemory;
  bindInfo.memoryOffset = info.offset;

  if (auto result = vkBindAccelerationStructureMemoryNV(sDevice, 1, &bindInfo);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkBindAccelerationStructureMemoryNV"));
  }

  memReqInfo.type =
    VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

  VkMemoryRequirements2 bottomLevelMemReq = {};
  bottomLevelMemReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

  memReqInfo.accelerationStructure = sBottomLevelAccelerationStructure;
  vkGetAccelerationStructureMemoryRequirementsNV(sDevice, &memReqInfo,
                                                 &bottomLevelMemReq);

  VkBuffer scratchBuffer;
  VmaAllocation scratchAllocation;

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size = bottomLevelMemReq.memoryRequirements.size;
  bufferCI.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;

  allocationCI = {};
  allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &scratchBuffer,
                        &scratchAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  auto commandBuffer = BeginOneTimeSubmit();
  if (!commandBuffer) {
    LOG_LEAVE();
    return tl::unexpected(commandBuffer.error());
  }

  vkCmdBuildAccelerationStructureNV(
    *commandBuffer, &accelerationStructureCI.info,
    VK_NULL_HANDLE /* instanceData */, 0 /* instanceOffset */,
    VK_FALSE /* update */, sBottomLevelAccelerationStructure /* dst */,
    VK_NULL_HANDLE /* src */, scratchBuffer, 0 /* scratchOffset */);

  if (auto result = EndOneTimeSubmit(*commandBuffer); !result) {
    LOG_LEAVE();
    return tl::unexpected(result.error());
  }

  Ensures(sBottomLevelAccelerationStructure != VK_NULL_HANDLE);
  Ensures(sBottomLevelAccelerationStructureAllocation != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateBottomLevelAccelerationStructure

static tl::expected<void, std::system_error>
CreateTopLevelAccelerationStructure() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sAllocator != VK_NULL_HANDLE);

  VkAccelerationStructureCreateInfoNV accelerationStructureCI = {};
  accelerationStructureCI.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
  accelerationStructureCI.compactedSize = 0;
  accelerationStructureCI.info.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
  accelerationStructureCI.info.type =
    VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
  accelerationStructureCI.info.flags = 0;
  accelerationStructureCI.info.instanceCount = 1;
  accelerationStructureCI.info.geometryCount = 0;

  if (auto result = vkCreateAccelerationStructureNV(
        sDevice, &accelerationStructureCI, nullptr,
        &sTopLevelAccelerationStructure);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkCreateAccelerationStructureNV"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV,
             sTopLevelAccelerationStructure, "sTopLevelAccelerationStructure");

  VkAccelerationStructureMemoryRequirementsInfoNV memReqInfo = {};
  memReqInfo.sType =
    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
  memReqInfo.accelerationStructure = sTopLevelAccelerationStructure;
  memReqInfo.type =
    VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

  VkMemoryRequirements2 memReq = {};
  memReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

  vkGetAccelerationStructureMemoryRequirementsNV(sDevice, &memReqInfo, &memReq);

  char objectName[] = "sTopLevelAccelerationStructureAllocation";

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_UNKNOWN;
  allocationCI.pUserData = objectName;

  if (auto result =
        vmaAllocateMemory(sAllocator, &memReq.memoryRequirements, &allocationCI,
                          &sTopLevelAccelerationStructureAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaAllocateMemory"));
  }

  VmaAllocationInfo info;
  vmaGetAllocationInfo(sAllocator, sTopLevelAccelerationStructureAllocation,
                       &info);

  VkBindAccelerationStructureMemoryInfoNV bindInfo = {};
  bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
  bindInfo.accelerationStructure = sTopLevelAccelerationStructure;
  bindInfo.memory = info.deviceMemory;
  bindInfo.memoryOffset = info.offset;

  if (auto result = vkBindAccelerationStructureMemoryNV(sDevice, 1, &bindInfo);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkBindAccelerationStructureMemoryNV"));
  }

  std::uint64_t bottomLevelHandle;
  if (auto result = vkGetAccelerationStructureHandleNV(
        sDevice, sBottomLevelAccelerationStructure, sizeof(bottomLevelHandle),
        &bottomLevelHandle);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "vkGetAccelerationStructureHandleNV"));
  }

  struct VkGeometryInstanceNV {
    float transform[12];
    std::uint32_t instanceCustomIndex : 24;
    std::uint32_t mask : 8;
    std::uint32_t instanceOffset : 24;
    std::uint32_t flags : 8;
    std::uint64_t accelerationStructureHandle;
  };

  VkBuffer instanceBuffer;
  VmaAllocation instanceAllocation;

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size = sizeof(VkGeometryInstanceNV);
  bufferCI.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;

  allocationCI = {};
  allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &instanceBuffer,
                        &instanceAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  VkGeometryInstanceNV* instanceData;
  if (auto ptr =
        MapMemory<VkGeometryInstanceNV*>(sAllocator, instanceAllocation)) {
    instanceData = *ptr;
  } else {
    LOG_LEAVE();
    return tl::unexpected(ptr.error());
  }

  glm::mat4 transform = glm::mat4(1.f);

  std::memcpy(instanceData->transform, glm::value_ptr(transform),
              sizeof(transform));
  instanceData->instanceCustomIndex = 0;
  instanceData->mask = 0xF;
  instanceData->instanceOffset = 0;
  instanceData->accelerationStructureHandle = bottomLevelHandle;

  vmaUnmapMemory(sAllocator, instanceAllocation);

  memReqInfo.type =
    VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;

  VkMemoryRequirements2 topLevelMemReq = {};
  topLevelMemReq.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

  memReqInfo.accelerationStructure = sTopLevelAccelerationStructure;
  vkGetAccelerationStructureMemoryRequirementsNV(sDevice, &memReqInfo,
                                                 &topLevelMemReq);

  VkBuffer scratchBuffer;
  VmaAllocation scratchAllocation;

  bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size = topLevelMemReq.memoryRequirements.size;
  bufferCI.usage = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV;

  allocationCI = {};
  allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &scratchBuffer,
                        &scratchAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  auto commandBuffer = BeginOneTimeSubmit();
  if (!commandBuffer) {
    LOG_LEAVE();
    return tl::unexpected(commandBuffer.error());
  }

  vkCmdBuildAccelerationStructureNV(
    *commandBuffer, &accelerationStructureCI.info,
    instanceBuffer /* instanceData */, 0 /* instanceOffset */,
    VK_FALSE /* update */, sTopLevelAccelerationStructure /* dst */,
    VK_NULL_HANDLE /* src */, scratchBuffer, 0 /* scratchOffset */);

  if (auto result = EndOneTimeSubmit(*commandBuffer); !result) {
    LOG_LEAVE();
    return tl::unexpected(result.error());
  }

  Ensures(sTopLevelAccelerationStructure != VK_NULL_HANDLE);
  Ensures(sTopLevelAccelerationStructureAllocation != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateTopLevelAccelerationStructure

static tl::expected<void, std::system_error> CreateShaderBindingTable() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sPipeline != VK_NULL_HANDLE);
  Expects(sAllocator != VK_NULL_HANDLE);
  Expects(sBottomLevelAccelerationStructure != VK_NULL_HANDLE);
  Expects(sTopLevelAccelerationStructure != VK_NULL_HANDLE);

  VkPhysicalDeviceRayTracingPropertiesNV rtProps = {};
  rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;

  VkPhysicalDeviceProperties2 props = {};
  props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props.pNext = &rtProps;

  vkGetPhysicalDeviceProperties2(sPhysicalDevice, &props);
  sShaderGroupHandleSize = rtProps.shaderGroupHandleSize;
#ifndef NDEBUG
  std::fprintf(stderr, "sShaderGroupHandleSize: %d\n", sShaderGroupHandleSize);
#endif

  sShaderBindingTableGenerator.AddRayGen(0);
  sShaderBindingTableGenerator.AddMiss(1);
  sShaderBindingTableGenerator.AddHitGroup(2);

  VkBufferCreateInfo bufferCI = {};
  bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCI.size =
    sShaderBindingTableGenerator.ComputeSize(sShaderGroupHandleSize);
  bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo allocationCI = {};
  allocationCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

  VkBuffer stagingBuffer;
  VmaAllocation stagingAllocation;

  if (auto result =
        vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI, &stagingBuffer,
                        &stagingAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  std::byte* pStaging;
  if (auto ptr = MapMemory<std::byte*>(sAllocator, stagingAllocation)) {
    pStaging = *ptr;
  } else {
    LOG_LEAVE();
    return tl::unexpected(ptr.error());
  }

  VmaAllocationInfo info;
  vmaGetAllocationInfo(sAllocator, stagingAllocation, &info);

  if (auto result =
        sShaderBindingTableGenerator.Generate(sDevice, sPipeline, pStaging);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(
      vk::make_error_code(result), "ShaderBindingTableGenerator::Generate"));
  }

  vmaUnmapMemory(sAllocator, stagingAllocation);

  char objectName[] = "sShaderBindingTable";

  bufferCI.usage =
    VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
#ifndef NDEBUG
  std::fprintf(stderr, "sShaderBindingTable size: %zu\n", bufferCI.size);
#endif

  allocationCI.flags = VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
  allocationCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocationCI.pUserData = objectName;

  if (auto result = vmaCreateBuffer(sAllocator, &bufferCI, &allocationCI,
                                    &sShaderBindingTable,
                                    &sShaderBindingTableAllocation, nullptr);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vmaCreateBuffer"));
  }

  auto commandBuffer = BeginOneTimeSubmit();
  if (!commandBuffer) {
    LOG_LEAVE();
    return tl::unexpected(commandBuffer.error());
  }

  VkBufferCopy region = {};
  region.srcOffset = 0;
  region.dstOffset = 0;
  region.size = bufferCI.size;

  vkCmdCopyBuffer(*commandBuffer, stagingBuffer, sShaderBindingTable, 1,
                  &region);

  if (auto result = EndOneTimeSubmit(*commandBuffer); !result) {
    LOG_LEAVE();
    return tl::unexpected(result.error());
  }

  vmaDestroyBuffer(sAllocator, stagingBuffer, stagingAllocation);

  Ensures(sShaderBindingTable != VK_NULL_HANDLE);
  Ensures(sShaderBindingTableAllocation != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateShaderBindingTable

static tl::expected<void, std::system_error> CreateDescriptorSets() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(sDescriptorPool != VK_NULL_HANDLE);
  Expects(sBottomLevelAccelerationStructure != VK_NULL_HANDLE);
  Expects(sTopLevelAccelerationStructure != VK_NULL_HANDLE);
  Expects(sUniformBuffer != VK_NULL_HANDLE);
  Expects(sOutputImage != VK_NULL_HANDLE);
  Expects(sSpheresBuffer != VK_NULL_HANDLE);

  sDescriptorSets.resize(1);

  VkDescriptorSetAllocateInfo descriptorSetAI = {};
  descriptorSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAI.descriptorPool = sDescriptorPool;
  descriptorSetAI.descriptorSetCount =
    gsl::narrow_cast<std::uint32_t>(sDescriptorSets.size());
  descriptorSetAI.pSetLayouts = &sDescriptorSetLayout;

  if (auto result = vkAllocateDescriptorSets(sDevice, &descriptorSetAI,
                                             sDescriptorSets.data());
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(std::system_error(vk::make_error_code(result),
                                            "vkAllocateDescriptorSets"));
  }

  VkWriteDescriptorSetAccelerationStructureNV accelerationStructureInfo = {};
  accelerationStructureInfo.sType =
    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
  accelerationStructureInfo.accelerationStructureCount = 1;
  accelerationStructureInfo.pAccelerationStructures =
    &sTopLevelAccelerationStructure;

  VkDescriptorImageInfo outputImageInfo = {};
  outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  outputImageInfo.imageView = sOutputImageView;

  VkDescriptorBufferInfo uniformBufferInfo = {};
  uniformBufferInfo.buffer = sUniformBuffer;
  uniformBufferInfo.offset = 0;
  uniformBufferInfo.range = sizeof(UniformBuffer);

  VkDescriptorBufferInfo spheresBufferInfo = {};
  spheresBufferInfo.buffer = sSpheresBuffer;
  spheresBufferInfo.offset = 0;
  spheresBufferInfo.range = sizeof(Sphere) * sSpheres.size();

  std::array<VkWriteDescriptorSet, 4> writeDescriptorSets;

  writeDescriptorSets[0] = {};
  writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets[0].pNext = &accelerationStructureInfo;
  writeDescriptorSets[0].dstSet = sDescriptorSets[0];
  writeDescriptorSets[0].dstBinding = 0;
  writeDescriptorSets[0].descriptorCount = 1;
  writeDescriptorSets[0].descriptorType =
    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

  writeDescriptorSets[1] = {};
  writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets[1].dstSet = sDescriptorSets[0];
  writeDescriptorSets[1].dstBinding = 1;
  writeDescriptorSets[1].descriptorCount = 1;
  writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  writeDescriptorSets[1].pImageInfo = &outputImageInfo;

  writeDescriptorSets[2] = {};
  writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets[2].dstSet = sDescriptorSets[0];
  writeDescriptorSets[2].dstBinding = 2;
  writeDescriptorSets[2].descriptorCount = 1;
  writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSets[2].pBufferInfo = &uniformBufferInfo;

  writeDescriptorSets[3] = {};
  writeDescriptorSets[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSets[3].dstSet = sDescriptorSets[0];
  writeDescriptorSets[3].dstBinding = 3;
  writeDescriptorSets[3].descriptorCount = 1;
  writeDescriptorSets[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writeDescriptorSets[3].pBufferInfo = &spheresBufferInfo;

  vkUpdateDescriptorSets(
    sDevice, gsl::narrow_cast<std::uint32_t>(writeDescriptorSets.size()),
    writeDescriptorSets.data(), 0, nullptr);

  Ensures(!sDescriptorSets.empty());

  LOG_LEAVE();
  return {};
} // CreateDescriptorSets

static tl::expected<void, std::system_error> CreateSyncObjects() noexcept {
  LOG_ENTER();
  Expects(sDevice != VK_NULL_HANDLE);
  Expects(!sFrames.empty());

  VkFenceCreateInfo fenceCI = {};
  fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  sFramesComplete.resize(sFrames.size());
  for (std::size_t i = 0; i < sFramesComplete.size(); ++i) {
    if (auto result =
          vkCreateFence(sDevice, &fenceCI, nullptr, &sFramesComplete[i]);
        result != VK_SUCCESS) {
      LOG_LEAVE();
      return tl::unexpected(
        std::system_error(vk::make_error_code(result), "vkCreateFence"));
    }

    NameObject(sDevice, VK_OBJECT_TYPE_FENCE, sFramesComplete[i],
               "sFramesComplete");
  }

  VkSemaphoreCreateInfo semaphoreCI = {};
  semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  if (auto result =
        vkCreateSemaphore(sDevice, &semaphoreCI, nullptr, &sRenderFinished);
      result != VK_SUCCESS) {
    LOG_LEAVE();
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkCreateSemaphore"));
  }

  NameObject(sDevice, VK_OBJECT_TYPE_SEMAPHORE, sRenderFinished,
             "sRenderFinished");

  Ensures(!sFramesComplete.empty());
  Ensures(sRenderFinished != VK_NULL_HANDLE);

  LOG_LEAVE();
  return {};
} // CreateSyncObjects

static tl::expected<void, std::system_error> RecreateSwapchain() noexcept {
  int width = 0, height = 0;
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(sWindow, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(sDevice);

  // TODO: Clean up swapchain, swapchain image views, framebuffers

  // clang-format off
  auto result = CreateSwapchain()
    .and_then(CreateSwapchainImagesAndViews)
    .and_then(CreateFramebuffers)
    .and_then(CreateOutputImage)
    ;
  // clang-format on

  sCamera.aspectRatio(static_cast<float>(sSwapchainExtent.width) /
                      static_cast<float>(sSwapchainExtent.height));
  return result;
} // RecreateSwapchain

static tl::expected<void, std::system_error> Draw() noexcept {
  VkFence frameComplete = sFramesComplete[sCurrentFrame];

  if (auto result =
        vkWaitForFences(sDevice, 1, &frameComplete, VK_TRUE, UINT64_MAX);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkWaitForFences"));
  }

  if (auto result = vkResetFences(sDevice, 1, &frameComplete);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkResetFences"));
  }

  auto&& frame = sFrames[sCurrentFrame];
  VkSemaphore submitWaitSemaphore = frame.imageAvailable;

  VkAcquireNextImageInfoKHR nextInfo = {};
  nextInfo.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
  nextInfo.swapchain = sSwapchain;
  nextInfo.timeout = UINT64_MAX;
  nextInfo.semaphore = frame.imageAvailable;

  VkResult result = vkAcquireNextImage2KHR(sDevice, &nextInfo, &sCurrentFrame);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    if (auto recreated = RecreateSwapchain(); !recreated) {
      return tl::unexpected(recreated.error());
    }
    result = vkAcquireNextImage2KHR(sDevice, &nextInfo, &sCurrentFrame);
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkAcquireNextImage2KHR"));
  }

  if (auto result = vkResetCommandPool(sDevice, frame.commandPool, 0);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkResetCommandPool"));
  }

  frame = sFrames[sCurrentFrame];

  UniformBuffer* uniformBufferData;
  if (auto ptr =
        MapMemory<UniformBuffer*>(sAllocator, sUniformBufferAllocation)) {
    uniformBufferData = *ptr;
  } else {
    return tl::unexpected(ptr.error());
  }

  uniformBufferData->Eye = glm::vec4(sCamera.eye(), 1.f);
  uniformBufferData->U = glm::vec4(sCamera.u(), 0.f);
  uniformBufferData->V = glm::vec4(sCamera.v(), 0.f);
  uniformBufferData->W = glm::vec4(sCamera.w(), 0.f);

  vmaUnmapMemory(sAllocator, sUniformBufferAllocation);

  VkCommandBufferBeginInfo commandBufferBI = {};
  commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBI.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  vkBeginCommandBuffer(frame.commandBuffer, &commandBufferBI);

  vkCmdResetQueryPool(frame.commandBuffer, sQueryPool, 0, 32);
  vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      sQueryPool, 0);

  VkImageSubresourceRange sr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  VkImageMemoryBarrier readyBarrier = {};
  readyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  readyBarrier.srcAccessMask = 0;
  readyBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  readyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  readyBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  readyBarrier.srcQueueFamilyIndex = readyBarrier.dstQueueFamilyIndex =
    VK_QUEUE_FAMILY_IGNORED;
  readyBarrier.image = sOutputImage;
  readyBarrier.subresourceRange = sr;

  vkCmdPipelineBarrier(frame.commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &readyBarrier);

  vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
                    sPipeline);
  vkCmdBindDescriptorSets(
    frame.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, sPipelineLayout,
    0, gsl::narrow_cast<std::uint32_t>(sDescriptorSets.size()),
    sDescriptorSets.data(), 0, nullptr);

  vkCmdWriteTimestamp(frame.commandBuffer,
                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, sQueryPool,
                      2);

  vkCmdTraceRaysNV(
    frame.commandBuffer,
    sShaderBindingTable,                       // raygenShaderBindingTableBuffer
    0,                                         // raygenShaderBindingOffset
    sShaderBindingTable,                       // missShaderBindingTableBuffer
    sShaderBindingTableGenerator.MissOffset(), // missShaderBindingOffset
    sShaderBindingTableGenerator.MissStride(), // missShaderBindingStride
    sShaderBindingTable,                       // hitShaderBindingTableBuffer
    sShaderBindingTableGenerator.HitGroupOffset(), // hitShaderBindingOffset
    sShaderBindingTableGenerator.HitGroupStride(), // hitShaderBindingStride
    VK_NULL_HANDLE,          // callableShaderBindingTableBuffer
    0,                       // callableShaderBindingOffset
    0,                       // callableShaderBindingStride
    sSwapchainExtent.width,  // width
    sSwapchainExtent.height, // height
    1                        // depth
  );

  vkCmdWriteTimestamp(frame.commandBuffer,
                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV, sQueryPool,
                      3);

  VkImageMemoryBarrier tracedBarrier = {};
  tracedBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  tracedBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  tracedBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  tracedBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  tracedBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  tracedBarrier.srcQueueFamilyIndex = tracedBarrier.dstQueueFamilyIndex =
    VK_QUEUE_FAMILY_IGNORED;
  tracedBarrier.image = sOutputImage;
  tracedBarrier.subresourceRange = sr;

  vkCmdPipelineBarrier(frame.commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &tracedBarrier);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
    VK_QUEUE_FAMILY_IGNORED;
  barrier.image = sSwapchainImages[sCurrentFrame];
  barrier.subresourceRange = sr;

  vkCmdPipelineBarrier(frame.commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkImageCopy copy = {};
  copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.srcOffset = {0, 0, 0};
  copy.dstSubresource = copy.srcSubresource;
  copy.dstOffset = {0, 0, 0};
  copy.extent = {sSwapchainExtent.width, sSwapchainExtent.height, 1};

  vkCmdCopyImage(frame.commandBuffer, sOutputImage,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 sSwapchainImages[sCurrentFrame],
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = 0;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  vkCmdPipelineBarrier(frame.commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkCmdWriteTimestamp(frame.commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      sQueryPool, 1);

  vkEndCommandBuffer(frame.commandBuffer);

  VkPipelineStageFlags waitDstStageMask =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &submitWaitSemaphore;
  submitInfo.pWaitDstStageMask = &waitDstStageMask;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &frame.commandBuffer;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &sRenderFinished;

  if (result = vkQueueSubmit(sQueue, 1, &submitInfo, frameComplete);
      result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkQueueSubmit"));
  }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &sRenderFinished;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &sSwapchain;
  presentInfo.pImageIndices = &sCurrentFrame;

  if (result = vkQueuePresentKHR(sQueue, &presentInfo); result != VK_SUCCESS) {
    return tl::unexpected(
      std::system_error(vk::make_error_code(result), "vkQueuePresentKHR"));
  }

  return {};
} // Draw

int main() {
  // clang-format off
  auto result = InitWindow()
    .and_then(InitVulkan)
    .and_then(CreateDebugUtilsMessenger)
    .and_then(ChoosePhysicalDevice)
    .and_then(CreateDevice)
    .and_then(CreateCommandPool)
    .and_then(CreateAllocator)
    .and_then(CreateRenderPass)
    .and_then(CreateSurface)
    .and_then(VerifySurfaceFormat)
    .and_then(CreateFrames)
    .and_then(CreateSwapchain)
    .and_then(CreateSwapchainImagesAndViews)
    .and_then(CreateFramebuffers)
    .and_then(CreateDescriptorPool)
    .and_then(CreateQueryPool)
    .and_then(CreateDescriptorSetLayout)
    .and_then(CreatePipeline)
    .and_then(CreateUniformBuffer)
    .and_then(CreateOutputImage)
    .and_then(CreateSpheresBuffer)
    .and_then(CreateBottomLevelAccelerationStructure)
    .and_then(CreateTopLevelAccelerationStructure)
    .and_then(CreateShaderBindingTable)
    .and_then(CreateDescriptorSets)
    .and_then(CreateSyncObjects)
    ;
  // clang-format on

  if (!result) {
    std::fprintf(stderr, "%s\n", result.error().what());
    std::exit(EXIT_FAILURE);
  }

  sCamera.aspectRatio(static_cast<float>(sSwapchainExtent.width) /
                      static_cast<float>(sSwapchainExtent.height));

  std::uint64_t frameCount = 0;
  double now = glfwGetTime(), last = now;

  while (!glfwWindowShouldClose(sWindow)) {
    glfwPollEvents();

    if (glfwGetKey(sWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(sWindow, true);
      continue;
    }

    if (result = Draw(); !result) {
      std::fprintf(stderr, "%s\n", result.error().what());
      std::exit(EXIT_FAILURE);
    }

    frameCount++;
    now = glfwGetTime();

    if (frameCount % 100 == 0) {
      std::array<std::uint64_t, 4> queries;
      if (auto result = vkGetQueryPoolResults(
            sDevice, sQueryPool, 0,
            gsl::narrow_cast<std::uint32_t>(queries.size()),
            queries.size() * sizeof(std::uint64_t), queries.data(),
            sizeof(std::uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
          result != VK_SUCCESS) {
        std::fprintf(stderr, "Cannot get query results: %s\n",
                     vk::to_string(result));
      }

      std::printf("current frame:\n");
      std::printf("  pipe : %2.5g ms\n",
                  (queries[1] - queries[0]) * 1e-06);
      std::printf("  trace: %2.5g ms\n", (queries[3] - queries[2]) * 1e-06);

      double const delta = now - last;
      std::printf("  delta: %2.5g ms\n", delta * 1000.0);
    }

    last = now;
  }
}
