#define EV_MODULE_DEFINE
#include <evol/evolmod.h>

#include <evjson.h>
#include <hashmap.h>
#include <Vulkan.h>
#include <Pipeline.h>
#include <Swapchain.h>
#include <Vulkan_utils.h>
#include <Renderer_types.h>

#include <VulkanQueueManager.h>

#define DEFAULTPIPELINE "DefaultPipeline"
#define DEFAULTEXTURE "DefaultTexture"

#define EV_WINDOW_VULKAN_SUPPORT
#define IMPORT_MODULE evmod_glfw
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_assets
#include IMPORT_MODULE_H
#define IMPORT_MODULE evmod_game
#include IMPORT_MODULE_H

#define DATA(X) RendererData.X

typedef GenericHandle PipelineHandle;
#define INVALID_PIPELINE_HANDLE (~0ull)

typedef GenericHandle MaterialHandle;
#define INVALID_MATERIAL_HANDLE (~0ull)

typedef GenericHandle MeshHandle;
#define INVALID_MESH_HANDLE (~0ull)

typedef GenericHandle TextureHandle;
#define INVALID_TEXTURE_HANDLE (~0ull)

HashmapDefine(evstring, MaterialHandle, evstring_free, NULL);
HashmapDefine(evstring, PipelineHandle, evstring_free, NULL);
HashmapDefine(evstring, TextureHandle, evstring_free, NULL);
HashmapDefine(evstring, MeshHandle, evstring_free, NULL);

#define UBOMAXSIZE 16384

typedef struct {
  Matrix4x4 transform;
  LightComponent;
} LightObject;

typedef struct{
  pthread_mutex_t lightMutex;
  vec(LightObject) lightObjects;
} FrameLightData;

typedef struct {
  struct {
    pthread_mutex_t objectMutex;
    vec(RenderComponent) objectComponents;
    vec(Matrix4x4) objectTranforms;
  };

  FrameLightData;

} FrameData;

typedef struct {
  Map(evstring, MaterialHandle) map;
  vec(Material) store;
  vec(PipelineHandle) pipelineHandles;
  bool dirty;
} MaterialLibrary;

typedef struct {
  Map(evstring, PipelineHandle) map;
  vec(Pipeline) store;
  bool dirty;
} PipelineLibrary;

typedef struct {
  Map(evstring, TextureHandle) map;
  vec(Texture) store;
  bool dirty;
} TextureLibrary;

typedef struct {
  Map(evstring, MeshHandle) map;
  vec(Mesh) store;
  bool dirty;
} MeshLibrary;

struct ev_Renderer_Data
{
  FrameData currentFrame;

  WindowHandle windowHandle;
  bool windowResized;

  DescriptorSet sceneSet;
  DescriptorSet cameraSet;
  DescriptorSet resourcesSet;

  MeshLibrary meshLibrary;
  TextureLibrary textureLibrary;
  PipelineLibrary pipelineLibrary;
  MaterialLibrary materialLibrary;

  UBO scenesBuffer;
  UBO lightsBuffer;
  UBO cameraBuffer;

  EvBuffer materialsBuffer;

  vec(EvTexture) textureBuffers;
  vec(EvBuffer)  vertexBuffers;
  vec(EvBuffer)  indexBuffers;
  vec(EvBuffer)  customBuffers;

  Pipeline lightPipeline;
  Pipeline skyboxPipeline;

  uint32_t frameNumber;
} RendererData;

DECLARE_EVENT_LISTENER(WindowResizedListener, (WindowResizedEvent *event) {
  RendererData.windowResized = true;

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  swapchain->windowExtent.width = event->width;
  swapchain->windowExtent.height= event->height;

  ev_log_debug("width: %d, hieght: %d", swapchain->windowExtent.width, swapchain->windowExtent.height);
})

evolmodule_t game_module;
evolmodule_t asset_module;
evolmodule_t window_module;

void ev_renderer_registerLightPipeline();
void ev_renderer_registerskyboxPipeline();

