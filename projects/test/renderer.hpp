#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "core/core.hpp"

#include <glm/glm.hpp>

#include <functional>
#include <filesystem>
#include <vector>
#include <string>
#include <stdint.h>

// forward declaration
struct GLFWwindow;

namespace gfx {

namespace vulkan {

class image_t;
class framebuffer_t;
class gpu_timer_t;
class descriptor_set_t;
class buffer_t;

} // namespace vulkan

} // namespace gfx

namespace msdf_atlas {

class GlyphGeometry;
class FontGeometry;

} // namespace msdf_atlas


namespace renderer {

// mostly for windows, might move it out of renderer
bool init(const std::string& title, uint32_t width, uint32_t height);
void destroy();
bool should_continue();
void poll_events();
// dont do cursed shit with this pls
GLFWwindow *get_window_ptr();

struct rect_t {
    glm::vec2 position;  
    glm::vec2 size;     

    // TODO: add contains methods
};

struct circle_t {
    union {
        glm::vec2 position;
        float x, y;
    };
    float radius;
};

struct surface_t {
    // uses the size of the surface 
    rect_t rect(const glm::vec2& position) const {
        return rect_t{ .position = position, .size = size() };
    }

    const glm::vec2& size() const;
    core::ref<gfx::vulkan::image_t> image() const;
    core::ref<gfx::vulkan::framebuffer_t> framebuffer() const;
    core::ref<gfx::vulkan::descriptor_set_t> projection_descriptor_set() const;
    core::ref<gfx::vulkan::descriptor_set_t> surface_image_descriptor_set() const;
    core::ref<gfx::vulkan::buffer_t> uniform_buffer() const;
    
    uint32_t _surface_id = 0;  // 0 is invalid
};

struct font_t {

    const std::vector<msdf_atlas::GlyphGeometry> *glyphs() const;
    const msdf_atlas::FontGeometry& geometry() const; 
    core::ref<gfx::vulkan::image_t> atlas() const;
    core::ref<gfx::vulkan::descriptor_set_t> descriptor_set() const;

    uint32_t _font_id;
    float _original_font_size;
    float _max_height;
};

surface_t create_surface(const glm::vec2& size);
surface_t get_screen_surface();

font_t create_font(float font_size, const std::filesystem::path& path);

// SECTION DRAW
void draw_rect(const surface_t& surface, const rect_t& rect, const glm::vec4& color);
void draw_circle(const surface_t& surface, const circle_t& circle, const glm::vec4& color);
void fill_surface(const surface_t& surface, const glm::vec4& color);
void draw_surface(const surface_t& surface, const surface_t& other_surface, const rect_t& rect);
glm::vec2 draw_text(const surface_t& surface, const font_t& font, const char *text, const glm::vec4& color, const glm::vec2& position, float font_size = 1.f);  // str should be /0 terminated
// void draw_text(const surface_t& surface, const font_t& font, const char *str, const glm::vec4& color, const glm::vec2& position, float scale = 1.f)

void imgui_draw_callback(std::function<void(void)> fn);

// SECTION RENDER
void render();

} // namespace renderer

#endif