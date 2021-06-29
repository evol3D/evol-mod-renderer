#include <EvImage.h>

EvImage 
evimage_new(
    VkImageCreateInfo *imageCreateInfo, 
    VmaAllocator allocator,
    VmaAllocationCreateInfo *allocationCreateInfo)
{
  EvImage image;
  VK_ASSERT(vmaCreateImage(
      allocator, 
      imageCreateInfo, 
      allocationCreateInfo, 
      &(image.vma_image.vk_image), 
      &(image.vma_image.allocData.allocation), 
      &(image.vma_image.allocData.allocationInfo)));

  image.vma_image.allocData.allocator = allocator;

  return image;
}

void
VmaImageDestroy(
    VmaImage vma_image)
{
  vmaDestroyImage(vma_image.allocData.allocator, vma_image.vk_image, vma_image.allocData.allocation);
}

void
evimage_destroy(
    EvImage image)
{
  VmaImageDestroy(image.vma_image);
}
