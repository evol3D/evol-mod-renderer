//for now we wont use input attachments and multi-subpasses

#include <RenderPass/RenderPass.h>

#include <Vulkan.h>
#include <Vulkan_utils.h>
#include <evol/common/ev_log.h>

void init_subpass(Subpass *subpass)
{
  subpass->colorAttachments = vec_init(VkAttachmentReference);
  subpass->inputAttachments = vec_init(VkAttachmentReference);
  subpass->depthAttchaments = vec_init(VkAttachmentReference);
}
void deinit_subpass(Subpass *subpass)
{
  vec_fini(subpass->colorAttachments);
  vec_fini(subpass->depthAttchaments);
  vec_fini(subpass->inputAttachments);
}

void build_vkrenderpass(uint32_t attachmentsCount, PassAttachment* attachments, uint32_t subpassCount, uint32_t dependencyCount, VkSubpassDependency* dependencies, VkRenderPass *renderPass)
{
  vec(VkAttachmentDescription) attachmentsDescriptions = vec_init(VkAttachmentDescription);
  vec(VkSubpassDescription) subpassDescriptions = vec_init(VkSubpassDescription);
  vec(Subpass) subpasses = vec_init(Subpass, 0, deinit_subpass);
  for (size_t i = 0; i < subpassCount; i++)
    init_subpass(subpasses + i);

  for (size_t attachmentID = 0; attachmentID < attachmentsCount; attachmentID++)
  {
    VkAttachmentDescription description = {
      .flags = NULL,
      .format = attachments[attachmentID].format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = attachments[attachmentID].loadOp,
      .storeOp = attachments[attachmentID].storeOp,
      .stencilLoadOp = attachments[attachmentID].stencilLoadOp,
      .stencilStoreOp = attachments[attachmentID].stencilStoreOp,
      .initialLayout = attachments[attachmentID].initialLayout,
      .finalLayout = attachments[attachmentID].finalLayout,
    };
    vec_push(&attachmentsDescriptions, &description);

    VkAttachmentReference reference = {
      .attachment = attachmentID,
      .layout = attachments[attachmentID].useLayout,
    };
    VkAttachmentReference* pushDestination;
    switch (attachments[attachmentID].type)
    {
      case EV_RENDERPASSATTACHMENT_TYPE_INPUT:
        pushDestination = &subpasses[attachments[attachmentID].subpass].inputAttachments;
        break;
      case EV_RENDERPASSATTACHMENT_TYPE_COLOR:
        pushDestination = &subpasses[attachments[attachmentID].subpass].colorAttachments;
        break;
      case EV_RENDERPASSATTACHMENT_TYPE_DEPTH:
        pushDestination = &subpasses[attachments[attachmentID].subpass].depthAttchaments;
        break;
    }
    vec_push(pushDestination, &reference);
  }

  for (size_t subpassID = 0; subpassID < subpassCount; subpassID++)
  {
    assert(vec_len(subpasses[subpassID].depthAttchaments) <= 1);

    VkSubpassDescription description = {
      .flags = NULL,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,

      .inputAttachmentCount = vec_len(subpasses[subpassID].inputAttachments),
      .pInputAttachments = subpasses[subpassID].inputAttachments,

      .colorAttachmentCount = vec_len(subpasses[subpassID].colorAttachments),
      .pColorAttachments = subpasses[subpassID].colorAttachments,

      .pDepthStencilAttachment = vec_len(subpasses[subpassID].depthAttchaments) == 0 ? NULL : subpasses[subpassID].depthAttchaments,

      .pResolveAttachments = NULL,

      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL,
    };
    vec_push(&subpassDescriptions, &description);
  }

  VkRenderPassCreateInfo createInfo = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = NULL,
    .flags = NULL,

    .attachmentCount = attachmentsCount,
    .pAttachments = attachmentsDescriptions,

    .subpassCount = subpassCount,
    .pSubpasses = subpassDescriptions,

    .dependencyCount = dependencyCount,
    .pDependencies = dependencies,
  };

  VK_ASSERT(vkCreateRenderPass(ev_vulkan_getlogicaldevice(), &createInfo, NULL, renderPass));

  vec_fini(subpasses);
  vec_fini(subpassDescriptions);
  vec_fini(attachmentsDescriptions);
}

void createattachmenttexture(PassAttachment attachment, EvTexture *texture)
{
  VkImageCreateInfo imageCreateInfo = {
    .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType     = VK_IMAGE_TYPE_2D,
    .format        = attachment.format,
    .extent        = (VkExtent3D) {
      .width       = attachment.extent.width,
      .height      = attachment.extent.height,
      .depth       = attachment.extent.depth,
    },
    .mipLevels     = 1,
    .arrayLayers   = 1,
    .samples       = VK_SAMPLE_COUNT_1_BIT,
    .tiling        = VK_IMAGE_TILING_OPTIMAL,
    .usage         = attachment.usageFlags,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VmaAllocationCreateInfo vmaAllocationCreateInfo = {
    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
  };
  ev_vulkan_createimage(&imageCreateInfo, &vmaAllocationCreateInfo, &texture->image);

  VkImageViewCreateInfo viewCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = texture->image.image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = attachment.format,
    .components = {0, 0, 0, 0},
    .subresourceRange = (VkImageSubresourceRange) {
      .aspectMask = attachment.aspectFlags,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    }
  };
  vkCreateImageView(ev_vulkan_getlogicaldevice(), &viewCreateInfo, NULL, &texture->imageView);

  VkSamplerCreateInfo samplerCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 1.0f,
    .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE,
    .compareEnable = VK_FALSE,
    .compareOp = VK_COMPARE_OP_ALWAYS,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .mipLodBias = 0.0f,
    .minLod = 0.0f,
    .maxLod = 0.0f,
  };
  vkCreateSampler(ev_vulkan_getlogicaldevice(), &samplerCreateInfo, NULL, &texture->sampler);
}

void ev_renderpass_build(uint32_t bufferingMode, VkExtent3D passExtent, uint32_t attachmentsCount, PassAttachment* attachments, uint32_t subpassCount, uint32_t dependencyCount, VkSubpassDependency* dependencies, RenderPass *pass)
{
  pass->extent = passExtent;

  //building VkRenderPass
  build_vkrenderpass(attachmentsCount, attachments, subpassCount, dependencyCount, dependencies, &pass->renderPass);

  //building framebuffer
  pass->framebuffers = vec_init(Framebuffer);
  vec_setlen(&pass->framebuffers, bufferingMode);

  for (size_t framebufferID = 0; framebufferID < bufferingMode; framebufferID++)
  {
    pass->framebuffers[framebufferID].frameAttachments = vec_init(EvTexture);
    vec_setlen(&pass->framebuffers[framebufferID].frameAttachments, attachmentsCount);

    vec(VkImageView) views = vec_init(VkImageView);
    vec_setlen(&views, attachmentsCount);
    for (size_t attachmentID = 0; attachmentID < attachmentsCount; attachmentID++)
    {
      createattachmenttexture(attachments[attachmentID], &pass->framebuffers[framebufferID].frameAttachments[attachmentID]);
      views[attachmentID] = pass->framebuffers[framebufferID].frameAttachments[attachmentID].imageView;
    }

    VkFramebufferCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = pass->renderPass,
      .attachmentCount = attachmentsCount,
      .pAttachments = views,
      .width = passExtent.width,
      .height = passExtent.height,
      .layers = passExtent.depth,
    };

    VK_ASSERT(vkCreateFramebuffer(ev_vulkan_getlogicaldevice(), &createInfo, NULL, &pass->framebuffers[0].framebuffer));
    vec_fini(views);
  }
}
