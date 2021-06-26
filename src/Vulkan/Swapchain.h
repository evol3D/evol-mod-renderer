#pragma once

#include <Vulkan.h>
#include <evol/evol.h>
#include <Renderer_types.h>



void ev_swapchain_create(EvSwapchain *Swapchain, VkSurfaceKHR *surface);
void ev_swapchain_destroy(EvSwapchain *Swapchain);
