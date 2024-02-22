#include "timer.hpp"

#include "core/log.hpp"

namespace gfx {

namespace vulkan {

gpu_timer_t::gpu_timer_t(core::ref<context_t> context)
  : _context(context) {
    VkQueryPoolCreateInfo query_pool_create_info{};
    query_pool_create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_create_info.queryCount = 2;
    query_pool_create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;

    if (vkCreateQueryPool(context->device(), &query_pool_create_info, nullptr, &_queryPool) != VK_SUCCESS) {
        ERROR("Failed to create query pool");
        std::terminate();
    }
}

gpu_timer_t::~gpu_timer_t() {
    vkDestroyQueryPool(_context->device(), _queryPool, nullptr);
}

void gpu_timer_t::begin(VkCommandBuffer commandbuffer, VkPipelineStageFlagBits pipeline_stage_flag) {
    vkCmdResetQueryPool(commandbuffer, _queryPool, 0, 2);
    vkCmdWriteTimestamp(commandbuffer, pipeline_stage_flag, _queryPool, 0);
}

void gpu_timer_t::end(VkCommandBuffer commandbuffer, VkPipelineStageFlagBits pipeline_stage_flag) {
    vkCmdWriteTimestamp(commandbuffer, pipeline_stage_flag, _queryPool, 1);
}

std::optional<float> gpu_timer_t::get_time() {
    uint64_t time_stamps[2];
    auto result = vkGetQueryPoolResults(_context->device(), _queryPool, 0, 2, sizeof(uint64_t) * 2, time_stamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return std::nullopt;
    } else if (result != VK_SUCCESS) {
        WARN("not successful time stamp");
        return std::nullopt;
    }
    return ((time_stamps[1] - time_stamps[0]) * _context->physical_device_properties().limits.timestampPeriod) / 1000000.f;
}

} // namespace vulkan

} // namespace gfx
