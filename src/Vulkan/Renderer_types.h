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
  Matrix4x4 transform;
  // EvMesh mesh;
} EvRenderObject;














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
  vec(EvBuffer) pMeshBuffers;
  vec(EvBuffer) pCustomBuffers;

  VkPipeline pipeline;
  VkPipelineLayout pipelineLayout;

  vec(DescriptorSet) pSets;

  vec(EvRenderObject) pObjects;
} RendererMaterial;
