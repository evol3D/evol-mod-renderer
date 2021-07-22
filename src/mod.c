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

#include <SyncManager/SyncManager.h>
#include <RenderPass/RenderPass.h>

#define DEFAULTPIPELINE "DefaultPipeline"
#define DEFAULTEXTURE "DefaultTexture"

#define BINDLESSARRAYSIZE 2000

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

  Pipeline shadowmapPipeline;
  Pipeline lightPipeline;
  Pipeline skyboxPipeline;
  Pipeline fxaaPipeline;

  EvTexture skyboxTexture;

  uint32_t frameNumber;

  RenderPass offscreenPass;
  RenderPass shadowmapPass;
  RenderPass lightPass;
  RenderPass skyboxPass;

  VkSemaphore offscreenRendering[SWAPCHAIN_MAX_IMAGES];
  VkSemaphore shadowmapRendering[SWAPCHAIN_MAX_IMAGES];
  VkSemaphore lightRendering[SWAPCHAIN_MAX_IMAGES];
  VkSemaphore skyboxRendering[SWAPCHAIN_MAX_IMAGES];

  VkCommandBuffer offscreencommandbuffer[SWAPCHAIN_MAX_IMAGES];
  VkCommandBuffer shadowmapcommandbuffer[SWAPCHAIN_MAX_IMAGES];
  VkCommandBuffer lightcommandbuffer[SWAPCHAIN_MAX_IMAGES];
  VkCommandBuffer skyboxcommandbuffer[SWAPCHAIN_MAX_IMAGES];

  VkExtent3D extent;
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

void ev_renderer_createoffscreenpass(VkExtent3D passExtent);
void ev_renderer_createshadowmappass(VkExtent3D passExtent);
void ev_renderer_createlightpass(VkExtent3D passExtent);
void ev_renderer_createskyboxtpass(VkExtent3D passExtent);

void ev_renderer_registershadowmapPipeline();
void ev_renderer_registerLightPipeline();
void ev_renderer_registerskyboxPipeline();
void ev_renderer_registerfxaaPipeline();

void ev_renderer_createSurface();

