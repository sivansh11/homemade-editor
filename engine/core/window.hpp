#ifndef CORE_WINDOW_HPP
#define CORE_WINDOW_HPP

#define GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_NONE
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <string>

namespace core {

class window_t {
public:
    window_t(const std::string& title, uint32_t width, uint32_t height);
    ~window_t();

    std::pair<int, int> get_dimensions();

    const std::string& get_title() const { return _title; }
    GLFWwindow *window() { return _window_p; }
    bool should_close() { return glfwWindowShouldClose(_window_p); }
    void poll_events() { return glfwPollEvents(); }

private:
    std::string _title{};
    GLFWwindow *_window_p{};
};

} // namespace core

#endif