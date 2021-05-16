#ifndef EVOL_VULKAN_UTILS_HEADER
#define EVOL_VULKAN_UTILS_HEADER

#define VK_ASSERT(fn) EV_DEBUG_BREAK_IF(fn != VK_SUCCESS)
#define ARRAYSIZE(array) (sizeof(array)/sizeof(array[0]))

#endif
