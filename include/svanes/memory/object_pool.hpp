#pragma once

#include <cstddef>
#include <cstdio>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace svanes {

template <typename T> class ObjectPool final {
    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

    union Slot {
        T object;
        Slot *next;

        Slot() noexcept : next(nullptr) {}
        ~Slot() {}
    };

public:
    class Deleter {
    public:
        Deleter() = default;

        void operator()(T *object) const noexcept {
            if (object) {
                pool->Destroy(object);
            }
        }

    private:
        friend class ObjectPool;

        explicit Deleter(ObjectPool *pool) noexcept : pool(pool) {}

        ObjectPool *pool = nullptr;
    };

    using Pointer = std::unique_ptr<T, Deleter>;

    explicit ObjectPool(std::size_t initial_capacity) {
        if (initial_capacity == 0) {
            throw std::invalid_argument(
                "ObjectPool capacity must be positive.");
        }
        AddBlock(initial_capacity);
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;
    ObjectPool(ObjectPool &&) = delete;
    ObjectPool &operator=(ObjectPool &&) = delete;

    ~ObjectPool() {
        if (size != 0) {
            std::fputs(
                "ObjectPool must outlive all objects allocated from it.\n",
                stderr);
            std::terminate();
        }
    }

    template <typename... Args> Pointer Make(Args &&...args) {
        if (!first_free) {
            AddBlock(capacity);
        }

        Slot *slot = first_free;
        first_free = slot->next;

        try {
            T *object = std::construct_at(std::addressof(slot->object),
                                          std::forward<Args>(args)...);
            ++size;
            return Pointer(object, Deleter(this));
        } catch (...) {
            ReturnSlot(slot);
            throw;
        }
    }

    std::size_t GetCapacity() const noexcept { return capacity; }

    std::size_t GetSize() const noexcept { return size; }

    std::size_t GetAvailable() const noexcept { return capacity - size; }

private:
    void AddBlock(std::size_t count) {
        const std::size_t max_capacity =
            std::numeric_limits<std::size_t>::max() / sizeof(Slot);
        if (count > max_capacity || capacity > max_capacity - count) {
            throw std::length_error(
                "ObjectPool capacity exceeds its size limit.");
        }

        auto block = std::make_unique<Slot[]>(count);
        Slot *first = block.get();
        for (std::size_t i = 0; i + 1 < count; ++i) {
            block[i].next = &block[i + 1];
        }
        block[count - 1].next = first_free;

        blocks.push_back(std::move(block));
        first_free = first;
        capacity += count;
    }

    void ReturnSlot(Slot *slot) noexcept {
        std::construct_at(std::addressof(slot->next), first_free);
        first_free = slot;
    }

    void Destroy(T *object) noexcept {
        Slot *slot = reinterpret_cast<Slot *>(object);
        std::destroy_at(object);
        ReturnSlot(slot);
        --size;
    }

    std::vector<std::unique_ptr<Slot[]>> blocks;
    Slot *first_free = nullptr;
    std::size_t capacity = 0;
    std::size_t size = 0;
};

}
