#include <EvBuffer.h>

EvBuffer 
evbuffer_new(
    VkBufferCreateInfo *bufferCreateInfo, 
    VmaAllocator allocator,
    VmaAllocationCreateInfo *allocationCreateInfo)
{
  EvBuffer buffer;
  VK_ASSERT(
          vmaCreateBuffer(
              allocator, 
              bufferCreateInfo, 
              allocationCreateInfo, 
              &(buffer.vma_buffer.vk_buffer), 
              &(buffer.vma_buffer.allocData.allocation), 
              &(buffer.vma_buffer.allocData.allocationInfo)));

  buffer.vma_buffer.allocData.allocator = allocator;

  return buffer;
}

void
VmaBufferDestroy(
    VmaBuffer vma_buffer)
{
  vmaDestroyBuffer(vma_buffer.allocData.allocator, vma_buffer.vk_buffer, vma_buffer.allocData.allocation);
}

void 
evbuffer_destroy(
    EvBuffer buffer)
{
    VmaBufferDestroy(buffer.vma_buffer);
}

void
evbuffer_pdestroy(
    EvBuffer *buffer)
{
  evbuffer_destroy(*buffer);
}
