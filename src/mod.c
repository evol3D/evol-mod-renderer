#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <Vulkan.h>
#include <Pipeline.h>
#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <VulkanQueueManager.h>

#define EV_WINDOW_VULKAN_SUPPORT
#define NAMESPACE_MODULE evmod_glfw
#include <evol/meta/namespace_import.h>
#define EVENT_MODULE evmod_glfw
#include <evol/meta/event_include.h>

#define TYPE_MODULE evmod_assetsystem
#include <evol/meta/type_import.h>
#define NAMESPACE_MODULE evmod_assetsystem
#include <evol/meta/namespace_import.h>

#define DATA(X) RendererData.X

struct ev_Renderer_Data
{
  WindowHandle windowHandle;

  EvSwapchain Swapchain;

  VkFramebuffer framebuffers[SWAPCHAIN_MAX_IMAGES];

  VkRenderPass renderPass;

  VkPipeline defaultPipeline;
} RendererData;

void run();
void createSurface();
void createPipelines();
void recreateSwapChain();
void ev_renderer_createrenderpass();
void ev_renderer_updatewindowsize();
void ev_renderer_createframebuffers();
void setWindow(WindowHandle handle);

void ev_renderer_destroyrenderpass();
void ev_renderer_destroyframebuffer();
void ev_renderer_destroypipeline(VkPipeline pipeline);
void ev_renderer_destroysurface();

void ev_renderer_createrenderpass()
{
  VkAttachmentDescription attachmentDescriptions[] =
  {
    {
      .format = DATA(Swapchain).surfaceFormat.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    // {
    //   .format = depthStencilFormat,
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
      //.pDepthStencilAttachment = &depthStencilAttachmentReference,
      .pDepthStencilAttachment = NULL,
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

  VK_ASSERT(vkCreateRenderPass(ev_vulkan_getlogicaldevice(), &renderPassCreateInfo, NULL, &DATA(renderPass)));
}

void ev_renderer_createframebuffers()
{
  for(size_t i = 0; i < DATA(Swapchain).imageCount; ++i)
  {
    VkImageView attachments[] =
    {
      DATA(Swapchain).imageViews[i],
      //depthBufferImageView
    };

    ev_vulkan_createframebuffer(attachments, ARRAYSIZE(attachments), DATA(renderPass), DATA(Swapchain.windowExtent), DATA(framebuffers + i));
  }
}

void createPipelines()
{
  evstring shaderCodes[] = {
    AssetSystem->load_shader("default.vert.spv"),
    AssetSystem->load_shader("default.frag.spv")
  };

  VkShaderStageFlagBits stageFlags[] = {
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  EvGraphicsPipelineCreateInfo p = {
    .stageCount = ARRAYSIZE(shaderCodes),
    .pShaderCodes = shaderCodes,
    .pStageFlags = stageFlags,
    .renderPass = DATA(renderPass),
  };

  ev_pipeline_build(p, &DATA(defaultPipeline));
}

void ev_renderer_updatewindowsize()
{
  Window->getSize(DATA(windowHandle), &DATA(Swapchain.windowExtent.width), &DATA(Swapchain.windowExtent.height));
}

void ev_renderer_createSurface()
{
  VK_ASSERT(Window->createVulkanSurface(DATA(windowHandle), ev_vulkan_getinstance(), &DATA(Swapchain.surface)));
  ev_vulkan_checksurfacecompatibility(DATA(Swapchain.surface));
}

void setWindow(WindowHandle handle)
{
  DATA(windowHandle) = handle;
  //TODO handle setting another window ??
  ev_renderer_updatewindowsize();

  ev_renderer_createSurface();
  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createrenderpass();
  ev_renderer_createframebuffers();
  createPipelines();
}

void recreateSwapChain() {
  vkDeviceWaitIdle(ev_vulkan_getlogicaldevice());

  ev_renderer_destroyframebuffer();

  ev_renderer_updatewindowsize();

  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createframebuffers();
}

void run()
{
  uint32_t swapchainImageIndex = 0;
  VkResult result = vkAcquireNextImageKHR(ev_vulkan_getlogicaldevice(), DATA(Swapchain).swapchain, 1000000000, DATA(Swapchain).presentSemaphore, NULL, &swapchainImageIndex);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    ev_log_debug("failed to acquire swap chain image!");
  }

  VK_ASSERT(VkResult v = vkWaitForFences(ev_vulkan_getlogicaldevice(), 1, &DATA(Swapchain).frameSubmissionFences[swapchainImageIndex], true, ~0ull));
  VK_ASSERT(vkResetFences(ev_vulkan_getlogicaldevice(), 1, &DATA(Swapchain).frameSubmissionFences[swapchainImageIndex]));
  //request image from the swapchain, one second timeout


  //now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
  VK_ASSERT(vkResetCommandBuffer(DATA(Swapchain).commandBuffers[swapchainImageIndex], 0));

  //naming it cmd for shorter writing
	VkCommandBuffer cmd = DATA(Swapchain).commandBuffers[swapchainImageIndex];

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
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
    .image = DATA(Swapchain).images[swapchainImageIndex],
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

  VkClearValue clearValues =
  {
      .color = { {0.3f, 0.3f, 0.3f, 1.f} }
  };

	//start the main renderpass.
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = {
  	.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
  	.pNext = NULL,

  	.renderPass = DATA(renderPass),
  	.renderArea.offset.x = 0,
  	.renderArea.offset.y = 0,
  	.renderArea.extent.width = DATA(Swapchain.windowExtent.width),
    .renderArea.extent.height = DATA(Swapchain.windowExtent.height),
  	.framebuffer = DATA(framebuffers[swapchainImageIndex]),

  	//connect clear values
  	.clearValueCount = 1,
  	.pClearValues = &clearValues,
  };

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  //DO Something
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, DATA(defaultPipeline));

  {
    VkRect2D scissor = {
      .offset = {0, 0},
      .extent = DATA(Swapchain.windowExtent),
    };

    VkViewport viewport =
    {
      .x = 0,
      .y = DATA(Swapchain.windowExtent.height),
      .width = DATA(Swapchain.windowExtent.width),
      .height = -(float) DATA(Swapchain.windowExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
  }

  vkCmdDraw(cmd, 3, 1, 0, 0);

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
    .image = DATA(Swapchain).images[swapchainImageIndex],
    .subresourceRange =
    {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
    }
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &imageMemoryBarrier1);


  //finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_ASSERT(vkEndCommandBuffer(cmd));

  //prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = {
  	.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
  	.pNext = NULL,
  };

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &DATA(Swapchain).presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &DATA(Swapchain).submittionSemaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, DATA(Swapchain).frameSubmissionFences[swapchainImageIndex]));

  // this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {
  	.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
  	.pNext = NULL,

  	.pSwapchains = &DATA(Swapchain).swapchain,
  	.swapchainCount = 1,

  	.pWaitSemaphores = &DATA(Swapchain).submittionSemaphore,
  	.waitSemaphoreCount = 1,

  	.pImageIndices = &swapchainImageIndex,
  };

	result = vkQueuePresentKHR(VulkanQueueManager.getQueue(GRAPHICS), &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    recreateSwapChain();
  } else if (result != VK_SUCCESS) {
      ev_log_debug("failed to present swap chain image!");
  }
}

