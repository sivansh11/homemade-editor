#ifndef GFX_VULKAN_IMAGE_HPP
#define GFX_VULKAN_IMAGE_HPP

#include "context.hpp"
#include "buffer.hpp"

#include <filesystem>
#include <unordered_map>
#include <cassert>

namespace gfx {

namespace vulkan {

class image_t;

struct image_builder_t {
    image_builder_t& mip_maps();
    image_builder_t& set_tiling(VkImageTiling image_tiling);
    image_builder_t& set_initial_layout(VkImageLayout image_layout);
    // setting compare op enables compare
    image_builder_t& set_compare_op(VkCompareOp compare_op);
    core::ref<image_t> build2D(core::ref<context_t> context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags image_usage_flags, VkMemoryPropertyFlags memory_type_index);
    core::ref<image_t> build3D(core::ref<context_t> context, uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags image_usage_flags, VkMemoryPropertyFlags memory_type_index);
    core::ref<image_t> load_from_path(core::ref<context_t> context, const std::filesystem::path& file_path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

    bool enable_mip_maps{false};
    bool enable_compare_op{false};
    VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;
    VkImageTiling image_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

const VkImageViewType auto_image_view_type = static_cast<VkImageViewType>(0x7FFFFFFF - 1);  // -1 cause I dont wanna collide with the max enum enum in the enum definition
const VkFormat auto_image_format = static_cast<VkFormat>(0x7FFFFFFF - 1);  
const uint32_t auto_level_count = 0x7FFFFFFF;   // this should work I guess ??
const uint32_t auto_layer_count = 0x7FFFFFFF;   // this should work I guess ??
const uint32_t default_base_mip_level = 0;
const uint32_t default_base_array_layer = 0;

struct image_view_create_info_t {
    VkImageViewType image_view_type = auto_image_view_type;
    VkFormat format = auto_image_format;
    uint32_t base_mip_level = default_base_mip_level;
    uint32_t level_count = auto_level_count;  // gets the max level count possible for the image
    uint32_t base_array_layer = default_base_array_layer;
    uint32_t layer_count = auto_layer_count;

    bool operator==(const image_view_create_info_t& other) const {
        return image_view_type  == other.image_view_type  &&
               format           == other.format           &&
               base_mip_level   == other.base_mip_level   &&
               level_count      == other.level_count      &&
               base_array_layer == other.base_array_layer &&
               layer_count      == other.layer_count;
    }
};

} // namespace vulkan

} // namespace gfx

namespace std {

template <>
struct hash<gfx::vulkan::image_view_create_info_t> {
    size_t operator()(gfx::vulkan::image_view_create_info_t const &image_view_create_info) const {
        size_t seed = 0;
        core::hash_combine(seed, image_view_create_info.image_view_type, image_view_create_info.format, image_view_create_info.base_mip_level, image_view_create_info.level_count, image_view_create_info.base_array_layer, image_view_create_info.layer_count);
        return seed;
    }
};

}  // namespace std

namespace gfx {

namespace vulkan {

struct image_info_t {
    VkImage image{};
    VkDeviceMemory device_memory{};
    VkFormat format{};
    uint32_t level_count{};
    uint32_t layer_count{};
    uint32_t width{}, height{}, depth{};
    // if i ever need this, fix the numChannels giving out 4 channels
    // VkDeviceSize size{};
    VkImageAspectFlags aspect{};
    VkImageType image_type{};
    std::unordered_map<image_view_create_info_t, VkImageView> image_view_table;
};

class image_t {
public:
    image_t(core::ref<context_t> context, const image_info_t& image_info);
    ~image_t();

    VkDescriptorImageInfo descriptor_info(VkImageLayout image_layout, const sampler_create_info_t& sampler_info = {}, const image_view_create_info_t& image_view_info = {}) {
        return VkDescriptorImageInfo {
            .sampler = sampler(sampler_info),  
            .imageView = image_view(image_view_info),
            .imageLayout = image_layout,
        };
    }

    // todo: add base mip and level count 
    void transition_layout(VkImageLayout old_layout, VkImageLayout new_layout, uint32_t base_mip_level = 0);
    // todo: add base mip and level count 
    void transition_layout(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t base_mip_level = 0);
    void gen_mip_maps(VkCommandBuffer commandbuffer, VkImageLayout old_layout, VkImageLayout new_layout, VkFilter filter = VK_FILTER_LINEAR);
    void gen_mip_maps(VkImageLayout old_layout, VkImageLayout new_layout, VkFilter filter = VK_FILTER_LINEAR);

    static void copy_buffer_to_image(core::ref<context_t> context, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy);
    static void copy_buffer_to_image(VkCommandBuffer commandbuffer, buffer_t& buffer, image_t& image, VkImageLayout image_layout, VkBufferImageCopy buffer_image_copy);

    void *map(VkDeviceSize poffset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();

    // use flush after writing (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    // use invalidate before reading (NOTE: only required when not VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    

    VkImage& image() { return _image_info.image; }

    VkImageView image_view(const image_view_create_info_t& image_view_info = {}) {
        auto image_view_itr = _image_info.image_view_table.find(image_view_info);
        if (image_view_itr != _image_info.image_view_table.end()) {
            // found
            return image_view_itr->second; 
        }
        // didnt find
        create_image_view(image_view_info);
        return image_view(image_view_info);  // create image view adds it to the map        
    }

    VkSampler sampler(const sampler_create_info_t& sampler_info = {}) {
        return _context->sampler(sampler_info);
    }

    VkFormat& format() { return _image_info.format; }
    // VkDeviceSize size() { return _image_info.size; }
    VkImageAspectFlags aspect() { return _image_info.aspect; }
    uint32_t level_count() { return _image_info.level_count; }
    std::pair<uint32_t, uint32_t> dimensions() { return {_image_info.width, _image_info.height}; }

    friend class renderpass_t;

private:
    void create_image_view(const image_view_create_info_t& image_view_create_info);

private:
    core::ref<context_t> _context{};
    image_info_t _image_info{};
};

} // namespace vulkan

} // namespace gfx

#endif