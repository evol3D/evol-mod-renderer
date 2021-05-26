#pragma once

#define VOLK_VERSION   VK_VERSION_1_2
#define VULKAN_VERSION VK_API_VERSION_1_1

#include <volk.h>

#include <vk_mem_alloc.h>
#include <VulkanQueueManager.h>

typedef unsigned int VkApiVersion;

typedef struct
{
  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvImage;

typedef struct
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvBuffer;

int ev_vulkan_init();
int ev_vulkan_deinit();

//swapchain
void ev_vulkan_createswapchain(unsigned int* imageCount, VkExtent2D extent, VkSurfaceKHR* surface, VkSurfaceFormatKHR *surfaceFormat, VkSwapchainKHR oldSwapchain, VkSwapchainKHR* swapchain);
void ev_vulkan_createframebuffer(VkImageView* attachments, unsigned int attachmentCount, VkRenderPass renderPass, VkExtent2D surfaceExtent, VkFramebuffer *framebuffer);
void ev_vulkan_retrieveswapchainimages(VkSwapchainKHR swapchain, unsigned int* imageCount, VkImage* images);
void ev_vulkan_checksurfacecompatibility(VkSurfaceKHR surface);
void ev_vulkan_destroyswapchain(VkSwapchainKHR swapchain);

//memory pools
void ev_vulkan_allocatememorypool(VmaPoolCreateInfo *poolCreateInfo, VmaPool* pool);
void ev_vulkan_allocatebufferinpool(VkBufferCreateInfo *bufferCreateInfo, VmaPool pool, EvBuffer *buffer);
void ev_vulkan_freememorypool(VmaPool pool);
void ev_vulkan_memorydump();

void ev_vulkan_allocateprimarycommandbuffer(QueueType queueType, VkCommandBuffer *cmdBuffer);

//vulkan images
void ev_vulkan_createimage(VkImageCreateInfo *imageCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvImage *image);
void ev_vulkan_createimageview(VkFormat imageFormat, VkImage *image, VkImageView* view);
void ev_vulkan_destroyimage(EvImage *image);

//vulkan buffers
void ev_vulkan_createbuffer(VkBufferCreateInfo *bufferCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvBuffer *buffer);
void ev_vulkan_destroybuffer(EvBuffer *buffer);

//getters
VkInstance ev_vulkan_getinstance();
VkDevice ev_vulkan_getlogicaldevice();
VkPhysicalDevice ev_vulkan_getphysicaldevice();
VkCommandPool ev_vulkan_getcommandpool(QueueType type);
VmaAllocator ev_vulkan_getvmaallocator();
