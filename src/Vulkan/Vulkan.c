#include <Vulkan.h>

#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <DescriptorManager.h>
#include <evol/common/ev_log.h>

#define EV_USAGEFLAGS_RESOURCE_BUFFER   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
# define EV_USAGEFLAGS_RESOURCE_IMAGE   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT

struct ev_Vulkan_Data {
  VkInstance       instance;
  VkApiVersion     apiVersion;
  VkDevice         logicalDevice;
  VkPhysicalDevice physicalDevice;

  VkSurfaceKHR surface;

  VmaPool buffersPool;
  VmaPool imagesPool;
  VmaAllocator     allocator;

  VkCommandPool    commandPools[QUEUE_TYPE_COUNT];

  EvSwapchain swapchain;
  VkRenderPass renderPass;
  VkFramebuffer framebuffers[SWAPCHAIN_MAX_IMAGES];

  FrameBuffer offscreenFrameBuffer;
  VkCommandBuffer offscreencommandbuffer[SWAPCHAIN_MAX_IMAGES];
  VkSemaphore offscreenrendersemaphore[SWAPCHAIN_MAX_IMAGES];

  uint32_t swapchainImageIndex;
} VulkanData;

#define DATA(X) VulkanData.X

int ev_vulkan_init();
int ev_vulkan_deinit();

void ev_vulkan_initvma();
void ev_vulkan_createinstance();
void ev_vulkan_createlogicaldevice();
void ev_vulkan_detectphysicaldevice();

int ev_vulkan_init()
{
  // Zeroing up commandpools to identify the ones that get initialized
  for(int i = 0; i < QUEUE_TYPE_COUNT; ++i)
  {
    VulkanData.commandPools[i] = 0;
  }

  VK_ASSERT(volkInitialize());

  // Create the vulkan instance
  ev_log_debug("Creating Vulkan Instance");
  ev_vulkan_createinstance();
  ev_log_debug("Created Vulkan Instance Successfully");

  volkLoadInstance(VulkanData.instance);

  // Detect the physical device
  ev_log_debug("Detecting Vulkan Physical Device");
  ev_vulkan_detectphysicaldevice();
  ev_log_debug("Detected Vulkan Physical Device");

  // Create the logical device
  ev_log_debug("Creating Vulkan Logical Device");
  ev_vulkan_createlogicaldevice();
  ev_log_debug("Created Vulkan Logical Device");

  // Initialize VMA
  ev_log_debug("Initializing VMA");
  ev_vulkan_initvma();
  ev_log_debug("Initialized VMA");

  ev_vulkan_createresourcememorypool(EV_USAGEFLAGS_RESOURCE_BUFFER ,128ull * 1024 * 1024, 1, 4, &DATA(buffersPool));
  ev_vulkan_createresourcememorypool(EV_USAGEFLAGS_RESOURCE_IMAGE ,512ull * 1024 * 1024, 1, 4, &DATA(imagesPool));

  ev_descriptormanager_init();

  VkSemaphoreCreateInfo semaphoreCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };
  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++) {
    ev_vulkan_allocateprimarycommandbuffer(GRAPHICS, &VulkanData.offscreencommandbuffer[i]);
    VK_ASSERT(vkCreateSemaphore(ev_vulkan_getlogicaldevice(), &semaphoreCreateInfo, NULL, &VulkanData.offscreenrendersemaphore[i]));
  }

  return 0;
}

int ev_vulkan_deinit()
{
  ev_vulkan_destroyframebuffer();
  ev_vulkan_destroyrenderpass();

  ev_vulkan_destroyoffscreenframebuffer();
  ev_vulkan_destroyoffscreenrenderpass();
  for(size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; ++i)
  {
    vkDestroySemaphore(ev_vulkan_getlogicaldevice(), DATA(offscreenrendersemaphore)[i], NULL);
  }

  ev_swapchain_destroy(&DATA(swapchain));

  // Destroy any commandpool that was created earlier
  for(int i = 0; i < QUEUE_TYPE_COUNT; ++i)
    if(VulkanData.commandPools[i])
      vkDestroyCommandPool(VulkanData.logicalDevice, VulkanData.commandPools[i], NULL);

  ev_descriptormanager_dinit();

  //Destroy Surface
  ev_vulkan_destroysurface(VulkanData.surface);

  // Destroy VMA
  ev_vulkan_freememorypool(DATA(imagesPool));
  ev_vulkan_freememorypool(DATA(buffersPool));
  vmaDestroyAllocator(VulkanData.allocator);

  // Destroy the logical device
  vkDestroyDevice(VulkanData.logicalDevice, NULL);

  // Destroy the vulkan instance
  vkDestroyInstance(VulkanData.instance, NULL);

  return 0;
}

void ev_vulkan_initvma()
{
  VmaVulkanFunctions vmaFunctions = {
    .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
    .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
    .vkAllocateMemory = vkAllocateMemory,
    .vkFreeMemory = vkFreeMemory,
    .vkMapMemory = vkMapMemory,
    .vkUnmapMemory = vkUnmapMemory,
    .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
    .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
    .vkBindBufferMemory = vkBindBufferMemory,
    .vkBindImageMemory = vkBindImageMemory,
    .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
    .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
    .vkCreateBuffer = vkCreateBuffer,
    .vkDestroyBuffer = vkDestroyBuffer,
    .vkCreateImage = vkCreateImage,
    .vkDestroyImage = vkDestroyImage,
    .vkCmdCopyBuffer = vkCmdCopyBuffer,
  };

  VmaAllocatorCreateInfo createInfo = {
    .physicalDevice   = VulkanData.physicalDevice,
    .device           = VulkanData.logicalDevice,
    .instance         = VulkanData.instance,
    .vulkanApiVersion = VulkanData.apiVersion,
    .pVulkanFunctions = &vmaFunctions,
  };

  vmaCreateAllocator(&createInfo, &VulkanData.allocator);
}

