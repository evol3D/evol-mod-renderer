#pragma once

#define VK_ASSERT(fn) EV_DEBUG_BREAK_IF(fn != VK_SUCCESS)
#define ARRAYSIZE(array) (sizeof(array)/sizeof(array[0]))

#define MAX(x1, x2) ( x1 > x2 ? x1:x2)
#define MIN(x1, x2) ( x1 < x2 ? x1:x2)
