#include "context.hpp"

#include "core/core.hpp"
#include "core/log.hpp"

#include <set>
#include <string>

namespace gfx {

namespace vulkan {

static bool isVolkInitialized = false;

static void initialize_volk() {
    VIZON_PROFILE_FUNCTION();
    if (volkInitialize() != VK_SUCCESS) {
        ERROR("Failed to initialize Volk");
        std::terminate();
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data) {
    VIZON_PROFILE_FUNCTION();
    switch (message_severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 
            TRACE("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: 
            INFO("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: 
            WARN("{}", p_callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: 
            ERROR("{}", p_callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

context_t::context_t(std::shared_ptr<core::window_t> window, uint32_t MAX_FRAMES_IN_FLIGHT, bool validation, bool enable_raytracing)
  : MAX_FRAMES_IN_FLIGHT(MAX_FRAMES_IN_FLIGHT),
    _window(window),
    _validation(validation),
    _raytracing(enable_raytracing) {
    VIZON_PROFILE_FUNCTION();
    if (!isVolkInitialized) {
        isVolkInitialized = true;
        initialize_volk();
    }

    if (_validation) {
        if (!check_instance_validation_layer_support()) {
            ERROR("Validation layers requested but not supported");
            std::terminate();
        }
        push_instance_validation_layers();
        push_instance_debug_utils_messenger_extension();
    }
    push_required_instance_extensions();

    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = window->get_title().c_str();
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "Horizon";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &application_info;
    
    instance_create_info.enabledExtensionCount = _instance_extensions.size();
    instance_create_info.ppEnabledExtensionNames = _instance_extensions.data();

    instance_create_info.enabledLayerCount = _instance_layers.size();
    instance_create_info.ppEnabledLayerNames = _instance_layers.data();

    auto debug_utils_messenger_create_info = get_debug_utils_messenger_create_info();
    if (_validation) {
        instance_create_info.pNext = &debug_utils_messenger_create_info;
    }
    
    auto result = vkCreateInstance(&instance_create_info, nullptr, &_instance);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create instance!");
        std::terminate();
    }

    volkLoadInstance(_instance);

    TRACE("Created vulkan instance");

    if (_validation) {
        setup_debug_messenger();
    }
    create_surface();
    pick_physical_device();
    create_logical_device();
    create_swapchain();
    create_renderpass();
    create_framebuffers();
    create_command_pool();
    allocate_commandbuffers();
    create_sync_objects();
    create_descriptor_pool();

    INFO("Created Context");
}

context_t::~context_t() {
    vkDeviceWaitIdle(_device);
    for (auto& [sampler_info, sampler] : _sampler_table) {
        vkDestroySampler(_device, sampler, nullptr);
    }
    vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_device, _image_available_semaphores[i], nullptr);
        vkDestroySemaphore(_device, _render_finished_semaphores[i], nullptr);
        vkDestroyFence(_device, _in_flight_fences[i], nullptr);
    }
    vkDestroyCommandPool(_device, _command_pool, nullptr);
    for (auto framebuffer : _swapchain_framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(_device, _swapchain_renderpass, nullptr);
    for (auto imageView : _swapchain_image_views) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_device, nullptr);
    if (_validation) {
        vkDestroyDebugUtilsMessengerEXT(_instance, _debug_utils_messenger, nullptr);
    }
    vkDestroyInstance(_instance, nullptr);
    TRACE("Destroyed Context");
}

std::optional<std::pair<VkCommandBuffer, uint32_t>> context_t::start_frame() {
    VIZON_PROFILE_FUNCTION();
    vkWaitForFences(_device, 1, &_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);
    
    auto result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _image_available_semaphores[_current_frame], VK_NULL_HANDLE, &_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain_and_its_resources();
        return std::nullopt;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ERROR("Failed to acquire swap chain image");
        std::terminate();
    }
    vkResetFences(_device, 1, &_in_flight_fences[_current_frame]);
    vkResetCommandBuffer(_commandbuffers[_current_frame], 0);
    VkCommandBufferBeginInfo commandbuffer_begin_info{};
	commandbuffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(_commandbuffers[_current_frame], &commandbuffer_begin_info) != VK_SUCCESS) {
        ERROR("Failed to being command buffer");
        std::terminate();
    }
    return std::pair<VkCommandBuffer, uint32_t>{ _commandbuffers[_current_frame], _current_frame };
}

bool context_t::end_frame(VkCommandBuffer commandbuffer) {
    VIZON_PROFILE_FUNCTION();
    if (vkEndCommandBuffer(commandbuffer) != VK_SUCCESS) {
        ERROR("Failed to record command buffer");
        std::terminate();
    }

    VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	VkSemaphore wait_semaphores[] = { _image_available_semaphores[_current_frame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = waitStages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &commandbuffer;
	VkSemaphore signal_semaphores[] = { _render_finished_semaphores[_current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;
	  
	if (vkQueueSubmit(_graphics_queue, 1, &submit_info, _in_flight_fences[_current_frame]) != VK_SUCCESS) {
		ERROR("Failed to submit draw command buffer");
        std::terminate();
	}

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	VkSwapchainKHR swapchains[] = { _swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &_image_index;
	present_info.pResults = nullptr;

	auto result = vkQueuePresentKHR(_present_queue, &present_info);
    bool recreated = false;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain_and_its_resources();
        recreated = true;
    } else if (result != VK_SUCCESS) {
        ERROR("Failed to present swap chain");
        std::terminate();
    }

    _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return recreated;
}

void context_t::begin_swapchain_renderpass(VkCommandBuffer commandbuffer, const VkClearValue& clear_value) {
    VIZON_PROFILE_FUNCTION();
    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.framebuffer = _swapchain_framebuffers[_image_index];
    render_pass_begin_info.renderPass = _swapchain_renderpass;
    render_pass_begin_info.renderArea.offset = {0, 0};
    render_pass_begin_info.renderArea.extent = _swapchain_extent;
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    vkCmdBeginRenderPass(commandbuffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);  // hardcoded subpass contents inline
}

void context_t::end_swapchain_renderpass(VkCommandBuffer commandbuffer) {
    VIZON_PROFILE_FUNCTION();
    vkCmdEndRenderPass(commandbuffer);
}

VkCommandBuffer context_t::start_single_use_commandbuffer() {
    VIZON_PROFILE_FUNCTION();
    VkCommandBufferAllocateInfo commandbuffer_allocate_info{};
    commandbuffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandbuffer_allocate_info.commandPool = _command_pool;
    commandbuffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandbuffer_allocate_info.commandBufferCount = 1;

    VkCommandBuffer commandbuffer;

    vkAllocateCommandBuffers(_device, &commandbuffer_allocate_info, &commandbuffer);

    VkCommandBufferBeginInfo commandbuffer_begin_info{};
    commandbuffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandbuffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandbuffer, &commandbuffer_begin_info);

    return commandbuffer;
}

void context_t::end_single_use_commandbuffer(VkCommandBuffer commandbuffer) {
    VIZON_PROFILE_FUNCTION();
    vkEndCommandBuffer(commandbuffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &commandbuffer;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence single_use_command_complete_fence;
    {
        auto result = vkCreateFence(_device, &fence_create_info, nullptr, &single_use_command_complete_fence);
        if (result != VK_SUCCESS) {
            ERROR("Failed to create fence!");
            std::terminate();
        }
    }

    vkQueueSubmit(_graphics_queue, 1, &submit_info, single_use_command_complete_fence);
    {
        auto result = vkWaitForFences(_device, 1, &single_use_command_complete_fence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) {
            ERROR("Failed to wait for fence!");
            std::terminate();
        }
    }

    vkDestroyFence(_device, single_use_command_complete_fence, nullptr);

    vkFreeCommandBuffers(_device, _command_pool, 1, &commandbuffer);
}

void context_t::single_use_commandbuffer(std::function<void(VkCommandBuffer)> fn) {
    VIZON_PROFILE_FUNCTION();
    auto commandbuffer = start_single_use_commandbuffer();
    fn(commandbuffer);
    end_single_use_commandbuffer(commandbuffer);
}

uint32_t context_t::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VIZON_PROFILE_FUNCTION();
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(_physical_device, &physical_device_memory_properties);

    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (physical_device_memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    ERROR("Failed to find suitable memory type");
    std::terminate();
}

VkSampler context_t::sampler(const sampler_create_info_t& sampler_create_info) {
    auto sampler_itr = _sampler_table.find(sampler_create_info);
    if (sampler_itr != _sampler_table.end()) {
        return sampler_itr->second;
    }
    VkSamplerCreateInfo vk_sampler_create_info{};
    vk_sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    vk_sampler_create_info.flags;
    vk_sampler_create_info.magFilter = sampler_create_info.mag_filter;
    vk_sampler_create_info.minFilter = sampler_create_info.min_filter;
    vk_sampler_create_info.mipmapMode = sampler_create_info.mipmap_mode;
    vk_sampler_create_info.addressModeU = sampler_create_info.address_mode_u;
    vk_sampler_create_info.addressModeV = sampler_create_info.address_mode_v;
    vk_sampler_create_info.addressModeW = sampler_create_info.address_mode_w;
    vk_sampler_create_info.mipLodBias = sampler_create_info.mip_lod_bias;
    vk_sampler_create_info.anisotropyEnable = sampler_create_info.anisotropy_enable;
    vk_sampler_create_info.maxAnisotropy = sampler_create_info.max_anisotropy;
    vk_sampler_create_info.compareEnable = sampler_create_info.compare_enable;
    vk_sampler_create_info.compareOp = sampler_create_info.compare_op;
    vk_sampler_create_info.minLod = sampler_create_info.min_lod;
    vk_sampler_create_info.maxLod = sampler_create_info.max_lod;
    vk_sampler_create_info.borderColor = sampler_create_info.border_color;
    vk_sampler_create_info.unnormalizedCoordinates = sampler_create_info.unnormalized_coordinates;

    VkSampler sampler{};
    if (vkCreateSampler(_device, &vk_sampler_create_info, nullptr, &sampler) != VK_SUCCESS) {
        ERROR("Failed to create sampler");
        std::terminate();
    }

    _sampler_table[sampler_create_info] = sampler;
    return sampler;
}

bool context_t::check_instance_validation_layer_support() {
    VIZON_PROFILE_FUNCTION();
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());

    for (auto layer : layers) {
        std::string_view layerName = layer.layerName;
        if (layerName == "VK_LAYER_KHRONOS_validation") 
            return true;
    }
    return false;
}   

void context_t::push_instance_validation_layers() {
    VIZON_PROFILE_FUNCTION();
    _instance_layers.push_back("VK_LAYER_KHRONOS_validation");   
}

void context_t::push_instance_debug_utils_messenger_extension() {
    VIZON_PROFILE_FUNCTION();
    _instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

void context_t::push_required_instance_extensions() {
    VIZON_PROFILE_FUNCTION();
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    for (uint32_t i = 0; i < glfw_extension_count; i++) {
        _instance_extensions.push_back(glfw_extensions[i]);
    }
    _instance_extensions.push_back("VK_KHR_get_physical_device_properties2");
}

VkDebugUtilsMessengerCreateInfoEXT context_t::get_debug_utils_messenger_create_info() {
    VIZON_PROFILE_FUNCTION();
    VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{};
    debug_utils_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_utils_messenger_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
                                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_utils_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    | 
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT; /*| 
                                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; */
    debug_utils_messenger_create_info.pfnUserCallback = debug_callback;
    debug_utils_messenger_create_info.pUserData = nullptr;
    return debug_utils_messenger_create_info;
}

void context_t::setup_debug_messenger() {
    VIZON_PROFILE_FUNCTION();
    auto debug_messenger_create_info = get_debug_utils_messenger_create_info();

    auto result = vkCreateDebugUtilsMessengerEXT(_instance, &debug_messenger_create_info, nullptr, &_debug_utils_messenger);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create debug utils messenger!");
        std::terminate();
    }
    TRACE("Created debug utils messenger");
}

void context_t::create_surface() {
    VIZON_PROFILE_FUNCTION();
    auto result = glfwCreateWindowSurface(_instance, _window->window(), nullptr, &_surface);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create surface");
        std::terminate();
    }
    TRACE("Created surface");
}

void context_t::push_required_device_extensions() {
    VIZON_PROFILE_FUNCTION();
    _device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    if (_raytracing) {
        _device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        _device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        _device_extensions.push_back("VK_EXT_descriptor_indexing");
        _device_extensions.push_back(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
        _device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        _device_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }
}

queue_family_indices_t context_t::find_queue_families(VkPhysicalDevice physical_device) {
    VIZON_PROFILE_FUNCTION();
    queue_family_indices_t queue_family_indices{};

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families_properties(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families_properties.data());

    int i = 0;
    for (auto queue_family_properties : queue_families_properties) {
        if ((queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            queue_family_indices.graphics_family = i;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, _surface, &present_support);
        if (present_support) {
            queue_family_indices.present_family = i;
        }

        if (queue_family_indices.is_complete()) {
            break;
        }
        i++;
    }

    return queue_family_indices;
}

bool context_t::is_device_extension_supported(VkPhysicalDevice physical_device) {
    VIZON_PROFILE_FUNCTION();
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(_device_extensions.begin(), _device_extensions.end());

    for (auto extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }
    return required_extensions.empty();
}

swapchain_support_details_t context_t::query_swapchain_support(VkPhysicalDevice physical_device) {
    VIZON_PROFILE_FUNCTION();
    swapchain_support_details_t swap_chain_support_details{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _surface, &swap_chain_support_details.capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _surface, &format_count, nullptr);
    if (format_count != 0) {
        swap_chain_support_details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _surface, &format_count, swap_chain_support_details.formats.data());
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, _surface, &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        swap_chain_support_details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, _surface, &present_mode_count, swap_chain_support_details.present_modes.data());
    }

    return swap_chain_support_details;
}

bool context_t::is_physical_device_suitable(VkPhysicalDevice physical_device) {
    VIZON_PROFILE_FUNCTION();
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &device_features);

    auto queue_family_indices = find_queue_families(physical_device);

    bool extensions_supported = is_device_extension_supported(physical_device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        auto swap_chain_support_details = query_swapchain_support(physical_device);
        swapchain_adequate = !swap_chain_support_details.formats.empty() && !swap_chain_support_details.present_modes.empty();
    }

    INFO("timestampperiod {}", device_properties.limits.timestampPeriod);    

    return queue_family_indices.is_complete() && swapchain_adequate && device_properties.limits.timestampComputeAndGraphics == VK_TRUE;
}

void context_t::pick_physical_device() {
    VIZON_PROFILE_FUNCTION();
    push_required_device_extensions();

    uint32_t device_count;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
    if (device_count == 0) {
        ERROR("Failed to find a GPU with vulkan support, try updating the drivers");
        std::terminate();
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(_instance, &device_count, devices.data());

    for (auto device : devices) {
        if (is_physical_device_suitable(device)) {
            _physical_device = device;
            break;
        }
    }
    if (_physical_device == VK_NULL_HANDLE) {
        ERROR("Failed to find a suitable GPU");
        std::terminate();
    }

    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceProperties(_physical_device, &device_properties);
    vkGetPhysicalDeviceFeatures(_physical_device, &device_features);

    _physical_device_properties = device_properties;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR physical_device_raytracing_pipeline_properties_KHR{};
    physical_device_raytracing_pipeline_properties_KHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 physical_device_properties_2{}; 
    physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physical_device_properties_2.properties = _physical_device_properties;
    physical_device_properties_2.pNext = &physical_device_raytracing_pipeline_properties_KHR;

    vkGetPhysicalDeviceProperties2(_physical_device, &physical_device_properties_2);

    VkPhysicalDeviceMemoryProperties physical_device_memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(_physical_device, &physical_device_memory_properties);

    INFO("Picked physical device: {}", device_properties.deviceName);
}

void context_t::push_device_validation_layers() {
    VIZON_PROFILE_FUNCTION();
    _device_layers.push_back("VK_LAYER_KHRONOS_validation"); 
}

void context_t::create_logical_device() {
    VIZON_PROFILE_FUNCTION();
    queue_family_indices_t queue_family_indices = find_queue_families(_physical_device);

    std::vector<VkDeviceQueueCreateInfo> device_queue_create_infos{};
    std::set<uint32_t> unique_queue_families = { queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value() };
    
    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo device_queue_create_info{};
        device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_create_info.queueFamilyIndex = queue_family;
        device_queue_create_info.queueCount = 1;
        device_queue_create_info.pQueuePriorities = &queue_priority;
        device_queue_create_infos.push_back(device_queue_create_info);
    }

    VkPhysicalDeviceScalarBlockLayoutFeatures physical_device_scalar_block_layout_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures
        physical_device_buffer_device_address_features = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &physical_device_scalar_block_layout_features,
            .bufferDeviceAddress = VK_TRUE,
            .bufferDeviceAddressCaptureReplay = VK_FALSE,
            .bufferDeviceAddressMultiDevice = VK_FALSE};

    VkPhysicalDeviceAccelerationStructureFeaturesKHR
        physical_device_acceleration_structure_features = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
            .pNext = &physical_device_buffer_device_address_features,
            .accelerationStructure = VK_TRUE,
            .accelerationStructureCaptureReplay = VK_FALSE,
            .accelerationStructureIndirectBuild = VK_FALSE,
            .accelerationStructureHostCommands = VK_FALSE,
            .descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE};

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR
        physical_device_raytracing_pipeline_features = {
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = &physical_device_acceleration_structure_features,
            .rayTracingPipeline = VK_TRUE,
            .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
            .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
            .rayTracingPipelineTraceRaysIndirect = VK_FALSE,
            .rayTraversalPrimitiveCulling = VK_FALSE};
    VkPhysicalDeviceRayQueryFeaturesKHR physical_device_ray_query_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
        .pNext = &physical_device_raytracing_pipeline_features,
        .rayQuery = VK_TRUE};

    VkPhysicalDeviceFeatures device_features = {.geometryShader = VK_TRUE, .fragmentStoresAndAtomics = VK_TRUE};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = device_queue_create_infos.data();
    device_create_info.queueCreateInfoCount = device_queue_create_infos.size();
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = 0;
    if (_raytracing)
        device_create_info.pNext = &physical_device_ray_query_features;
    else  // always enable bda 
        device_create_info.pNext = &physical_device_buffer_device_address_features;

    if (_validation) 
        push_device_validation_layers();

    device_create_info.enabledLayerCount = _device_layers.size();
    device_create_info.ppEnabledLayerNames = _device_layers.data();
    device_create_info.enabledExtensionCount = _device_extensions.size();
    device_create_info.ppEnabledExtensionNames = _device_extensions.data();

    auto result = vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create device");
        std::terminate();
    }
    TRACE("Created device");

