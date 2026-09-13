#include <svanes/async/async_parallel_for_driver.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace svanes {

// In case a worker thread is executing a ParallelFor and tries to invoke
// another ParallelFor with the same driver, we need to track the active driver
// a worker thread is currently executing. This allows us to avoid having a
// worker thread try to give its own driver work, which could lead to the driver
// scheduling work for that same worker thread, which is already busy executing
// the current ParallelFor. This would lead to a deadlock.

// Currently executing driver for the current thread, if any.
thread_local AsyncParallelForDriver *AsyncParallelForDriver::active_driver =
    nullptr;


// Currently executing worker index for the current thread, if any.
thread_local std::uint32_t AsyncParallelForDriver::active_worker_index = 0;

AsyncParallelForDriver::AsyncParallelForDriver(std::uint32_t concurrency)
    : concurrency(concurrency) {
    if (concurrency == 0) {
        throw std::invalid_argument(
            "Parallel-for concurrency must be positive.");
    }

    // Main thread is always a worker, so we only need to create (concurrency -
    // 1) additional threads.
    workers.reserve(concurrency - 1);

    // Create and start worker threads, each running the RunWorker method.

    // A more readable version of this looks like:
    //
    // for (std::uint32_t index = 1; index < concurrency; ++index) {
    //     auto worker_function =
    //         [this, index](std::stop_token stop) {
    //             RunWorker(stop, index);
    //         };
    //
    //     std::jthread worker(worker_function);
    //     workers.push_back(std::move(worker));
    // }
    //
    // Note however that the above version is less efficient,
    // as it would create our std::jthread objects and then move them into the
    // vector, as opposed to emplace_back, which constructs the std::jthread
    // objects directly in the vector's storage.

    for (std::uint32_t index = 1; index < concurrency; ++index) {
        workers.emplace_back(
            [this, index](std::stop_token stop) { RunWorker(stop, index); });
    }
}

AsyncParallelForDriver::~AsyncParallelForDriver() {

    // Request cancellation for all worker threads.
    // Upon the next call to work_available.wait(),
    // false will be returned, and the worker threads will exit their loops.
    for (std::jthread &worker : workers) {
        worker.request_stop();
    }

    // Force all worker threads to wake up and check for the stop request.
    work_available.notify_all();

    // Wait for all worker threads to finish execution and join them
    // (we used jthread, so they will automatically join on destruction).
    workers.clear();
}

std::uint32_t AsyncParallelForDriver::GetConcurrency() const {
    return concurrency;
}

