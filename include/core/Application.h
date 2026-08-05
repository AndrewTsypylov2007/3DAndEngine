#pragma once
#include "EcsRegistry.h"
#include "EventBus.h"
#include "JobSystem.h"
#include "PluginContract.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <atomic> // ИСПРАВЛЕНО: Добавлен заголовок для атомиков

namespace core {

    class Application {
    private:
        EcsRegistry     ecs_;
        EventBus        event_bus_;
        JobSystem       job_system_;

        // ИСПРАВЛЕНО: Флаг сделан атомарным для предотвращения кэширования компилятором в Release
        std::atomic<bool> is_running_{ false };

        std::vector<PluginInterface*> plugins_;

    public:
        void registerPlugin(PluginInterface* plugin) {
            if (!plugin) return;
            plugins_.push_back(plugin);

            // Сортировка по приоритету выполнения для исключения рассинхронизации кадра
            std::sort(plugins_.begin(), plugins_.end(), [](PluginInterface* a, PluginInterface* b) {
                return a->priority < b->priority;
                });

            EngineContext ctx{ &ecs_, &event_bus_, &job_system_ };
            plugin->on_load(ctx);
        }

        void run() {
            job_system_.initialize();

            // ИСПРАВЛЕНО: Установка флага с барьером памяти release
            is_running_.store(true, std::memory_order_release);

            auto last_time = std::chrono::high_resolution_clock::now();

            // ИСПРАВЛЕНО: Потокобезопасное чтение с барьером acquire
            while (is_running_.load(std::memory_order_acquire)) {
                auto current_time = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(current_time - last_time).count();
                last_time = current_time;

                // 1. Выполнение логики всех плагинов в строго заданном порядке
                for (auto* plugin : plugins_) {
                    if (plugin->on_update) {
                        plugin->on_update(dt);
                    }
                }

                // 2. Сброс кольцевого буфера событий в конце кадра
                event_bus_.clear();

                // Если плагинов нет, выходим, чтобы не подвешивать поток на пустом цикле
                if (plugins_.empty()) {
                    is_running_.store(false, std::memory_order_release);
                }
            }

            // Корректное завершение работы в обратном порядке (LIFO)
            for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
                if ((*it)->on_unload) {
                    (*it)->on_unload();
                }
            }
            job_system_.shutdown();
        }

        void stop() {
            // ИСПРАВЛЕНО: Теперь метод stop() может быть безопасно вызван 
            // из ЛЮБОГО потока (например, из плагина Окна или Физики)
            is_running_.store(false, std::memory_order_release);
        }
    };

} // namespace core