void ev_vulkan_createinstance()
{
  const char *extensions[] = {
    "VK_KHR_surface",
    "VK_KHR_display",
    "VK_EXT_direct_mode_display",
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    #ifdef _WIN32
    "VK_KHR_win32_surface",
    #else
    "VK_KHR_xcb_surface",
    #endif
  };

  const char *validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
    //"VK_LAYER_LUNARG_monitor",
    // "VK_LAYER_LUNARG_api_dump",
  };

  VkApplicationInfo applicationInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "evol_vulkan",
    .applicationVersion = 0,
    .apiVersion = VULKAN_VERSION,
  };

  VkInstanceCreateInfo instanceCreateInfo = {
    .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo        = &applicationInfo,
    .enabledLayerCount       = ARRAYSIZE(validation_layers),
    .ppEnabledLayerNames     = validation_layers,
    .enabledExtensionCount   = ARRAYSIZE(extensions),
    .ppEnabledExtensionNames = extensions
  };

  VK_ASSERT(vkCreateInstance(&instanceCreateInfo, NULL, &VulkanData.instance));
}

void ev_vulkan_detectphysicaldevice()
{
  unsigned int physicalDeviceCount = 10;

  VkPhysicalDevice physicalDevices[physicalDeviceCount];
  vkEnumeratePhysicalDevices( VulkanData.instance, &physicalDeviceCount, physicalDevices);

  VulkanData.physicalDevice = physicalDevices[0];

  for(unsigned int i = 0; i < physicalDeviceCount; ++i)
  {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
    if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      VulkanData.physicalDevice = physicalDevices[i];
      break;
    }
  }
}

void ev_vulkan_createlogicaldevice()
{
  const char *deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
  };

  VkDeviceQueueCreateInfo *deviceQueueCreateInfos = NULL;
  unsigned int queueCreateInfoCount = 0;
  VulkanQueueManager.init(VulkanData.physicalDevice, &deviceQueueCreateInfos, &queueCreateInfoCount);

  VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeatures = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
  };

  VkDeviceCreateInfo deviceCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &physicalDeviceDescriptorIndexingFeatures,
    .enabledExtensionCount = ARRAYSIZE(deviceExtensions),
    .ppEnabledExtensionNames = deviceExtensions,
    .queueCreateInfoCount = queueCreateInfoCount,
    .pQueueCreateInfos = deviceQueueCreateInfos,
  };

  VK_ASSERT(vkCreateDevice(VulkanData.physicalDevice, &deviceCreateInfo, NULL, &VulkanData.logicalDevice));

  VulkanQueueManager.retrieveQueues(VulkanData.logicalDevice, deviceQueueCreateInfos, &queueCreateInfoCount);
}

void ev_vulkan_wait()
{
  vkWaitForFences(ev_vulkan_getlogicaldevice(), DATA(swapchain).imageCount, DATA(swapchain).renderFences, VK_TRUE, ~0ull);
}

EvSwapchain* ev_vulkan_getSwapchain()
{
  return &VulkanData.swapchain;
}

VkSurfaceKHR* ev_vulkan_getSurface()
{
  return &VulkanData.surface;
}

void ev_vulkan_recreateSwapChain()
{
  vkDeviceWaitIdle(ev_vulkan_getlogicaldevice());

  ev_vulkan_destroyframebuffer();

  ev_renderer_updatewindowsize();

  ev_swapchain_create(0 ,&DATA(swapchain), &DATA(surface));
  ev_vulkan_createframebuffers();
}

void ev_vulkan_checksurfacecompatibility()
{
  VkResult res;

  VkBool32 surfaceSupported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(VulkanData.physicalDevice, VulkanQueueManager.getFamilyIndex(GRAPHICS), VulkanData.surface, &surfaceSupported);

  res = surfaceSupported == VK_FALSE ? !VK_SUCCESS : VK_SUCCESS;

  VK_ASSERT(res);
}

void ev_vulkan_destroysurface(VkSurfaceKHR surface)
{
  // Destroy the vulkan surface
  vkDestroySurfaceKHR(VulkanData.instance, surface, NULL);
}

void ev_vulkan_createEvswapchain(uint32_t framebuffering)
{
    ev_swapchain_create(framebuffering, &VulkanData.swapchain, &VulkanData.surface);
}

