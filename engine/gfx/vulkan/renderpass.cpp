#include "renderpass.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

renderpass_builder_t& renderpass_builder_t::add_color_attachment(const VkAttachmentDescription& attachment_description) {
    attachment_descriptions.push_back(attachment_description);
    VkAttachmentReference attachment_refrence{};
    attachment_refrence.attachment = counter++;
    attachment_refrence.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachments_refrences.push_back(attachment_refrence);
    return *this;
}

renderpass_builder_t& renderpass_builder_t::set_depth_attachment(const VkAttachmentDescription& attachment_description) {
    if (depth_added) {
        ERROR("Depth already added");
        std::terminate();
    } 
    attachment_descriptions.push_back(attachment_description);
    VkAttachmentReference attachment_refrence{};
    attachment_refrence.attachment = counter++;
    attachment_refrence.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_attachment_refrence = attachment_refrence;
    depth_added = true;
    return *this;
}

core::ref<renderpass_t> renderpass_builder_t::build(core::ref<context_t> context) {
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = color_attachments_refrences.size();
    subpass.pColorAttachments = color_attachments_refrences.data();
    if (depth_added) 
        subpass.pDepthStencilAttachment = &depth_attachment_refrence;
    
    VkSubpassDependency dependencies[2]{};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask |= color_attachments_refrences.size() > 0 ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : 0;
	dependencies[0].dstStageMask |= depth_added ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : 0;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = color_attachments_refrences.size() > 0 ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
	dependencies[0].dstAccessMask |= depth_added ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    
    dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = dependencies[0].dstStageMask;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = dependencies[0].dstAccessMask;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderpass_create_info{};
    renderpass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpass_create_info.attachmentCount = attachment_descriptions.size();
    renderpass_create_info.pAttachments = attachment_descriptions.data();
    renderpass_create_info.subpassCount = 1;              // hard coded
    renderpass_create_info.pSubpasses = &subpass;         // hard coded
    renderpass_create_info.dependencyCount = 2;           // hard coded
    renderpass_create_info.pDependencies = dependencies;  // hard coded

    VkRenderPass renderpass{};

    if (vkCreateRenderPass(context->device(), &renderpass_create_info, nullptr, &renderpass) != VK_SUCCESS) {
        ERROR("Failed to create renderpass");
        std::terminate();
    }

    return core::make_ref<renderpass_t>(context, renderpass);
}

renderpass_t::renderpass_t(core::ref<context_t> context, VkRenderPass renderpass)
  : _context(context), _renderpass(renderpass) {
    TRACE("Created renderpass");
}

renderpass_t::~renderpass_t() {
    vkDestroyRenderPass(_context->device(), _renderpass, nullptr);
    TRACE("Destroyed renderpass");
}

void renderpass_t::begin(VkCommandBuffer commandbuffer, VkFramebuffer framebuffer, const VkRect2D render_area, const std::vector<VkClearValue>& clear_values) {
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.framebuffer = framebuffer;
    renderPassBeginInfo.renderPass = _renderpass;
    renderPassBeginInfo.renderArea = render_area;
    renderPassBeginInfo.clearValueCount = clear_values.size();
    renderPassBeginInfo.pClearValues = clear_values.data();
    vkCmdBeginRenderPass(commandbuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // hardcoded subpass contents inline
}

void renderpass_t::end(VkCommandBuffer commandbuffer) {
    vkCmdEndRenderPass(commandbuffer);
}

} // namespace vulkan

} // namespace gfx
