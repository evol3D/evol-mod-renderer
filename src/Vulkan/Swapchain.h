#pragma once

#include <Vulkan.h>
#include <evol/evol.h>

#define SWAPCHAIN_MAX_IMAGES 5

typedef struct {
  VkSurfaceKHR surface;
  VkExtent2D windowExtent;

  uint32_t imageCount;
  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surfaceFormat;

  EvImage depthImage;
  VkImageView depthImageView;
  VkFormat depthStencilFormat;
  VkDeviceMemory depthImageMemory;

  VkImage images[SWAPCHAIN_MAX_IMAGES];
  VkImageView imageViews[SWAPCHAIN_MAX_IMAGES];
  VkCommandBuffer commandBuffers[SWAPCHAIN_MAX_IMAGES];

  VkSemaphore presentSemaphore;
  VkSemaphore submittionSemaphore;
  VkFence frameSubmissionFences[SWAPCHAIN_MAX_IMAGES];
} EvSwapchain;

void ev_swapchain_create(EvSwapchain *Swapchain);
void ev_swapchain_destroy(EvSwapchain *Swapchain);
