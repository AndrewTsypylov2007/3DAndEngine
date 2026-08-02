#include "../../include/core/TaskScheduler.h"
#include "../../include/core/IServiceManager.h"
#include <iostream>

namespace core {
    bool TaskScheduler::init(IServiceManager& services) {
        std::cout << "[Core::Scheduler] TaskScheduler initialized." << std::endl;
        return true;
    }
    void TaskScheduler::start() {}
    void TaskScheduler::stop() {}
}
