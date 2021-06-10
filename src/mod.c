#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <Vulkan.h>
#include <Pipeline.h>
#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <Renderer_types.h>
#include <DescriptorManager.h>
#include <VulkanQueueManager.h>

#define EV_WINDOW_VULKAN_SUPPORT
#define IMPORT_MODULE evmod_glfw
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_assets
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_game
#include IMPORT_MODULE_H

#define DATA(X) RendererData.X

struct ev_Renderer_Data
{
  WindowHandle windowHandle;
  EvSwapchain Swapchain;
  VkFramebuffer framebuffers[SWAPCHAIN_MAX_IMAGES];
  VkRenderPass renderPass;
  VmaPool storagePool;

  DescriptorSet cameraSet;
  UBO cameraBuffer;

  vec(EvRenderObject) objects;
  vec(Pipeline) pipelines;
  vec(Material) materials;

  vec(EvBuffer) vertexBuffers;
  vec(EvBuffer) indexBuffers;
  vec(EvBuffer) materialBuffers;
  vec(EvBuffer) customBuffers;
} RendererData;

evolmodule_t game_module;
evolmodule_t asset_module;
evolmodule_t window_module;

EvRenderObject triangleObj;

void run();
void recreateSwapChain();
void ev_renderer_createSurface();
void ev_renderer_createrenderpass();
void ev_renderer_updatewindowsize();
void ev_renderer_createframebuffers();
void setWindow(WindowHandle handle);

void ev_renderer_destroyrenderpass();
void ev_renderer_destroyframebuffer();
void ev_renderer_destroypipeline(VkPipeline pipeline);
void ev_renderer_destroysurface();

void ev_renderer_createresourcememorypool(unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool);
void ev_rendererbackend_allocatebufferinpool(VmaPool pool, unsigned long long bufferSize, unsigned long long usageFlags, EvBuffer *buffer);
void ev_rendererbackend_updatestagingbuffer(EvBuffer *buffer, unsigned long long bufferSize, const void *data);
void ev_rendererbackend_allocatestagingbuffer(unsigned long long bufferSize, EvBuffer *buffer);
int ev_renderer_registervertexbuffer(void *vertices, unsigned long long size, vec(EvBuffer) vertexBuffers);
void ev_rendererbackend_copybuffer(unsigned long long size, EvBuffer *src, EvBuffer *dst);

void ev_rendererbackend_freeubo(UBO *ubo);

#define EV_USAGEFLAGS_RESOURCE_BUFFER VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT

uint32_t ev_renderer_registerbuffer(void *data, unsigned long long size, vec(EvBuffer) *buffers)
{
  EvBuffer newBuffer;
  ev_rendererbackend_allocatebufferinpool(DATA(storagePool), size, EV_USAGEFLAGS_RESOURCE_BUFFER, &newBuffer);

  EvBuffer newStagingBuffer;
  ev_rendererbackend_allocatestagingbuffer(size, &newStagingBuffer);
  ev_rendererbackend_updatestagingbuffer(&newStagingBuffer, size, data);
  ev_rendererbackend_copybuffer(size, &newStagingBuffer, &newBuffer);

  ev_vulkan_destroybuffer(&newStagingBuffer);

  return vec_push(buffers, &newBuffer);
}

void UpdateDescriptorSet(DescriptorSet set, Binding *binding, void *data)
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
  vkUpdateDescriptorSets(ev_vulkan_getlogicaldevice(), 1, &setWrite, 0, NULL);
}















void ev_rendererbackend_allocateubo(unsigned long long bufferSize, bool persistentMap, UBO *ubo)
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

void ev_rendererbackend_freeubo(UBO *ubo)
{
  if(ubo->mappedData)
    vmaUnmapMemory(ev_vulkan_getlogicaldevice(), ubo->buffer.allocation);
  ev_vulkan_destroybuffer(&ubo->buffer);
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
    {
      .format = DATA(Swapchain).depthStencilFormat,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    }
  };

  VkAttachmentReference colorAttachmentReferences[] =
  {
    {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
  };

  VkAttachmentReference depthStencilAttachmentReference =
  {
    .attachment = 1,
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
      DATA(Swapchain).depthImageView,
    };

    ev_vulkan_createframebuffer(attachments, ARRAYSIZE(attachments), DATA(renderPass), DATA(Swapchain.windowExtent), DATA(framebuffers + i));
  }
}





