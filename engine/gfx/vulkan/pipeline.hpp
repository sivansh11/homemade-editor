#ifndef GFX_VULKAN_PIPELINE_HPP
#define GFX_VULKAN_PIPELINE_HPP

#include "context.hpp"
#include "descriptor.hpp"

#include <filesystem>
#include <vector>
#include <fstream>

namespace gfx {

namespace vulkan {

namespace utils {

static std::vector<char> read_file(const std::filesystem::path& filename);

} // namespace utils


VkShaderModule load_shader_module(core::ref<gfx::vulkan::context_t> context, const std::filesystem::path& shader_path);

class shader_t;

enum shader_type_t {
    e_vertex,
    e_fragment,
    e_compute,
    e_geometry,
};

struct shader_builder_t {
    // in future maybe return an optional
    core::ref<shader_t> build(core::ref<gfx::vulkan::context_t> context, shader_type_t shader_type, const std::string& name, const std::string& code);
};

class shader_t {
public:
    shader_t(core::ref<context_t> context, const std::vector<uint32_t>& shader_module_code, const std::string& name, shader_type_t shader_type);
    ~shader_t();
    
    VkShaderModule& shader_module() { return _shader_module; }
    shader_type_t shader_type() { return _shader_type; }

private:
    core::ref<context_t> _context;
    std::string _name;
    shader_type_t _shader_type;
    VkShaderModule _shader_module;
};

class pipeline_t;

struct pipeline_builder_t {
    pipeline_builder_t();
    pipeline_builder_t& add_shader(const std::filesystem::path& shader_path);
    pipeline_builder_t& add_shader(core::ref<shader_t> shader);
    pipeline_builder_t& add_dynamic_state(VkDynamicState state);

    pipeline_builder_t& add_descriptor_set_layout(core::ref<descriptor_set_layout_t> descriptor_set_layout);
    pipeline_builder_t& add_push_constant_range(uint64_t offset, uint64_t size, VkShaderStageFlags shader_stage_flag);

    pipeline_builder_t& add_default_color_blend_attachment_state();
    pipeline_builder_t& add_color_blend_attachment_state(const VkPipelineColorBlendAttachmentState& pipeline_color_blend_attachment_state);

    pipeline_builder_t& set_depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo& pipeline_depth_stencil_state);

    pipeline_builder_t& set_polygon_mode(const VkPolygonMode& polygon_mode);

    pipeline_builder_t& add_vertex_input_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
    pipeline_builder_t& add_vertex_input_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset);   
    pipeline_builder_t& set_vertex_input_binding_description_vector(const std::vector<VkVertexInputBindingDescription>& val);
    pipeline_builder_t& set_vertex_input_attribute_description_vector(const std::vector<VkVertexInputAttributeDescription>& val);        

    core::ref<pipeline_t> build(core::ref<context_t> context, VkRenderPass renderpass = VK_NULL_HANDLE);

    std::vector<VkDynamicState> dynamic_states;
    std::vector<std::filesystem::path> shader_paths;
    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
    std::vector<VkVertexInputBindingDescription> vertex_input_binding_descriptions;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts; 
    std::vector<VkPipelineColorBlendAttachmentState> pipeline_color_blend_attachment_states;
    std::vector<VkPushConstantRange> push_constant_ranges;
    VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info{};
    VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
};

class pipeline_t {
public:
    pipeline_t(core::ref<context_t> context, VkPipelineLayout pipeline_layout, VkPipeline pipeline, VkPipelineBindPoint pipeline_bind_point);
    ~pipeline_t();

    void bind(VkCommandBuffer commandbuffer);

    VkPipelineLayout& pipeline_layout() { return _pipeline_layout; }

private:
    core::ref<context_t> _context;
    VkPipelineLayout _pipeline_layout;
    VkPipeline _pipeline;
    VkPipelineBindPoint _pipeline_bind_point;
};

} // namespace vulkan

} // namespace gfx

#endif