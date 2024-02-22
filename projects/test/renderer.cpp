#include "renderer.hpp"

#include "core/log.hpp"
#include "core/window.hpp"

#include "gfx/vulkan/context.hpp"
#include "gfx/vulkan/pipeline.hpp"
#include "gfx/vulkan/descriptor.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/image.hpp"
#include "gfx/vulkan/renderpass.hpp"
#include "gfx/vulkan/framebuffer.hpp"
#include "gfx/vulkan/timer.hpp"

#include "core/imgui_utils.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

namespace renderer {

struct glyph_data_t {
    double width = 0;
    double height = 0;
    double bearing_x = 0;
    double bearing_y = 0;
    double advance_x = 0;
    double bearing_underline = 0;
    glm::vec4 atlas_bounds{};
    glm::vec4 plane_bounds{};
};

// forward decalare
void _start_renderpass(VkCommandBuffer commandbuffer, const surface_t& surface, const glm::vec4& color);
void _end_renderpass(VkCommandBuffer commandbuffer, const surface_t& surface);
glyph_data_t _get_data_from_glyph(const msdf_atlas::GlyphGeometry *glyph, float font_size);
rect_t _transform_coordinate_system(const surface_t& surface, const rect_t& rect);        

struct command_draw_rect_t {
    surface_t surface;
    rect_t rect;
    glm::vec4 color;
};

struct command_draw_circle_t {
    surface_t surface;
    circle_t circle;
    glm::vec4 color;
};

struct command_fill_surface_t {
    surface_t surface;
    glm::vec4 color;
};

struct command_draw_surface_t {
    surface_t surface;
    surface_t other_surface;
    rect_t rect;
};

struct command_draw_text_t {
    surface_t surface;
    font_t font;
    const char *text;
    glm::vec4 color;
    glm::vec2 position;
    float font_size;
};

enum class command_type_t {
    e_none,
    e_draw_rect,
    e_draw_circle,
    e_fill_surface,
    e_draw_surface,
    e_draw_text,
};

struct command_t {
    command_type_t command_type;
    union {
        command_draw_rect_t draw_rect;
        command_draw_circle_t draw_circle;
        command_fill_surface_t fill_surface;
        command_draw_surface_t draw_surface;
        command_draw_text_t draw_text;
    } command;
};

// maybe expose this ?
struct transform_2d_t {
    glm::vec3 position{ 0.f, 0.f, 0.f };
    glm::vec2 scale{ 1.f, 1.f };
    float angle{ 0.f };

    transform_2d_t() = default;

    glm::mat4 matrix() const {
        return glm::translate(glm::mat4{ 1.f }, position) 
            * glm::rotate(glm::mat4{ 1.f }, angle, glm::vec3{ 0.f, 0.f, 1.f })
            * glm::scale(glm::mat4{ 1.f }, glm::vec3{ scale, 1.f });
    }
};

struct renderer_data_t {
    bool _initialized = false;

    core::ref<core::window_t> _window;
    core::ref<gfx::vulkan::context_t> _gfx_context;

    core::ref<gfx::vulkan::renderpass_t> _renderpass; // no depth, only color

    core::ref<gfx::vulkan::descriptor_set_layout_t> _swapchain_descriptor_set_layout;
    core::ref<gfx::vulkan::descriptor_set_t> _swapchain_descriptor_set;
    core::ref<gfx::vulkan::descriptor_set_layout_t> _projection_descriptor_set_layout;
    core::ref<gfx::vulkan::descriptor_set_layout_t> _surface_image_descriptor_set_layout;
    core::ref<gfx::vulkan::descriptor_set_layout_t> _font_descriptor_set_layout;

    core::ref<gfx::vulkan::pipeline_t> _swapchain_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _rect_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _circle_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _surface_pipeline;
    core::ref<gfx::vulkan::pipeline_t> _text_pipeline;

    std::vector<glm::vec2> _surface_size_vector;
    std::vector<core::ref<gfx::vulkan::image_t>> _surface_image_vector;
    std::vector<core::ref<gfx::vulkan::framebuffer_t>> _surface_framebuffer_vector;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _surface_projection_descriptor_set_vector;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _surface_image_descriptor_set_vector;
    std::vector<core::ref<gfx::vulkan::buffer_t>> _surface_uniform_buffer_vector;

    std::vector<std::vector<msdf_atlas::GlyphGeometry> *> _font_glyphs;
    std::vector<msdf_atlas::FontGeometry> _font_geometries;
    std::vector<core::ref<gfx::vulkan::image_t>> _font_atlases;
    std::vector<core::ref<gfx::vulkan::descriptor_set_t>> _font_descriptor_sets;

    std::vector<std::function<void(void)>> _imgui_draw_callbacks;

    uint32_t _surface_counter = 0;
    uint32_t _font_counter = 0;

    uint32_t _surface_swaps = 0;
    uint32_t _pipeline_swaps = 0;

    std::vector<command_t> _commands;

    surface_t _screen_surface;