    // volkLoadDevice(_device);

    vkGetDeviceQueue(_device, queue_family_indices.graphics_family.value(), 0, &_graphics_queue);
    vkGetDeviceQueue(_device, queue_family_indices.present_family.value(), 0, &_present_queue);
}

VkSurfaceFormatKHR context_t::choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
    VIZON_PROFILE_FUNCTION();
    for (auto available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }
    return available_formats[0];
}

VkPresentModeKHR context_t::choose_swap_present_mode(const std::vector<VkPresentModeKHR>& available_present_modes) {
    VIZON_PROFILE_FUNCTION();
    for (auto available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D context_t::choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) {
    VIZON_PROFILE_FUNCTION();
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(_window->window(), &width, &height);

        VkExtent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

void context_t::create_swapchain() {
    VIZON_PROFILE_FUNCTION();
    swapchain_support_details_t swapchain_support = query_swapchain_support(_physical_device);

    auto surface_format = choose_swap_surface_format(swapchain_support.formats);
    auto present_mode = choose_swap_present_mode(swapchain_support.present_modes);
    auto extent = choose_swap_extent(swapchain_support.capabilities);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

    if (swapchain_support.capabilities.maxImageCount > 0 && image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = _surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = surface_format.format;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto queue_family_indices = find_queue_families(_physical_device);
    uint32_t queue_indices[] = { queue_family_indices.graphics_family.value(), queue_family_indices.present_family.value() };

    if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_indices;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchain_create_info.preTransform = swapchain_support.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = present_mode;
    swapchain_create_info.clipped = VK_TRUE;

    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    auto result = vkCreateSwapchainKHR(_device, &swapchain_create_info, nullptr, &_swapchain);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create swap chain");
        std::terminate();
    }

    _swapchain_extent = extent;
    _swapchain_format = surface_format.format;

    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr);
    _swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data());

    _swapchain_image_views.resize(image_count);
    for (size_t i = 0; i < _swapchain_images.size(); i++) {
        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = _swapchain_images[i];
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = _swapchain_format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;
        auto result = vkCreateImageView(_device, &image_view_create_info, nullptr, &_swapchain_image_views[i]);
        if (result != VK_SUCCESS) {
            ERROR("Failed to create image view");
            std::terminate();
        }
    }

    TRACE("Created swap chain");
}

