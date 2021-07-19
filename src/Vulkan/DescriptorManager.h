#pragma once

#include <Vulkan.h>

void ev_descriptormanager_init();

void ev_descriptormanager_dinit();

VkResult ev_descriptormanager_allocate(VkDescriptorSetLayout layout, VkDescriptorSet* set);