void ev_renderer_registerCubeMap(CONST_STR imagePath);

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
      },
      {
        .binding = 2,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
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
    ev_descriptormanager_allocate(DATA(sceneSet).layout, &DATA(sceneSet).set[0]);

    //sceneBuffer
    ev_vulkan_allocateubo(sizeof(EvScene), false, &RendererData.scenesBuffer);
    ev_vulkan_writeintobinding(0, 0, DATA(sceneSet), &DATA(sceneSet).pBindings[0], 0, &(DATA(scenesBuffer).buffer));
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
    ev_descriptormanager_allocate(DATA(cameraSet).layout, &DATA(cameraSet).set[0]);

    ev_vulkan_allocateubo(sizeof(CameraData), false, &RendererData.cameraBuffer);
    ev_vulkan_writeintobinding(0, 0, DATA(cameraSet), &DATA(cameraSet).pBindings[0], 0, &(DATA(cameraBuffer).buffer));
  }

  //Resources set
  {
    VkDescriptorSetLayoutBinding resourcesbindings[] =
    {
      {
        .binding = 0,
        .descriptorCount = BINDLESSARRAYSIZE,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      },
      {
        .binding = 1,
        .descriptorCount = BINDLESSARRAYSIZE,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      },
      {
        .binding = 2,
        .descriptorCount = BINDLESSARRAYSIZE,
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
        .descriptorCount = BINDLESSARRAYSIZE,
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
    ev_descriptormanager_allocate(DATA(resourcesSet).layout, &DATA(resourcesSet).set[0]);
  }

  //lightsBuffer
  ev_vulkan_allocateubo(UBOMAXSIZE, false, &RendererData.lightsBuffer);
}

void ev_renderer_globalsetsdinit()
{
  //SceneSet
  ev_vulkan_freeubo(&DATA(scenesBuffer));
  ev_vulkan_freeubo(&DATA(lightsBuffer));
  ev_vulkan_destroysetlayout(RendererData.sceneSet.layout);

  //cameraSet
  ev_vulkan_freeubo(&DATA(cameraBuffer));
  ev_vulkan_destroysetlayout(RendererData.cameraSet.layout);

  //Resources set
  ev_vulkan_destroybuffer(&RendererData.materialsBuffer);
  // ev_vulkan_destroysetlayout(RendererData.resourcesSet.layout);
}

void setWindow(WindowHandle handle)
{
  DATA(windowHandle) = handle;

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  Window->getSize(DATA(windowHandle), &swapchain->windowExtent.width, &swapchain->windowExtent.height);

  RendererData.extent.width = swapchain->windowExtent.width,
  RendererData.extent.height = swapchain->windowExtent.height,
  RendererData.extent.depth = 1,

  ev_renderer_createSurface();
  ev_vulkan_createEvswapchain(framebuffering_degree);

  ev_renderer_createoffscreenpass(RendererData.extent);
  ev_renderer_createshadowmappass(RendererData.extent);
  ev_renderer_createlightpass(RendererData.extent);
  ev_renderer_createskyboxtpass(RendererData.extent);

  ev_vulkan_createrenderpass();
  ev_vulkan_createframebuffers();
}

void ev_renderer_createoffscreenpass(VkExtent3D passExtent)
{
  PassAttachment attachmentDescriptions[] = {
    //position
    {
      .subpass = 0,

      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    //normal
    {
      .subpass = 0,

      .format = VK_FORMAT_R16G16B16A16_SFLOAT,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    //albedo
    {
      .subpass = 0,

      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    //specular
    {
      .subpass = 0,

      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    //depth
    {
      .subpass = 0,

      .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .type = EV_RENDERPASSATTACHMENT_TYPE_DEPTH,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    },
  };

  VkSubpassDependency dependencies[] = {
    {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
    {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    },
  };

  ev_renderpass_build(SWAPCHAIN_MAX_IMAGES, passExtent, ARRAYSIZE(attachmentDescriptions), attachmentDescriptions, 1, ARRAYSIZE(dependencies), dependencies, &RendererData.offscreenPass);

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();

  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++)
  {
    vkDestroyFramebuffer(ev_vulkan_getlogicaldevice(), RendererData.offscreenPass.framebuffers[i].framebuffer, NULL);

    VkImageView views[] = {
      RendererData.offscreenPass.framebuffers[i].frameAttachments[0].imageView,
      RendererData.offscreenPass.framebuffers[i].frameAttachments[1].imageView,
      RendererData.offscreenPass.framebuffers[i].frameAttachments[2].imageView,
      RendererData.offscreenPass.framebuffers[i].frameAttachments[3].imageView,
      swapchain->depthImageView[i],
    };
    VkFramebufferCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = RendererData.offscreenPass.renderPass,
      .attachmentCount = ARRAYSIZE(views),
      .pAttachments = views,
      .width = RendererData.offscreenPass.extent.width,
      .height = RendererData.offscreenPass.extent.height,
      .layers = RendererData.offscreenPass.extent.depth,
    };
    VK_ASSERT(vkCreateFramebuffer(ev_vulkan_getlogicaldevice(), &createInfo, NULL, &RendererData.offscreenPass.framebuffers[i].framebuffer));
  }
}

void ev_renderer_createshadowmappass(VkExtent3D passExtent)
{
  PassAttachment attachmentDescriptions[] = {
    //depth
    {
      .subpass = 0,

      .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .type = EV_RENDERPASSATTACHMENT_TYPE_DEPTH,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
    },
  };

  ev_renderpass_build(SWAPCHAIN_MAX_IMAGES, passExtent, ARRAYSIZE(attachmentDescriptions), attachmentDescriptions, 1, 0, NULL, &RendererData.shadowmapPass);

  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++)
  {
    vkDestroySampler(ev_vulkan_getlogicaldevice(), RendererData.shadowmapPass.framebuffers[i].frameAttachments[0].sampler, NULL);

    VkSamplerCreateInfo samplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_TRUE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .mipLodBias = 0.0f,
      .minLod = 0.0f,
      .maxLod = 0.0f,
    };
    vkCreateSampler(ev_vulkan_getlogicaldevice(), &samplerCreateInfo, NULL, &RendererData.shadowmapPass.framebuffers[i].frameAttachments[0].sampler);
  }
}

void ev_renderer_createlightpass(VkExtent3D passExtent)
{
  PassAttachment attachmentDescriptions[] = {
    //position
    {
      .subpass = 0,

      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
  };

  ev_renderpass_build(SWAPCHAIN_MAX_IMAGES, passExtent, ARRAYSIZE(attachmentDescriptions), attachmentDescriptions, 1, 0, NULL, &RendererData.lightPass);

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();

  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++) {
    vkDestroyFramebuffer(ev_vulkan_getlogicaldevice(), RendererData.lightPass.framebuffers[i].framebuffer, NULL);

    VkImageView views[] = {
      RendererData.lightPass.framebuffers[i].frameAttachments[0].imageView,
    };
    VkFramebufferCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = RendererData.lightPass.renderPass,
      .attachmentCount = ARRAYSIZE(views),
      .pAttachments = views,
      .width = RendererData.lightPass.extent.width,
      .height = RendererData.lightPass.extent.height,
      .layers = RendererData.lightPass.extent.depth,
    };
    VK_ASSERT(vkCreateFramebuffer(ev_vulkan_getlogicaldevice(), &createInfo, NULL, &RendererData.lightPass.framebuffers[i].framebuffer));
  }
}

void ev_renderer_createskyboxtpass(VkExtent3D passExtent)
{
  PassAttachment attachmentDescriptions[] = {
    //position
    {
      .subpass = 0,

      .format = VK_FORMAT_B8G8R8A8_UNORM,
      .type = EV_RENDERPASSATTACHMENT_TYPE_COLOR,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .useLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
    },
    //depth
    {
      .subpass = 0,

      .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
      .type = EV_RENDERPASSATTACHMENT_TYPE_DEPTH,
      .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .useLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,

      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

      .extent = passExtent,
      .usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    },
  };

  ev_renderpass_build(SWAPCHAIN_MAX_IMAGES, passExtent, ARRAYSIZE(attachmentDescriptions), attachmentDescriptions, 1, 0, NULL, &RendererData.skyboxPass);

  EvSwapchain *swapchain = ev_vulkan_getSwapchain();

  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++) {
    vkDestroyFramebuffer(ev_vulkan_getlogicaldevice(), RendererData.skyboxPass.framebuffers[i].framebuffer, NULL);

    VkImageView views[] = {
      RendererData.lightPass.framebuffers[i].frameAttachments[0].imageView,
      swapchain->depthImageView[i],
    };
    VkFramebufferCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = RendererData.skyboxPass.renderPass,
      .attachmentCount = ARRAYSIZE(views),
      .pAttachments = views,
      .width = RendererData.skyboxPass.extent.width,
      .height = RendererData.skyboxPass.extent.height,
      .layers = RendererData.skyboxPass.extent.depth,
    };
    VK_ASSERT(vkCreateFramebuffer(ev_vulkan_getlogicaldevice(), &createInfo, NULL, &RendererData.skyboxPass.framebuffers[i].framebuffer));
  }
}

