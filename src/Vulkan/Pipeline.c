#include "Pipeline.h"

#include <Renderer_types.h>
#include <Vulkan_utils.h>
#include <evol/common/ev_macros.h>
#include <vec.h>
#include <Vulkan_utils.h>
#include <spvref/spirv_reflect.h>
#include <evol/common/ev_log.h>

typedef struct
{
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info;
  vec(VkDescriptorSetLayoutBinding) bindings;
  vec(uint32_t*) bindingNames;
} DescriptorSetLayoutData;

void ev_pipeline_createshadermodule(Shader code, VkShaderModule *shaderModule)
{
  VkShaderModuleCreateInfo shaderModuleCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code.length,
    .pCode = code.data,
  };

  VK_ASSERT(vkCreateShaderModule(ev_vulkan_getlogicaldevice(), &shaderModuleCreateInfo, NULL, shaderModule));
}

void descriptorSetLayoutData_destr(DescriptorSetLayoutData *data)
{
  vec_fini(data->bindings);
}

void ev_pipeline_reflectlayout(EvGraphicsPipelineCreateInfo pipelineCreateInfo, RendererMaterial *material)
{
  vec(VkPushConstantRange) constant_ranges = vec_init(VkPushConstantRange);
  vec(DescriptorSetLayoutData) set_datalayouts = vec_init(DescriptorSetLayoutData, descriptorSetLayoutData_destr);

  for (size_t stageIndex = 0; stageIndex < pipelineCreateInfo.stageCount; stageIndex++)
  {
    SpvReflectShaderModule spvmodule;
    SpvReflectResult result = spvReflectCreateShaderModule(pipelineCreateInfo.pShaders[stageIndex].length, pipelineCreateInfo.pShaders[stageIndex].data, &spvmodule);

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    vec(SpvReflectDescriptorSet*) sets = vec_init(SpvReflectDescriptorSet*);
    vec_setlen(&sets, count);
    result = spvReflectEnumerateDescriptorSets(&spvmodule, &count, sets);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    for (size_t setIndex = 0; setIndex < vec_len(sets); setIndex++)
    {
      SpvReflectDescriptorSet* set = sets[setIndex];

      DescriptorSetLayoutData layout = {
        .bindings = vec_init(VkDescriptorSetLayoutBinding),
        .bindingNames = vec_init(uint32_t*),
      };
      vec_setlen(&layout.bindings, set->binding_count);
      vec_setlen(&layout.bindingNames, set->binding_count);

      for (uint32_t i_binding = 0; i_binding < set->binding_count; ++i_binding)
      {
        const SpvReflectDescriptorBinding refl_binding = *(set->bindings[i_binding]);
        VkDescriptorSetLayoutBinding *layout_binding = &layout.bindings[i_binding];
        layout_binding->binding = refl_binding.binding;
        layout_binding->descriptorType = (VkDescriptorType)refl_binding.descriptor_type;
        layout_binding->descriptorCount = 1;
        layout.bindingNames[i_binding] = refl_binding.name;

        for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim)
        {
          layout_binding->descriptorCount *= refl_binding.array.dims[i_dim];
        }
        layout_binding->stageFlags = (VkShaderStageFlagBits)spvmodule.shader_stage;
      }

      layout.set_number = set->set;
      layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      layout.create_info.bindingCount = set->binding_count;
      layout.create_info.pBindings = layout.bindings;

      vec_push(&set_datalayouts, &layout);
    }

    result = spvReflectEnumeratePushConstants(&spvmodule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    vec(SpvReflectBlockVariable*) pconstants = vec_init(SpvReflectBlockVariable*);

    result = spvReflectEnumeratePushConstants(&spvmodule, &count, pconstants);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (count > 0) {
      VkPushConstantRange pcs = {0};
      pcs.offset = pconstants[0]->offset;
      pcs.size = pconstants[0]->size;
      pcs.stageFlags = pipelineCreateInfo.pShaders[stageIndex].stage;

      vec_push(&constant_ranges, &pcs);
    }

    vec_fini(pconstants);

    vec_fini(sets);
  }

  DescriptorSetLayoutData merged_datalayouts[4] = {
    [0] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [1] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [2] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [3] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
  };

  material->pSets = vec_init(DescriptorSet);
  vec_setcapacity(&material->pSets, 4);

  for (int i = 0; i < 4; i++)
  {
    DescriptorSet realSet;
    realSet.layout = VK_NULL_HANDLE;
    realSet.pBindings = vec_init(Binding);

    DescriptorSetLayoutData *ly = &(merged_datalayouts[i]);
    ly->set_number = i;
    ly->create_info = (VkDescriptorSetLayoutCreateInfo){
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    };

    for (size_t setLayoutIndex = 0; setLayoutIndex < vec_len(set_datalayouts); setLayoutIndex++)
    {
      DescriptorSetLayoutData *setLayoutData = &(set_datalayouts[setLayoutIndex]);
      if(setLayoutData->set_number == i)
      {
        vec_setlen(&realSet.pBindings, vec_len(setLayoutData->bindings));

        for(size_t bindingIdx = 0; bindingIdx < vec_len(setLayoutData->bindings); bindingIdx++)
        {
          vec_push(&(ly->bindings), &(setLayoutData->bindings[bindingIdx]));

          realSet.pBindings[bindingIdx].pSetWrites   = vec_init(VkWriteDescriptorSet);
          realSet.pBindings[bindingIdx].pBufferInfos = vec_init(VkDescriptorBufferInfo);
          realSet.pBindings[bindingIdx].pImageInfos  = vec_init(VkDescriptorImageInfo);

          realSet.pBindings[bindingIdx].binding = setLayoutData->bindings[bindingIdx].binding;
          realSet.pBindings[bindingIdx].type = setLayoutData->bindings[bindingIdx].descriptorType;
          realSet.pBindings[bindingIdx].bindingName = setLayoutData->bindingNames[bindingIdx];
        }
      }
    }

    ly->create_info.bindingCount = vec_len(ly->bindings);
    ly->create_info.pBindings = ly->bindings;
    ly->create_info.flags = 0;
    ly->create_info.pNext = 0;

    if (ly->create_info.bindingCount > 0)
    {
      ev_log_debug("binding count: %d", vec_len(ly->bindings));
      vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &(ly->create_info), NULL, &realSet.layout);
      vec_push(&material->pSets, &realSet);
    }
    else {
      //sl = VK_NULL_HANDLE;
    }
  }

  VkDescriptorSetLayout setLayouts[] = {
    material->pSets[0].layout,
    material->pSets[1].layout,
    material->pSets[2].layout,
    material->pSets[3].layout,
  };

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = vec_len(material->pSets),
    .pSetLayouts = setLayouts,
    .pushConstantRangeCount = NULL,
    .pPushConstantRanges = NULL,
  };

  vkCreatePipelineLayout(ev_vulkan_getlogicaldevice(), &pipelineLayoutCreateInfo, NULL, &material->pipelineLayout);

  vec_fini(merged_datalayouts[0].bindings);
  vec_fini(merged_datalayouts[1].bindings);
  vec_fini(merged_datalayouts[2].bindings);
  vec_fini(merged_datalayouts[3].bindings);

  vec_fini(set_datalayouts);
  vec_fini(constant_ranges);
}