void AsyncParallelForDriver::ParallelFor(
    std::size_t count, std::size_t batch_size,
    std::function<void(std::size_t, std::size_t, std::uint32_t)> work) {
    if (batch_size == 0) {
        throw std::invalid_argument(
            "Parallel-for batch size must be positive.");
    }

    if (!work) {
        throw std::invalid_argument("Parallel-for requires a work function.");
    }

    // If there is no work to do, we can return early without doing anything.
    // could also throw an exception, but what if we have some weird dynamic
    // situation where the count is unknown until some point in the program, and
    // it turns out to be zero? would be annoying to have to wrap every call to
    // ParallelFor in a try/catch block just to handle that case. size_t so <=
    // same as ==
    if (count == 0) {
        return;
    }

    // Could have a situation like this:
    //
    // ParallelFor()
    //   -> RunBatches()
    //        -> active_driver = this
    //        -> work_function(...)
    //             -> calls driver.ParallelFor(...)
    //                  -> ???
    //
    // Which can cause deadlocks.
    //
    // So we need to check if the current thread is already executing a
    // ParallelFor with this same driver. If it is, we tell the calling thread
    // (who is already a worker belonging to this driver) that they do
    // everything that would otherwise be parallelized. So it becomes
    //
    // ParallelFor()
    //   -> RunBatches()
    //        -> active_driver = this
    //        -> work_function(...)
    //             -> calls driver.ParallelFor(...)
    //                  -> active_driver == this
    //                  -> execute nested work directly

    if (active_driver == this) {
        for (std::size_t begin = 0; begin < count;) {
            const std::size_t end = begin + std::min(batch_size, count - begin);
            work(begin, end, active_worker_index);
            begin = end;
        }
        return;
    }

    // A new c++ trick I learned: With RAII, use an anonymous scope to ensure
    // that some resource(s), in this case the lock on the mutex, get
    // released when the scope ends.
    {
        // std::lock_guard is a RAII wrapper for std::mutex that locks the mutex
        // on construction and unlocks it on destruction. This way no matter how
        // we exit this scope, the mutex will be unlocked.
        std::lock_guard lock(mutex);
        item_count = count;
        active_batch_size = batch_size;

        // Swap the work function into the member variable, so that it can be
        // accessed by the worker threads. We use std::swap to avoid copying the
        // work function, which could be expensive if it captures a lot of
        // state.
        work_function.swap(work);

        // The next_index is the index of the next item to be processed by any
        // worker thread. We reset it to 0 at the start of each ParallelFor call
        // to ensure that all items are processed from the beginning.
        next_index.store(0, std::memory_order_relaxed);

        // The failed flag indicates whether any worker thread has encountered
        // an exception during execution. We reset it to false at the start of
        // each ParallelFor call to ensure that we start with a clean slate.
        failed.store(false, std::memory_order_relaxed);

        // The failure pointer is used to store the exception thrown by any
        // worker thread. We reset it to nullptr at the start of each
        // ParallelFor call to ensure that we don't carry over any exceptions
        // from previous calls.
        failure = nullptr;

        // At this point all worker threads are waiting on the work_available
        // condition variable.
        remaining_workers = concurrency - 1;

        // Now we increment the generation counter to indicate that new work has
        // been scheduled.
        ++generation;
    }

    // Wake up all worker threads as we now have work available for them to
    // process.
    work_available.notify_all();

    // The main thread will also participate in executing the parallel for loop.
    // We call RunBatches with worker_index 0, which is reserved for the main
    // thread.
    RunBatches(0);

    std::exception_ptr error;

    {
        std::unique_lock lock(mutex);

        // Main thread waits for all worker threads to complete their work for
        // the current generation.
        work_completed.wait(lock, [this] { return remaining_workers == 0; });

        // Retrieve and clear the exception pointer, if any, so that we can
        // rethrow it outside of the lock.
        error = std::exchange(failure, nullptr);

        // To ensure that the work function is not holding any resources,
        // we swap it back with the local work variable, which will go out of
        // scope and be destroyed, releasing any resources it may be holding.
        // Additionally, this clears the now stale work_function member variable
        // for the next call to ParallelFor.
        work.swap(work_function);
    }

    if (error) {
        std::rethrow_exception(error);
    }
}

void AsyncParallelForDriver::RunWorker(std::stop_token stop,
                                       std::uint32_t worker_index) {
    // Tracks the generation of work that the worker thread has observed.
    // Used to determine if new work has been scheduled since the last time the
    // worker thread checked.
    std::size_t observed_generation = 0;

    // A lock on our shared mutex which controls access to shared
    // AsyncParallelForDriver state.
    std::unique_lock lock(mutex);

    // Even though we have a CV that says "new work is available",
    // apparently threads can wake up randomly, so we need to check if there is
    // actually new work available.
    const auto has_new_work = [this, &observed_generation] {
        return generation != observed_generation;
    };

    // Now we just wait for new work to be available, and when it is, we run the
    // batches of work.
    while (work_available.wait(lock, stop, has_new_work)) {
        // Ensure the next time we wake up we know if there is new work
        // available.
        observed_generation = generation;
        lock.unlock();
        // Parallel step
        RunBatches(worker_index);
        lock.lock();
        // Signal that this worker has completed its work for this generation.
        --remaining_workers;

        if (remaining_workers == 0) {
            // We're the last worker to finish,
            // so we notify the main thread in
            // ParallelFor that all workers have completed their work.
            work_completed.notify_one();
        }
    }
}