void ev_renderer_registershadowmapPipeline()
{
  AssetHandle vertAsset= Asset->load("shaders://shadowmap.vert");
  ShaderAsset shaderVertAsset = ShaderLoader->loadAsset(vertAsset, EV_SHADERASSETSTAGE_VERTEX, "shadowmap.vert", NULL, EV_SHADER_BIN);

  Shader shaders[] = {
    {
      .data = shaderVertAsset.binary,
      .length = shaderVertAsset.len,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
    },
  };

  RendererData.shadowmapPipeline.pSets = vec_init(DescriptorSet);

  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
    .colorWriteMask =
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_A_BIT ,
  };

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .lineWidth = 1.0,
  };

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &pipelineColorBlendAttachmentState,
  };

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .depthBoundsTestEnable = VK_FALSE,
    .back = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .front = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .stencilTestEnable = VK_FALSE,
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = ARRAYSIZE(shaders),
    .pShaders = shaders,
    .renderPass = RendererData.shadowmapPass.renderPass,
    .pColorBlendState = &pipelineColorBlendState,
    .pDepthStencilState = &pipelineDepthStencilState,
    .pRasterizationState = &pipelineRasterizationState,
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  vec_push(&overrides, &RendererData.resourcesSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.shadowmapPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(RendererData.shadowmapPipeline.pSets); i++)
  {
    for (size_t j = 0; j < SWAPCHAIN_MAX_IMAGES; j++)
    {
      ev_descriptormanager_allocate(RendererData.shadowmapPipeline.pSets[i].layout, &RendererData.shadowmapPipeline.pSets[i].set[j]);
    }
  }

  ev_vulkan_writeintobinding(0, 0, DATA(shadowmapPipeline.pSets[1]), &DATA(shadowmapPipeline.pSets[1]).pBindings[0], 0, &(DATA(lightsBuffer).buffer));
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
    .depthTestEnable = VK_FALSE,
  };

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_NONE,
    .lineWidth = 1.0,
  };

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = ARRAYSIZE(shaders),
    .pShaders = shaders,
    .renderPass = RendererData.lightPass.renderPass,
    .pColorBlendState = &pipelineColorBlendState,
    .pDepthStencilState = &pipelineDepthStencilState,
    .pRasterizationState = &pipelineRasterizationState,
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.lightPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(RendererData.lightPipeline.pSets); i++)
  {
    for (size_t j = 0; j < SWAPCHAIN_MAX_IMAGES; j++)
    {
      ev_descriptormanager_allocate(RendererData.lightPipeline.pSets[i].layout, &RendererData.lightPipeline.pSets[i].set[j]);
    }
  }

  ev_vulkan_writeintobinding(0, 0, DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[0], 0, &(DATA(cameraBuffer).buffer));
  ev_vulkan_writeintobinding(0, 0, DATA(lightPipeline.pSets[1]), &DATA(lightPipeline.pSets[1]).pBindings[1], 0, &(DATA(lightsBuffer).buffer));
}

void ev_renderer_registerskyboxPipeline()
{
  RendererData.skyboxPipeline.pSets = vec_init(DescriptorSet);

  AssetHandle vertAsset= Asset->load("shaders://skybox.vert");
  ShaderAsset shaderVertAsset = ShaderLoader->loadAsset(vertAsset, EV_SHADERASSETSTAGE_VERTEX, "skybox.vert", NULL, EV_SHADER_BIN);

  AssetHandle fragAsset = Asset->load("shaders://skybox.frag");
  ShaderAsset shaderFragAsset = ShaderLoader->loadAsset(fragAsset, EV_SHADERASSETSTAGE_FRAGMENT, "skybox.frag", NULL, EV_SHADER_BIN);

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

  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
    .colorWriteMask =
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_A_BIT ,
  };

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .lineWidth = 1.0,
  };

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &pipelineColorBlendAttachmentState,
  };

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_FALSE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .depthBoundsTestEnable = VK_FALSE,
    .back = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .front = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .stencilTestEnable = VK_FALSE,
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = ARRAYSIZE(shaders),
    .pShaders = shaders,
    .renderPass = RendererData.skyboxPass.renderPass,
    .pColorBlendState = &pipelineColorBlendState,
    .pDepthStencilState = &pipelineDepthStencilState,
    .pColorBlendState = &pipelineColorBlendState,
    .pRasterizationState = &pipelineRasterizationState
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.skyboxPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(RendererData.skyboxPipeline.pSets); i++)
    for (size_t j = 0; j < SWAPCHAIN_MAX_IMAGES; j++) {
      ev_descriptormanager_allocate(RendererData.skyboxPipeline.pSets[i].layout, &RendererData.skyboxPipeline.pSets[i].set[j]);
    }

  Framebuffer offscreenFrameBuffer = RendererData.offscreenPass.framebuffers[0];
  Framebuffer lightFrameBuffer = RendererData.lightPass.framebuffers[0];

  ev_renderer_registerCubeMap("assets://textures/");

  ev_vulkan_writeintobinding(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(skyboxPipeline.pSets[0]), &DATA(skyboxPipeline.pSets[0]).pBindings[0], 0, &RendererData.skyboxTexture);
  ev_vulkan_writeintobinding(0, 0, DATA(skyboxPipeline.pSets[1]), &DATA(skyboxPipeline.pSets[1]).pBindings[0], 0, &(DATA(cameraBuffer).buffer));

  ev_vulkan_writeintobinding(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(sceneSet), &DATA(sceneSet).pBindings[2], 0, &RendererData.skyboxTexture);
}