void ev_renderer_globalsetsinit()
{
  //SceneSet
  {
    VkDescriptorSetLayoutBinding sceneBindings[] = {
      {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
        .binding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      }
    };
    VkDescriptorSetLayoutCreateInfo sceneDescriptorSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = ARRAYSIZE(sceneBindings),
      .pBindings = sceneBindings,
    };
    DATA(sceneSet).pBindings = vec_init(Binding);
    for (size_t i = 0; i < ARRAYSIZE(sceneBindings); i++) {
      vec_push(&DATA(sceneSet).pBindings, &(Binding) {
        .binding = sceneBindings[i].binding,
        .type = sceneBindings[i].descriptorType,
      });
    }
    VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &sceneDescriptorSetLayoutCreateInfo, NULL, &DATA(sceneSet).layout));
    ev_descriptormanager_allocate(DATA(sceneSet).layout, &DATA(sceneSet).set);

    //sceneBuffer
    ev_vulkan_allocateubo(sizeof(EvScene), false, &RendererData.scenesBuffer);
    ev_vulkan_writeintobinding(DATA(sceneSet), &DATA(sceneSet).pBindings[0], 0, &(DATA(scenesBuffer).buffer));

    // //lightsBuffer
    // ev_vulkan_allocateubo(UBOMAXSIZE, false, &RendererData.lightsBuffer);
    // ev_vulkan_writeintobinding(DATA(sceneSet), &DATA(sceneSet).pBindings[1], 0, &(DATA(lightsBuffer).buffer));
  }

  //CameraSet
  {
    VkDescriptorSetLayoutBinding camerabindings[] = {
      {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      }
    };
    VkDescriptorSetLayoutCreateInfo cameradescriptorSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = ARRAYSIZE(camerabindings),
      .pBindings = camerabindings
    };
    DATA(cameraSet).pBindings = vec_init(Binding);
    for (size_t i = 0; i < ARRAYSIZE(camerabindings); i++) {
      vec_push(&DATA(cameraSet).pBindings, &(Binding) {
        .binding = camerabindings[i].binding,
        .type = camerabindings[i].descriptorType,
      });
    }
    VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &cameradescriptorSetLayoutCreateInfo, NULL, &DATA(cameraSet).layout));
    ev_descriptormanager_allocate(DATA(cameraSet).layout, &DATA(cameraSet).set);

    ev_vulkan_allocateubo(sizeof(CameraData), false, &RendererData.cameraBuffer);
    ev_vulkan_writeintobinding(DATA(cameraSet), &DATA(cameraSet).pBindings[0], 0, &(DATA(cameraBuffer).buffer));
  }

  //Resources set
  {
    VkDescriptorSetLayoutBinding resourcesbindings[] =
    {
      {
        .binding = 0,
        .descriptorCount = 2000,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      },
      {
        .binding = 1,
        .descriptorCount = 2000,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      },
      {
        .binding = 2,
        .descriptorCount = 2000,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      },
      {
        .binding = 3,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
        .binding = 4,
        .descriptorCount = 2000,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
    };
    VkDescriptorBindingFlagsEXT bindingFlags[] = {
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
      0,
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT descriptorSetLayoutBindingFlagsCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
      .bindingCount = ARRAYSIZE(resourcesbindings),
      .pBindingFlags = bindingFlags,
    };

    VkDescriptorSetLayoutCreateInfo resourcesdescriptorSetLayoutCreateInfo =
    {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = ARRAYSIZE(resourcesbindings),
      .pNext = &descriptorSetLayoutBindingFlagsCreateInfo,
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

  //lightsBuffer
  ev_vulkan_allocateubo(UBOMAXSIZE, false, &RendererData.lightsBuffer);
}

void ev_renderer_globalsetsdinit()
{
  //SceneSet
  ev_vulkan_freeubo(&DATA(scenesBuffer));
  ev_vulkan_freeubo(&DATA(lightsBuffer));
  // ev_vulkan_destroysetlayout(RendererData.sceneSet.layout);

  //cameraSet
  ev_vulkan_freeubo(&DATA(cameraBuffer));
  // ev_vulkan_destroysetlayout(RendererData.cameraSet.layout);

  //Resources set
  ev_vulkan_destroybuffer(&RendererData.materialsBuffer);
  // ev_vulkan_destroysetlayout(RendererData.resourcesSet.layout);
}

void setWindow(WindowHandle handle)
{
  DATA(windowHandle) = handle;

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  Window->getSize(DATA(windowHandle), &swapchain->windowExtent.width, &swapchain->windowExtent.height);

  ev_renderer_createSurface();
  ev_vulkan_createEvswapchain(framebuffering_degree);

  ev_vulkan_createoffscreenrenderpass();
  ev_vulkan_createoffscreenframebuffer();

  ev_vulkan_createrenderpass();
  ev_vulkan_createframebuffers();
}

void draw(VkCommandBuffer cmd)
{
  CameraData cam;
  Camera->getViewMat(NULL, NULL, cam.viewMat);
  Camera->getProjectionMat(NULL, NULL, cam.projectionMat);

  ev_vulkan_updateubo(sizeof(CameraData), &cam, &DATA(cameraBuffer));

  VkPipeline oldPipeline;
  for (size_t componentIndex = 0; componentIndex < vec_len(DATA(currentFrame).objectComponents); componentIndex++)
  {
    RenderComponent component = DATA(currentFrame).objectComponents[componentIndex];
    // ev_log_debug("mesh # %d, vertexbuffer: %d, indexbuffer: %d", componentIndex, component.mesh.vertexBufferIndex, component.mesh.indexBufferIndex);
    Pipeline pipeline = DATA(pipelineLibrary.store[component.pipelineIndex]);

    MeshPushConstants pushconstant;
    memcpy(pushconstant.transform, DATA(currentFrame).objectTranforms[componentIndex], sizeof(Matrix4x4));
    pushconstant.indexBufferIndex = component.mesh.indexBufferIndex;
    pushconstant.vertexBufferIndex = component.mesh.indexBufferIndex;
    pushconstant.materialIndex = component.materialIndex;

    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(MeshPushConstants), &pushconstant);

    if (oldPipeline != pipeline.pipeline)
    {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
      oldPipeline = pipeline.pipeline;
    }

    VkDescriptorSet ds[4];
    for (size_t i = 0; i < vec_len(pipeline.pSets); i++)
    {
      ds[i] = pipeline.pSets[i].set;
    }

    ds[0] = DATA(sceneSet).set;
    ds[1] = DATA(cameraSet).set;
    ds[2] = DATA(resourcesSet).set;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, vec_len(pipeline.pSets), ds, 0, 0);

    vkCmdDraw(cmd, component.mesh.indexCount, 1, 0, 0);
  }
}