    surface_t _current_surface = { ._surface_id = 0 };
    core::ref<gfx::vulkan::pipeline_t> _current_pipeline{nullptr};
    bool _force_transition = false;
};

static renderer_data_t s_renderer_data{};

struct rect_push_constant_t {
    glm::mat4 model_matrix;
    glm::vec4 color;
};

static_assert(sizeof(rect_push_constant_t) <= 128);  // max push constant size 100% ensured by vulkan

struct circle_push_constant_t {
    glm::mat4 model_matrix;
    glm::vec4 color;
    float radius;
};

static_assert(sizeof(circle_push_constant_t) <= 128);  // max push constant size 100% ensured by vulkan

struct surface_push_constant_t {
    glm::mat4 model_matrix;
};

static_assert(sizeof(surface_push_constant_t) <= 128);  // max push constant size 100% ensured by vulkan

struct text_push_constant_t {
    glm::mat4 model_matrix;
    glm::vec4 color;
    glm::vec2 tex_coord_min;
    glm::vec2 tex_coord_max;
};

static_assert(sizeof(text_push_constant_t) <= 128);  // max push constant size 100% ensured by vulkan

struct uniform_buffer_t {
    glm::mat4 projection;
};

bool init(const std::string& title, uint32_t width, uint32_t height) {
    VIZON_PROFILE_FUNCTION();
    s_renderer_data._initialized = true;

    s_renderer_data._window = core::make_ref<core::window_t>(title, width, height);
    s_renderer_data._gfx_context = core::make_ref<gfx::vulkan::context_t>(s_renderer_data._window, 2, true);

    // renderpass
    s_renderer_data._renderpass = gfx::vulkan::renderpass_builder_t{}
        .add_color_attachment(VkAttachmentDescription{
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,            
        })
        .build(s_renderer_data._gfx_context);

    // pipeline and descriptors
    s_renderer_data._swapchain_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(s_renderer_data._gfx_context);

    s_renderer_data._projection_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT)
        .build(s_renderer_data._gfx_context);

    s_renderer_data._surface_image_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(s_renderer_data._gfx_context);
    
    s_renderer_data._font_descriptor_set_layout = gfx::vulkan::descriptor_set_layout_builder_t{}
        .addLayoutBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(s_renderer_data._gfx_context);

    // screen surface (need to put this here as swapchain descriptor set requires a valid image)
    s_renderer_data._screen_surface = create_surface(glm::vec2{ width, height });
    s_renderer_data._gfx_context->add_resize_callback([]() {
        auto [width, height] = s_renderer_data._window->get_dimensions();
        s_renderer_data._screen_surface = create_surface(glm::vec2{ width, height });  
    });
    
    // pipeline and descriptors
    s_renderer_data._swapchain_descriptor_set = s_renderer_data._swapchain_descriptor_set_layout->new_descriptor_set();
    s_renderer_data._swapchain_descriptor_set->write()
        .pushImageInfo(0, 1, get_screen_surface().image()->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    s_renderer_data._swapchain_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/swapchain/glsl.vert")
        .add_shader("../../assets/shaders/swapchain/glsl.frag")
        .add_descriptor_set_layout(s_renderer_data._swapchain_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(s_renderer_data._gfx_context, s_renderer_data._gfx_context->swapchain_renderpass());

    s_renderer_data._rect_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/rect/glsl.vert")
        .add_shader("../../assets/shaders/rect/glsl.frag")
        .add_push_constant_range(0, sizeof(rect_push_constant_t), VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_descriptor_set_layout(s_renderer_data._projection_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(s_renderer_data._gfx_context, s_renderer_data._renderpass->renderpass());

    s_renderer_data._circle_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/circle/glsl.vert")
        .add_shader("../../assets/shaders/circle/glsl.frag")
        .add_push_constant_range(0, sizeof(circle_push_constant_t), VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_descriptor_set_layout(s_renderer_data._projection_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(s_renderer_data._gfx_context, s_renderer_data._renderpass->renderpass());
    
    s_renderer_data._surface_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/surface/glsl.vert")
        .add_shader("../../assets/shaders/surface/glsl.frag")
        .add_push_constant_range(0, sizeof(surface_push_constant_t), VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_descriptor_set_layout(s_renderer_data._projection_descriptor_set_layout)
        .add_descriptor_set_layout(s_renderer_data._surface_image_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(s_renderer_data._gfx_context, s_renderer_data._renderpass->renderpass());
    
    s_renderer_data._text_pipeline = gfx::vulkan::pipeline_builder_t{}
        .add_shader("../../assets/shaders/text/glsl.vert")
        .add_shader("../../assets/shaders/text/glsl.frag")
        .add_push_constant_range(0, sizeof(text_push_constant_t), VK_SHADER_STAGE_ALL_GRAPHICS)
        .add_descriptor_set_layout(s_renderer_data._projection_descriptor_set_layout)
        .add_descriptor_set_layout(s_renderer_data._font_descriptor_set_layout)
        .add_default_color_blend_attachment_state()
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR)
        .add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .build(s_renderer_data._gfx_context, s_renderer_data._renderpass->renderpass());

    core::ImGui_init(s_renderer_data._window, s_renderer_data._gfx_context);

    return true;
}

void destroy() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    s_renderer_data._gfx_context->wait_idle();
    for (auto glyph : s_renderer_data._font_glyphs) {
        delete glyph;
    }
    core::ImGui_shutdown();
    s_renderer_data = {};
}

bool should_continue() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    return s_renderer_data._window->should_close();
}

void poll_events() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    s_renderer_data._window->poll_events();
}

GLFWwindow *get_window_ptr() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    return s_renderer_data._window->window();
}

const glm::vec2& surface_t::size() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_size_vector.size() >= _surface_id);
    return s_renderer_data._surface_size_vector[_surface_id - 1];
}

core::ref<gfx::vulkan::image_t> surface_t::image() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_image_vector.size() >= _surface_id);
    return s_renderer_data._surface_image_vector[_surface_id - 1];
}

