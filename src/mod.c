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

HashmapDefine(evstring, MaterialHandle, evstring_free, NULL);
HashmapDefine(evstring, PipelineHandle, evstring_free, NULL);
HashmapDefine(evstring, MeshHandle, evstring_free, NULL);

typedef struct {

  struct {
    pthread_mutex_t objectMutex;
    vec(RenderComponent) objectComponents;
    vec(Matrix4x4) objectTranforms;
  };

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
} PipelineLibrary;

typedef struct {
  Map(evstring, MeshHandle) map;
  vec(Mesh) store;
} MeshLibrary;

struct ev_Renderer_Data
{
  FrameData currentFrame;

  WindowHandle windowHandle;

  UBO cameraBuffer;

  DescriptorSet cameraSet;
  DescriptorSet resourcesSet;

  MeshLibrary meshLibrary;
  PipelineLibrary pipelineLibrary;
  MaterialLibrary materialLibrary;

  EvBuffer materialsBuffer;
  vec(EvBuffer) vertexBuffers;
  vec(EvBuffer) indexBuffers;
  vec(EvBuffer) customBuffers;
} RendererData;

evolmodule_t game_module;
evolmodule_t asset_module;
evolmodule_t window_module;

void setWindow(WindowHandle handle);
void ev_renderer_updatewindowsize();

void ev_renderer_createSurface();
void ev_renderer_destroysurface();

void run();

int ev_renderer_registervertexbuffer(void *vertices, unsigned long long size, vec(EvBuffer) vertexBuffers);
MeshHandle ev_renderer_registerMesh(CONST_STR meshPath);
RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName);



void ev_renderer_updatewindowsize()
{
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  Window->getSize(DATA(windowHandle), &swapchain->windowExtent.width, &swapchain->windowExtent.height);
}

void ev_renderer_createSurface()
{
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  VK_ASSERT(Window->createVulkanSurface(DATA(windowHandle), ev_vulkan_getinstance(), &swapchain->surface));
  ev_vulkan_checksurfacecompatibility();
}

void ev_renderer_destroysurface(VkSurfaceKHR surface)
{
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  vkDestroySurfaceKHR(ev_vulkan_getinstance(), surface, NULL);
}

void draw(VkCommandBuffer cmd)
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

  for (size_t componentIndex = 0; componentIndex < vec_len(DATA(currentFrame).objectComponents); componentIndex++)
  {
    RenderComponent component = DATA(currentFrame).objectComponents[componentIndex];
    Pipeline pipeline = DATA(pipelineLibrary.store[component.pipelineIndex]);

    MeshPushConstants pushconstant;
    memcpy(pushconstant.transform, DATA(currentFrame).objectTranforms[componentIndex], sizeof(Matrix4x4));
    pushconstant.indexBufferIndex = component.mesh.indexBufferIndex;
    pushconstant.vertexBufferIndex = component.mesh.indexBufferIndex;
    pushconstant.materialIndex = component.materialIndex;

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

void ev_renderer_addFrameObjectData(RenderComponent *components, Matrix4x4 *transforms, uint32_t count)
{
  pthread_mutex_lock(&DATA(currentFrame).objectMutex);

  vec_append(&DATA(currentFrame).objectComponents, &components, count);

  vec_append(&DATA(currentFrame).objectTranforms, &transforms, count);

  pthread_mutex_unlock(&DATA(currentFrame).objectMutex);
}

void run()
{
  VkCommandBuffer cmd = ev_vulkan_startframe();

  draw(cmd);

  ev_vulkan_endframe(cmd);

  FrameData_clear(&DATA(currentFrame));
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
    .renderPass = ev_vulkan_getrenderpass(),
  };

  ev_pipeline_build(pipelineCreateInfo, &newPipeline);

  for (size_t i = 0; i < vec_len(newPipeline.pSets); i++)
  ev_descriptormanager_allocate(newPipeline.pSets[i].layout, &newPipeline.pSets[i].set);

  PipelineHandle new_handle = (PipelineHandle)vec_push(&RendererData.pipelineLibrary.store, &newPipeline);
  Hashmap(evstring, MaterialHandle).push(DATA(pipelineLibrary).map, evstring_new(pipelineName), new_handle);

  return new_handle;
}

void destroyPipeline(Pipeline *pipeline)
{
  ev_vulkan_destroypipeline(pipeline->pipeline);
  ev_vulkan_destroypipelinelayout(pipeline->pipelineLayout);

  for (size_t i = 0; i < vec_len(pipeline->pSets); i++)
  {
    ev_vulkan_destroysetlayout(pipeline->pSets[i].layout);
  }
}

RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName)
{
  RenderComponent newComponent = { 0 };

  Mesh mesh = RendererData.meshLibrary.store[ev_renderer_registerMesh(meshPath)];

  //TODO setup mesh index and regester it

  newComponent.materialIndex = ev_renderer_getMaterial(materialName);
  newComponent.pipelineIndex = RendererData.materialLibrary.pipelineHandles[newComponent.materialIndex];

  return newComponent;
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
  ev_vulkan_createEvswapchain();
  ev_vulkan_createrenderpass();
  ev_vulkan_createframebuffers();
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

void FrameData_init(FrameData *frame)
{
  frame->objectComponents = vec_init(RenderComponent);

  frame->objectTranforms = vec_init(Matrix4x4);

  pthread_mutex_init(&frame->objectMutex, NULL);
}

void FrameData_fini(FrameData *frame)
{
  vec_fini(frame->objectComponents);

  vec_fini(frame->objectTranforms);

  pthread_mutex_destroy(&frame->objectMutex);
}

void FrameData_clear(FrameData *frame)
{
  vec_clear(frame->objectComponents);

  vec_clear(frame->objectTranforms);
}

void ev_renderer_globalsetsdinit()
{
  //cameraSet
  ev_vulkan_freeubo(&DATA(cameraBuffer));
  ev_vulkan_destroysetlayout(RendererData.cameraSet.layout);

  //Resources set
  ev_vulkan_destroybuffer(&RendererData.materialsBuffer);
  ev_vulkan_destroysetlayout(RendererData.resourcesSet.layout);
}

EV_CONSTRUCTOR
{
  game_module = evol_loadmodule_weak("game");        DEBUG_ASSERT(game_module);
  imports(game_module  , (Game, Camera, Object, Scene));

  asset_module = evol_loadmodule("assetmanager"); DEBUG_ASSERT(asset_module);
  imports(asset_module  , (AssetManager, Asset, MeshLoader, ShaderLoader));

  window_module  = evol_loadmodule("window");     DEBUG_ASSERT(window_module);
  imports(window_module, (Window));

  FrameData_init(&DATA(currentFrame));

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
  ev_vulkan_wait();

  vec_fini(DATA(customBuffers));
  vec_fini(DATA(vertexBuffers));
  vec_fini(DATA(indexBuffers));

  meshLibraryDestroy(DATA(meshLibrary));
  materialLibraryDestroy(DATA(materialLibrary));
  pipelineLibraryDestroy(DATA(pipelineLibrary));

  ev_renderer_globalsetsdinit();

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

  EV_NS_BIND_FN(Material, readJSONList, ev_material_readjsonlist);

  EV_NS_BIND_FN(GraphicsPipeline, readJSONList, ev_graphicspipeline_readjsonlist);
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
