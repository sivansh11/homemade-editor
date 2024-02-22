#ifndef GFX_VULKAN_CONTEXT_HPP
#define GFX_VULKAN_CONTEXT_HPP

#include "core/core.hpp"

#define VK_NO_PROTOTYPES
#include <volk.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "core/window.hpp"

#include <memory>
#include <vector>

#include "core/window.hpp"

#include <volk.h>

#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace gfx {

namespace vulkan {

struct queue_family_indices_t {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool is_complete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct swapchain_support_details_t {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

struct sampler_create_info_t {
    VkFilter                mag_filter = /*VK_FILTER_LINEAR*/ VK_FILTER_NEAREST;
    VkFilter                min_filter = /*VK_FILTER_LINEAR*/ VK_FILTER_NEAREST;
    VkSamplerMipmapMode     mipmap_mode = /*VK_SAMPLER_MIPMAP_MODE_LINEAR*/ VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VkSamplerAddressMode    address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode    address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode    address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    float                   mip_lod_bias = 0;
    VkBool32                anisotropy_enable = VK_FALSE;
    float                   max_anisotropy = 0;  // maybe shouldnt be 0 ?
    VkBool32                compare_enable = VK_FALSE;
    VkCompareOp             compare_op = VK_COMPARE_OP_LESS;  // maybe should choose a different default
    float                   min_lod = 0;
    float                   max_lod = VK_LOD_CLAMP_NONE;
    VkBorderColor           border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32                unnormalized_coordinates = VK_FALSE;

    bool operator==(const sampler_create_info_t& other) const {
        return mag_filter == other.mag_filter &&
               min_filter == other.min_filter &&
               mipmap_mode == other.mipmap_mode &&
               address_mode_u == other.address_mode_u &&
               address_mode_v == other.address_mode_v &&
               address_mode_w == other.address_mode_w &&
               mip_lod_bias == other.mip_lod_bias &&
               anisotropy_enable == other.anisotropy_enable &&
               max_anisotropy == other.max_anisotropy &&
               compare_enable == other.compare_enable &&
               compare_op == other.compare_op &&
               min_lod == other.min_lod &&
               max_lod == other.max_lod &&
               border_color == other.border_color &&
               unnormalized_coordinates == other.unnormalized_coordinates;
    }
};

namespace utils {

// might be wrong ?
template <typename T>
inline uint32_t reinterpret_to_u32(const T& value) {
    static_assert(sizeof(T) == sizeof(uint32_t));  
    return reinterpret_cast<const uint32_t&>(value);  
}    

} // namespace utils

} // namespace vulkan

} // namespace gfx

namespace std {

template <>
struct hash<gfx::vulkan::sampler_create_info_t> {
    size_t operator()(gfx::vulkan::sampler_create_info_t const &sampler_create_info) const {
        size_t seed = 0;  // seed is u64 while all my params are u32, might be an issue ??
        core::hash_combine(seed, 
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.mag_filter), 
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.min_filter),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.mipmap_mode),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.address_mode_u),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.address_mode_v),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.address_mode_w),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.mip_lod_bias),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.anisotropy_enable),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.max_anisotropy),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.compare_enable),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.compare_op),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.min_lod),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.max_lod),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.border_color),
                           gfx::vulkan::utils::reinterpret_to_u32(sampler_create_info.unnormalized_coordinates)
        );
        return seed;
    }
};

}  // namespace std

namespace gfx {

namespace vulkan {

class context_t {
public:
    
    context_t(core::ref<core::window_t> window, uint32_t MAX_FRAMES_IN_FLIGHT, bool validation, bool enable_raytracing = false);
    ~context_t();

    std::optional<std::pair<VkCommandBuffer, uint32_t>> start_frame();
    bool end_frame(VkCommandBuffer commandbuffer);

    void begin_swapchain_renderpass(VkCommandBuffer commandbuffer, const VkClearValue& clearValue);
    void end_swapchain_renderpass(VkCommandBuffer commandbuffer);

    VkCommandBuffer start_single_use_commandbuffer();
    void end_single_use_commandbuffer(VkCommandBuffer commandbuffer);
    void single_use_commandbuffer(std::function<void(VkCommandBuffer)> fn);

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);

    VkSampler sampler(const sampler_create_info_t& sampler_create_info);

    VkInstance& instance() { return _instance; }
    VkPhysicalDevice& physical_device() { return _physical_device; }
    VkDevice& device() { return _device; }
    VkSwapchainKHR& swapchain() { return _swapchain; }

