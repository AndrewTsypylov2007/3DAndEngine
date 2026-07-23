#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace core {

using EventHandlerId = std::uint64_t;

class EventBus {
public:
	EventBus() = default;
	~EventBus() = default;

	EventHandlerId subscribe(const std::string &topic, std::function<void()> handler);
	void unsubscribe(const std::string &topic, EventHandlerId id);
	void publish(const std::string &topic);

private:
	std::mutex mutex_;
	std::unordered_map<std::string, std::vector<std::pair<EventHandlerId, std::function<void()>>>> handlers_;
	EventHandlerId nextId_{1};
};

} // namespace core
