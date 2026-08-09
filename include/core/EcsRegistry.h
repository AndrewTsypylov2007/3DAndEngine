#pragma once
#include "Types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <string>
#include <functional>
#include <shared_mutex>
#include <mutex>

namespace core {

    // ==============================================================================
    // РЕАКТИВНЫЙ ИНТЕРФЕЙС (v0.3.0)
    // ==============================================================================
    struct EcsListener {
        std::function<void(Entity, ComponentTypeId)> on_component_added;
        std::function<void(Entity, ComponentTypeId)> on_component_removed;
    };

    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
        virtual bool has(Entity entity) const = 0;
    };

    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        std::vector<T> data;
        std::vector<Entity> dense_to_entity;
        std::unordered_map<Entity, size_t> entity_to_sparse;

        T& assign(Entity entity, T&& component) {
            assert(entity_to_sparse.find(entity) == entity_to_sparse.end());
            size_t index = data.size();
            entity_to_sparse[entity] = index;
            dense_to_entity.push_back(entity);
            data.push_back(std::move(component));
            return data[index];
        }

        T* get(Entity entity) {
            auto it = entity_to_sparse.find(entity);
            if (it == entity_to_sparse.end()) return nullptr;
            return &data[it->second];
        }

        bool has(Entity entity) const override {
            return entity_to_sparse.find(entity) != entity_to_sparse.end();
        }

        void remove(Entity entity) override {
            auto it = entity_to_sparse.find(entity);
            if (it == entity_to_sparse.end()) return;

            size_t index_to_remove = it->second;
            size_t last_index = data.size() - 1;

            if (index_to_remove != last_index) {
                data[index_to_remove] = std::move(data[last_index]);
                Entity last_entity = dense_to_entity[last_index];
                dense_to_entity[index_to_remove] = last_entity;
                entity_to_sparse[last_entity] = index_to_remove;
            }

            data.pop_back();
            dense_to_entity.pop_back();
            entity_to_sparse.erase(entity);
        }
    };

    // ==============================================================================
    // КРОСС-DLL СЧЕТЧИК ТИПОВ (v0.3.0 Профессиональный Стандарт)
    // ==============================================================================
    // Избавляемся от статических инлайнов. Храним карту распределения ID типов
    // в куче, доступной через атомарный синглтон-мост.
    class ComponentTypeCounter {
    public:
        template<typename T>
        static ComponentTypeId get_id() {
            // Структура хранилища карт типов, гарантированно единая для всего рантайма
            struct TypeRegistry {
                std::unordered_map<std::string, ComponentTypeId> table;
                ComponentTypeId counter = 0;
                std::shared_mutex mutex;
            };

            // Легковесный синглтон-держатель, устойчивый к оптимизациям линкера MSVC
            static TypeRegistry* registry = []() {
                static TypeRegistry instance;
                return &instance;
                }();

            std::unique_lock lock(registry->mutex);
            std::string type_name = typeid(T).name();

            auto it = registry->table.find(type_name);
            if (it == registry->table.end()) {
                ComponentTypeId new_id = registry->counter++;
                registry->table[type_name] = new_id;
                return new_id;
            }
            return it->second;
        }
    };

    // ==============================================================================
    // REACTIONAL ECS REGISTRY
    // ==============================================================================
    class EcsRegistry {
    private:
        Entity next_entity_ = 1;
        std::vector<std::unique_ptr<IComponentPool>> pools_;
        std::vector<EcsListener> listeners_;
        mutable std::shared_mutex main_mutex_;

        template<typename T>
        ComponentPool<T>* get_pool() {
            ComponentTypeId id = ComponentTypeCounter::get_id<T>();
            if (id >= pools_.size()) {
                pools_.resize(id + 1);
            }
            if (!pools_[id]) {
                pools_[id] = std::make_unique<ComponentPool<T>>();
            }
            return static_cast<ComponentPool<T>*>(pools_[id].get());
        }

    public:
        EcsRegistry() = default;
        ~EcsRegistry() = default;

        // Исключаем копирование тяжелого реестра данных
        EcsRegistry(const EcsRegistry&) = delete;
        EcsRegistry& operator=(const EcsRegistry&) = delete;

        Entity createEntity() {
            std::unique_lock lock(main_mutex_);
            return next_entity_++;
        }

        template<typename T>
        T& addComponent(Entity entity, T component = {}) {
            ComponentTypeId tid = ComponentTypeCounter::get_id<T>();
            T* ptr = nullptr;
            {
                std::unique_lock lock(main_mutex_);
                ptr = &get_pool<T>()->assign(entity, std::move(component));
            }

            // Оповещаем внешние плагины (Физика/Рендер) через реактивные хуки
            for (auto& listener : listeners_) {
                if (listener.on_component_added) listener.on_component_added(entity, tid);
            }
            return *ptr;
        }

        template<typename T>
        T* getComponent(Entity entity) {
            std::shared_lock lock(main_mutex_);
            return get_pool<T>()->get(entity);
        }

        template<typename T>
        void removeComponent(Entity entity) {
            ComponentTypeId tid = ComponentTypeCounter::get_id<T>();
            {
                std::unique_lock lock(main_mutex_);
                get_pool<T>()->remove(entity);
            }

            for (auto& listener : listeners_) {
                if (listener.on_component_removed) listener.on_component_removed(entity, tid);
            }
        }

        void addListener(EcsListener listener) {
            std::unique_lock lock(main_mutex_);
            listeners_.push_back(std::move(listener));
        }

        template<typename T>
        ComponentPool<T>* view() {
            std::shared_lock lock(main_mutex_);
            return get_pool<T>();
        }
    };

} // namespace core