void ev_renderer_destroyrenderpass()
{
  vkDestroyRenderPass(ev_vulkan_getlogicaldevice(), DATA(renderPass), NULL);
}
void ev_renderer_destroypipeline(VkPipeline pipeline)
{
  vkDestroyPipeline(ev_vulkan_getlogicaldevice(), pipeline, NULL);
}
void ev_renderer_destroysurface()
{
  vkDestroySurfaceKHR(ev_vulkan_getinstance(), DATA(Swapchain.surface), NULL);
}
void ev_renderer_destroyframebuffer()
{
  for (size_t i = 0; i < DATA(Swapchain).imageCount; i++)
  {
    vkDestroyFramebuffer(ev_vulkan_getlogicaldevice(), DATA(framebuffers[i]), NULL);
  }
}

EV_CONSTRUCTOR
{
  evolmodule_t asset_module   = evol_loadmodule("asset-importer"); DEBUG_ASSERT(asset_module);
  IMPORT_NAMESPACE(AssetSystem, asset_module);

  evolmodule_t window_module  = evol_loadmodule("window");  DEBUG_ASSERT(window_module);
  IMPORT_NAMESPACE(Window, window_module);

  ev_vulkan_init();
}

EV_DESTRUCTOR
{
  ev_renderer_destroypipeline(DATA(defaultPipeline));
  ev_renderer_destroyframebuffer();
  ev_renderer_destroyrenderpass();
  ev_swapchain_destroy(&DATA(Swapchain));
  ev_renderer_destroysurface();

  ev_vulkan_deinit();
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
  EV_NS_BIND_FN(Renderer, run, run);
}
