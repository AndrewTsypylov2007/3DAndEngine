// include/core/Application.h
#pragma once

#include "LibraryLoader.h"
#include "ServiceManager.h"
#include "EventBus.h"
#include <memory>

namespace core {

	class Application {
	public:
		Application();
		~Application();

		int run();
		void stop();

		LibraryLoader& libraryLoader() { return libraryLoader_; }
		ServiceManager& services() { return services_; }
		std::shared_ptr<EventBus> eventBus() { return eventBus_; }

	private:
		bool running_ = false;
		LibraryLoader libraryLoader_;
		ServiceManager services_;
		std::shared_ptr<EventBus> eventBus_;
	};

} // namespace core
