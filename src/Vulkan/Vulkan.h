#pragma once

#define VOLK_VERSION   VK_VERSION_1_2
#define VULKAN_VERSION VK_API_VERSION_1_2

#include <volk.h>
#include <Renderer_types.h>
#include <VulkanQueueManager.h>
#include <Swapchain.h>

typedef unsigned int VkApiVersion;

int ev_vulkan_init();
int ev_vulkan_deinit();

//swapchain
void ev_vulkan_createEvswapchain();
void ev_vulkan_createswapchain(unsigned int* imageCount, VkExtent2D extent, VkSurfaceKHR* surface, VkSurfaceFormatKHR *surfaceFormat, VkSwapchainKHR oldSwapchain, VkSwapchainKHR* swapchain);
void ev_vulkan_createframebuffer(VkImageView* attachments, unsigned int attachmentCount, VkRenderPass renderPass, VkExtent2D surfaceExtent, VkFramebuffer *framebuffer);
void ev_vulkan_retrieveswapchainimages(VkSwapchainKHR swapchain, unsigned int* imageCount, VkImage* images);
void ev_vulkan_checksurfacecompatibility();
void ev_vulkan_destroyswapchain(VkSwapchainKHR swapchain);
void ev_vulkan_recreateSwapChain();

//memory pools
void ev_vulkan_allocatememorypool(VmaPoolCreateInfo *poolCreateInfo, VmaPool* pool);
void ev_vulkan_allocatebufferinpool(VmaPool pool, unsigned long long bufferSize, unsigned long long usageFlags, EvBuffer *buffer);
void ev_vulkan_freememorypool(VmaPool pool);
void ev_vulkan_memorydump();

void ev_vulkan_allocateprimarycommandbuffer(QueueType queueType, VkCommandBuffer *cmdBuffer);

//vulkan images
void ev_vulkan_createimage(VkImageCreateInfo *imageCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvImage *image);
void ev_vulkan_createimageview(VkFormat imageFormat, VkImage *image, VkImageView* view);
void ev_vulkan_destroyimage(EvImage *image);
void ev_vulkan_destroyimageview(VkImageView imageView);

//vulkan buffers
void ev_vulkan_createbuffer(VkBufferCreateInfo *bufferCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvBuffer *buffer);
void ev_vulkan_destroybuffer(EvBuffer *buffer);

//getters
VkInstance ev_vulkan_getinstance();
VkDevice ev_vulkan_getlogicaldevice();
VkPhysicalDevice ev_vulkan_getphysicaldevice();
VkCommandPool ev_vulkan_getcommandpool(QueueType type);
VmaAllocator ev_vulkan_getvmaallocator();

void ev_vulkan_createdescriptorpool(VkDescriptorPoolCreateInfo *info, VkDescriptorPool *pool);
void ev_vulkan_destroydescriptorpool(VkDescriptorPool *pool);

void ev_vulkan_allocatedescriptor(VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set);


void ev_vulkan_allocateubo(unsigned long long bufferSize, bool persistentMap, UBO *ubo);
void ev_vulkan_updateubo(unsigned long long bufferSize, const void *data, UBO *ubo);
void ev_vulkan_freeubo(UBO *ubo);
void ev_vulkan_copybuffer(unsigned long long size, EvBuffer *src, EvBuffer *dst);
void ev_vulkan_updatestagingbuffer(EvBuffer *buffer, unsigned long long bufferSize, const void *data);
void ev_vulkan_allocatestagingbuffer(unsigned long long bufferSize, EvBuffer *buffer);
void ev_vulkan_createresourcememorypool(unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool);
EvBuffer ev_vulkan_registerbuffer(void *data, unsigned long long size);
void ev_vulkan_destroypipeline(VkPipeline pipeline);
void ev_vulkan_destroypipelinelayout(VkPipelineLayout pipelineLayout);
void ev_vulkan_destroyshadermodule(VkShaderModule shaderModule);
void ev_vulkan_destroysetlayout(VkDescriptorSetLayout descriptorSetLayout);
EvSwapchain* ev_vulkan_getSwapchain();
void ev_vulkan_createrenderpass();
void ev_vulkan_createframebuffers();
void ev_vulkan_destroyframebuffer();
void ev_vulkan_destroyrenderpass();

VkCommandBuffer ev_vulkan_startframe();
void ev_vulkan_endframe(VkCommandBuffer cmd);
VkRenderPass ev_vulkan_getrenderpass();
void ev_vulkan_wait();
