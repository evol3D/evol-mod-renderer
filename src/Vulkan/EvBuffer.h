#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vk_utils.h>

typedef struct {
  VkBuffer vk_buffer;
  VmaData allocData;
} VmaBuffer;

typedef struct {
  VmaBuffer vma_buffer;
} EvBuffer;

EvBuffer 
evbuffer_new(
    VkBufferCreateInfo *bufferCreateInfo, 
    VmaAllocator allocator,
    VmaAllocationCreateInfo *allocationCreateInfo);

void 
evbuffer_destroy(
    EvBuffer buffer);

void
evbuffer_pdestroy(
    EvBuffer *buffer);
