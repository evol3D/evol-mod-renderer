#include <Vulkan.h>

#include <Vulkan_utils.h>
#include <DescriptorManager.h>
#include <evol/common/ev_log.h>

#define EV_USAGEFLAGS_RESOURCE_BUFFER VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT

struct ev_Vulkan_Data {
  VkInstance       instance;
  VkApiVersion     apiVersion;
  VkDevice         logicalDevice;
  VkPhysicalDevice physicalDevice;

  VmaPool storagePool;
  VmaAllocator     allocator;

  VkCommandPool    commandPools[QUEUE_TYPE_COUNT];
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

  ev_vulkan_createresourcememorypool(128ull * 1024 * 1024, 1, 4, &DATA(storagePool));

  ev_descriptormanager_init();

  return 0;
}

int ev_vulkan_deinit()
{
  // Destroy any commandpool that was created earlier
  for(int i = 0; i < QUEUE_TYPE_COUNT; ++i)
    if(VulkanData.commandPools[i])
      vkDestroyCommandPool(VulkanData.logicalDevice, VulkanData.commandPools[i], NULL);

  ev_descriptormanager_dinit();

  // Destroy VMA
  ev_vulkan_freememorypool(DATA(storagePool));
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
    "VK_LAYER_LUNARG_monitor",
    //"VK_LAYER_LUNARG_api_dump",
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

void ev_vulkan_checksurfacecompatibility(VkSurfaceKHR surface)
{
  VkResult res;

  VkBool32 surfaceSupported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(VulkanData.physicalDevice, VulkanQueueManager.getFamilyIndex(GRAPHICS), surface, &surfaceSupported);

  res = surfaceSupported == VK_FALSE ? !VK_SUCCESS : VK_SUCCESS;

  VK_ASSERT(res);
}

void ev_vulkan_destroysurface(VkSurfaceKHR surface)
{
  // Destroy the vulkan surface
  vkDestroySurfaceKHR(VulkanData.instance, surface, NULL);
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
    .presentMode      = VK_PRESENT_MODE_FIFO_KHR, // TODO: Make sure that this is always supported
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

void ev_vulkan_destroyimage(EvImage *image)
{
  vmaDestroyImage(VulkanData.allocator, image->image, image->allocation);
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

void ev_vulkan_writeintobinding(DescriptorSet set, Binding *binding, void *data)
{
  VkWriteDescriptorSet setWrite;

  switch(binding->type)
  {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      {
        VkDescriptorBufferInfo bufferInfo = {
          .buffer = ((EvBuffer*)data)->buffer,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
        };
        setWrite = (VkWriteDescriptorSet){
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .descriptorCount = 1,
          .descriptorType = binding->type,
          .dstSet = set.set,
          .dstBinding = binding->binding,
          .dstArrayElement = binding->writtenSetsCount,
          .pBufferInfo = &bufferInfo,
        };
        break;
      }
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        // d.imageInfo = (VkDescriptorImageInfo){
        //   .imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        //   .imageInfo.imageView = ((EvTexture*)data)->imageView,
        //   .imageInfo.sampler = ((EvTexture*)descriptors[i].descriptorData)->sampler,
        // }
        //
        // setWrites[i] = (VkWriteDescriptorSet)
        // {
        //   .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        //   .dstSet = descriptorSet,
        //   .dstBinding = 0,
        //   .dstArrayElement = i,
        //   .descriptorType = (VkDescriptorType)descriptors[i].type,
        //   .descriptorCount = 1,
        //   .pImageInfo = &imageInfos[i]
        // };
        // break;
    default:
      ;
  }

  binding->writtenSetsCount++;
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

void ev_vulkan_createresourcememorypool(unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool)
{
  unsigned int memoryType;

  { // Detecting memorytype index
    VkBufferCreateInfo sampleBufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    sampleBufferCreateInfo.size = 1024;
    sampleBufferCreateInfo.usage = EV_USAGEFLAGS_RESOURCE_BUFFER;

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
  ev_vulkan_allocatebufferinpool(DATA(storagePool), size, EV_USAGEFLAGS_RESOURCE_BUFFER, &newBuffer);

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