void ev_vulkan_createswapchain(unsigned int* imageCount, VkExtent2D extent, VkSurfaceKHR* surface, VkSurfaceFormatKHR *surfaceFormat, VkSwapchainKHR oldSwapchain, VkSwapchainKHR* swapchain)
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;

  VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanData.physicalDevice, *surface, &surfaceCapabilities));

  unsigned int windowWidth  = surfaceCapabilities.currentExtent.width;
  unsigned int windowHeight = surfaceCapabilities.currentExtent.height;

  if(windowWidth == UINT32_MAX || windowHeight == UINT32_MAX)
  {
    windowWidth = extent.width;
    windowHeight = extent.height;
  }

  // The forbidden fruit (don't touch it)
  VkCompositeAlphaFlagBitsKHR compositeAlpha =
    (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
      ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
      : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
        : (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
          ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
          : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

  *imageCount = MAX(*imageCount, surfaceCapabilities.minImageCount);

  if(surfaceCapabilities.maxImageCount) // If there is an upper limit
  {
    *imageCount = MIN(*imageCount, surfaceCapabilities.maxImageCount);
  }

  VkSwapchainCreateInfoKHR swapchainCreateInfo =
  {
    .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface          = *surface,
    .minImageCount    = *imageCount,
    .imageFormat      = surfaceFormat->format,
    .imageColorSpace  = surfaceFormat->colorSpace,
    .imageExtent      = {
        .width          = windowWidth,
        .height         = windowHeight,
      },
    .imageArrayLayers = 1,
    .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform     = surfaceCapabilities.currentTransform,
    .compositeAlpha   = compositeAlpha,
    .presentMode      = VK_PRESENT_MODE_MAILBOX_KHR, // TODO: Make sure that this is always supported
    .clipped          = VK_TRUE,
    .oldSwapchain     = VK_NULL_HANDLE,
  };

  VK_ASSERT(vkCreateSwapchainKHR(VulkanData.logicalDevice, &swapchainCreateInfo, NULL, swapchain));
}

void ev_vulkan_retrieveswapchainimages(VkSwapchainKHR swapchain, unsigned int *imageCount, VkImage *images)
{
  VK_ASSERT(vkGetSwapchainImagesKHR(VulkanData.logicalDevice, swapchain, imageCount, NULL));

  VK_ASSERT(vkGetSwapchainImagesKHR(VulkanData.logicalDevice, swapchain, imageCount, images));
}

void ev_vulkan_destroyswapchain(VkSwapchainKHR swapchain)
{
  vkDestroySwapchainKHR(VulkanData.logicalDevice, swapchain, NULL);
}

void ev_vulkan_createframebuffer(VkImageView* attachments, unsigned int attachmentCount, VkRenderPass renderPass, VkExtent2D surfaceExtent, VkFramebuffer *framebuffer)
{
  VkFramebufferCreateInfo swapchainFramebufferCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = renderPass,
    .attachmentCount = attachmentCount,
    .pAttachments = attachments,
    .width = surfaceExtent.width,
    .height = surfaceExtent.height,
    .layers = 1,
  };
  VK_ASSERT(vkCreateFramebuffer(VulkanData.logicalDevice, &swapchainFramebufferCreateInfo, NULL, framebuffer));
}

void ev_vulkan_allocatememorypool(VmaPoolCreateInfo *poolCreateInfo, VmaPool* pool)
{
  VK_ASSERT(vmaCreatePool(VulkanData.allocator, poolCreateInfo, pool));
}

void ev_vulkan_allocatebufferinpool(VmaPool pool, unsigned long long bufferSize, unsigned long long usageFlags, EvBuffer *buffer)
{
  VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferCreateInfo.size = bufferSize;
  bufferCreateInfo.usage = usageFlags;

  VmaAllocationCreateInfo allocationCreateInfo = {
    .pool = pool,
  };

  ev_vulkan_createbuffer(&bufferCreateInfo, &allocationCreateInfo, buffer);
}

void ev_vulkan_freememorypool(VmaPool pool)
{
  vmaDestroyPool(VulkanData.allocator, pool);
}

void ev_vulkan_memorydump()
{
  char* vmaDump;
  vmaBuildStatsString(VulkanData.allocator, &vmaDump, VK_TRUE);

  FILE *dumpFile = fopen("vmadump.json", "w+");
  fputs(vmaDump, dumpFile);
  fclose(dumpFile);

  vmaFreeStatsString(VulkanData.allocator, vmaDump);
}

void ev_vulkan_allocateprimarycommandbuffer(QueueType queueType, VkCommandBuffer *cmdBuffer)
{
  VkCommandPool pool = ev_vulkan_getcommandpool(queueType);

  VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  vkAllocateCommandBuffers(VulkanData.logicalDevice, &cmdBufferAllocateInfo, cmdBuffer);
}

void ev_vulkan_createimage(VkImageCreateInfo *imageCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvImage *image)
{
  vmaCreateImage(VulkanData.allocator, imageCreateInfo, allocationCreateInfo, &(image->image), &(image->allocation), &(image->allocationInfo));
}

void ev_vulkan_createimageview(VkFormat imageFormat, VkImage *image, VkImageView* view)
{
    VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = *image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = imageFormat,
      .components = {0, 0, 0, 0},
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
    };

    vkCreateImageView(VulkanData.logicalDevice, &imageViewCreateInfo, NULL, view);
}

void ev_vulkan_destroyimage(EvImage image)
{
  vmaDestroyImage(VulkanData.allocator, image.image, image.allocation);
}

void ev_vulkan_createbuffer(VkBufferCreateInfo *bufferCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvBuffer *buffer)
{
  VK_ASSERT(vmaCreateBuffer(VulkanData.allocator, bufferCreateInfo, allocationCreateInfo, &(buffer->buffer), &(buffer->allocation), &(buffer->allocationInfo)));
}

void ev_vulkan_destroybuffer(EvBuffer *buffer)
{
  vmaDestroyBuffer(VulkanData.allocator, buffer->buffer, buffer->allocation);
}

VkInstance ev_vulkan_getinstance()
{
  return VulkanData.instance;
}

VkDevice ev_vulkan_getlogicaldevice()
{
  return VulkanData.logicalDevice;
}

VmaAllocator ev_vulkan_getvmaallocator()
{
  return VulkanData.allocator;
}

VkPhysicalDevice ev_vulkan_getphysicaldevice()
{
  return VulkanData.physicalDevice;
}