void ev_renderer_updatewindowsize()
{
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  Window->getSize(DATA(windowHandle), &swapchain->windowExtent.width, &swapchain->windowExtent.height);
}

void ev_renderer_createSurface()
{
  VkSurfaceKHR *surface = ev_vulkan_getSurface();
  VK_ASSERT(Window->createVulkanSurface(DATA(windowHandle), ev_vulkan_getinstance(), surface));

  ev_vulkan_checksurfacecompatibility();
}

void ev_renderer_destroysurface(VkSurfaceKHR surface)
{
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  vkDestroySurfaceKHR(ev_vulkan_getinstance(), surface, NULL);
}

void run()
{
  if (DATA(materialLibrary).dirty)
  {
    ev_vulkan_wait();
    size_t materialBufferLength = MAX(vec_len(RendererData.materialLibrary.store), vec_capacity(RendererData.materialLibrary.store));
    RendererData.materialsBuffer = ev_vulkan_registerbuffer(RendererData.materialLibrary.store, sizeof(Material) * materialBufferLength);
    ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[3], 0, &RendererData.materialsBuffer);

    DATA(materialLibrary).dirty = false;
  }

  if (DATA(meshLibrary).dirty)
  {
    ev_vulkan_wait();
    for (size_t i = 0; i < vec_len(RendererData.indexBuffers); i++) {
      ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[2], i, &(DATA(indexBuffers)[i].buffer));
    }

    for (size_t i = 0; i < vec_len(RendererData.vertexBuffers); i++) {
      ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[1], i, &(DATA(vertexBuffers)[i].buffer));
    }

    DATA(meshLibrary).dirty = false;
  }

  if (DATA(textureLibrary).dirty)
  {
    ev_vulkan_wait();
    for (size_t i = 0; i < vec_len(RendererData.textureBuffers); i++) {
      ev_vulkan_writeintobinding(DATA(resourcesSet), &DATA(resourcesSet).pBindings[4], i, &(DATA(textureBuffers)[i]));
    }

    DATA(textureLibrary).dirty = false;
  }

  if (RendererData.windowResized)
  {
    ev_vulkan_recreateSwapChain();
    RendererData.windowResized = false;
  }

  // ev_log_debug();

  VkCommandBuffer cmd = ev_vulkan_startframeoffscreen( (RendererData.frameNumber % framebuffering_degree ) );
  draw(cmd);
  ev_vulkan_endframeoffscreen(cmd, (RendererData.frameNumber % framebuffering_degree));

  VkCommandBuffer cmd1 = ev_vulkan_startframe((RendererData.frameNumber % framebuffering_degree));
  if (cmd1)
  {
    ev_vulkan_updateubo(sizeof(LightObject) * vec_len(DATA(currentFrame).lightObjects), RendererData.currentFrame.lightObjects, &(DATA(lightsBuffer).buffer));
    LightPushConstants lightPushConstants;
    lightPushConstants.lightCount = vec_len(DATA(currentFrame).lightObjects);
    vkCmdPushConstants(cmd1, RendererData.lightPipeline.pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(LightPushConstants), &lightPushConstants);
    vkCmdBindPipeline(cmd1, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.lightPipeline.pipeline);
    VkDescriptorSet ds[4];
    for (size_t i = 0; i < vec_len(RendererData.lightPipeline.pSets); i++)
    {
      ds[i] = RendererData.lightPipeline.pSets[i].set;
    }
    vkCmdBindDescriptorSets(cmd1, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.lightPipeline.pipelineLayout, 0, vec_len(RendererData.lightPipeline.pSets), ds, 0, 0);
    vkCmdDraw(cmd1, 3, 1, 0, 0);

    ev_vulkan_endframe(cmd1, (RendererData.frameNumber % framebuffering_degree));
  }

  FrameData_clear(&DATA(currentFrame));
  RendererData.frameNumber++;
}

