#ifndef GFX_VULKAN_DESCRIPTOR_HPP
#define GFX_VULKAN_DESCRIPTOR_HPP

#include "context.hpp"
#include "buffer.hpp"

namespace gfx {

namespace vulkan {

class descriptor_set_t;
class descriptor_set_layout_t;

struct descriptor_set_layout_builder_t {
    descriptor_set_layout_builder_t& addLayoutBinding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t count, VkShaderStageFlags shader_stage_flags);
    
    core::ref<descriptor_set_layout_t> build(core::ref<context_t> context);

    std::vector<VkDescriptorSetLayoutBinding> descriptor_set_layout_bindings{};
};

class descriptor_set_layout_t {
public:

    descriptor_set_layout_t(core::ref<context_t> context, VkDescriptorSetLayout descriptor_set_layout);
    ~descriptor_set_layout_t();

    core::ref<descriptor_set_t> new_descriptor_set();

    VkDescriptorSetLayout& descriptor_set_layout() { return _descriptor_set_layout; } 

    descriptor_set_layout_t(const descriptor_set_layout_t&) = delete;
    descriptor_set_layout_t& operator=(const descriptor_set_layout_t&) = delete;

private:
    core::ref<context_t> _context;
    VkDescriptorSetLayout _descriptor_set_layout{};
};

struct descriptor_set_builder_t {
    core::ref<descriptor_set_t> build(core::ref<context_t> context, core::ref<descriptor_set_layout_t> descriptor_set_layout);
    core::ref<descriptor_set_t> build(core::ref<context_t> context, VkDescriptorSetLayout descriptor_set_layout);
};

class descriptor_set_t {
public:

    descriptor_set_t(core::ref<context_t> context, VkDescriptorSet descriptor_set);
    ~descriptor_set_t();

    descriptor_set_t(const descriptor_set_t&) = delete;
    descriptor_set_t& operator=(const descriptor_set_t&) = delete;

    VkDescriptorSet& descriptor_set() { return _descriptor_set; }

    struct write_t {
        write_t(core::ref<context_t> context, VkDescriptorSet descriptor_set);
        // TODO: add fix for multiple count
        write_t& pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptor_image_info, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        write_t& pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptor_buffer_info, VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        write_t& pushAccelerationStructureInfo(uint32_t binding, uint32_t count, const VkWriteDescriptorSetAccelerationStructureKHR& acceleration_set_info);

        void update();

        std::vector<VkWriteDescriptorSet> writes;
        core::ref<context_t> context;
        VkDescriptorSet descriptor_set{};
    };    

    write_t& write();

private:
    core::ref<context_t> _context;
    VkDescriptorSet _descriptor_set{}; 
};

} // namespace vulkan

} // namespace gfx

#endif