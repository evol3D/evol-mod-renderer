#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

typedef struct {
  VmaAllocator allocator;
  VmaAllocation allocation;
  VmaAllocationInfo allocationInfo;
} VmaData;

typedef struct {
  VkImage vk_image;
  VmaData allocData;
} VmaImage;

typedef struct {
  VmaImage vma_image;
} EvImage;

EvImage 
evimage_new(
    VkImageCreateInfo *imageCreateInfo, 
    VmaAllocator allocator,
    VmaAllocationCreateInfo *allocationCreateInfo);

void
evimage_destroy(
    EvImage image);