void ev_renderer_addFrameObjectData(RenderComponent *components, Matrix4x4 *transforms, uint32_t count)
{
  pthread_mutex_lock(&DATA(currentFrame).objectMutex);

  vec_append(&DATA(currentFrame).objectComponents, &components, count);

  vec_append(&DATA(currentFrame).objectTranforms, &transforms, count);

  pthread_mutex_unlock(&DATA(currentFrame).objectMutex);
}

void ev_renderer_addFrameLightData(LightComponent *components, Matrix4x4 *transforms, uint32_t count)
{
  pthread_mutex_lock(&DATA(currentFrame).lightMutex);

  for(size_t i = 0; i < count; i++) {
    vec_push(&DATA(currentFrame).lightObjects, &(LightObject) {
      .color = components[i].color,
      .intensity = components[i].intensity,
    });

    memcpy(DATA(currentFrame).lightObjects[i].transform, transforms[i], sizeof(Matrix4x4));
  }

  pthread_mutex_unlock(&DATA(currentFrame).lightMutex);
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

  MaterialHandle new_handle = (MaterialHandle)vec_push(&RendererData.materialLibrary.store, &material);
  size_t usedPipelineIdx = vec_push(&RendererData.materialLibrary.pipelineHandles, &usedPipeline);
  (void) usedPipelineIdx;
  DEBUG_ASSERT(vec_len(RendererData.materialLibrary.store) == vec_len(RendererData.materialLibrary.pipelineHandles));
  DEBUG_ASSERT(usedPipelineIdx == new_handle);

  Hashmap(evstring, MaterialHandle).push(DATA(materialLibrary).map, evstring_new(materialName), new_handle);
  ev_log_trace("new material! %f %f %f", material.baseColor.r, material.baseColor.g, material.baseColor.b);

  RendererData.materialLibrary.dirty = true;

  return new_handle;
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
    .renderPass = ev_vulkan_getoffscreenrenderpass(),
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  vec_push(&overrides, &RendererData.sceneSet);
  vec_push(&overrides, &RendererData.cameraSet);
  vec_push(&overrides, &RendererData.resourcesSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &newPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(newPipeline.pSets); i++)
    ev_descriptormanager_allocate(newPipeline.pSets[i].layout, &newPipeline.pSets[i].set);

  PipelineHandle new_handle = (PipelineHandle)vec_push(&RendererData.pipelineLibrary.store, &newPipeline);
  Hashmap(evstring, MaterialHandle).push(DATA(pipelineLibrary).map, evstring_new(pipelineName), new_handle);

  return new_handle;
}

