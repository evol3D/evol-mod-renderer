#include <Vulkan.h>
#include <Renderer_types.h>

typedef enum {
  EV_RENDERPASSATTACHMENT_TYPE_INPUT,
  EV_RENDERPASSATTACHMENT_TYPE_COLOR,
  EV_RENDERPASSATTACHMENT_TYPE_DEPTH,
} EV_RENDEPASSATTACHMENT_TYPE;

typedef struct {
  uint32_t subpass;

  VkFormat format;
  EV_RENDEPASSATTACHMENT_TYPE type;

  VkAttachmentLoadOp  loadOp;
  VkAttachmentStoreOp storeOp;
  VkAttachmentLoadOp  stencilLoadOp;
  VkAttachmentStoreOp stencilStoreOp;

  VkImageLayout initialLayout;
  VkImageLayout useLayout;
  VkImageLayout finalLayout;

  VkExtent3D extent;
  VkImageUsageFlags usageFlags;
  VkImageAspectFlags aspectFlags;
} PassAttachment;

typedef struct {
  vec(VkAttachmentReference) colorAttachments;
  vec(VkAttachmentReference) inputAttachments;
  vec(VkAttachmentReference) depthAttchaments;
} Subpass;

typedef struct {
  VkFramebuffer framebuffer;
  vec(EvTexture) frameAttachments;
} Framebuffer;

typedef struct {
  VkRenderPass renderPass;
  vec(Framebuffer) framebuffers;

  VkExtent3D extent;
} RenderPass;

void ev_renderpass_build(uint32_t bufferingMode, VkExtent3D passExtent, uint32_t attachmentsCount, PassAttachment* attachments, uint32_t subpassCount, uint32_t dependencyCount, VkSubpassDependency* dependencies, RenderPass *pass);
