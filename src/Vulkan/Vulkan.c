#include <Vulkan.h>

#include <stdlib.h>
#include <Vulkan_utils.h>

#include <evol/common/ev_log.h>

struct ev_Vulkan_Data {
  VkInstance       instance;
  VkSurfaceKHR     surface;
  VmaAllocator     allocator;
  VkApiVersion     apiVersion;
  VkDevice         logicalDevice;
  VkPhysicalDevice physicalDevice;

  VkCommandPool    commandPools[QUEUE_TYPE_COUNT];
} VulkanData;

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

  return 0;
}

int ev_vulkan_deinit()
{
  // Destroy any commandpool that was created earlier
  for(int i = 0; i < QUEUE_TYPE_COUNT; ++i)
    if(VulkanData.commandPools[i])
      vkDestroyCommandPool(VulkanData.logicalDevice, VulkanData.commandPools[i], NULL);

  // Destroy VMA
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
    "VK_LAYER_LUNARG_api_dump",
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
  unsigned int physicalDeviceCount = 0;
  vkEnumeratePhysicalDevices( VulkanData.instance, &physicalDeviceCount, NULL);

  if(!physicalDeviceCount)
    assert(!"No physical devices found");

  VkPhysicalDevice *physicalDevices = malloc(physicalDeviceCount * sizeof(VkPhysicalDevice));
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

  free(physicalDevices);
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

void ev_vulkan_setsurface(VkSurfaceKHR surface)
{
  VulkanData.surface = surface;
}

VkResult ev_vulkan_checksurface(VkSurfaceKHR surface)
{
  //Check that the surface is supported by the Graphics QueueFamily present

  VkBool32 surfaceSupported = VK_FALSE;
  vkGetPhysicalDeviceSurfaceSupportKHR(VulkanData.physicalDevice, VulkanQueueManager.getFamilyIndex(GRAPHICS), surface, &surfaceSupported);

  return surfaceSupported;
}

void ev_vulkan_destroysurface(VkSurfaceKHR surface)
{
  // Destroy the vulkan surface
  vkDestroySurfaceKHR(VulkanData.instance, surface, NULL);
}

void ev_vulkan_createswapchain(unsigned int* imageCount, VkSurfaceKHR* surface, VkSurfaceFormatKHR *surfaceFormat, VkSwapchainKHR* swapchain)
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;

  VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanData.physicalDevice, *surface, &surfaceCapabilities));

  unsigned int windowWidth  = surfaceCapabilities.currentExtent.width;
  unsigned int windowHeight = surfaceCapabilities.currentExtent.height;

  //TODO
  // if(windowWidth == UINT32_MAX || windowHeight == UINT32_MAX)
  //   Window->getSize(VulkanData.WindowHandle, &windowWidth, &windowHeight);

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

void ev_vulkan_createframebuffer(VkImageView* attachments, unsigned int attachmentCount, VkRenderPass renderPass, VkFramebuffer *framebuffer)
{
  unsigned int windowWidth, windowHeight;
  //TODO
  //Window->getSize(VulkanData.WindowHandle, &windowWidth, &windowHeight);

  VkFramebufferCreateInfo swapchainFramebufferCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .renderPass = renderPass,
    .attachmentCount = attachmentCount,
    .pAttachments = attachments,
    .width = windowWidth,
    .height = windowHeight,
    .layers = 1,
  };
  VK_ASSERT(vkCreateFramebuffer(VulkanData.logicalDevice, &swapchainFramebufferCreateInfo, NULL, framebuffer));
}

void ev_vulkan_retrieveswapchainimages(VkSwapchainKHR swapchain, unsigned int * imageCount, VkImage ** images)
{
  VK_ASSERT(vkGetSwapchainImagesKHR(VulkanData.logicalDevice, swapchain, imageCount, NULL));

  *images = malloc(sizeof(VkImage) * (*imageCount));
  if(!images)
  assert(!"Couldn't allocate memory for the swapchain images!");

  VK_ASSERT(vkGetSwapchainImagesKHR(VulkanData.logicalDevice, swapchain, imageCount, *images));
}

void ev_vulkan_destroyswapchain(VkSwapchainKHR swapchain)
{
  vkDestroySwapchainKHR(VulkanData.logicalDevice, swapchain, NULL);
}

void ev_vulkan_allocatememorypool(VmaPoolCreateInfo *poolCreateInfo, VmaPool* pool)
{
  VK_ASSERT(vmaCreatePool(VulkanData.allocator, poolCreateInfo, pool));
}

void ev_vulkan_allocatebufferinpool(VkBufferCreateInfo *bufferCreateInfo, VmaPool pool, EvBuffer *buffer)
{
  VmaAllocationCreateInfo allocationCreateInfo = {
    .pool = pool,
  };

  ev_vulkan_createbuffer(bufferCreateInfo, &allocationCreateInfo, buffer);
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
  VkCommandBufferAllocateInfo cmdBufferAllocateInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = VulkanData.commandPools[queueType],
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  vkAllocateCommandBuffers(VulkanData.logicalDevice, &cmdBufferAllocateInfo, cmdBuffer);
}

void ev_vulkan_createimage(VkImageCreateInfo *imageCreateInfo, VmaAllocationCreateInfo *allocationCreateInfo, EvImage *image)
{
  vmaCreateImage(VulkanData.allocator, imageCreateInfo, allocationCreateInfo, &(image->image), &(image->allocation), &(image->allocationInfo));
}

void ev_vulkan_createimageviews(unsigned int imageCount, VkFormat imageFormat, VkImage *images, VkImageView **views)
{
  *views = malloc(sizeof(VkImageView) * imageCount);
  for (unsigned int i = 0; i < imageCount; ++i)
  {
    VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = images[i],
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

    vkCreateImageView(VulkanData.logicalDevice, &imageViewCreateInfo, NULL, *views + i);
  }
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
