#include "core/core.hpp"
#include "core/imgui_utils.hpp"

#include "renderer.hpp"
#include "app.hpp"

#include <GLFW/glfw3.h>

#include <chrono>

int main() {
    renderer::init("test", 1200, 800);

    app::app_t *app = app::app_t::create();

    float target_FPS = 1000.f;
    auto last_time = std::chrono::system_clock::now();
    while (!renderer::should_continue()) {
        auto current_time = std::chrono::system_clock::now();
        auto time_difference = current_time - last_time;
        if (time_difference.count() / 1e6 < 1000.f / target_FPS) {
            continue;
        }
        last_time = current_time;
        float dt = time_difference.count() / float(1e6);
        renderer::poll_events();
        if (glfwGetKey(renderer::get_window_ptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
        
        app->draw(dt);

        renderer::render();
        core::clear_frame_function_times();
    }

    app::app_t::destroy(app);
    renderer::destroy();

    return 0;
}