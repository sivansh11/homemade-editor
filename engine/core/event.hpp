#ifndef EVENT_HPP
#define EVENT_HPP

#include <queue>
#include <functional>
#include <unordered_map>
#include <vector>

namespace event {

using event_id_t = unsigned int;

struct event_t {};

class dispatcher_t {
public:
    dispatcher_t() = default;
    ~dispatcher_t() {}

    template <typename event_type_t>
    void subscribe(std::function<void(const event_t&)> &&handler) {
        event_id_t event_id = get_event_id<event_type_t>();
        _event_to_callbacks[event_id].push_back(handler);
    }

    template <typename event_type_t, typename... args_t>
    void post(args_t&&... args) {
        event_type_t e(std::forward<args_t>(args)...);
        event_id_t event_id = get_event_id<event_type_t>();
        auto event_callback_search = _event_to_callbacks.find(event_id);
        if (event_callback_search == _event_to_callbacks.end()) return;
        for (auto& callback : event_callback_search->second) {
            callback(static_cast<event_t&>(e));
        }
    }

private:
    template <typename event_type_t>
    event_id_t get_event_id() {
        static event_id_t s_event_id = _event_id_counter++;
        return s_event_id;
    }

private:
    event_id_t _event_id_counter = 0;
    std::unordered_map<event_id_t, std::vector<std::function<void(const event_t&)>>> _event_to_callbacks;
};

} // namespace event

#endif