void AsyncParallelForDriver::RunBatches(std::uint32_t worker_index) {
    // Imagine a situation like this:
    // RunBatches(driver A)
    //     active_driver = A
    //
    //     RunBatches(driver B)
    //         active_driver = B
    //         ...
    //         active_driver = A
    //
    //     ...
    //     active_driver = nullptr
    //
    // We would lose track of the active driver for the current thread,
    // and if we were to call driver A's ParallelFor again, it would think that
    // the current thread is not a worker for driver A, and would try to
    // schedule work for it, which could lead to a deadlock.
    //
    // So not only do we need to set the active driver for the current thread to
    // this driver, but we also need to keep track of the previous active driver
    // for the current thread, so that we can restore it when we're done
    // executing the batches of work.

    AsyncParallelForDriver *previous_driver =
        std::exchange(active_driver, this);

    // Similarly, we need to keep track of the previous active worker index for
    // the current thread, so that we can restore it when we're done executing
    // the batches of work.

    const std::uint32_t previous_index =
        std::exchange(active_worker_index, worker_index);

    try {
        // Rather than divvy up the work into some set of batches each of which
        // is assigned to a specific worker thread, we instead have all worker
        // threads (including the main thread) compete for work by atomically
        // incrementing a shared index. This then tells each worker thread which
        // batch of work to execute next. This both avoids the need for a mutex
        // to protect access to some shared queue of work, and also allows for
        // automatic load balancing between worker threads, as faster threads
        // will naturally process more batches of work than slower threads.

        // https://en.cppreference.com/cpp/atomic/memory_order
        // enum class memory_order : /* unspecified */
        // {
        //     relaxed, consume, acquire, release, acq_rel, seq_cst
        // };
        //
        // So by default, https://en.cppreference.com/cpp/atomic/atomic/load,
        // we use memory_order_seq_cst. But we don't need any synchronization or
        // ordering constraints, since we only care about the atomicity of the
        // operation, so we can use memory_order_relaxed for better performance.

        // this will be the first index that this worker thread will process.
        // And it will be updated to the next index to process after each batch
        // of work is completed.
        std::size_t begin = next_index.load(std::memory_order_relaxed);
        while (begin < item_count && !failed.load(std::memory_order_relaxed)) {

            // End is either big enough so that [begin, end) is a full batch of
            // work, or it is the end of the range of work to be done.
            const std::size_t end =
                begin + std::min(active_batch_size, item_count - begin);

            // Every thread might have the same value for begin, or otherwise
            // some overlap in the ranges [begin1, end1) and [begin2, end2) for
            // two different threads. How do we remedy this?
            //
            // Well how about this idea: When we acquired the value of begin,
            // we also knew that it was equal to next_index. So if it's still
            // equal to next_index, let's atomically update next_index to be
            // equal to end, so that the next thread that comes along will
            // see that next_index has been updated, and therefore if that
            // happened after it acquired its own value for begin, it will
            // know that since there was some sort of update to next_index,
            // it should re-acquire the value of next_index to get a new value
            // for begin.
            //
            // In theory I suppose it might be possible that that second thread
            // would still be safe to process the range [begin2, end2),
            // since maybe there was no overlap with [begin1, end1), but by
            // doing it this way where you need to re-acquire the value of
            // next_index, after it has been updated, we can guarantee that
            // there will be no overlap between the ranges of work that
            // different threads are processing.
            //
            // Furthermore, according to the documentation at
            // https://en.cppreference.com/cpp/atomic/atomic/compare_exchange
            // compare_exchange_weak is allowed to fail spuriously, that is,
            // acts as if *this != expected even if they are equal. For example,
            // on some platforms, compare-and-exchange is implemented using
            // load-link/store-conditional instructions. The store-conditional
            // instruction may fail even if the loaded value compares equal, for
            // example because its reservation was lost. compare_exchange_weak
            // is allowed to report this as a spurious failure, whereas
            // compare_exchange_strong must retry or otherwise ensure that it
            // does not return false solely for this reason. When a
            // compare-and-exchange is in a loop, compare_exchange_weak will
            // yield better performance on some platforms.

            if (!next_index.compare_exchange_weak(begin, end,
                                                  std::memory_order_relaxed)) {
                continue;
            }

            // At this point, we have successfully claimed the range [begin,
            // end) for this worker thread to process. Now we can safely execute
            // the work function for this batch of work.
            work_function(begin, end, worker_index);

            // The cycle continues.
            begin = next_index.load(std::memory_order_relaxed);
        }
    // If anyone fails, record the first exception and set the failed flag to true, 
    // so that when all current batches finish we abort the remaining work
    // and then inform the calling thread of the first exception that was thrown. 
    // Note that this will drop any later exceptions that are thrown.
    } catch (...) {
        std::lock_guard lock(mutex);
        if (!failure) {
            failure = std::current_exception();
        }
        failed.store(true, std::memory_order_relaxed);
    }

    // Restore the previous active driver and worker index for the current thread,
    // so that if this thread is executing a nested ParallelFor, it will know
    // which driver and worker index to use.

    active_driver = previous_driver;
    active_worker_index = previous_index;
}

} // namespace svanes