void ev_rendererbackend_updateubo(unsigned long long bufferSize, const void *data, UBO *ubo)
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









void recreateSwapChain()
{
  vkDeviceWaitIdle(ev_vulkan_getlogicaldevice());

  ev_renderer_destroyframebuffer();

  ev_renderer_updatewindowsize();

  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createframebuffers();
}

void draw(VkCommandBuffer cmd)
{
  CameraData cam;
  Camera->getViewMat(NULL, NULL, cam.viewMat);
  Camera->getProjectionMat(NULL, NULL, cam.projectionMat);

  ev_rendererbackend_updateubo(sizeof(CameraData), &cam, &DATA(cameraBuffer));

  for (size_t objectIndex = 0; objectIndex < vec_len(DATA(objects)); objectIndex++)
  {
    EvRenderObject object = DATA(objects)[objectIndex];
    Pipeline pipeline = DATA(pipelines)[object.pipelineIndex];

    MeshPushConstants pushconstant;
    GameObject player = Scene->getObject(NULL, "Player");
    glm_mat4_dup(*Object->getWorldTransform(NULL, player), pushconstant.tranform);
    pushconstant.meshIndex = object.meshIndex;

    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushconstant);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    VkDescriptorSet ds[4];
    for (size_t i = 1; i < vec_len(pipeline.pSets); i++)
    {
      ds[i] = pipeline.pSets[i].set;
    }
    ds[0] = DATA(cameraSet).set;
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, vec_len(pipeline.pSets), ds, 0, 0);

    vkCmdDraw(cmd, object.mesh.indexCount, 1, 0, 0);
  }
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


  //now that we are sure tvoid ev_renderer_registerMaterial()hat the commands finished executing, we can safely reset the command buffer to begin recording again.
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

  VkClearValue clearValues[] =
  {
    {
      .color = { {0.13f, 0.22f, 0.37f, 1.f} },
    },
    {
      .depthStencil = {1.0f, 0.0f},
    }
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
  	.clearValueCount = ARRAYSIZE(clearValues),
  	.pClearValues = &clearValues,
  };

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

  //DO Something
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

  draw(cmd);

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











//TODO thes should be reallocated
//TODO this is hard coded for storage buffers
void ev_renderer_createresourcememorypool(unsigned long long blockSize, unsigned int minBlockCount, unsigned int maxBlockCount, VmaPool *pool)
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

void ev_rendererbackend_allocatebufferinpool(VmaPool pool, unsigned long long bufferSize, unsigned long long usageFlags, EvBuffer *buffer)
{
  VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferCreateInfo.size = bufferSize;
  bufferCreateInfo.usage = usageFlags;

  ev_vulkan_allocatebufferinpool(&bufferCreateInfo, pool, buffer);
}

void ev_rendererbackend_updatestagingbuffer(EvBuffer *buffer, unsigned long long bufferSize, const void *data)
{
  void *mapped;
  vmaMapMemory(ev_vulkan_getvmaallocator(), buffer->allocation, &mapped);
  memcpy(mapped, data, bufferSize);
  vmaUnmapMemory(ev_vulkan_getvmaallocator(), buffer->allocation);
}

void ev_rendererbackend_allocatestagingbuffer(unsigned long long bufferSize, EvBuffer *buffer)
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

void ev_rendererbackend_copybuffer(unsigned long long size, EvBuffer *src, EvBuffer *dst)
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
























void registerBuffer(ShaderData *shaderData)
{
  switch (shaderData->resourceType)
  {
    uint32_t resourceIndex;

    case VERTEXRESOURCE:
      shaderData->index = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, &DATA(vertexBuffers));
      shaderData->data = &DATA(vertexBuffers)[shaderData->index];
      break;
    case INDEXRESOURCE:
      shaderData->index = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, &DATA(indexBuffers));
      shaderData->data = &DATA(indexBuffers)[shaderData->index];
      break;
    case MATERIALRESOURCE:
      shaderData->index = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, &DATA(materialBuffers));
      shaderData->data = &DATA(materialBuffers)[shaderData->index];
      break;
    case CUSTOMBUFFER:
      shaderData->index = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, &DATA(customBuffers));
      shaderData->data = &DATA(customBuffers)[shaderData->index];
      break;
  }
}

bool isnewPipeline()
{
  return true;
}

