#pragma once

#include <svanes/entity.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace svanes {

namespace detail {

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Remove(std::uint32_t index) = 0;
    virtual bool Has(std::uint32_t index) const = 0;
};

template <typename T>
class ComponentPool final : public IComponentPool {
public:
    template <typename... Args>
    T& Emplace(std::uint32_t index, Args&&... args)
    {
        auto [it, inserted] = components_.insert_or_assign(index, T(std::forward<Args>(args)...));
        return it->second;
    }

    void Remove(std::uint32_t index) override
    {
        components_.erase(index);
    }

    bool Has(std::uint32_t index) const override
    {
        return components_.find(index) != components_.end();
    }

    T* Find(std::uint32_t index)
    {
        const auto it = components_.find(index);
        return it == components_.end() ? nullptr : &it->second;
    }

    const T* Find(std::uint32_t index) const
    {
        const auto it = components_.find(index);
        return it == components_.end() ? nullptr : &it->second;
    }

    std::unordered_map<std::uint32_t, T>& All()
    {
        return components_;
    }

private:
    std::unordered_map<std::uint32_t, T> components_;
};

} // namespace detail

class Registry final {
public:
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    [[nodiscard]] bool IsValid(Entity entity) const;

    template <typename T, typename... Args>
    T& AddComponent(Entity entity, Args&&... args)
    {
        assert(IsValid(entity));
        return PoolFor<T>().Emplace(entity.index, std::forward<Args>(args)...);
    }

    template <typename T>
    void RemoveComponent(Entity entity)
    {
        if (auto* pool = PoolIfExists<T>()) {
            pool->Remove(entity.index);
        }
    }

    template <typename T>
    [[nodiscard]] bool HasComponent(Entity entity) const
    {
        const auto* pool = PoolIfExists<T>();
        return pool != nullptr && pool->Has(entity.index);
    }

    template <typename T>
    [[nodiscard]] T& GetComponent(Entity entity)
    {
        T* component = TryGetComponent<T>(entity);
        assert(component != nullptr);
        return *component;
    }

    template <typename T>
    [[nodiscard]] const T& GetComponent(Entity entity) const
    {
        const T* component = TryGetComponent<T>(entity);
        assert(component != nullptr);
        return *component;
    }

    template <typename T>
    [[nodiscard]] T* TryGetComponent(Entity entity)
    {
        auto* pool = PoolIfExists<T>();
        return pool == nullptr ? nullptr : pool->Find(entity.index);
    }

    template <typename T>
    [[nodiscard]] const T* TryGetComponent(Entity entity) const
    {
        const auto* pool = PoolIfExists<T>();
        return pool == nullptr ? nullptr : pool->Find(entity.index);
    }

    template <typename... Components, typename Func>
    void ForEach(Func&& func)
    {
        static_assert(sizeof...(Components) > 0, "ForEach requires at least one component type");

        using FirstComponent = std::tuple_element_t<0, std::tuple<Components...>>;

        auto* first_pool = PoolIfExists<FirstComponent>();
        if (first_pool == nullptr) {
            return;
        }

        for (auto& entry : first_pool->All()) {
            const Entity entity{entry.first, generations_[entry.first]};
            if ((HasComponent<Components>(entity) && ...)) {
                func(entity, GetComponent<Components>(entity)...);
            }
        }
    }

private:
    template <typename T>
    detail::ComponentPool<T>& PoolFor()
    {
        const std::type_index key{typeid(T)};
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            it = pools_.emplace(key, std::make_unique<detail::ComponentPool<T>>()).first;
        }
        return static_cast<detail::ComponentPool<T>&>(*it->second);
    }

    template <typename T>
    detail::ComponentPool<T>* PoolIfExists()
    {
        const auto it = pools_.find(std::type_index{typeid(T)});
        return it == pools_.end() ? nullptr : static_cast<detail::ComponentPool<T>*>(it->second.get());
    }

    template <typename T>
    const detail::ComponentPool<T>* PoolIfExists() const
    {
        const auto it = pools_.find(std::type_index{typeid(T)});
        return it == pools_.end() ? nullptr : static_cast<const detail::ComponentPool<T>*>(it->second.get());
    }

    std::vector<std::uint32_t> generations_;
    std::vector<bool> alive_;
    std::vector<std::uint32_t> free_indices_;
    std::unordered_map<std::type_index, std::unique_ptr<detail::IComponentPool>> pools_;
};

} // namespace svanes
