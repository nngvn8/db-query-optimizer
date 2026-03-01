/**
 * @file thread_pool.hpp
 * @brief Defines a simple thread pool for executing tasks concurrently.
 *
 * This file contains the declaration of the `ThreadPool` class, which manages a
 * pool of worker threads to execute tasks from a queue. This is a common pattern
 * for handling multiple client requests or other concurrent jobs in a server.
 */

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

/**
 * @class ThreadPool
 * @brief A class for managing a pool of worker threads.
 */
class ThreadPool {
public:
    /**
     * @brief Constructs a ThreadPool object.
     * @param numberOfThreads The number of worker threads to create.
     */
    ThreadPool(size_t numberOfThreads);

    /**
     * @brief Destroys the ThreadPool object, joining all worker threads.
     */
    ~ThreadPool();

    /**
     * @brief Enqueues a task to be executed by a worker thread.
     * @param task The task to execute.
     */
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workerThreads;
    std::queue<std::function<void()>> clientTasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
