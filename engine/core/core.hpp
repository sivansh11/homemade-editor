#ifndef CORE_CORE_HPP
#define CORE_CORE_HPP

// to be able to log timers
#include <spdlog/fmt/bundled/format.h>

#include <memory>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <source_location>

namespace core {

template <typename T>
using ref = std::shared_ptr<T>;

template <typename T, typename... Args>
static ref<T> make_ref(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// from: https://stackoverflow.com/a/57595105
template <typename T, typename... Rest>
void hash_combine(uint64_t& seed, const T& v, const Rest&... rest) {
    seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
};

namespace timer {

using duration_t = std::chrono::duration<double, std::milli>;

using timer_end_callback_t = std::function<void(duration_t)>;

struct scope_timer_t {
    scope_timer_t(timer_end_callback_t timer_end_callback) noexcept;
    ~scope_timer_t() noexcept;

    timer_end_callback_t _timer_end_callback;
    std::chrono::_V2::system_clock::time_point _start;
};

struct frame_function_timer_t {
    frame_function_timer_t(std::string_view scope_name) noexcept;

    scope_timer_t _scope_timer;
};

} // namespace timer

void clear_frame_function_times() noexcept;

std::unordered_map<std::string_view, timer::duration_t>& get_frame_function_times();

} // namespace core

template<>
struct fmt::formatter<core::timer::duration_t> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.end();
    }

    template <typename FormatContext>
    auto format(const core::timer::duration_t& input, FormatContext& ctx) -> decltype(ctx.out()) {
        return format_to(ctx.out(),
            "{}ms",
            input.count());
    }
};

#ifdef VIZON_PROFILE_ENABLE
#define VIZON_PROFILE_FUNCTION()  core::timer::frame_function_timer_t frame_function_timer{std::source_location::current().function_name()}
#else
#define VIZON_PROFILE_FUNCTION()
#endif

#endif