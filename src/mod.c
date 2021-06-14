#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <Vulkan.h>
#include <Pipeline.h>
#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <Renderer_types.h>

#include <VulkanQueueManager.h>

#define EV_WINDOW_VULKAN_SUPPORT
#define IMPORT_MODULE evmod_glfw
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_assets
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_game
#include IMPORT_MODULE_H

#define DATA(X) RendererData.X

#include <hashmap.h>
#include <evjson.h>

typedef GenericHandle PipelineHandle;
#define INVALID_PIPELINE_HANDLE (~0ull)

typedef GenericHandle MaterialHandle;
#define INVALID_MATERIAL_HANDLE (~0ull)

typedef GenericHandle MeshHandle;
#define INVALID_MESH_HANDLE (~0ull)

HashmapDefine(evstring, MaterialHandle, evstring_free, NULL);
HashmapDefine(evstring, PipelineHandle, evstring_free, NULL);
HashmapDefine(evstring, MeshHandle, evstring_free, NULL);

typedef struct {
  Map(evstring, MaterialHandle) map;
  vec(Material) store;
  vec(PipelineHandle) pipelineHandles;
  bool dirty;
} MaterialLibrary;

typedef struct {
  Map(evstring, PipelineHandle) map;
  vec(Pipeline) store;
} PipelineLibrary;

typedef struct {
  Map(evstring, MeshHandle) map;
  vec(Mesh) store;
} MeshLibrary;

struct ev_Renderer_Data
{
  WindowHandle windowHandle;
  EvSwapchain Swapchain;
  VkFramebuffer framebuffers[SWAPCHAIN_MAX_IMAGES];
  VkRenderPass renderPass;

  DescriptorSet cameraSet;
  UBO cameraBuffer;

  DescriptorSet resourcesSet;

  MeshLibrary meshLibrary;
  PipelineLibrary pipelineLibrary;
  MaterialLibrary materialLibrary;

  vec(EvBuffer) vertexBuffers;
  vec(EvBuffer) indexBuffers;
  vec(EvBuffer) customBuffers;

  EvBuffer materialsBuffer;
} RendererData;

evolmodule_t game_module;
evolmodule_t asset_module;
evolmodule_t window_module;

RenderComponent triangleObj;

void run();
void setWindow(WindowHandle handle);
void recreateSwapChain();
void ev_renderer_createSurface();
void ev_renderer_createrenderpass();
void ev_renderer_updatewindowsize();
void ev_renderer_createframebuffers();

void ev_renderer_destroyrenderpass();
void ev_renderer_destroyframebuffer();
void ev_renderer_destroysurface();
MeshHandle ev_renderer_registerMesh(CONST_STR meshPath);
int ev_renderer_registervertexbuffer(void *vertices, unsigned long long size, vec(EvBuffer) vertexBuffers);
RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName);

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

void recreateSwapChain()
{
  vkDeviceWaitIdle(ev_vulkan_getlogicaldevice());

  ev_renderer_destroyframebuffer();

  ev_renderer_updatewindowsize();

  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createframebuffers();
}

void draw(VkCommandBuffer cmd, vec(RenderComponent) components, vec(Matrix4x4) transforms)
{
  if (DATA(materialLibrary).dirty)
  {
    for (size_t i = 0; i < vec_len(RendererData.vertexBuffers); i++) {
      ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[1], &(DATA(vertexBuffers)[i].buffer));
    }

    for (size_t i = 0; i < vec_len(RendererData.indexBuffers); i++) {
      ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[2], &(DATA(indexBuffers)[i].buffer));
    }

    RendererData.materialsBuffer = ev_vulkan_registerbuffer(RendererData.materialLibrary.store, sizeof(Material) * vec_len(RendererData.materialLibrary.store));
    ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[3], &RendererData.materialsBuffer);

    DATA(materialLibrary).dirty = false;
  }

  CameraData cam;
  Camera->getViewMat(NULL, NULL, cam.viewMat);
  Camera->getProjectionMat(NULL, NULL, cam.projectionMat);

  ev_vulkan_updateubo(sizeof(CameraData), &cam, &DATA(cameraBuffer));

  for (size_t componentIndex = 0; componentIndex < vec_len(components); componentIndex++)
  {
    RenderComponent component = components[componentIndex];
    Pipeline pipeline = DATA(pipelineLibrary.store[component.pipelineIndex]);

    MeshPushConstants pushconstant;
    //pushconstant.transform = transforms[componentIndex];
    pushconstant.meshIndex = component.meshIndex;

    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushconstant);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    VkDescriptorSet ds[4];
    for (size_t i = 2; i < vec_len(pipeline.pSets); i++)
    {
      ds[i] = pipeline.pSets[i].set;
    }
    ds[0] = DATA(cameraSet).set;
    ds[1] = DATA(resourcesSet).set;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, vec_len(pipeline.pSets), ds, 0, 0);

    vkCmdDraw(cmd, component.mesh.indexCount, 1, 0, 0);
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

  vec(Matrix4x4) transforms = vec_init(Matrix4x4);
  vec_setlen(&transforms, 1);
  GameObject player = Scene->getObject(NULL, "Player");

  memcpy(transforms[0], *Object->getWorldTransform(NULL, player), sizeof(Matrix4x4));

  vec(RenderComponent) components = vec_init(RenderComponent);
  vec_setlen(&components, 1);
  components[0] =  ev_renderer_registerRenderComponent("project://Map.mesh", "WhiteMaterial");

  draw(cmd, components, transforms);


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

