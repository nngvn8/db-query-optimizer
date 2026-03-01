#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "client/thread_pool.hpp"

ThreadPool::ThreadPool(size_t numberOfThreads) {
    stop = false;
    for (size_t threadSize = 1; threadSize <= numberOfThreads; threadSize++) {
        workerThreads.emplace_back([this] {
            while (true) {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this] { return stop || !clientTasks.empty(); });

                if (stop && clientTasks.empty()) {
                    return;
                }

                // get the next client task and execute it
                auto clientTask = std::move(clientTasks.front());
                clientTasks.pop();
                lock.unlock();
                clientTask();
            }
        });
    }
};

void ThreadPool::enqueue(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(queueMutex);
    clientTasks.push(std::move(task));
    lock.unlock();
    condition.notify_one();
}

ThreadPool::~ThreadPool() {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
    lock.unlock();
    condition.notify_all();
    for (std::thread& workerThread : workerThreads) {
        workerThread.join();
    }
}
