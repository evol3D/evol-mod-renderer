#include <SyncManager/SyncManager.h>
#include <Vulkan_utils.h>

struct {
  vec(VkFence) fences;
  vec(VkSemaphore) semaphores;
} SyncManagerData;

#define DATA(X) SyncManagerData.X

ev_syncmanager_deinitfence(VkFence *fence)
{
  vkDestroyFence(ev_vulkan_getlogicaldevice(), *fence, NULL);
}

ev_syncmanager_deinitsemaphore(VkSemaphore *semaphore)
{
  vkDestroySemaphore(ev_vulkan_getlogicaldevice(), *semaphore, NULL);
}

void ev_syncmanager_init()
{
  DATA(fences) = vec_init(VkFence, 0, ev_syncmanager_deinitfence);
  DATA(semaphores) = vec_init(VkSemaphore, 0, ev_syncmanager_deinitsemaphore);
}

void ev_syncmanager_deinit()
{
  vec_fini(DATA(fences));
  vec_fini(DATA(semaphores));
}

void ev_syncmanager_allocatesemaphores(uint32_t count, VkSemaphore *semaphores)
{
  VkSemaphoreCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };

  for(size_t i = 0; i < count; ++i)
  {
    VK_ASSERT(vkCreateSemaphore(ev_vulkan_getlogicaldevice(), &createInfo, NULL, semaphores+i));
    vec_push(&DATA(semaphores), semaphores+i);
  }
}

void ev_syncmanager_allocatefences(uint32_t count, VkFence *fences)
{
  VkFenceCreateInfo createInfo =
  {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for(size_t i = 0; i < count; ++i)
  {
    VK_ASSERT(vkCreateFence(ev_vulkan_getlogicaldevice(), &createInfo, NULL, fences+i));
    vec_push(&DATA(fences), fences+i);
  }
}
