#include "../../include/core/EventBus.h"
#include <algorithm>
#include <utility>

namespace core {

EventHandlerId EventBus::subscribe(const std::string &topic, std::function<void()> handler) {
	std::lock_guard<std::mutex> lk(mutex_);
	auto id = nextId_++;
	handlers_[topic].emplace_back(id, std::move(handler));
	return id;
}

void EventBus::unsubscribe(const std::string &topic, EventHandlerId id) {
	std::lock_guard<std::mutex> lk(mutex_);
	auto it = handlers_.find(topic);
	if (it == handlers_.end()) return;
	auto &vec = it->second;
	vec.erase(std::remove_if(vec.begin(), vec.end(), [id](auto &p){ return p.first == id; }), vec.end());
	if (vec.empty()) handlers_.erase(it);
}

void EventBus::publish(const std::string &topic) {
	std::vector<std::function<void()>> toCall;
	{
		std::lock_guard<std::mutex> lk(mutex_);
		auto it = handlers_.find(topic);
		if (it == handlers_.end()) return;
		for (auto &p : it->second) toCall.push_back(p.second);
	}
	for (auto &h : toCall) {
		if (h) h();
	}
}

} // namespace core
