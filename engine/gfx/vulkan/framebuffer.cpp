#include "framebuffer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

framebuffer_builder_t& framebuffer_builder_t::add_attachment_view(VkImageView view) {
    attachments.push_back(view);
    return *this;
}

core::ref<framebuffer_t> framebuffer_builder_t::build(core::ref<context_t> context, VkRenderPass renderPass, uint32_t width, uint32_t height) {
    VkFramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.renderPass = renderPass;
    framebufferCreateInfo.attachmentCount = attachments.size();
    framebufferCreateInfo.pAttachments = attachments.data();
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = 1;

    VkFramebuffer framebuffer{};

    if (vkCreateFramebuffer(context->device(), &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        ERROR("Failed to create framebuffer");
        std::terminate();
    }

    return core::make_ref<framebuffer_t>(context, framebuffer);
}

framebuffer_t::framebuffer_t(core::ref<context_t> context, VkFramebuffer framebuffer) 
  : _context(context), _framebuffer(framebuffer) {
    TRACE("Created framebuffer");
}

framebuffer_t::~framebuffer_t() {
    vkDestroyFramebuffer(_context->device(), _framebuffer, nullptr);
    TRACE("Destroyed framebuffer");
}

} // namespace vulkan

} // namespace gfx
