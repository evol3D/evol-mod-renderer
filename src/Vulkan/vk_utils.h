#pragma once

#include <volk.h>
#include <stddef.h>
#include <evol/common/ev_macros.h>
#include <evol/common/ev_log.h>

#define VK_ASSERT(fn) do { \
  VkResult __vk_assert_result_internal = fn; \
  if(__vk_assert_result_internal != VK_SUCCESS) { \
    ev_log_error("[VulkanError] `%s` returned error code %d ('%s')", EV_STRINGIZE(fn), __vk_assert_result_internal, VkResultStrings(__vk_assert_result_internal));\
    EV_DEBUG_BREAK_IF(true); \
  } \
} while (0)
#define ARRAYSIZE(array) (sizeof(array)/sizeof(array[0]))

#define MAX(x1, x2) ( x1 > x2 ? x1:x2)
#define MIN(x1, x2) ( x1 < x2 ? x1:x2)

#include <vk_mem_alloc.h>

typedef struct {
  VmaAllocator allocator;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} VmaData;

static inline const char * VkResultStrings(VkResult res) {
  switch(res) {
    case VK_SUCCESS:
      return "VK_SUCCESS";
    case VK_NOT_READY:
      return "VK_NOT_READY";
    case VK_TIMEOUT:
      return "VK_TIMEOUT";
    case VK_EVENT_SET:
      return "VK_EVENT_SET";
    case VK_EVENT_RESET:
      return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
      return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
      return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
      return "VK_ERROR_UNKNOWN";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
      return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
      return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
      return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_EXT:
      return "VK_ERROR_NOT_PERMITTED_EXT";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
      return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
      return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR:
      return "VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR";
    case VK_ERROR_FRAGMENTATION_EXT:
      return "VK_ERROR_FRAGMENTATION_EXT";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR:
      return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS_KHR / VK_ERROR_INVALID_DEVICE_ADDRESS";
    default:
      return NULL;
  }
}