void ev_renderer_registerLightPipeline()
{
  AssetHandle vertAsset= Asset->load("shaders://deferred.vert");
  ShaderAsset shaderVertAsset = ShaderLoader->loadAsset(vertAsset, EV_SHADERASSETSTAGE_VERTEX, "deferred.vert", NULL, EV_SHADER_BIN);

  AssetHandle fragAsset = Asset->load("shaders://deferred.frag");
  ShaderAsset shaderFragAsset = ShaderLoader->loadAsset(fragAsset, EV_SHADERASSETSTAGE_FRAGMENT, "deferred.frag", NULL, EV_SHADER_BIN);

  Shader shaders[] = {
    {
      .data = shaderVertAsset.binary,
      .length = shaderVertAsset.len,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
    },
    {
      .data = shaderFragAsset.binary,
      .length = shaderFragAsset.len,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    },
  };

  RendererData.lightPipeline.pSets = vec_init(DescriptorSet);

  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
    .colorWriteMask =
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_A_BIT ,
  };
  VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &pipelineColorBlendAttachmentState,
  };

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
  };

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = ARRAYSIZE(shaders),
    .pShaders = shaders,
    .renderPass = ev_vulkan_getrenderpass(),
    .pColorBlendState = &pipelineColorBlendState,
    .pDepthStencilState = &pipelineDepthStencilState,
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.lightPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(RendererData.lightPipeline.pSets); i++)
    ev_descriptormanager_allocate(RendererData.lightPipeline.pSets[i].layout, &RendererData.lightPipeline.pSets[i].set);

  FrameBuffer *framebuffer = ev_vulkan_getoffscreenframebuffer();

  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[0], 0, &framebuffer->position.texture);
  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[1], 0, &framebuffer->normal.texture);
  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[2], 0, &framebuffer->albedo.texture);
  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[3], 0, &framebuffer->specular.texture);

  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[0], 0, &(DATA(cameraBuffer).buffer));
  ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[1], 0, &(DATA(lightsBuffer).buffer));
}

void ev_renderer_skyboxLightPipeline()
{
  // RendererData.skyboxPipeline.pSets = vec_init(DescriptorSet);
  //
  // AssetHandle vertAsset= Asset->load("shaders://skybox.vert");
  // ShaderAsset shaderVertAsset = ShaderLoader->loadAsset(vertAsset, EV_SHADERASSETSTAGE_VERTEX, "skybox.vert", NULL, EV_SHADER_BIN);
  //
  // AssetHandle fragAsset = Asset->load("shaders://skybox.frag");
  // ShaderAsset shaderFragAsset = ShaderLoader->loadAsset(fragAsset, EV_SHADERASSETSTAGE_FRAGMENT, "skybox.frag", NULL, EV_SHADER_BIN);
  //
  // Shader shaders[] = {
  //   {
  //     .data = shaderVertAsset.binary,
  //     .length = shaderVertAsset.len,
  //     .stage = VK_SHADER_STAGE_VERTEX_BIT,
  //   },
  //   {
  //     .data = shaderFragAsset.binary,
  //     .length = shaderFragAsset.len,
  //     .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
  //   },
  // };
  //
  // VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
  //   .colorWriteMask =
  //     VK_COLOR_COMPONENT_B_BIT |
  //     VK_COLOR_COMPONENT_G_BIT |
  //     VK_COLOR_COMPONENT_R_BIT |
  //     VK_COLOR_COMPONENT_A_BIT ,
  // };
  //
  // VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
  //   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
  //   .depthClampEnable = VK_FALSE,
  //   .rasterizerDiscardEnable = VK_FALSE,
  //   .polygonMode = VK_POLYGON_MODE_FILL,
  //   .cullMode = VK_CULL_MODE_FRONT_BIT,
  //   .lineWidth = 1.0,
  // };
  //
  // VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
  //   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
  //   .attachmentCount = 1,
  //   .pAttachments = &pipelineColorBlendAttachmentState,
  // };
  //
  // VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
  //   .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
  //   .depthTestEnable = VK_TRUE,
  // };
  //
  // EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
  //   .stageCount = ARRAYSIZE(shaders),
  //   .pShaders = shaders,
  //   .renderPass = ev_vulkan_getrenderpass(asd), //change this
  //   .pColorBlendState = &pipelineColorBlendState,
  //   .pDepthStencilState = &pipelineDepthStencilState,
  //   .pColorBlendState = pipelineColorBlendState,
  // };
  //
  // vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  // ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.skyboxPipeline);
  // vec_fini(overrides);
  //
  // for (size_t i = 0; i < vec_len(RendererData.skyboxPipeline.pSets); i++)
  //   ev_descriptormanager_allocate(RendererData.skyboxPipeline.pSets[i].layout, &RendererData.skyboxPipeline.pSets[i].set);
  //
  // FrameBuffer *framebuffer = ev_vulkan_getoffscreenframebuffer();
  //
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[0], 0, &framebuffer->position.texture);
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[1], 0, &framebuffer->normal.texture);
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[2], 0, &framebuffer->albedo.texture);
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[3], 0, &framebuffer->specular.texture);
  //
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[0], 0, &(DATA(cameraBuffer).buffer));
  // ev_vulkan_writeintobinding(DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[1], 0, &(DATA(lightsBuffer).buffer));
}

