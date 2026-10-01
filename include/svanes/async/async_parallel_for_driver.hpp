#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace svanes {

/**
 * Represents a driver for executing parallel for loops asynchronously.
 * Given a specified concurrency level (as in the number of threads),
 * this class manages the execution of a parallel for loop, distributing
 * the workload across multiple threads.
 */
class AsyncParallelForDriver final {
public:
    /**
     * Constructs an AsyncParallelForDriver with the specified concurrency
     * level. The concurrency level determines the number of worker threads that
     * will be used to execute parallel for loops. If the specified concurrency
     * is zero, an exception is thrown.
     *
     * If the concurrency is greater than one, worker threads are created and
     * started, each running the RunWorker method. The main thread will also
     * participate in executing the parallel for loop. If the concurrency is
     * one, no additional threads are created, and the main thread will handle
     * all work.
     *
     * @param concurrency The number of worker threads to create for parallel
     * execution.
     * @throws std::invalid_argument if concurrency is zero.
     */
    explicit AsyncParallelForDriver(std::uint32_t concurrency);

    /**
     * Destroys the AsyncParallelForDriver instance,
     * ensuring that all worker threads are requested to stop and joined before
     * the object is destroyed. This prevents any potential resource leaks or
     * undefined behavior due to dangling threads.
     */
    ~AsyncParallelForDriver();

    /**
     * Override of the copy constructor to prevent copying of the
     * AsyncParallelForDriver instance. Prevents something like
     * `AsyncParallelForDriver driver2 = driver1;`
     */
    AsyncParallelForDriver(const AsyncParallelForDriver &) = delete;

    /**
     * Override of the assignment operator to prevent copying of the
     * AsyncParallelForDriver instance. Prevents something like `driver2 =
     * driver1;`
     */
    AsyncParallelForDriver &operator=(const AsyncParallelForDriver &) = delete;

    /**
     * Override of the move constructor to prevent moving of the
     * AsyncParallelForDriver instance. Prevents something like
     * `AsyncParallelForDriver driver2 = std::move(driver1);`
     */
    AsyncParallelForDriver(AsyncParallelForDriver &&) = delete;

    /**
     * Override of the move assignment operator to prevent moving of the
     * AsyncParallelForDriver instance. Prevents something like `driver2 =
     * std::move(driver1);`
     */
    AsyncParallelForDriver &operator=(AsyncParallelForDriver &&) = delete;

    /**
     * Returns the concurrency level of the AsyncParallelForDriver,
     * which indicates the number of worker threads that can be used for
     * parallel execution.
     *
     * @return The concurrency level as a std::uint32_t.
     */
    std::uint32_t GetConcurrency() const;

    /**
     * Executes a callback over an indexed range in parallel.
     *
     * The driver creates the index range [0, count), divides it into batches of
     * at most batch_size indices, and invokes work(begin, end, worker_index)
     * for each batch. The callback receives a half-open range [begin, end),
     * where end is exclusive.
     *
     * If the current thread is already executing a ParallelFor with this same
     * driver, it will execute the work directly in the current thread, avoiding
     * deadlocks.
     *
     * If any worker thread throws an exception during execution, the exception
     * is captured and rethrown in the calling thread after all worker threads
     * have completed their work. The ParallelFor method will not return until
     * all worker threads have completed their work, even if an exception is
     * thrown.
     *
     * @param count The total number of indices to process.
     * @param batch_size The maximum number of indices to process in a single
     * batch.
     * @param work The callback function to execute for each batch of indices.
     *             It takes three parameters: the starting index (inclusive),
     *             the ending index (exclusive), and the worker thread index.
     * @throws std::invalid_argument if batch_size is zero or if work is null.
     */
    void ParallelFor(std::size_t count, std::size_t batch_size,
                     std::function<void(std::size_t begin, std::size_t end,
                                        std::uint32_t worker_index)>
                         work);

private:
    /**
     * Runs the worker thread's main loop, waiting for work to be available and
     * executing batches of work when signaled. The worker thread will continue
     * to run until a stop request is received, at which point it will exit the
     * loop and terminate.
     *
     * @param stop A std::stop_token that allows the worker thread to be
     * requested to stop.
     * @param worker_index The index of the worker thread, used to identify
     * which worker is executing the work.
     */
    void RunWorker(std::stop_token stop, std::uint32_t worker_index);
    void RunBatches(std::uint32_t worker_index);

    // Currently executing driver for the current thread, if any.
    static thread_local AsyncParallelForDriver *active_driver;

    // Currently executing worker index for the current thread, if any.
    static thread_local std::uint32_t active_worker_index;

    // The number of worker threads to create for parallel execution.
    const std::uint32_t concurrency;

    // A mutex to protect access to shared state between the main thread and
    // worker threads.
    std::mutex mutex;

    // A condition variable to signal when new work is available for worker
    // threads.
    std::condition_variable_any work_available;

    // A condition variable signalled by the last worker thread to report to the
    // main thread that all work has been completed.
    std::condition_variable work_completed;

    // A counter that tracks the current generation of work that has been
    // scheduled. Used to determine if new work has been scheduled since the
    // last time a worker thread woke up.
    std::size_t generation = 0;

    // A counter that tracks the number of worker threads that have not yet
    // completed their work for the current generation.
    std::uint32_t remaining_workers = 0;

    // The total number of indices in the current ParallelFor call. The valid
    // indices are in the range [0, item_count), where the end is exclusive.
    std::size_t item_count = 0;

    // The maximum number of indices that a worker thread may claim and process
    // in one batch during the current ParallelFor call.
    std::size_t active_batch_size = 0;

    /**
     * The work function that will be executed by the worker threads.
     *
     * @param begin The starting index of the work to be executed.
     * @param end The ending index (exclusive) of the work to be executed.
     * @param worker_index The index of the worker thread executing the work.
     */
    std::function<void(std::size_t, std::size_t, std::uint32_t)> work_function;

    // The first index of the next unclaimed batch. All worker threads share
    // this value and atomically advance it when they claim a batch of work.
    std::atomic<std::size_t> next_index{0};

    // Indicates that a callback has thrown an exception. Workers check this
    // flag before claiming more batches and stop claiming work after it is set.
    std::atomic<bool> failed{false};

    // Stores the first exception thrown by a callback so it can be rethrown on
    // the calling thread after all already-running batches have completed.
    std::exception_ptr failure;

    // A vector of worker threads that will be used to execute the parallel for
    // loop.
    std::vector<std::jthread> workers;
};

} // namespace svanes
