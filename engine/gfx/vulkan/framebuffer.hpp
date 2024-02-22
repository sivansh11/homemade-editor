#ifndef GFX_VULKAN_FRAMEBUFFER_HPP
#define GFX_VULKAN_FRAMEBUFFER_HPP

#include "context.hpp"
#include "renderpass.hpp"

namespace gfx {

namespace vulkan {

class framebuffer_t;

struct framebuffer_builder_t {
    framebuffer_builder_t& add_attachment_view(VkImageView view);
    core::ref<framebuffer_t> build(core::ref<context_t> context, VkRenderPass renderpass, uint32_t width, uint32_t height);

    std::vector<VkImageView> attachments{};
};

class framebuffer_t {
public:
    framebuffer_t(core::ref<context_t> context, VkFramebuffer framebuffer);
    ~framebuffer_t();

    VkFramebuffer& framebuffer() { return _framebuffer; }
    
private:
    core::ref<context_t> _context;
    VkFramebuffer _framebuffer;
};

} // namespace vulkan

} // namespace gfx

#endif