#include <Vulkan.h>

void ev_syncmanager_init();

void ev_syncmanager_deinit();

void ev_syncmanager_allocatesemaphores(uint32_t count, VkSemaphore *semaphores);

void ev_syncmanager_allocatefences(uint32_t count, VkFence *fences);
