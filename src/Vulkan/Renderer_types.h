#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <evol/evol.h>

typedef struct
{
  VkImage image;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvImage;

typedef struct
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvBuffer;

typedef struct {
  void* data;
  size_t length;
  VkShaderStageFlags stage;
} Shader;

typedef struct {
  uint32_t binding;
  VkDescriptorType type;
  const uint32_t *bindingName;

  vec(VkWriteDescriptorSet)   pSetWrites;
  vec(VkDescriptorBufferInfo) pBufferInfos;
  vec(VkDescriptorImageInfo)  pImageInfos;
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
} RendererMaterial;

typedef struct {
  uint32_t indexCount;
  uint32_t indexBufferIndex;

	uint32_t vertexCount;
  uint32_t vertexBufferIndex;
} Mesh;

typedef struct {
  float baseColor[3];
} Material;

typedef struct {
  Mesh mesh;
  Material material;

  Matrix4x4 transform;

  uint32_t rendererMaterialIndex;
} EvRenderObject;