void ev_renderer_destroyrenderpass()
{
  vkDestroyRenderPass(ev_vulkan_getlogicaldevice(), DATA(renderPass), NULL);
}













MaterialHandle ev_renderer_getMaterial(const char *materialName)
{
  MaterialHandle *handle = Hashmap(evstring, MaterialHandle).get(DATA(materialLibrary).map, materialName);

  if (handle) {
    return *handle;
  }

  return INVALID_MATERIAL_HANDLE;
}

MaterialHandle ev_renderer_registerMaterial(const char *materialName, Material material, PipelineHandle usedPipeline)
{
  MaterialHandle *handle = Hashmap(evstring, MaterialHandle).get(DATA(materialLibrary).map, materialName);

  if (handle) {
    return *handle;
  }
  DATA(materialLibrary).dirty = true;

  MaterialHandle new_handle = (MaterialHandle)vec_push(&RendererData.materialLibrary.store, &material);
  size_t usedPipelineIdx = vec_push(&RendererData.materialLibrary.pipelineHandles, &usedPipeline);
  (void) usedPipelineIdx;
  DEBUG_ASSERT(vec_len(RendererData.materialLibrary.store) == vec_len(RendererData.materialLibrary.pipelineHandles));
  DEBUG_ASSERT(usedPipelineIdx == new_handle);

  Hashmap(evstring, MaterialHandle).push(DATA(materialLibrary).map, evstring_new(materialName), new_handle);
  ev_log_trace("new material! %f %f %f", material.baseColor.r, material.baseColor.g, material.baseColor.b);

  return new_handle;
}

void destroyPipeline(Pipeline *pipeline)
{
  ev_vulkan_destroypipeline(pipeline->pipeline);
  ev_vulkan_destroypipelinelayout(pipeline->pipelineLayout);

  for (size_t i = 0; i < vec_len(pipeline->pSets); i++)
  {
    vkDestroyDescriptorSetLayout(ev_vulkan_getlogicaldevice(), pipeline->pSets[i].layout, NULL);
  }
}

RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName)
{
  RenderComponent newComponent = { 0 };

  Mesh mesh = RendererData.meshLibrary.store[ev_renderer_registerMesh(meshPath)];

  ShaderMesh m = {
    .indexBufferIndex =  mesh.indexBufferIndex,
    .vertexBufferIndex = mesh.vertexBufferIndex,
  };
  //TODO setup mesh index and regester it


  newComponent.materialIndex = ev_renderer_getMaterial(materialName);
  newComponent.pipelineIndex = RendererData.materialLibrary.pipelineHandles[newComponent.materialIndex];
  newComponent.meshIndex = 0;

  return newComponent;
}

PipelineHandle ev_renderer_getPipeline(CONST_STR pipelineName)
{
  PipelineHandle *handle = Hashmap(evstring, PipelineHandle).get(DATA(pipelineLibrary).map, pipelineName);

  if (handle) {
    return *handle;
  }

  return INVALID_PIPELINE_HANDLE;
}

