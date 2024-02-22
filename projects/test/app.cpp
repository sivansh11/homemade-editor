#include "app.hpp"

/*

I know the way I write code here is bad, I cant have multiple app instances, this is just for fast testing, I wont be doing this while creating a real application

*/

#include "renderer.hpp"

#include "core/imgui_utils.hpp"

namespace app {

using namespace renderer;

static surface_t screen;
static surface_t test_surf;
static font_t font;

app_t *app_t::create() {
    screen = get_screen_surface();
    test_surf = create_surface({100, 100});
    font = create_font(32.f, "../../assets/fonts/static/EBGaramond-Regular.ttf");

    return new app_t;
}    

void app_t::destroy(app_t *app) {
    delete app;
}      

void app_t::draw(float dt) {
    
    fill_surface(screen, {0, 0, 0, 1});
    fill_surface(test_surf, {1, 0, 0, 1});
    draw_text(test_surf, font, "hello", {1, 1, 1, 1}, {0, 0}, 32.f);
    draw_surface(screen, test_surf, rect_t{ .position = {0, 0}, .size = {100, 100}});
    draw_surface(screen, test_surf, rect_t{ .position = {0, 100}, .size = {50, 50}});
    draw_surface(screen, test_surf, rect_t{ .position = {0, 150}, .size = {200, 200}});

    imgui_draw_callback([dt]() {
        ImGui::Begin("temp");
        ImGui::Text("%f", dt);
        ImGui::End();
    });
}

} // namespace app
