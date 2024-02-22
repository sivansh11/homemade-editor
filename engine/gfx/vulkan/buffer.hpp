#ifndef GFX_VULKAN_BUFFER_HPP
#define GFX_VULKAN_BUFFER_HPP

#include "context.hpp"

#include <limits>

namespace gfx {

namespace vulkan {

inline uint32_t aligned_size(uint32_t size, uint32_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

class buffer_t {
public:

    buffer_t(core::ref<context_t> context, VkBuffer buffer, VkDeviceMemory device_memory);
    ~buffer_t();

    VkDescriptorBufferInfo descriptor_info(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) {
        return VkDescriptorBufferInfo{
            .buffer = buffer(),
            .offset = offset,
            .range = range
        };
    } 

    void *map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    
    // todo: need to add commandbuffer version
    static void copy(core::ref<context_t> context, buffer_t& src_buffer, buffer_t& dst_buffer, const VkBufferCopy& buffer_copy);

    VkBuffer& buffer() { return _buffer; }
    VkDeviceMemory& device_memory() { return _device_memory; }
    VkDeviceAddress& device_address() { return _device_address; }

private:
    void *_mapped{nullptr};
    core::ref<context_t> _context;
    VkBuffer _buffer;
    VkDeviceMemory _device_memory;
    VkDeviceAddress _device_address;
};

struct buffer_builder_t {
    core::ref<buffer_t> build(core::ref<context_t> context, VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryTypeIndex);
};  

} // namespace vulkan

// class buffer_t {
// public:
//     struct buffer_info_t {
//         VkDeviceSize _size /* vulkan buffer size */;
//         VkBufferUsageFlags _buffer_usage /* vulkan buffer usage flags */;
//         VkMemoryPropertyFlags _memory_property /* vulkan device memory property flags */;
//         VkBuffer _buffer /* vulkan buffer handle */;
//         VkDeviceMemory _device_memory /* vulkan device memory handle */;
//     };
    
//     static buffer_t make_buffer(core::ref<vulkan::Context> context, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags memory_property);
//     static core::ref<buffer_t> make_buffer_ref(core::ref<vulkan::Context> context, VkDeviceSize size, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags memory_property);

//     buffer_t(core::ref<vulkan::Context> context, buffer_info_t buffer_info);
//     ~buffer_t();

// private:
//     core::ref<vulkan::Context> _context;
//     buffer_info_t _buffer_info;
// };

} // namespace gfx

#endif 