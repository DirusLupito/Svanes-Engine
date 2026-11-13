#pragma once

#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace svanes {

/**
 * Manages reusable memory for objects of type T. The pool allocates memory in
 * blocks, but only constructs an object when Make is called.
 *
 * Each object is owned by a unique pointer. When that pointer destroys the
 * object, its slot goes back to the pool and can be used for another object.
 * The pool must outlive all objects allocated from it.
 *
 * This only provides memory for T itself. For example, if T contains a vector,
 * the vector still allocates its elements separately using its own allocator.
 *
 * The pool does not synchronize access between threads. Calls that create or
 * destroy objects must not overlap with other accesses to the same pool.
 *
 * @tparam T The type of object to store. Must be a non-array object type,
 * without const or volatile qualification, and with a destructor that does
 * not throw.
 */
template <typename T> class ObjectPool final {
    // Compile-time checks on T's type.
    // T must be:
    // - an object type (not a function or reference)
    // - not an array type
    // - not const or volatile qualified
    // - nothrow destructible

    static_assert(std::is_object_v<T> && !std::is_array_v<T>);
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);

    /**
     * Represents space for either an object or a pointer to the next free slot.
     * A union lets these two members share the same memory, since we only need
     * the next pointer when there is no object in the slot.
     *
     * The compiler makes each Slot large enough and properly aligned for both
     * members. This also respects any alignment requested by T with alignas.
     *
     * FIELDS:
     * - object: The object stored while this slot is in use.
     * - next: The next free slot, or nullptr if this is the last one. Only used
     * while the slot does not contain an object.
     */
    union Slot {
        T object;
        Slot *next;

        /**
         * Constructs an empty slot, initializing only the next pointer. The
         * user may allocate a whole block of slots without constructing any T
         * objects or requiring T to have a default constructor.
         */
        Slot() noexcept : next(nullptr) {}

        /**
         * Leaves destruction of T to ObjectPool::Destroy. All objects must
         * already have been destroyed before their block is freed.
         */
        ~Slot() {}
    };

public:
    /**
     * Used by std::unique_ptr to destroy an object and return its slot to the
     * pool it came from. We define the call operator () so the unique pointer
     * can call deleter(object) when it needs to destroy the object.
     *
     * The deleter remembers which pool owns the memory, so that pool must still
     * exist when the unique pointer calls it.
     */
    class Deleter {
    public:
        /**
         * Constructs a deleter with no associated pool so Pointer can be
         * default constructed as an empty unique pointer. Make supplies a
         * deleter with a pool when it creates an object.
         */
        Deleter() = default;

        /**
         * Destroys the object and returns its slot to the associated pool.
         * Does nothing if the pointer is null.
         *
         * @param object The object to destroy. Must have come from this
         * deleter's pool, or be nullptr.
         */
        void operator()(T *object) const noexcept {
            if (object) {
                pool->Destroy(object);
            }
        }

    private:
        // We need friend access to call ObjectPool::Destroy, which is private.
        friend class ObjectPool;

        /**
         * Constructs a deleter for objects allocated from the given pool.
         *
         * @param pool The pool to return slots to after destroying objects.
         */
        explicit Deleter(ObjectPool *pool) noexcept : pool(pool) {}

        // The pool that owns the object's memory. This pointer does not own
        // the pool or extend its lifetime.
        ObjectPool *pool = nullptr;
    };

    // A unique pointer whose deleter returns the object's memory to its pool.
    using Pointer = std::unique_ptr<T, Deleter>;

    /**
     * Constructs an ObjectPool with space for the given number of objects.
     * Allocates the initial block of slots without constructing any T objects.
     *
     * @param initial_capacity The initial number of slots, not the number of
     * bytes. Must be greater than zero.
     *
     * @throws std::invalid_argument if initial_capacity is zero.
     * @throws std::length_error if the capacity exceeds the pool's size limit.
     */
    explicit ObjectPool(std::size_t initial_capacity) {
        if (initial_capacity == 0) {
            throw std::invalid_argument(
                "ObjectPool capacity must be positive.");
        }

        AddBlock(initial_capacity);
    }

    /**
     * Deleted copy constructor to prevent two pools from owning the same
     * blocks of memory. This prevents something like:
     *
     * ObjectPool<Ship> pool1(100);
     * ObjectPool<Ship> pool2 = pool1;
     */
    ObjectPool(const ObjectPool &) = delete;

    /**
     * Deleted copy assignment operator to prevent copying ownership of the
     * pool's blocks of memory. This prevents something like:
     *
     * ObjectPool<Ship> pool1(100);
     * ObjectPool<Ship> pool2(100);
     * pool2 = pool1;
     */
    ObjectPool &operator=(const ObjectPool &) = delete;

    /**
     * Deleted move constructor because existing deleters store this pool's
     * address. Moving the pool would leave them pointing to the old instance.
     * This prevents something like:
     *
     * ObjectPool<Ship> pool1(100);
     * ObjectPool<Ship> pool2 = std::move(pool1);
     */
    ObjectPool(ObjectPool &&) = delete;

    /**
     * Deleted move assignment operator because existing deleters must keep
     * referring to the pool that owns their objects' memory.
     * This prevents something like:
     *
     * ObjectPool<Ship> pool1(100);
     * ObjectPool<Ship> pool2(100);
     * pool2 = std::move(pool1);
     */
    ObjectPool &operator=(ObjectPool &&) = delete;

    /**
     * Destroys the pool and frees its blocks of memory. All objects allocated
     * from this pool must already have been destroyed. Otherwise, we log the
     * lifetime error and terminate, since their pointers would refer to freed
     * memory and their deleters would refer to a pool that no longer exists.
     */
    ~ObjectPool() {
        if (size != 0) {
            std::cerr
                << "ObjectPool must outlive all objects allocated from it.\n";
            std::terminate();
        }
    }

    /**
     * Constructs an object in a free slot and returns a unique pointer that
     * owns it. If all slots are in use, the pool doubles its total capacity by
     * adding another block. Existing objects stay at their current addresses.
     *
     * If the object's constructor throws, the slot is returned to the pool
     * and the exception is rethrown. Allocation failures also propagate to the
     * caller.
     *
     * Example usage:
     *
     * ObjectPool<Ship> ships(100);
     * auto ship = ships.Make(world, gameplay_timeline, definition);
     *
     * The ship is destroyed before ships when this scope ends, returning its
     * slot before the pool frees the memory.
     *
     * @tparam Args The types of the arguments to pass to T's constructor.
     *
     * @param args The arguments to pass to T's constructor.
     *
     * @return A unique pointer using this pool's Deleter to destroy the object
     * and return its slot for reuse.
     *
     * @throws std::length_error if growing the pool exceeds its size limit.
     */
    template <typename... Args> Pointer Make(Args &&...args) {
        if (!first_free) {
            AddBlock(capacity);
        }

        Slot *slot = first_free;
        first_free = slot->next;

        try {
            // std::construct_at calls T's constructor in memory we already
            // own. It does not allocate another T. std::forward preserves
            // whether each argument was passed as an lvalue or an rvalue.
            T *object = std::construct_at(std::addressof(slot->object),
                                          std::forward<Args>(args)...);
            ++size;
            return Pointer(object, Deleter(this));
        } catch (...) {
            // If construction throws, there is no T to destroy, but its slot
            // still needs to go back into the free list.
            ReturnSlot(slot);
            throw;
        }
    }

    /**
     * Returns the total number of slots allocated across all blocks.
     *
     * @return The pool's capacity in objects, including both used and free
     * slots.
     */
    std::size_t GetCapacity() const noexcept { return capacity; }

    /**
     * Returns the number of objects currently alive in the pool.
     *
     * @return The number of constructed objects that have not been destroyed.
     */
    std::size_t GetSize() const noexcept { return size; }

    /**
     * Returns the number of free slots currently available.
     *
     * @return The number of objects we can construct without growing the pool.
     */
    std::size_t GetAvailable() const noexcept { return capacity - size; }