void destroyMaterial(Pipeline *material)
{
  vkDestroyPipeline(ev_vulkan_getlogicaldevice(), material->pipeline, NULL);
  vkDestroyPipelineLayout(ev_vulkan_getlogicaldevice(), material->pipelineLayout, NULL);

  for (size_t i = 0; i < vec_len(material->pSets); i++)
  {
    vkDestroyDescriptorSetLayout(ev_vulkan_getlogicaldevice(), material->pSets[i].layout, NULL);
  }
}

void ev_renderer_registerObject(MeshAsset mesh, Material material, vec(Shader) shaders, uint32_t customDataCount ,ShaderData *customShaderData)
{
  EvRenderObject object = { 0 };
  object.mesh.indexCount = mesh.indexCount;
  object.mesh.vertexCount = mesh.vertexCount;

  if(isnewPipeline())
  {
    Pipeline newPipeline;
    newPipeline.pSets = vec_init(DescriptorSet);

    EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
      .stageCount = vec_len(shaders),
      .pShaders = shaders,
      .renderPass = DATA(renderPass),
    };
    ev_pipeline_build(pipelineCreateInfo, &newPipeline);

    for (size_t i = 0; i < vec_len(newPipeline.pSets); i++)
      ev_descriptormanager_allocate(newPipeline.pSets[i].layout, &newPipeline.pSets[i].set);

    object.pipelineIndex = vec_push(&DATA(pipelines), &newPipeline);
  }

  ShaderData vertexData = {
    .shaderBindingName = evstring_new("VertexBuffers"),
    .resourceType = VERTEXRESOURCE,
    .bufferize = mesh.vertexBuferSize,
    .data = mesh.vertexData,
  };
  registerBuffer(&vertexData);

  ShaderData indexData = {
    .shaderBindingName = evstring_new("IndexBuffers"),
    .resourceType = INDEXRESOURCE,
    .bufferize = mesh.indexBuferSize,
    .data = mesh.indexData,
  };
  registerBuffer(&indexData);

  ShaderData materialData = {
    .shaderBindingName = evstring_new("MaterialBuffers"),
    .resourceType = MATERIALRESOURCE,
    .bufferize = sizeof(material),
    .data = &material,
  };
  registerBuffer(&materialData);

  ShaderMesh m = {
    .indexBufferIndex = indexData.index,
    .vertexBufferIndex = vertexData.index,
  };
  ShaderData meshData = {
    .shaderBindingName = evstring_new("MeshBuffers"),
    .resourceType = CUSTOMBUFFER,
    .bufferize = sizeof(Material),
    .data = &m,
  };
  registerBuffer(&meshData);

  //TODO add custom data
  ShaderData defaultData[] = {
    indexData,
    vertexData,
    meshData,
    materialData,
  };

  //TODO change to map
  for (size_t i = 0; i < vec_len(DATA(pipelines)[object.pipelineIndex].pSets); i++)
  {
    for (size_t j = 0; j < vec_len(DATA(pipelines)[object.pipelineIndex].pSets[i].pBindings); j++)
    {
      for (size_t k = 0; k < ARRAYSIZE(defaultData); k++)
      {
        if (!strcmp(DATA(pipelines)[object.pipelineIndex].pSets[i].pBindings[j].bindingName, defaultData[k].shaderBindingName))
        {
          ev_log_debug("set: %d, binding: %d writing", i, j);
          UpdateDescriptorSet(DATA(pipelines)[object.pipelineIndex].pSets[i], &DATA(pipelines)[object.pipelineIndex].pSets[i].pBindings[j], defaultData[k].data);
        }
      }
    }
  }

  object.meshIndex = meshData.index;
  vec_push(&DATA(objects), &object);
}

