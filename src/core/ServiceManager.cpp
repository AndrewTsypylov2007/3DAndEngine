#include "../../include/core/ServiceManager.h"
#include "../../include/core/IService.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>

namespace core {

ServiceManager::~ServiceManager() {
	stopAll();
}

void ServiceManager::startAll() {
	std::lock_guard<std::mutex> lk(mutex_);
	// Topological sort with cycle detection (DFS)
	std::vector<std::type_index> order;
	std::vector<std::type_index> stack;
	std::unordered_map<std::type_index,int> state; // 0=unvisited,1=visiting,2=visited
	std::function<void(const std::type_index&)> dfs = [&](const std::type_index &t) {
		int st = state[t];
		if (st == 2) return; // already processed
		if (st == 1) {
			// cycle detected; construct cycle path for error message
			auto itpos = std::find(stack.begin(), stack.end(), t);
			std::string cycle;
			if (itpos != stack.end()) {
				for (auto it = itpos; it != stack.end(); ++it) {
					cycle += (*it).name();
					cycle += " -> ";
				}
				cycle += t.name();
			} else {
				cycle = t.name();
			}
			throw std::runtime_error(std::string("Dependency cycle detected: ") + cycle);
		}
		state[t] = 1;
		stack.push_back(t);
		auto it = deps_.find(t);
		if (it != deps_.end()) {
			for (auto &d : it->second) dfs(d);
		}
		stack.pop_back();
		state[t] = 2;
		order.push_back(t);
	};

	for (auto &kv : services_) dfs(kv.first);

	// Start services in dependency order (reverse topological order)
	for (auto it = order.rbegin(); it != order.rend(); ++it) {
		auto svcIt = services_.find(*it);
		if (svcIt != services_.end()) {
			auto svc = std::static_pointer_cast<IService>(svcIt->second);
			if (svc) svc->start();
		}
	}
}

void ServiceManager::stopAll() {
	std::lock_guard<std::mutex> lk(mutex_);
	for (auto &kv : services_) {
		auto svc = std::static_pointer_cast<IService>(kv.second);
		if (svc) svc->stop();
	}
}

} // namespace core
