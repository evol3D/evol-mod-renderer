#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <evol/evol.h>
#include <evol/threads/evolpthreads.h>
#include <EvImage.h>

typedef enum {
    VERTEXRESOURCE,
    INDEXRESOURCE,
    MATERIALRESOURCE,
    CUSTOMBUFFER,
} RESOURCETYPE;

typedef struct EvTexturer
 {
     EvImage image;
     VkImageView imageView;
     VkSampler sampler;
 } EvTexture;

#define SWAPCHAIN_MAX_IMAGES 5

typedef struct
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvBuffer;

typedef struct
{
  Vec3 color;
  uint32_t intensity;
} Lights;

typedef struct
{
  uint lightsCount;
} EvScene;

typedef struct {
  VkExtent2D windowExtent;

  uint32_t imageCount;
  VkSwapchainKHR swapchain;
  VkSurfaceFormatKHR surfaceFormat;

  EvImage depthImage;
  VkImageView depthImageView;
  VkFormat depthStencilFormat;
  VkDeviceMemory depthImageMemory;

  VkImage images[SWAPCHAIN_MAX_IMAGES];
  VkImageView imageViews[SWAPCHAIN_MAX_IMAGES];
  VkCommandBuffer commandBuffers[SWAPCHAIN_MAX_IMAGES];

  VkSemaphore presentSemaphore;
  VkSemaphore submittionSemaphore;
  VkFence frameSubmissionFences[SWAPCHAIN_MAX_IMAGES];
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
  VkDescriptorSet set;
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
} Material;