void ev_pipeline_build(EvGraphicsPipelineCreateInfo evCreateInfo, RendererMaterial *material)
{
  VkShaderModule shaderModules[evCreateInfo.stageCount];
  VkPipelineShaderStageCreateInfo shaderStageCreateInfos[evCreateInfo.stageCount];

  {
    if (evCreateInfo.stageCount == 0) return;
    if (evCreateInfo.renderPass == NULL) return;
  }

  for (size_t stgIndex = 0; stgIndex < evCreateInfo.stageCount; stgIndex++)
  {
    ev_pipeline_createshadermodule(evCreateInfo.pShaders[stgIndex], &shaderModules[stgIndex]);

    shaderStageCreateInfos[stgIndex] = (VkPipelineShaderStageCreateInfo){
      .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage  = evCreateInfo.pShaders[stgIndex].stage,
      .module = shaderModules[stgIndex],
      .pName  = "main"
    };
  }

  ev_pipeline_reflectlayout(evCreateInfo, material);

  VkPipelineVertexInputStateCreateInfo pipelineVertexInputState ={
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = NULL,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = NULL
  };
  VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  };
  VkPipelineViewportStateCreateInfo pipelineViewportState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1,
  };
  VkPipelineRasterizationStateCreateInfo pipelineRasterizationState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .lineWidth = 1.0,
  };
  VkPipelineMultisampleStateCreateInfo pipelineMultisampleState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };
  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS,
    .depthBoundsTestEnable = VK_FALSE,
    .back = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .front = {
      .failOp = VK_STENCIL_OP_KEEP,
      .passOp = VK_STENCIL_OP_KEEP,
      .compareOp = VK_COMPARE_OP_ALWAYS
    },
    .stencilTestEnable = VK_FALSE
  };
  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState = {
    .colorWriteMask =
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_A_BIT ,
  };
  VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &pipelineColorBlendAttachmentState,
  };
  VkDynamicState dynamicStates[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
  };
  VkPipelineDynamicStateCreateInfo pipelineDynamicState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount = ARRAYSIZE(dynamicStates),
    .pDynamicStates = dynamicStates,
  };

  //TODO use passed settings
  VkGraphicsPipelineCreateInfo graphicsPipelinesCreateInfo =
  {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,

    .stageCount = evCreateInfo.stageCount,
    .pStages = shaderStageCreateInfos,

    .renderPass = evCreateInfo.renderPass,

    .subpass = 0,
    .layout = material->pipelineLayout,
    .pVertexInputState = &pipelineVertexInputState,
    .pInputAssemblyState = &pipelineInputAssemblyState,
    .pViewportState = &pipelineViewportState,
    .pRasterizationState = &pipelineRasterizationState,
    .pMultisampleState = &pipelineMultisampleState,
    .pDepthStencilState = &pipelineDepthStencilState,
    .pColorBlendState = &pipelineColorBlendState,
    .pDynamicState = &pipelineDynamicState,
  };

  VK_ASSERT(
    vkCreateGraphicsPipelines(
      ev_vulkan_getlogicaldevice(), NULL,
      1,
      &graphicsPipelinesCreateInfo, NULL,
      &material->pipeline)
    );
}
