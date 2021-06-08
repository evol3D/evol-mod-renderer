#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <Vulkan.h>
#include <Pipeline.h>
#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <Renderer_types.h>
#include <DescriptorManager.h>
#include <VulkanQueueManager.h>

#include <cglm/cglm.h>

#define EV_WINDOW_VULKAN_SUPPORT
#define NAMESPACE_MODULE evmod_glfw
#include <evol/meta/namespace_import.h>
#define EVENT_MODULE evmod_glfw
#include <evol/meta/event_include.h>

#define IMPORT_MODULE evmod_assets
#include IMPORT_MODULE_H

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

  vec(RendererMaterial) material;

  VmaPool storagePool;
} RendererData;

EvRenderObject triangleObj;
// EvMesh triangleMesh;

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

#define EV_USAGEFLAGS_RESOURCE_BUFFER VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT

int ev_renderer_registerbuffer(void *data, unsigned long long size, vec(EvBuffer) buffers)
{
  unsigned int idx = vec_len(buffers);

  EvBuffer newBuffer;
  ev_rendererbackend_allocatebufferinpool(DATA(storagePool), size, EV_USAGEFLAGS_RESOURCE_BUFFER, &newBuffer);

  EvBuffer newStagingBuffer;
  ev_rendererbackend_allocatestagingbuffer(size, &newStagingBuffer);
  ev_rendererbackend_updatestagingbuffer(&newStagingBuffer, size, data);
  ev_rendererbackend_copybuffer(size, &newStagingBuffer, &newBuffer);

  //TODO We should have a system that controls staging buffers
  // freeMemoryBuffer(&vertexStagingBuffer);

  vec_push(&buffers, &newBuffer);

  return idx;
}

void build_descriptors_for_a_binding(DescriptorSet set, Binding binding, void *data)
{
  // uint32_t descriptorsCount = vec_len(data);
  //
  //
  // for(unsigned int i = 0; i < descriptorsCount; ++i)
  // {
    switch(binding.type)
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
          VkWriteDescriptorSet setWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .descriptorCount = 1,
            .descriptorType = binding.type,
            .dstSet = set.set,
            .dstBinding = binding.binding,
            .dstArrayElement = 0,
            .pBufferInfo = &bufferInfo,
          };

          vec_push(&binding.pBufferInfos, &bufferInfo);
          vec_push(&binding.pSetWrites, &setWrite);
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
  // }
  vkUpdateDescriptorSets(ev_vulkan_getlogicaldevice(), vec_len(binding.pSetWrites), binding.pSetWrites, 0, NULL);
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
  for (size_t materialIndex = 0; materialIndex < vec_len(DATA(material)); materialIndex++)
  {
    RendererMaterial *mm = DATA(material) + materialIndex;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mm->pipeline);

    VkDescriptorSet ds[4];
    for (size_t i = 0; i < vec_len(mm->pSets); i++)
    {
      ds[i] = mm->pSets[i].set;
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mm->pipelineLayout, 0, vec_len(mm->pSets), ds, 0, 0);

    for (size_t objectIndex = 0; objectIndex < vec_len(mm->pObjects); objectIndex++)
    {
      vkCmdDraw(cmd, 2046, 1, 0, 0);
    }
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



















typedef enum {
    MESHRESOURCE,
    CUSTOMBUFFER,
} RESOURCETYPE;

typedef struct {
  evstring shaderBindingName;
  RESOURCETYPE resourceType;
  uint32_t bufferize;
  void *data;
} ShaderData;



void registerResource(RendererMaterial *material, ShaderData *shaderData)
{
  switch (shaderData->resourceType)
  {
    uint32_t resourceIndex;

    case MESHRESOURCE:
      resourceIndex = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, material->pMeshBuffers);
      shaderData->data = &material->pMeshBuffers[resourceIndex];
      break;
    case CUSTOMBUFFER:
      resourceIndex = ev_renderer_registerbuffer(shaderData->data, shaderData->bufferize, material->pCustomBuffers);
      shaderData->data = &material->pCustomBuffers[resourceIndex];
      break;
  }
}