VkCommandPool ev_vulkan_getcommandpool(QueueType type)
{
  if(!VulkanData.commandPools[type])
  {
    VkCommandPoolCreateInfo commandPoolCreateInfo =
    {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      // TODO: Is this really what we want to have?
      .flags = (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
      .queueFamilyIndex = VulkanQueueManager.getFamilyIndex(type),
    };
    VK_ASSERT(vkCreateCommandPool(VulkanData.logicalDevice, &commandPoolCreateInfo, NULL, &(VulkanData.commandPools[type])));
  }
  return VulkanData.commandPools[type];
}

void ev_vulkan_createdescriptorpool(VkDescriptorPoolCreateInfo *info, VkDescriptorPool *pool)
{
  VK_ASSERT(vkCreateDescriptorPool(DATA(logicalDevice), info, NULL, pool));
}

void ev_vulkan_destroydescriptorpool(VkDescriptorPool *pool)
{
  vkDestroyDescriptorPool(DATA(logicalDevice), *pool, NULL);
}

void ev_vulkan_allocatedescriptor(VkDescriptorSetAllocateInfo *info, VkDescriptorSet *set)
{
  VK_ASSERT(vkAllocateDescriptorSets(DATA(logicalDevice), info, set));
}

void ev_vulkan_destroyimageview(VkImageView imageView)
{
  vkDestroyImageView(DATA(logicalDevice), imageView, NULL);
}

void ev_vulkan_writeintobinding(DescriptorSet set, Binding *binding, uint32_t arrayElement, void *data)
{
  VkWriteDescriptorSet setWrite;
  VkDescriptorBufferInfo bufferInfo = {0};
  VkDescriptorImageInfo imageinfo = {0};

  switch(binding->type)
  {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      {
        bufferInfo.buffer = ((EvBuffer*)data)->buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        setWrite = (VkWriteDescriptorSet){
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .descriptorCount = 1,
          .descriptorType = binding->type,
          .dstSet = set.set,
          .dstBinding = binding->binding,
          .dstArrayElement = arrayElement,
          .pBufferInfo = &bufferInfo,
        };
        break;
      }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      {
        imageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        imageinfo.imageView   = ((EvTexture*)data)->imageView,
        imageinfo.sampler     = ((EvTexture*)data)->sampler,

        setWrite = (VkWriteDescriptorSet)
        {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .descriptorCount = 1,
          .descriptorType = binding->type,
          .dstSet = set.set,
          .dstBinding = binding->binding,
          .dstArrayElement = arrayElement,
          .pImageInfo = &imageinfo,
        };
        break;
      }
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
    default:
      ;
  }

  vkUpdateDescriptorSets(DATA(logicalDevice), 1, &setWrite, 0, NULL);
}

void ev_vulkan_allocateubo(unsigned long long bufferSize, bool persistentMap, UBO *ubo)
{
  VmaAllocationCreateInfo allocationCreateInfo = {
    .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  };

  VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferCreateInfo.size = bufferSize;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  ev_vulkan_createbuffer(&bufferCreateInfo, &allocationCreateInfo, &ubo->buffer);

  if(persistentMap)
    vmaMapMemory(ev_vulkan_getvmaallocator, ubo->buffer.allocation, &ubo->mappedData);
  else
    ubo->mappedData = 0;
}

void ev_vulkan_updateubo(unsigned long long bufferSize, const void *data, UBO *ubo)
{
  if(ubo->mappedData)
  {
    ev_log_debug("%p", data);
    memcpy(ubo->mappedData, (CameraData*)data, bufferSize);
  }
  else
  {
    vmaMapMemory(ev_vulkan_getvmaallocator(), ubo->buffer.allocation, &ubo->mappedData);
    memcpy(ubo->mappedData, data, bufferSize);
    vmaUnmapMemory(ev_vulkan_getvmaallocator(), ubo->buffer.allocation);

    ubo->mappedData = 0;
  }
}

void ev_vulkan_freeubo(UBO *ubo)
{
  if(ubo->mappedData)
    vmaUnmapMemory(DATA(allocator), ubo->buffer.allocation);

  ev_vulkan_destroybuffer(&ubo->buffer);
}

void ev_vulkan_copybuffer(unsigned long long size, EvBuffer *src, EvBuffer *dst)
{
  VkCommandBuffer tempCommandBuffer;
  ev_vulkan_allocateprimarycommandbuffer(TRANSFER, &tempCommandBuffer);

  VkCommandBufferBeginInfo tempCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  tempCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(tempCommandBuffer, &tempCommandBufferBeginInfo);

  VkBufferCopy copyRegion;
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = size;
  vkCmdCopyBuffer(tempCommandBuffer, src->buffer, dst->buffer, 1, &copyRegion);

  vkEndCommandBuffer(tempCommandBuffer);

  VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

  VkSubmitInfo submitInfo         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.pWaitDstStageMask    = &stageMask;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &tempCommandBuffer;
  submitInfo.waitSemaphoreCount   = 0;
  submitInfo.pWaitSemaphores      = NULL;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores    = NULL;

  VkQueue transferQueue = VulkanQueueManager.getQueue(TRANSFER);
  VK_ASSERT(vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE));

  //TODO Change this to take advantage of fences
  vkQueueWaitIdle(transferQueue);

  //vkFreeCommandBuffers(ev_vulkan_getlogicaldevice(), tempCommandBuffer, 1, &tempCommandBuffer);
}

void ev_vulkan_updatestagingbuffer(EvBuffer *buffer, unsigned long long bufferSize, const void *data)
{
  void *mapped;
  vmaMapMemory(ev_vulkan_getvmaallocator(), buffer->allocation, &mapped);
  memcpy(mapped, data, bufferSize);
  vmaUnmapMemory(ev_vulkan_getvmaallocator(), buffer->allocation);
}

void ev_vulkan_allocatestagingbuffer(unsigned long long bufferSize, EvBuffer *buffer)
{
  VmaAllocationCreateInfo allocationCreateInfo = {
    .usage = VMA_MEMORY_USAGE_CPU_ONLY, // TODO: Experiment with VMA_MEMORY_USAGE_CPU_TO_GPU
  };

  VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
  bufferCreateInfo.size = bufferSize;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  ev_vulkan_createbuffer(&bufferCreateInfo, &allocationCreateInfo, buffer);
}

void ev_vulkan_createresourcememorypool(VkBufferUsageFlagBits memoryFlags ,unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool)
{
  unsigned int memoryType;

  { // Detecting memorytype index
    VkBufferCreateInfo sampleBufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    sampleBufferCreateInfo.size = 1024;
    sampleBufferCreateInfo.usage = memoryFlags;

    VmaAllocationCreateInfo allocationCreateInfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
  };

    vmaFindMemoryTypeIndexForBufferInfo(ev_vulkan_getvmaallocator(), &sampleBufferCreateInfo, &allocationCreateInfo, &memoryType);
  }

  VmaPoolCreateInfo poolCreateInfo = {
	  .memoryTypeIndex   = memoryType,
	  .blockSize         = blockSize,
	  .minBlockCount     = minBlockCount,
	  .maxBlockCount     = maxBlockCount,
  };

  ev_vulkan_allocatememorypool(&poolCreateInfo, pool);
}

