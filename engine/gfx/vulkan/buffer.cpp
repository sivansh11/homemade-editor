#include "buffer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

core::ref<buffer_t> buffer_builder_t::build(core::ref<context_t> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex) {
    bufferUsageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = bufferUsageFlags;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(context->device(), &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
        ERROR("Failed to create buffer");
        std::terminate();
    }

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(context->device(), buffer, &memory_requirements);

    VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = NULL,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0};


    VkMemoryAllocateInfo memory_allocate_info{};
    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = context->find_memory_type(memory_requirements.memoryTypeBits, memoryTypeIndex);
    memory_allocate_info.pNext = &memory_allocate_flags_info;

    VkDeviceMemory device_memory;
    if (vkAllocateMemory(context->device(), &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) {
        ERROR("Failed to allocate device memory");
        std::terminate();
    }

    vkBindBufferMemory(context->device(), buffer, device_memory, 0);

    return core::make_ref<buffer_t>(context, buffer, device_memory);
}

buffer_t::buffer_t(core::ref<context_t> context, VkBuffer buffer, VkDeviceMemory device_memory) 
  : _context(context),
    _buffer(buffer),
    _device_memory(device_memory) {
    VkBufferDeviceAddressInfo buffer_device_address_info{};
    buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    buffer_device_address_info.pNext = NULL;
    buffer_device_address_info.buffer = _buffer;
    _device_address = vkGetBufferDeviceAddress(_context->device(), &buffer_device_address_info);
    TRACE("Created buffer");
}

buffer_t::~buffer_t() {
    vkDestroyBuffer(_context->device(), _buffer, nullptr);
    vkFreeMemory(_context->device(), _device_memory, nullptr);
    TRACE("Destoryed buffer");
}

void buffer_t::invalidate(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mappedMemoryRange{};
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.memory = _device_memory;
    mappedMemoryRange.offset = offset;
    mappedMemoryRange.size = size;
    vkInvalidateMappedMemoryRanges(_context->device(), 1, &mappedMemoryRange);
}

void buffer_t::flush(VkDeviceSize offset, VkDeviceSize size) {
    VkMappedMemoryRange mappedMemoryRange{};
    mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRange.memory = _device_memory;
    mappedMemoryRange.offset = offset;
    mappedMemoryRange.size = size;
    vkFlushMappedMemoryRanges(_context->device(), 1, &mappedMemoryRange);
}

void *buffer_t::map(VkDeviceSize offset, VkDeviceSize size) {
    if (_mapped) return _mapped;
    vkMapMemory(_context->device(), _device_memory, offset, size, 0, &_mapped);
    return _mapped;
}

void buffer_t::unmap() {
    if (!_mapped) return;
    vkUnmapMemory(_context->device(), _device_memory);
}

void buffer_t::copy(core::ref<context_t> context, buffer_t& src_buffer, buffer_t& dst_buffer, const VkBufferCopy& buffer_copy) {
    VkCommandBuffer commandBuffer = context->start_single_use_commandbuffer();

    vkCmdCopyBuffer(commandBuffer, src_buffer.buffer(), dst_buffer.buffer(), 1, &buffer_copy);

    context->end_single_use_commandbuffer(commandBuffer);
}

} // namespace vulkan

} // namespace gfx
