#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <evol/evol.h>

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

typedef struct
{
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} EvBuffer;

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
  Matrix4x4 tranform;
  uint32_t meshIndex
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
  uint32_t indexBufferIndex;
  uint32_t vertexBufferIndex;
  uint32_t materialIndex;
} ShaderMesh;

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
  uint32_t materialIndex;
  uint32_t pipelineIndex;
  uint32_t meshIndex;

  Mesh mesh;
} EvRenderObject;