EvBuffer ev_vulkan_registerbuffer(void *data, unsigned long long size)
{
  EvBuffer newBuffer;
  ev_vulkan_allocatebufferinpool(DATA(buffersPool), size, EV_USAGEFLAGS_RESOURCE_BUFFER, &newBuffer);

  EvBuffer newStagingBuffer;
  ev_vulkan_allocatestagingbuffer(size, &newStagingBuffer);
  ev_vulkan_updatestagingbuffer(&newStagingBuffer, size, data);
  ev_vulkan_copybuffer(size, &newStagingBuffer, &newBuffer);

  ev_vulkan_destroybuffer(&newStagingBuffer);

  return newBuffer;
}

void ev_vulkan_destroypipeline(VkPipeline pipeline)
{
  vkDestroyPipeline(ev_vulkan_getlogicaldevice(), pipeline, NULL);
}

void ev_vulkan_destroypipelinelayout(VkPipelineLayout pipelineLayout)
{
  vkDestroyPipelineLayout(VulkanData.logicalDevice, pipelineLayout, NULL);
}

void ev_vulkan_destroyshadermodule(VkShaderModule shaderModule)
{
  vkDestroyShaderModule(VulkanData.logicalDevice, shaderModule, NULL);
}

void ev_vulkan_destroysetlayout(VkDescriptorSetLayout descriptorSetLayout)
{
  vkDestroyDescriptorSetLayout(VulkanData.logicalDevice, descriptorSetLayout, NULL);
}

