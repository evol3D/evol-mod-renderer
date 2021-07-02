#pragma once

#define VOLK_VERSION   VK_VERSION_1_2
#define VULKAN_VERSION VK_API_VERSION_1_2

#include <volk.h>
#include <Renderer_types.h>
#include <VulkanQueueManager.h>
#include <Swapchain.h>

static inline CONST_STR VkResultStrings(VkResult res) {
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
};

typedef unsigned int VkApiVersion;

int ev_vulkan_init();

int ev_vulkan_deinit();

void ev_vulkan_initvma();
void ev_vulkan_createinstance();

void ev_vulkan_detectphysicaldevice();

void ev_vulkan_createlogicaldevice();

void ev_vulkan_wait();

EvSwapchain* ev_vulkan_getSwapchain();

VkSurfaceKHR* ev_vulkan_getSurface();

void ev_vulkan_recreateSwapChain();

void ev_vulkan_checksurfacecompatibility();

void ev_vulkan_destroysurface(VkSurfaceKHR surface);

void ev_vulkan_createEvswapchain();

void ev_vulkan_createswapchain(unsigned int* imageCount, VkExtent2D extent, VkSurfaceKHR* surface, VkSurfaceFormatKHR *surfaceFormat, VkSwapchainKHR oldSwapchain, VkSwapchainKHR* swapchain);

void ev_vulkan_retrieveswapchainimages(VkSwapchainKHR swapchain, unsigned int *imageCount, VkImage *images);

void ev_vulkan_destroyswapchain(VkSwapchainKHR swapchain);

void ev_vulkan_createframebuffer(VkImageView* attachments, unsigned int attachmentCount, VkRenderPass renderPass, VkExtent2D surfaceExtent, VkFramebuffer *framebuffer);

void ev_vulkan_allocatememorypool(VmaPoolCreateInfo *poolCreateInfo, VmaPool* pool);

void ev_vulkan_allocatebufferinpool(VmaPool pool, unsigned long long bufferSize, unsigned long long usageFlags, EvBuffer *buffer);
void ev_vulkan_freememorypool(VmaPool pool);

void ev_vulkan_memorydump();

void ev_vulkan_allocateprimarycommandbuffer(QueueType queueType, VkCommandBuffer *cmdBuffer);

void ev_vulkan_createimage(VkImageCreateInfo *imageCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvImage *image);

void ev_vulkan_createimageview(VkFormat imageFormat, VkImage *image, VkImageView* view);

void ev_vulkan_destroyimage(EvImage image);

void ev_vulkan_createbuffer(VkBufferCreateInfo *bufferCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvBuffer *buffer);

void ev_vulkan_destroybuffer(EvBuffer *buffer);

VkInstance ev_vulkan_getinstance();
VkDevice ev_vulkan_getlogicaldevice();

VmaAllocator ev_vulkan_getvmaallocator();

VkPhysicalDevice ev_vulkan_getphysicaldevice();

VkCommandPool ev_vulkan_getcommandpool(QueueType type);
void ev_vulkan_createdescriptorpool(VkDescriptorPoolCreateInfo *info, VkDescriptorPool *pool);
void ev_vulkan_destroydescriptorpool(VkDescriptorPool *pool);

void ev_vulkan_allocatedescriptor(VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set);

void ev_vulkan_destroyimageview(VkImageView imageView);

void ev_vulkan_writeintobinding(DescriptorSet set, Binding *binding, uint32_t arrayElement, void *data);

void ev_vulkan_allocateubo(unsigned long long bufferSize, bool persistentMap, UBO *ubo);

void ev_vulkan_updateubo(unsigned long long bufferSize, const void *data, UBO *ubo);

void ev_vulkan_freeubo(UBO *ubo);

void ev_vulkan_copybuffer(unsigned long long size, EvBuffer *src, EvBuffer *dst);

void ev_vulkan_updatestagingbuffer(EvBuffer *buffer, unsigned long long bufferSize, const void *data);

void ev_vulkan_allocatestagingbuffer(unsigned long long bufferSize, EvBuffer *buffer);

void ev_vulkan_createresourcememorypool(VkBufferUsageFlagBits memoryFlags ,unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool);

EvBuffer ev_vulkan_registerbuffer(void *data, unsigned long long size);

void ev_vulkan_destroypipeline(VkPipeline pipeline);

void ev_vulkan_destroypipelinelayout(VkPipelineLayout pipelineLayout);

void ev_vulkan_destroyshadermodule(VkShaderModule shaderModule);

void ev_vulkan_destroysetlayout(VkDescriptorSetLayout descriptorSetLayout);

void ev_vulkan_createoffscreenrenderpass();

void ev_vulkan_createrenderpass();

void ev_vulkan_createoffscreenframebuffer();

void ev_vulkan_createframebuffers();

void ev_vulkan_destroyframebuffer();
void ev_vulkan_destroyrenderpass();

VkCommandBuffer ev_vulkan_startframeoffscreen(uint32_t frameNumber);

void ev_vulkan_endframeoffscreen(VkCommandBuffer cmd, uint32_t frameNumber);

VkCommandBuffer ev_vulkan_startframe(uint32_t frameNumber);
void ev_vulkan_endframe(VkCommandBuffer cmd, uint32_t frameNumber);

VkRenderPass ev_vulkan_getrenderpass();

VkRenderPass ev_vulkan_getoffscreenrenderpass();
void ev_vulkan_allocateimageinpool(VmaPool pool, uint32_t width, uint32_t height, unsigned long long usageFlags, EvImage *image);

void ev_vulkan_transitionimagelayout(EvImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
void ev_vulkan_copybuffertoimage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

EvTexture ev_vulkan_registerTexture(VkFormat format, uint32_t width, uint32_t height, void* pixels);

void ev_vulkan_destroytexture(EvTexture *texture);

void ev_vulkan_buildlightPipeline();
FrameBuffer *ev_vulkan_getoffscreenframebuffer();