PipelineHandle ev_renderer_registerPipeline(CONST_STR pipelineName, vec(Shader) *shaders)
{
  PipelineHandle *handle = Hashmap(evstring, PipelineHandle).get(DATA(pipelineLibrary).map, pipelineName);

  if (handle) {
    return *handle;
  }

  Pipeline newPipeline;
  newPipeline.pSets = vec_init(DescriptorSet);

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = vec_len(*shaders),
    .pShaders = *shaders,
    .renderPass = DATA(renderPass),
  };

  ev_pipeline_build(pipelineCreateInfo, &newPipeline);

  for (size_t i = 0; i < vec_len(newPipeline.pSets); i++)
    ev_descriptormanager_allocate(newPipeline.pSets[i].layout, &newPipeline.pSets[i].set);

  PipelineHandle new_handle = (PipelineHandle)vec_push(&RendererData.pipelineLibrary.store, &newPipeline);
  Hashmap(evstring, MaterialHandle).push(DATA(pipelineLibrary).map, evstring_new(pipelineName), new_handle);

  return new_handle;
}

MeshHandle ev_renderer_registerMesh(CONST_STR meshPath)
{
  MeshHandle *handle = Hashmap(evstring, MeshHandle).get(DATA(meshLibrary).map, meshPath);

  if (handle) {
    ev_log_debug("found mesh in library!");
    return *handle;
  }

  Mesh newMesh;

  ev_log_debug("New mesh!: %s", meshPath);
  AssetHandle mesh_handle = Asset->load(meshPath);
  MeshAsset meshAsset = MeshLoader->loadAsset(mesh_handle);

  newMesh.indexCount = meshAsset.indexCount;
  newMesh.vertexCount = meshAsset.vertexCount;

  EvBuffer indexBuffer  = ev_vulkan_registerbuffer(meshAsset.indexData, meshAsset.indexBuferSize);
  EvBuffer vertexBuffer = ev_vulkan_registerbuffer(meshAsset.vertexData, meshAsset.vertexBuferSize);

  newMesh.indexBufferIndex  = vec_push(&DATA(indexBuffers),  &indexBuffer);
  newMesh.vertexBufferIndex = vec_push(&DATA(vertexBuffers), &vertexBuffer);

  MeshHandle new_handle = (MeshHandle)vec_push(&RendererData.meshLibrary.store, &newMesh);
  Hashmap(evstring, MeshHandle).push(DATA(meshLibrary).map, evstring_new(meshPath), new_handle);

  return new_handle;
}

void ev_material_readjsonlist(evjson_t *json_context, const char *list_name)
{
  evstring materials = evstring_newfmt("%s.len", list_name);
  int material_count = (int)evjs_get(json_context , materials)->as_num;
  evstring_free(materials);

  for(int i = 0; i < material_count; i++)
  {
    Material newMaterial = {0};

    evstring material_id = evstring_newfmt("%s[%d].id", list_name, i);
    evstring materialname = evstring_refclone(evjs_get(json_context, material_id)->as_str);

    for (size_t j = 0; j < 3; j++)
    {
      evstring material_basecolor = evstring_newfmt("%s[%d].baseColor[%d]", list_name, i, j);
      ((float*)&newMaterial.baseColor)[j] = (float)evjs_get(json_context, material_basecolor)->as_num;

      evstring_free(material_basecolor);
    }

    evstring materialPipeline_jsonid = evstring_newfmt("%s[%d].pipeline", list_name, i);
    evstring materialPipeline = evstring_refclone(evjs_get(json_context, materialPipeline_jsonid)->as_str);

    PipelineHandle materialPipelineHandle = ev_renderer_getPipeline(materialPipeline);

    evstring_free(materialPipeline);
    evstring_free(materialPipeline_jsonid);

    ev_renderer_registerMaterial(materialname, newMaterial, materialPipelineHandle);

    evstring_free(materialname);
    evstring_free(material_id);
  }
}

VkShaderStageFlags jsonshadertype_to_vkshaderstage(evstr_ref type) {
  switch(*(type.data + type.offset)) {
    case 'V':
      return VK_SHADER_STAGE_VERTEX_BIT;
    case 'F':
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
      break;
  }
}

ShaderAssetStage jsonshadertype_to_assetstage(evstr_ref type) {
  switch(*(type.data + type.offset)) {
    case 'V':
      return EV_SHADERASSETSTAGE_VERTEX;
    case 'F':
      return EV_SHADERASSETSTAGE_FRAGMENT;
    default:
      break;
  }
}

