#ifndef CORE_IMGUI_UTILS_HPP
#define CORE_IMGUI_UTILS_HPP

#include "gfx/vulkan/context.hpp"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace core {

void ImGui_init(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context);
void ImGui_shutdown();
void ImGui_newframe();
void ImGui_endframe(VkCommandBuffer commandBuffer);

} // namespace core

#endif