core::ref<gfx::vulkan::framebuffer_t> surface_t::framebuffer() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_framebuffer_vector.size() >= _surface_id);
    return s_renderer_data._surface_framebuffer_vector[_surface_id - 1];
}

core::ref<gfx::vulkan::descriptor_set_t> surface_t::projection_descriptor_set() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_projection_descriptor_set_vector.size() >= _surface_id);
    return s_renderer_data._surface_projection_descriptor_set_vector[_surface_id - 1];
}


core::ref<gfx::vulkan::descriptor_set_t> surface_t::surface_image_descriptor_set() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_image_descriptor_set_vector.size() >= _surface_id);
    return s_renderer_data._surface_image_descriptor_set_vector[_surface_id - 1];
}

core::ref<gfx::vulkan::buffer_t> surface_t::uniform_buffer() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._surface_uniform_buffer_vector.size() >= _surface_id);
    return s_renderer_data._surface_uniform_buffer_vector[_surface_id - 1];
}

const std::vector<msdf_atlas::GlyphGeometry> *font_t::glyphs() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._font_glyphs.size() >= _font_id);
    return s_renderer_data._font_glyphs[_font_id - 1];
}

const msdf_atlas::FontGeometry& font_t::geometry() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._font_geometries.size() >= _font_id);
    return s_renderer_data._font_geometries[_font_id - 1];
}

core::ref<gfx::vulkan::image_t> font_t::atlas() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._font_atlases.size() >= _font_id);
    return s_renderer_data._font_atlases[_font_id - 1];
}

core::ref<gfx::vulkan::descriptor_set_t> font_t::descriptor_set() const {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    assert(s_renderer_data._font_descriptor_sets.size() >= _font_id);
    return s_renderer_data._font_descriptor_sets[_font_id - 1];
}

