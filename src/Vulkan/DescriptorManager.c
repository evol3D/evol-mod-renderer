#include <DescriptorManager.h>

#include <vec.h>
#include <Vulkan_utils.h>

struct {
  vec(VkDescriptorPool) pools;
} DescriptorManagerData;

#define DATA(X) DescriptorManagerData.X

void createPool(int count, VkDescriptorPoolCreateFlags flags);

void ev_descriptormanager_init()
{
  DATA(pools) = vec_init(VkDescriptorPool, NULL, ev_vulkan_destroydescriptorpool);
  createPool(2000, 0);
}

void ev_descriptormanager_dinit()
{
  vec_fini(DATA(pools));
}

void createPool(int count, VkDescriptorPoolCreateFlags flags)
{
  VkDescriptorPoolSize sizes[] =
  {
    { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5 * count },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f * count},
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f * count},
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f * count},
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f * count},
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f * count},
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f * count},
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f * count},
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f * count},
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f * count},
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f * count}
  };

  VkDescriptorPoolCreateInfo info =
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = VK_NULL_HANDLE,
    .flags = flags,
    .maxSets = count,
    .poolSizeCount = ARRAYSIZE(sizes),
    .pPoolSizes = sizes,
  };

  VkDescriptorPool pool;
	ev_vulkan_createdescriptorpool(&info, &pool);

  vec_push(&DATA(pools), &pool);
}

void ev_descriptormanager_allocate(VkDescriptorSetLayout layout, VkDescriptorSet* set)
{
  //TODO TRACK descriptors count allocated

  VkDescriptorSetAllocateInfo info;
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.pNext = NULL;
  info.descriptorPool = *((VkDescriptorPool*)vec_last(DATA(pools)));
  info.descriptorSetCount = 1;
  info.pSetLayouts = &layout;

  ev_vulkan_allocatedescriptor(&info, set);
}
