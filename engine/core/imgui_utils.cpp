#include "imgui_utils.hpp"

#include "core/log.hpp"
#include "gfx/vulkan/context.hpp"

#include <imgui_internal.h>

namespace core {

static void checkVkResult(VkResult error) {
    if (error == 0) return;
    ERROR("Vulkan Error: {}", error);
    if (error < 0) abort();
}

struct ImGuiBackendRendererData {
    ref<gfx::vulkan::context_t> context;

};

void ImGui_init(core::ref<core::window_t> window, core::ref<gfx::vulkan::context_t> context) {
    VIZON_PROFILE_FUNCTION();
    IMGUI_CHECKVERSION();
    
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    // io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
    // io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    // io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    io.ConfigFlags  |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags  |= ImGuiConfigFlags_ViewportsEnable;

    // TODO: add multi viewport 
    ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
    platformIO.Platform_CreateWindow = [](ImGuiViewport *viewport) {
        // ImGuiBackendRendererData

    };

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window->window(), true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = context->instance();
    initInfo.PhysicalDevice = context->physical_device();
    initInfo.Device = context->device();
    initInfo.QueueFamily = context->queue_family_indices().graphics_family.value();
    initInfo.Queue = context->graphics_queue();

    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = context->descriptor_pool();

    initInfo.Allocator = VK_NULL_HANDLE;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = context->swapchain_image_count();
    initInfo.CheckVkResultFn = checkVkResult;
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_LoadFunctions([](const char *function_name, void *vulkan_instance) {
        return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance *>(vulkan_instance)), function_name);
    }, &context->instance());

    ImGui_ImplVulkan_Init(&initInfo, context->swapchain_renderpass());
    
    context->single_use_commandbuffer([](VkCommandBuffer commandBuffer) {
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    });
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void ImGui_shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGui_newframe() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGui_endframe(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

} // namespace core
