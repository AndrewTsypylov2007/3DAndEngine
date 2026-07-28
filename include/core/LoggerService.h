#pragma once
#include "IService.h"
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace core {

    class LoggerService : public IService {
    public:
        LoggerService();
        virtual ~LoggerService();

        // Паспорт сервиса v0.1.1
        std::string getServiceName() const override { return "LoggerService"; }

        // Жизненный цикл (согласно твоему IService.h)
        bool init(ServiceManager& services) override;
        void start() override;
        void stop() override;

        // Методы логирования (теперь они точно члены класса)
        void info(const std::string& msg);
        void warn(const std::string& msg);
        void error(const std::string& msg);

        // Метод доступа к объекту
        std::shared_ptr<spdlog::logger> getLogger();

    private:
        std::shared_ptr<spdlog::logger> logger_; // Та самая переменная
    };

} // namespace core
