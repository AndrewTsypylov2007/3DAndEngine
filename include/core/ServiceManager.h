#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <vector>
#include "IService.h"
#include <cstring>

namespace core {

    class ServiceManager {
    public:
        ServiceManager() = default;
        ~ServiceManager();

        // Регистрация сервиса (используется внутри Ядра)
        template<typename T, typename... Deps>
        void registerService(std::shared_ptr<T> svc) {
            std::lock_guard<std::mutex> lk(mutex_);
            // Сохраняем под ключом typeid для быстрой работы внутри EXE
            services_[std::type_index(typeid(T))] = std::static_pointer_cast<IService>(svc);

            // Регистрируем зависимости для построения графа DFS
            std::vector<std::type_index> dv = { std::type_index(typeid(Deps))... };
            deps_[std::type_index(typeid(T))] = dv;
        }

        // Классический метод (эффективен внутри одного бинарного модуля)
        template<typename T>
        std::shared_ptr<T> getService() {
            std::lock_guard<std::mutex> lk(mutex_);
            auto it = services_.find(std::type_index(typeid(T)));
            if (it == services_.end()) return nullptr;

            // ВНИМАНИЕ: В Release этот cast может вернуть nullptr, если RTTI не совпал.
            // Для плагинов рекомендуется использовать getServiceByName.
            return std::static_pointer_cast<T>(it->second);
        }

        /**
         * УЛЬТИМАТИВНЫЙ МЕТОД ДЛЯ ПЛАГИНОВ (Release v0.1.1)
         * Поиск сервиса по строковому имени. Не зависит от typeid и RTTI.
         * Гарантирует связь между EXE и DLL.
         */
        std::shared_ptr<IService> getServiceByName(const char* name) {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto& pair : services_) {
                // Безопасное сравнение Си-строк
                if (pair.second && std::strcmp(pair.second->getServiceName(), name) == 0) {
                    return pair.second;
                }
            }
            return nullptr;
        }

        // Запуск и остановка всех систем (алгоритм DFS с детекцией циклов)
        void startAll();
        void stopAll();

    private:
        // Карта сервисов: ключ - тип, значение - указатель на базовый класс
        std::unordered_map<std::type_index, std::shared_ptr<IService>> services_;

        // Карта зависимостей для топологической сортировки
        std::unordered_map<std::type_index, std::vector<std::type_index>> deps_;

        // Порядок активации (для корректного LIFO выключения)
        std::vector<std::type_index> activeOrder_;

        std::mutex mutex_;
    };

} // namespace core