void ev_vulkan_createoffscreenrenderpass()
{
  VkAttachmentDescription attachmentDescriptions[] =
  {
    //position
    {
      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    //normal
    {
      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    //albedo
    {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    //specular
    {
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    },
    //depth
    {
      .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    },
  };

  VkAttachmentReference colorAttachmentReferences[] =
  {
    //position
    {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    //normal
    {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    //albed
    {
      .attachment = 2,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    //specular
    {
      .attachment = 3,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
  };

  VkAttachmentReference depthStencilAttachmentReference =
  {
    .attachment = 4,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpassDescriptions[] =
  {
    {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .colorAttachmentCount = ARRAYSIZE(colorAttachmentReferences),
      .pColorAttachments = colorAttachmentReferences,
      .pResolveAttachments = NULL,
      .pDepthStencilAttachment = &depthStencilAttachmentReference,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL,
    },
  };

  VkSubpassDependency dependencies[] = {
    {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
    	.dstSubpass = 0,
    	.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    	.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    	.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
    	.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    	.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
    {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
  };

  VkRenderPassCreateInfo renderPassCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = ARRAYSIZE(attachmentDescriptions),
    .pAttachments = attachmentDescriptions,
    .subpassCount = ARRAYSIZE(subpassDescriptions),
    .pSubpasses = subpassDescriptions,
    .dependencyCount = ARRAYSIZE(dependencies),
    .pDependencies = dependencies,
  };

  VK_ASSERT(vkCreateRenderPass(VulkanData.logicalDevice, &renderPassCreateInfo, NULL, &DATA(offscreenFrameBuffer).renderPass));
}

void ev_vulkan_createrenderpass()
{
  VkAttachmentDescription attachmentDescriptions[] =
  {
    //albedo
    {
      .format = DATA(swapchain).surfaceFormat.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    // //depth
    // {
    //   .format = DATA(swapchain).depthStencilFormat,
    //   .samples = VK_SAMPLE_COUNT_1_BIT,
    //   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    //   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    //   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    //   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    //   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    //   .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    // }
  };

  VkAttachmentReference colorAttachmentReferences[] =
  {
    //albedo
    {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
  };

  // VkAttachmentReference depthStencilAttachmentReference =
  // {
  //   .attachment = 1,
  //   .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  // };

  VkSubpassDescription subpassDescriptions[] =
  {
    {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .colorAttachmentCount = ARRAYSIZE(colorAttachmentReferences),
      .pColorAttachments = colorAttachmentReferences,
      .pResolveAttachments = NULL,
      // .pDepthStencilAttachment = &depthStencilAttachmentReference,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL,
    },
  };

  VkRenderPassCreateInfo renderPassCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = ARRAYSIZE(attachmentDescriptions),
    .pAttachments = attachmentDescriptions,
    .subpassCount = ARRAYSIZE(subpassDescriptions),
    .pSubpasses = subpassDescriptions,
    .dependencyCount = 0,
    .pDependencies = NULL,
  };

  VK_ASSERT(vkCreateRenderPass(VulkanData.logicalDevice, &renderPassCreateInfo, NULL, &DATA(renderPass)));
}

void ev_vulkan_createoffscreenframebuffer()
{
  //position texture
  {
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = VK_FORMAT_R16G16B16A16_SFLOAT,
      .extent        = (VkExtent3D) {
        .width       = VulkanData.swapchain.windowExtent.width,
        .height      = VulkanData.swapchain.windowExtent.height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo vmaAllocationCreateInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &VulkanData.offscreenFrameBuffer.position.texture.image);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = VulkanData.offscreenFrameBuffer.position.texture.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &VulkanData.offscreenFrameBuffer.position.texture.imageView);

    VkSamplerCreateInfo samplerInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(VulkanData.logicalDevice, &samplerInfo, NULL, &VulkanData.offscreenFrameBuffer.position.texture.sampler);
  }

  //normal texture
  {
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = VK_FORMAT_R16G16B16A16_SFLOAT,
      .extent        = (VkExtent3D) {
        .width       = VulkanData.swapchain.windowExtent.width,
        .height      = VulkanData.swapchain.windowExtent.height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo vmaAllocationCreateInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &VulkanData.offscreenFrameBuffer.normal.texture.image);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = VulkanData.offscreenFrameBuffer.normal.texture.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &VulkanData.offscreenFrameBuffer.normal.texture.imageView);

    VkSamplerCreateInfo samplerInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(VulkanData.logicalDevice, &samplerInfo, NULL, &VulkanData.offscreenFrameBuffer.normal.texture.sampler);
  }

  //albedo texture
  {
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = VK_FORMAT_R8G8B8A8_UNORM,
      .extent        = (VkExtent3D) {
        .width       = VulkanData.swapchain.windowExtent.width,
        .height      = VulkanData.swapchain.windowExtent.height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo vmaAllocationCreateInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &VulkanData.offscreenFrameBuffer.albedo.texture.image);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = VulkanData.offscreenFrameBuffer.albedo.texture.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &VulkanData.offscreenFrameBuffer.albedo.texture.imageView);

    VkSamplerCreateInfo samplerInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(VulkanData.logicalDevice, &samplerInfo, NULL, &VulkanData.offscreenFrameBuffer.albedo.texture.sampler);
  }

  //specular texture
  {
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = VK_FORMAT_R8G8B8A8_UNORM,
      .extent        = (VkExtent3D) {
        .width       = VulkanData.swapchain.windowExtent.width,
        .height      = VulkanData.swapchain.windowExtent.height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo vmaAllocationCreateInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &VulkanData.offscreenFrameBuffer.specular.texture.image);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = VulkanData.offscreenFrameBuffer.specular.texture.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &VulkanData.offscreenFrameBuffer.specular.texture.imageView);

    VkSamplerCreateInfo samplerInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(VulkanData.logicalDevice, &samplerInfo, NULL, &VulkanData.offscreenFrameBuffer.specular.texture.sampler);
  }

  //depth image and image view
  {
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .extent        = (VkExtent3D) {
        .width       = VulkanData.swapchain.windowExtent.width,
        .height      = VulkanData.swapchain.windowExtent.height,
        .depth       = 1,
      },
      .mipLevels     = 1,
      .arrayLayers   = 1,
      .samples       = VK_SAMPLE_COUNT_1_BIT,
      .tiling        = VK_IMAGE_TILING_OPTIMAL,
      .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VmaAllocationCreateInfo vmaAllocationCreateInfo = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &VulkanData.offscreenFrameBuffer.depth.texture.image);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = VulkanData.offscreenFrameBuffer.depth.texture.image.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &VulkanData.offscreenFrameBuffer.depth.texture.imageView);
  }

  VkImageView attachments[] =
  {
    VulkanData.offscreenFrameBuffer.position.texture.imageView,
    VulkanData.offscreenFrameBuffer.normal.texture.imageView,
    VulkanData.offscreenFrameBuffer.albedo.texture.imageView,
    VulkanData.offscreenFrameBuffer.specular.texture.imageView,
    VulkanData.offscreenFrameBuffer.depth.texture.imageView,
  };

  ev_vulkan_createframebuffer(attachments, ARRAYSIZE(attachments), VulkanData.offscreenFrameBuffer.renderPass, DATA(swapchain.windowExtent), &DATA(offscreenFrameBuffer).frameBuffer);
}

void ev_vulkan_createframebuffers()
{
  for(size_t i = 0; i < DATA(swapchain).imageCount; ++i)
  {
    VkImageView attachments[] =
    {
      DATA(swapchain).imageViews[i],
      // DATA(swapchain).depthImageView,
    };

    ev_vulkan_createframebuffer(attachments, ARRAYSIZE(attachments), VulkanData.renderPass, DATA(swapchain.windowExtent), DATA(framebuffers + i));
  }
}

void ev_vulkan_destroyframebuffer()
{
  for (size_t i = 0; i < DATA(swapchain).imageCount; i++)
  {
    vkDestroyFramebuffer(VulkanData.logicalDevice, DATA(framebuffers[i]), NULL);
  }
}

void ev_vulkan_destroyoffscreenframebuffer()
{
  ev_vulkan_destroytexture(&DATA(offscreenFrameBuffer).position.texture);
  ev_vulkan_destroytexture(&DATA(offscreenFrameBuffer).normal.texture);
  ev_vulkan_destroytexture(&DATA(offscreenFrameBuffer).albedo.texture);
  ev_vulkan_destroytexture(&DATA(offscreenFrameBuffer).specular.texture);
  ev_vulkan_destroytexture(&DATA(offscreenFrameBuffer).depth.texture);

  vkDestroyFramebuffer(VulkanData.logicalDevice, DATA(offscreenFrameBuffer).frameBuffer, NULL);
}

void ev_vulkan_destroyrenderpass()
{
  vkDestroyRenderPass(VulkanData.logicalDevice, DATA(renderPass), NULL);
}

void ev_vulkan_destroyoffscreenrenderpass()
{
  vkDestroyRenderPass(VulkanData.logicalDevice, DATA(offscreenFrameBuffer).renderPass, NULL);
}

VkCommandBuffer ev_vulkan_startframeoffscreen(uint32_t frameNumber)
{
  VK_ASSERT(vkWaitForFences(ev_vulkan_getlogicaldevice(), 1, &DATA(swapchain).renderFences[frameNumber], true, ~0ull));
  VK_ASSERT(vkResetFences(ev_vulkan_getlogicaldevice(), 1, &DATA(swapchain).renderFences[frameNumber]));

  VK_ASSERT(vkResetCommandBuffer(DATA(offscreencommandbuffer[frameNumber]), 0));

  VkCommandBufferBeginInfo cmdBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,

    .pInheritanceInfo = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vkBeginCommandBuffer(DATA(offscreencommandbuffer[frameNumber]), &cmdBeginInfo));

  VkClearValue clearValues[] =
  {
    {
      .color = { {0.0f, 0.0f, 0.0f, 1.f} },
    },
    {
      .color = { {0.0f, 0.0f, 0.0f, 1.f} },
    },
    {
      .color = { {0.0f, 0.0f, 0.0f, 1.f} },
    },
    {
      .color = { {0.0f, 0.0f, 0.0f, 1.f} },
    },
    {
      .depthStencil = {1.0f, 0.0f},
    }
  };

  VkRenderPassBeginInfo rpInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext = NULL,

    .renderPass = DATA(offscreenFrameBuffer).renderPass,
    .renderArea.offset.x = 0,
    .renderArea.offset.y = 0,
    .renderArea.extent.width = DATA(swapchain.windowExtent.width),
    .renderArea.extent.height = DATA(swapchain.windowExtent.height),
    .framebuffer = VulkanData.offscreenFrameBuffer.frameBuffer,

    .clearValueCount = ARRAYSIZE(clearValues),
    .pClearValues = &clearValues,
  };
  vkCmdBeginRenderPass(DATA(offscreencommandbuffer[frameNumber]), &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  {
    VkRect2D scissor = {
      .offset = {0, 0},
      .extent = DATA(swapchain.windowExtent),
    };

    VkViewport viewport =
    {
      .x = 0,
      .y = DATA(swapchain.windowExtent.height),
      .width = DATA(swapchain.windowExtent.width),
      .height = -(float) DATA(swapchain.windowExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetScissor(DATA(offscreencommandbuffer[frameNumber]), 0, 1, &scissor);
    vkCmdSetViewport(DATA(offscreencommandbuffer[frameNumber]), 0, 1, &viewport);
  }

  return DATA(offscreencommandbuffer[frameNumber]);
}

void ev_vulkan_endframeoffscreen(VkCommandBuffer cmd, uint32_t frameNumber)
{
  vkCmdEndRenderPass(cmd);
  VK_ASSERT(vkEndCommandBuffer(cmd));
  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = NULL,
  };

  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.pWaitDstStageMask = &waitStage;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &DATA(offscreenrendersemaphore)[frameNumber];

  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, VK_NULL_HANDLE));
}

VkCommandBuffer ev_vulkan_startframe(uint32_t frameNumber)
{
  VK_ASSERT(vkResetCommandBuffer(DATA(swapchain).commandBuffers[frameNumber], 0));

  VkResult result = vkAcquireNextImageKHR(ev_vulkan_getlogicaldevice(), DATA(swapchain).swapchain, 1000000000, DATA(swapchain).presentSemaphores[frameNumber], NULL, &DATA(swapchainImageIndex));
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ev_vulkan_recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    ev_log_debug("failed to acquire swap chain image!");
  }

  VkCommandBuffer cmd = DATA(swapchain).commandBuffers[frameNumber];

  VkCommandBufferBeginInfo cmdBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,

    .pInheritanceInfo = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  VkImageMemoryBarrier imageMemoryBarrier =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = NULL,
    .srcAccessMask = 0,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,  // TODO
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,  // TODO
    .image = DATA(swapchain).images[DATA(swapchainImageIndex)],
    .subresourceRange =
    {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    }
  };

  vkCmdPipelineBarrier(cmd,
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &imageMemoryBarrier);

  VkClearValue clearValues[] =
  {
    {
      .color = { {0.13f, 0.22f, 0.37f, 1.f} },
    },
    {
      .depthStencil = {1.0f, 0.0f},
    }
  };

  VkRenderPassBeginInfo rpInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .pNext = NULL,

    .renderPass = DATA(renderPass),
    .renderArea.offset.x = 0,
    .renderArea.offset.y = 0,
    .renderArea.extent.width = DATA(swapchain.windowExtent.width),
    .renderArea.extent.height = DATA(swapchain.windowExtent.height),
    .framebuffer = DATA(framebuffers[DATA(swapchainImageIndex)]),

    .clearValueCount = ARRAYSIZE(clearValues),
    .pClearValues = &clearValues,
  };

  vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  {
    VkRect2D scissor = {
      .offset = {0, 0},
      .extent = DATA(swapchain.windowExtent),
    };

    VkViewport viewport =
    {
      .x = 0,
      .y = DATA(swapchain.windowExtent.height),
      .width = DATA(swapchain.windowExtent.width),
      .height = -(float) DATA(swapchain.windowExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
  }

  return cmd;
}

void ev_vulkan_endframe(VkCommandBuffer cmd, uint32_t frameNumber)
{
  vkCmdEndRenderPass(cmd);

  VkImageMemoryBarrier imageMemoryBarrier1 =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .pNext = NULL,
    .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dstAccessMask = 0,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,  // TODO
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,  // TODO
    .image = DATA(swapchain).images[DATA(swapchainImageIndex)],
    .subresourceRange =
    {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    }
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &imageMemoryBarrier1);

  VK_ASSERT(vkEndCommandBuffer(cmd));

  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = NULL,
  };

  VkSemaphore waitSemaphores[] = {
    DATA(offscreenrendersemaphore)[frameNumber],
    DATA(swapchain).presentSemaphores[frameNumber],
  };

  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  submit.waitSemaphoreCount = ARRAYSIZE(waitStages);
  submit.pWaitDstStageMask = &waitStages;

  submit.waitSemaphoreCount = ARRAYSIZE(waitSemaphores);
  submit.pWaitSemaphores = waitSemaphores;

  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &DATA(swapchain).renderSemaphores[frameNumber];

  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, DATA(swapchain).renderFences[frameNumber]));

  VkPresentInfoKHR presentInfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = NULL,

    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &DATA(swapchain).renderSemaphores[frameNumber],

    .swapchainCount = 1,
    .pSwapchains = &DATA(swapchain).swapchain,

    .pImageIndices = &DATA(swapchainImageIndex),
  };

  VkResult result = vkQueuePresentKHR(VulkanQueueManager.getQueue(GRAPHICS), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    ev_vulkan_recreateSwapChain();
  } else if (result != VK_SUCCESS) {
    ev_log_debug("failed to present swap chain image!");
  }
}

