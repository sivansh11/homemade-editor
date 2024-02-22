#ifndef GFX_VULKAN_ACCELERATION_STRUCTURE_HPP
#define GFX_VULKAN_ACCELERATION_STRUCTURE_HPP

#include "core/core.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/buffer.hpp"

namespace gfx {

namespace vulkan {

struct acceleration_structure_t;

enum acceleration_structure_type_t {
    e_bottom,
    e_top,
};

struct acceleration_structure_builder_t {
    acceleration_structure_builder_t& triangles(uint64_t vertex_buffer_device_address, uint32_t vertex_stride, uint32_t vertex_count, uint64_t index_buffer_device_address, uint32_t index_count);
    acceleration_structure_builder_t& instances(uint64_t instance_buffer_device_address, uint32_t instance_count);
    core::ref<acceleration_structure_t> build(core::ref<context_t> context);

    acceleration_structure_type_t type;
    VkAccelerationStructureGeometryKHR geometry{};
    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
    uint32_t primitive_count{};
};

struct acceleration_structure_t {
public:
    acceleration_structure_t(core::ref<context_t> context, core::ref<buffer_t> acceleration_structure_buffer, VkAccelerationStructureKHR acceleration_structure);
    ~acceleration_structure_t();

    VkAccelerationStructureKHR& acceleration_structure() { return _acceleration_structure; }
    core::ref<buffer_t> acceleration_structure_buffer() { return _acceleration_structure_buffer; }
    VkDeviceAddress& device_address() { return _device_address; }

private:
    core::ref<context_t> _context;
    core::ref<buffer_t> _acceleration_structure_buffer;
    VkAccelerationStructureKHR _acceleration_structure;
    VkDeviceAddress _device_address;
};

} // namespace vulkan

} // namespace gfx

#endif