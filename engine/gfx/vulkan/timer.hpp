#ifndef GFX_VULKAN_TIMER_HPP
#define GFX_VULKAN_TIMER_HPP

#include "context.hpp"

#include <optional>

namespace gfx {
    
namespace vulkan {

// GPU side profiler
class gpu_timer_t {
public:
    gpu_timer_t(core::ref<context_t> context);
    ~gpu_timer_t();

    void begin(VkCommandBuffer commandbuffer, VkPipelineStageFlagBits pipeline_stage_flag = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    void end(VkCommandBuffer commandbuffer, VkPipelineStageFlagBits pipeline_stage_flag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    std::optional<float> get_time();

private:
    core::ref<context_t> _context{};
    VkQueryPool _queryPool{};
};

} // namespace vulkan

} // namespace gfx

#endif