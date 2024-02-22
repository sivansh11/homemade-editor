#include "descriptor.hpp"

#include "core/log.hpp"

namespace gfx {
    
namespace vulkan {

descriptor_set_layout_builder_t& descriptor_set_layout_builder_t::addLayoutBinding(uint32_t binding, VkDescriptorType descriptor_type, uint32_t count, VkShaderStageFlags shader_stage_flags) {
    VkDescriptorSetLayoutBinding descriptor_set_layout_binding{};
    descriptor_set_layout_binding.binding = binding;
    descriptor_set_layout_binding.descriptorType = descriptor_type;
    descriptor_set_layout_binding.descriptorCount = count;
    descriptor_set_layout_binding.stageFlags = shader_stage_flags;
    descriptor_set_layout_bindings.push_back(descriptor_set_layout_binding);
    return *this;
}

core::ref<descriptor_set_layout_t> descriptor_set_layout_builder_t::build(core::ref<context_t> context) {
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{};
    descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.bindingCount = descriptor_set_layout_bindings.size();
    descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings.data();

    VkDescriptorSetLayout descriptor_set_layout{};

    if (vkCreateDescriptorSetLayout(context->device(), &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
        ERROR("Failed to create descriptor set");
        std::terminate();
    }

    return core::make_ref<descriptor_set_layout_t>(context, descriptor_set_layout);
}

descriptor_set_layout_t::descriptor_set_layout_t(core::ref<context_t> context, VkDescriptorSetLayout descriptor_set_layout) 
  : _context(context),
    _descriptor_set_layout(descriptor_set_layout) {

    TRACE("Created descriptor set layout");
}

descriptor_set_layout_t::~descriptor_set_layout_t() {
    vkDestroyDescriptorSetLayout(_context->device(), _descriptor_set_layout, nullptr);
    TRACE("Destroyed descriptor set layout");
}

core::ref<descriptor_set_t> descriptor_set_layout_t::new_descriptor_set() {
    return descriptor_set_builder_t{}
        .build(_context, _descriptor_set_layout);
}


core::ref<descriptor_set_t> descriptor_set_builder_t::build(core::ref<context_t> context, core::ref<descriptor_set_layout_t> descriptor_set_layout) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = context->descriptor_pool();
    descriptor_set_allocate_info.descriptorSetCount = 1;
    VkDescriptorSetLayout descriptor_set_layout_p[] = { descriptor_set_layout->descriptor_set_layout() }; 
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layout_p;

    VkDescriptorSet descriptor_set;

    if (vkAllocateDescriptorSets(context->device(), &descriptor_set_allocate_info, &descriptor_set) != VK_SUCCESS) {
        ERROR("Failed to allocate descriptor set");
        std::terminate();
    }
    return core::make_ref<descriptor_set_t>(context, descriptor_set);
}

core::ref<descriptor_set_t> descriptor_set_builder_t::build(core::ref<context_t> context, VkDescriptorSetLayout descriptor_set_layout) {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info{};
    descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.descriptorPool = context->descriptor_pool();
    descriptor_set_allocate_info.descriptorSetCount = 1;
    VkDescriptorSetLayout descriptor_set_layout_p[] = { descriptor_set_layout }; 
    descriptor_set_allocate_info.pSetLayouts = descriptor_set_layout_p;

    VkDescriptorSet descriptor_set;

    if (vkAllocateDescriptorSets(context->device(), &descriptor_set_allocate_info, &descriptor_set) != VK_SUCCESS) {
        ERROR("Failed to allocate descriptor set");
        std::terminate();
    }
    return core::make_ref<descriptor_set_t>(context, descriptor_set);
}

descriptor_set_t::descriptor_set_t(core::ref<context_t> context, VkDescriptorSet descriptor_set) 
  : _context(context),
    _descriptor_set(descriptor_set) {
    TRACE("Created descriptor set");
}

descriptor_set_t::~descriptor_set_t() {
    // vkFreeDescriptorSets(_context->device(), _context->descriptor_pool(), 1, &_descriptor_set);
    // TRACE("Destroyed descriptor set");
}

descriptor_set_t::write_t::write_t(core::ref<context_t> context, VkDescriptorSet descriptor_set) 
  : context(context),
    descriptor_set(descriptor_set) {
    
}

descriptor_set_t::write_t& descriptor_set_t::write() {
    return *new write_t(_context, _descriptor_set);    
}

descriptor_set_t::write_t& descriptor_set_t::write_t::pushImageInfo(uint32_t binding, uint32_t count, const VkDescriptorImageInfo& descriptor_image_info, VkDescriptorType type) {
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstBinding = binding;
    write_descriptor_set.descriptorCount = count;
    write_descriptor_set.descriptorType = type;
    write_descriptor_set.dstSet = descriptor_set;
    write_descriptor_set.pImageInfo = &descriptor_image_info;
    writes.push_back(write_descriptor_set);
    return *this;
}

descriptor_set_t::write_t& descriptor_set_t::write_t::pushBufferInfo(uint32_t binding, uint32_t count, const VkDescriptorBufferInfo& descriptor_buffe_info, VkDescriptorType type) {
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstBinding = binding;
    write_descriptor_set.descriptorCount = count;
    write_descriptor_set.descriptorType = type;
    write_descriptor_set.dstSet = descriptor_set;
    write_descriptor_set.pBufferInfo = &descriptor_buffe_info;
    writes.push_back(write_descriptor_set);
    return *this;
}

descriptor_set_t::write_t& descriptor_set_t::write_t::pushAccelerationStructureInfo(uint32_t binding, uint32_t count, const VkWriteDescriptorSetAccelerationStructureKHR& acceleration_set_info) {
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.pNext = &acceleration_set_info;
    write_descriptor_set.dstBinding = binding;
    write_descriptor_set.descriptorCount = count;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write_descriptor_set.dstSet = descriptor_set;
    writes.push_back(write_descriptor_set);
    return *this;
}


void descriptor_set_t::write_t::update() {
    vkUpdateDescriptorSets(context->device(), writes.size(), writes.data(), 0, nullptr);
    delete this;
}

} // namespace vulkan

} // namespace gfx
