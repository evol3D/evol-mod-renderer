#pragma once

#include <Vulkan.h>
#include <evol/evol.h>
#include <Renderer_types.h>



void ev_swapchain_create(uint32_t framebuffering, EvSwapchain *Swapchain, VkSurfaceKHR *surface);
void ev_swapchain_destroy(EvSwapchain *Swapchain);
