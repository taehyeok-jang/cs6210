#include "threadpool.h"

threadpool::threadpool(size_t max_threads) : stop(false) {
    for (size_t i = 0; i < max_threads; ++i) {
        workers.emplace_back(&threadpool::worker, this);
    }
}

threadpool::~threadpool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) {
        worker.join();
    }
}

void threadpool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) {
                return;
            }
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}