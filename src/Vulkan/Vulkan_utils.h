#pragma once

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