void destroyPipeline(Pipeline *pipeline)
{
  ev_vulkan_destroypipeline(pipeline->pipeline);
  ev_vulkan_destroypipelinelayout(pipeline->pipelineLayout);

  ev_log_debug("set number : %d", vec_len(pipeline->pSets));
  for (size_t i = 0; i < vec_len(pipeline->pSets); i++)
  {
    ev_vulkan_destroysetlayout(pipeline->pSets[i].layout);
  }
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

  RendererData.meshLibrary.dirty = true;

  return new_handle;
}

TextureHandle ev_renderer_registerTexture(CONST_STR imagePath)
{
  TextureHandle *handle = Hashmap(evstring, TextureHandle).get(DATA(textureLibrary).map, imagePath);

  if (handle) {
    ev_log_debug("found image in library!");
    return *handle;
  }

  Texture newTexture;

  VkFormat format;
  EvTexture textureBuffer;
  uint32_t textureIndex;
  TextureHandle new_handle;

  if (!strcmp(imagePath, DEFAULTEXTURE)) {
    // default 2x2 texture
    format = VK_FORMAT_R8G8B8A8_SRGB;

    uint32_t pixels[4] = {~0, ~0, ~0, ~0};
    newTexture.bufferSize = sizeof(pixels);
    newTexture.width = 2;
    newTexture.height = 2;

    textureBuffer = ev_vulkan_registerTexture(format, 2, 2, pixels);
    textureIndex = vec_push(&DATA(textureBuffers),  &textureBuffer);

    new_handle = (TextureHandle)vec_push(&RendererData.textureLibrary.store, &newTexture);
  }
  else
  {
    AssetHandle image_handle = Asset->load(imagePath);
    ImageAsset imageAsset = ImageLoader->loadAsset(image_handle);

    switch (imageAsset.format) {
      case EV_IMAGEFORMAT_R8G8B8A8_SRGB:
        format = VK_FORMAT_R8G8B8A8_SRGB;
    }

    format = VK_FORMAT_R8G8B8A8_SRGB;

    newTexture.bufferSize = imageAsset.bufferSize;
    newTexture.width = imageAsset.width;
    newTexture.height = imageAsset.height;

    textureBuffer = ev_vulkan_registerTexture(format, imageAsset.width, imageAsset.height, imageAsset.data);
    textureIndex = vec_push(&DATA(textureBuffers),  &textureBuffer);

    new_handle = (TextureHandle)vec_push(&RendererData.textureLibrary.store, &newTexture);
  }

  Hashmap(evstring, TextureHandle).push(DATA(textureLibrary).map, evstring_new(imagePath), new_handle);
  RendererData.textureLibrary.dirty = true;
  DEBUG_ASSERT(textureIndex == new_handle);
  return new_handle;
}

RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName)
{
  RenderComponent newComponent = { 0 };

  newComponent.materialIndex = ev_renderer_getMaterial(materialName);
  newComponent.pipelineIndex = RendererData.materialLibrary.pipelineHandles[newComponent.materialIndex];
  newComponent.mesh = RendererData.meshLibrary.store[ev_renderer_registerMesh(meshPath)];

  return newComponent;
}

