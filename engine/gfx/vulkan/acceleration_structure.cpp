#include "acceleration_structure.hpp"

#include "core/log.hpp"

namespace gfx::vulkan {

acceleration_structure_builder_t& acceleration_structure_builder_t::triangles(uint64_t vertex_buffer_device_address, uint32_t vertex_stride, uint32_t vertex_count, uint64_t index_buffer_device_address, uint32_t index_count) {
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertex_buffer_device_address;
    geometry.geometry.triangles.vertexStride = vertex_stride;
    geometry.geometry.triangles.maxVertex = vertex_count;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = index_buffer_device_address;

    build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_geometry_info.geometryCount = 1;
    build_geometry_info.pGeometries = &geometry;

    primitive_count = index_count / 3;

    type = acceleration_structure_type_t::e_bottom;

    return *this;
}

acceleration_structure_builder_t& acceleration_structure_builder_t::instances(uint64_t instance_buffer_device_address, uint32_t instance_count) {
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instance_buffer_device_address;

    build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    build_geometry_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    build_geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    build_geometry_info.geometryCount = 1;
    build_geometry_info.pGeometries = &geometry;

    primitive_count = instance_count;

    return *this;
}

core::ref<acceleration_structure_t> acceleration_structure_builder_t::build(core::ref<context_t> context) {
    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info{};
    build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    vkGetAccelerationStructureBuildSizesKHR(context->device(),
                                            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &build_geometry_info,
                                            &primitive_count,
                                            &build_sizes_info);
    
    auto acceleration_structure_buffer = buffer_builder_t{}
        .build(context,
               build_sizes_info.accelerationStructureSize,
               VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
               VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);
    
    VkAccelerationStructureCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    create_info.buffer = acceleration_structure_buffer->buffer();
    create_info.size = build_sizes_info.accelerationStructureSize;
    create_info.type = build_geometry_info.type;

    VkAccelerationStructureKHR acceleration_structure;

    vkCreateAccelerationStructureKHR(context->device(),
                                        &create_info,
                                        nullptr,
                                        &acceleration_structure);

    auto scratch_buffer = buffer_builder_t{}
        .build(context,
                build_sizes_info.buildScratchSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    build_geometry_info.dstAccelerationStructure = acceleration_structure;
    build_geometry_info.scratchData.deviceAddress = scratch_buffer->device_address();

    VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
    build_range_info.primitiveCount = primitive_count;
    build_range_info.primitiveOffset = 0;
    build_range_info.firstVertex = 0;
    build_range_info.transformOffset = 0;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> build_range_infos = { &build_range_info };

    context->single_use_commandbuffer([&](VkCommandBuffer commandbuffer) {
        vkCmdBuildAccelerationStructuresKHR(commandbuffer,
                                    build_range_infos.size(),
                                    &build_geometry_info,
                                    build_range_infos.data());
    });


    // VkAccelerationStructureInstanceKHR instance{};
    // instance.transform = {
    //     1.0f, 0.0f, 0.0f, 0.0f,
    //     0.0f, 1.0f, 0.0f, 0.0f,
    //     0.0f, 0.0f, 1.0f, 0.0f,
    // };
    // instance.instanceCustomIndex = 0;
    // instance.mask = 0xFF;
    // instance.instanceShaderBindingTableRecordOffset = 0;
    // instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    // instance.accelerationStructureReference = device_address;

    return core::make_ref<acceleration_structure_t>(context, acceleration_structure_buffer, acceleration_structure);
}

acceleration_structure_t::acceleration_structure_t(core::ref<context_t> context, core::ref<buffer_t> acceleration_structure_buffer, VkAccelerationStructureKHR acceleration_structure) 
  : _context(context), _acceleration_structure_buffer(acceleration_structure_buffer), _acceleration_structure(acceleration_structure) {
    VkAccelerationStructureDeviceAddressInfoKHR device_address_info{};
    device_address_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    device_address_info.accelerationStructure = acceleration_structure;
    _device_address = vkGetAccelerationStructureDeviceAddressKHR(context->device(), &device_address_info);
    TRACE("Created acceleration structure");
}

acceleration_structure_t::~acceleration_structure_t() {
    vkDestroyAccelerationStructureKHR(_context->device(), _acceleration_structure, nullptr);
    TRACE("Destroyed acceleration structure");
}

} // namespace gfx::vulkan