private:
    /**
     * Allocates another block of slots and adds them to the free list.
     * Each block owns a separate allocation, so growing the vector of blocks
     * does not move the slots or any objects already stored in them.
     *
     * @param count The number of additional slots to allocate. Must be
     * positive.
     * @throws std::length_error if the new capacity exceeds the pool's size
     * limit.
     */
    void AddBlock(std::size_t count) {
        // The largest representable byte count limits the number of slots.
        // Check the remaining capacity with subtraction so the check itself
        // cannot overflow by adding capacity and count.
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

        // Store the block before making its slots available. If the vector
        // allocation throws, the local unique pointer frees the new block and
        // the existing free list is still intact.
        blocks.push_back(std::move(block));
        first_free = first;
        capacity += count;
    }

    /**
     * Adds an existing slot back to the front of the linked list of free
     * slots.
     *
     * @param slot The slot to make available again. It must belong to this pool
     * and must not contain a live T object.
     */
    void ReturnSlot(Slot *slot) noexcept {
        // The object and next pointer share storage. Constructing next starts
        // its lifetime again now that there is no T object in the slot.
        std::construct_at(std::addressof(slot->next), first_free);
        first_free = slot;
    }

    /**
     * Destroys an object and makes its slot available for reuse.
     * Called by the unique pointer's deleter.
     *
     * @param object The live object to destroy. Must belong to this pool.
     */
    void Destroy(T *object) noexcept {
        // A union and each of its members have the same address. The object
        // pointer therefore also identifies the Slot that contains it.
        Slot *slot = reinterpret_cast<Slot *>(object);
        std::destroy_at(object);
        ReturnSlot(slot);
        --size;
    }

    // The blocks of memory owned by this pool. Blocks are kept until the pool
    // is destroyed, even when all of their slots are free.
    std::vector<std::unique_ptr<Slot[]>> blocks;

    // The first slot in the linked list of free slots, or nullptr if all slots
    // are in use. Each free slot's next pointer leads to the following one.
    Slot *first_free = nullptr;

    // The total number of slots allocated across all blocks.
    std::size_t capacity = 0;

    // The number of constructed objects that have not yet been destroyed.
    std::size_t size = 0;
};

} // namespace svanes
