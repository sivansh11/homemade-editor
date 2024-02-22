#ifndef GFX_VULKAN_RENDERPASS_HPP
#define GFX_VULKAN_RENDERPASS_HPP

#include  "context.hpp"

namespace gfx {

namespace vulkan {

class renderpass_t;

struct renderpass_builder_t {
    renderpass_builder_t& add_color_attachment(const VkAttachmentDescription& attachment_description);
    renderpass_builder_t& set_depth_attachment(const VkAttachmentDescription& attachment_description);
    core::ref<renderpass_t> build(core::ref<context_t> context);

    bool depth_added = false;
    uint32_t counter = 0;
    std::vector<VkAttachmentDescription> attachment_descriptions{};
    std::vector<VkAttachmentReference> color_attachments_refrences{};
    VkAttachmentReference depth_attachment_refrence{};
};

class renderpass_t {
public:
    renderpass_t(core::ref<context_t> context, VkRenderPass renderpass);
    ~renderpass_t();

    void begin(VkCommandBuffer commandbuffer, VkFramebuffer framebuffer, const VkRect2D render_area, const std::vector<VkClearValue>& clear_values);
    void end(VkCommandBuffer commandbuffer);

    VkRenderPass& renderpass() { return _renderpass; }

private:
    core::ref<context_t> _context;
    VkRenderPass _renderpass;
};

} // namespace vulkan

} // namespace gfx

#endif