bool isNewMaterial()
{
  return true;
}


void ev_renderer_registerMaterial(evstring shaderCodes[], VkShaderStageFlags stageFlags[], uint32_t dataCount ,ShaderData shaderData[])
{
  RendererMaterial newMaterial;

  if(isNewMaterial())
  {
    newMaterial.pShaderCodes = vec_init(evstring);
    newMaterial.pStageFlags = vec_init(VkShaderStageFlagBits);
    newMaterial.pSets = vec_init(DescriptorSet);

    newMaterial.pObjects = vec_init(EvRenderObject);

    newMaterial.pMeshBuffers = vec_init(EvBuffer);
    newMaterial.pCustomBuffers = vec_init(EvBuffer);

    vec_push(&newMaterial.pShaderCodes, &shaderCodes[0]);
    vec_push(&newMaterial.pShaderCodes, &shaderCodes[1]);

    vec_push(&newMaterial.pStageFlags, &stageFlags[0]);
    vec_push(&newMaterial.pStageFlags, &stageFlags[1]);

    EvGraphicsPipelineCreateInfo pipelineCreateInfo1 = {
      .stageCount = vec_len(newMaterial.pShaderCodes),
      .pShaderCodes = newMaterial.pShaderCodes,
      .pStageFlags = newMaterial.pStageFlags,
      .renderPass = DATA(renderPass),
    };

    ev_pipeline_build(pipelineCreateInfo1, &newMaterial);

    for (size_t i = 0; i < vec_len(newMaterial.pSets); i++)
    {
      ev_descriptormanager_allocate(newMaterial.pSets[i].layout, &newMaterial.pSets[i].set);
    }

    vec_push(&DATA(material), &newMaterial);
  }

  for (size_t i = 0; i < dataCount; i++)
  {
    registerResource(&newMaterial, shaderData + i);
  }

  //push vertexbuffer
  for (size_t i = 0; i < vec_len(newMaterial.pSets); i++)
  {
    for (size_t j = 0; j < vec_len(newMaterial.pSets[i].pBindings); j++)
    {
      for (size_t k = 0; k < dataCount; k++)
      {
        if (!strcmp(newMaterial.pSets[i].pBindings[j].bindingName, shaderData[k].shaderBindingName))
        {
          ev_log_debug("set: %d, binding: %d writing", i, j);
          build_descriptors_for_a_binding(newMaterial.pSets[i], newMaterial.pSets[i].pBindings[j], shaderData[k].data);
        }
      }
    }
  }

  //triangle object
  // Matrix4x4 m = {{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1}};
  // memcpy(&triangleObj.transform, &m, sizeof(Matrix4x4));
  vec_push(&newMaterial.pObjects, &triangleObj);

}

// EvVertex qwe;
// EvVertex color;
// EvVertex ss;