surface_t create_surface(const glm::vec2& size) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    core::ref<gfx::vulkan::image_t> image = gfx::vulkan::image_builder_t{}
        .build2D(s_renderer_data._gfx_context, size.x, size.y, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    core::ref<gfx::vulkan::framebuffer_t> framebuffer = gfx::vulkan::framebuffer_builder_t{}
        .add_attachment_view(image->image_view())
        .build(s_renderer_data._gfx_context, s_renderer_data._renderpass->renderpass(), size.x, size.y);
    core::ref<gfx::vulkan::gpu_timer_t> gpu_timer = core::make_ref<gfx::vulkan::gpu_timer_t>(s_renderer_data._gfx_context);
    core::ref<gfx::vulkan::descriptor_set_t> projection_descriptor_set = s_renderer_data._projection_descriptor_set_layout->new_descriptor_set();
    core::ref<gfx::vulkan::descriptor_set_t> surface_image_descriptor_set = s_renderer_data._surface_image_descriptor_set_layout->new_descriptor_set();
    core::ref<gfx::vulkan::buffer_t> uniform_buffer = gfx::vulkan::buffer_builder_t{}
        .build(s_renderer_data._gfx_context, sizeof(uniform_buffer_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    core::ref<gfx::vulkan::buffer_t> staging_buffer = gfx::vulkan::buffer_builder_t{}
        .build(s_renderer_data._gfx_context, sizeof(uniform_buffer_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    uniform_buffer_t *_uniform_buffer = (uniform_buffer_t *)staging_buffer->map();
    _uniform_buffer->projection = glm::ortho(-float(size.x) / 2.f, float(size.x) / 2.f, -float(size.y) / 2.f, float(size.y) / 2.f);
    gfx::vulkan::buffer_t::copy(s_renderer_data._gfx_context, *staging_buffer, *uniform_buffer, VkBufferCopy{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = sizeof(uniform_buffer_t),
    });
    
    projection_descriptor_set->write()
        .pushBufferInfo(0, 1, uniform_buffer->descriptor_info())
        .update();
    
    surface_image_descriptor_set->write()
        .pushImageInfo(0, 1, image->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    s_renderer_data._surface_size_vector.push_back(size);
    s_renderer_data._surface_image_vector.push_back(image);
    s_renderer_data._surface_framebuffer_vector.push_back(framebuffer);
    s_renderer_data._surface_projection_descriptor_set_vector.push_back(projection_descriptor_set);
    s_renderer_data._surface_image_descriptor_set_vector.push_back(surface_image_descriptor_set);
    s_renderer_data._surface_uniform_buffer_vector.push_back(uniform_buffer);

    surface_t surface{};
    surface._surface_id = ++s_renderer_data._surface_counter;

    s_renderer_data._gfx_context->single_use_commandbuffer([&](VkCommandBuffer commandbuffer) {
        image->transition_layout(commandbuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // _start_renderpass(commandbuffer, surface, glm::vec4{0, 0, 0, 0});
        // _end_renderpass(commandbuffer, surface);
    });
    draw_rect(surface, surface.rect({0, 0}), {0, 0, 0, 0});

    return surface;
}

surface_t get_screen_surface() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    return s_renderer_data._screen_surface;
}

font_t create_font(float font_size, const std::filesystem::path& path) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);

    std::string path_string = path.string();

    msdfgen::FreetypeHandle *freetype_handle;
    msdfgen::FontHandle *font_handle;

    freetype_handle = msdfgen::initializeFreetype();
    if (!freetype_handle)
        throw std::runtime_error("Failed to initialize free type!");

    font_handle = msdfgen::loadFont(freetype_handle, path_string.c_str());
    if (!font_handle)
        throw std::runtime_error("Failed to load font!");

    std::vector<msdf_atlas::GlyphGeometry> *glyphs = new std::vector<msdf_atlas::GlyphGeometry>;
    msdf_atlas::FontGeometry geometry(glyphs);
    core::ref<gfx::vulkan::image_t> atlas;
    
    geometry.loadCharset(font_handle, 1.0, msdf_atlas::Charset::ASCII);

    const unsigned long long LCG_MULTIPLIER = 6364136223846793005ull;
    const unsigned long long LCG_INCREMENT = 1442695040888963407ull;
    unsigned long long glyphSeed = 0;
    const double maxCornerAngle = 3.0;
    uint64_t coloringSeed = 0;
    bool expensiveColoring = false;

    if (false) {
        throw std::runtime_error("??");
    } else {
        for (auto& glyph : *glyphs) {
            glyphSeed *= LCG_MULTIPLIER;
            glyph.edgeColoring(msdfgen::edgeColoringInkTrap, maxCornerAngle, glyphSeed);
        }
    }

    msdf_atlas::TightAtlasPacker packer;
    packer.setDimensionsConstraint(msdf_atlas::TightAtlasPacker::DimensionsConstraint::SQUARE);
    packer.setScale(font_size);
    packer.setPixelRange(2.0);
    packer.setMiterLimit(1.0);
    int remaining = packer.pack(glyphs->data(), glyphs->size());
    assert(remaining == 0);

    int width = 0, height = 0;
    packer.getDimensions(width, height);
    
    msdf_atlas::ImmediateAtlasGenerator<float, 4, &msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<uint8_t, 4>> generator(width, height);

    msdf_atlas::GeneratorAttributes attributes;
    attributes.config.overlapSupport = true;
    attributes.scanlinePass = true;

    generator.setAttributes(attributes);
    generator.setThreadCount(4);
    generator.generate(glyphs->data(), glyphs->size());

    msdfgen::BitmapConstRef<uint8_t, 4> bitmap = static_cast<msdfgen::BitmapConstRef<uint8_t, 4>>(generator.atlasStorage());

    auto staging_buffer = gfx::vulkan::buffer_builder_t{}   
        .build(s_renderer_data._gfx_context, 4 * bitmap.width * bitmap.height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(staging_buffer->map(), bitmap.pixels, bitmap.width * bitmap.height * 4);
    atlas = gfx::vulkan::image_builder_t{}
        .build2D(s_renderer_data._gfx_context, bitmap.width, bitmap.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    atlas->transition_layout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    gfx::vulkan::image_t::copy_buffer_to_image(s_renderer_data._gfx_context, *staging_buffer, *atlas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkBufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {static_cast<uint32_t>(bitmap.width), static_cast<uint32_t>(bitmap.height), 1}
        });
    atlas->gen_mip_maps(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    msdfgen::destroyFont(font_handle);
    msdfgen::deinitializeFreetype(freetype_handle);

    core::ref<gfx::vulkan::descriptor_set_t> descriptor_set = s_renderer_data._font_descriptor_set_layout->new_descriptor_set();
    descriptor_set->write()
        .pushImageInfo(0, 1, atlas->descriptor_info(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
        .update();

    s_renderer_data._font_geometries.push_back(geometry);
    s_renderer_data._font_glyphs.push_back(glyphs);
    s_renderer_data._font_atlases.push_back(atlas);
    s_renderer_data._font_descriptor_sets.push_back(descriptor_set);

    float max_height = 0;

    for (auto& glyph : *glyphs) {
        glyph_data_t data = _get_data_from_glyph(&glyph, font_size);
        max_height = std::max(max_height, float(data.height + data.bearing_underline));
    }

    font_t font{};
    font._font_id = ++s_renderer_data._font_counter;
    font._original_font_size = font_size;
    font._max_height = max_height;
    return font;
}

// SECTION DRAW
void draw_rect(const surface_t& surface, const rect_t& rect, const glm::vec4& color) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    command_t command{ .command_type = command_type_t::e_draw_rect };
    command_draw_rect_t draw_rect;
    draw_rect.surface = surface;
    draw_rect.rect = rect;
    draw_rect.color = color;
    command.command.draw_rect = draw_rect;
    s_renderer_data._commands.push_back(command);
}

void draw_circle(const surface_t& surface, const circle_t& circle, const glm::vec4& color) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    command_t command{ .command_type = command_type_t::e_draw_circle };
    command_draw_circle_t draw_circle;
    draw_circle.surface = surface;
    draw_circle.circle = circle;
    draw_circle.color = color;
    command.command.draw_circle = draw_circle;
    s_renderer_data._commands.push_back(command);
}

void fill_surface(const surface_t& surface, const glm::vec4& color) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    command_t command{ .command_type = command_type_t::e_fill_surface };
    command_fill_surface_t fill_surface;
    fill_surface.surface = surface;
    fill_surface.color = color;
    command.command.fill_surface = fill_surface;
    s_renderer_data._commands.push_back(command);
}

void draw_surface(const surface_t& surface, const surface_t& other_surface, const rect_t& rect) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    command_t command{ .command_type = command_type_t::e_draw_surface };
    command_draw_surface_t draw_surface;
    draw_surface.surface = surface;
    draw_surface.other_surface = other_surface;
    draw_surface.rect = rect;
    command.command.draw_surface = draw_surface;
    s_renderer_data._commands.push_back(command);
}

glm::vec2 draw_text(const surface_t& surface, const font_t& font, const char *text, const glm::vec4& color, const glm::vec2& position, float font_size) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    command_t command{ .command_type = command_type_t::e_draw_text };
    command_draw_text_t draw_text;
    draw_text.surface = surface;
    draw_text.font = font;
    draw_text.text = text;
    draw_text.color = color;
    draw_text.position = position;
    draw_text.font_size = font_size;
    command.command.draw_text = draw_text;
    s_renderer_data._commands.push_back(command);

    float target_font_size = draw_text.font_size;
    double x_pos = draw_text.position.x;
    double y_pos = draw_text.position.y;
    double scale = target_font_size / draw_text.font._original_font_size;   

    size_t index = 0;
    while (draw_text.text[index]) {
        auto glyph = draw_text.font.geometry().getGlyph(draw_text.text[index]);
        assert(glyph);
        auto glyph_data = _get_data_from_glyph(glyph, draw_text.font._original_font_size);

        double x0 = x_pos + glyph_data.plane_bounds.x * target_font_size;
        double y0 = y_pos - glyph_data.plane_bounds.y * target_font_size;

        auto [atlas_width, atlas_height] = draw_text.font.atlas()->dimensions();

        double s0 = glyph_data.atlas_bounds.x / float(atlas_width);
        double t0 = glyph_data.atlas_bounds.w / float(atlas_height);
        double s1 = glyph_data.atlas_bounds.z / float(atlas_width);
        double t1 = glyph_data.atlas_bounds.y / float(atlas_height);

        rect_t rect{};
        glm::vec2 pos = glm::vec2{ x0, y0 + std::round(draw_text.font._max_height * scale) - (glyph_data.height * scale)};
        rect.position = pos;
        rect.size = { glyph_data.width * scale, glyph_data.height * scale };
        rect = _transform_coordinate_system(surface, rect);
        text_push_constant_t push{};
        transform_2d_t transform{};
        transform.position = { rect.position, 0 };
        transform.scale = rect.size;
        push.model_matrix = transform.matrix();
        push.color = draw_text.color;

        float texel_width = 1.0f / float(atlas_width);
        float texel_height = 1.0f / float(atlas_height);
        glm::vec2 texel_dims(texel_width, texel_height);

        push.tex_coord_min = glm::vec2{ glyph_data.atlas_bounds.x, glyph_data.atlas_bounds.y } * texel_dims;
        push.tex_coord_max = glm::vec2{ glyph_data.atlas_bounds.z, glyph_data.atlas_bounds.w } * texel_dims;

        double advance = glyph_data.advance_x * target_font_size;
        x_pos += advance;
        index++;
    }

    return {x_pos, y_pos};
}

void imgui_draw_callback(std::function<void(void)> fn) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    s_renderer_data._imgui_draw_callbacks.push_back(fn);
}

// SECTION RENDER

// internal
void _start_renderpass(VkCommandBuffer commandbuffer, const surface_t& surface, const glm::vec4& color) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    glm::vec2 size = surface.size();

    VkClearValue clear_color{};
    clear_color.color = {color.x, color.y, color.z, color.w};  
    
    s_renderer_data._renderpass->begin(commandbuffer, surface.framebuffer()->framebuffer(), VkRect2D{
        .offset = {0, 0},
        .extent = { static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y) },
    }, {
        clear_color,
    }); 
}

