#pragma once

#include <Vulkan.h>
#include <evstr.h>
#include <evol/evol.h>
#include <evol/common/ev_globals.h>

typedef struct
{
  U32                                              stageCount;
  Shader*                                          pShaders;

  VkRenderPass                                     renderPass;

  VkStructureType                                  sType;
  const void*                                      pNext;
  VkPipelineCreateFlags                            flags;
  const VkPipelineVertexInputStateCreateInfo*      pVertexInputState;
  const VkPipelineInputAssemblyStateCreateInfo*    pInputAssemblyState;
  const VkPipelineTessellationStateCreateInfo*     pTessellationState;
  const VkPipelineViewportStateCreateInfo*         pViewportState;
  const VkPipelineRasterizationStateCreateInfo*    pRasterizationState;
  const VkPipelineMultisampleStateCreateInfo*      pMultisampleState;
  const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState;
  const VkPipelineColorBlendStateCreateInfo*       pColorBlendState;
  const VkPipelineDynamicStateCreateInfo*          pDynamicState;
  VkPipelineLayout                                 layout;
  uint32_t                                         subpass;
  VkPipeline                                       basePipelineHandle;
  int32_t                                          basePipelineIndex;
} EvGraphicsPipelineCreateInfo;

void ev_pipeline_build(EvGraphicsPipelineCreateInfo evCreateInfo, vec(DescriptorSet) overideSets, Pipeline *material);
