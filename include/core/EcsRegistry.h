#pragma once

#include "Types.h"
#include <vector>
#include <memory>
#include <cassert>
#include <string>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <unordered_map>
#include <typeinfo>
#include <cstring>
#include <algorithm>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4324)
#endif

namespace core {

    // ==============================================================================
    // РЕАКТИВНЫЙ ИНТЕРФЕЙС СОБЫТИЙ СУЩНОСТЕЙ
    // ==============================================================================
    struct EcsListener {
        std::function<void(Entity, ComponentTypeId)> on_component_added;
        std::function<void(Entity, ComponentTypeId)> on_component_removed;
    };

    // ==============================================================================
    // БАЗОВЫЙ ИНТЕРФЕЙС ПУЛА КОМПОНЕНТОВ
    // ==============================================================================
    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual void remove(Entity entity) = 0;
        virtual bool has(Entity entity) const = 0;
        virtual void clear() = 0;
        virtual size_t size() const = 0;
    };

    // ==============================================================================
    // PAGED SPARSE SET COMPONENT POOL (AAA Industry Standard)
    // ==============================================================================
    template<typename T>
    class ComponentPool : public IComponentPool {
    public:
        static constexpr size_t PAGE_SIZE = 1024;
        static constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

    private:
        // Плотные упакованные массивы данных (DOD: максимальный кэш L1/L2)
        std::vector<T>      dense_data_;
        std::vector<Entity> dense_entities_;

        // Постраничный разреженный массив (Sparse Pages)
        std::vector<size_t*> sparse_pages_;

        size_t* get_or_create_sparse_page(size_t page_index) {
            if (page_index >= sparse_pages_.size()) {
                sparse_pages_.resize(page_index + 1, nullptr);
            }
            if (!sparse_pages_[page_index]) {
                sparse_pages_[page_index] = new size_t[PAGE_SIZE];
                std::fill_n(sparse_pages_[page_index], PAGE_SIZE, INVALID_INDEX);
            }
            return sparse_pages_[page_index];
        }

        size_t get_sparse_index(Entity entity) const {
            size_t page_index = entity / PAGE_SIZE;
            size_t page_offset = entity % PAGE_SIZE;

            if (page_index >= sparse_pages_.size() || !sparse_pages_[page_index]) {
                return INVALID_INDEX;
            }
            return sparse_pages_[page_index][page_offset];
        }

    public:
        ComponentPool() = default;

        ~ComponentPool() override {
            clear();
        }

        ComponentPool(const ComponentPool&) = delete;
        ComponentPool& operator=(const ComponentPool&) = delete;

        ComponentPool(ComponentPool&& other) noexcept
            : dense_data_(std::move(other.dense_data_)),
            dense_entities_(std::move(other.dense_entities_)),
            sparse_pages_(std::move(other.sparse_pages_)) {
            other.sparse_pages_.clear();
        }

        ComponentPool& operator=(ComponentPool&& other) noexcept {
            if (this != &other) {
                clear();
                dense_data_ = std::move(other.dense_data_);
                dense_entities_ = std::move(other.dense_entities_);
                sparse_pages_ = std::move(other.sparse_pages_);
                other.sparse_pages_.clear();
            }
            return *this;
        }

        T& assign(Entity entity, T component) {
            size_t page_index = entity / PAGE_SIZE;
            size_t page_offset = entity % PAGE_SIZE;

            size_t* page = get_or_create_sparse_page(page_index);
            size_t existing_dense = page[page_offset];

            if (existing_dense != INVALID_INDEX) {
                // Если компонент уже есть — перезаписываем на месте
                dense_data_[existing_dense] = std::move(component);
                return dense_data_[existing_dense];
            }

            // Добавляем в конец плотного массива
            size_t new_dense_idx = dense_data_.size();
            page[page_offset] = new_dense_idx;

            dense_entities_.push_back(entity);
            dense_data_.push_back(std::move(component));

            return dense_data_[new_dense_idx];
        }

        T* get(Entity entity) {
            size_t dense_idx = get_sparse_index(entity);
            if (dense_idx == INVALID_INDEX) return nullptr;
            return &dense_data_[dense_idx];
        }

        const T* get(Entity entity) const {
            size_t dense_idx = get_sparse_index(entity);
            if (dense_idx == INVALID_INDEX) return nullptr;
            return &dense_data_[dense_idx];
        }

        bool has(Entity entity) const override {
            return get_sparse_index(entity) != INVALID_INDEX;
        }

        void remove(Entity entity) override {
            size_t page_index = entity / PAGE_SIZE;
            size_t page_offset = entity % PAGE_SIZE;

            if (page_index >= sparse_pages_.size() || !sparse_pages_[page_index]) return;

            size_t index_to_remove = sparse_pages_[page_index][page_offset];
            if (index_to_remove == INVALID_INDEX) return;

            size_t last_index = dense_data_.size() - 1;

            if (index_to_remove != last_index) {
                // Перемещаем последний элемент на место удаляемого (O(1) Swap-and-Pop)
                dense_data_[index_to_remove] = std::move(dense_data_[last_index]);
                Entity last_entity = dense_entities_[last_index];
                dense_entities_[index_to_remove] = last_entity;

                // Обновляем sparse-указатель перемещенного элемента
                size_t last_page_idx = last_entity / PAGE_SIZE;
                size_t last_page_offset = last_entity % PAGE_SIZE;
                sparse_pages_[last_page_idx][last_page_offset] = index_to_remove;
            }

            // Освобождаем ячейку удаленного
            sparse_pages_[page_index][page_offset] = INVALID_INDEX;

            dense_data_.pop_back();
            dense_entities_.pop_back();
        }

        void clear() override {
            dense_data_.clear();
            dense_entities_.clear();
            for (size_t* page : sparse_pages_) {
                delete[] page;
            }
            sparse_pages_.clear();
        }

        // Высокоскоростные DOD геттеры
        size_t size() const override { return dense_data_.size(); }
        bool empty() const { return dense_data_.empty(); }

        T* data() { return dense_data_.data(); }
        const T* data() const { return dense_data_.data(); }

        const Entity* entities() const { return dense_entities_.data(); }

        template<typename Func>
        void each(Func&& func) {
            for (size_t i = 0; i < dense_data_.size(); ++i) {
                func(dense_entities_[i], dense_data_[i]);
            }
        }
    };

    // ==============================================================================
    // КРОСС-DLL СЧЕТЧИК ТИПОВ С КЭШИРОВАНИЕМ (Zero-Cost Lookup)
    // ==============================================================================
    class ComponentTypeCounter {
    private:
        struct Registry {
            std::unordered_map<std::string, ComponentTypeId> table;
            ComponentTypeId counter = 0;
            std::shared_mutex mutex;
        };

        static Registry& get_registry() {
            static Registry instance;
            return instance;
        }

        static ComponentTypeId register_type(const char* type_name) {
            auto& reg = get_registry();
            std::unique_lock lock(reg.mutex);

            auto it = reg.table.find(type_name);
            if (it == reg.table.end()) {
                ComponentTypeId new_id = reg.counter++;
                reg.table[type_name] = new_id;
                return new_id;
            }
            return it->second;
        }

    public:
        template<typename T>
        static ComponentTypeId get_id() {
            // КЭШИРУЕТСЯ СТРОГО 1 РАЗ ПРИ ПЕРВОМ ВЫЗОВЕ ТИПА
            static const ComponentTypeId cached_id = register_type(typeid(T).name());
            return cached_id;
        }
    };

    // ==============================================================================
    // REACTIONAL ECS REGISTRY (High-Speed Engine Core)
    // ==============================================================================
    class EcsRegistry {
    private:
        Entity next_entity_ = 1;
        std::vector<Entity> free_entities_; // Пул повторно используемых ID сущностей

        std::vector<std::unique_ptr<IComponentPool>> pools_;
        std::vector<EcsListener> listeners_;

        mutable std::shared_mutex main_mutex_;
        mutable std::shared_mutex listener_mutex_;

        template<typename T>
        ComponentPool<T>* get_or_create_pool_unlocked() {
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
        ~EcsRegistry() {
            clear();
        }

        EcsRegistry(const EcsRegistry&) = delete;
        EcsRegistry& operator=(const EcsRegistry&) = delete;

        Entity createEntity() {
            std::unique_lock lock(main_mutex_);
            if (!free_entities_.empty()) {
                Entity recycled = free_entities_.back();
                free_entities_.pop_back();
                return recycled;
            }
            return next_entity_++;
        }

        void destroyEntity(Entity entity) {
            std::unique_lock lock(main_mutex_);
            for (auto& pool : pools_) {
                if (pool && pool->has(entity)) {
                    pool->remove(entity);
                }
            }
            free_entities_.push_back(entity);
        }

        template<typename T>
        T& addComponent(Entity entity, T component = {}) {
            ComponentTypeId tid = ComponentTypeCounter::get_id<T>();
            T* ptr = nullptr;
            {
                std::unique_lock lock(main_mutex_);
                ptr = &get_or_create_pool_unlocked<T>()->assign(entity, std::move(component));
            }

            // Потокобезопасное оповещение слушателей по локальной копии (защита от дедлока/рекурсии)
            std::vector<EcsListener> listeners_copy;
            {
                std::shared_lock l_lock(listener_mutex_);
                listeners_copy = listeners_;
            }

            for (const auto& listener : listeners_copy) {
                if (listener.on_component_added) {
                    try {
                        listener.on_component_added(entity, tid);
                    }
                    catch (...) {}
                }
            }
            return *ptr;
        }

        template<typename T>
        T* getComponent(Entity entity) {
            std::shared_lock lock(main_mutex_);
            ComponentTypeId id = ComponentTypeCounter::get_id<T>();
            if (id >= pools_.size() || !pools_[id]) return nullptr;
            return static_cast<ComponentPool<T>*>(pools_[id].get())->get(entity);
        }

        template<typename T>
        const T* getComponent(Entity entity) const {
            std::shared_lock lock(main_mutex_);
            ComponentTypeId id = ComponentTypeCounter::get_id<T>();
            if (id >= pools_.size() || !pools_[id]) return nullptr;
            return static_cast<const ComponentPool<T>*>(pools_[id].get())->get(entity);
        }

        template<typename T>
        bool hasComponent(Entity entity) const {
            std::shared_lock lock(main_mutex_);
            ComponentTypeId id = ComponentTypeCounter::get_id<T>();
            if (id >= pools_.size() || !pools_[id]) return false;
            return pools_[id]->has(entity);
        }

        template<typename T>
        void removeComponent(Entity entity) {
            ComponentTypeId tid = ComponentTypeCounter::get_id<T>();
            {
                std::unique_lock lock(main_mutex_);
                if (tid < pools_.size() && pools_[tid]) {
                    pools_[tid]->remove(entity);
                }
            }

            std::vector<EcsListener> listeners_copy;
            {
                std::shared_lock l_lock(listener_mutex_);
                listeners_copy = listeners_;
            }

            for (const auto& listener : listeners_copy) {
                if (listener.on_component_removed) {
                    try {
                        listener.on_component_removed(entity, tid);
                    }
                    catch (...) {}
                }
            }
        }

        void addListener(EcsListener listener) {
            std::unique_lock lock(listener_mutex_);
            listeners_.push_back(std::move(listener));
        }

        template<typename T>
        ComponentPool<T>* view() {
            std::shared_lock lock(main_mutex_);
            ComponentTypeId id = ComponentTypeCounter::get_id<T>();
            if (id >= pools_.size() || !pools_[id]) return nullptr;
            return static_cast<ComponentPool<T>*>(pools_[id].get());
        }

        void clear() {
            std::unique_lock lock(main_mutex_);
            for (auto& pool : pools_) {
                if (pool) pool->clear();
            }
            pools_.clear();
            free_entities_.clear();
            next_entity_ = 1;

            std::unique_lock l_lock(listener_mutex_);
            listeners_.clear();
        }
    };

} // namespace core

#if defined(_MSC_VER)
#pragma warning(pop)
#endif