void _end_renderpass(VkCommandBuffer commandbuffer, const surface_t& surface) {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    s_renderer_data._renderpass->end(commandbuffer);
}

void surface_swaps(VkCommandBuffer commandbuffer, const surface_t& surface) {
    if (surface._surface_id == s_renderer_data._screen_surface._surface_id) s_renderer_data._force_transition = false;
    if (s_renderer_data._current_surface._surface_id != surface._surface_id) {
        if (s_renderer_data._current_surface._surface_id != 0) 
            _end_renderpass(commandbuffer, s_renderer_data._current_surface);
        _start_renderpass(commandbuffer, surface, glm::vec4{0, 0, 0, 0});
        s_renderer_data._current_surface = surface;
        s_renderer_data._surface_swaps++;
    }
}

void pipeline_swaps(VkCommandBuffer commandbuffer, core::ref<gfx::vulkan::pipeline_t> pipeline) {
    if (s_renderer_data._current_pipeline != pipeline) {
        pipeline->bind(commandbuffer);
        s_renderer_data._current_pipeline = pipeline;
        s_renderer_data._pipeline_swaps++;
    }
}

glyph_data_t _get_data_from_glyph(const msdf_atlas::GlyphGeometry *glyph, float font_size) {
    assert(glyph);
    // TODO: add fall back glyph
    double p_left, p_bottom, p_right, p_top;
    double a_left, a_bottom, a_right, a_top;
    glyph->getQuadPlaneBounds(p_left, p_bottom, p_right, p_top);
    glyph->getQuadAtlasBounds(a_left, a_bottom, a_right, a_top);

    glyph_data_t glyph_data;
    glyph_data.atlas_bounds = { a_left, a_bottom, a_right, a_top };
    glyph_data.plane_bounds = { p_left, p_bottom, p_right, p_top };
    glyph_data.advance_x = glyph->getAdvance();
    glyph_data.width = a_right - a_left;
    glyph_data.height = a_top - a_bottom;
    glyph_data.bearing_underline = p_bottom * font_size;
    return glyph_data;
}