void ev_graphicspipeline_readjsonlist(evjson_t *json_context, const char *list_name)
{
  vec(AssetHandle) loadedAssets = vec_init(AssetHandle);
  evstring pipelineCount_jsonid = evstring_newfmt("%s.len", list_name);
  U32 pipelineCount = (U32)evjs_get(json_context, pipelineCount_jsonid)->as_num;
  for(U32 i = 0; i < pipelineCount; i++) {
    evstring pipelineName_jsonid = evstring_newfmt("%s[%d].id", list_name, i);
    evstring pipelineName = evstring_refclone(evjs_get(json_context, pipelineName_jsonid)->as_str);

    evstring shaderStages_jsonid = evstring_newfmt("%s[%d].shaderStages", list_name, i);
    evstring shaderStagesCount_jsonid = evstring_newfmt("%s.len", shaderStages_jsonid);
    U32 shaderStagesCount = (U32)evjs_get(json_context, shaderStagesCount_jsonid)->as_num;

    vec(Shader) shaders = vec_init(Shader);

    for(U32 shader_idx = 0; shader_idx < shaderStagesCount; shader_idx++) {
      evstring shaderStage_jsonid = evstring_newfmt("%s[%u]", shaderStages_jsonid, shader_idx);
      evstring shaderType_jsonid = evstring_newfmt("%s.type", shaderStage_jsonid);
      evstr_ref shaderType_ref = evjs_get(json_context, shaderType_jsonid)->as_str;

      VkShaderStageFlags vkShaderStage = jsonshadertype_to_vkshaderstage(shaderType_ref);
      ShaderAssetStage shaderAssetStage = jsonshadertype_to_assetstage(shaderType_ref);

      evstring shaderPath_jsonid = evstring_newfmt("%s.shaderPath", shaderStage_jsonid);
      evstring shaderPath = evstring_refclone(evjs_get(json_context, shaderPath_jsonid)->as_str);

      AssetHandle asset = Asset->load(shaderPath);
      vec_push(&loadedAssets, &asset);

      ShaderAsset shaderAsset = ShaderLoader->loadAsset(asset, shaderAssetStage, "default.vert", NULL, EV_SHADER_BIN);

      vec_push(&shaders, &(Shader){
        .data = shaderAsset.binary,
        .length = shaderAsset.len,
        .stage = vkShaderStage,
      });

      evstring_free(shaderPath);
      evstring_free(shaderPath_jsonid);

      evstring_free(shaderType_jsonid);
      evstring_free(shaderStage_jsonid);
    }

    ev_renderer_registerPipeline(pipelineName, &shaders);

    vec_fini(shaders);

    evstring_free(shaderStagesCount_jsonid);
    evstring_free(shaderStages_jsonid);

    evstring_free(pipelineName_jsonid);
    evstring_free(pipelineName);
  }

  for(size_t asset_idx = 0; asset_idx < vec_len(loadedAssets); asset_idx++) {
    Asset->free(loadedAssets[asset_idx]);
  }
  vec_fini(loadedAssets);

  evstring_free(pipelineCount_jsonid);
}

void setWindow(WindowHandle handle)
{
  DATA(windowHandle) = handle;

  ev_renderer_updatewindowsize();

  ev_renderer_createSurface();
  ev_swapchain_create(&DATA(Swapchain));
  ev_renderer_createrenderpass();
  ev_renderer_createframebuffers();
}

