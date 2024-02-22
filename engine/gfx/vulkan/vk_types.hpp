#ifndef GFX_VULKAN_VK_TYPES_HPP
#define GFX_VULKAN_VK_TYPES_HPP

#define VK_NO_PROTOTYPES
#include <volk.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace gfx {

namespace vulkan {

enum format_t : uint32_t {
    e_default,          // will be treated as none if no way to get format programatically

    e_r32ui,
    e_r32i,
    e_r32f,
    e_rgba8_unorm,
    e_rgba8_snorm,
    e_rgba8_uint,
    e_rgba8_srgb,
    
};
    
enum image_type_t : uint32_t {
    e_default,        // will be treated as none if no way to get image_type programatically
    
    e_1D,
    e_2D,
    e_3D,

};

enum image_view_type_t : uint32_t {
    e_default,        // will be treated as none if no way to get image_view_type programatically
    
    e_1D,
    e_2D,
    e_3D,

};

} // namespace vulkan  

} // namespace gfx

#endif