rect_t _transform_coordinate_system(const surface_t& surface, const rect_t& rect) {
    // convert from top left position and full size rect to center position and half size rect
    rect_t new_rect = rect;
    new_rect.size /= 2.f;
    const glm::vec2& surface_size = surface.size() / 2.f;
    new_rect.position.x -= surface_size.x - new_rect.size.x;
    new_rect.position.y  = (surface_size.y - new_rect.size.y) - new_rect.position.y;
    return new_rect;
} 

void _command_draw_rect(VkCommandBuffer commandbuffer, const command_draw_rect_t& draw_rect) {
    const surface_t& surface = draw_rect.surface;
    surface_swaps(commandbuffer, surface);
    pipeline_swaps(commandbuffer, s_renderer_data._rect_pipeline);
    assert(s_renderer_data._current_pipeline == s_renderer_data._rect_pipeline);
    glm::vec2 surface_size = s_renderer_data._current_surface.size();
    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = static_cast<uint32_t>(surface_size.x);
    swapchain_viewport.height = static_cast<uint32_t>(surface_size.y);
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = { static_cast<uint32_t>(surface_size.x), static_cast<uint32_t>(surface_size.y) };    
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._rect_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._current_surface.projection_descriptor_set()->descriptor_set(), 0, nullptr);
    rect_push_constant_t push{};
    transform_2d_t transform{};
    rect_t rect = _transform_coordinate_system(surface, draw_rect.rect);
    transform.position = glm::vec3{ rect.position, 0 };
    transform.scale = rect.size;
    push.model_matrix = transform.matrix();
    push.color = draw_rect.color;
    vkCmdPushConstants(commandbuffer, s_renderer_data._rect_pipeline->pipeline_layout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(rect_push_constant_t), &push);
    vkCmdDraw(commandbuffer, 6, 1, 0, 0);
}

void _command_draw_circle(VkCommandBuffer commandbuffer, const command_draw_circle_t& draw_circle) {
    const surface_t& surface = draw_circle.surface;
    surface_swaps(commandbuffer, surface);
    pipeline_swaps(commandbuffer, s_renderer_data._circle_pipeline);
    assert(s_renderer_data._current_pipeline == s_renderer_data._circle_pipeline);
    glm::vec2 surface_size = s_renderer_data._current_surface.size();
    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = static_cast<uint32_t>(surface_size.x);
    swapchain_viewport.height = static_cast<uint32_t>(surface_size.y);
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = { static_cast<uint32_t>(surface_size.x), static_cast<uint32_t>(surface_size.y) };    
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._circle_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._current_surface.projection_descriptor_set()->descriptor_set(), 0, nullptr);
    circle_push_constant_t push{};
    transform_2d_t transform{};
    // TODO: fix circles
    transform.position = glm::vec3{ draw_circle.circle.position, 0 };
    transform.scale = glm::vec2{ draw_circle.circle.radius };
    push.model_matrix = transform.matrix();
    push.color = draw_circle.color;
    vkCmdPushConstants(commandbuffer, s_renderer_data._circle_pipeline->pipeline_layout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(circle_push_constant_t), &push);
    vkCmdDraw(commandbuffer, 6, 1, 0, 0);
}

