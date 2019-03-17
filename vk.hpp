#ifndef VK_HPP_
#define VK_HPP_

#define VK_VERSION_1_0
// clang-format off
#include "flextVk.h"
#include "GLFW/glfw3.h"
// clang-format on

#include <system_error>

namespace vk {

template <class T>
void NameObject(VkDevice device, VkObjectType objectType, T objectHandle,
                char const* objectName) noexcept {
#if 0 // broken in 1.1.101.0 SDK with validation layers enabled
  VkDebugUtilsObjectNameInfoEXT objectNameInfo = {
    VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, objectType,
    reinterpret_cast<std::uint64_t>(objectHandle), objectName};
  vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
#endif
} // NameObject

enum class VulkanResult {
  kSuccess = VK_SUCCESS,
  kNotReady = VK_NOT_READY,
  kTimeout = VK_TIMEOUT,
  kEventSet = VK_EVENT_SET,
  kEventReset = VK_EVENT_RESET,
  kIncomplete = VK_INCOMPLETE,
  kErrorOutOfHostMemory = VK_ERROR_OUT_OF_HOST_MEMORY,
  kErrorOutOfDeviceMemory = VK_ERROR_OUT_OF_DEVICE_MEMORY,
  kErrorInitializationFailed = VK_ERROR_INITIALIZATION_FAILED,
  kErrorDeviceLost = VK_ERROR_DEVICE_LOST,
  kErrorMemoryMapFailed = VK_ERROR_MEMORY_MAP_FAILED,
  kErrorLayerNotPresent = VK_ERROR_LAYER_NOT_PRESENT,
  kErrorExtensionNotPresent = VK_ERROR_EXTENSION_NOT_PRESENT,
  kErrorFeatureNotPresent = VK_ERROR_FEATURE_NOT_PRESENT,
  kErrorIncompatibleDriver = VK_ERROR_INCOMPATIBLE_DRIVER,
  kErrorTooManyObjects = VK_ERROR_TOO_MANY_OBJECTS,
  kErrorFormatNotSupported = VK_ERROR_FORMAT_NOT_SUPPORTED,
  kErrorFragmentedPool = VK_ERROR_FRAGMENTED_POOL,
  kErrorOutOfPoolMemory = VK_ERROR_OUT_OF_POOL_MEMORY,
  kErrorInvalidExternalHandle = VK_ERROR_INVALID_EXTERNAL_HANDLE,
  kErrorSurfaceLostKHR = VK_ERROR_SURFACE_LOST_KHR,
  kErrorNativeWindowInUseKHR = VK_ERROR_NATIVE_WINDOW_IN_USE_KHR,
  kSuboptimalKHR = VK_SUBOPTIMAL_KHR,
  kErrorOutOfDataKHR = VK_ERROR_OUT_OF_DATE_KHR,
}; // enum class VulkanResult
static_assert(VK_SUCCESS == 0);

inline std::string to_string(VulkanResult result) noexcept {
  using namespace std::string_literals;
  switch (result) {
  case VulkanResult::kSuccess: return "success"s;
  case VulkanResult::kNotReady: return "not ready"s;
  case VulkanResult::kTimeout: return "timeout"s;
  case VulkanResult::kEventSet: return "event set"s;
  case VulkanResult::kEventReset: return "event reset"s;
  case VulkanResult::kIncomplete: return "incomplete"s;
  case VulkanResult::kErrorOutOfHostMemory: return "error: out of host memory"s;
  case VulkanResult::kErrorOutOfDeviceMemory:
    return "error: out of device memory"s;
  case VulkanResult::kErrorInitializationFailed:
    return "error: initialization failed"s;
  case VulkanResult::kErrorDeviceLost: return "error: device lost"s;
  case VulkanResult::kErrorMemoryMapFailed: return "error: memory map failed"s;
  case VulkanResult::kErrorLayerNotPresent: return "error: layer not present"s;
  case VulkanResult::kErrorExtensionNotPresent:
    return "error: extension not present"s;
  case VulkanResult::kErrorFeatureNotPresent:
    return "error: feature not present"s;
  case VulkanResult::kErrorIncompatibleDriver:
    return "error: incompatible driver"s;
  case VulkanResult::kErrorTooManyObjects: return "error: too many objects"s;
  case VulkanResult::kErrorFormatNotSupported:
    return "error: format not supported"s;
  case VulkanResult::kErrorFragmentedPool: return "error: fragmented pool"s;
  case VulkanResult::kErrorOutOfPoolMemory: return "error: out of pool memory"s;
  case VulkanResult::kErrorInvalidExternalHandle:
    return "error: invalid external handle"s;
  case VulkanResult::kErrorSurfaceLostKHR: return "error: surface lost"s;
  case VulkanResult::kErrorNativeWindowInUseKHR:
    return "error: native window in use"s;
  case VulkanResult::kSuboptimalKHR: return "suboptimal"s;
  case VulkanResult::kErrorOutOfDataKHR: return "error: out of date"s;
  }
  return "unknown"s;
} // to_string

inline [[nodiscard]] std::string to_string(VkResult result) noexcept {
  return to_string(static_cast<VulkanResult>(result));
}

class VulkanResultCategory : public std::error_category {
public:
  virtual ~VulkanResultCategory() noexcept {}

  virtual const char* name() const noexcept override {
    return "vk::VulkanResult";
  }

  virtual std::string message(int ev) const override {
    return to_string(static_cast<VulkanResult>(ev));
  }
}; // class VulkanResultCategory

inline VulkanResultCategory const gVulkanResultCategory;

inline [[nodiscard]] std::error_category const& GetVulkanResultCategory() {
  return gVulkanResultCategory;
}

inline [[nodiscard]] std::error_code make_error_code(VulkanResult r) noexcept {
  return std::error_code(static_cast<int>(r), GetVulkanResultCategory());
}

inline [[nodiscard]] std::error_code make_error_code(VkResult r) noexcept {
  return std::error_code(static_cast<int>(r), GetVulkanResultCategory());
}

} // namespace vk

#endif // VK_HPP_
