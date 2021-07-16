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

void ev_pipeline_build(EvGraphicsPipelineCreateInfo evCreateInfo, vec(DescriptorSet) overideSets, Pipeline *pipeline)
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

  ev_pipeline_reflectlayout(evCreateInfo, overideSets, pipeline);

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
    .stencilTestEnable = VK_FALSE,
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 1.0f,
  };
  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentStates[] = {
    {
			.colorWriteMask = 0xf,
			.blendEnable = VK_FALSE,
    },
    {
			.colorWriteMask = 0xf,
			.blendEnable = VK_FALSE,
    },
    {
      .colorWriteMask = 0xf,
      .blendEnable = VK_FALSE,
    },
    {
      .colorWriteMask = 0xf,
      .blendEnable = VK_FALSE,
    },
  };
  VkPipelineColorBlendStateCreateInfo pipelineColorBlendState = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .attachmentCount = ARRAYSIZE(pipelineColorBlendAttachmentStates),
    .pAttachments = &pipelineColorBlendAttachmentStates,
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
    .layout = pipeline->pipelineLayout,
    .pVertexInputState = evCreateInfo.pVertexInputState == 0 ? &pipelineVertexInputState : evCreateInfo.pVertexInputState,
    .pInputAssemblyState = evCreateInfo.pInputAssemblyState == 0 ? &pipelineInputAssemblyState : evCreateInfo.pInputAssemblyState,
    .pViewportState = evCreateInfo.pViewportState == 0 ? &pipelineViewportState : evCreateInfo.pViewportState,
    .pRasterizationState = evCreateInfo.pRasterizationState == 0 ? &pipelineRasterizationState : evCreateInfo.pRasterizationState,
    .pMultisampleState = evCreateInfo.pMultisampleState == 0 ? &pipelineMultisampleState : evCreateInfo.pMultisampleState,
    .pDepthStencilState = evCreateInfo.pDepthStencilState == 0 ? &pipelineDepthStencilState : evCreateInfo.pDepthStencilState,
    .pColorBlendState = evCreateInfo.pColorBlendState == 0 ? &pipelineColorBlendState : evCreateInfo.pColorBlendState,
    .pDynamicState = evCreateInfo.pDynamicState == 0 ? &pipelineDynamicState : evCreateInfo.pDynamicState,
  };

  VK_ASSERT(
    vkCreateGraphicsPipelines(
      ev_vulkan_getlogicaldevice(), NULL,
      1,
      &graphicsPipelinesCreateInfo, NULL,
      &pipeline->pipeline)
    );

  for (size_t i = 0; i < ARRAYSIZE(shaderModules); i++)
    ev_vulkan_destroyshadermodule(shaderModules[i]);
}

void ev_pipeline_reflectStages(EvGraphicsPipelineCreateInfo pipelineCreateInfo, VkPushConstantRange* pc, DescriptorSetLayoutData* set_datalayouts)
{
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

      vec_push(set_datalayouts, &layout);
    }

    //sprv pushconstant reflection
    result = spvReflectEnumeratePushConstants(&spvmodule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    SpvReflectBlockVariable* pconstant;

    result = spvReflectEnumeratePushConstants(&spvmodule, &count, &pconstant);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (count > 0)
    {
        pc->offset = pconstant->offset;
        pc->size = pconstant->size;
        pc->stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    }

    vec_fini(sets);
  }
}

void ev_pipeline_reflectlayout(EvGraphicsPipelineCreateInfo pipelineCreateInfo, vec(DescriptorSet) overideSets, Pipeline *pipeline)
{
  VkPushConstantRange pc = { 0 };
  vec(DescriptorSetLayoutData) setLayouts_data = vec_init(DescriptorSetLayoutData, descriptorSetLayoutData_destr);
  ev_pipeline_reflectStages(pipelineCreateInfo, &pc, &setLayouts_data);

  vec_setcapacity(&pipeline->pSets, 4);

  uint32_t overrideCount = vec_len(overideSets);
  for (size_t i = 0; i < overrideCount; i++)
    vec_push(&pipeline->pSets, &overideSets[i]);

  DescriptorSetLayoutData final_setLayouts_data[4] =
  {
    [0] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [1] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [2] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
    [3] = { .bindings = vec_init(VkDescriptorSetLayoutBinding) },
  };

  for (int i = overrideCount; i < 4; i++)
  {
    DescriptorSet set;
    set.layout = VK_NULL_HANDLE;
    set.pBindings = vec_init(Binding);

    DescriptorSetLayoutData final_setLayout = final_setLayouts_data[i];

    for (size_t setLayoutIndex = 0; setLayoutIndex < vec_len(setLayouts_data); setLayoutIndex++)
    {
      DescriptorSetLayoutData setLayout = setLayouts_data[setLayoutIndex];

      if(setLayout.set_number == i)
      {
        for(size_t bindingIdx = 0; bindingIdx < vec_len(setLayout.bindings); bindingIdx++)
        {
          vec_push(&final_setLayout.bindings, &(VkDescriptorSetLayoutBinding) {
            .binding          = setLayout.bindings[bindingIdx].binding,
            .descriptorType   = setLayout.bindings[bindingIdx].descriptorType,
            .descriptorCount  = setLayout.bindings[bindingIdx].descriptorCount,
            .stageFlags       = setLayout.bindings[bindingIdx].stageFlags,
            .pImmutableSamplers = 0
          });

          vec_push(&set.pBindings, &(VkDescriptorSetLayoutBinding) {
            .binding          = setLayout.bindings[bindingIdx].binding,
            .descriptorType   = setLayout.bindings[bindingIdx].descriptorType,
            .descriptorCount  = setLayout.bindings[bindingIdx].descriptorCount,
            .stageFlags       = setLayout.bindings[bindingIdx].stageFlags,
            .pImmutableSamplers = 0
          });
        }
      }
    }

    final_setLayout.set_number = i;
    final_setLayout.create_info = (VkDescriptorSetLayoutCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = vec_len(final_setLayout.bindings),
      .pBindings = final_setLayout.bindings,
      .flags = 0,
      .pNext = 0,
    };

    if (final_setLayout.create_info.bindingCount > 0)
    {
      //here you should retrieve all things
      VK_ASSERT(vkCreateDescriptorSetLayout(ev_vulkan_getlogicaldevice(), &final_setLayout.create_info, NULL, &set.layout));

      vec_push(&pipeline->pSets, &set);
    }
  }

  VkDescriptorSetLayout setLayouts[4] = {
    pipeline->pSets[0].layout,
    pipeline->pSets[1].layout,
    pipeline->pSets[2].layout,
    pipeline->pSets[3].layout,
  };

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .setLayoutCount = vec_len(pipeline->pSets),
    .pSetLayouts = setLayouts,
    .pushConstantRangeCount = pc.size > 0 ? 1:0,
    .pPushConstantRanges = &pc,
  };

  VK_ASSERT(vkCreatePipelineLayout(ev_vulkan_getlogicaldevice(), &pipelineLayoutCreateInfo, NULL, &pipeline->pipelineLayout));

  vec_fini(setLayouts_data);
}
