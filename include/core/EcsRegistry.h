#pragma once
#include "Types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace core {

    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
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

    // Генератор уникальных ID для типов во время выполнения без typeid
    class ComponentTypeCounter {
    private:
        static inline ComponentTypeId next_id = 0;
    public:
        template<typename T>
        static ComponentTypeId get_id() {
            static ComponentTypeId id = next_id++;
            return id;
        }
    };

    class EcsRegistry {
    private:
        Entity next_entity_ = 1;
        // Используем плоский вектор пулов вместо unordered_map для максимальной скорости
        std::vector<std::unique_ptr<IComponentPool>> pools_;

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
        Entity createEntity() {
            return next_entity_++;
        }

        template<typename T>
        T& addComponent(Entity entity, T component = {}) {
            return get_pool<T>()->assign(entity, std::move(component));
        }

        template<typename T>
        T* getComponent(Entity entity) {
            return get_pool<T>()->get(entity);
        }

        template<typename T>
        void removeComponent(Entity entity) {
            get_pool<T>()->remove(entity);
        }

        template<typename T>
        ComponentPool<T>* view() {
            return get_pool<T>();
        }
    };

} // namespace core