void addObject()
{
  AssetHandle project_config = Asset->load("project://Avocado.mesh");
  MeshAsset project_desc = MeshLoader->loadAsset(project_config);

  // for (size_t i = 0; i < project_desc.vertexCount; i++)
  // {
  //   ev_log_debug("position: %f %f %f %f",   *(project_desc.vertexData + i*16 + 0), *(project_desc.vertexData + i*16 + 1), *(project_desc.vertexData + i*16 + 2), *(project_desc.vertexData + i*16 + 3));
  //   ev_log_debug("Normal  : %f %f %f %f",   *(project_desc.vertexData + i*16 + 4), *(project_desc.vertexData + i*16 + 5), *(project_desc.vertexData + i*16 + 6), *(project_desc.vertexData + i*16 + 7));
  //   ev_log_debug("Color   : %f %f %f %f",   *(project_desc.vertexData + i*16 + 8), *(project_desc.vertexData + i*16 + 9), *(project_desc.vertexData + i*16 + 10), *(project_desc.vertexData + i*16 + 11));
  //   ev_log_debug("UV      : %f %f %f %f\n", *(project_desc.vertexData + i*16+ 12), *(project_desc.vertexData + i*16 + 13), *(project_desc.vertexData + i*16+ 14), *(project_desc.vertexData + i*16 + 15));
  // }
  //
  // for (size_t i = 0; i < project_desc.indexCount; i++) {
  //   ev_log_debug("index: %d\n",*(project_desc.indexData + i));
  // }

  evstring shaderCodes[2] = {
    AssetSystem->load_shader("default.vert.spv"),
    AssetSystem->load_shader("default.frag.spv"),
  };

  VkShaderStageFlags stageFlags[] = {
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  // ShaderData shaderData[] = {
  //   {evstring_new("ResourceBuffers"), MESHRESOURCE, vec_len(triangleMesh.vertices) * sizeof(EvVertex), triangleMesh.vertices},
  //   {evstring_new("PositionBuffers"), CUSTOMBUFFER, 1 * sizeof(EvVertex), &qwe},
  //   {evstring_new("ColorBuffers"), CUSTOMBUFFER, 1 * sizeof(EvVertex), &color},
  //   {evstring_new("ssBuffers"), CUSTOMBUFFER, 1 * sizeof(EvVertex), &ss},
  // };

  ev_log_debug("%d", project_desc.indexCount);

  ShaderData shaderData[] = {
    {evstring_new("ResourceBuffers"), MESHRESOURCE, project_desc.vertexBuferSize, project_desc.vertexData},
    {evstring_new("PositionBuffers"), CUSTOMBUFFER, project_desc.indexBuferSize, project_desc.indexData},
    // {evstring_new("ColorBuffers"), CUSTOMBUFFER, 1 * sizeof(EvVertex), &color},
    // {evstring_new("ssBuffers"), CUSTOMBUFFER, 1 * sizeof(EvVertex), &ss},
  };

  ev_renderer_registerMaterial(shaderCodes, stageFlags, ARRAYSIZE(shaderData), shaderData);
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

  ev_renderer_createresourcememorypool(128ull * 1024 * 1024, 1, 4, &DATA(storagePool));

  addObject();
}

EV_CONSTRUCTOR
{
  // qwe.position = Vec3new( 1.0, 2.0, 3.0);
  // color.position = Vec3new(4.0, 5.0, 6.0);
  // ss.position = Vec3new(7.0, 8.0, 9.0);
  //
  // //triangle mesh init
  // {
  //   EvVertex v1, v2, v3;
  //   v1.position = Vec3new( 0.0, -0.5, 1.0);
  //   v2.position = Vec3new( 0.5,  0.5, 1.0);
  //   v3.position = Vec3new(-0.5,  0.5, 1.0);
  //
  //
  //   triangleMesh.vertices = vec_init(EvVertex);
  //   vec_push(&triangleMesh, &v1);
  //   vec_push(&triangleMesh, &v2);
  //   vec_push(&triangleMesh, &v3);
  //   triangleObj.mesh = triangleMesh;
  // }

  evolmodule_t asset_mod   = evol_loadmodule("assetmanager");   DEBUG_ASSERT(asset_mod);
  imports(asset_mod  , (AssetManager, Asset, MeshLoader))

  evolmodule_t asset_module   = evol_loadmodule("asset-importer"); DEBUG_ASSERT(asset_module);
  IMPORT_NAMESPACE(AssetSystem, asset_module);

  evolmodule_t window_module  = evol_loadmodule("window");  DEBUG_ASSERT(window_module);
  IMPORT_NAMESPACE(Window, window_module);

  DATA(material) = vec_init(RendererMaterial);

  ev_vulkan_init();
  ev_descriptormanager_init();
}

EV_DESTRUCTOR
{
  //destroy renderermaterial

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