LightComponent ev_renderer_registerLightomponent(evjson_t *json_context, evjson_t *light_ID)
{
  LightComponent newComponent = { 0 };

  for (size_t j = 0; j < 4; j++)
  {
    evstring light_Color = evstring_newfmt("%s.color[%d]", light_ID, j);
    ((float*)&newComponent.color)[j] = (float)evjs_get(json_context, light_Color)->as_num;
    evstring_free(light_Color);
  }

  evstring light_Intensity = evstring_newfmt("%s.intensity", light_ID);
  newComponent.intensity = (float)evjs_get(json_context , light_Intensity)->as_num;
  evstring_free(light_Intensity);

  return newComponent;
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

    for (size_t j = 0; j < 4; j++)
    {
      evstring material_basecolor = evstring_newfmt("%s[%d].baseColor[%d]", list_name, i, j);
      ((float*)&newMaterial.baseColor)[j] = (float)evjs_get(json_context, material_basecolor)->as_num;

      evstring_free(material_basecolor);
    }

    evstring albedo_jsonid = evstring_newfmt("%s[%d].albedoTexture", list_name, i);
    evjson_entry *albedoEntry = evjs_get(json_context, albedo_jsonid);
    if (albedoEntry) {
      evstring albedo = evstring_refclone(albedoEntry->as_str);
      newMaterial.albedoTexture = ev_renderer_registerTexture(albedo);
      evstring_free(albedo);
    }
    else {
      newMaterial.albedoTexture = 0;
    }
    evstring_free(albedo_jsonid);

    evstring normal_jsonid = evstring_newfmt("%s[%d].normalTexture", list_name, i);
    evjson_entry *normalEntry = evjs_get(json_context, normal_jsonid);
    if (normalEntry) {
      evstring normal = evstring_refclone(normalEntry->as_str);
      newMaterial.normalTexture = ev_renderer_registerTexture(normal);
      evstring_free(normal);
    }
    else {
      newMaterial.normalTexture = 0;
    }
    evstring_free(normal_jsonid);

    evstring metallicFactor_jsonid = evstring_newfmt("%s[%d].metallicFactor", list_name, i);
    evjson_entry *metallicFactorEntry = evjs_get(json_context, metallicFactor_jsonid);
    if (metallicFactorEntry) {
      newMaterial.metallicFactor = (float)metallicFactorEntry->as_num;
    }
    else {
      newMaterial.metallicFactor = 0;
    }
    evstring_free(metallicFactor_jsonid);

    evstring roughnessFactor_jsonid = evstring_newfmt("%s[%d].roughnessFactor", list_name, i);
    evjson_entry *roughnessFactorEntry = evjs_get(json_context, roughnessFactor_jsonid);
    if (roughnessFactorEntry) {
      newMaterial.roughnessFactor = (float)roughnessFactorEntry->as_num;
    }
    else {
      newMaterial.roughnessFactor = 0;
    }
    evstring_free(roughnessFactor_jsonid);

    evstring metallicRoughnessTexture_jsonid = evstring_newfmt("%s[%d].metallicRoughnessTexture", list_name, i);
    evjson_entry *metallicRoughnessTextureEntry = evjs_get(json_context, metallicRoughnessTexture_jsonid);
    if (metallicRoughnessTextureEntry) {
      evstring metallicRoughnessTexture = evstring_refclone(metallicRoughnessTextureEntry->as_str);
      newMaterial.metallicRoughnessTexture = ev_renderer_registerTexture(metallicRoughnessTexture);
      evstring_free(metallicRoughnessTexture);
    }
    else {
      newMaterial.metallicRoughnessTexture = 0;
    }
    evstring_free(metallicRoughnessTexture_jsonid);

// TODO fix this
    evstring materialPipeline_jsonid = evstring_newfmt("%s[%d].pipeline", list_name, i);
    PipelineHandle materialPipelineHandle;
    if (evjs_get(json_context, materialPipeline_jsonid))
    {
      evstring materialPipeline = evstring_refclone(evjs_get(json_context, materialPipeline_jsonid)->as_str);
      materialPipelineHandle = ev_renderer_getPipeline(materialPipeline);
      evstring_free(materialPipeline);
    }
    else {
      materialPipelineHandle = ev_renderer_getPipeline(DEFAULTPIPELINE);
    }
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
  ev_renderer_registerLightPipeline();

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

      ShaderAsset shaderAsset = ShaderLoader->loadAsset(asset, shaderAssetStage, shaderPath, NULL, EV_SHADER_BIN);

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

void FrameData_init(FrameData *frame)
{
  frame->objectComponents = vec_init(RenderComponent);
  frame->objectTranforms = vec_init(Matrix4x4);
  pthread_mutex_init(&frame->objectMutex, NULL);

  frame->lightObjects = vec_init(LightObject);
  pthread_mutex_init(&frame->lightMutex, NULL);
}

void FrameData_fini(FrameData *frame)
{
  vec_fini(frame->objectComponents);

  vec_fini(frame->objectTranforms);

  vec_fini(frame->lightObjects);

  pthread_mutex_destroy(&frame->objectMutex);

  pthread_mutex_destroy(&frame->lightMutex);
}

void FrameData_clear(FrameData *frame)
{
  vec_clear(frame->objectComponents);

  vec_clear(frame->objectTranforms);

  vec_clear(frame->lightObjects);
}

EV_CONSTRUCTOR
{
  game_module = evol_loadmodule_weak("game");        DEBUG_ASSERT(game_module);
  imports(game_module  , (Game, Camera, Object, Scene));

  asset_module = evol_loadmodule("assetmanager"); DEBUG_ASSERT(asset_module);
  imports(asset_module  , (AssetManager, Asset, MeshLoader, ShaderLoader, ImageLoader));

  window_module  = evol_loadmodule("window");     DEBUG_ASSERT(window_module);
  imports(window_module, (Window));

  IMPORT_EVENTS_evmod_glfw(window_module);

  ACTIVATE_EVENT_LISTENER(WindowResizedListener, WindowResizedEvent);
  RendererData.windowResized = false;

  FrameData_init(&DATA(currentFrame));
  RendererData.frameNumber = 0;

  RendererData.textureBuffers = vec_init(EvTexture, NULL, ev_vulkan_destroytexture);
  RendererData.vertexBuffers  = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  RendererData.indexBuffers   = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);
  RendererData.customBuffers  = vec_init(EvBuffer, NULL, ev_vulkan_destroybuffer);

  ev_vulkan_init();

  ev_renderer_globalsetsinit();

  materialLibraryInit(&DATA(materialLibrary));
  pipelineLibraryInit(&DATA(pipelineLibrary));
  textureLibraryInit(&(DATA(textureLibrary)));
  meshLibraryInit(&DATA(meshLibrary));

  ev_renderer_registerTexture(DEFAULTEXTURE);
}