void _command_fill_surface(VkCommandBuffer commandbuffer, const command_fill_surface_t& fill_surface) {
    command_draw_rect_t draw_rect{};
    draw_rect.surface = fill_surface.surface;
    draw_rect.color = fill_surface.color;
    draw_rect.rect.position = glm::vec2{ 0, 0 };
    glm::vec2 size = draw_rect.surface.size();
    draw_rect.rect.size = size;
    _command_draw_rect(commandbuffer, draw_rect);
}

void _command_draw_surface(VkCommandBuffer commandbuffer, const command_draw_surface_t& draw_surface) {
    const surface_t& surface = draw_surface.surface;
    surface_swaps(commandbuffer, surface);
    pipeline_swaps(commandbuffer, s_renderer_data._surface_pipeline);
    assert(s_renderer_data._current_pipeline == s_renderer_data._surface_pipeline);
    glm::vec2 surface_size = s_renderer_data._current_surface.size();
    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = static_cast<uint32_t>(surface_size.x);
    swapchain_viewport.height = static_cast<uint32_t>(surface_size.y);
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = { static_cast<uint32_t>(surface_size.x), static_cast<uint32_t>(surface_size.y) };    
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._surface_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._current_surface.projection_descriptor_set()->descriptor_set(), 0, nullptr);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._surface_pipeline->pipeline_layout(), 1, 1, &draw_surface.other_surface.surface_image_descriptor_set()->descriptor_set(), 0, nullptr);
    surface_push_constant_t push{};
    transform_2d_t transform{};
    rect_t rect = _transform_coordinate_system(surface, draw_surface.rect);
    transform.position = glm::vec3{ rect.position, 0 };
    transform.scale = rect.size;
    push.model_matrix = transform.matrix();
    vkCmdPushConstants(commandbuffer, s_renderer_data._surface_pipeline->pipeline_layout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(surface_push_constant_t), &push);
    vkCmdDraw(commandbuffer, 6, 1, 0, 0);
}

void _command_draw_text(VkCommandBuffer commandbuffer, const command_draw_text_t& draw_text) {
    const surface_t& surface = draw_text.surface;
    surface_swaps(commandbuffer, surface);
    pipeline_swaps(commandbuffer, s_renderer_data._text_pipeline);
    assert(s_renderer_data._current_pipeline == s_renderer_data._text_pipeline);
    glm::vec2 surface_size = s_renderer_data._current_surface.size();
    VkViewport swapchain_viewport{};
    swapchain_viewport.x = 0;
    swapchain_viewport.y = 0;
    swapchain_viewport.width = static_cast<uint32_t>(surface_size.x);
    swapchain_viewport.height = static_cast<uint32_t>(surface_size.y);
    swapchain_viewport.minDepth = 0;
    swapchain_viewport.maxDepth = 1;
    VkRect2D swapchain_scissor{};
    swapchain_scissor.offset = {0, 0};
    swapchain_scissor.extent = { static_cast<uint32_t>(surface_size.x), static_cast<uint32_t>(surface_size.y) };  
    vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
    vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._text_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._current_surface.projection_descriptor_set()->descriptor_set(), 0, nullptr);
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._text_pipeline->pipeline_layout(), 1, 1, &draw_text.font.descriptor_set()->descriptor_set(), 0, nullptr);
    float target_font_size = draw_text.font_size;
    double x_pos = draw_text.position.x;
    double y_pos = draw_text.position.y;
    double scale = target_font_size / draw_text.font._original_font_size;   

    size_t index = 0;
    while (draw_text.text[index]) {
        auto glyph = draw_text.font.geometry().getGlyph(draw_text.text[index]);
        assert(glyph);
        auto glyph_data = _get_data_from_glyph(glyph, draw_text.font._original_font_size);

        double x0 = x_pos + glyph_data.plane_bounds.x * target_font_size;
        double y0 = y_pos - glyph_data.plane_bounds.y * target_font_size;

        auto [atlas_width, atlas_height] = draw_text.font.atlas()->dimensions();

        double s0 = glyph_data.atlas_bounds.x / float(atlas_width);
        double t0 = glyph_data.atlas_bounds.w / float(atlas_height);
        double s1 = glyph_data.atlas_bounds.z / float(atlas_width);
        double t1 = glyph_data.atlas_bounds.y / float(atlas_height);

        rect_t rect{};
        glm::vec2 pos = glm::vec2{ x0, y0 + std::round(draw_text.font._max_height * scale) - (glyph_data.height * scale)};
        rect.position = pos;
        rect.size = { glyph_data.width * scale, glyph_data.height * scale };
        rect = _transform_coordinate_system(surface, rect);
        text_push_constant_t push{};
        transform_2d_t transform{};
        transform.position = { rect.position, 0 };
        transform.scale = rect.size;
        push.model_matrix = transform.matrix();
        push.color = draw_text.color;

        float texel_width = 1.0f / float(atlas_width);
        float texel_height = 1.0f / float(atlas_height);
        glm::vec2 texel_dims(texel_width, texel_height);

        push.tex_coord_min = glm::vec2{ glyph_data.atlas_bounds.x, glyph_data.atlas_bounds.y } * texel_dims;
        push.tex_coord_max = glm::vec2{ glyph_data.atlas_bounds.z, glyph_data.atlas_bounds.w } * texel_dims;

        vkCmdPushConstants(commandbuffer, s_renderer_data._text_pipeline->pipeline_layout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(text_push_constant_t), &push);
        vkCmdDraw(commandbuffer, 6, 1, 0, 0);
        double advance = glyph_data.advance_x * target_font_size;
        x_pos += advance;
        index++;
    }
}