void ev_renderer_globalsetsinit()
{
  //cameraSet
  ev_vulkan_allocateubo(sizeof(CameraData), 0, &RendererData.cameraBuffer);
  VkDescriptorSetLayoutBinding camerabindings[] = {
    {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    }
  };
  VkDescriptorSetLayoutCreateInfo cameradescriptorSetLayoutCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = ARRAYSIZE(camerabindings),
    .pBindings = camerabindings
  };
  DATA(cameraSet).pBindings = vec_init(Binding);
  vec_push(&DATA(cameraSet).pBindings, &(Binding) {
    .binding = 0,
    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
  });
  VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &cameradescriptorSetLayoutCreateInfo, NULL, &DATA(cameraSet).layout));
  ev_descriptormanager_allocate(DATA(cameraSet).layout, &DATA(cameraSet).set);
  for (size_t i = 0; i < vec_len(DATA(cameraSet).pBindings); i++)
  {
    ev_vulkan_writeintobinding(DATA(cameraSet), &DATA(cameraSet).pBindings[i], &(DATA(cameraBuffer).buffer));
  }


  //Resources set
  VkDescriptorSetLayoutBinding resourcesbindings[] =
  {
    {
      .binding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    },
    {
      .binding = 1,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    },
    {
      .binding = 2,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    },
    {
      .binding = 3,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    }
  };
  VkDescriptorSetLayoutCreateInfo resourcesdescriptorSetLayoutCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = ARRAYSIZE(resourcesbindings),
    .pBindings = resourcesbindings,
  };
  DATA(resourcesSet).pBindings = vec_init(Binding);
  for (size_t i = 0; i < ARRAYSIZE(resourcesbindings); i++) {
    vec_push(&DATA(resourcesSet).pBindings, &(Binding) {
      .binding = resourcesbindings[i].binding,
      .type = resourcesbindings[i].descriptorType,
    });
  }
  VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &resourcesdescriptorSetLayoutCreateInfo, NULL, &DATA(resourcesSet).layout));
  ev_descriptormanager_allocate(DATA(resourcesSet).layout, &DATA(resourcesSet).set);
}

void materialLibraryInit(MaterialLibrary *library)
{
  library->map = Hashmap(evstring, MaterialHandle).new();
  library->store = vec_init(Material);
  library->pipelineHandles = vec_init(PipelineHandle);
  library->dirty = true;
}

void materialLibraryDestroy(MaterialLibrary library)
{
  Hashmap(evstring, MaterialHandle).free(library.map);
  vec_fini(library.store);
  vec_fini(library.pipelineHandles);
}

void pipelineLibraryInit(PipelineLibrary *library)
{
  library->map = Hashmap(evstring, PipelineHandle).new();
  library->store = vec_init(Pipeline, NULL, destroyPipeline);
}

void pipelineLibraryDestroy(PipelineLibrary library)
{
  Hashmap(evstring, PipelineHandle).free(library.map);
  vec_fini(library.store);
}

void meshLibraryInit(MeshLibrary *library)
{
  library->map = Hashmap(evstring, MeshHandle).new();
  library->store = vec_init(Mesh);
}

void meshLibraryDestroy(MeshLibrary library)
{
  Hashmap(evstring, MeshHandle).free(library.map);
  vec_fini(library.store);
}

EV_CONSTRUCTOR
{
  game_module = evol_loadmodule_weak("game");        DEBUG_ASSERT(game_module);
  imports(game_module  , (Game, Camera, Object, Scene));

  asset_module = evol_loadmodule("assetmanager"); DEBUG_ASSERT(asset_module);
  imports(asset_module  , (AssetManager, Asset, MeshLoader, ShaderLoader));

  window_module  = evol_loadmodule("window");     DEBUG_ASSERT(window_module);
  imports(window_module, (Window));

  RendererData.vertexBuffers = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  RendererData.indexBuffers  = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  RendererData.customBuffers = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);

  ev_vulkan_init();

  ev_renderer_globalsetsinit();

  materialLibraryInit(&DATA(materialLibrary));
  pipelineLibraryInit(&DATA(pipelineLibrary));
  meshLibraryInit(&DATA(meshLibrary));
}

EV_DESTRUCTOR
{
  vkWaitForFences(ev_vulkan_getlogicaldevice(), DATA(Swapchain).imageCount, DATA(Swapchain).frameSubmissionFences, VK_TRUE, ~0ull);

  ev_vulkan_freeubo(&DATA(cameraBuffer));

  vec_fini(DATA(customBuffers));
  vec_fini(DATA(vertexBuffers));
  vec_fini(DATA(indexBuffers));

  ev_renderer_destroyframebuffer();
  ev_renderer_destroyrenderpass();
  ev_swapchain_destroy(&DATA(Swapchain));
  ev_renderer_destroysurface();

  meshLibraryDestroy(DATA(meshLibrary));
  materialLibraryDestroy(DATA(materialLibrary));
  pipelineLibraryDestroy(DATA(pipelineLibrary));

  ev_vulkan_deinit();

  evol_unloadmodule(window_module);
  evol_unloadmodule(asset_module);
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
  EV_NS_BIND_FN(Renderer, run, run);

  EV_NS_BIND_FN(Material, readJSONList, ev_material_readjsonlist);

  EV_NS_BIND_FN(GraphicsPipeline, readJSONList, ev_graphicspipeline_readjsonlist);
}