VkRenderPass ev_vulkan_getrenderpass()
{
  return DATA(renderPass);
}

VkRenderPass ev_vulkan_getoffscreenrenderpass()
{
  return DATA(offscreenFrameBuffer).renderPass;
}

FrameBuffer *ev_vulkan_getoffscreenframebuffer()
{
  return &DATA(offscreenFrameBuffer);
}

void ev_vulkan_allocateimageinpool(VmaPool pool, uint32_t width, uint32_t height, unsigned long long usageFlags, EvImage *image)
{
    VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = usageFlags,
        .samples = VK_SAMPLE_COUNT_1_BIT,
      };

  VmaAllocationCreateInfo allocationCreateInfo = {
    .pool = pool,
  };

  ev_vulkan_createimage(&imageCreateInfo, &allocationCreateInfo, image);
}

void ev_vulkan_transitionimagelayout(EvImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
  VkCommandBuffer tempCommandBuffer;
  ev_vulkan_allocateprimarycommandbuffer(TRANSFER, &tempCommandBuffer);
  VkCommandBufferBeginInfo tempCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  tempCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(tempCommandBuffer, &tempCommandBufferBeginInfo);


  VkImageMemoryBarrier barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .oldLayout = oldLayout,
    .newLayout = newLayout,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = image.image,

    .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1,
  };

  VkPipelineStageFlags sourceStage = 0;
  VkPipelineStageFlags destinationStage = 0;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }

  vkCmdPipelineBarrier(
      tempCommandBuffer,
      sourceStage, destinationStage,
      0,
      0, NULL,
      0, NULL,
      1, &barrier
  );


  vkEndCommandBuffer(tempCommandBuffer);

  VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

  VkSubmitInfo submitInfo         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.pWaitDstStageMask    = &stageMask;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &tempCommandBuffer;
  submitInfo.waitSemaphoreCount   = 0;
  submitInfo.pWaitSemaphores      = NULL;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores    = NULL;

  VkQueue transferQueue = VulkanQueueManager.getQueue(TRANSFER);
  VK_ASSERT(vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE));

  //TODO Change this to take advantage of fences
  vkQueueWaitIdle(transferQueue);

  //vkFreeCommandBuffers(VulkanData.logicalDevice, DATA(transferCommandPool), 1, &tempCommandBuffer);
}