void ev_renderer_registerfxaaPipeline()
{
  RendererData.fxaaPipeline.pSets = vec_init(DescriptorSet);

  AssetHandle vertAsset= Asset->load("shaders://fxaa.vert");
  ShaderAsset shaderVertAsset = ShaderLoader->loadAsset(vertAsset, EV_SHADERASSETSTAGE_VERTEX, "fxaa.vert", NULL, EV_SHADER_BIN);

  AssetHandle fragAsset = Asset->load("shaders://fxaa.frag");
  ShaderAsset shaderFragAsset = ShaderLoader->loadAsset(fragAsset, EV_SHADERASSETSTAGE_FRAGMENT, "fxaa.frag", NULL, EV_SHADER_BIN);

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

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .cullMode = VK_CULL_MODE_NONE,
    .lineWidth = 1.0,
  };

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_FALSE,
  };

  EvGraphicsPipelineCreateInfo pipelineCreateInfo = {
    .stageCount = ARRAYSIZE(shaders),
    .pShaders = shaders,
    .renderPass = ev_vulkan_getrenderpass(),
    .pColorBlendState = &pipelineColorBlendState,
    .pDepthStencilState = &pipelineDepthStencilState,
    .pColorBlendState = &pipelineColorBlendState,
    .pRasterizationState = &pipelineRasterizationState
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &RendererData.fxaaPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(RendererData.fxaaPipeline.pSets); i++)
    for (size_t j = 0; j < SWAPCHAIN_MAX_IMAGES; j++) {
      ev_descriptormanager_allocate(RendererData.fxaaPipeline.pSets[i].layout, &RendererData.fxaaPipeline.pSets[i].set[j]);
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
    ev_vulkan_writeintobinding(0, 0, DATA(resourcesSet), &DATA(resourcesSet).pBindings[3], 0, &RendererData.materialsBuffer);

    DATA(materialLibrary).dirty = false;
  }

  if (DATA(meshLibrary).dirty)
  {
    ev_vulkan_wait();
    for (size_t i = 0; i < vec_len(RendererData.indexBuffers); i++) {
      ev_vulkan_writeintobinding(0, 0, DATA(resourcesSet), &DATA(resourcesSet).pBindings[2], i, &(DATA(indexBuffers)[i].buffer));
    }

    for (size_t i = 0; i < vec_len(RendererData.vertexBuffers); i++) {
      ev_vulkan_writeintobinding(0, 0, DATA(resourcesSet), &DATA(resourcesSet).pBindings[1], i, &(DATA(vertexBuffers)[i].buffer));
    }

    DATA(meshLibrary).dirty = false;
  }

  if (DATA(textureLibrary).dirty)
  {
    ev_vulkan_wait();
    for (size_t i = 0; i < vec_len(RendererData.textureBuffers); i++) {
      ev_vulkan_writeintobinding(0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(resourcesSet), &DATA(resourcesSet).pBindings[4], i, &(DATA(textureBuffers)[i]));
    }

    DATA(textureLibrary).dirty = false;
  }

  if (RendererData.windowResized)
  {
    ev_vulkan_recreateSwapChain();
    RendererData.windowResized = false;
  }

  VkCommandBuffer cmd;
  uint32_t swapchainImageIndex;
  EvSwapchain *swapchain = ev_vulkan_getSwapchain();
  uint32_t frameNumber = RendererData.frameNumber % framebuffering_degree;
  VkCommandBufferBeginInfo cmdBeginInfo = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = NULL,

    .pInheritanceInfo = NULL,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_ASSERT(vkWaitForFences(ev_vulkan_getlogicaldevice(), 1, &swapchain->renderFences[frameNumber], true, ~0ull));
  VK_ASSERT(vkResetFences(ev_vulkan_getlogicaldevice(), 1, &swapchain->renderFences[frameNumber]));

  vkAcquireNextImageKHR(ev_vulkan_getlogicaldevice(), swapchain->swapchain, ~0ull, swapchain->presentSemaphores[frameNumber], NULL, &swapchainImageIndex);

  Framebuffer framebuffer = RendererData.offscreenPass.framebuffers[swapchainImageIndex];
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[0], 0, &framebuffer.frameAttachments[0]);
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[1], 0, &framebuffer.frameAttachments[1]);
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[2], 0, &framebuffer.frameAttachments[2]);
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[3], 0, &framebuffer.frameAttachments[3]);

  framebuffer = RendererData.shadowmapPass.framebuffers[swapchainImageIndex];
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, DATA(lightPipeline.pSets[0]), &DATA(lightPipeline.pSets[0]).pBindings[4], 0, &framebuffer.frameAttachments[0]);

  framebuffer = RendererData.lightPass.framebuffers[swapchainImageIndex];
  ev_vulkan_writeintobinding(swapchainImageIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, DATA(fxaaPipeline.pSets[0]), &DATA(fxaaPipeline.pSets[0]).pBindings[0], 0, &framebuffer.frameAttachments[0]);

  /////////////////////////////
  //First pass
  {
    cmd = DATA(offscreencommandbuffer)[frameNumber];
    VK_ASSERT(vkResetCommandBuffer(cmd, 0));
    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    VkClearValue clearValuesoffscreen[] =
    {
      {
        .color = { {0.0f, 0.0f, 0.0f, 1.f} },
      },
      {
        .color = { {0.0f, 0.0f, 0.0f, 1.f} },
      },
      {
        .color = { {0.0f, 0.0f, 0.0f, 1.f} },
      },
      {
        .color = { {0.0f, 0.0f, 0.0f, 1.f} },
      },
      {
        .depthStencil = {1.0f, 0.0f},
      }
    };

    VkRenderPassBeginInfo rpInfooffscreen = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,

      .renderPass = DATA(offscreenPass).renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent.width = DATA(offscreenPass).extent.width,
      .renderArea.extent.height = DATA(offscreenPass).extent.height,
      .framebuffer = DATA(offscreenPass).framebuffers[swapchainImageIndex].framebuffer,

      .clearValueCount = ARRAYSIZE(clearValuesoffscreen),
      .pClearValues = &clearValuesoffscreen,
    };
    vkCmdBeginRenderPass(cmd, &rpInfooffscreen, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkRect2D scissor = {
        .offset = {0, 0},
        .extent = (VkExtent2D) {
          .width = DATA(offscreenPass).extent.width,
          .height = DATA(offscreenPass).extent.height,
        },
      };

      VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = DATA(offscreenPass).extent.width,
        .height = (float) DATA(offscreenPass).extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };

      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    CameraData cam;
    Camera->getViewMat(NULL, NULL, cam.viewMat);
    Camera->getProjectionMat(NULL, NULL, cam.projectionMat);

    ev_vulkan_updateubo(sizeof(CameraData), &cam, &DATA(cameraBuffer));

    VkPipeline oldPipeline;
    for (size_t componentIndex = 0; componentIndex < vec_len(DATA(currentFrame).objectComponents); componentIndex++)
    {
      RenderComponent component = DATA(currentFrame).objectComponents[componentIndex];
      ev_log_debug("mesh # %d, vertexbuffer: %d, indexbuffer: %d", componentIndex, component.mesh.vertexBufferIndex, component.mesh.indexBufferIndex);
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
        ds[i] = pipeline.pSets[i].set[0];
      }

      ds[0] = DATA(sceneSet).set[0];
      ds[1] = DATA(cameraSet).set[0];
      ds[2] = DATA(resourcesSet).set[0];

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, vec_len(pipeline.pSets), ds, 0, 0);

      vkCmdDraw(cmd, component.mesh.indexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    VK_ASSERT(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,

      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &swapchain->presentSemaphores[frameNumber],
      .pWaitDstStageMask = &waitStage,

      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,

      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &DATA(offscreenRendering)[frameNumber],
    };

    VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, VK_NULL_HANDLE));
  }
  // end First pass
  /////////////////////////////

  /////////////////////////////
  //Second pass
  {
    cmd = DATA(shadowmapcommandbuffer)[frameNumber];
    VK_ASSERT(vkResetCommandBuffer(cmd, 0));
    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    VkClearValue clearValuesoffscreen[] =
    {
      {
        .depthStencil = {1.0f, 0.0f},
      }
    };

    VkRenderPassBeginInfo rpInfooffscreen = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,

      .renderPass = DATA(shadowmapPass).renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent.width = DATA(shadowmapPass).extent.width,
      .renderArea.extent.height = DATA(shadowmapPass).extent.height,
      .framebuffer = DATA(shadowmapPass).framebuffers[swapchainImageIndex].framebuffer,

      .clearValueCount = ARRAYSIZE(clearValuesoffscreen),
      .pClearValues = &clearValuesoffscreen,
    };
    vkCmdBeginRenderPass(cmd, &rpInfooffscreen, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkRect2D scissor = {
        .offset = {0, 0},
        .extent = (VkExtent2D) {
          .width = DATA(shadowmapPass).extent.width,
          .height = DATA(shadowmapPass).extent.height,
        },
      };

      VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = DATA(shadowmapPass).extent.width,
        .height = (float) DATA(shadowmapPass).extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };

      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    ev_vulkan_updateubo(sizeof(LightObject) * vec_len(DATA(currentFrame).lightObjects), RendererData.currentFrame.lightObjects, &(DATA(lightsBuffer).buffer));

    VkPipeline oldPipeline;
    for (size_t componentIndex = 0; componentIndex < vec_len(DATA(currentFrame).objectComponents); componentIndex++)
    {
      RenderComponent component = DATA(currentFrame).objectComponents[componentIndex];
      Pipeline pipeline = RendererData.shadowmapPipeline;

      ShadowmapPushConstants pushconstant;
      memcpy(pushconstant.transform, DATA(currentFrame).objectTranforms[componentIndex], sizeof(Matrix4x4));
      pushconstant.indexBufferIndex = component.mesh.indexBufferIndex;
      pushconstant.vertexBufferIndex = component.mesh.indexBufferIndex;

      vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(ShadowmapPushConstants), &pushconstant);

      if (oldPipeline != pipeline.pipeline)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        oldPipeline = pipeline.pipeline;
      }
      VkDescriptorSet ds[4];
      for (size_t i = 0; i < vec_len(pipeline.pSets); i++)
      {
        ds[i] = pipeline.pSets[i].set[0];
      }

      ds[0] = DATA(resourcesSet).set[0];

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, vec_len(pipeline.pSets), ds, 0, 0);

      vkCmdDraw(cmd, component.mesh.indexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    VK_ASSERT(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
    };

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.waitSemaphoreCount = 0;
    submit.pWaitDstStageMask = 0;
    submit.pWaitSemaphores = 0,

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &DATA(shadowmapRendering)[frameNumber];

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, VK_NULL_HANDLE));
  }
  // end Second pass
  /////////////////////////////

  /////////////////////////////
  //Third pass
  {
    cmd = DATA(lightcommandbuffer)[frameNumber];
    VK_ASSERT(vkResetCommandBuffer(cmd, 0));
    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    VkClearValue clearValuesoffscreen[] =
    {
      {
        .color = { {0.0f, 0.0f, 0.0f, 1.f} },
      }
    };

    VkRenderPassBeginInfo rpInfooffscreen = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,

      .renderPass = DATA(lightPass).renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent.width = DATA(lightPass).extent.width,
      .renderArea.extent.height = DATA(lightPass).extent.height,
      .framebuffer = DATA(lightPass).framebuffers[swapchainImageIndex].framebuffer,

      .clearValueCount = ARRAYSIZE(clearValuesoffscreen),
      .pClearValues = &clearValuesoffscreen,
    };
    vkCmdBeginRenderPass(cmd, &rpInfooffscreen, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkRect2D scissor = {
        .offset = {0, 0},
        .extent = (VkExtent2D) {
          .width = DATA(lightPass).extent.width,
          .height = DATA(lightPass).extent.height,
        },
      };

      VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = DATA(lightPass).extent.width,
        .height = (float) DATA(lightPass).extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };

      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    LightPushConstants lightPushConstants;
    lightPushConstants.lightCount = vec_len(DATA(currentFrame).lightObjects);
    vkCmdPushConstants(cmd, RendererData.lightPipeline.pipelineLayout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(LightPushConstants), &lightPushConstants);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.lightPipeline.pipeline);
    VkDescriptorSet ds[4];
    ds[0] = RendererData.lightPipeline.pSets[0].set[swapchainImageIndex];
    ds[1] = RendererData.lightPipeline.pSets[1].set[0];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.lightPipeline.pipelineLayout, 0, vec_len(RendererData.lightPipeline.pSets), ds, 0, 0);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    VK_ASSERT(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
    };

    VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSemaphore waitSemaphores[] = {
      DATA(offscreenRendering)[frameNumber],
      DATA(shadowmapRendering)[frameNumber],
    };

    submit.waitSemaphoreCount = ARRAYSIZE(waitStages);
    submit.pWaitDstStageMask = waitStages;
    submit.pWaitSemaphores = waitSemaphores;

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &DATA(lightRendering)[frameNumber];

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, VK_NULL_HANDLE));
  }
  // end Third pass
  /////////////////////////////

  /////////////////////////////
  //Fourth pass
  {
    cmd = DATA(skyboxcommandbuffer)[frameNumber];
    VK_ASSERT(vkResetCommandBuffer(cmd, 0));
    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    VkClearValue clearValuesoffscreen[] =
    {
      {
        .color = { {1.0f, 1.0f, 1.0f, 1.f} },
      },
      {
        .depthStencil = {1.0f, 0.0f},
      }
    };

    VkRenderPassBeginInfo rpInfooffscreen = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,

      .renderPass = DATA(skyboxPass).renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent.width = DATA(skyboxPass).extent.width,
      .renderArea.extent.height = DATA(skyboxPass).extent.height,
      .framebuffer = DATA(skyboxPass).framebuffers[swapchainImageIndex].framebuffer,

      .clearValueCount = ARRAYSIZE(clearValuesoffscreen),
      .pClearValues = &clearValuesoffscreen,
    };
    vkCmdBeginRenderPass(cmd, &rpInfooffscreen, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkRect2D scissor = {
        .offset = {0, 0},
        .extent = (VkExtent2D) {
          .width = DATA(skyboxPass).extent.width,
          .height = DATA(skyboxPass).extent.height,
        },
      };

      VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = DATA(skyboxPass).extent.width,
        .height = (float) DATA(skyboxPass).extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };

      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.skyboxPipeline.pipeline);
    VkDescriptorSet ds[4];
    for (size_t i = 0; i < vec_len(RendererData.skyboxPipeline.pSets); i++)
    {
      ds[i] = RendererData.skyboxPipeline.pSets[i].set[0];
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.skyboxPipeline.pipelineLayout, 0, vec_len(RendererData.skyboxPipeline.pSets), ds, 0, 0);
    vkCmdDraw(cmd, 36, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    VK_ASSERT(vkEndCommandBuffer(cmd));
    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
    };

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.waitSemaphoreCount = 1;
    submit.pWaitDstStageMask = &waitStage;
    submit.pWaitSemaphores = &DATA(lightRendering)[frameNumber],

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &DATA(skyboxRendering)[frameNumber];

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, VK_NULL_HANDLE));
  }
  // end Fourth pass
  /////////////////////////////

  /////////////////////////////
  //Fifth pass////
  {
    VK_ASSERT(vkResetCommandBuffer(swapchain->commandBuffers[frameNumber], 0));
    cmd = swapchain->commandBuffers[frameNumber];

    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    VkClearValue clearValues[] =
    {
      {
        .color = { {0.13f, 0.22f, 0.37f, 1.f} },
      },
    };

    VkRenderPassBeginInfo rpInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,

      .renderPass = swapchain->renderPass,
      .renderArea.offset.x = 0,
      .renderArea.offset.y = 0,
      .renderArea.extent.width = swapchain->windowExtent.width,
      .renderArea.extent.height = swapchain->windowExtent.height,
      .framebuffer = swapchain->framebuffers[swapchainImageIndex],

      .clearValueCount = ARRAYSIZE(clearValues),
      .pClearValues = &clearValues,
    };

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkRect2D scissor = {
        .offset = {0, 0},
        .extent = swapchain->windowExtent,
      };

      VkViewport viewport =
      {
        .x = 0,
        .y = 0,
        .width = swapchain->windowExtent.width,
        .height = (float) swapchain->windowExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
      };
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.fxaaPipeline.pipeline);
    VkDescriptorSet ds[4];
    for (size_t i = 0; i < vec_len(RendererData.fxaaPipeline.pSets); i++)
    {
      ds[i] = RendererData.fxaaPipeline.pSets[i].set[swapchainImageIndex];
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererData.fxaaPipeline.pipelineLayout, 0, vec_len(RendererData.fxaaPipeline.pSets), ds, 0, 0);
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
      .image = swapchain->images[swapchainImageIndex],
      .subresourceRange =
      {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      }
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, NULL, 0, NULL, 1, &imageMemoryBarrier1);

    VK_ASSERT(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
    };

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.waitSemaphoreCount = 1;
    submit.pWaitDstStageMask = &waitStage;
    submit.pWaitSemaphores = &DATA(skyboxRendering)[frameNumber];

    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &swapchain->renderSemaphores[frameNumber];

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_ASSERT(vkQueueSubmit(VulkanQueueManager.getQueue(GRAPHICS), 1, &submit, swapchain->renderFences[frameNumber]));

    VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = NULL,

      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &swapchain->renderSemaphores[frameNumber],

      .swapchainCount = 1,
      .pSwapchains = &swapchain->swapchain,

      .pImageIndices = &swapchainImageIndex,
    };

    vkQueuePresentKHR(VulkanQueueManager.getQueue(GRAPHICS), &presentInfo);
  }
  //end Fourth pass
  ////////////////

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
    .renderPass = RendererData.offscreenPass.renderPass,
  };

  vec(DescriptorSet) overrides = vec_init(DescriptorSet);
  vec_push(&overrides, &RendererData.sceneSet);
  vec_push(&overrides, &RendererData.cameraSet);
  vec_push(&overrides, &RendererData.resourcesSet);
  ev_pipeline_build(pipelineCreateInfo, overrides, &newPipeline);
  vec_fini(overrides);

  for (size_t i = 0; i < vec_len(newPipeline.pSets); i++)
    for (size_t j = 0; j < SWAPCHAIN_MAX_IMAGES; j++) {
      ev_descriptormanager_allocate(newPipeline.pSets[i].layout, &newPipeline.pSets[i].set[j]);
    }

  PipelineHandle new_handle = (PipelineHandle)vec_push(&RendererData.pipelineLibrary.store, &newPipeline);
  Hashmap(evstring, MaterialHandle).push(DATA(pipelineLibrary).map, evstring_new(pipelineName), new_handle);

  return new_handle;
}

