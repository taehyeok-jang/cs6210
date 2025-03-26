// threadpool.h
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <functional>

class threadpool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;

    void worker();

public:
    threadpool(size_t max_threads);
    ~threadpool();

    template <typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        using return_type = decltype(f());
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("submit on stopped threadpool");
            }
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return future;
    }
};

#endif // THREADPOOL_H