    queue_family_indices_t queue_family_indices() { return find_queue_families(_physical_device); }

    VkFormat& swapchain_format() { return _swapchain_format; }
    VkExtent2D& swapchain_extent() { return _swapchain_extent; }

    VkQueue& graphics_queue() { return _graphics_queue; }
    VkQueue& present_queue() { return _present_queue; }
    
    VkRenderPass& swapchain_renderpass() { return _swapchain_renderpass; }

    uint32_t swapchain_image_count() { return _swapchain_images.size(); }
 
    uint32_t& current_frame() { return _current_frame; }

    VkPhysicalDeviceProperties& physical_device_properties() { return _physical_device_properties; }

    // NOTE: maybe change this
    std::vector<VkImageView>& swapchain_image_views() { return _swapchain_image_views; }
    std::vector<VkFramebuffer>& swapchain_framebuffers() { return _swapchain_framebuffers; }

    VkDescriptorPool& descriptor_pool() { return _descriptor_pool; }


    void wait_idle() { 
        vkDeviceWaitIdle(_device);
    }
    
    void add_resize_callback(std::function<void()> resize_call_back) {
        _resize_call_backs.push_back(resize_call_back);
    }

    const core::ref<core::window_t> window() const { return _window; }

    const uint32_t MAX_FRAMES_IN_FLIGHT;

private:
    bool check_instance_validation_layer_support();
    void push_instance_validation_layers();
    void push_instance_debug_utils_messenger_extension();
    void push_required_instance_extensions();
    VkDebugUtilsMessengerCreateInfoEXT get_debug_utils_messenger_create_info();
    void setup_debug_messenger();

    void create_surface();

    void push_required_device_extensions();
    queue_family_indices_t find_queue_families(VkPhysicalDevice physical_device);
    bool is_device_extension_supported(VkPhysicalDevice physical_device);
    swapchain_support_details_t query_swapchain_support(VkPhysicalDevice physical_device);
    bool is_physical_device_suitable(VkPhysicalDevice physical_device);
    void pick_physical_device();

    void push_device_validation_layers();
    void create_logical_device();

    VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes);
    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities);
    void create_swapchain();

    void create_renderpass();

    void create_framebuffers();

    void create_command_pool();

    void allocate_commandbuffers();

    void create_sync_objects();

    void create_descriptor_pool();

    void recreate_swapchain_and_its_resources();

private:
    core::ref<core::window_t> _window;

    friend class buffer_builder_t;
    friend class pipeline_builder_t;
    const bool _raytracing;
    const bool _validation;
    VkPhysicalDeviceProperties _physical_device_properties{};

    std::vector<const char *> _instance_layers{};
    std::vector<const char *> _instance_extensions{};

    std::vector<const char *> _device_layers{};
    std::vector<const char *> _device_extensions{};

    VkInstance _instance{};
    VkDebugUtilsMessengerEXT _debug_utils_messenger{};
    VkSurfaceKHR _surface{};
    VkPhysicalDevice _physical_device{};
    VkDevice _device{};

    // maybe a separate queue class with its own submits ?
    // how would physical device selection work tho ?
    // might need to hint requirement of the following queues in advance to the context
    VkQueue _graphics_queue{};
    VkQueue _present_queue{};
    
    // swapchain stuff
    VkSwapchainKHR _swapchain{};
    VkFormat _swapchain_format{};
    VkExtent2D _swapchain_extent;
    std::vector<VkImage> _swapchain_images{};
    std::vector<VkImageView> _swapchain_image_views{}; // NOTE: maybe change this
    std::vector<VkFramebuffer> _swapchain_framebuffers{};
    VkRenderPass _swapchain_renderpass{}; // no really a part of swapchain, but still in it ?

    // command buffers, can stay
    VkCommandPool _command_pool{};
    std::vector<VkCommandBuffer> _commandbuffers{};

    // renderer part ?
    uint32_t _current_frame{};
    uint32_t _image_index{};
    std::vector<VkSemaphore> _image_available_semaphores{};
    std::vector<VkSemaphore> _render_finished_semaphores{};
    std::vector<VkFence> _in_flight_fences{};
    
    // maybe someway to simplify descriptor set creations from the pool
    VkDescriptorPool _descriptor_pool{}; // maybe create a seperate descriptor pool in the renderer ?

    std::vector<std::function<void()>> _resize_call_backs;

    std::unordered_map<sampler_create_info_t, VkSampler> _sampler_table;
};

} // namespace vulkan

} // namespace gfx

#endif