void destroyPipeline(Pipeline *pipeline)
{
  ev_vulkan_destroypipeline(pipeline->pipeline);
  ev_vulkan_destroypipelinelayout(pipeline->pipelineLayout);
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
  ev_log_debug("%s", imagePath);
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
      case EV_IMAGEFORMAT_R8G8B8_SRGB:
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

void ev_renderer_registerCubeMap(CONST_STR imagePath)
{
  Texture newTexture;
  VkFormat format;
  uint32_t textureIndex;
  TextureHandle new_handle;

  AssetHandle image_handle1 = Asset->load("assets://textures/skybox/back.tx");
  ImageAsset imageAsset1 = ImageLoader->loadAsset(image_handle1);

  AssetHandle image_handle2 = Asset->load("assets://textures/skybox/front.tx");
  ImageAsset imageAsset2 = ImageLoader->loadAsset(image_handle2);

  AssetHandle image_handle3 = Asset->load("assets://textures/skybox/bottom.tx");
  ImageAsset imageAsset3 = ImageLoader->loadAsset(image_handle3);

  AssetHandle image_handle4 = Asset->load("assets://textures/skybox/top.tx");
  ImageAsset imageAsset4 = ImageLoader->loadAsset(image_handle4);

  AssetHandle image_handle5 = Asset->load("assets://textures/skybox/left.tx");
  ImageAsset imageAsset5 = ImageLoader->loadAsset(image_handle5);

  AssetHandle image_handle6 = Asset->load("assets://textures/skybox/right.tx");
  ImageAsset imageAsset6 = ImageLoader->loadAsset(image_handle6);

  // switch (imageAsset.format) {
  //   case EV_IMAGEFORMAT_R8G8B8_SRGB:
  //     format = VK_FORMAT_R8G8B8A8_SRGB;
  // }

  format = VK_FORMAT_R8G8B8A8_SRGB;

  newTexture.bufferSize = imageAsset1.bufferSize;
  newTexture.width = imageAsset1.width;
  newTexture.height = imageAsset1.height;

  void *pixels[] = {
    imageAsset6.data,
    imageAsset5.data,
    imageAsset4.data,
    imageAsset3.data,
    imageAsset2.data,
    imageAsset1.data,
  };

  RendererData.skyboxTexture = ev_vulkan_registerCubeMap(format, imageAsset1.width, imageAsset1.height, 6, pixels);
}

RenderComponent ev_renderer_registerRenderComponent(const char *meshPath, const char *materialName)
{
  RenderComponent newComponent = { 0 };

  newComponent.materialIndex = ev_renderer_getMaterial(materialName);
  newComponent.pipelineIndex = RendererData.materialLibrary.pipelineHandles[newComponent.materialIndex];

  uint32_t meshID = ev_renderer_registerMesh(meshPath);
  newComponent.mesh = *(RendererData.meshLibrary.store + meshID);

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

    evstring emissive_jsonid = evstring_newfmt("%s[%d].emissiveTexture", list_name, i);
    evjson_entry *emissiveEntry = evjs_get(json_context, emissive_jsonid);
    if (emissiveEntry) {
      evstring emissive = evstring_refclone(emissiveEntry->as_str);
      newMaterial.emissiveTexture = ev_renderer_registerTexture(emissive);
      evstring_free(emissive);
    }
    else {
      newMaterial.emissiveTexture = 0;
    }
    evstring_free(emissive_jsonid);

    evstring emissiveFactor_jsonID = evstring_newfmt("%s[%d].emissiveFactor", list_name, i);
    evjson_entry *emissiveFactorEntry = evjs_get(json_context, emissiveFactor_jsonID);
    if(emissiveFactorEntry) {
      evstring_pushstr(&emissiveFactor_jsonID, "[x]");
      size_t emissiveFactor_jsonID_len = evstring_len(emissiveFactor_jsonID);
      for(size_t i = 0; i < 3; i++) {
        emissiveFactor_jsonID[emissiveFactor_jsonID_len-2] = '0' + i;
        ((float*)&newMaterial.emissiveFactor)[i] = (float)evjs_get(json_context, emissiveFactor_jsonID)->as_num;
      }
    }
    evstring_free(emissiveFactor_jsonID);

// TODO fix this
    evstring materialPipeline_jsonid = evstring_newfmt("%s[%d].pipeline", list_name, i);
    PipelineHandle materialPipelineHandle;
    if (evjs_get(json_context, materialPipeline_jsonid))
    {
      evstring materialPipeline = evstring_refclone(evjs_get(json_context, materialPipeline_jsonid)->as_str);
      materialPipelineHandle = ev_renderer_getPipeline(materialPipeline);
      evstring_free(materialPipeline);
    }
    // else {
    //   materialPipelineHandle = ev_renderer_getPipeline(DEFAULTPIPELINE);
    // }
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
  ev_renderer_registershadowmapPipeline();
  ev_renderer_registerskyboxPipeline();
  ev_renderer_registerLightPipeline();
  ev_renderer_registerfxaaPipeline();

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
  ev_syncmanager_init();

  ev_renderer_globalsetsinit();

  materialLibraryInit(&DATA(materialLibrary));
  pipelineLibraryInit(&DATA(pipelineLibrary));
  textureLibraryInit(&(DATA(textureLibrary)));
  meshLibraryInit(&DATA(meshLibrary));

  ev_renderer_registerTexture(DEFAULTEXTURE);

  ev_syncmanager_allocatesemaphores(SWAPCHAIN_MAX_IMAGES, &DATA(offscreenRendering));
  ev_syncmanager_allocatesemaphores(SWAPCHAIN_MAX_IMAGES, &DATA(shadowmapRendering));
  ev_syncmanager_allocatesemaphores(SWAPCHAIN_MAX_IMAGES, &DATA(lightRendering));
  ev_syncmanager_allocatesemaphores(SWAPCHAIN_MAX_IMAGES, &DATA(skyboxRendering));

  for (size_t i = 0; i < SWAPCHAIN_MAX_IMAGES; i++) {
    ev_vulkan_allocateprimarycommandbuffer(GRAPHICS, &DATA(offscreencommandbuffer)[i]);
    ev_vulkan_allocateprimarycommandbuffer(GRAPHICS, &DATA(shadowmapcommandbuffer)[i]);
    ev_vulkan_allocateprimarycommandbuffer(GRAPHICS, &DATA(lightcommandbuffer)[i]);
    ev_vulkan_allocateprimarycommandbuffer(GRAPHICS, &DATA(skyboxcommandbuffer)[i]);
  }
}

void ev_renderer_clear()
{
  Hashmap(evstring, MaterialHandle).clear(RendererData.materialLibrary.map);
  vec_clear(RendererData.materialLibrary.store);
  vec_clear(RendererData.materialLibrary.pipelineHandles);

  Hashmap(evstring, MaterialHandle).clear(RendererData.pipelineLibrary.map);
  vec_clear(RendererData.materialLibrary.store);
}

EV_DESTRUCTOR
{
  ev_vulkan_wait();

  ev_renderpass_destory(RendererData.skyboxPass);
  ev_renderpass_destory(RendererData.lightPass);
  ev_renderpass_destory(RendererData.shadowmapPass);
  ev_renderpass_destory(RendererData.offscreenPass);

  destroyPipeline(&DATA(fxaaPipeline));
  for (size_t i = 0; i < vec_len(DATA(fxaaPipeline).pSets); i++) {
    ev_vulkan_destroysetlayout(DATA(fxaaPipeline).pSets[i].layout);
  }

  destroyPipeline(&DATA(skyboxPipeline));
  for (size_t i = 0; i < vec_len(DATA(skyboxPipeline).pSets); i++) {
    ev_vulkan_destroysetlayout(DATA(skyboxPipeline).pSets[i].layout);
  }

  destroyPipeline(&DATA(lightPipeline));
  for (size_t i = 0; i < vec_len(DATA(lightPipeline).pSets); i++) {
    ev_vulkan_destroysetlayout(DATA(lightPipeline).pSets[i].layout);
  }

  destroyPipeline(&DATA(shadowmapPipeline));
  for (size_t i = 0; i < vec_len(DATA(shadowmapPipeline).pSets); i++) {
    ev_vulkan_destroysetlayout(DATA(shadowmapPipeline).pSets[i].layout);
  }

  vec_fini(DATA(textureBuffers));
  vec_fini(DATA(customBuffers));
  vec_fini(DATA(vertexBuffers));
  vec_fini(DATA(indexBuffers));

  meshLibraryDestroy(DATA(meshLibrary));
  textureLibraryDestroy(DATA(textureLibrary));
  materialLibraryDestroy(DATA(materialLibrary));
  pipelineLibraryDestroy(DATA(pipelineLibrary));

  ev_vulkan_destroytexture(&RendererData.skyboxTexture);

  ev_renderer_globalsetsdinit();

  ev_syncmanager_deinit();
  ev_vulkan_deinit();

  FrameData_fini(&DATA(currentFrame));

  evol_unloadmodule(window_module);
  evol_unloadmodule(asset_module);
}

EV_BINDINGS
{
  EV_NS_BIND_FN(Renderer, setWindow, setWindow);
  EV_NS_BIND_FN(Renderer, run, run);
  EV_NS_BIND_FN(Renderer, clear, ev_renderer_clear);
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
  vec_fini(library.store);
  Hashmap(evstring, PipelineHandle).free(library.map);
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