void render() {
    VIZON_PROFILE_FUNCTION();
    assert(s_renderer_data._initialized);
    if (auto start_frame = s_renderer_data._gfx_context->start_frame()) {
        auto [commandbuffer, current_index] = *start_frame;
        // VkClearValue clear_color{};
        // clear_color.color = {0, 0, 0, 0};  

        // rendering
        s_renderer_data._current_surface = { 0 };
        s_renderer_data._current_pipeline = nullptr;
        s_renderer_data._force_transition = true;

        for (auto command : s_renderer_data._commands) {
            switch (command.command_type) {
                case command_type_t::e_draw_rect: 
                    _command_draw_rect(commandbuffer, command.command.draw_rect);
                    break;

                case command_type_t::e_draw_circle: 
                    _command_draw_circle(commandbuffer, command.command.draw_circle);
                    break;
                
                case command_type_t::e_fill_surface: 
                    _command_fill_surface(commandbuffer, command.command.fill_surface);
                    break;
                
                case command_type_t::e_draw_surface: 
                    _command_draw_surface(commandbuffer, command.command.draw_surface);
                    break;
                
                case command_type_t::e_draw_text: 
                    _command_draw_text(commandbuffer, command.command.draw_text);
                    break;

                default:
                    throw std::runtime_error("command type not recognised");
            }
        }
        if (s_renderer_data._force_transition) {
            if (s_renderer_data._current_surface._surface_id != 0) _end_renderpass(commandbuffer, s_renderer_data._current_surface);
            _start_renderpass(commandbuffer, s_renderer_data._screen_surface, glm::vec4{0, 0, 0, 0});
            _end_renderpass(commandbuffer, s_renderer_data._screen_surface);    
        }
        else 
            _end_renderpass(commandbuffer, s_renderer_data._current_surface);

        // composite to screen (is composite even the correct word here ?)
        VkClearValue clear_color{};
        clear_color.color = {0, 0, 0, 0};    
        VkClearValue clear_depth{};
        clear_depth.depthStencil.depth = 1;

        VkViewport swapchain_viewport{};
        swapchain_viewport.x = 0;
        swapchain_viewport.y = 0;
        swapchain_viewport.width = s_renderer_data._gfx_context->swapchain_extent().width;
        swapchain_viewport.height = s_renderer_data._gfx_context->swapchain_extent().height;
        swapchain_viewport.minDepth = 0;
        swapchain_viewport.maxDepth = 1;
        VkRect2D swapchain_scissor{};
        swapchain_scissor.offset = {0, 0};
        swapchain_scissor.extent = s_renderer_data._gfx_context->swapchain_extent();

        s_renderer_data._gfx_context->begin_swapchain_renderpass(commandbuffer, clear_color);

        vkCmdSetViewport(commandbuffer, 0, 1, &swapchain_viewport);
        vkCmdSetScissor(commandbuffer, 0, 1, &swapchain_scissor);

        vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_renderer_data._swapchain_pipeline->pipeline_layout(), 0, 1, &s_renderer_data._swapchain_descriptor_set->descriptor_set(), 0, nullptr);

        s_renderer_data._swapchain_pipeline->bind(commandbuffer);

        vkCmdDraw(commandbuffer, 6, 1, 0, 0);

        core::ImGui_newframe();

        // imgui gui
        ImGui::Begin("renderer info");
        ImGui::Text("commands issued: %lu", s_renderer_data._commands.size());
        ImGui::Text("surface swaps: %u", s_renderer_data._surface_swaps);
        ImGui::Text("pipeline swaps: %u", s_renderer_data._pipeline_swaps);
        ImGui::End();

        for (auto& fn : s_renderer_data._imgui_draw_callbacks) {
            fn();
        }

        core::ImGui_endframe(commandbuffer);
        s_renderer_data._gfx_context->end_swapchain_renderpass(commandbuffer);
        
        s_renderer_data._gfx_context->end_frame(commandbuffer);

        s_renderer_data._commands.clear();
        s_renderer_data._imgui_draw_callbacks.clear();
        s_renderer_data._surface_swaps = 0;
        s_renderer_data._pipeline_swaps = 0;
    }
}

} // namespace renderer