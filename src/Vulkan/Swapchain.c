#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <Vulkan.h>
#include <evstr.h>
#include <evol/evol.h>
#include <evol/common/ev_globals.h>
#include <evol/common/ev_log.h>

void ev_swapchain_createcommandbuffers(EvSwapchain *Swapchain)
{
  VkCommandPool graphicsPool = ev_vulkan_getcommandpool(GRAPHICS);
  VkCommandBufferAllocateInfo commandBuffersAllocateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = graphicsPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = Swapchain->imageCount,
  };

  VK_ASSERT(vkAllocateCommandBuffers(ev_vulkan_getlogicaldevice(), &commandBuffersAllocateInfo, Swapchain->commandBuffers));
}
void ev_swapchain_destroycommandbuffers(EvSwapchain *Swapchain)
{
  vkFreeCommandBuffers(ev_vulkan_getlogicaldevice(), ev_vulkan_getcommandpool(GRAPHICS),
      Swapchain->imageCount, Swapchain->commandBuffers);
}

void ev_swapchain_createsyncstructures(EvSwapchain *Swapchain)
{
  VkSemaphoreCreateInfo semaphoreCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };

  VK_ASSERT(vkCreateSemaphore(ev_vulkan_getlogicaldevice(), &semaphoreCreateInfo, NULL, &Swapchain->presentSemaphore));
  VK_ASSERT(vkCreateSemaphore(ev_vulkan_getlogicaldevice(), &semaphoreCreateInfo, NULL, &Swapchain->submittionSemaphore));

  VkFenceCreateInfo fenceCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for(size_t i = 0; i < Swapchain->imageCount; ++i)
  {
    VK_ASSERT(vkCreateFence(ev_vulkan_getlogicaldevice(), &fenceCreateInfo, NULL, &Swapchain->frameSubmissionFences[i]));
  }
}
void ev_swapchain_destroysyncstructures(EvSwapchain *Swapchain)
{
  vkDestroySemaphore(ev_vulkan_getlogicaldevice(), Swapchain->presentSemaphore, NULL);
  vkDestroySemaphore(ev_vulkan_getlogicaldevice(), Swapchain->submittionSemaphore, NULL);

  for (size_t i = 0; i < Swapchain->imageCount; i++)
  {
    vkDestroyFence(ev_vulkan_getlogicaldevice(), Swapchain->frameSubmissionFences[i], NULL);
  }
}

void ev_swapchain_create(EvSwapchain *Swapchain)
{
  EvSwapchain oldSwapchain = *Swapchain;
  if (Swapchain->swapchain == NULL)
  {
    oldSwapchain.swapchain = VK_NULL_HANDLE;
  }

  //get this from config vars //double-buff or triple-buff
  //TODO: Actually detect the format
  Swapchain->imageCount = 3;
  Swapchain->surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
  Swapchain->surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

  //depth buffer
  {
    Swapchain->depthStencilFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    VkImageCreateInfo depthImageCreateInfo = {
      .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType     = VK_IMAGE_TYPE_2D,
      .format        = Swapchain->depthStencilFormat,
      .extent        = (VkExtent3D) {
        .width       = Swapchain->windowExtent.width,
        .height      = Swapchain->windowExtent.height,
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

    ev_vulkan_createimage(&depthImageCreateInfo, &vmaAllocationCreateInfo, &Swapchain->depthImage);

    VkImageViewCreateInfo depthImageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = Swapchain->depthImage.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = Swapchain->depthStencilFormat,
      .components = {0, 0, 0, 0},
      .subresourceRange = (VkImageSubresourceRange) {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(ev_vulkan_getlogicaldevice(), &depthImageViewCreateInfo, NULL, &Swapchain->depthImageView);
  }

  ev_vulkan_createswapchain(&Swapchain->imageCount, Swapchain->windowExtent, &Swapchain->surface, &Swapchain->surfaceFormat, oldSwapchain.swapchain, &Swapchain->swapchain);

  ev_vulkan_retrieveswapchainimages(Swapchain->swapchain, &Swapchain->imageCount, Swapchain->images);

  for (size_t i = 0; i < Swapchain->imageCount; i++)
    ev_vulkan_createimageview(Swapchain->surfaceFormat.format, Swapchain->images + i, Swapchain->imageViews + i);

  ev_swapchain_createcommandbuffers(Swapchain);

  ev_swapchain_createsyncstructures(Swapchain);

  //should i destroy now the old one or wait ?
  if (oldSwapchain.swapchain != VK_NULL_HANDLE)
  {
    Swapchain->surface = oldSwapchain.surface;
    ev_swapchain_destroy(&oldSwapchain);
  }
}
void ev_swapchain_destroy(EvSwapchain *Swapchain)
{
  ev_swapchain_destroysyncstructures(Swapchain);
  ev_swapchain_destroycommandbuffers(Swapchain);

  ev_vulkan_destroyimage(&Swapchain->depthImage);
  ev_vulkan_destroyimageview(Swapchain->depthImageView);

  for (size_t i = 0; i < Swapchain->imageCount; i++)
    vkDestroyImageView(ev_vulkan_getlogicaldevice(), Swapchain->imageViews[i], NULL);

  vkDestroySwapchainKHR(ev_vulkan_getlogicaldevice(), Swapchain->swapchain, NULL);

  ev_renderer_destroysurface(Swapchain->surface);
}
