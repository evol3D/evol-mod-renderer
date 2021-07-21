#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <evol/evol.h>
#include <evol/threads/evolpthreads.h>

#define SWAPCHAIN_MAX_IMAGES 5

typedef enum {
    VERTEXRESOURCE,
    INDEXRESOURCE,
    MATERIALRESOURCE,
    CUSTOMBUFFER,
} RESOURCETYPE;

typedef struct
{
  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvImage;

typedef struct EvTexturer
 {
     EvImage image;
     VkImageView imageView;
     VkSampler sampler;
 } EvTexture;

typedef struct
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvBuffer;

typedef struct
{
  uint lightsCount;
} EvScene;

typedef struct {
  VkExtent2D windowExtent;

  uint32_t imageCount;
  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surfaceFormat;

  EvImage depthImage[SWAPCHAIN_MAX_IMAGES];
  VkImageView depthImageView[SWAPCHAIN_MAX_IMAGES];
  VkFormat depthStencilFormat;
  VkDeviceMemory depthImageMemory;

  VkImage images[SWAPCHAIN_MAX_IMAGES];
  VkImageView imageViews[SWAPCHAIN_MAX_IMAGES];

  VkCommandBuffer commandBuffers[SWAPCHAIN_MAX_IMAGES];

  VkSemaphore presentSemaphores[SWAPCHAIN_MAX_IMAGES];
  VkSemaphore renderSemaphores[SWAPCHAIN_MAX_IMAGES];
  VkFence renderFences[SWAPCHAIN_MAX_IMAGES];

  VkRenderPass renderPass;
  VkFramebuffer framebuffers[SWAPCHAIN_MAX_IMAGES];
} EvSwapchain;

typedef struct
{
  EvBuffer buffer;
  void *mappedData;
} UBO;

typedef struct {
  Matrix4x4 projectionMat;
  Matrix4x4 viewMat;
} CameraData;

typedef struct {
  evstring shaderBindingName;
  RESOURCETYPE resourceType;
  uint32_t bufferize;
  void *data;
  uint32_t index;
} ShaderData;

typedef struct {
  Matrix4x4 transform;
  uint32_t indexBufferIndex;
  uint32_t vertexBufferIndex;
  uint32_t materialIndex;
} MeshPushConstants;

typedef struct {
  Matrix4x4 transform;
  uint32_t indexBufferIndex;
  uint32_t vertexBufferIndex;
} ShadowmapPushConstants;

typedef struct {
  uint32_t lightCount;
} LightPushConstants;

typedef struct {
  void* data;
  size_t length;
  VkShaderStageFlags stage;
} Shader;

typedef struct {
  uint32_t binding;
  VkDescriptorType type;
  const uint32_t *bindingName;

  uint32_t writtenSetsCount;
} Binding;

typedef struct {
  VkDescriptorSet set[SWAPCHAIN_MAX_IMAGES];
  VkDescriptorSetLayout layout;
  vec(Binding) pBindings;
} DescriptorSet;

typedef struct {
  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  vec(DescriptorSet) pSets;
} Pipeline;

typedef struct {
  Vec4 baseColor;
  uint32_t albedoTexture;

  uint32_t normalTexture;

  float metallicFactor;
  float roughnessFactor;
  uint32_t metallicRoughnessTexture;

  Vec4 emissiveFactor;
  uint32_t emissiveTexture;
} Material;