void context_t::create_renderpass() {
    VIZON_PROFILE_FUNCTION();
    VkAttachmentDescription color_attachment{};
	color_attachment.format = _swapchain_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo renderpass_info{};
	renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = 1;
	renderpass_info.pAttachments = &color_attachment;
	renderpass_info.subpassCount = 1;
	renderpass_info.pSubpasses = &subpass;
	renderpass_info.dependencyCount = 1;
	renderpass_info.pDependencies = &dependency;

	if (vkCreateRenderPass(_device, &renderpass_info, nullptr, &_swapchain_renderpass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void context_t::create_framebuffers() {
    VIZON_PROFILE_FUNCTION();
    _swapchain_framebuffers.resize(_swapchain_images.size());
    for (size_t i = 0; i < _swapchain_images.size(); i++) {
        VkImageView attachments[] = {
            _swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = _swapchain_renderpass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = _swapchain_extent.width;
        framebuffer_info.height = _swapchain_extent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(_device, &framebuffer_info, nullptr, &_swapchain_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }

    }
}

void context_t::create_command_pool() {
    VIZON_PROFILE_FUNCTION();
    auto queue_family_indices = find_queue_families(_physical_device);
    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
    if (vkCreateCommandPool(_device, &command_pool_create_info, nullptr, &_command_pool) != VK_SUCCESS) {
        ERROR("Failed to create command pool");
        std::terminate();
    }
}

void context_t::allocate_commandbuffers() {
    VIZON_PROFILE_FUNCTION();
    VkCommandBufferAllocateInfo commandbuffer_allocate_info{};
    commandbuffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandbuffer_allocate_info.commandPool = _command_pool;
    commandbuffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandbuffer_allocate_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    
    _commandbuffers.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(_device, &commandbuffer_allocate_info, _commandbuffers.data()) != VK_SUCCESS) {
        ERROR("Failed to allocate commandbuffer");
        std::terminate();
    }
}

void context_t::create_sync_objects() {
    VIZON_PROFILE_FUNCTION();
    _image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_image_available_semaphores[i]) != VK_SUCCESS) {
            ERROR("Failed to create semaphore");
            std::terminate();
        }

        if (vkCreateSemaphore(_device, &semaphore_create_info, nullptr, &_render_finished_semaphores[i]) != VK_SUCCESS) {
            ERROR("Failed to create semaphore");
            std::terminate();
        }

        if (vkCreateFence(_device, &fence_create_info, nullptr, &_in_flight_fences[i]) != VK_SUCCESS) {
            ERROR("Failed to create fence");
            std::terminate();
        }
    }    
}