EV_DESTRUCTOR
{
  ev_vulkan_wait();

  vec_fini(DATA(textureBuffers));
  vec_fini(DATA(customBuffers));
  vec_fini(DATA(vertexBuffers));
  vec_fini(DATA(indexBuffers));

  meshLibraryDestroy(DATA(meshLibrary));
  textureLibraryDestroy(DATA(textureLibrary));
  materialLibraryDestroy(DATA(materialLibrary));
  pipelineLibraryDestroy(DATA(pipelineLibrary));

  ev_renderer_globalsetsdinit();
  destroyPipeline(&DATA(lightPipeline));

  ev_vulkan_deinit();

  FrameData_fini(&DATA(currentFrame));

  evol_unloadmodule(window_module);
  evol_unloadmodule(asset_module);
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
  EV_NS_BIND_FN(Renderer, run, run);
  EV_NS_BIND_FN(Renderer, addFrameObjectData, ev_renderer_addFrameObjectData);
  EV_NS_BIND_FN(Renderer, registerComponent, ev_renderer_registerRenderComponent);

  EV_NS_BIND_FN(Material, readJSONList, ev_material_readjsonlist);

  EV_NS_BIND_FN(GraphicsPipeline, readJSONList, ev_graphicspipeline_readjsonlist);

  EV_NS_BIND_FN(Light, registerComponent, ev_renderer_registerLightomponent);
  EV_NS_BIND_FN(Light, addFrameLightData, ev_renderer_addFrameLightData);
}

void materialLibraryInit(MaterialLibrary *library)
{
  library->map = Hashmap(evstring, MaterialHandle).new();
  library->store = vec_init(Material);
  library->pipelineHandles = vec_init(PipelineHandle);
  library->dirty = false;
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
  library->dirty = false;
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
  library->dirty = false;
}

void meshLibraryDestroy(MeshLibrary library)
{
  Hashmap(evstring, MeshHandle).free(library.map);
  vec_fini(library.store);
}

void textureLibraryInit(TextureLibrary *library)
{
  library->map = Hashmap(evstring, TextureHandle).new();
  library->store = vec_init(Texture);
  library->dirty = false;
}

void textureLibraryDestroy(TextureLibrary library)
{
  Hashmap(evstring, TextureHandle).free(library.map);
  vec_fini(library.store);
}