void addObject()
{
  AssetHandle avocado_asset = Asset->load("project://Avocado.mesh");
  MeshAsset avocado_mesh = MeshLoader->loadAsset(avocado_asset);

  AssetHandle vertShader_asset = Asset->load("shaders://default.vert");
  ShaderAsset vertShader_desc = ShaderLoader->loadAsset(vertShader_asset, EV_SHADERASSETSTAGE_VERTEX, "default.vert", NULL, EV_SHADER_BIN);

  AssetHandle fragShader_asset = Asset->load("shaders://default.frag");
  ShaderAsset fragShader_desc = ShaderLoader->loadAsset(fragShader_asset, EV_SHADERASSETSTAGE_FRAGMENT, "default.frag", NULL, EV_SHADER_BIN);

  vec(Shader) shaders = vec_init(Shader);
  vec_push(&shaders, &(Shader){
    .data = vertShader_desc.binary,
    .length = vertShader_desc.len,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
  });
  vec_push(&shaders, &(Shader){
    .data = fragShader_desc.binary,
    .length = fragShader_desc.len,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
  });

  ShaderData customShaderData[] = {
    // {evstring_new("MeshBuffers"),     CUSTOMBUFFER, sizeof(Material), &m},
    {evstring_new("ResourceBuffers"), VERTEXRESOURCE, avocado_mesh.vertexBuferSize, avocado_mesh.vertexData},
    // {evstring_new("PositionBuffers"), CUSTOMBUFFER, avocado_mesh.indexBuferSize, avocado_mesh.indexData},
  };

  Material material = {
    .baseColor = {1.0, 1.0, 1.0},
  };

  ev_renderer_registerObject(avocado_mesh, material, shaders, ARRAYSIZE(customShaderData), customShaderData);

  vec_fini(shaders);

  Asset->free(avocado_asset);
  Asset->free(vertShader_asset);
  Asset->free(fragShader_asset);
}

void setWindow(WindowHandle handle)
{
  DATA(windowHandle) = handle;

  ev_renderer_updatewindowsize();

  ev_renderer_createSurface();
  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createrenderpass();
  ev_renderer_createframebuffers();

  addObject();
}

void ev_renderer_globalsetsinit()
{
  ev_rendererbackend_allocateubo(sizeof(CameraData), 0, &DATA(cameraBuffer));

  VkDescriptorSetLayoutBinding bindings[] =
  {
    {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    }
  };
  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = ARRAYSIZE(bindings),
    .pBindings = bindings
  };

  DATA(cameraSet).pBindings = vec_init(Binding);
  vec_push(&DATA(cameraSet).pBindings, &(Binding) {
    .binding = 0,
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  });

  VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &descriptorSetLayoutCreateInfo, NULL, &DATA(cameraSet).layout));

  ev_descriptormanager_allocate(DATA(cameraSet).layout, &DATA(cameraSet).set);

  for (size_t i = 0; i < vec_len(DATA(cameraSet).pBindings); i++)
  {
    UpdateDescriptorSet(DATA(cameraSet), &DATA(cameraSet).pBindings[i], &(DATA(cameraBuffer).buffer));
  }
}

EV_CONSTRUCTOR
{
  game_module = evol_loadmodule("game");          DEBUG_ASSERT(game_module);
  imports(game_module  , (Game, Camera, Object, Scene));

  asset_module = evol_loadmodule("assetmanager"); DEBUG_ASSERT(asset_module);
  imports(asset_module  , (AssetManager, Asset, MeshLoader, ShaderLoader));

  window_module  = evol_loadmodule("window");     DEBUG_ASSERT(window_module);
  imports(window_module, (Window));

  DATA(objects) = vec_init(EvRenderObject);
  DATA(pipelines) = vec_init(Pipeline, NULL, destroyMaterial);
  DATA(materials) = vec_init(Material, NULL, destroyMaterial);

  DATA(vertexBuffers) = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  DATA(indexBuffers) = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  DATA(materialBuffers) = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  DATA(customBuffers) = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);

  ev_vulkan_init();

  ev_descriptormanager_init();

  ev_renderer_createresourcememorypool(128ull * 1024 * 1024, 1, 4, &DATA(storagePool));

  ev_renderer_globalsetsinit();
}

EV_DESTRUCTOR
{
  vkWaitForFences(ev_vulkan_getlogicaldevice(), DATA(Swapchain).imageCount, DATA(Swapchain).frameSubmissionFences, VK_TRUE, ~0ull);

  ev_rendererbackend_freeubo(&DATA(cameraBuffer));

  vec_fini(DATA(materialBuffers));
  vec_fini(DATA(customBuffers));
  vec_fini(DATA(vertexBuffers));
  vec_fini(DATA(indexBuffers));
  vec_fini(DATA(pipelines));
  vec_fini(DATA(objects));

  ev_renderer_destroyframebuffer();
  ev_renderer_destroyrenderpass();
  ev_swapchain_destroy(&DATA(Swapchain));
  ev_renderer_destroysurface();

  ev_vulkan_freememorypool(DATA(storagePool));
  ev_descriptormanager_dinit();
  ev_vulkan_deinit();

  evol_unloadmodule(window_module);
  evol_unloadmodule(asset_module);
  evol_unloadmodule(game_module);
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
  EV_NS_BIND_FN(Renderer, run, run);
}