void context_t::create_descriptor_pool() {
    VIZON_PROFILE_FUNCTION();
    std::vector<VkDescriptorPoolSize> pool_sizes{};

    if (_raytracing) {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        pool_size.descriptorCount = 1000;
        pool_sizes.push_back(pool_size);
    }

    
    VkDescriptorPoolCreateInfo descriptor_pool_create_info{};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.poolSizeCount = pool_sizes.size();
    descriptor_pool_create_info.pPoolSizes = pool_sizes.data();
    descriptor_pool_create_info.maxSets = 1000 * 7;

    if (vkCreateDescriptorPool(_device, &descriptor_pool_create_info, nullptr, &_descriptor_pool) != VK_SUCCESS) {
        ERROR("Failed to create descriptor pool");
        std::terminate();
    }
}

void context_t::recreate_swapchain_and_its_resources() {
    VIZON_PROFILE_FUNCTION();
    int width, height;
    glfwGetFramebufferSize(_window->window(), &width, &height);
    if (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window->window(), &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(_device);

    for (auto framebuffer : _swapchain_framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }
    for (auto imageView : _swapchain_image_views) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    create_swapchain();
    create_framebuffers();

    for (auto resizeCallBack : _resize_call_backs) {
        resizeCallBack();
    }
}

} // namespace vulkan

} // namespace gfx
