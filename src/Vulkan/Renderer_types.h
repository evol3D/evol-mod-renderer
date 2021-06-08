#pragma once

//TODO use this once
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

// typedef struct {
//     Vec3 position;
//     Vec3 normal;
//     Vec3 color;
// } EvVertex;
//
// typedef struct {
// 	vec(EvVertex) vertices;
//   uint32_t vertexIndex;
// } EvMesh;

typedef struct {
  VkDescriptorType type;
  void *descriptorData;
} oldDescriptor;

typedef struct {
  //passed to engine
  evstring shaderCodes[2];
  VkShaderStageFlagBits stageFlags[2];

  //built by engine
  vec(VkDescriptorSetLayout) setLayouts;
  vec(VkDescriptorSet) sets;
  vec(void *) setsdescriptors;

  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
} EvMaterial;

typedef struct {
  Matrix4x4 transform;
  // EvMesh mesh;
} EvRenderObject;
















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
  vec(evstring) pShaderCodes;
  vec(VkShaderStageFlagBits) pStageFlags;

  vec(EvBuffer) pMeshBuffers;
  vec(EvBuffer) pCustomBuffers;

  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  vec(DescriptorSet) pSets;

  vec(EvRenderObject) pObjects;
} RendererMaterial;
