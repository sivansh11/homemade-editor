#include "app.hpp"

/*

I know the way I write code here is bad, I cant have multiple app instances, this is just for fast testing, I wont be doing this while creating a real application

*/

#include "renderer.hpp"

#include "core/imgui_utils.hpp"
#include "ui.hpp"

#include <cmath>

namespace app {

using namespace renderer;

static surface_t screen;
static font_t font;

app_t *app_t::create() {
    font = create_font(64.f, "../../assets/fonts/static/EBGaramond-Regular.ttf");
    ui::init();
    return new app_t;
}    

void app_t::destroy(app_t *app) {
    ui::destroy();
    delete app;
}      

void app_t::draw(float dt) {
    screen = get_screen_surface();

    static float clock = 0;
    clock += dt / 1000.f;
    
    fill_surface(screen, {1, 0, 1, 1});

    draw_text(screen, font, "example text", {1, 1, 1, 1}, {0, 300}, 64.f + (std::sin(clock) * 48.f));

    ui::start_frame(renderer::get_window_ptr());

    ui::begin("test");
    ui::text("hi :D", 24);
    ui::text("ola :D", 24);
    ui::end();

    ui::end_frame(screen);

    imgui_draw_callback([dt]() {
        ImGui::Begin("temp");
        ImGui::Text("%f", dt);
        ImGui::End();
    });
}

} // namespace app
