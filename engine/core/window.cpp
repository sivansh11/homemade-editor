#include "window.hpp"

#include "core/log.hpp"

namespace core {

window_t::window_t(const std::string& title, uint32_t width, uint32_t height)
  : _title(title) {
    if (!glfwInit()) {
        ERROR("Failed to initialize GLFW");
        std::terminate();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _window_p = glfwCreateWindow(width, height, _title.c_str(), NULL, NULL);

    TRACE("Window {} created", _title);
}    

window_t::~window_t() {
    glfwTerminate();
    TRACE("Window {} destroyed", _title);
}

std::pair<int, int> window_t::get_dimensions() {
    int width, height;
    glfwGetWindowSize(_window_p, &width, &height);
    return { width, height };
}

} // namespace core