void ev_vulkan_copybuffertoimage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
  VkCommandBuffer tempCommandBuffer;
  ev_vulkan_allocateprimarycommandbuffer(TRANSFER, &tempCommandBuffer);
  VkCommandBufferBeginInfo tempCommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  tempCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(tempCommandBuffer, &tempCommandBufferBeginInfo);

  VkBufferImageCopy region = {
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,

    .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .imageSubresource.mipLevel = 0,
    .imageSubresource.baseArrayLayer = 0,
    .imageSubresource.layerCount = 1,
  };

  region.imageOffset.x = 0;
  region.imageOffset.y = 0;
  region.imageOffset.z = 0;

  region.imageExtent.depth = 1;
  region.imageExtent.height = height;
  region.imageExtent.width = width;

  vkCmdCopyBufferToImage(
      tempCommandBuffer,
      buffer,
      image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &region
  );


  vkEndCommandBuffer(tempCommandBuffer);

  VkPipelineStageFlags stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;

  VkSubmitInfo submitInfo         = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.pWaitDstStageMask    = &stageMask;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &tempCommandBuffer;
  submitInfo.waitSemaphoreCount   = 0;
  submitInfo.pWaitSemaphores      = NULL;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores    = NULL;

  VkQueue transferQueue = VulkanQueueManager.getQueue(TRANSFER);
  VK_ASSERT(vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE));

  //TODO Change this to take advantage of fences
  vkQueueWaitIdle(transferQueue);

  //vkFreeCommandBuffers(Vulkan.getDevice(), DATA(transferCommandPool), 1, &tempCommandBuffer);
}

EvTexture ev_vulkan_registerTexture(VkFormat format, uint32_t width, uint32_t height, void* pixels)
{
  uint32_t size = width * height * 4;

  EvImage newimage;
  ev_vulkan_allocateimageinpool(DATA(imagesPool), width, height , EV_USAGEFLAGS_RESOURCE_IMAGE, &newimage);
  EvBuffer imageStagingBuffer;
  ev_vulkan_allocatestagingbuffer(size, &imageStagingBuffer);
  ev_vulkan_updatestagingbuffer(&imageStagingBuffer, size, pixels);

  ev_vulkan_transitionimagelayout(newimage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  ev_vulkan_copybuffertoimage(imageStagingBuffer.buffer, newimage.image, width, height);
  ev_vulkan_transitionimagelayout(newimage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  ev_vulkan_destroybuffer(&imageStagingBuffer);

  VkImageView imageView;
  ev_vulkan_createimageview(format, &newimage.image, &imageView);

  VkSampler sampler;
  VkSamplerCreateInfo samplerInfo =
  {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .mipLodBias = 0.0f,
      .minLod = 0.0f,
      .maxLod = 0.0f,
  };
  vkCreateSampler(VulkanData.logicalDevice, &samplerInfo, NULL, &sampler);

  EvTexture evtexture = {
      .image = newimage,
      .imageView = imageView,
      .sampler = sampler
  };
  return evtexture;
}

void ev_vulkan_destroytexture(EvTexture *texture)
{
  vkDestroySampler(VulkanData.logicalDevice, texture->sampler, NULL);
  ev_vulkan_destroyimageview(texture->imageView);
  ev_vulkan_destroyimage(texture->image);
}
